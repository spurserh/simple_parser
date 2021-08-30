
#ifndef SYNTAX_TREE_H
#define SYNTAX_TREE_H

#include "absl/container/inlined_vector.h"
#include "inlined_set.h"
#include "block_allocator.h"
#include "rules.h"

#include <sstream>
#include <vector>

namespace parser {


// 0 = NULL, indexes from 1
typedef unsigned LexedTokenIdx;

struct Node;

struct ParsedSlot {
	ParsedSlot(LexedTokenIdx lexed) : lexed_idx(lexed) { }
	ParsedSlot(Node* sub) : lexed_idx(0) {subs.insert(sub);}
	ParsedSlot() : lexed_idx(0) { }

	LexedTokenIdx 				lexed_idx;
	InlinedSet<Node*, 4> 	    subs;
};

struct Node {
	Node(Rule const&rule) 
	  : rule(rule), parent(0)//, subs_complete(0) 
	{
	}

	// Pointer instead of reference for move semantics
	Rule const&rule;

	Node* parent;

	// These correspond to the tokens in the rule pattern
	absl::InlinedVector<ParsedSlot, 8> parsed;

	// The number of sub-nodes complete in the last slot
	// - All but the last slot must be all complete
	// - Complete means all slots filled and all subs complete
//	unsigned subs_complete;

	inline void AddParsed(LexedTokenIdx lexed) {
		parsed.emplace_back(std::move(ParsedSlot(lexed)));
//		subs_complete = 0;
	}

	inline void AddParsed(Node* sub) {
		parsed.emplace_back(std::move(ParsedSlot(sub)));
//		subs_complete = 0;
	}

	inline void AddParsed() {
		parsed.emplace_back(std::move(ParsedSlot()));
//		subs_complete = 0;
	}
};

typedef Node* NodePtr;

struct LexedToken {
	LexedToken() : tok(0), lineno(0), colno(0) {
	}

	Token tok;
	int lineno;
	int colno;
};

template<typename T>
using OneOrMore = absl::InlinedVector<T, 1>;

struct SyntaxTree {
	SyntaxTree();

	bool Init(char const* top_rule_name);

	// Iter is an iterator<LexedToken>	
	// Allow random access iterator to have other behaviors
	template<typename Iter>
	bool Parse(Iter first, Iter last) {
		for(Iter it = first;it != last;++it) {
			LexedToken const&lexed = *it;
			if(!ConsumeToken(lexed)) {
				return false;
			}
		}
		return true;
	}

	bool CanComplete()const;

	// multiline=0 to enable
	std::string ToString(int multiline=-1, Node const*n=0)const;

	// TODO: Generate graph

  private:

  	bool NodeContainsWorkPtr_slow(Node *n)const;
  	bool NodeIsSane(Node *n)const;

  	// Recursively check that all sub-nodes are complete, without using subs_complete
  	bool CheckComplete_slow(Node *n)const;

  	unsigned CountSubsComplete_slow(Node *n)const;

  	void UpdateWorkPtr(Node* incomplete);

  	bool ConsumeToken(LexedToken const&next);

  	bool ConsumeInNode(Node* incomplete,
  					   TokenType next_tok_type,
  					   LexedTokenIdx lexed_idx);

  	Node* BuildStepDownStack(StepDownStack const&stack,
  							 NodePtr& last_descendant);

  	void MarkCompleteAndMoveUp(Node* incomplete);
  	bool IsComplete(Node* n)const;
  	bool CanComplete(Node* n)const;

  	LexedToken const&GetLexedTokenByIdx(LexedTokenIdx idx)const;

  	Node* AddNode(Rule const&rule);
  	Node* ShallowCopyNode(Node* n);
  	void DeleteNode(Node* n);

  	bool NodeComplete(Node const* node)const;
  	Node* GetTop()const;

  	Token NextTokenInPattern(Node const*node)const;

  	static void MakeIndent(std::ostringstream &ostr, int n);

  	// Incomplete nodes cannot be children of complete ones
  	// The next token is always consumed by the bottom-most incomplete nodes
  	// The plural "nodes" is used because a grammar may be ambiguous. Even an unambiguous
  	//  grammar will often produce ambiguous states which are resolved with later symbols
  	//  (ie requiring look-ahead).

  	// After a token is to be consumed by the AST, it must have been consumed by all incomplete nodes. 
  	// This can be either directly, fitting into the rule pattern of that node, or by an incomplete descendant
  	//  on at least one branch of ambiguity. 
  	// New incomplete nodes may be inserted to satisfy this requirement. 
  	// Incomplete nodes may also be removed to satisfy it. 

  	// Next incomplete nodes up the tree. Any descendents of these are complete. 
  	InlinedSet<Node*, 4>       		 work_ptrs_;

  	// A list of completed nodes at or above the work_ptrs
  	//  which could consume a given token by inserting a node with the same
  	//  rule in their place. These are things like binary expressions and lists. 
  	std::multimap<TokenType, Node*>  step_up_ptrs_;

  	// std::vector uses copy constructor..
//	std::vector<Node>     	   node_array_;
	BlockAllocator<Node> 	   node_array_;

	// TODO: Use BlockAllocator? Need to index though?
	std::vector<LexedToken>    token_array_;
};

};  // namespace parser

#endif//SYNTAX_TREE_H

