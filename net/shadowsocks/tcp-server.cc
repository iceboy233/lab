#include "net/shadowsocks/tcp-server.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "base/logging.h"
#include "boost/endian/conversion.hpp"
#include "boost/smart_ptr/intrusive_ptr.hpp"
#include "boost/smart_ptr/intrusive_ref_counter.hpp"
#include "net/proxy/stream.h"
#include "net/shadowsocks/wire-structs.h"

namespace net {
namespace shadowsocks {

class TcpServer::Connection : public boost::intrusive_ref_counter<
    Connection, boost::thread_unsafe_counter> {
public:
    explicit Connection(TcpServer &server);

    void accept();

private:
    void forward_read();
    void forward_read_tail();
    void forward_parse(absl::Span<const uint8_t> chunk);
    void forward_write(absl::Span<const uint8_t> chunk);
    void forward_rate_limit(size_t size);
    void backward_read();
    void backward_write();
    void backward_rate_limit();
    void set_timer();
    void update_timer();
    void close();

    TcpServer &server_;
    tcp::socket socket_;
    std::unique_ptr<proxy::Stream> remote_stream_;
    std::optional<TimerList::Timer> timer_;
    std::unique_ptr<uint8_t[]> backward_buffer_;
    static constexpr size_t backward_buffer_size_ = 16383;
    size_t backward_read_size_;
    EncryptedStream encrypted_stream_;
};

TcpServer::TcpServer(
    const any_io_executor &executor,
    const tcp::endpoint &endpoint,
    const proxy::shadowsocks::PreSharedKey &pre_shared_key,
    proxy::Connector &connector,
    const Options &options)
    : executor_(executor),
      pre_shared_key_(pre_shared_key),
      salt_filter_(options.salt_filter),
      connection_timeout_(options.connection_timeout),
      acceptor_(executor_, endpoint),
      connector_(connector),
      timer_list_(executor_, connection_timeout_) {
    if (options.forward_bytes_rate_limit) {
        forward_bytes_rate_limiter_.emplace(
            executor,
            options.forward_bytes_rate_limit,
            options.rate_limit_capacity);
    }
    if (options.backward_bytes_rate_limit) {
        backward_bytes_rate_limiter_.emplace(
            executor,
            options.backward_bytes_rate_limit,
            options.rate_limit_capacity);
    }
    accept();
}

void TcpServer::accept() {
    boost::intrusive_ptr<Connection> connection(new Connection(*this));
    connection->accept();
}

TcpServer::Connection::Connection(TcpServer &server)
    : server_(server),
      socket_(server_.executor_),
      backward_buffer_(std::make_unique<uint8_t[]>(backward_buffer_size_)),
      encrypted_stream_(
          socket_, server_.pre_shared_key_, server_.salt_filter_) {}

void TcpServer::Connection::accept() {
    server_.acceptor_.async_accept(
        socket_,
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec) {
            if (ec) {
                LOG(error) << "async_accept failed: " << ec;
                connection->server_.accept();
                return;
            }
            connection->socket_.set_option(tcp::no_delay(true));
            connection->forward_read();
            connection->set_timer();
            connection->server_.accept();
        });
}

void TcpServer::Connection::forward_read() {
    encrypted_stream_.read(
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, absl::Span<const uint8_t> chunk) {
            if (ec) {
                connection->close();
                return;
            }
            connection->forward_parse(chunk);
            connection->update_timer();
        });
}

void TcpServer::Connection::forward_read_tail() {
    encrypted_stream_.read(
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, absl::Span<const uint8_t> chunk) {
            if (ec) {
                connection->close();
                return;
            }
            connection->forward_write(chunk);
            connection->update_timer();
        });
}

