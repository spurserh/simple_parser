
#ifndef PARSER_H
#define PARSER_H


#include <cassert>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>
#include <list>
#include <map>
#include <set>

#include <sys/time.h>

#include "absl/container/flat_hash_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"

#include "immer/map.hpp"

namespace parser {


double doubletime() {
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	return tv.tv_sec + double(tv.tv_usec) / 1000000.0;
}


using namespace std;

// For all of these, 0 = NULL
typedef unsigned TokenType;
typedef unsigned Token;
typedef unsigned RuleName;
typedef unsigned GroupName;

struct RawRule {
	const string token_name;
	const string name;
	const vector<string> pattern;
	const unsigned priority;		// 0 is no priority

	RawRule(string token_name, string name, vector<string> pattern, unsigned priority=0)
	  : token_name(token_name), name(name), pattern(pattern), priority(priority) {
	}
};

// sLexicalTokenTypes
// sRules

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

extern "C" TokenType GetTokenTypeId(const char*type) {
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


static const int sInlinedRuleLen = 32;


// Translated names from strings to ints from RawRule
struct Rule {
	const Token token_name;
	const RuleName name;
	const unsigned priority; // 0 is no priority
	absl::InlinedVector<Token, sInlinedRuleLen> pattern;

	// Handy values
	const unsigned first_non_lexical_index;

	Rule(Token token_name, 
		 RuleName name,
		 int priority,
		 absl::InlinedVector<Token, sInlinedRuleLen> pattern)
	  : token_name(token_name), name(name), priority(priority), pattern(pattern),
	    first_non_lexical_index(FindFirstNonLexical(pattern)) {
	}

	static unsigned FindFirstNonLexical(absl::InlinedVector<Token, sInlinedRuleLen> const&pattern) {
		for(unsigned i=0;i<pattern.size();++i) {
			if(!TokenIsLexical(pattern[i])) {
				return i;
			}
		}
		return pattern.size();
	}
};

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
		absl::InlinedVector<Token, sInlinedRuleLen> translated_pattern;
		for(const string&pat_token_name : raw_rule.pattern) {
			if(!GetTokenTypeId(pat_token_name.c_str())) {
				fprintf(stderr, "Missing rule '%s'\n", pat_token_name.c_str());
				assert(!"Missing rule");
			}
			translated_pattern.push_back(GetTokenInstName(pat_token_name.c_str(), ""));
		}
		v.push_back(Rule(token_name, rule_name, raw_rule.priority, translated_pattern));
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

string Indent(unsigned level) {
	string ret = "";
	for(;level;--level) {
		ret += "  ";
	}
	return ret;
}


struct StepContext {
	// The token type that is consumable by the "stepped down" branch
	TokenType lexed;
	// Generic name, like expr, not number_expr
	Token needed_rule;

	bool operator<(StepContext const&o)const {
		if(lexed == o.lexed) {
			return needed_rule < o.needed_rule;
		}
		return lexed < o.lexed;
	}
};

typedef std::vector<RuleName> StepDownStack;
typedef std::multimap<StepContext, StepDownStack> StepDownMap;
StepDownMap sStepDownMap;


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

void CreateStepDowns(RuleName ruleId, const Token needed_rule, StepDownStack stack) {
	assert(sRulesByRuleName.find(ruleId) != sRulesByRuleName.end());

	stack.push_back(ruleId);

	Rule const&rule = sRulesByRuleName.find(ruleId)->second;

	assert(rule.pattern.size() >= 1);
	const Token first_token = rule.pattern[0];

  	if(TokenIsLexical(first_token)) {
  		StepContext ctx;
  		ctx.lexed = GetTokenInstType(first_token);
  		ctx.needed_rule = needed_rule;
  		sStepDownMap.insert(StepDownMap::value_type(ctx, stack));
  	} else {
  		vector<Rule> const&sub_rules = GetRulesForTokenName(first_token);

  		for(Rule const&sub_rule : sub_rules) {
	  		const RuleName sub_ruleId = sub_rule.name;

	  		// Prevent >1 instance of the same general rule / token rule
	  		// Those situations are handled by "step-up"
	  		if(!StackContainsToken(stack, first_token)) {
	  			CreateStepDowns(sub_ruleId, needed_rule, stack);
	  		}

	  	}
  	}
}

void CreateStepDowns() {
	for(RuleName rule_name = 1;rule_name <= sRules.size();++rule_name) {
		auto found = sRulesByRuleName.find(rule_name);
		assert(found != sRulesByRuleName.end());
		Rule const&rule = found->second;

		StepDownStack stack;
		CreateStepDowns(rule_name, rule.token_name, stack);
	}
}

#if 1

struct StepUpAction {
	RuleName step_up_rule_id;

	StepDownStack then_step_down;
};

typedef std::multimap<StepContext, StepUpAction> StepUpMap;
StepUpMap sStepUpMap;

// Must create step downs first
void CreateStepUps(RuleName ruleId, const Token needed_rule) {
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
		sStepUpMap.insert(StepUpMap::value_type(ctx, action));
  	} else {
		// Slow search doesn't matter, this is just a preparatory action
		for(auto step_down_it = sStepDownMap.begin();
			step_down_it != sStepDownMap.end();
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
			sStepUpMap.insert(StepUpMap::value_type(ctx, action));

		}
  	}

}
#endif

void CreateStepUps() {
	for(RuleName rule_name = 1;rule_name <= sRules.size();++rule_name) {
		auto found = sRulesByRuleName.find(rule_name);
		assert(found != sRulesByRuleName.end());
		Rule const&rule = found->second;

		CreateStepUps(rule_name, rule.token_name);
	}
}


enum NodeId {
	NodeId_Null = 0,
	NodeId_Top = 1,
};

struct Node {

