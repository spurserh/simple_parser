cc_binary(
    name = "parse",
    srcs = ["main_immutable.cc", "lex.yy.c", "grammar.h", "parser.h"],
    deps = ["@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:flat_hash_set", 
            "@com_google_absl//absl/container:inlined_vector",
            "@immer//:immer"
            ]
)