void TcpServer::Connection::forward_parse(absl::Span<const uint8_t> chunk) {
    // Parse address, assuming the whole address is in the first chunk.
    if (chunk.size() < 1) {
        close();
        return;
    }
    const auto *header =
        reinterpret_cast<const wire::AddressHeader *>(chunk.data());
    auto callback = [connection = boost::intrusive_ptr<Connection>(this)](
        std::error_code ec, std::unique_ptr<proxy::Stream> stream) {
        if (ec) {
            connection->close();
            return;
        }
        connection->remote_stream_ = std::move(stream);
        connection->forward_read_tail();
        connection->backward_read();
        connection->update_timer();
    };
    size_t host_length;
    switch (header->type) {
    case wire::AddressType::ipv4:
        if (chunk.size() < 7) {
            close();
            return;
        }
        server_.connector_.connect_tcp_v4(
            address_v4(header->ipv4_address),
            boost::endian::load_big_u16(&chunk[5]),
            buffer(&chunk[7], chunk.size() - 7),
            std::move(callback));
        break;
    case wire::AddressType::host:
        if (chunk.size() < 2) {
            close();
            return;
        }
        host_length = header->host_length;
        if (chunk.size() < host_length + 4) {
            close();
            return;
        }
        server_.connector_.connect_tcp_host(
            {reinterpret_cast<const char *>(&chunk[2]), host_length},
            boost::endian::load_big_u16(&chunk[host_length + 2]),
            buffer(&chunk[host_length + 4], chunk.size() - (host_length + 4)),
            std::move(callback));
        break;
    case wire::AddressType::ipv6:
        if (chunk.size() < 19) {
            close();
            return;
        }
        server_.connector_.connect_tcp_v6(
            address_v6(header->ipv6_address),
            boost::endian::load_big_u16(&chunk[17]),
            buffer(&chunk[19], chunk.size() - 19),
            std::move(callback));
        break;
    default:
        close();
        return;
    }
}

void TcpServer::Connection::forward_write(absl::Span<const uint8_t> chunk) {
    if (!remote_stream_) {
        return;
    }
    async_write(
        *remote_stream_,
        buffer(chunk.data(), chunk.size()),
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, size_t size) {
            if (ec) {
                connection->close();
                return;
            }
            if (connection->server_.forward_bytes_rate_limiter_) {
                connection->forward_rate_limit(size);
            } else {
                connection->forward_read_tail();
            }
            connection->update_timer();
        });
}

void TcpServer::Connection::forward_rate_limit(size_t size) {
    server_.forward_bytes_rate_limiter_->acquire(
        size,
        [connection = boost::intrusive_ptr<Connection>(this)]() {
            connection->forward_read_tail();
        });
}

void TcpServer::Connection::backward_read() {
    if (!remote_stream_) {
        return;
    }
    remote_stream_->async_read_some(
        buffer(backward_buffer_.get(), backward_buffer_size_),
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, size_t size) {
            if (ec) {
                connection->close();
                return;
            }
            connection->backward_read_size_ = size;
            connection->backward_write();
            connection->update_timer();
        });
}

void TcpServer::Connection::backward_write() {
    encrypted_stream_.write(
        {backward_buffer_.get(), backward_read_size_},
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec) {
            if (ec) {
                connection->close();
                return;
            }
            if (connection->server_.forward_bytes_rate_limiter_) {
                connection->backward_rate_limit();
            } else {
                connection->backward_read();
            }
            connection->update_timer();
        });
}

void TcpServer::Connection::backward_rate_limit() {
    server_.backward_bytes_rate_limiter_->acquire(
        backward_read_size_,
        [connection = boost::intrusive_ptr<Connection>(this)]() {
            connection->backward_read();
        });
}

void TcpServer::Connection::set_timer() {
    if (server_.connection_timeout_ == std::chrono::nanoseconds::zero()) {
        return;
    }
    timer_.emplace(server_.timer_list_, [this]() { close(); });
}

void TcpServer::Connection::update_timer() {
    if (!timer_) {
        return;
    }
    timer_->update();
}

void TcpServer::Connection::close() {
    timer_.reset();
    remote_stream_.reset();
    socket_.close();
}

}  // namespace shadowsocks
}  // namespace net