	struct ParsedToken {
		ParsedToken(Token lexed, unsigned token_index, int lineno) 
			: lexed(lexed), sub(NodeId_Null),
			  token_index(token_index), lineno(lineno) {
		}

		ParsedToken(NodeId sub) : lexed(0), sub(sub) {
		}

		Token lexed;
		NodeId sub;

		// Extra metadata if lexed is valid
		unsigned token_index;
		int lineno;
	};

	// Pointer instead of reference for move semantics
	Rule const*rule;
	NodeId parent;
	absl::InlinedVector<ParsedToken, sInlinedRuleLen> parsed_tokens;

	Node() : rule(0), parent(NodeId_Null) {

	}

	Node(Rule const&rule, NodeId parent)
	  : rule(&rule),
		parent(parent) {
	}

 	unsigned pattern_length()const {
 		return rule->pattern.size();
 	}

 	unsigned next_unprocessed_index()const {
 		return parsed_tokens.size();
 	}

 	Token next_token_in_pattern()const {
 		assert(rule->pattern.size() > parsed_tokens.size());
 		return rule->pattern[parsed_tokens.size()];
 	}


};


static const unsigned sCandidateInlineCount = 32;
static const unsigned sNodeIdInlineCount = 32;

struct Candidate {
	unsigned					next_node_id;
	immer::map<NodeId, Node>	nodes_by_id;

	// The AST may not be balanced, so traversing it can be linear time
	NodeId						work_id;

	// Passed out for userspace actions
	NodeId 						top_completed;

	typedef absl::InlinedVector<Candidate, sCandidateInlineCount> CandidateVector;
	typedef absl::InlinedVector<NodeId, sNodeIdInlineCount> NodeIdVector;

	Candidate()
	 : next_node_id(NodeId_Top), work_id(NodeId_Top) {

	}

	NodeId add_node(Node const&node) {
		const NodeId nid = (NodeId)next_node_id;
		nodes_by_id = nodes_by_id.set(nid, node);
		++next_node_id;
		return nid;
	}

	string ToString()const {
		return ToString(NodeId_Top);
	}

	bool is_complete()const {
		return is_complete(NodeId_Top);
	}

	bool is_complete(NodeId nid)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

 		if(node.parsed_tokens.size() < node.rule->pattern.size())
 			return false;

 		Node::ParsedToken const&last = node.parsed_tokens.back();
 		return last.lexed || (last.sub && is_complete(last.sub));
 	}

 	Token next_token_in_pattern(NodeId nid)const {
		if(is_complete(nid)) {
			assert(false);
			return 0;
		}

		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

		Token next_token_in_pattern = node.next_token_in_pattern();

		return next_token_in_pattern;
 	}

