#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "absl/strings/escaping.h"
#include "base/flags.h"
#include "base/logging.h"
#include "flatbuffers/idl.h"
#include "io/file-utils.h"
#include "io/native-file.h"
#include "net/asio.h"
#include "net/blocking-result.h"
#include "net/rpc/client.h"
#include "net/types/addr-port.h"
#include "util/flatbuffers-reflection.h"

DEFINE_FLAG(net::AddrPort, server,
            net::AddrPort(net::address_v4::loopback(), 1024), "");
DEFINE_FLAG(std::string, method, "", "");
DEFINE_FLAG(std::string, key, "", "");
DEFINE_FLAG(std::string, schema_file, "", "");
DEFINE_FLAG(std::string, request_type, "", "");
DEFINE_FLAG(std::string, response_type, "", "");

int main(int argc, char *argv[]) {
    using namespace net;

    base::init_logging();
    base::parse_flags(argc, argv);

    io_context io_context;
    rpc::Client rpc_client(io_context.get_executor(), {});
    io::NativeFile schema_file;
    std::error_code ec = io::open(schema_file, flags::schema_file);
    if (ec) {
        LOG(fatal) << "open failed: " << ec;
        return 1;
    }
    std::string schema;
    ec = io::read_to_end(schema_file, schema);
    if (ec) {
        LOG(fatal) << "read failed: " << ec;
        return 1;
    }
    flatbuffers::Parser parser;
    parser.Parse(schema.c_str());
    parser.SetRootType(flags::request_type.c_str());
    std::string request_json;
    ec = read_to_end(io::std_input(), request_json);
    if (ec) {
        LOG(fatal) << "read failed: " << ec;
        return 1;
    }
    std::vector<uint8_t> request;
    if (!util::pack(parser, request_json.c_str(), request)) {
        LOG(fatal) << "pack failed";
        return 1;
    }
    rpc::Client::RequestOptions options;
    if (!flags::key.empty()) {
        // TODO(iceboy): Use a better implementation.
        std::string key_bytes;
        security::KeyArray key_array;
        if (!absl::HexStringToBytes(flags::key, &key_bytes) ||
            key_bytes.size() != key_array.size()) {
            LOG(fatal) << "invalid key";
            return 1;
        }
        std::copy_n(key_bytes.begin(), key_bytes.size(), key_array.begin());
        options.key = security::Key(key_array);
    }
    BlockingResult<std::error_code, std::vector<uint8_t>> result;
    rpc_client.request(
        flags::server, flags::method, request, options, result.callback());
    result.run(io_context);
    if (std::get<0>(result.args())) {
        LOG(fatal) << "request failed: " << std::get<0>(result.args());
        return 1;
    }
    parser.Parse(schema.c_str());
    parser.SetRootType(flags::response_type.c_str());
    std::string response_json;
    if (!util::verify_and_unpack(
        parser, std::get<1>(result.args()), response_json)) {
        LOG(fatal) << "verify_and_unpack failed: "
                   << std::get<0>(result.args());
        return 1;
    }
    ec = write(io::std_output(), response_json);
    if (ec) {
        LOG(fatal) << "write failed: " << ec;
        return 1;
    }
}
