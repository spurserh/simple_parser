
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

#include "parser.h"
#include "boost/multiprecision/cpp_int.hpp"

using namespace parser;

using namespace boost::multiprecision;


#define DEBUG 0
#define PROFILING 0
#define SHOW_STEP_DOWNS 0
#define SHOW_STEP_UPS 0



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


void on_exit() {
	if(sStartTime == 0) {
		return;
	}
	const double end_time = doubletime();
	fprintf(stderr, "Parsing time %fms\n", (end_time-sStartTime) * 1000.0);
	sStartTime = 0;
}

struct TristateValue {
	// ...
};

typedef absl::flat_hash_map<string, cpp_int> InterfaceValueMap;

void ParseCSV(const char*path, InterfaceValueMap &output) {
	std::ifstream in(path);
	if(!in.good()) {
		fprintf(stderr, "Failed to open test file: %s\n", path);
		exit(1);
	}
	for(unsigned tick=0;in;++tick) {
		char linebuf[256];
		in.getline(linebuf, sizeof(linebuf));

		const string line(linebuf);

		// Ignore empty lines
		if(line.find(',') == string::npos) {
			continue;
		}

		// Ignore header
		if(tick == 0) {
			continue;
		}

		std::istringstream linestr(line);

		while(linestr) {
			char colbuf[256];
			linestr.getline(colbuf, sizeof(colbuf), ',');
			cols.push_back(colbuf);
			cerr << "(" << colbuf << ")" << " ";

			cpp_int parsed(colbuf);
			if(std::string(parsed) != colbuf) {
				parsed = 55;				
			}
			cerr << parsed << " ";
		}


		fprintf(stderr, "\n");

	}
}

// ---- / grammar specific ----


extern "C" {
extern int yylex (void);
void yyset_in  ( FILE * _in_str  );
extern int yylineno;
};


int main(int argc, const char **argv) {

	InterfaceValueMap test_values;
	ParseCSV("verilog_sim/test.csv", test_values);



	return 1;

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
			if((cand.top_completed == NodeId_Null) || (!ViolatesOperatorRules(cand))) {
				candidates.push_back(cand);
			}
		}
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