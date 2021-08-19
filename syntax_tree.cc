
#include "syntax_tree.h"
#include <cassert>

using namespace std;

namespace parser {

SyntaxTree::SyntaxTree() {
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
	return &node_array_[0];
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

	if(incomplete->parsed.size() == incomplete->rule.pattern.size()) {
		// Move up: 
		// - Setting completion flags
		// - Finally, innserting the work ptr at first incomplete
		for(Node *n = incomplete;n;n = n->parent) {
			if(n->parsed.size() == incomplete->rule.pattern.size()) {
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
	assert(work_ptrs_.size() > 0);
	assert(node_array_.size() > 0);

	fprintf(stderr, "--- consume ---\n");

	const TokenType next_tok_type = GetTokenInstType(next.tok);

	const LexedTokenIdx lexed_idx = token_array_.size();
	token_array_.push_back(next);

	const auto prev_work_ptrs = std::move(work_ptrs_);

	for(Node* incomplete : prev_work_ptrs) {
		assert(!incomplete->is_complete);

		if(!ConsumeInNode(incomplete, next_tok_type, lexed_idx)) {
			const Token next_tok = NextTokenInPattern(incomplete);

			fprintf(stderr, "-- next %s\n", TokenToString(next_tok).c_str());

			StepContext ctx;
			ctx.lexed = next.tok;
			ctx.needed_rule = next_tok;
			std::pair<StepDownMap::iterator, StepDownMap::iterator> found = GetStepDowns(ctx);
			if(found.first != found.second) {
				incomplete->parsed.emplace_back(std::move(ParsedSlot()));
				for(auto it = found.first; it != found.second; ++it) {
					fprintf(stderr, "TODO: Step down\n");
					for(RuleName down_rule : it->second) {
						fprintf(stderr, "-- %s\n", GetRuleName(down_rule));

						Node* new_node = AddNode(GetRuleByName(down_rule));
						
						// AddNode can invalidate all Node* pointers..

						new_node->parent = incomplete;
						incomplete->parsed.back().subs.insert(new_node);
						assert(!new_node->is_complete);
						const bool ret = ConsumeInNode(new_node, next_tok_type, lexed_idx);
						assert(ret);
						if(!ret) {
							fprintf(stderr, "Internal consistency failure 180811\n");
							return false;
						}
					}
				}
			} else {
				fprintf(stderr, "TODO: step up?\n");

			}
		}
	}

	return Complete() || (work_ptrs_.size() > 0);
}

Node* SyntaxTree::AddNode(Rule const&rule) {
	Node n(rule);
	node_array_.emplace_back(std::move(n));
	Node* new_node = &node_array_.back();

	return new_node;
}

};  // namespace parser
