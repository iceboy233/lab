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
    explicit Connection(UdpServer &server);

    void receive();

private:
    void forward_read();
    void forward_write(absl::Span<const uint8_t> chunk);
    void backward_read();
    void backward_write();
    void close();

    UdpServer &server_;
    udp::socket socket_;
    std::optional<udp::socket> remote_socket_;
};


UdpServer::UdpServer(
    const any_io_executor &executor,
    const udp::endpoint &endpoint,
    const AeadMasterKey &master_key)
    : executor_(executor),
      master_key_(master_key),
      socket_(executor, endpoint),
      aead_datagram_(socket_, master_key) {
    receive();
}

void UdpServer::receive() {

}

}  // namespace shadowsocks
}  // namespace net