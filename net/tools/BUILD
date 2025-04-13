cc_binary(
    name = "http-server",
    srcs = ["http-server.cc"],
    deps = [
        "@trunk//base:flags",
        "@trunk//base:logging",
        "@trunk//net:asio",
        "@trunk//net:endpoint",
        "@trunk//net/http:server",
    ],
)

cc_binary(
    name = "icmp-scanner",
    srcs = ["icmp-scanner.cc"],
    deps = [
        "@trunk//base:flags",
        "@trunk//base:logging",
        "@trunk//io:native-file",
        "@trunk//io:stream",
        "@trunk//net:asio",
        "@trunk//net:asio-flags",
        "@trunk//net:blocking-result",
        "@trunk//net:icmp-client",
    ],
)

cc_binary(
    name = "rpc-benchmark",
    srcs = ["rpc-benchmark.cc"],
    deps = [
        "@trunk//base:flags",
        "@trunk//base:logging",
        "@trunk//net:asio",
        "@trunk//net:endpoint",
        "@trunk//net/rpc:client",
    ],
)

cc_binary(
    name = "rpc-client",
    srcs = ["rpc-client.cc"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@flatbuffers",
        "@trunk//base:flags",
        "@trunk//base:logging",
        "@trunk//io:file-utils",
        "@trunk//io:native-file",
        "@trunk//net:asio",
        "@trunk//net:blocking-result",
        "@trunk//net:endpoint",
        "@trunk//net/rpc:client",
        "@trunk//util:flatbuffers-reflection",
    ],
)

cc_binary(
    name = "rpc-relay",
    srcs = ["rpc-relay.cc"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@trunk//base:flags",
        "@trunk//base:logging",
        "@trunk//net:asio",
        "@trunk//net:endpoint",
        "@trunk//net/rpc:client",
        "@trunk//net/rpc:server",
        "@trunk//security:key",
    ],
)

cc_binary(
    name = "rpc-server",
    srcs = ["rpc-server.cc"],
    deps = [
        "@trunk//base:flags",
        "@trunk//net:asio",
        "@trunk//net:endpoint",
        "@trunk//net/rpc:server",
    ],
)
