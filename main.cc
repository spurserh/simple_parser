
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

using namespace std;


// For all of these, 0 = NULL
typedef unsigned TokenType;
typedef unsigned Token;
typedef unsigned RuleName;
typedef unsigned GroupName;

const vector<string> sLexicalTokenTypes = {
	"NULL",
	"ID",
	"TYPEID",
	"NUM",
	"INT",
	"VOID",
	"PLUS",
	"MINUS",
	"TIMES",
	"DIVIDE",
	"LPAREN",
	"RPAREN",
	"RETURN",
	"SEMI",
	"EQUALS",
	"LBRACE",
	"RBRACE",
	"MOD",
	"LT",
	"GT",
	"COMMA",
	"DBLCOLON",
	"TRUE",
	"FALSE"
};


struct RawRule {
	const string name;
	const string token_name;
	const unsigned priority;		// 0 is no priority
	const vector<string> pattern;

	RawRule(string token_name, string name, vector<string> pattern, unsigned priority=0)
	  : token_name(token_name), name(name), pattern(pattern), priority(priority) {
	}
};


const vector<RawRule> sRules = {
	RawRule("expr", "paren_expr", {"LPAREN", "expr", "RPAREN"}),
	RawRule("expr", "c_cast_expr", {"LPAREN", "type", "RPAREN", "expr"}),
	RawRule("expr", "cpp_cast_expr", {"type", "LPAREN", "expr", "RPAREN"}),

	RawRule("func_call_params", "func_call_params_ext", {"expr", "COMMA", "func_call_params"}),
 	RawRule("func_call_params", "func_call_params_base", {"expr"}),

	RawRule("expr", "templated_expr", {"type", "LT", "func_call_params", "GT"}),

	RawRule("expr", "func_expr", {"ID", "LPAREN", "func_call_params", "RPAREN"}),
	RawRule("expr", "func_expr_empty", {"ID", "LPAREN", "RPAREN"}),
	RawRule("expr", "id_expr", {"ID"}),
	RawRule("expr", "num_expr", {"NUM"}),
	RawRule("bool", "true_expr", {"TRUE"}),
	RawRule("bool", "false_expr", {"FALSE"}),
	RawRule("expr", "bool_expr", {"bool"}),

	RawRule("expr", "plus_expr", {"expr", "PLUS", "expr"}, 4),
	RawRule("expr", "minus_expr", {"expr", "MINUS", "expr"}, 4),

	RawRule("expr", "times_expr", {"expr", "TIMES", "expr"}, 3),
	RawRule("expr", "divide_expr", {"expr", "DIVIDE", "expr"}, 3),
	RawRule("expr", "modulo_expr", {"expr", "MOD", "expr"}, 3),

	RawRule("expr", "gt_expr", {"expr", "GT", "expr"}, 6),
	RawRule("expr", "lt_expr", {"expr", "LT", "expr"}, 6),

	RawRule("expr", "static_ref_expr", {"expr", "DBLCOLON", "expr"}, 1),

	RawRule("type", "void_type", {"VOID"}),
	RawRule("type", "int_type", {"INT"}),
//	RawRule("type", "id_type", {"ID"}),
	RawRule("type", "id_type", {"TYPEID"}),

	RawRule("type", "static_ref_type", {"type", "DBLCOLON", "type"}),
//	RawRule("type_template_params", "type_template_params_t", {"type"}),
 //    RawRule("type_template_params", "type_template_params_e", {"expr"}),
	// RawRule("type_template_params", "type_template_params_ext", {"type_template_params", "COMMA", "type_template_params"}),

	RawRule("type", "templated_type", {"type", "LT", "func_call_params", "GT"}),

	RawRule("semi_expr", "semi_expr_decl", {"type", "ID", "SEMI"}),
	RawRule("semi_expr", "semi_expr_decl_constructor_empty", {"type", "ID", "LPAREN", "RPAREN", "SEMI"}),
	RawRule("semi_expr", "semi_expr_decl_constructor", {"type", "ID", "LPAREN", "func_call_params", "RPAREN", "SEMI"}),
	RawRule("semi_expr", "semi_expr_decl_init", {"type", "ID", "EQUALS", "expr", "SEMI"}),
	RawRule("semi_expr", "semi_expr_return", {"RETURN", "expr", "SEMI"}),
	RawRule("semi_expr", "semi_expr", {"expr", "SEMI"}),

	RawRule("semi_expr_list", "semi_expr_ext", {"semi_expr", "semi_expr_list"}),
	RawRule("semi_expr_list", "semi_expr_base", {"semi_expr"}),

	RawRule("func_param_list", "func_param_list_single", {"type", "ID"}),
	RawRule("func_param_list", "func_param_list_ext", {"func_param_list", "COMMA", "func_param_list"}),

	RawRule("func_def", "func_def_empty", {"type", "ID", "LPAREN", "RPAREN", "LBRACE", "semi_expr_list", "RBRACE"}),
	RawRule("func_def", "func_def_params", {"type", "ID", "LPAREN", "func_param_list", "RPAREN", "LBRACE", "semi_expr_list", "RBRACE"}),
	RawRule("func_def", "func_def_empty_empty", {"type", "ID", "LPAREN", "RPAREN", "LBRACE", "RBRACE"}),
	RawRule("func_def", "func_def_params_empty", {"type", "ID", "LPAREN", "func_param_list", "RPAREN", "LBRACE", "RBRACE"}),

	RawRule("top_base", "top_base_func", {"func_def"}),

	RawRule("top_list", "top_list_base", {"top_base", "top_list"}),
	RawRule("top_list", "top_list_ext", {"top_base"}),

	RawRule("top", "top", {"top_list"}),
//	RawRule("top", "top", {"func_def"}),
//	RawRule("top", "top", {"type"}),
//	RawRule("top", "top", {"expr"}),
};


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


