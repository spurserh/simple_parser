

#include "gtest/gtest.h"
#include "inlined_set.h"
#include "absl/container/flat_hash_set.h"

#include <algorithm>

namespace {

template<typename T, int NInline>
bool CheckSetsEqual(InlinedSet<T, NInline> &test,
					absl::flat_hash_set<T> &ref) {
	std::vector<T> values_from_test;
	for(T v : test) {
		values_from_test.push_back(v);
	}

	std::vector<T> values_from_ref;
	for(T v : ref) {
		values_from_ref.push_back(v);
	}

	std::sort(values_from_test.begin(), values_from_test.end());
	std::sort(values_from_ref.begin(), values_from_ref.end());

	return values_from_test == values_from_ref;
}

struct InlinedSetTest : public ::testing::Test {

	template<int nInline>
	void TestRandom() {
		const int kMaxVal = 100;
		srand(5555);	
		for(int ti=0;ti<200;++ti) {
			InlinedSet<int, nInline> test;
			absl::flat_hash_set<int> ref;
			for(int ci=0;ci<200;++ci) {

				// Stay relatively close to 0, 
				//  since that's where the custom function is
				if(0==(rand()%3)) {
					const int val = rand()%kMaxVal;
					test.insert(val);
					ref.insert(val);
				}
				if(0==(rand()%2)) {
					const int val = rand()%kMaxVal;
					test.erase(val);
					ref.erase(val);
				}
				{
					// Check random
					const int val = rand()%kMaxVal;
					ASSERT_EQ(ref.contains(val), test.contains(val));
				}

				const bool sets_equal = CheckSetsEqual<int, nInline>(test, ref);
				ASSERT_TRUE(sets_equal);
			}
		}
	}
};

TEST_F(InlinedSetTest, Simple) {
	InlinedSet<float, 4> test;

	ASSERT_FALSE(test.contains(3.1f));

	test.insert(3.1f);

	ASSERT_TRUE(test.contains(3.1f));

	test.erase(3.1f);

	ASSERT_FALSE(test.contains(3.1f));
}

TEST_F(InlinedSetTest, OverflowAndAlloc) {
	InlinedSet<float, 4> test;

	ASSERT_FALSE(test.contains(3.1f));

	test.insert(3.1f);
	test.insert(3.2f);
	test.insert(3.3f);
	test.insert(3.4f);
	test.insert(3.5f);
	test.insert(3.6f);

	ASSERT_TRUE(test.contains(3.2f));
	ASSERT_TRUE(test.contains(3.5f));

	test.erase(3.2f);

	ASSERT_FALSE(test.contains(3.2f));
	ASSERT_TRUE(test.contains(3.5f));

	test.erase(3.5f);

	ASSERT_FALSE(test.contains(3.2f));
	ASSERT_FALSE(test.contains(3.5f));
}

TEST_F(InlinedSetTest, IterateTest) {
	std::vector<float> values;
	values.push_back(1.1f);
	values.push_back(2.1f);
	values.push_back(4.1f);
	values.push_back(10.11f);
	values.push_back(22.0f);
	values.push_back(12.0f);

	InlinedSet<float, 4> test;

	for(float v : values) {
		test.insert(v);
	}

	std::vector<float> values_from_test;

	for(float v : test) {
		values_from_test.push_back(v);		
	}

	std::sort(values.begin(), values.end());
	std::sort(values_from_test.begin(), values_from_test.end());

	ASSERT_EQ(values, values_from_test);
}

TEST_F(InlinedSetTest, IterateTestShort) {
	std::vector<float> values;
	values.push_back(1.1f);
	values.push_back(2.1f);
	values.push_back(4.1f);

	InlinedSet<float, 4> test;

	for(float v : values) {
		test.insert(v);
	}

	std::vector<float> values_from_test;

	for(float v : test) {
		values_from_test.push_back(v);		
	}

	std::sort(values.begin(), values.end());
	std::sort(values_from_test.begin(), values_from_test.end());

	ASSERT_EQ(values, values_from_test);
}

TEST_F(InlinedSetTest, RandomTest) {
	TestRandom<1>();
	TestRandom<2>();
	TestRandom<3>();
	TestRandom<4>();
}

}  // namespace



