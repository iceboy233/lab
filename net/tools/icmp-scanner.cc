#include <cstddef>
#include <cstdint>
#include <chrono>
#include <queue>
#include <system_error>

#include "base/flags.h"
#include "base/logging.h"
#include "io/native-file.h"
#include "io/stream.h"
#include "net/asio.h"
#include "net/asio-flags.h"
#include "net/blocking-result.h"
#include "net/icmp-client.h"

DEFINE_FLAG(net::address, from, {}, "");
DEFINE_FLAG(net::address, to, {}, "");
DEFINE_FLAG(int32_t, limit, 256, "");
DEFINE_FLAG(int32_t, rps, 1000, "");
DEFINE_FLAG(size_t, size, 0, "");
DEFINE_FLAG(bool, sort, false, "");

namespace net {
namespace {

struct Context {
    std::chrono::steady_clock::time_point start_time;
    net::address address;
    bool done = false;
    std::error_code ec;
    std::chrono::microseconds duration;
};

void print(const Context &context, std::ostream &os) {
    if (context.ec) {
        if (context.ec == make_error_code(std::errc::timed_out)) {
            return;
        }
        os << context.address << " " << context.ec << std::endl;
    } else {
        os << context.address << " " << context.duration.count() << std::endl;
    }
}

void request(
    io_context &io_context,
    IcmpClient &icmp_client,
    steady_timer &timer,
    const address &address,
    std::queue<Context> &queue,
    std::ostream &os) {
    Context &context = queue.emplace();
    context.start_time = std::chrono::steady_clock::now();
    context.address = address;
    timer.expires_at(
        context.start_time +
            std::chrono::nanoseconds(std::chrono::seconds(1)) / flags::rps);
    icmp_client.request(
        {address, 0},
        std::vector<uint8_t>(flags::size),
        [&context, &queue, &os](std::error_code ec, ConstBufferSpan) {
            context.done = true;
            if (ec) {
                context.ec = ec;
            } else {
                context.duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - context.start_time);
            }
            if (!flags::sort) {
                print(context, os);
            }
            while (!queue.empty() && queue.front().done) {
                if (flags::sort) {
                    print(queue.front(), os);
                }
                queue.pop();
            }
        });
    BlockingResult<std::error_code> wait_result;
    timer.async_wait(wait_result.callback());
    wait_result.run(io_context);
    if (std::get<0>(wait_result.args())) {
        LOG(error) << "wait failed: " << std::get<0>(wait_result.args());
    }
}

}  // namespace
}  // namespace net

int main(int argc, char *argv[]) {
    using namespace net;

    base::init_logging();
    base::parse_flags(argc, argv);

    io_context io_context;
    auto executor = io_context.get_executor();
    IcmpClient icmp_client(executor, {});
    steady_timer timer(executor);
    io::OStream os(io::std_output());
    std::queue<Context> queue;
    if (flags::from.is_v4()) {
        address_v4_iterator first(flags::from.to_v4());
        address_v4_iterator last(flags::to.to_v4());
        for (; first != last && flags::limit; ++first, --flags::limit) {
            request(io_context, icmp_client, timer, *first, queue, os);
        }
    } else {
        address_v6_iterator first(flags::from.to_v6());
        address_v6_iterator last(flags::to.to_v6());
        for (; first != last && flags::limit; ++first, --flags::limit) {
            request(io_context, icmp_client, timer, *first, queue, os);
        }
    }
    while (!queue.empty() && io_context.run_one()) {}
}
