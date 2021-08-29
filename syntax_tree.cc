
#include "syntax_tree.h"
#include <sstream>
#include <cassert>

#include <boost/range/adaptor/reversed.hpp>

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
	return IsComplete(GetTop());
}

Node *SyntaxTree::GetTop()const {
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


bool SyntaxTree::NodeContainsWorkPtr_slow(Node *n)const {
	if(work_ptrs_.contains(n)) {
		return true;
	}

//	if(n->parsed.size() > 0) {
	for(ParsedSlot const&slot : n->parsed) {
		for(Node *sn : slot.subs) {
			if(NodeContainsWorkPtr_slow(sn)) {
				return true;
			}
		}
	}
	return false;
}


bool SyntaxTree::ConsumeInNode(Node* incomplete,
							   TokenType next_tok_type,
							   LexedTokenIdx lexed_idx) {
	fprintf(stderr, "ConsumeInNode %s in %s\n",
		GetRuleName(incomplete->rule.name),
		GetTokenTypeName(next_tok_type));

	const Token next_tok = NextTokenInPattern(incomplete);

	if(GetTokenInstType(next_tok) != next_tok_type) {
		return false;
	}

//	incomplete->parsed.emplace_back(std::move(ParsedSlot(lexed_idx)));
	incomplete->AddParsed(lexed_idx);


	return true;
}

void SyntaxTree::UpdateWorkPtr(Node* incomplete) {
//	if(incomplete->parsed.size() == incomplete->rule.pattern.size()) {
	if(IsComplete(incomplete)) {
		MarkCompleteAndMoveUp(incomplete);
	} else {
		// Continue working on this node
		work_ptrs_.insert(incomplete);
	}
}

bool SyntaxTree::ConsumeToken(LexedToken const&next) {
	if(work_ptrs_.size() == 0) {
		return false;
	}

	assert(node_array_.size() > 0);

	fprintf(stderr, "--- consume %s ---\n", TokenToString(next.tok).c_str());
/*
fprintf(stderr, "ConsumeToken tree:\n%s\n", 
	ToString(0).c_str());
*/

	assert(TokenIsLexical(next.tok));

	const TokenType next_tok_type = GetTokenInstType(next.tok);

	token_array_.push_back(next);
	// Index from 1
	const LexedTokenIdx lexed_idx = token_array_.size();

	const auto prev_work_ptrs = std::move(work_ptrs_);

	for(Node* incomplete : prev_work_ptrs) {
		assert(!IsComplete(incomplete));

fprintf(stderr, "*** next work_ptr tree:\n%s\n", 
	ToString(0, incomplete).c_str());


		if(ConsumeInNode(incomplete, next_tok_type, lexed_idx)) {
			UpdateWorkPtr(incomplete);
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
			//incomplete->parsed.emplace_back(std::move(ParsedSlot()));
			incomplete->AddParsed();

			absl::InlinedVector<Node*, 4> last_descendants;

			// For each step down stack (ambiguous)
			for(auto it = found.first; it != found.second; ++it) {
				StepDownStack const&stack = it->second;

				Node* last_descendant;
				Node* top_of_stack = BuildStepDownStack(stack, last_descendant);

				incomplete->parsed.back().subs.insert(top_of_stack);
				top_of_stack->parent = incomplete;

				// Completion needs to happen as a separate step..
				const bool ret = ConsumeInNode(last_descendant, next_tok_type, lexed_idx);
				assert(ret);
				if(!ret) {
					fprintf(stderr, "Internal consistency failure 180811\n");
					return false;
				}

				last_descendants.emplace_back(last_descendant);
			}

			for(Node* last_descendant : last_descendants) {
				UpdateWorkPtr(last_descendant);
			}

		} else {
			fprintf(stderr, "TODO: step up?\n");
		}
	}

	fprintf(stderr, "\n>> After ConsumeToken (ptrs %i, complete %i):\n%s\n",
		(int)work_ptrs_.size(), (int)Complete(),
		ToString(0).c_str());

	assert(NodeIsSane(GetTop()));

	return Complete() || (work_ptrs_.size() > 0);
}

bool SyntaxTree::NodeIsSane(Node *n)const {
	if(n->parsed.size() > 0) {
		// All nodes must be complete to the left of the newest
		for(int li=0;li<(n->parsed.size()-1);++li) {
			ParsedSlot const&slot = n->parsed[li];
			for(Node* sub : slot.subs) {
				if(!CheckComplete_slow(sub)) {
					return false;
				}
				if(!IsComplete(sub)) {
					return false;
				}
				/*
				assert(sub->subs_complete == sub->parsed.back().subs.size());
				if(sub->subs_complete != sub->parsed.back().subs.size()) {
					return false;
				}*/
			}
		}
		/*
		assert(n->subs_complete == CountSubsComplete_slow(n));
		if(n->subs_complete != CountSubsComplete_slow(n)) {
			return false;
		}
		*/

	}
	return true;
}

Node* SyntaxTree::BuildStepDownStack(StepDownStack const&stack,
									 NodePtr& last_descendant) {
	Node* child = 0;

	for(RuleName down_rule : boost::adaptors::reverse(stack)) {
		Node* new_node = AddNode(GetRuleByName(down_rule));

		if(child) {
//			new_node->parsed.emplace_back(std::move(ParsedSlot(child)));
			new_node->AddParsed(child);
			child->parent = new_node;
		}

		if(!child) {
			last_descendant = new_node;
		}
		child = new_node;
	}
	return child;
}

void SyntaxTree::MarkCompleteAndMoveUp(Node* incomplete) {
	// Move up: 
	// - Setting completion flags
	// - Finally, inserting the work ptr at first incomplete
	fprintf(stderr, "MarkCompleteAndMoveUp {\n");
	for(Node *n = incomplete->parent;n;n = n->parent) {

		fprintf(stderr, "-- n %s subs %i/%i\n",
			GetRuleName(n->rule.name),
			CountSubsComplete_slow(n),
			n->parsed.back().subs.size());

		if(n->parsed.back().subs.size()) {
			fprintf(stderr, "--- %s\n",
					GetRuleName((*n->parsed.back().subs.begin())->rule.name));
		}

		//++n->subs_complete;
		//assert(n->subs_complete == CountSubsComplete_slow(n));

		if(n->parsed.size() == n->rule.pattern.size()) {
			assert((n==incomplete) || n->parsed.back().subs.size());
		} else {
			if(n->parsed.size() > 0) {
				ParsedSlot const&last_slot = n->parsed.back();
				bool all_subs_complete = true;
				for(Node* sub : last_slot.subs) {
					if(!IsComplete(sub)) {
						all_subs_complete = false;
						break;
					}
				}
				if(!all_subs_complete) {
					// Cannot advance work ptr beyond incomplete node
					// Need to resolve by cloning / splitting the node

					fprintf(stderr, "TODO: Not all subs complete in %s!\n", GetRuleName(n->rule.name));
					exit(1);
				}
			}
			// Set semantics will ensure no duplicates
			work_ptrs_.insert(n);
			break;
		}
	}
	fprintf(stderr, "MarkCompleteAndMoveUp }\n");
}

bool SyntaxTree::IsComplete(Node* n)const {
	if(n->parsed.size() < n->rule.pattern.size()) {
		return false;
	}
	assert(n->parsed.size() > 0);
//	if(n->subs_complete < n->parsed.back().subs.size()) {
	if(CountSubsComplete_slow(n) < n->parsed.back().subs.size()) {
		return false;
	}

	assert(CheckComplete_slow(n));
	//assert(n->subs_complete == CountSubsComplete_slow(n));

	return true;
}

bool SyntaxTree::CheckComplete_slow(Node *n)const {
	for(ParsedSlot const&slot : n->parsed) {
		for(Node* sub : slot.subs) {
			if(!CheckComplete_slow(sub)) {
				return false;
			}
		}
	}
	return n->parsed.size() == n->rule.pattern.size();
}

unsigned SyntaxTree::CountSubsComplete_slow(Node *n)const {
	if(n->parsed.size() <= 0) {
		return 0;
	}
	unsigned n_complete = 0;

	for(Node* sub : n->parsed.back().subs) {
		if(CheckComplete_slow(sub)) {
			++n_complete;
		}
	}

	return n_complete;
}

Node* SyntaxTree::AddNode(Rule const&rule) {
//	Node n(rule);
//	node_array_.emplace_back(std::move(n));

	return new (node_array_.allocate()) Node (rule);
}

void SyntaxTree::DeleteNode(Node* n) {
	assert(n!=GetTop());
	assert(n->parent);

	if(NodeContainsWorkPtr_slow(n)) {
		fprintf(stderr, "TODO: Remove work_ptr from deleted node\n");
		exit(1);
	}

fprintf(stderr, "Delete %s, tree:\n%s\n", 
	GetRuleName(n->rule.name),
	ToString(0).c_str());

	Node* to_remove = n;
	Node* remove_from = n->parent;

	// Find first ancestor with >1 sub in the last slot, or top
	while(true) {
		assert(remove_from->parsed.size());
		size_t n_subs_last = remove_from->parsed.back().subs.size();
		assert(n_subs_last > 0);

fprintf(stderr, "At remove_from %s (s %i) to_remove %s (s %i)\n", 
	GetRuleName(remove_from->rule.name),
	(int)remove_from->parsed.back().subs.size(),
	GetRuleName(to_remove->rule.name),
	(int)to_remove->parsed.back().subs.size());


		if((n_subs_last > 1) || (remove_from == GetTop())) {
fprintf(stderr, "--- break\n");
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
			if((i == n->parsed.size()) && work_ptrs_.contains(const_cast<Node*>(n))) {
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
