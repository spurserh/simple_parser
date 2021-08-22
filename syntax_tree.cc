
#include "syntax_tree.h"
#include <sstream>
#include <cassert>

using namespace std;

namespace parser {

static size_t sInitialNodesAlloc = 1024;
static size_t sNodesAlign = 32;

SyntaxTree::SyntaxTree()
	: node_array_(sInitialNodesAlloc, sNodesAlign) {
}

bool SyntaxTree::Init(char const* top_rule_name) {
	assert(work_ptrs_.size() == 0);
	assert(node_array_.size() == 0);

	Token top_tok = parser::GetTokenInstName(top_rule_name);
	if (top_tok == 0) {
		return false;
	}

	vector<Rule> const&top_rules = GetRulesForTokenName(top_tok);
	assert(top_rules.size() == 1);

	if(top_rules.size() == 0) {
		return false;
	}

	Node* new_node = AddNode(top_rules[0]);

	work_ptrs_.insert(new_node);
	// Top rule can't be a step-up (rules.size() == 1)

	return true;
}

bool SyntaxTree::Complete()const {
	return GetTop()->is_complete;
}

Node const*SyntaxTree::GetTop()const {
	assert(node_array_.size() > 0);
	if(node_array_.size() == 0) {
		return 0;
	}
	return node_array_.get_first();
}

Token SyntaxTree::NextTokenInPattern(Node const*node)const {
	assert(node->rule.pattern.size() > node->parsed.size());
	return node->rule.pattern[node->parsed.size()];
}

bool SyntaxTree::ConsumeInNode(Node* incomplete, TokenType next_tok_type, LexedTokenIdx lexed_idx) {
	const Token next_tok = NextTokenInPattern(incomplete);

	if(GetTokenInstType(next_tok) != next_tok_type) {
		return false;
	}

	incomplete->parsed.emplace_back(std::move(ParsedSlot(lexed_idx)));

fprintf(stderr, "emplaced %s\n", TokenToString(GetLexedTokenByIdx(lexed_idx).tok).c_str());

	if(incomplete->parsed.size() == incomplete->rule.pattern.size()) {
		// Move up: 
		// - Setting completion flags
		// - Finally, inserting the work ptr at first incomplete
		for(Node *n = incomplete;n;n = n->parent) {
			if(n->parsed.size() == n->rule.pattern.size()) {
				n->is_complete = true;
				assert((n==incomplete) || n->parsed.back().subs.size());
			} else {
				// Set semantics will ensure no duplicates
				work_ptrs_.insert(n);
				break;
			}
		}
	} else {
		// Continue working on this node
		work_ptrs_.insert(incomplete);
	}
	return true;
}

bool SyntaxTree::ConsumeToken(LexedToken const&next) {
	if(work_ptrs_.size() == 0) {
		return false;
	}

	assert(node_array_.size() > 0);

	fprintf(stderr, "--- consume %s ---\n", TokenToString(next.tok).c_str());

	assert(TokenIsLexical(next.tok));

	const TokenType next_tok_type = GetTokenInstType(next.tok);

	token_array_.push_back(next);
	// Index from 1
	const LexedTokenIdx lexed_idx = token_array_.size();

	const auto prev_work_ptrs = std::move(work_ptrs_);

	for(Node* incomplete : prev_work_ptrs) {
		assert(!incomplete->is_complete);

		if(ConsumeInNode(incomplete, next_tok_type, lexed_idx)) {
			continue;
		}

		const Token next_tok = NextTokenInPattern(incomplete);

		// Lexical mismatch: delete node
		if(TokenIsLexical(next_tok)) {
			DeleteNode(incomplete);
			continue;
		}

//		fprintf(stderr, "-- next %s\n", TokenToString(next_tok).c_str());

		StepContext ctx;
		ctx.lexed = GetTokenInstType(next.tok);
		ctx.needed_rule = next_tok;

		std::pair<StepDownMap::iterator, StepDownMap::iterator> found = GetStepDowns(ctx);
		if(found.first != found.second) {

			// One new slot for all step downs (multiple subs created)
			incomplete->parsed.emplace_back(std::move(ParsedSlot()));

			// For each step down stack (ambiguous)
			for(auto it = found.first; it != found.second; ++it) {

				Node *parent = incomplete;

				// For each rule in the stack
				for(RuleName down_rule : it->second) {

					Node* new_node = AddNode(GetRuleByName(down_rule));

					new_node->parent = parent;
					if(parent != incomplete) {
						parent->parsed.emplace_back(std::move(ParsedSlot()));
					}
					parent->parsed.back().subs.insert(new_node);
					parent = new_node;
					assert(!new_node->is_complete);
				}

				const bool ret = ConsumeInNode(parent, next_tok_type, lexed_idx);
				assert(ret);
				if(!ret) {
					fprintf(stderr, "Internal consistency failure 180811\n");
					return false;
				}
			}
		} else {
			fprintf(stderr, "TODO: step up?\n");
		}
	}

	fprintf(stderr, "\n>> After ConsumeToken (ptrs %i, complete %i):\n%s\n",
		(int)work_ptrs_.size(), (int)Complete(),
		ToString(0).c_str());

	return Complete() || (work_ptrs_.size() > 0);
}

Node* SyntaxTree::AddNode(Rule const&rule) {
//	Node n(rule);
//	node_array_.emplace_back(std::move(n));

	return new (node_array_.allocate()) Node (rule);
}

void SyntaxTree::DeleteNode(Node* n) {
	assert(n!=GetTop());
	assert(n->parent);

	Node* to_remove = n;
	Node* remove_from = n->parent;

	// Find first ancestor with >1 sub in the last slot, or top
	while(true) {
		assert(remove_from->parsed.size());
		size_t n_subs_last = remove_from->parsed.back().subs.size();
		assert(n_subs_last > 0);
		if((n_subs_last > 1) || (remove_from == GetTop())) {
			break;
		}

		// Move up
		to_remove = remove_from;
		remove_from = remove_from->parent;
	}

	assert(to_remove);
	assert(remove_from);

	const size_t prev_size = remove_from->parsed.back().subs.size();
	remove_from->parsed.back().subs.erase(to_remove);
	assert(remove_from->parsed.back().subs.size() < prev_size);
}

void SyntaxTree::MakeIndent(ostringstream &ostr, int n) {
	ostr << "\n";
	for(;n>0;--n) {
		ostr << "\t";
	}
}

std::string SyntaxTree::ToString(int multiline, Node const*n)const {
	if(n == 0) {
		n = GetTop();
	}

	ostringstream ostr;

	ostr << GetRuleName(n->rule.name) << " {";
	MakeIndent(ostr, multiline+1);

	for(size_t i=0;i<n->rule.pattern.size();++i) {

		auto separate = [i, n, &ostr, multiline]() {
			if(i < (n->rule.pattern.size()-1)) {
				ostr << " ";
				MakeIndent(ostr, multiline+1);
			} else {
				MakeIndent(ostr, multiline);
			}
		};

		if(i >= n->parsed.size()) {
			if(work_ptrs_.contains(const_cast<Node*>(n))) {
				ostr << "^";
			}

			ostr << TokenToString(n->rule.pattern[i]);
			separate();
			continue;
		}

		ParsedSlot const&slot = n->parsed[i];
		if(slot.lexed_idx) {
			LexedToken const&lexed = GetLexedTokenByIdx(slot.lexed_idx);
			ostr << TokenToString(lexed.tok);
			separate();
			continue;
		}

		if(slot.subs.size() > 1) {
			ostr << "(";
			MakeIndent(ostr, multiline+1);
		}
		size_t si = 0;
		for(Node const*sn : slot.subs) {
			ostr << ToString((multiline<0)?-1:(multiline+1), sn);
			if(multiline < 0) {
				if(si < (slot.subs.size()-1)) {
					ostr << " ";
				}
			}
			++si;
		}
		if(slot.subs.size() > 1) {
			ostr << ")";
			MakeIndent(ostr, multiline+1);
		}

		separate();
	}

	ostr << "}";
	MakeIndent(ostr, multiline);

	return ostr.str();
}

LexedToken const&SyntaxTree::GetLexedTokenByIdx(LexedTokenIdx idx)const {
	assert(idx>0);
	assert(idx<=token_array_.size());
	return token_array_[idx-1];
}

};  // namespace parser
