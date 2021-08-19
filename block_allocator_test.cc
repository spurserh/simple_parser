

#include "gtest/gtest.h"
#include "block_allocator.h"

namespace {

TEST(BlockAllocatorTest, Simple) {
	const size_t alignment = 32;
	BlockAllocator<float> test(128, alignment);
	std::vector<float> test_ref;
	std::vector<float*> test_ptrs;

	for(int i=0;i<10000;++i) {
		const float value = float(rand()) / RAND_MAX;
		float* vptr = new (test.allocate()) float(value);
		if(i == 0) {
			ASSERT_EQ(0, ((unsigned long long)vptr) % alignment);
		}
		test_ref.push_back(value);
		test_ptrs.push_back(vptr);

		ASSERT_EQ(test_ref.size(), test_ptrs.size());
		for(size_t vi=0;vi<test_ref.size();++vi) {
			EXPECT_EQ(test_ref[i], *test_ptrs[i]);
		}

		for(size_t vi=0;vi<test_ref.size();++vi) {
			if(0 == (rand()%10)) {
				test_ref[i] = *test_ptrs[i] = value;
			}
		}
	}
}

TEST(BlockAllocatorTest, Ctor) {
	static int refs = 0;
	static int tests = 0;

	struct CountedRef {
		CountedRef() {
			++refs;
		}
		CountedRef(CountedRef&& o) {
			++refs;
		}
		CountedRef(CountedRef const&o) = delete;
		~CountedRef() {
			--refs;
		}
	};
	struct CountedTest {
		CountedTest() {
			++tests;
		}
		~CountedTest() {
			--tests;
		}
		CountedTest(CountedTest const&o) = delete;
		CountedTest(CountedTest &&o) = delete;
	};

	{
		std::vector<CountedRef> test_ref;
		BlockAllocator<CountedTest> test(16, 8);

		test_ref.emplace_back(std::move(CountedRef()));
		new (test.allocate()) CountedTest;

		EXPECT_EQ(refs, 1);
		EXPECT_EQ(tests, 1);

		test_ref.emplace_back(std::move(CountedRef()));
		new (test.allocate()) CountedTest;

		EXPECT_EQ(refs, 2);
		EXPECT_EQ(tests, 2);
	}

	EXPECT_EQ(refs, 0);
	EXPECT_EQ(tests, 0);
}

}  // namespace

