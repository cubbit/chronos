cc_library(
    name = "chronos",
    srcs = [
        "chronos.cc",
    ],
    hdrs = [
        "chronos.hpp",
        "condition_variable.hpp",
        "future.hpp",
        "promise.hpp",
        "shared_state.hpp",
    ],
    include_prefix = "chronos",
    visibility = ["//visibility:public"],
    deps = [
        "@marl",
    ],
)

cc_binary(
    name = "demo",
    srcs = ["demo/main.cc"],
    deps = [":chronos"],
)
