
#include "rules.h"

#include "gtest/gtest.h"

TEST(RulesTest, BasicChecks) {
	EXPECT_EQ(8, parser::BuildTokenTypes().size());
	EXPECT_EQ(0, parser::GetStepUpMap().size());
	EXPECT_EQ(12, parser::GetStepDownMap().size());

	EXPECT_NE(0, parser::GetTokenTypeId("top"));
	for(parser::Rule const&rule : parser::GetRulesForTokenName(parser::GetTokenInstName("top"))) {
		EXPECT_FALSE(rule.is_leaf);
	}

	EXPECT_NE(0, parser::GetTokenTypeId("true1"));
	for(parser::Rule const&rule : parser::GetRulesForTokenName(parser::GetTokenInstName("true1"))) {
		EXPECT_TRUE(rule.is_leaf);
	}
}

TEST(RulesTest, FindStepUps) {	
	parser::StepContext ctx;
	ctx.lexed = parser::GetTokenTypeId("COMMA");
	ctx.needed_rule = parser::GetTokenInstName("expr");

	EXPECT_NE(0, ctx.lexed);
	EXPECT_NE(0, ctx.needed_rule);
	auto it_pair = parser::GetStepUps(ctx);
	unsigned n = 0;
	for(auto it = it_pair.first;it != it_pair.second; ++it) {
		++n;
	}
	EXPECT_EQ(1, n);
}


TEST(RulesTest, FindStepDowns) {	
	parser::StepContext ctx;
	ctx.lexed = parser::GetTokenTypeId("TRUE");
	ctx.needed_rule = parser::GetTokenInstName("top");

	EXPECT_NE(0, ctx.lexed);
	EXPECT_NE(0, ctx.needed_rule);
	auto it_pair = parser::GetStepDowns(ctx);
	unsigned n = 0;
	for(auto it = it_pair.first;it != it_pair.second; ++it) {
		++n;
	}
	EXPECT_EQ(1, n);
}

