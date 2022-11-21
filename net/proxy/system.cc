#include "net/proxy/system.h"

#include <array>
#include <utility>

#include "absl/container/fixed_array.h"
#include "absl/strings/str_cat.h"
#include "net/proxy/stream.h"

namespace net {

class SystemConnector::TcpSocketStream : public Stream {
public:
    explicit TcpSocketStream(const any_io_executor &executor)
        : socket_(executor) {}

    TcpSocketStream(const TcpSocketStream &) = delete;
    TcpSocketStream &operator=(const TcpSocketStream &) = delete;

    any_io_executor get_executor() override {
        return socket_.get_executor();
    }

    void async_read_some(
        absl::Span<mutable_buffer const> buffers,
        absl::AnyInvocable<void(std::error_code, size_t) &&> callback) override;

    void async_write_some(
        absl::Span<const_buffer const> buffers,
        absl::AnyInvocable<void(std::error_code, size_t) &&> callback) override;

    tcp::socket &socket() { return socket_; }
    const tcp::socket &socket() const { return socket_; }

private:
    tcp::socket socket_;
};

class SystemConnector::UdpSocketDatagram : public Datagram {
public:
    explicit UdpSocketDatagram(const any_io_executor &executor)
        : socket_(executor) {}

    UdpSocketDatagram(const UdpSocketDatagram &) = delete;
    UdpSocketDatagram &operator=(const UdpSocketDatagram &) = delete;

    void async_receive_from(
        absl::Span<mutable_buffer const> buffers,
        udp::endpoint &endpoint,
        absl::AnyInvocable<void(std::error_code, size_t) &&> callback) override;

    void async_send_to(
        absl::Span<const_buffer const> buffers,
        const udp::endpoint &endpoint,
        absl::AnyInvocable<void(std::error_code, size_t) &&> callback) override;

    any_io_executor get_executor() override {
        return socket_.get_executor();
    }

    udp::socket &socket() { return socket_; }
    const udp::socket &socket() const { return socket_; }

private:
    udp::socket socket_;
};

SystemConnector::SystemConnector(const any_io_executor &executor)
    : executor_(executor),
      resolver_(executor_) {}

void SystemConnector::connect_tcp_v4(
    const address_v4 &address,
    uint16_t port,
    const_buffer initial_data,
    absl::AnyInvocable<void(
        std::error_code, std::unique_ptr<Stream>) &&> callback) {
    connect_tcp(
        std::array<tcp::endpoint, 1>({tcp::endpoint(address, port)}),
        initial_data,
        std::move(callback));
}

void SystemConnector::connect_tcp_v6(
    const address_v6 &address,
    uint16_t port,
    const_buffer initial_data,
    absl::AnyInvocable<void(
        std::error_code, std::unique_ptr<Stream>) &&> callback) {
    connect_tcp(
        std::array<tcp::endpoint, 1>({tcp::endpoint(address, port)}),
        initial_data,
        std::move(callback));
}

void SystemConnector::connect_tcp_host(
    std::string_view host,
    uint16_t port,
    const_buffer initial_data,
    absl::AnyInvocable<void(
        std::error_code, std::unique_ptr<Stream>) &&> callback) {
    resolver_.async_resolve(
        host,
        absl::StrCat(port),
        [this, initial_data, callback = std::move(callback)](
            std::error_code ec,
            const tcp::resolver::results_type &endpoints) mutable {
        if (ec) {
            std::move(callback)(ec, nullptr);
            return;
        }
        connect_tcp(endpoints, initial_data, std::move(callback));
    });
}

std::error_code SystemConnector::bind_udp_v4(
    std::unique_ptr<Datagram> &datagram) {
    auto new_datagram = std::make_unique<UdpSocketDatagram>(executor_);
    boost::system::error_code ec;
    new_datagram->socket().open(udp::v4(), ec);
    if (ec) {
        return ec;
    }
    datagram = std::move(new_datagram);
    return {};
}

std::error_code SystemConnector::bind_udp_v6(
    std::unique_ptr<Datagram> &datagram) {
    auto new_datagram = std::make_unique<UdpSocketDatagram>(executor_);
    boost::system::error_code ec;
    new_datagram->socket().open(udp::v6(), ec);
    if (ec) {
        return ec;
    }
    datagram = std::move(new_datagram);
    return {};
}

template <typename EndpointsT>
void SystemConnector::connect_tcp(
    const EndpointsT &endpoints,
    const_buffer initial_data,
    absl::AnyInvocable<void(
        std::error_code, std::unique_ptr<Stream>) &&> callback) {
    auto stream = std::make_unique<TcpSocketStream>(executor_);
    tcp::socket &socket = stream->socket();
    async_connect(
        socket,
        endpoints,
        [stream = std::move(stream), initial_data,
            callback = std::move(callback)](
            std::error_code ec, const tcp::endpoint &) mutable {
            if (ec) {
                std::move(callback)(ec, nullptr);
                return;
            }
            // TODO(iceboy): Make this an option.
            stream->socket().set_option(tcp::no_delay(true));
            if (initial_data.size()) {
                send_initial_data(
                    std::move(stream), initial_data, std::move(callback));
                return;
            }
            std::move(callback)({}, std::move(stream));
        });
}

void SystemConnector::send_initial_data(
    std::unique_ptr<TcpSocketStream> stream,
    const_buffer initial_data,
    absl::AnyInvocable<void(
        std::error_code, std::unique_ptr<Stream>) &&> callback) {
    tcp::socket &socket = stream->socket();
    async_write(
        socket,
        initial_data,
        [stream = std::move(stream), callback = std::move(callback)](
            std::error_code ec, size_t) mutable {
        if (ec) {
            std::move(callback)(ec, nullptr);
            return;
        }
        std::move(callback)({}, std::move(stream));
    });
}

void SystemConnector::TcpSocketStream::async_read_some(
    absl::Span<mutable_buffer const> buffers,
    absl::AnyInvocable<void(std::error_code, size_t) &&> callback) {
    socket_.async_read_some(
        absl::FixedArray<mutable_buffer, 1>(buffers.begin(), buffers.end()),
        std::move(callback));
}

void SystemConnector::TcpSocketStream::async_write_some(
    absl::Span<const_buffer const> buffers,
    absl::AnyInvocable<void(std::error_code, size_t) &&> callback) {
    socket_.async_write_some(
        absl::FixedArray<const_buffer, 1>(buffers.begin(), buffers.end()),
        std::move(callback));
}

void SystemConnector::UdpSocketDatagram::async_receive_from(
    absl::Span<mutable_buffer const> buffers,
    udp::endpoint &endpoint,
    absl::AnyInvocable<void(std::error_code, size_t) &&> callback) {
    socket_.async_receive_from(
        absl::FixedArray<mutable_buffer, 1>(buffers.begin(), buffers.end()),
        endpoint,
        std::move(callback));
}

void SystemConnector::UdpSocketDatagram::async_send_to(
    absl::Span<const_buffer const> buffers,
    const udp::endpoint &endpoint,
    absl::AnyInvocable<void(std::error_code, size_t) &&> callback) {
    socket_.async_send_to(
        absl::FixedArray<const_buffer, 1>(buffers.begin(), buffers.end()),
        endpoint,
        std::move(callback));
}

}  // namespace net
