#ifndef _NET_SHADOWSOCKS_TCP_SERVER_H
#define _NET_SHADOWSOCKS_TCP_SERVER_H

#include <chrono>

#include "net/asio.h"
#include "net/shadowsocks/encryption.h"
#include "net/timer-list.h"

namespace net {
namespace shadowsocks {

// The provided executor must be single-threaded, and all functions must be
// called in the executor thread.
class TcpServer {
public:
    struct Options {
        SaltFilter *salt_filter = nullptr;
        std::chrono::nanoseconds connection_timeout =
            std::chrono::nanoseconds::zero();
    };

    TcpServer(
        const any_io_executor &executor,
        const tcp::endpoint &endpoint,
        const MasterKey &master_key,
        const Options &options);

private:
    class Connection;

    void accept();

    any_io_executor executor_;
    const MasterKey &master_key_;
    Options options_;
    tcp::acceptor acceptor_;
    tcp::resolver resolver_;
    TimerList timer_list_;
};

}  // namespace shadowsocks
}  // namespace net

#endif  // _NET_SHADOWSOCKS_TCP_SERVER_H
