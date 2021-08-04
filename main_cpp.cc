
#include <cassert>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <list>
#include <map>
#include <set>

#include <sys/time.h>

#include <stdarg.h>


#define DEBUG 0
#define PROFILING 1
#define SHOW_STEP_DOWNS 0
#define SHOW_STEP_UPS 0


#include "parser.h"

using namespace parser;


bool NodeHasOperatorPriority(Node const&node) {
	const Token expr_rule = GetTokenInstName("expr", "");

	if(expr_rule != node.rule->token_name) {
		return false;
	}

	if(node.rule->priority == 0) {
		return false;
	}

	return true;
}

bool ViolatesOperatorRules(Candidate const&cand) {
	Node const&node = cand.get_node(cand.top_completed);

	if(!NodeHasOperatorPriority(node)) {
		return false;
	}

	assert(node.parsed_tokens.size() == node.rule->pattern.size());

	for(Node::ParsedToken const&parsed : node.parsed_tokens) {
		if(parsed.sub == NodeId_Null) {
			continue;
		}
		Node const&sub_node = cand.get_node(parsed.sub);
		if(!NodeHasOperatorPriority(sub_node)) {
			continue;
		}

		// Some operators have higher priority regardless of lexical position
		if(node.rule->priority < sub_node.rule->priority) {
			return true;
		}
		
		// Last encloses (first has highest priority, ie left to right, top to bottom)
		if((node.rule->priority == sub_node.rule->priority) && 
			(cand.get_first_lexical_token_index(cand.top_completed) <
				cand.get_first_lexical_token_index(parsed.sub))) {
			return true;
		}
	}
	return false;
}

typedef string Identifier;

enum DeclType {
	DeclType_Null=0,
	DeclType_Func,
	DeclType_Typedef,
	DeclType_Class
};

struct Decl {
	DeclType type;
	bool templated;

	Decl() : type(DeclType_Null), templated(false) { }
};

void GenerateError(const char*fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(1);
}

struct CPPType {

};

struct CPPContext {
	absl::flat_hash_set<NodeId> already_visited;
	absl::flat_hash_map<Identifier, Decl> decls;

	bool ParseType(Candidate const&cand, NodeId type_nid, CPPType &to_type) {
		const RuleName templated_type = GetRuleNameInefficiently("templated_type");
		const RuleName int_type = GetRuleNameInefficiently("int_proto");
		const RuleName id_type = GetRuleNameInefficiently("id_type");

		Node const&node = cand.get_node(type_nid);
		const RuleName this_rule = node.rule->name;

		if(this_rule == templated_type) {
			const NodeId first_sub = node.parsed_tokens[0].sub;
			// TODO
			CPPType sub_type;
			return ParseType(cand, first_sub, sub_type);

		} else if(this_rule == int_type) {
			// TODO
			return true;
		} else if(this_rule == id_type) {
			const Token id_tok = node.parsed_tokens[0].lexed;
			assert(TokenIsLexical(id_tok));
			const string id = GetTokenInstContent(id_tok);
			if(!decls.contains(id)) {
				GenerateError("Undeclared Identifier: %s", id.c_str());
				
				// Or Minimalism: Don't have to keep whole context here
				return false;
			}
			if(!((decls[id].type == DeclType_Typedef) || 
			     (decls[id].type == DeclType_Class))) {
				return false;
			}
			// TODO
			return true;
		}

		return false;
	}
	
