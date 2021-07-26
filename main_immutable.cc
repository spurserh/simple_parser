
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

using namespace std;

using std::shared_ptr;
using std::unique_ptr;

#define DEBUG 0
#define PROFILING 1
//#define PROFILING 1
//#define DO_STEP_UP

double doubletime() {
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	return tv.tv_sec + double(tv.tv_usec) / 1000000.0;
}

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
	return (type>0) && (type<=sLexicalTokenTypes.size());
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

	Rule(Token token_name, RuleName name, int priority, absl::InlinedVector<Token, sInlinedRuleLen> pattern)
	  : token_name(token_name), name(name), priority(priority), pattern(pattern) {
	}
};

map<Token, vector<Rule> > BuildRulesByTokenName() {
	map<Token, vector<Rule> > ret;
	set<string> used_rule_names;
	for(RuleName rule_name=1;rule_name<=sRules.size();++rule_name) {
		const RawRule &raw_rule = sRules[rule_name-1];
		assert(used_rule_names.find(raw_rule.name) == used_rule_names.end());
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


struct StepDownContext {
	TokenType lexed;
	// Generic name, like expr, not number_expr
	Token needed_rule;

	bool operator<(StepDownContext const&o)const {
		if(lexed == o.lexed) {
			return needed_rule < o.needed_rule;
		}
		return lexed < o.lexed;
	}
};

typedef std::vector<RuleName> StepDownStack;
typedef std::multimap<StepDownContext, StepDownStack> StepDownMap;
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

	assert(rule.pattern.size() > 0);
	const Token first_token = rule.pattern[0];

  	if(TokenIsLexical(first_token)) {
  		StepDownContext ctx;
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

enum NodeId {
	NodeId_Null = 0,
	NodeId_Top = 1,
};

struct Node {

	struct ParsedToken {
		ParsedToken(ParsedToken const&o) 
		  : lexed(o.lexed), sub(o.sub) {
		}

		ParsedToken(Token lexed) : lexed(lexed), sub(NodeId_Null) {
		}

		ParsedToken(NodeId sub) : lexed(0), sub(sub) {
		}

		Token lexed;
		NodeId sub;
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
	NodeId						last_completed_id;
	immer::map<NodeId, Node>	nodes_by_id;

	typedef absl::InlinedVector<Candidate, sCandidateInlineCount> CandidateVector;
	typedef absl::InlinedVector<NodeId, sNodeIdInlineCount> NodeIdVector;

	Candidate()
	 : next_node_id(NodeId_Top),
	   last_completed_id(NodeId_Null) {

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
		return sRules[node.rule->name-1].name;
 	}

 	void get_parent_stack(NodeId nid, NodeIdVector &output)const {
 		while(true) {
			Node const*node_ptr = nodes_by_id.find(nid);
			assert(node_ptr);
			Node const&node = *node_ptr;
			output.push_back(nid);
			if(!node.parent) {
				return;
			}
			nid = node.parent;
 		}
 	}

	void consume(Token tok,
				 CandidateVector& successors) {
		consume(NodeId_Top, tok, successors);
	}

	void consume(NodeId nid,
				 Token tok,
				 CandidateVector& successors) {
		
		if(!is_complete(nid)) {
			// Check if already working on an incomplete sub-node
			{
				Node const*node_ptr = nodes_by_id.find(nid);
				assert(node_ptr);
				Node const&node = *node_ptr;

				// Check if sub-node already created
				if(node.parsed_tokens.size() > 0) {
			 		Node::ParsedToken const&last = node.parsed_tokens.back();

			 		if(last.sub && !is_complete(last.sub)) {
			 			//fprintf(stderr, "-- Ask sub to consume\n");
			 			consume(last.sub, tok, successors);
			 			return;
			 		}
			 	}
			 }

			const TokenType tok_type = GetTokenInstType(tok);

			const Token next_token_this_node = next_token_in_pattern(nid);

		  	// Can trivially consume?
		  	if(GetTokenInstType(next_token_this_node) == tok_type) {
		  		/*
		  		fprintf(stderr, "Trivially consume %s by: %s\n", 
		  			GetTokenTypeName(tok_type),
		  			ToString().c_str());
		  			*/

		  		nodes_by_id = nodes_by_id.update(nid, [&](Node node) {
		  			node.parsed_tokens.push_back(tok);
		  			return node;
		  		});
		  		if(is_complete(nid)) {
		  			last_completed_id = nid;
		  		}
		  		successors.push_back(*this);
		  		return;
		  	}

			if(IsRuleTokenName(next_token_this_node)) {
				StepDownContext step_down_ctx;

				step_down_ctx.lexed = GetTokenInstType(tok);
				step_down_ctx.needed_rule = next_token_this_node;

#if DEBUG
				fprintf(stderr, "Step down on %s rule %s\n", 
					GetTokenTypeName(step_down_ctx.lexed),
					TokenToString(next_token_this_node).c_str());
#endif


				auto found_step_downs_lower = sStepDownMap.lower_bound(step_down_ctx);
				auto found_step_downs_upper = sStepDownMap.upper_bound(step_down_ctx);

				for(auto step_down_it = found_step_downs_lower;
					step_down_it != found_step_downs_upper;
					++step_down_it) {
					const StepDownStack& stack = step_down_it->second;
					
					// Create stack of parents, that's one candidate
					Candidate new_cand(*this);
					NodeId last_nid = nid;
					for(RuleName const&step_down_rule_id : stack) {
						auto found_rule = sRulesByRuleName.find(step_down_rule_id);
						assert(found_rule != sRulesByRuleName.end());
						Rule const&rule = found_rule->second;

						Node sub_node(rule, nid);
						const NodeId sub_nid = new_cand.add_node(sub_node);

				  		new_cand.nodes_by_id = new_cand.nodes_by_id.update(last_nid, [&](Node node) {
				  			node.parsed_tokens.push_back(sub_nid);
				  			return node;
				  		});

				  		last_nid = sub_nid;
					}
			  		new_cand.consume(last_nid, tok, successors);
				}

				return;
			}
		}

		// Applies even to completed
		if(last_completed_id != NodeId_Null) {
		  	// Create parent to wrap
		  	// TODO: Walk stack of last completed Nodes
		  	NodeIdVector parent_stack;
		  	get_parent_stack(last_completed_id, parent_stack);

#if DEBUG
		  	fprintf(stderr, "Stack:\n");
#endif

		  	// TODO: Order/priority?
		  	for(NodeId snid : parent_stack) {

		  		if(!is_complete(snid)) {
		  			continue;
		  		}

				Node const*node_ptr = nodes_by_id.find(snid);
				assert(node_ptr);
				Node const&node = *node_ptr;

				if(!node.parent) {
					continue;
				}

				Node const*parent_ptr = nodes_by_id.find(node.parent);
				assert(parent_ptr);
				Node const&parent = *parent_ptr;
				assert(parent.parsed_tokens.size());
				assert(parent.parsed_tokens.back().sub);
				assert(parent.rule);

				Token token_for_sub = parent.rule->pattern[parent.parsed_tokens.size()-1];
				vector<Rule> const& rules = GetRulesForTokenName(token_for_sub);

#if DEBUG
		  		fprintf(stderr, "  %s tok %s rules %i\n", 
		  			rule_name_from_nid(snid).c_str(),
		  			TokenToString(token_for_sub).c_str(),
		  			(int)rules.size());
#endif

		  		// TODO: Faster lookup
				for(const Rule& rule : rules) {
					Token first_token = rule.pattern[0];

					if(GetTokenInstType(first_token) != GetTokenInstType(token_for_sub)) {
						continue;
					}

	#if DEBUG
					fprintf(stderr, "Step up to %s?\n", 
						sRules[rule.name-1].name.c_str());
	#endif
					Candidate new_cand(*this);
					// Create new node
					Node new_node(rule, node.parent);
					new_node.parsed_tokens.push_back(snid);
					const NodeId new_nid = new_cand.add_node(new_node);

					// Set sub-pointer in parent
			  		new_cand.nodes_by_id = new_cand.nodes_by_id.update(node.parent, [&](Node parent_mut) {
			  			parent_mut.parsed_tokens.back() = Node::ParsedToken(new_nid);
			  			return parent_mut;
			  		});

					// Update parent pointer in child
			  		new_cand.nodes_by_id = new_cand.nodes_by_id.update(snid, [&](Node child_mut) {
			  			child_mut.parent = new_nid;
			  			return child_mut;
			  		});

			  		new_cand.last_completed_id = NodeId_Null;

			  		new_cand.consume(new_nid, tok, successors);

				}

		  	}
		}

	}

	string ToString(NodeId nid)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

		ostringstream ostr;
		ostr << sRules[node.rule->name-1].name;
		ostr << " { ";
		for(unsigned i=0;i<node.pattern_length();++i) {
			if(i == node.next_unprocessed_index()) {
				ostr << "^";
			}
			if(i >= node.next_unprocessed_index()) {
				ostr << TokenToString(node.rule->pattern[i]);
			} else {
				if(node.parsed_tokens[i].lexed) {
					ostr << TokenToString(node.parsed_tokens[i].lexed);
				} else {
					assert(node.parsed_tokens[i].sub);
					ostr << ToString(node.parsed_tokens[i].sub);
				}
			}
			ostr << " ";
		}
		ostr << "} ";
		if(node.next_unprocessed_index() == node.pattern_length()) {
			ostr << "^";
		}


		if(is_complete(nid)) {
			ostr << " (c)";
		}
		return ostr.str();
 	}


	string ToStringPretty(NodeId nid, int level=0)const {
		Node const*node_ptr = nodes_by_id.find(nid);
		assert(node_ptr);
		Node const&node = *node_ptr;

		ostringstream ostr;
		ostr << sRules[node.rule->name-1].name;

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

void on_exit() {
	if(sStartTime == 0) {
		return;
	}
	const double end_time = doubletime();
	fprintf(stderr, "Parsing time %fms\n", (end_time-sStartTime) * 1000.0);
	sStartTime = 0;
}


void PrintCandidates(CandidateVector const&candidates, bool pretty = false) {
	for(Candidate const &cand : candidates) {
		string str = pretty ? cand.ToStringPretty(NodeId_Top) : cand.ToString(NodeId_Top);
		fprintf(stderr, "%s\n", str.c_str());
	}
}

void ConsumeToken(Token tok, CandidateVector &candidates) {
	CandidateVector prev_candidates = candidates;
	candidates.clear();
	for(Candidate &cand : prev_candidates) {
		cand.consume(tok, candidates);
	}
}


#if 0
void SanityCheckCandidates(CandidateVector &candidates) {
	for(NodePtr cand : candidates) {
		cand->sanity_check();
	}	
}
#endif

extern "C" {
extern int yylex (void);
void yyset_in  ( FILE * _in_str  );
extern int yylineno;
};


int main(int argc, const char **argv) {

	const double start_create_step_downs_time = doubletime();
	CreateStepDowns();
	const double end_create_step_downs_time = doubletime();

	fprintf(stderr, "Time to generate step-downs %fms\n", 1000.0*(end_create_step_downs_time - start_create_step_downs_time));
	fprintf(stderr, "Step downs count: %i\n", (int)sStepDownMap.size());
#if DEBUG
	for(auto const&step_down_val : sStepDownMap) {
		const StepDownContext& ctx = step_down_val.first;
		const StepDownStack& stack = step_down_val.second;

		fprintf(stderr, "  (tok %s, rule %s): ",
			GetTokenTypeName(ctx.lexed), TokenToString(ctx.needed_rule).c_str());
		for(RuleName ruleId : stack) {
			fprintf(stderr, "%s ", sRules[ruleId-1].name.c_str());
		}
		fprintf(stderr, "\n-- ");
		for(RuleName ruleId : stack) {
			fprintf(stderr, "%s ", TokenToString(sRulesByRuleName.find(ruleId)->second.token_name).c_str());
		}
		fprintf(stderr, "\n");
	}
#endif


	const Token top_token = GetTokenTypeId("top");
	if(!top_token) {
		fprintf(stderr, "No top rules found!\n");
		return 1;
	}
	vector<Rule> const&top_rules = GetRulesForTokenName(GetTokenInstName("top", ""));
	assert(top_rules.size() == 1);

	// Parse	
	if(argc != 2) {
		fprintf(stderr, "Usage: parse file\n");
		return 1;
	}

	const char*input_path = argv[1];

	FILE* input = ::fopen(input_path, "rb");

	if(input == 0) {
		fprintf(stderr, "Couldn't open input file: %s\n",
			input_path);
	}

	yyset_in(input);

	fprintf(stderr, "--- Parsing starts ---\n");


	::atexit(on_exit);


	Node top_node(top_rules[0], NodeId_Null);

	Candidate top_cand;
	top_cand.add_node(top_node);
	assert(top_cand.next_node_id == (NodeId_Top+1));

	CandidateVector candidates;
	candidates.push_back(top_cand);

	sStartTime = doubletime();

	while(true) {
		Token tok = yylex();
		if(tok == 0) {
			break;
		}

		string tok_type_name = GetTokenInstTypeName(tok);

#if !PROFILING
		fprintf(stderr, "\n\n---- Next %s, candidates before %i\n",
			TokenToString(tok).c_str(), (int)candidates.size());
#endif

#if DEBUG
		PrintCandidates(candidates);
#endif

		ConsumeToken(tok, candidates);

		if(candidates.size() == 0) {
			// TODO: Report line number in preprocessed file
			fprintf(stderr, "ERROR at line %i, token %s\n", yylineno, tok_type_name.c_str());
			exit(1);
		}

#if 0
		SanityCheckCandidates(candidates);
#endif
	}

	on_exit();

	fprintf(stderr, "\nFinal candidates (%i):\n", (int)candidates.size());
	PrintCandidates(candidates);

	CandidateVector completed_candidates;
	for(Candidate const&cand : candidates) {
		if(cand.is_complete()) {
			completed_candidates.push_back(cand);
		}
	}
	fprintf(stderr, "\nCompleted candidates (%i):\n", (int)completed_candidates.size());
	PrintCandidates(completed_candidates, true);

	fclose(input);

	return 0;
}