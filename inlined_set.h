
#ifndef INLINED_SET_H
#define INLINED_SET_H

#include "absl/container/flat_hash_set.h"

#include <algorithm>
#include <iterator>

// When >NInline elements, the set allocates
template<typename T, int NInline>
struct InlinedSet {

	struct iterator 
	{
		friend struct InlinedSet;

	    using iterator_category = std::forward_iterator_tag;
	    using difference_type   = std::ptrdiff_t;
	    using value_type        = T;
	    using pointer           = T const*;  // or also value_type*
	    using reference         = T const&;  // or also value_type&

	    iterator(InlinedSet const *it_set) : it_set_(it_set), local_idx_(0) {
	    	if(it_set->n_in_local_ == 0) {
	    		if(it_set_->more_storage_) {
		    		more_it_ = it_set_->more_storage_->begin();
		    	}
	    	}
	    }

	    reference operator*() const {
	    	if(local_idx_ < it_set_->n_in_local_) {
	    		return it_set_->local_storage_[local_idx_];
	    	}
	    	return *more_it_;
	    }
	    pointer operator->() {
	    	return &operator*();
	 	}

	    // Prefix increment
	    iterator& operator++() {
    		++local_idx_;
	    	if(local_idx_ == it_set_->n_in_local_) {
	    		if(it_set_->more_storage_) {
		    		more_it_ = it_set_->more_storage_->begin();
		    	}
	    	} else if (local_idx_ > it_set_->n_in_local_) {
	    		assert(it_set_->more_storage_);
	    		++more_it_;
	    	}
	    	return *this;
	    }

	    // Postfix increment
	    iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }

	    friend bool operator== (const iterator& a, const iterator& b) {
	    	return (a.it_set_ == b.it_set_) && (a.local_idx_ == b.local_idx_) && (a.more_it_ == b.more_it_);
	    };
	    friend bool operator!= (const iterator& a, const iterator& b) { return !(a == b); };

private:

	    InlinedSet const *it_set_;
	    unsigned int local_idx_;
	    typename absl::flat_hash_set<T>::const_iterator more_it_;
	};

	InlinedSet() : n_in_local_(0) {}

	bool contains(T const&val)const {
		const auto p = std::equal_range(local_storage_, local_storage_ + n_in_local_, val);
		if(p.first == p.second) {
			return more_storage_ ? more_storage_->contains(val) : false;
		}

		auto pit = p.first;
		++pit;
		assert(pit == p.second);

		return true;
	}

	void insert(T const&val) {
		if(contains(val)) {
			return;
		}
		if(n_in_local_ < NInline) {
			local_storage_[n_in_local_] = val;
			n_in_local_++;
			std::sort(local_storage_, local_storage_ + n_in_local_);
		} else {
			if(!more_storage_) {
				more_storage_.reset(new absl::flat_hash_set<T>);
			}
			more_storage_->insert(val);
		}
	}

	void erase(T const&val) {
		if(!contains(val)) {
			return;
		}

		const auto p = std::equal_range(local_storage_, local_storage_ + n_in_local_, val);

		if(p.first == p.second) {
			assert(more_storage_);
			more_storage_->erase(val);
			return;
		}

		const unsigned index = p.first - local_storage_;

		const unsigned n_after = NInline - index - 1;

		if(n_after > 0) {
			memmove(&local_storage_[index], &local_storage_[index+1], n_after * sizeof(T));
		}

		--n_in_local_;
	}

	size_t size()const {
		return n_in_local_ + (more_storage_ ? more_storage_->size() : 0);
	}

	iterator begin()const {
		return iterator(this);
	}

	iterator end()const {
		iterator ret(this);
		ret.local_idx_ = n_in_local_;
		if(more_storage_) {
			ret.more_it_ = more_storage_->end();
			ret.local_idx_ += more_storage_->size();
		}
		return ret;
	}


private:

	std::unique_ptr<absl::flat_hash_set<T> > more_storage_;
	T local_storage_[NInline];
	unsigned int n_in_local_;
};


#endif//INLINED_SET_H