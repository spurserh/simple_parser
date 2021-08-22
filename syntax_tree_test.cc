

#include "gtest/gtest.h"
#include "syntax_tree.h"
#include "rules.h"

TEST(SyntaxTreeTest, Init) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
}

TEST(SyntaxTreeTest, SimpleStepDown) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.Complete());
}

TEST(SyntaxTreeTest, TooLong) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.begin()+1));
	EXPECT_TRUE(tree.Complete());
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_FALSE(tree.Parse(tokens.begin()+1, tokens.end()));
}

TEST(SyntaxTreeTest, RuleOutBranch) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("COMMA");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("FALSE");
	tokens.push_back(tok);

	// TODO: Print out AST

	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.Complete());
}

TEST(SyntaxTreeTest, IncompleteBranch) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("COMMA");
	tokens.push_back(tok);
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("FALSE");
	tokens.push_back(tok);

	// TODO: Check that extra branch was removed

	// TODO: Print out AST

	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.Complete());
}

// TODO: Test of multiple step ups to remove

