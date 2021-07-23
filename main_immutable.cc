
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

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"

using namespace std;

using std::shared_ptr;
using std::unique_ptr;

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

// Immutable for shared structure
// The whole stack changes with every update anyway.. 
// Node parent pointers are fine!
// ... except that they create cycles ...
// Weak pointer to parent

// TODO: Aligned alloc?

static const unsigned sCandidateInlineCount = 32;


struct Node {
 	typedef shared_ptr<const Node> NodePtr;
	typedef absl::InlinedVector<shared_ptr<const Node>, sCandidateInlineCount> CandidateVector;
 private:

	struct ParsedToken {
		ParsedToken(ParsedToken const&o) 
		  : lexed(o.lexed), sub(o.sub) {
		}

		ParsedToken(Token lexed) : lexed(lexed) {
		}

		ParsedToken(NodePtr sub) : lexed(0), sub(sub) {
		}

		Token lexed;
		NodePtr sub;
	};

	Rule const&rule;
	std::weak_ptr<const Node> parent;
	absl::InlinedVector<ParsedToken, sInlinedRuleLen> parsed_tokens;

 public:

 	Node(Rule const&rule, std::weak_ptr<const Node> parent)
 	  : rule(rule),
 	    parent(parent) {
 	}

 	Node(Node const&o)
 	  : rule(o.rule),
 	    parent(o.parent),
 	    parsed_tokens(o.parsed_tokens) {
 	}

 	unsigned pattern_length()const {
 		return rule.pattern.size();
 	}

 	unsigned next_unprocessed_index()const {
 		return parsed_tokens.size();
 	}

 	Token next_token_in_pattern()const {
 		assert(rule.pattern.size() > parsed_tokens.size());
 		return rule.pattern[parsed_tokens.size()];
 	}

 	bool is_complete()const {
 		if(parsed_tokens.size() < rule.pattern.size())
 			return false;

 		ParsedToken const&last = parsed_tokens.back();
 		return last.lexed || (last.sub && last.sub->is_complete());
 	}

 	// Mutablility!
 	static NodePtr create_successor_tree(shared_ptr<Node> new_leaf) {
 		// TODO: Set parent pointers

 		shared_ptr<Node> ret = new_leaf;
 		while(!ret->parent.expired()) {
 			shared_ptr<Node> new_parent(new Node(*ret->parent.lock()));
 			assert(new_parent->parsed_tokens.size());
 			assert(new_parent->parsed_tokens.back().sub);
 			new_parent->parsed_tokens.back().sub = ret;
 			ret->parent = new_parent;
 			ret = new_parent;
 		}
 		return ret;
 	}

	void consume(Token tok,
				 CandidateVector& successors)const {
		fprintf(stderr, ">> %p consumes, parent is %p\n", this, parent.lock().get());
		// Tokens after completion are errors / pattern mismatches
		if(is_complete()) {
			return;
		}

		if(parsed_tokens.size() && parsed_tokens.back().sub && (!parsed_tokens.back().sub->is_complete())) {
			fprintf(stderr, "-- Asking sub to consume\n");
			parsed_tokens.back().sub->consume(tok, successors);
			return;
		}

		const TokenType tok_type = GetTokenInstType(tok);

	  	// Can trivially consume?
	  	if(GetTokenInstType(next_token_in_pattern()) == tok_type) {
	  		fprintf(stderr, "Trivially consume by: %s\n", this->ToString().c_str());
	  		fprintf(stderr, "\tparent %p\n", this->parent.lock().get());
	  		shared_ptr<Node> leaf_successor(new Node(*this));
	  		leaf_successor->parsed_tokens.push_back(tok);
	  		NodePtr top_successor = create_successor_tree(leaf_successor);
	  		successors.push_back(top_successor);
	  		return;
	  	}

		if(IsRuleTokenName(next_token_in_pattern())) {
//			if()

			// TODO: Recurse to leaf
			// TODO: Optimize by keeping a fast pointer to the leaf
	  		assert(parent.expired());

			vector<Rule> const& rules = GetRulesForTokenName(next_token_in_pattern());
			for(const Rule& rule : rules) {
				// We create a valid node here, but we still need to consume the token...
		  		shared_ptr<Node> successor(new Node(*this));
		  		shared_ptr<const Node> successor_sub(new Node(rule, successor));
		  		successor->parsed_tokens.push_back(successor_sub);

		  		fprintf(stderr, "Stepped down to: %s\n", 
		  			successor->ToString().c_str());

		  		successor->consume(tok, successors);
		  	}
			return;
		}

		// TODO: If complete, add parent
	}

 	string ToString()const {
		ostringstream ostr;

		for(unsigned i=0;i<pattern_length();++i) {
			if(i == next_unprocessed_index()) {
				ostr << "^";
			}
			if(i >= next_unprocessed_index()) {
				ostr << TokenToString(rule.pattern[i]);
			} else {
				if(parsed_tokens[i].lexed) {
					ostr << TokenToString(parsed_tokens[i].lexed);
				} else {
					assert(parsed_tokens[i].sub);
					ostr << sRules[parsed_tokens[i].sub->rule.name-1].name;
					ostr << " { " << parsed_tokens[i].sub->ToString() << "} ";
				}
			}
			ostr << " ";
		}
		if(next_unprocessed_index() == pattern_length()) {
			ostr << "^";
		}


		if(is_complete()) {
			ostr << " (c)";
		}
		return ostr.str();
 	}

};

typedef Node::NodePtr NodePtr;
typedef Node::CandidateVector CandidateVector;


static double sStartTime = 0;

void on_exit() {
	const double end_time = doubletime();
	fprintf(stderr, "Parsing time %fms\n", (end_time-sStartTime) * 1000.0);
}

void ConsumeToken(Token tok, CandidateVector &candidates) {
	CandidateVector prev_candidates = candidates;
	candidates.clear();
	for(NodePtr cand : prev_candidates) {
		cand->consume(tok, candidates);
	}
}

void PrintCandidates(CandidateVector &candidates) {
	for(NodePtr &cand : candidates) {
		fprintf(stderr, "Candidate:\n%s\n", cand->ToString().c_str());
	}
}

extern "C" {
extern int yylex (void);
void yyset_in  ( FILE * _in_str  );
extern int yylineno;
};

int main(int argc, const char **argv) {

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

	sStartTime = doubletime();

	::atexit(on_exit);

	NodePtr top_cand(new Node(top_rules[0], std::weak_ptr<const Node>()));
	CandidateVector candidates;
	candidates.push_back(top_cand);

	while(true) {
		Token tok = yylex();
		if(tok == 0) {
			break;
		}

		string tok_type_name = GetTokenInstTypeName(tok);

		fprintf(stderr, "\n\nNext %s, candidates before %i\n",
			TokenToString(tok).c_str(), (int)candidates.size());

		PrintCandidates(candidates);

		ConsumeToken(tok, candidates);

		if(candidates.size() == 0) {
			// TODO: Report line number in preprocessed file
			fprintf(stderr, "ERROR at line %i, token %s\n", yylineno, tok_type_name.c_str());
			exit(1);
		}
	}

	PrintCandidates(candidates);

	fclose(input);

	return 0;
}