struct Rule {
	const RuleName name;
	const Token token_name;
	const unsigned priority; // 0 is no priority
	const vector<Token> pattern;

	Rule(Token token_name, RuleName name, int priority, vector<Token> pattern)
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
		vector<Token> translated_pattern;
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

#include "lex.yy.c"

struct Node {
	// Pure book-keeping
	Node *parent;
	unsigned parent_token_idx;

	// Matters for comparison
	Rule const&rule;
	vector<Token> tokens;
	vector<unique_ptr<Node> > subs;
	unsigned next_token;

	Node(Rule const&rule, Node *parent, unsigned parent_token_idx)
	  : rule(rule),
	  	tokens(rule.pattern),
	  	next_token(0),
	  	parent_token_idx(parent_token_idx),
	  	parent(parent) {
		subs.resize(rule.pattern.size());
	}

	Node(Node const&o) 
	  : rule(o.rule),
	  	tokens(o.tokens),
	  	next_token(o.next_token),
	  	parent(o.parent),
	  	parent_token_idx(o.parent_token_idx) {
	  	for(unique_ptr<Node> const&sub : o.subs) {
	  		Node *new_ptr = 0;
	  		if(sub) {
	  			new_ptr = new Node(*sub);
	  			new_ptr->parent = this; 
	  		}
	  		subs.push_back(unique_ptr<Node>(new_ptr));
	  	}
	}

	bool complete()const {
		return next_token == tokens.size();
	}

	Token get_next_leaf()const {
		if(next_token == tokens.size())
			return 0;
		if(subs[next_token]) {
			Token ret = subs[next_token]->get_next_leaf();
			if(ret)
				return ret;
		}
		return get_next_child();
	}

	Token get_next_child()const {
		return tokens[next_token];
	}

	void get_most_recently_completed(vector<Node const*> &output)const {
		vector<Node*> ret;
		const_cast<Node*>(this)->get_most_recently_completed(ret);
		for(Node *n : ret)
			output.push_back(n);
	}

	void get_most_recently_completed(vector<Node*> &output) {
		if((next_token>0) && subs[next_token-1]) {
			subs[next_token-1]->get_most_recently_completed(output);
		}
		if((next_token < subs.size()) && subs[next_token]) {
			subs[next_token]->get_most_recently_completed(output);
		}
		if(complete()) {
			output.push_back(this);
		}
	}

	Node &get_next_descendant() {
		if(subs[next_token])
			return subs[next_token]->get_next_descendant();
		return *this;
	}

