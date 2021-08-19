
#include "rules.h"

#include <vector>

#define SHOW_STEP_DOWNS 1
#define SHOW_STEP_UPS 1

using namespace std;

namespace parser {

struct RawRule {
	const string token_name;
	const string name;
	const vector<string> pattern;
	std::string user_data;

	RawRule(string token_name, string name, vector<string> pattern, std::string user_data)
	  : token_name(token_name), name(name), pattern(pattern), user_data(user_data) {
	}
};

Rule::Rule(Token token_name, 
	 RuleName name,
	 std::string user_data,
	 absl::InlinedVector<Token, 8> pattern)
  : token_name(token_name), name(name), user_data(user_data), pattern(pattern)
{
}

#include "grammar.h"


vector<string> BuildTokenTypes() {
	vector<string> ret = sLexicalTokenTypes;
	set<string> already(ret.begin(), ret.end());

	for(const RawRule &rule : sRules) {
		if(already.find(rule.token_name) != already.end())
			continue;
		ret.push_back(rule.token_name);
		already.insert(rule.token_name);
	}

	return ret;
}

const vector<string> sTokenTypes = BuildTokenTypes();


map<string, TokenType> BuildTokenTypeIds() {
	map<string, TokenType> ret;
	TokenType idx = 0;
	for(const auto &s : sTokenTypes)
		ret[s] = idx++;
	return ret;
}

const map<string, TokenType> sTokenTypeIds = BuildTokenTypeIds();

TokenType GetTokenTypeId(const char*type) {
	if(sTokenTypeIds.find(type) != sTokenTypeIds.end()) {
		return sTokenTypeIds.find(type)->second;
	} else {
		return 0;
	}
}

const char* GetRuleName(RuleName name) {
	assert(name != 0);
	assert(name <= sRules.size());
	return sRules[name-1].name.c_str();
}

RuleName GetRuleNameInefficiently(const string&text_name) {
	for(RuleName name=1;name<=sRules.size();++name) {
		if(sRules[name-1].name == text_name)
			return name;
	}
	return 0;
}



typedef pair<TokenType, string> TokenInstanceKey;
map<TokenInstanceKey, Token> sTokenInstanceIds;
map<Token, TokenInstanceKey> sTokenInstanceKeys;



Token GetTokenInstName(const char*type_str, const char*content) {
	TokenInstanceKey key(GetTokenTypeId(type_str), content);
	const auto found = sTokenInstanceIds.find(key);
	if(found != sTokenInstanceIds.end()) {
		return found->second;
	} else {
		const Token newId = 1 + sTokenInstanceIds.size();
		sTokenInstanceIds[key] = newId;
		sTokenInstanceKeys[newId] = key;
		return newId;
	}
}

string TokenToString(Token tok) {
	assert(tok);
	const string content = GetTokenInstContent(tok);
	string ret = GetTokenInstTypeName(tok);
	if(content.size() > 0) {
		ret += "(";
		ret += content;
		ret += ")";
	}
	if(IsRuleTokenName(tok)) {
		ret += "*";
	}
	return ret;
}

extern "C" Token LexGetTokenInstName(const char*type_str, const char*content) {
	assert(find(sLexicalTokenTypes.begin(), sLexicalTokenTypes.end(), type_str) != sLexicalTokenTypes.end());
	return GetTokenInstName(type_str, content);
}

const char*GetTokenTypeName(TokenType t) {
	assert(t < sTokenTypes.size());
	return sTokenTypes[t].c_str();
}

const char*GetTokenInstContent(Token t) {
	const auto found = sTokenInstanceKeys.find(t);
	assert(found!=sTokenInstanceKeys.end());
	return found->second.second.c_str();
}

const TokenType GetTokenInstType(Token t) {
	const auto found = sTokenInstanceKeys.find(t);
	assert(found!=sTokenInstanceKeys.end());
	return found->second.first;
}

bool TokenIsLexical(Token t) {
	TokenType type = GetTokenInstType(t);
	return (type>0) && (type<sLexicalTokenTypes.size());
}

const char*GetTokenInstTypeName(Token t) {
	return GetTokenTypeName(GetTokenInstType(t));
}

map<Token, vector<Rule> > BuildRulesByTokenName() {
	map<Token, vector<Rule> > ret;
	set<string> used_rule_names;
	for(RuleName rule_name=1;rule_name<=sRules.size();++rule_name) {
		const RawRule &raw_rule = sRules[rule_name-1];
		if(used_rule_names.find(raw_rule.name) != used_rule_names.end()) {
			fprintf(stderr, "ERROR: Repeated rule name %s\n", raw_rule.name.c_str());
			exit(1);
		}
		used_rule_names.insert(raw_rule.name);
		const Token token_name = GetTokenInstName(raw_rule.token_name.c_str(), "");
		auto found = ret.find(token_name);
		vector<Rule> &v = (found != ret.end()) ? found->second : ret.insert(pair<Token, vector<Rule> >(token_name, {})).first->second;
		absl::InlinedVector<Token, 8> translated_pattern;
		for(const string&pat_token_name : raw_rule.pattern) {
			if(!GetTokenTypeId(pat_token_name.c_str())) {
				fprintf(stderr, "Missing rule '%s'\n", pat_token_name.c_str());
				assert(!"Missing rule");
			}
			translated_pattern.push_back(GetTokenInstName(pat_token_name.c_str(), ""));
		}
		v.push_back(Rule(token_name, rule_name, raw_rule.user_data, translated_pattern));
	}
	return ret;
}

const map<Token, vector<Rule> > sRulesByTokenName = BuildRulesByTokenName();

vector<Rule> const&GetRulesForTokenName(Token token_name) {
	const auto found = sRulesByTokenName.find(token_name);
	assert(found != sRulesByTokenName.end());
	return found->second;
}

bool IsRuleTokenName(Token tok) {
	return sRulesByTokenName.find(tok) != sRulesByTokenName.end();
}

map<RuleName, Rule> ExtractRules(map<Token, vector<Rule> > const&rules_by_token_name) {
	map<RuleName, Rule> ret;
	for(auto const&value : rules_by_token_name) {
		for(Rule const&rule : value.second) {
			ret.insert(map<RuleName, Rule>::value_type(rule.name, rule));
		}
	}
	return ret;
}

const map<RuleName, Rule> sRulesByRuleName = ExtractRules(sRulesByTokenName);

bool StackContainsToken(StepDownStack const&stack, Token token) {
	for(RuleName ruleId : stack) {
		auto found = sRulesByRuleName.find(ruleId);
		assert(found != sRulesByRuleName.end());
		Rule const&rule = found->second;
		if(rule.token_name == token) {
			return true;
		}
	}
	return false;
}

Rule const&GetRuleByName(RuleName name) {
	auto found_rule = sRulesByRuleName.find(name);
	assert(found_rule != sRulesByRuleName.end());
	Rule const&rule = found_rule->second;
	return rule;
}

void CreateStepDowns(RuleName ruleId, const Token needed_rule, StepDownStack stack,
					 StepDownMap& stepDownMap) {
	assert(sRulesByRuleName.find(ruleId) != sRulesByRuleName.end());

	stack.push_back(ruleId);

	Rule const&rule = sRulesByRuleName.find(ruleId)->second;

	assert(rule.pattern.size() >= 1);
	const Token first_token = rule.pattern[0];

  	if(TokenIsLexical(first_token)) {
  		StepContext ctx;
  		ctx.lexed = GetTokenInstType(first_token);
  		ctx.needed_rule = needed_rule;
  		stepDownMap.insert(StepDownMap::value_type(ctx, stack));
  	} else {
  		vector<Rule> const&sub_rules = GetRulesForTokenName(first_token);

  		for(Rule const&sub_rule : sub_rules) {
	  		const RuleName sub_ruleId = sub_rule.name;

	  		// Prevent >1 instance of the same general rule / token rule
	  		// Those situations are handled by "step-up"
	  		if(!StackContainsToken(stack, first_token)) {
	  			CreateStepDowns(sub_ruleId, needed_rule, stack, stepDownMap);
	  		}
	  	}
  	}
}

StepDownMap BuildStepDownMap() {
	StepDownMap ret;
	for(RuleName rule_name = 1;rule_name <= sRules.size();++rule_name) {
		auto found = sRulesByRuleName.find(rule_name);
		assert(found != sRulesByRuleName.end());
		Rule const&rule = found->second;

		StepDownStack stack;
		CreateStepDowns(rule_name, rule.token_name, stack, ret);
	}


#if SHOW_STEP_DOWNS
	fprintf(stderr, "------ step downs -----\n");
	for(auto const&step_down_val : ret) {
		const StepContext& ctx = step_down_val.first;
		const StepDownStack& stack = step_down_val.second;

		fprintf(stderr, "  (tok %s, rule %s): ",
			GetTokenTypeName(ctx.lexed), TokenToString(ctx.needed_rule).c_str());
		for(RuleName ruleId : stack) {
			fprintf(stderr, "%s ", GetRuleName(ruleId));
		}
		fprintf(stderr, "\n");
	}
#endif

	return ret;
}

// Must create step downs first
void CreateStepUps(RuleName ruleId, const Token needed_rule, StepDownMap const&stepDownMap,
				   StepUpMap& stepUpMap) {
	assert(sRulesByRuleName.find(ruleId) != sRulesByRuleName.end());

	Rule const&rule = sRulesByRuleName.find(ruleId)->second;

	if(rule.pattern.size() < 2) {
		return;
	}

	const Token second_token = rule.pattern[1];
  	const Token first_token = rule.pattern[0];

  	if(first_token != needed_rule) {
  		return;
  	}

	StepUpAction action;
	action.step_up_rule_id = ruleId;

  	if(TokenIsLexical(second_token)) {
		StepContext ctx;
		ctx.lexed = GetTokenInstType(second_token);
		ctx.needed_rule = needed_rule;
		stepUpMap.insert(StepUpMap::value_type(ctx, action));
  	} else {
		// Slow search doesn't matter, this is just a preparatory action
		for(auto step_down_it = stepDownMap.begin();
			step_down_it != stepDownMap.end();
			++step_down_it) {
			const StepContext& down_ctx = step_down_it->first;
			const StepDownStack& stack = step_down_it->second;

			if(down_ctx.needed_rule != second_token) {
				continue;
			}

			StepContext ctx;
			ctx.lexed = down_ctx.lexed;
			ctx.needed_rule = needed_rule;

			action.then_step_down = stack;
			stepUpMap.insert(StepUpMap::value_type(ctx, action));
		}
  	}
}

StepUpMap BuildStepUpMap(StepDownMap &stepDownMap) {
	stepDownMap = BuildStepDownMap();

	StepUpMap ret;
	for(RuleName rule_name = 1;rule_name <= sRules.size();++rule_name) {
		auto found = sRulesByRuleName.find(rule_name);
		assert(found != sRulesByRuleName.end());
		Rule const&rule = found->second;

		CreateStepUps(rule_name, rule.token_name, stepDownMap, ret);
	}


#if SHOW_STEP_UPS
	fprintf(stderr, "------ step ups -----\n");
	for(auto const&step_up_val : ret) {
		const StepContext& ctx = step_up_val.first;
		const StepUpAction& action = step_up_val.second; 
		//RuleName rule_name = step_up_val.second;
		RuleName rule_name = action.step_up_rule_id;

		fprintf(stderr, "  (tok %s, rule up %s): ",
			GetTokenTypeName(ctx.lexed), 
			TokenToString(ctx.needed_rule).c_str());

		assert(sRulesByRuleName.find(rule_name) != sRulesByRuleName.end());

//		for(Token tok : sRulesByRuleName.find(rule_name)->second.pattern) {
		const auto& pattern = sRulesByRuleName.find(rule_name)->second.pattern;
		for(unsigned pi=0;pi<pattern.size();++pi) {
			if((action.then_step_down.size() > 0) && (pi==1)) {
				fprintf(stderr, "{");
				for(RuleName step_down_rule : action.then_step_down) {
					fprintf(stderr, "%s ", GetRuleName(step_down_rule));
				}
				fprintf(stderr, "} ");
			} else {
				const Token tok = pattern[pi];
				fprintf(stderr, "%s ", TokenToString(tok).c_str());
			}
		}
		
		fprintf(stderr, "\n");
		/*
		fprintf(stderr, "-- ");
		for(RuleName ruleId : stack) {
			fprintf(stderr, "%s ", TokenToString(sRulesByRuleName.find(ruleId)->second.token_name).c_str());
		}
		fprintf(stderr, "\n");
		*/
	}
#endif

	return ret;
}


StepDownMap sStepDownMap;
StepUpMap sStepUpMap = BuildStepUpMap(sStepDownMap);

std::pair<StepDownMap::iterator, StepDownMap::iterator> GetStepDowns(StepContext const&ctx) {
	return sStepDownMap.equal_range(ctx);
}

std::pair<StepUpMap::iterator, StepUpMap::iterator> GetStepUps(StepContext const&ctx) {
	return sStepUpMap.equal_range(ctx);
}

std::multimap<StepContext, StepDownStack> const&GetStepDownMap() {
	return sStepDownMap;
}

std::multimap<StepContext, StepUpAction> const&GetStepUpMap() {
	return sStepUpMap;
}

}  // namespace parser
