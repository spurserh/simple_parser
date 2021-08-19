
#ifndef RULES_H
#define RULES_H

#include <string>
#include <vector>
#include <map>
#include <set>

#include "absl/container/inlined_vector.h"

namespace parser {

/*
Token is an instance of a token, not a type of token, 
like a lexical token with specific contents or a specific rule name. 

TokenType is a type of token without instantated contents. 

For example, in the grammar: 

FOO(FOO1): LPAREN ID RPAREN
FOO(FOO2): NUM

Parsing '(a)' becomes a FOO1: LPAREN ID(a) RPAREN
Parsing '5' becomes a FOO2: NUM(5)

ID and NUM are lexical TokenTypes, but not instances, since they need contents.
FOO1 and FOO2 are Token instances. 

LPAREN and RPAREN are *both* valid Tokens and TokenTypes. This is because they
require no contents.
*/
typedef unsigned Token;

// TokenTypes are valid Tokens
typedef unsigned TokenType;

// A RuleName is the specific name of a certain form, ie FOO1/FOO2 above
// A RuleName is a valid TokenType or Token
typedef unsigned RuleName;


struct Rule {
	const Token token_name;
	const RuleName name;
	const std::string user_data;
	absl::InlinedVector<Token, 8> pattern;

	Rule(Token token_name, 
		 RuleName name,
		 std::string user_data,
		 absl::InlinedVector<Token, 8> pattern);
};


std::vector<std::string> BuildTokenTypes();
std::map<std::string, TokenType> BuildTokenTypeIds();
TokenType GetTokenTypeId(const char*type);
const char* GetRuleName(RuleName name);
Token GetTokenInstName(const char*type_str, const char*content="");
extern "C" Token LexGetTokenInstName(const char*type_str, const char*content);
const char*GetTokenTypeName(TokenType t);
const char*GetTokenInstContent(Token t);
const TokenType GetTokenInstType(Token t);
bool TokenIsLexical(Token t);
const char*GetTokenInstTypeName(Token t);
std::vector<Rule> const&GetRulesForTokenName(Token token_name);
Rule const&GetRuleByName(RuleName name);

std::string TokenToString(Token tok);
bool IsRuleTokenName(Token tok);

struct StepContext {
	StepContext() : lexed(0), needed_rule(0) { }

	// The token type that is consumable by the "stepped down" branch
	TokenType lexed;
	// Token: like expr, not number_expr
	Token needed_rule;

	bool operator<(StepContext const&o)const {
		if(lexed == o.lexed) {
			return needed_rule < o.needed_rule;
		}
		return lexed < o.lexed;
	}
};
typedef std::vector<RuleName> StepDownStack;
struct StepUpAction {
	RuleName step_up_rule_id;
	StepDownStack then_step_down;
};
typedef std::multimap<StepContext, StepDownStack> StepDownMap;
typedef std::multimap<StepContext, StepUpAction> StepUpMap;

std::pair<StepDownMap::iterator, StepDownMap::iterator> GetStepDowns(StepContext const&ctx);
std::pair<StepUpMap::iterator, StepUpMap::iterator> GetStepUps(StepContext const&ctx);


std::multimap<StepContext, StepDownStack> const&GetStepDownMap();
std::multimap<StepContext, StepUpAction> const&GetStepUpMap();

}  // namespace parser

#endif//RULES_H
