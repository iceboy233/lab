#ifndef _NET_SHADOWSOCKS_UDP_SERVER_H
#define _NET_SHADOWSOCKS_UDP_SERVER_H

#include <list>

#include "absl/container/flat_hash_map.h"
#include "net/asio.h"
#include "net/asio-hash.h"
#include "net/shadowsocks/encryption.h"

namespace net {
namespace shadowsocks {

// The provided executor must be single-threaded, and all functions must be
// called in the executor thread.
class UdpServer {
public:
    UdpServer(
        const any_io_executor &executor,
        const udp::endpoint &endpoint,
        const MasterKey &master_key);

    void receive();
    void send(
        absl::Span<const uint8_t> chunk, const udp::endpoint& to_ep,
        std::function<void(std::error_code)> callback);

private:
    class Connection;

    void forward_connection(
        absl::Span<const uint8_t> chunk, const udp::endpoint& from_ep);

    any_io_executor executor_;
    const MasterKey &master_key_;
    udp::socket socket_;
    EncryptedDatagram encrypted_datagram_;

    absl::flat_hash_map<
        udp::endpoint, Connection*> client_eps_;
};


}  // namespace shadowsocks
}  // namespace net

#endif  // _NET_SHADOWSOCKS_UDP_SERVER_H