 	string rule_name_from_nid(NodeId nid)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;
		assert(node.rule);
		return GetRuleName(node.rule->name);
 	}

 	Node const&get_node(NodeId nid)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;
		return node;
 	}


	void step_down(Token tok,
				CandidateVector& successors) {

		const NodeId incomplete_work_id = get_incomplete_ancestor_or_top(work_id);

		const NodeId step_down_id = incomplete_work_id;

		// Step down
		if(!is_complete(step_down_id)) {
			assert(step_down_id != NodeId_Null);
			Node const&node = get_node(step_down_id);

			const Token next_token = node.next_token_in_pattern();

			// Need this check?
			if(IsRuleTokenName(next_token)) {
				StepContext step_down_ctx;

				step_down_ctx.lexed = GetTokenInstType(tok);
				step_down_ctx.needed_rule = next_token;


				auto found_step_downs_lower = sStepDownMap.lower_bound(step_down_ctx);
				auto found_step_downs_upper = sStepDownMap.upper_bound(step_down_ctx);

		#if DEBUG
				fprintf(stderr, "Step down on %s rule %s: %s\n", 
					GetTokenTypeName(step_down_ctx.lexed),
					TokenToString(step_down_ctx.needed_rule).c_str());
		#endif


				for(auto step_down_it = found_step_downs_lower;
					step_down_it != found_step_downs_upper;
					++step_down_it) {
					const StepDownStack& stack = step_down_it->second;

					// Create stack of parents, that's one candidate
					Candidate new_cand(*this);

					new_cand.work_id = new_cand.create_step_down_stack(stack, step_down_id);

			  		successors.push_back(new_cand);
				}
			}
		}

 	}

 	NodeId create_step_down_stack(StepDownStack const&stack, NodeId parent) {
		NodeId last_nid = parent;

		for(const RuleName step_down_rule_id : stack) {
			auto found_rule = sRulesByRuleName.find(step_down_rule_id);
			assert(found_rule != sRulesByRuleName.end());
			Rule const&rule = found_rule->second;

			Node sub_node(rule, last_nid);
			const NodeId sub_nid = add_node(sub_node);

			nodes_by_id = nodes_by_id.update(last_nid, [&](Node node) {
				node.parsed_tokens.push_back(sub_nid);
				return node;
			});

			last_nid = sub_nid;
		}

		return last_nid;
 	}

	void step_up(Token tok,
				 CandidateVector& successors) {

		for(NodeId nid = work_id;nid != NodeId_Null;nid = get_node(nid).parent) {
	
			if(!is_complete(nid)) {
				break;
			}
			if(nid == NodeId_Top) {
				break;
			}

			assert(nid != NodeId_Null);
			Node const&node = get_node(nid);

			// Complete, step up
			StepContext step_up_ctx;

			step_up_ctx.lexed = GetTokenInstType(tok);
			step_up_ctx.needed_rule = node.rule->token_name;

		#if DEBUG
				fprintf(stderr, "Step up on %s rule %s\n", 
					GetTokenTypeName(step_up_ctx.lexed),
					TokenToString(step_up_ctx.needed_rule).c_str());
		#endif

			auto found_step_ups_lower = sStepUpMap.lower_bound(step_up_ctx);
			auto found_step_ups_upper = sStepUpMap.upper_bound(step_up_ctx);

			for(auto step_up_it = found_step_ups_lower;
				step_up_it != found_step_ups_upper;
				++step_up_it) {
				const StepUpAction& action = step_up_it->second;
				const RuleName step_up_rule_id = action.step_up_rule_id;

				auto found_rule = sRulesByRuleName.find(step_up_rule_id);
				assert(found_rule != sRulesByRuleName.end());
				Rule const&rule = found_rule->second;

				Candidate new_cand(*this);

				Node new_node(rule, node.parent);
				new_node.parsed_tokens.push_back(Node::ParsedToken(nid));
				const NodeId new_nid = new_cand.add_node(new_node);


		  		new_cand.nodes_by_id = new_cand.nodes_by_id.update(nid, [&](Node node) {
		  			node.parent = new_nid;
		  			return node;
		  		});
		  		new_cand.nodes_by_id = new_cand.nodes_by_id.update(node.parent, [&](Node node) {
		  			const unsigned idx = node.parsed_tokens.size()-1;
		  			assert(node.parsed_tokens[idx].sub == nid);
		  			node.parsed_tokens[idx] = Node::ParsedToken(new_nid);
		  			return node;
		  		});

				if(action.then_step_down.size() == 0) {
			  		new_cand.work_id = new_nid;
			  	} else {
			  		new_cand.work_id = new_cand.create_step_down_stack(action.then_step_down, new_nid);
			  	}

				successors.push_back(new_cand);
			}
		}

 	}

 	NodeId get_incomplete_ancestor_or_top(NodeId nid)const {
		while(is_complete(nid)) {
			/*
			fprintf(stderr, "--- parent %i at %s\n", 
					(int)get_node(nid).parent,
					ToString(nid).c_str());
					*/
			nid = get_node(nid).parent;
			if(nid == NodeId_Top) {
				break;
			}
		}
		return nid;
 	}

	bool consume(Token tok, unsigned token_index, int lineno) {
		top_completed = NodeId_Null;

		// Find first incomplete
		for(NodeId nid = work_id;nid != NodeId_Null;nid = get_node(nid).parent) {
			if(!is_complete(nid)) {
				const TokenType tok_type = GetTokenInstType(tok);

				const Token next_token_this_node = next_token_in_pattern(nid);

			  	if(GetTokenInstType(next_token_this_node) == tok_type) {
					//fprintf(stderr, "consume at %s\n", ToString(nid).c_str());

			  		nodes_by_id = nodes_by_id.update(nid, [&](Node node) {
			  			node.parsed_tokens.push_back(
			  				Node::ParsedToken(tok, token_index, lineno));
			  			return node;
			  		});

		  			work_id = nid;

		  			// Keep track of completed nodes for user actions
		  			{
			  			NodeId scan_up = nid;
			  			top_completed = NodeId_Null;
			  			while(is_complete(scan_up)) {
			  				top_completed = scan_up;
			  				scan_up = get_node(scan_up).parent;
			  				if(scan_up == NodeId_Null) {
			  					break;
			  				} 
			  			}
			  		}

			  		return true;
			  	}
			  	break;
			}
		}

		return false;
	}

