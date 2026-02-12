// Copyright (c) 2026 Felix Kahle.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <gtest/gtest.h>
#include <vector>
#include "leviathan/bnb/search_trail.h"

struct ValueEntry
{
    size_t index;
    int old_value;
};

struct DirtyEntry
{
    size_t index;
};

struct BAPEntry
{
    int vessel_idx;
    int berth_idx;
    int64_t old_time;
    double old_cost;
};

class SearchTrailRestorationTest : public ::testing::Test
{
protected:
    std::vector<int> data_;

    void SetUp() override
    {
        data_ = {0, 10, 20, 30, 40};
    }
};

TEST_F(SearchTrailRestorationTest, PushAndBacktrack)
{
    leviathan::bnb::SearchTrail<ValueEntry> trail;

    // 1. Start Frame
    trail.push_frame();

    // 2. Record change: index 1 was 10, setting to 99
    trail.push({1, 10});
    data_[1] = 99;

    EXPECT_EQ(data_[1], 99);
    EXPECT_EQ(trail.depth(), 1);

    // 3. Backtrack
    trail.backtrack([&](const ValueEntry& e)
    {
        data_[e.index] = e.old_value;
    });

    // 4. Verify Restoration
    EXPECT_EQ(data_[1], 10);
    EXPECT_EQ(trail.depth(), 0);
    EXPECT_TRUE(trail.empty());
}

TEST_F(SearchTrailRestorationTest, NestedFrames)
{
    leviathan::bnb::SearchTrail<ValueEntry> trail;

    // Level 1: Change index 0 (0 -> 100)
    trail.push_frame();
    trail.push({0, 0});
    data_[0] = 100;

    // Level 2: Change index 1 (10 -> 200)
    trail.push_frame();
    trail.push({1, 10});
    data_[1] = 200;

    EXPECT_EQ(trail.depth(), 2);
    EXPECT_EQ(data_[0], 100);
    EXPECT_EQ(data_[1], 200);

    // Backtrack Level 2
    trail.backtrack([&](const ValueEntry& e)
    {
        data_[e.index] = e.old_value;
    });

    EXPECT_EQ(trail.depth(), 1);
    EXPECT_EQ(data_[1], 10); // Restored
    EXPECT_EQ(data_[0], 100); // Still modified

    // Backtrack Level 1
    trail.backtrack([&](const ValueEntry& e)
    {
        data_[e.index] = e.old_value;
    });

    EXPECT_EQ(trail.depth(), 0);
    EXPECT_EQ(data_[0], 0); // Restored
}

TEST(SearchTrailCoreTest, LIFOOrderCorrectness)
{
    leviathan::bnb::SearchTrail<std::string> trail;
    std::vector<std::string> ops;

    trail.push_frame();
    trail.push("First");
    trail.push("Second");
    trail.push("Third");

    // Backtrack should process: Third, Second, First
    trail.backtrack([&](const std::string& s)
    {
        ops.push_back(s);
    });

    ASSERT_EQ(ops.size(), 3);
    EXPECT_EQ(ops[0], "Third");
    EXPECT_EQ(ops[1], "Second");
    EXPECT_EQ(ops[2], "First");
}

TEST(SearchTrailCoreTest, EmplaceSemantics)
{
    leviathan::bnb::SearchTrail<BAPEntry> trail;
    trail.push_frame();

    // Verify emplace constructs in-place
    trail.emplace(1, 2, 100, 50.5);

    bool checked = false;
    trail.backtrack([&](const BAPEntry& e)
    {
        EXPECT_EQ(e.vessel_idx, 1);
        EXPECT_EQ(e.berth_idx, 2);
        EXPECT_EQ(e.old_time, 100);
        EXPECT_DOUBLE_EQ(e.old_cost, 50.5);
        checked = true;
    });
    EXPECT_TRUE(checked);
}

TEST(SearchTrailCoreTest, MemoryManagement)
{
    // Reserve space for 100 entries and 10 frames
    leviathan::bnb::SearchTrail<int> trail(100, 10);

    const size_t initial_cap = trail.allocated_memory_bytes();
    EXPECT_GT(initial_cap, 0);
    EXPECT_EQ(trail.used_memory_bytes(), 0);

    trail.push_frame();
    trail.push(42);

    // Usage should go up
    EXPECT_GT(trail.used_memory_bytes(), 0);

    // Capacity should ideally remain stable (since we reserved)
    EXPECT_EQ(trail.allocated_memory_bytes(), initial_cap);

    // After reset, usage is 0 but capacity is retained
    trail.clear();
    EXPECT_EQ(trail.used_memory_bytes(), 0);
    EXPECT_EQ(trail.allocated_memory_bytes(), initial_cap);
}

TEST(SearchTrailCoreTest, ReserveAndShrink)
{
    leviathan::bnb::SearchTrail<int> trail;

    trail.reserve(1000, 100);
    const size_t big_cap = trail.allocated_memory_bytes();

    trail.push_frame();
    trail.push(1);

    trail.shrink_to_fit();
    const size_t small_cap = trail.allocated_memory_bytes();

    EXPECT_LT(small_cap, big_cap);

    // Verify data integrity after shrink
    bool seen = false;
    trail.backtrack([&](int i)
    {
        EXPECT_EQ(i, 1);
        seen = true;
    });
    EXPECT_TRUE(seen);
}

TEST(SearchTrailCoreTest, DirtyIndexPattern)
{
    leviathan::bnb::SearchTrail<DirtyEntry> flag_trail;
    std::vector<bool> flags(5, false);

    flag_trail.push_frame();

    // Touch index 2
    flag_trail.push({2});
    flags[2] = true;

    // Touch index 4
    flag_trail.push({4});
    flags[4] = true;

    // Undo: Reset all recorded indices to false
    flag_trail.backtrack([&](const DirtyEntry& e)
    {
        flags[e.index] = false;
    });

    EXPECT_FALSE(flags[2]);
    EXPECT_FALSE(flags[4]);
}

#ifndef NDEBUG
TEST(SearchTrailDeathTest, BacktrackEmpty)
{
    leviathan::bnb::SearchTrail<int> trail;
    // Backtracking without a frame should trigger DCHECK
    EXPECT_DEATH(trail.backtrack([](int){}), "");
}
#endif
