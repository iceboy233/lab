cc_binary(
    name = "http-server",
    srcs = ["http-server.cc"],
    deps = [
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//base:logging",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:endpoint",
        "@org_iceboy_trunk//net/http:server",
    ],
)

cc_binary(
    name = "icmp-scanner",
    srcs = ["icmp-scanner.cc"],
    deps = [
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//base:logging",
        "@org_iceboy_trunk//io:native-file",
        "@org_iceboy_trunk//io:stream",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:asio-flags",
        "@org_iceboy_trunk//net:blocking-result",
        "@org_iceboy_trunk//net:icmp-client",
    ],
)

cc_binary(
    name = "rpc-benchmark",
    srcs = ["rpc-benchmark.cc"],
    deps = [
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//base:logging",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:endpoint",
        "@org_iceboy_trunk//net/rpc:client",
    ],
)

cc_binary(
    name = "rpc-client",
    srcs = ["rpc-client.cc"],
    deps = [
        "@com_github_google_flatbuffers//:flatbuffers",
        "@com_google_absl//absl/strings",
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//base:logging",
        "@org_iceboy_trunk//io:file-utils",
        "@org_iceboy_trunk//io:native-file",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:blocking-result",
        "@org_iceboy_trunk//net:endpoint",
        "@org_iceboy_trunk//net/rpc:client",
        "@org_iceboy_trunk//util:flatbuffers-reflection",
    ],
)

cc_binary(
    name = "rpc-relay",
    srcs = ["rpc-relay.cc"],
    deps = [
        "@com_google_absl//absl/strings",
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//base:logging",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:endpoint",
        "@org_iceboy_trunk//net/rpc:client",
        "@org_iceboy_trunk//net/rpc:server",
        "@org_iceboy_trunk//security:key",
    ],
)

cc_binary(
    name = "rpc-server",
    srcs = ["rpc-server.cc"],
    deps = [
        "@org_iceboy_trunk//base:flags",
        "@org_iceboy_trunk//net:asio",
        "@org_iceboy_trunk//net:endpoint",
        "@org_iceboy_trunk//net/rpc:server",
    ],
)
