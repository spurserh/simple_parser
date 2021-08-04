
#ifndef SYNTAX_TREE_H
#define SYNTAX_TREE_H

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "inlined_set.h"

#include <vector>

namespace parser {

struct Rule {
	const Token token_name;
	const RuleName name;
	const unsigned priority; // 0 is no priority
	absl::InlinedVector<Token, sInlinedRuleLen> pattern;
};

// 0 = NULL, indexes from 1
typedef unsigned LexedTokenIdx;

struct Node;

struct ParsedSlot {
	ParsedSlot(LexedTokenIdx lexed) : lexed(lexed), sub(0) { }
	ParsedSlot(Node *sub) : lexed(0), sub(sub) { }

	LexedTokenIdx lexed;
	Node 		 *sub;
};

struct Node {
	// Pointer instead of reference for move semantics
	Rule const*rule;


	InlinedSet<Node*, 4> parents;

	// These correspond to the tokens in the rule pattern
	absl::InlinedVector<ParsedSlot, 8> parsed;
};

struct LexedToken {
	TokenType type;
	string content;
	int lineno;
	int colno;
};

struct SyntaxTree {
	SyntaxTree();

	std::vector<Node>     		all_array_;
	std::vector<LexedToken> 	token_array_;
};

};  // namespace parser

#endif//SYNTAX_TREE_H

