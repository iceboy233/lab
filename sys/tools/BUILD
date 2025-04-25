cc_binary(
    name = "base64",
    srcs = ["base64.cc"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@trunk//base:flags",
        "@trunk//io:file-utils",
        "@trunk//io:native-file",
    ],
)

cc_binary(
    name = "pointer-chasing-benchmark",
    srcs = ["pointer-chasing-benchmark.cc"],
    deps = [
        "@abseil-cpp//absl/random",
        "@google_benchmark//:benchmark_main",
    ],
)
