

#include "gtest/gtest.h"
#include "syntax_tree.h"
#include "rules.h"

TEST(SyntaxTreeTest, Init) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
}

TEST(SyntaxTreeTest, SimpleParse) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("COMMA");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.Complete());
}

