#include "net/shadowsocks/udp-server.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "base/logging.h"
#include "boost/smart_ptr/intrusive_ptr.hpp"
#include "boost/smart_ptr/intrusive_ref_counter.hpp"
#include "net/shadowsocks/wire-structs.h"

namespace net {
namespace shadowsocks {

class UdpServer::Connection : public boost::intrusive_ref_counter<
    Connection, boost::thread_unsafe_counter> {
public:
    Connection(
        UdpServer &server, const udp::endpoint &client_ep, 
        absl::Span<const uint8_t> first_chunk);
    ~Connection();

    void forward_parse(absl::Span<const uint8_t> chunk);

private:
    void forward_send(
        absl::Span<const uint8_t> chunk, const udp::endpoint &to_ep);
    void backward_receive();
    void backward_send(size_t payload_size);
    void setup_timer();
    void close();

    UdpServer &server_;
    udp::socket remote_socket_;
    steady_timer timer_;
    udp::endpoint client_ep_;
    udp::endpoint remote_ep_;
    std::unique_ptr<uint8_t[]> backward_buffer_;
    static constexpr size_t backward_buffer_size_ = 65535 - 48;
    static constexpr size_t reserve_header_size_ = 19;
    static constexpr std::chrono::duration expire_time_ 
        = std::chrono::seconds(120);
};


UdpServer::UdpServer(
    const any_io_executor &executor,
    const udp::endpoint &endpoint,
    const MasterKey &master_key)
    : executor_(executor),
      master_key_(master_key),
      socket_(executor, endpoint),
      encrypted_datagram_(socket_, master_key) {
    receive();
}

void UdpServer::receive() {
    encrypted_datagram_.receive_from(
    [this] (
        std::error_code ec, absl::Span<const uint8_t> chunk,
        const udp::endpoint& from_ep) {
        if (ec) {
            receive();
            return;
        }
        forward_dispatch(chunk, from_ep);
    });
}

void UdpServer::send(
    absl::Span<const uint8_t> chunk, const udp::endpoint &to_ep,
    std::function<void(std::error_code)> callback) {
    encrypted_datagram_.send_to(chunk, to_ep,
        [callback = std::move(callback)] (std::error_code ec) {
            callback(ec);
        });
}

void UdpServer::forward_dispatch(
    absl::Span<const uint8_t> chunk, const udp::endpoint &from_ep) {
    auto iter = client_eps_.find(from_ep);
    if (iter != client_eps_.end()) {
        iter->second->forward_parse(chunk);
    } else {
        boost::intrusive_ptr<Connection> connection(
            new Connection(*this, from_ep, chunk));
    }
}

UdpServer::Connection::Connection(
    UdpServer &server, const udp::endpoint &client_ep, 
    absl::Span<const uint8_t> first_chunk)
    : server_(server),
      remote_socket_(server_.executor_),
      timer_(server_.executor_),
      client_ep_(client_ep),
      backward_buffer_(std::make_unique<uint8_t[]>(backward_buffer_size_)) {
    server_.client_eps_.emplace(client_ep_, this);
    timer_.expires_from_now(expire_time_);
    forward_parse(first_chunk);
    backward_receive();
    setup_timer();
}

UdpServer::Connection::~Connection() {
    close();
    server_.client_eps_.erase(client_ep_);
}

void UdpServer::Connection::forward_parse(absl::Span<const uint8_t> chunk) {
    // Parse address, assuming the whole address is in the first chunk.
    timer_.expires_from_now(expire_time_);
    if (chunk.size() < 1) {
        server_.receive();
        return;
    }
    const auto *header =
        reinterpret_cast<const wire::AddressHeader *>(chunk.data());
    switch (header->type) {
    case wire::AddressType::ipv4:
        if (chunk.size() < 7) {
            server_.receive();
            return;
        }
        if (!remote_socket_.is_open())
            remote_socket_.open(udp::v4());
        forward_send(
            chunk.subspan(7), udp::endpoint(
                address_v4(header->ipv4_address),
                (chunk[5]) << 8 | chunk[6]));
        break;
    // TODO: support wire::AddressType::host
    case wire::AddressType::ipv6:
        if (chunk.size() < 19) {
            server_.receive();
            return;
        }
        if (!remote_socket_.is_open())
            remote_socket_.open(udp::v6());
        forward_send(
            chunk.subspan(19), udp::endpoint(
                address_v6(header->ipv6_address),
                (chunk[17]) << 8 | chunk[18]));
        break;
    default:
        server_.receive();
        return;
    }
}

void UdpServer::Connection::forward_send(
    absl::Span<const uint8_t> chunk, const udp::endpoint &to_ep) {
    remote_socket_.async_send_to(
        buffer(chunk.data(), chunk.size()), to_ep,
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, size_t) {
            connection->server_.receive();
        });
}

void UdpServer::Connection::backward_receive() {
    remote_socket_.async_receive_from(
        buffer(backward_buffer_.get() + reserve_header_size_, 
            backward_buffer_size_ - reserve_header_size_), 
        remote_ep_,
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec, size_t size) {
            if (ec) {
                connection->close();
                return;
            }
            connection->timer_.expires_from_now(expire_time_);
            connection->backward_send(size);
        });
}

void UdpServer::Connection::backward_send(size_t payload_size) {
    wire::AddressHeader *header = nullptr;
    uint8_t *port = backward_buffer_.get() + reserve_header_size_ - 2;
    if (remote_ep_.address().is_v4()) {
        header = reinterpret_cast<wire::AddressHeader *>(
            backward_buffer_.get() + reserve_header_size_ - 7);
        payload_size += 7;
        header->type = wire::AddressType::ipv4;
        header->ipv4_address = remote_ep_.address().to_v4().to_bytes();
    } else {
        header = reinterpret_cast<wire::AddressHeader *>(
            backward_buffer_.get() + reserve_header_size_ - 19);
        payload_size += 19;
        header->type = wire::AddressType::ipv4;
        header->ipv6_address = remote_ep_.address().to_v6().to_bytes();
    }
    *port = (uint8_t)(remote_ep_.port() >> 8);
    *(port + 1) = (uint8_t)(remote_ep_.port() & 0xFF);
    server_.send(
        {(uint8_t *)header, payload_size}, client_ep_, 
        [connection = boost::intrusive_ptr<Connection>(this)](
            std::error_code ec) {
            if (ec) {
                connection->close();
                return;
            }
            connection->backward_receive();
        });
}

void UdpServer::Connection::setup_timer() {
    timer_.async_wait([connection = boost::intrusive_ptr<Connection>(this)](
        const boost::system::error_code &ec) {
        if (ec != boost::asio::error::operation_aborted) {
            connection->close();
        } else if (connection->remote_socket_.is_open()) {
            connection->setup_timer();
        }
    });
}

void UdpServer::Connection::close() {
    if (remote_socket_.is_open())
        remote_socket_.close();
    timer_.cancel();
}

}  // namespace shadowsocks
}  // namespace net
