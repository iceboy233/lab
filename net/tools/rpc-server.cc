#include "base/flags.h"
#include "net/asio.h"
#include "net/rpc/server.h"
#include "net/types/addr-port.h"

DEFINE_FLAG(net::AddrPort, listen,
            net::AddrPort(net::address_v4::loopback(), 1024), "");

int main(int argc, char *argv[]) {
    using namespace net;

    base::parse_flags(argc, argv);

    io_context io_context;
    rpc::Server server(io_context.get_executor(), flags::listen, {});
    server.start();
    io_context.run();
}