	void recursively_set_parent_next_tokens_here() {
		if(parent) {
			parent->next_token = this->parent_token_idx;
			parent->recursively_set_parent_next_tokens_here();
		}
	}

	operator string()const {
		return this->ToString();
	}

	bool advance(Token place) {
		if(subs[next_token] && subs[next_token]->advance(place))
			return true;
		if(next_token < tokens.size()) {
			tokens[next_token] = place;
			++next_token;
			return next_token < tokens.size();
		}
		return false;
	}

	string ToString()const {
		ostringstream ostr;
		ostr << sRules[rule.name-1].name;
		if(!parent)
			ostr << " R ";
		ostr << "{";
		for(size_t idx=0;idx<tokens.size();++idx) {
			if(idx == next_token)
				ostr<<"^";
			if(subs[idx]) {
				ostr<<subs[idx]->ToString();
			} else {
				ostr<<TokenToString(tokens[idx]);
			}
			if(idx != (tokens.size()-1))
				ostr<<" ";
		}
		if(next_token == tokens.size())
			ostr<<"^";
		ostr << "}";
		return ostr.str();
	}

	string ToStringPretty(int level=0)const {
		ostringstream ostr;
		ostr << Indent(level) << sRules[rule.name-1].name;
		if(!parent)
			ostr << " R ";
		ostr << "{" << endl;
		for(size_t idx=0;idx<tokens.size();++idx) {
			if(subs[idx]) {
				ostr<<subs[idx]->ToStringPretty(level+1);
			} else {
				ostr<<Indent(level+1)<<TokenToString(tokens[idx]);
			}
			ostr<<endl;
		}
		ostr << Indent(level) << "}";
		return ostr.str();
	}

