#ifndef _NET_SHADOWSOCKS_UDP_SERVER_H
#define _NET_SHADOWSOCKS_UDP_SERVER_H

#include <list>
#include <unordered_map>
#include <map>

#include "absl/container/flat_hash_map.h"
#include "net/asio.h"
#include "net/asio-hash.h"
#include "net/shadowsocks/aead-crypto.h"

namespace net {
namespace shadowsocks {

// The provided executor must be single-threaded, and all functions must be
// called in the executor thread.
class UdpServer {
public:
    UdpServer(
        const any_io_executor &executor,
        const udp::endpoint &endpoint,
        const AeadMasterKey &master_key);

    void receive();

private:
    class Connection;

    any_io_executor executor_;
    const AeadMasterKey &master_key_;
    udp::socket socket_;
    AeadDatagram aead_datagram_;

    absl::flat_hash_map<udp::endpoint, Connection*> client_ep_;
};


}  // namespace shadowsocks
}  // namespace net

#endif  // _NET_SHADOWSOCKS_UDP_SERVER_H
