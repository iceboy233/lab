#include "base/flags.h"
#include "base/logging.h"
#include "net/asio.h"
#include "net/http/server.h"
#include "net/types/addr-port.h"

DEFINE_FLAG(net::AddrPort, listen,
            net::AddrPort(net::address_v4::loopback(), 80), "");

int main(int argc, char *argv[]) {
    using namespace net;
    using namespace net::http;

    base::init_logging();
    base::parse_flags(argc, argv);

    io_context io_context;
    Server server(
        io_context.get_executor(),
        flags::listen,
        [](
            const Request &request,
            Response &response,
            std::function<void(std::error_code)> callback) {
            callback({});
        },
        {});
    io_context.run();
}
