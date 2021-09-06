
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

bool SyntaxTree::CanComplete()const {
	return CanComplete(GetTop());
}

bool SyntaxTree::FullyComplete()const {
	return CheckComplete_slow(GetTop());
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

	{
		const auto prev_work_ptrs = std::move(work_ptrs_);

		for(Node* n : prev_work_ptrs) {
			UpdateWorkPtr(n);
		}
	}

	const TokenType next_tok_type = GetTokenInstType(next.tok);

	token_array_.push_back(next);
	// Index from 1
	const LexedTokenIdx lexed_idx = token_array_.size();

	bool did_consume = false;

	const auto prev_work_ptrs = std::move(work_ptrs_);

	for(Node* incomplete : prev_work_ptrs) {
		assert(!IsComplete(incomplete));

fprintf(stderr, "*** next work_ptr tree:\n%s\n", 
	ToString(0, incomplete).c_str());


		if(ConsumeInNode(incomplete, next_tok_type, lexed_idx)) {
//			UpdateWorkPtr(incomplete);
			work_ptrs_.insert(incomplete);
			did_consume = true;
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

				did_consume = true;
				last_descendants.emplace_back(last_descendant);
			}

			for(Node* last_descendant : last_descendants) {
//				UpdateWorkPtr(last_descendant);
				work_ptrs_.insert(last_descendant);
			}

		} else {
			fprintf(stderr, "TODO: step up?\n");
		}
	}

	fprintf(stderr, "\n>> After ConsumeToken (ptrs %i, complete %i):\n%s\n",
		(int)work_ptrs_.size(), (int)IsComplete(GetTop()),
		ToString(0).c_str());

	for(Node *ptr : work_ptrs_) {
		fprintf(stderr, "-- %s\n", ToString(-1, ptr).c_str());
	}

	assert(NodeIsSane(GetTop()));

	return did_consume || work_ptrs_.size();
}

