cc_binary(
    name = "parse",
    srcs = ["main_immutable.cc", "lex.yy.c", "grammar.h", "parser.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:flat_hash_set", 
            "@com_google_absl//absl/container:inlined_vector",
            "@immer//:immer"
            ]
)

cc_binary(
    name = "verisim",
    srcs = ["main_verilog.cc", "lex.yy.c", "grammar.h", "parser.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:flat_hash_set", 
            "@com_google_absl//absl/container:inlined_vector",
            "@immer//:immer"
            ]
)

cc_binary(
    name = "cppint",
    srcs = ["main_cpp.cc", "lex.yy.c", "grammar.h", "parser.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:flat_hash_set", 
            "@com_google_absl//absl/container:inlined_vector",
            "@immer//:immer"
            ]
)

cc_library(
    name = "inlined_set",
    hdrs = ["inlined_set.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_set"]
)

cc_library(
    name = "syntax_tree",
    hdrs = ["syntax_tree.h"],
    srcs = ["syntax_tree.cc"],
    deps = [":inlined_set"]
)

cc_test(
    name = "syntax_tree_test",
    srcs = [
        "syntax_tree_test.cc",
    ],
    deps = [
        ":syntax_tree",
        "@gtest//:gtest",
        "@gtest//:gtest_main"
    ],
)


cc_test(
    name = "inlined_set_test",
    srcs = [
        "inlined_set_test.cc",
    ],
    deps = [
        ":inlined_set",
        "@gtest//:gtest",
        "@gtest//:gtest_main"
    ],
)
