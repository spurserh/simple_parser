


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
    name = "block_allocator",
    hdrs = ["block_allocator.h"]
)

genrule(
    name = "test_grammar",
    srcs = ["test.grammar"],
    outs = ["lex.yy.c", "grammar.h"],
    cmd = "$(location convert_grammar.py) $(location test.grammar) $(location lex.yy.c) $(location grammar.h)",
    tools = ["convert_grammar.py"],
)

cc_library(
    name = "rules",
    hdrs = ["rules.h"],
    srcs = ["rules.cc"],
    textual_hdrs = [
        ":test_grammar"
    ],
    deps = [":inlined_set",
            "@com_google_absl//absl/container:inlined_vector"],
)

cc_test(
    name = "rules_test",
    srcs = [
        "rules_test.cc",
    ],
    deps = [
        ":rules",
        "@gtest//:gtest",
        "@gtest//:gtest_main"
    ],
)


cc_library(
    name = "syntax_tree",
    hdrs = ["syntax_tree.h"],
    srcs = ["syntax_tree.cc"],
    deps = [":inlined_set",
            ":rules",
            ":block_allocator",
            "@com_google_absl//absl/container:inlined_vector",
            "@boost//:range"]
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



cc_test(
    name = "block_allocator_test",
    srcs = [
        "block_allocator_test.cc",
    ],
    deps = [
        ":block_allocator",
        "@gtest//:gtest",
        "@gtest//:gtest_main"
    ],
)