#if 1
	unsigned get_first_lexical_token_index(NodeId nid)const {
		Node const&node = get_node(nid);
		for(unsigned i=0;i<node.parsed_tokens.size();++i) {
			if(node.parsed_tokens[i].lexed != 0) {
				assert(TokenIsLexical(node.parsed_tokens[i].lexed));
				return node.parsed_tokens[i].token_index;
			}
		}
		assert(!"Shouldn't get here");
		return 0;
	}

#else
 	unsigned get_first_lexical_token_index(NodeId nid)const {
 		Node const&node = get_node(nid);
		assert(node.parsed_tokens.size() > 0);
		if(node.parsed_tokens[0].sub != 0) {
			return get_first_lexical_token_index(node.parsed_tokens[0].sub);
		}
		assert(TokenIsLexical(node.parsed_tokens[0].lexed));
		return node.parsed_tokens[0].lexed;
	}
#endif
	string ToString(NodeId nid)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

		const bool nid_complete = is_complete(nid);

		ostringstream ostr;
		ostr << GetRuleName(node.rule->name);
		ostr << " { ";
		for(unsigned i=0;i<node.pattern_length();++i) {
			bool complete_here = false;

			if(i < node.next_unprocessed_index()) {
				if(node.parsed_tokens[i].sub) {
					complete_here = is_complete(node.parsed_tokens[i].sub);
				} else {
					complete_here = true;
				}
			}

			if(i >= node.next_unprocessed_index()) {
				if((!nid_complete) && (!complete_here) && (i == node.parsed_tokens.size())) {
					ostr << "^";
				}

				ostr << TokenToString(node.rule->pattern[i]);
			} else {
				if((!nid_complete) && (!complete_here) && (i == (node.parsed_tokens.size()-1))) {
					ostr << "^";
				}

				if(node.parsed_tokens[i].lexed) {
					ostr << TokenToString(node.parsed_tokens[i].lexed);
				} else {
					assert(node.parsed_tokens[i].sub);
					ostr << ToString(node.parsed_tokens[i].sub);
				}
			}
			if((!nid_complete) && complete_here && (i == (node.parsed_tokens.size()-1))) {
				ostr << "^";
			}
			ostr << " ";
		}
		ostr << "} ";
		
		if((nid == NodeId_Top) && nid_complete) {
			ostr << " (c)";
		}


		return ostr.str();
 	}


	string ToStringPretty(NodeId nid, int level=0)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

		ostringstream ostr;
		ostr << GetRuleName(node.rule->name);

		if(is_complete(nid)) {
			ostr << " (c)";
		}

		ostr <<" { " << endl;
		for(unsigned i=0;i<node.pattern_length();++i) {
			ostr << Indent(level+1);
			if(i == node.next_unprocessed_index()) {
				ostr << endl << Indent(level+1) << "^";
			}
			if(i >= node.next_unprocessed_index()) {
				ostr << TokenToString(node.rule->pattern[i]);
			} else {
				if(node.parsed_tokens[i].lexed) {
					ostr << TokenToString(node.parsed_tokens[i].lexed);
				} else {
					assert(node.parsed_tokens[i].sub);
					ostr << ToStringPretty(node.parsed_tokens[i].sub, level + 1);
				}
			}
			ostr << endl;
		}
		if(!is_complete(nid) && node.next_unprocessed_index() == node.pattern_length()) {
			ostr << Indent(level) << "^" << endl;
		}
		ostr << Indent(level) << "} ";


		return ostr.str();
 	}

};

