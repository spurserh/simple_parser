
#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <list>
#include <memory>

template<typename T>
struct BlockAllocator {
	BlockAllocator(size_t initial_size, size_t alignment)
	  : alignment_(alignment),
	  	last_block_size_(initial_size),
	  	last_block_next_index_(0),
	  	total_allocated_(0) {
		assert(initial_size > 0);
		size_t size_rounded = sizeof(T)*initial_size;
		size_rounded = (size_rounded+(alignment-1))/alignment;
		size_rounded *= alignment;
		blocks_.push_back((T*)::aligned_alloc(alignment, size_rounded));
		last_block_size_bytes_ = size_rounded;
	}

	~BlockAllocator() {
		assert(blocks_.size() > 0);
		// Last block may be partial
		auto rit = blocks_.rbegin();
		for(size_t i=0;i<last_block_next_index_;++i) {
			(*rit)[i].~T();
		}
		// Earlier blocks are fully occupied
		for(++rit;rit != blocks_.rend();++rit) {
			delete [] *rit;
		}
	}

	// Returns unconstructed pointer
	// Use new on this address. Don't use delete. 
	T* allocate() {
		if(last_block_next_index_ == last_block_size_) {
			last_block_size_bytes_ *= 2;
			last_block_size_ *= 2;
			last_block_next_index_ = 0;
			blocks_.push_back((T*)::aligned_alloc(alignment_, last_block_size_bytes_));
		}
		++total_allocated_;
		return &blocks_.back()[last_block_next_index_++];
	}

	T* get_first()const {
		assert(size() > 0);
		assert(blocks_.size() > 0);
		return blocks_.front();
	}

	size_t size()const {
		return total_allocated_;
	}

private:

	size_t alignment_;
	size_t last_block_size_;
	size_t last_block_size_bytes_;
	size_t last_block_next_index_;
	size_t total_allocated_;
	std::list<T*> blocks_;
};

#endif//BLOCK_ALLOCATOR_H
