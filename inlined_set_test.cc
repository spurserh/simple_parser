

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

TEST(InlinedSetTest, Simple) {
	InlinedSet<float, 4> test;

	ASSERT_FALSE(test.contains(3.1f));

	test.insert(3.1f);

	ASSERT_TRUE(test.contains(3.1f));

	test.erase(3.1f);

	ASSERT_FALSE(test.contains(3.1f));
}

TEST(InlinedSetTest, OverflowAndAlloc) {
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

TEST(InlinedSetTest, IterateTest) {
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

TEST(InlinedSetTest, IterateTestShort) {
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

enum Command {
	Command_Insert=0,
	Command_Remove,
	Command_CheckRand,
	Command_Count,
};

TEST(InlinedSetTest, RandomTest) {
	static const int nInline = 4;
	InlinedSet<int, nInline> test;
	absl::flat_hash_set<int> ref;
	const int kMaxVal = 1000;
	srand(5555);	
	for(int i=0;i<20000;++i) {
		Command cmd = Command(rand()%int(Command_Count));
		switch(cmd) {
			case Command_Insert: {
				const int val = rand()%kMaxVal;
				test.insert(val);
				ref.insert(val);
				break;
			}
			case Command_Remove: {
				const int val = rand()%kMaxVal;
				test.erase(val);
				ref.erase(val);
				break;
			}
			case Command_CheckRand: {
				const int val = rand()%kMaxVal;
				ASSERT_EQ(ref.contains(val), test.contains(val));
				break;
			}
			default: assert(!"Internal failure");
		}
		const bool sets_equal = CheckSetsEqual<int, nInline>(test, ref);
		ASSERT_TRUE(sets_equal);
	}
}

}  // namespace