typedef Candidate::CandidateVector CandidateVector;


static double sStartTime = 0;



void PrintCandidates(CandidateVector const&candidates, bool pretty = false) {
	for(Candidate const &cand : candidates) {
		string str = pretty ? cand.ToStringPretty(NodeId_Top) : cand.ToString(NodeId_Top);
		fprintf(stderr, "%s\n", str.c_str());
	}
}

void ConsumeToken(Token tok, unsigned token_index, int lineno, 
				  CandidateVector &candidates) {

	CandidateVector prev_candidates = candidates;
	candidates.clear();

	CandidateVector branched_down;
	CandidateVector branched_up;

	for(Candidate &cand : prev_candidates) {
		if(cand.consume(tok, token_index, lineno)) {
			candidates.push_back(cand);
		} else {
			cand.step_down(tok, branched_down);
			cand.step_up(tok, branched_up);
		}
	}

#if !PROFILING
	fprintf(stderr, "Branched down to %i:\n", (int)branched_down.size());
	PrintCandidates(branched_down);
	fprintf(stderr, "Branched up to %i:\n", (int)branched_up.size());
	PrintCandidates(branched_up);
#endif
	for(Candidate &branched_cand : branched_down) {
		if(branched_cand.consume(tok, token_index, lineno)) {
			candidates.push_back(branched_cand);
		} else {
			assert(!"Successors should always be able to consume the next token");
		}
	}
	for(Candidate &branched_cand : branched_up) {
		if(branched_cand.consume(tok, token_index, lineno)) {
			candidates.push_back(branched_cand);
		} else {
			assert(!"Successors should always be able to consume the next token");
		}
	}
}

void SetupParser() {
	const double start_create_step_downs_time = doubletime();
	CreateStepDowns();
	const double end_create_step_downs_time = doubletime();

	const double start_create_step_ups_time = doubletime();
	CreateStepUps();
	const double end_create_step_ups_time = doubletime();


	fprintf(stderr, "Time to generate step-downs %fms step-ups %fms\n", 
		1000.0*(end_create_step_downs_time - start_create_step_downs_time),
		1000.0*(end_create_step_ups_time - start_create_step_ups_time));

	fprintf(stderr, "Step downs count: %i\n", (int)sStepDownMap.size());
	fprintf(stderr, "Step ups count: %i\n", (int)sStepUpMap.size());
#if SHOW_STEP_DOWNS
	fprintf(stderr, "------ step downs -----\n");
	for(auto const&step_down_val : sStepDownMap) {
		const StepContext& ctx = step_down_val.first;
		const StepDownStack& stack = step_down_val.second;

		fprintf(stderr, "  (tok %s, rule %s): ",
			GetTokenTypeName(ctx.lexed), TokenToString(ctx.needed_rule).c_str());
		for(RuleName ruleId : stack) {
			fprintf(stderr, "%s ", GetRuleName(ruleId));
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
#if SHOW_STEP_UPS
	fprintf(stderr, "------ step ups -----\n");
	for(auto const&step_up_val : sStepUpMap) {
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
}

}  // namespace parser

#endif//PARSER_H