	bool operator!=(Node const&o)const {
		return (!((*this) < o)) && (!(o < (*this)));
	}
/*
	bool operator<(Node const&o)const {
		if(rule.name != o.rule.name)
			return rule.name < o.rule.name;
		if(tokens != o.tokens)
			return tokens < o.tokens;
		assert(subs.size() == o.subs.size());
		for(size_t i=0;i<subs.size();++i) {
			int i_have = subs[i] ? 1 : 0;
			int o_have = o.subs[i] ? 1 : 0;

			if(i_have && (i_have == o_have)) {
				if(*subs[i] != *o.subs[i]) {
					return *subs[i] < *o.subs[i];
				}
			} else {
				return i_have < o_have;
			}
		}
		return next_token < o.next_token;
	}
*/
	// TODO
	bool operator<(Node const&o)const {
		return ToString() < o.ToString();
	}
};


void PrintCandidates(list<Node> &candidates) {
	for(Node const&n : candidates) {
		fprintf(stderr, "%s\n", string(n).c_str());
	}
}

void PrettyPrintCandidates(list<Node> &candidates) {
	int idx = 0;
	for(Node const&n : candidates) {
		fprintf(stderr, "Candidate %i\n", idx++);
		fprintf(stderr, "%s\n\n", n.ToStringPretty().c_str());
	}
}

bool NeedToExpandCandidate(Node const&cand) {
	Token next_leaf = cand.get_next_leaf();
	return IsRuleTokenName(next_leaf);
}

void SanityCheckTree(Node const&tree) {
	for(unsigned subi=0;subi<tree.subs.size();++subi) {
		if(!IsRuleTokenName(tree.tokens[subi])) {
			assert(!tree.subs[subi]);
		}

		if(tree.subs[subi]) {
			assert(tree.subs[subi]->parent == &tree);
			assert(tree.subs[subi]->parent_token_idx == subi);
		}
	}
}

bool RuleCanBeChild(Rule const&parent, Rule const&child) {
	if(parent.priority && child.priority) {
		//fprintf(stderr, "priority %i vs %i OK\n", parent.priority, child.priority);
		return parent.priority >= child.priority;
	}
	return true;
}

void ExpandCandidate(Node const&orig, list<Node> &expanded_list, set<RuleName> no_recurse = {}) {
	assert(IsRuleTokenName(orig.get_next_leaf()));
	for(Rule const&sub : GetRulesForTokenName(orig.get_next_leaf())) {
		Node new_tree(orig);
		Node &descendant = new_tree.get_next_descendant();
		assert(descendant.tokens[descendant.next_token] == orig.get_next_leaf());

		if(!RuleCanBeChild(descendant.rule, sub))
			continue;

		descendant.subs[descendant.next_token] = std::move(unique_ptr<Node>(new Node(sub, &descendant, descendant.next_token)));
		SanityCheckTree(new_tree);

		if(!IsRuleTokenName(new_tree.get_next_leaf())) {
			expanded_list.push_back(new_tree);
		} else if(no_recurse.find(new_tree.get_next_leaf()) == no_recurse.end()) {
			set<RuleName> next_no_recurse = no_recurse;
			next_no_recurse.insert(new_tree.get_next_leaf());
			ExpandCandidate(new_tree, expanded_list, next_no_recurse);
		}
	}
}


void StepUpCandidate(Node const&orig, list<Node> &expanded_list) {
	// For the top most completed rule:

	// Find rules that 
	// 1. fit into the parent
	// 2. descendent fits into
	

	vector<Node const*> top_completes;
	orig.get_most_recently_completed(top_completes);

	// if(top_completes.size()>0)
	// 	fprintf(stderr, "StepUpCandidate: %s completes %i\n", orig.ToString().c_str(), int(top_completes.size()));

	// Generate a candidate at each level there is a completed construct
	for(size_t top_complete_idx=0;top_complete_idx<top_completes.size();++top_complete_idx) {
		Node const*top_complete = top_completes[top_complete_idx];
		if(top_complete->parent) {
			// fprintf(stderr, "------ top_complete: %s\n", top_complete->ToString().c_str());

			vector<Rule> const&rules_fit_parent = GetRulesForTokenName(top_complete->rule.token_name);
			for(Rule const&r : rules_fit_parent) {
				// TODO: Optimize this search?
				if((r.pattern[0] == top_complete->rule.token_name) && RuleCanBeChild(top_complete->parent->rule, r) && RuleCanBeChild(r, top_complete->rule)) {
					// fprintf(stderr, "---------- found: %s\n", sRules[r.name-1].name.c_str());
					Node tree_copy(orig);

					vector<Node*> top_complete_copys;
					tree_copy.get_most_recently_completed(top_complete_copys);

					Node *top_complete_copy = top_complete_copys[top_complete_idx];
					assert(top_complete_copy != top_complete);

					Node *orig_parent = top_complete_copy->parent;
					unsigned orig_parent_idx = top_complete_copy->parent_token_idx;
					unique_ptr<Node> top_complete_ptr(std::move(orig_parent->subs[orig_parent_idx]));
					assert(top_complete_ptr.get() == top_complete_copy);

					unique_ptr<Node> new_parent(new Node(r, orig_parent, orig_parent_idx));
					new_parent->next_token = 1;

					orig_parent->next_token = orig_parent_idx;
					orig_parent->subs[orig_parent_idx] = std::move(new_parent);
					// new_parent pointer is now invalid
					top_complete_ptr->parent_token_idx = 0;
					orig_parent->subs[orig_parent_idx]->subs[0] = std::move(top_complete_ptr);
					orig_parent->subs[orig_parent_idx]->subs[0]->recursively_set_parent_next_tokens_here();

					SanityCheckTree(tree_copy);
					// fprintf(stderr, "---------- expanded_list: %s\n", tree_copy.ToString().c_str());
					expanded_list.push_back(tree_copy);
				}
			}
		}
	}
}

void ConsumeToken(Token const&next, list<Node> &candidates) {
	// fprintf(stderr, "Before erase empty:\n");

	// Erase empty
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();) {
		if(it->next_token == it->tokens.size()) {
			it = candidates.erase(it);
		} else {
			++it;
		}
	}

	// fprintf(stderr, "Before expand:\n");

	// First expand, recursively but without self-recursion
	// TODO: Do this before or after stepping up?
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();++it) {
		if(IsRuleTokenName(it->get_next_leaf())) {
			list<Node> expanded_list;
			ExpandCandidate(*it, expanded_list);

			it = candidates.erase(it);

			for(Node const&exp : expanded_list) {
				it = candidates.insert(it, exp);
			}
		}
	}

	// TODO: Prevent duplicates
	// fprintf(stderr, "Before step up:\n");