	bool ViolatesContextRules(Candidate const&cand, NodeId nid) {
		Node const&node = cand.get_node(nid);

		

#if 0
		assert(cand.is_complete(nid));
		
		// Already visited can miss child nodes
		if(!already_visited.contains(nid)) {
//			already_visited.insert(nid);
			
			const RuleName this_rule = node.rule->name;
			
			const RuleName func_proto_templated = GetRuleNameInefficiently("func_proto_templated");
			const RuleName func_proto = GetRuleNameInefficiently("func_proto");

			if((this_rule == func_proto) || (this_rule == func_proto_templated)) {
				const bool is_templated = (this_rule == func_proto_templated);
				const unsigned index_id = is_templated ? 2 : 1;
				const string id = GetTokenInstContent(node.parsed_tokens[index_id].lexed);
				if(!decls.contains(id)) {
					fprintf(stderr, "---- new decl %s\n", id.c_str());
					Decl new_decl;
					new_decl.templated = is_templated;
					new_decl.type = DeclType_Func;
					decls[id] = new_decl;
				} else {
					GenerateError("Redefined Identifier: %s", id.c_str());
				}
			}

			const RuleName cpp_cast_expr = GetRuleNameInefficiently("cpp_cast_expr");
			if(this_rule == cpp_cast_expr) {
				fprintf(stderr, "----- cpp cast\n");
				const NodeId type_nid = node.parsed_tokens[0].sub;
				CPPType to_type;
				if(!ParseType(cand, type_nid, to_type)) {
					return true;
				}
			}
			const RuleName id_expr = GetRuleNameInefficiently("id_expr");
			if(this_rule == id_expr) {
				const Token id_lexed = node.parsed_tokens[0].lexed;
				assert(TokenIsLexical(id_lexed));
				const string id = GetTokenInstContent(id_lexed);
				fprintf(stderr, "--- id_expr %s\n", id.c_str());
			}
			const RuleName id_type = GetRuleNameInefficiently("id_type");
			if(this_rule == id_type) {
				const Token id_lexed = node.parsed_tokens[0].lexed;
				assert(TokenIsLexical(id_lexed));
				const string id = GetTokenInstContent(id_lexed);
				fprintf(stderr, "--- id_type %s\n", id.c_str());
			}

			const RuleName lt_expr = GetRuleNameInefficiently("lt_expr");
			if(this_rule == lt_expr) {
				fprintf(stderr, "----- lt_expr %s\n", 
						cand.ToString(nid).c_str());
				const NodeId first_sub_nid = node.parsed_tokens[0].sub;
				Node const&first_sub_node = cand.get_node(first_sub_nid);
		
				const RuleName id_expr = GetRuleNameInefficiently("id_expr");
				if(first_sub_node.rule->name == id_expr) {
					const Token id_lexed = first_sub_node.parsed_tokens[0].lexed;
					assert(TokenIsLexical(id_lexed));
					const string id = GetTokenInstContent(id_lexed);
					fprintf(stderr, "  ----- id_expr id %s\n", id.c_str());
					if(decls.contains(id)) {
						fprintf(stderr, "    -- templated %i\n", (int)decls[id].templated);
						if(decls[id].templated) {
							fprintf(stderr, "      ------ lt_expr return true\n");
							return true;
						}
					} else {
						GenerateError("Undeclared Identifier: %s", id.c_str());
					}
				}
			}
		}

		// Recurse
		if(node.parent && cand.is_complete(node.parent)) {
			if(ViolatesContextRules(cand, node.parent)) {
				return true;
			}
		}
#endif
		return false;
	}
};


void on_exit() {
	if(sStartTime == 0) {
		return;
	}
	const double end_time = doubletime();
	fprintf(stderr, "Parsing time %fms\n", (end_time-sStartTime) * 1000.0);
	sStartTime = 0;
}



// ---- / grammar specific ----


extern "C" {
extern int yylex (void);
void yyset_in  ( FILE * _in_str  );
extern int yylineno;
};


int main(int argc, const char **argv) {

	SetupParser();


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

	CPPContext ctx;

	sStartTime = doubletime();

	for(unsigned token_index=0;;++token_index) {
		Token tok = yylex();
		if(tok == 0) {
			break;
		}

		string tok_type_name = GetTokenInstTypeName(tok);

#if !PROFILING
		fprintf(stderr, "\n\n---- Next %s (line %i), candidates before %i\n",
			TokenToString(tok).c_str(), yylineno, (int)candidates.size());
#endif

#if !PROFILING
//#if 1
		PrintCandidates(candidates);
		CandidateVector dbg_candidates = candidates;
#endif

		ConsumeToken(tok, token_index, yylineno, candidates);

		if(candidates.size() == 0) {
			// TODO: Report line number in preprocessed file
			fprintf(stderr, "ERROR at line %i, token %s\n", yylineno, tok_type_name.c_str());

#if DEBUG
			fprintf(stderr, "\nFinal candidates (%i):\n", (int)dbg_candidates.size());
			PrintCandidates(dbg_candidates, true);
#endif
			exit(1);
		}

		absl::InlinedVector unfiltered = candidates;
		candidates.clear();
		for(Candidate const&cand : unfiltered) {
			if((cand.top_completed == NodeId_Null) || 
				((!ViolatesOperatorRules(cand)) && (!ctx.ViolatesContextRules(cand, cand.top_completed)))) {
				candidates.push_back(cand);
			}
		}
	}

	on_exit();

	CandidateVector completed_candidates;
	for(Candidate const&cand : candidates) {
		if(cand.is_complete()) {
			completed_candidates.push_back(cand);
		}
	}

	fprintf(stderr, "\nCompleted candidates (%i):\n", (int)completed_candidates.size());
	PrintCandidates(completed_candidates, true);
	if(completed_candidates.size()>1) {
		GenerateError("Ambiguous parse");
	} else if(completed_candidates.size() == 0) {
		GenerateError("Incomplete parse");		
	}

	fclose(input);

	return 0;
}