bool SyntaxTree::NodeIsSane(Node *n)const {
	if(n->parent) {
		bool found_in_parent = false;
		for(ParsedSlot const&slot : n->parent->parsed) {
			for(Node* sub : slot.subs) {
				if(sub == n) {
					found_in_parent = true;
					break;
				}
			}
			if(found_in_parent) {
				break;
			}
		}
		if(!found_in_parent) {
			fprintf(stderr, "Node is insane: didn't find in parent\n");
			return false;
		}
	}

	if(n->parsed.size() > 0) {
		// All nodes must be complete to the left of the newest
		for(int li=0;li<(n->parsed.size()-1);++li) {
			ParsedSlot const&slot = n->parsed[li];
			for(Node* sub : slot.subs) {
				if(!CheckComplete_slow(sub)) {
					fprintf(stderr, "Node is insane: !CheckComplete_slow() for left-hand sub\n");
					return false;
				}
				if(!IsComplete(sub)) {
					fprintf(stderr, "Node is insane: !IsComplete() for left-hand sub\n");
					return false;
				}
				if(NodeContainsWorkPtr_slow(sub)) {
					fprintf(stderr, "Node is insane: Left-hand sub contains work ptr\n");
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
	
	int inserted_work_ptrs = 0;

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
			ParsedSlot const&last_slot = n->parsed.back();
			bool all_subs_complete = true;
			for(Node* sub : last_slot.subs) {
				if(!IsComplete(sub)) {
					all_subs_complete = false;
					break;
				}
			}
			if(!all_subs_complete) {
				absl::InlinedVector<Node*, 2> completes;
				absl::InlinedVector<Node*, 2> incompletes;

				for(Node* sub : last_slot.subs) {
					if(IsComplete(sub)) {
						completes.push_back(sub);
					} else {
						incompletes.push_back(sub);
					}
				}

				// Cannot advance work ptr beyond incomplete node
				// Need to resolve by cloning / splitting the node

				// Shallow copy is fine for all but last ParsedSlot
				// For that slot, complete subs go into one copy, incomplete into the other
				Node* copy_for_completes = ShallowCopyNode(n);

				
				for(Node* sub : incompletes) {
					copy_for_completes->parsed.back().subs.erase(sub);
				}
				for(Node* sub : completes) {
					n->parsed.back().subs.erase(sub);
					sub->parent = copy_for_completes;
				}

				assert(n->parent);
				n->parent->parsed.back().subs.insert(copy_for_completes);
				
				assert(NodeIsSane(n));
				assert(NodeIsSane(copy_for_completes));

				work_ptrs_.insert(copy_for_completes);
				++inserted_work_ptrs;
			} else {
				work_ptrs_.insert(n);
				++inserted_work_ptrs;
			}
			break;
		}
	}
	fprintf(stderr, "===== MarkCompleteAndMoveUp inserted_work_ptrs %i }\n",
		inserted_work_ptrs);
	if(inserted_work_ptrs == 0) {
		DeleteNode(incomplete);
	}
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

bool SyntaxTree::CanComplete(Node* n)const {
	if(n->parsed.size() != n->rule.pattern.size()) {
		return false;
	}
	if(n->parsed.back().subs.size()) {
		bool any_subs_can_complete = false;
		for(Node* sub : n->parsed.back().subs) {
			if(CanComplete(sub)) {
				any_subs_can_complete = true;
				break;
			}
		}
		if(!any_subs_can_complete) {
			return false;
		}
	} else {
		if(!TokenIsLexical(n->rule.pattern[0])) {
			return false;
		}
	}
	return true;
}

void SyntaxTree::DeleteIncomplete() {
	// Collect incomplete nodes
	// (What if they are descendents?)
	while(true) {
		absl::InlinedVector<Node*, 4> incompletes;
		GetPatternIncompleteNodes(incompletes);
		if(incompletes.size() == 0) {
			break;
		}
fprintf(stderr, ">>> Deleting incomplete\n");
		DeleteNode(incompletes[0]);
	}	

	// Collect all incomplete leaves
	{
		absl::InlinedVector<Node*, 4> incompletes;
		GetIncompleteLeaves(incompletes);

		// Then delete, avoiding iteration issues
		for(Node* n : incompletes) {
			DeleteNode(n);
		}
	}
}

void SyntaxTree::GetIncompleteLeaves(absl::InlinedVector<Node*, 4> &output, Node* n) {
	if(n == 0) {
		n = GetTop();
	}

	if(n->rule.is_leaf && (n->parsed.size() < n->rule.pattern.size())) {
		output.push_back(n);
		return;
	}

	if(n->parsed.back().subs.size()) {
		for(Node* sub : n->parsed.back().subs) {
			GetIncompleteLeaves(output, sub);
		}
	}
}

void SyntaxTree::GetPatternIncompleteNodes(absl::InlinedVector<Node*, 4> &output, Node* n) {
	if(n == 0) {
		n = GetTop();
	}

	if((!n->rule.is_leaf) && (n->parsed.size() < n->rule.pattern.size())) {
		output.push_back(n);
		return;
	}

	if(n->parsed.back().subs.size()) {
		for(Node* sub : n->parsed.back().subs) {
			GetPatternIncompleteNodes(output, sub);
		}
	}
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
	return new (node_array_.allocate()) Node (rule);
}

Node* SyntaxTree::ShallowCopyNode(Node* n) {
	return new (node_array_.allocate()) Node (*n);
}

void SyntaxTree::RemoveWorkPtrsFromTree(Node *n) {
	work_ptrs_.erase(n);

	if(n->parsed.size() > 0) {
		for(Node* n : n->parsed.back().subs) {
			RemoveWorkPtrsFromTree(n);
		}
	}
}

void SyntaxTree::DeleteNode(Node* n) {
	assert(n!=GetTop());
	assert(n->parent);
/*
	if(NodeContainsWorkPtr_slow(n)) {
		fprintf(stderr, "TODO: Remove work_ptr from deleted node\n");
		exit(1);
	}
*/
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


		if((n_subs_last > 1) || (remove_from == GetTop())) {
			break;
		}

		// Move up
		to_remove = remove_from;
		remove_from = remove_from->parent;
	}


	assert(to_remove);
	assert(remove_from);
	RemoveWorkPtrsFromTree(to_remove);
	assert(!NodeContainsWorkPtr_slow(to_remove));
fprintf(stderr, "\nTODO %s\n", ToString(0, to_remove).c_str());

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
	if(multiline >= 0) {
		MakeIndent(ostr, multiline+1);
	}

	for(size_t i=0;i<=n->rule.pattern.size();++i) {

		auto separate = [i, n, &ostr, multiline]() {
			if(i <= (n->rule.pattern.size()-1)) {
				ostr << " ";
				if(multiline >= 0) {
					MakeIndent(ostr, multiline+1);
				}
			} else {
				if(multiline >= 0) {
					MakeIndent(ostr, multiline);
				}
			}
		};

		if(i >= n->parsed.size()) {
			if((i == n->parsed.size()) && work_ptrs_.contains(const_cast<Node*>(n))) {
				ostr << "^";
			}

			if(i < n->rule.pattern.size()) {
				ostr << TokenToString(n->rule.pattern[i]);
			}
			separate();
			continue;
		}

		if(i >= n->parsed.size()) {
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
			if(multiline >= 0) {
				MakeIndent(ostr, multiline+1);
			}
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
			if(multiline >= 0) {
				MakeIndent(ostr, multiline+1);
			}
		}

		separate();
	}

	ostr << "}";
	if(multiline >= 0) {
		MakeIndent(ostr, multiline);
	}

	return ostr.str();
}

LexedToken const&SyntaxTree::GetLexedTokenByIdx(LexedTokenIdx idx)const {
	assert(idx>0);
	assert(idx<=token_array_.size());
	return token_array_[idx-1];
}

};  // namespace parser