//	PrintCandidates(candidates);

	// Next, step up
	// TODO: Do this before or after expanding?
	// TODO: Do this for complete only, or all?
	list<Node> expanded_list;
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();++it) {
		StepUpCandidate(*it, expanded_list);
	}
	for(Node const&exp : expanded_list) {
		// Insert at the beginning so we don't feed back within the loop
		candidates.push_back(exp);
	}

	// TODO: Prevent duplicates
	 // fprintf(stderr, "------- Before filter:\n");
	// PrintCandidates(candidates);

	// Now filter
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();) {
		Token next_leaf = it->get_next_leaf();

		const Token next_type = GetTokenInstType(next);
		const Token leaf_type = GetTokenInstType(next_leaf);

		//fprintf(stderr, "Filter %s: next %s leaf %s\n", it->ToString().c_str(), TokenToString(next).c_str(), TokenToString(next_leaf).c_str());

		if(next_type == leaf_type) {
			++it;
		} else {
			it = candidates.erase(it);
		}
	}

	// fprintf(stderr, "Before advance:\n");

	// Now advance
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();++it) {
		it->advance(next);
	}

	// fprintf(stderr, "Remove ambiguity:\n");
	// Resolve ambiguity & lower number of candidates by leaving only one complete
	// TODO: This doesn't really work. The whole tree is not complete until the last token
	if(false) {
		bool found_complete = false;
		unsigned n_ambiguities_removed = 0;
		for(list<Node>::iterator it = candidates.begin();it != candidates.end();) {
			if(it->complete()) {
				if(found_complete) {
					it = candidates.erase(it);
					++n_ambiguities_removed;
				} else {
					found_complete = true;
					++it;
				}
			} else {
				++it;
			}
		}

		if(n_ambiguities_removed)
			fprintf(stderr, "WARNING: Removed %i ambiguities\n", n_ambiguities_removed);
	}
}


int main() {
	const Token top_token = GetTokenTypeId("top");
	if(!top_token) {
		fprintf(stderr, "No top rules found!\n");
		return 1;
	}
	vector<Rule> const&top_rules = GetRulesForTokenName(GetTokenInstName("top", ""));
	assert(top_rules.size() == 1);

	Node top_rule_inst(top_rules[0], 0, 0);
	list<Node> candidates;
	candidates.push_back(top_rule_inst);

	// Parse

	// TOO SLOW
	char in_str[] = 
"   void PopCount(\n"
"       ac_int<ac::log2_ceil<N + 1>::val, false> in) {\n"
"  }";

	// char in_str[] = "int foo() { return a+b*c>1;} int bar() { return 11;} ";
	// char in_str[] = "int foo() { int i((int) adouble);} ";
	//char in_str[] = "void foo() {A a(B(x));}";

	FILE *input = fmemopen(in_str, strlen(in_str)+1, "r");
	
	yyset_in(input);

	fprintf(stderr, "--- Parsing starts ---\n");

	while(true) {
		Token tok = yylex();
		if(tok == 0) {
			break;
		}

		const string content = GetTokenInstContent(tok);
		string type_name = GetTokenInstTypeName(tok);
		if(type_name == "ID" && (content == "ac" || content == "ac_int" || content == "log2_ceil")) {
			tok = LexGetTokenInstName("TYPEID", content.c_str());
		}
		
		fprintf(stderr, "\n\nNext %s\n", TokenToString(tok).c_str());

		fprintf(stderr, "Candidates (%i):\n", candidates.size());
	//	PrintCandidates(candidates);
		ConsumeToken(tok, candidates);

		if(candidates.size() == 0) {
			fprintf(stderr, "ERROR at %i\n", yylineno);
			exit(1);
		}
	}

	// Erase incomplete
	for(list<Node>::iterator it = candidates.begin();it != candidates.end();) {
		if(!it->complete()) {
			it = candidates.erase(it);
		} else {
			++it;
		}
	}

	// Ambiguities are removed
	//assert(candidates.size() == 1);

	fprintf(stderr, "Final Complete Candidates (count = %i):\n", int(candidates.size()));
	PrintCandidates(candidates);
	fprintf(stderr, "Pretty printed:\n");
	PrettyPrintCandidates(candidates);

	fclose(input);

	return 0;
}
