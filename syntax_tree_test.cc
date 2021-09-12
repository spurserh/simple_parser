

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
	EXPECT_TRUE(tree.CanComplete());
}

TEST(SyntaxTreeTest, StepUp) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("NUM", "5");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("DASH");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("NUM", "10");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("DASH");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("NUM", "1");
	tokens.push_back(tok);	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.CanComplete());
}

TEST(SyntaxTreeTest, TooLong) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.begin()+1));
	EXPECT_TRUE(tree.CanComplete());
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);
	EXPECT_FALSE(tree.Parse(tokens.begin()+1, tokens.end()));
	EXPECT_FALSE(tree.CanComplete());
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

	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_TRUE(tree.CanComplete());
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
	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_FALSE(tree.CanComplete());
}

TEST(SyntaxTreeTest, OneComplete) {
	parser::SyntaxTree tree;
	ASSERT_TRUE(tree.Init("top"));
	parser::LexedToken tok;
	std::vector<parser::LexedToken> tokens;
	tok.tok = parser::GetTokenInstName("COMMA");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("COMMA");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("FALSE");
	tokens.push_back(tok);
	tok.tok = parser::GetTokenInstName("TRUE");
	tokens.push_back(tok);

	EXPECT_TRUE(tree.Parse(tokens.begin(), tokens.end()));
	EXPECT_FALSE(tree.FullyComplete());
	EXPECT_TRUE(tree.CanComplete());

	tree.DeleteIncomplete();

	fprintf(stderr, "After DeleteIncomplete %s\n", tree.ToString(0).c_str());

	EXPECT_TRUE(tree.FullyComplete());
	EXPECT_TRUE(tree.CanComplete());
}


// TODO: Test of multiple step ups to remove

