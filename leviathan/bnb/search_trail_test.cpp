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

// Fixture for standard Integer testing
class SearchTrailTest : public ::testing::Test
{
protected:
    // Simulated global state
    std::vector<int64_t> values_;
    std::vector<int> flags_;

    // The trail under test (default int64_t)
    leviathan::bnb::SearchTrail<int64_t> trail_;

    SearchTrailTest() : trail_(100)
    {
        values_.assign(10, 0);
        flags_.assign(10, -1);
    }
};

TEST_F(SearchTrailTest, BasicValueRestoration)
{
    // Initial State: values_[0] = 0
    trail_.push_checkpoint();

    // 1. Save old value (0)
    trail_.save_value(0, values_[0]);
    // 2. Modify state
    values_[0] = 42;

    EXPECT_EQ(values_[0], 42);

    // 3. Backtrack
    trail_.backtrack(values_, [](size_t){});

    EXPECT_EQ(values_[0], 0);
    EXPECT_TRUE(trail_.empty());
}

TEST_F(SearchTrailTest, DirtyIndexCleanup)
{
    // Initial State: flags_ all -1
    trail_.push_checkpoint();

    // 1. Mark index 5 as dirty
    trail_.mark_touched(5);
    flags_[5] = 1; // "True"

    // 2. Mark index 2 as dirty
    trail_.mark_touched(2);
    flags_[2] = 1;

    // 3. Backtrack with lambda to reset to -1
    trail_.backtrack(values_, [&](const size_t index)
    {
        flags_[index] = -1;
    });

    EXPECT_EQ(flags_[5], -1);
    EXPECT_EQ(flags_[2], -1);
    EXPECT_EQ(flags_[0], -1); // Untouched should remain -1
}

TEST_F(SearchTrailTest, NestedCheckpoints)
{
    // Level 0: values_[0] = 0
    trail_.push_checkpoint(); // CP 1
    trail_.save_value(0, 0);
    values_[0] = 10;

    // Level 1: values_[0] = 10
    trail_.push_checkpoint(); // CP 2
    trail_.save_value(0, 10);
    values_[0] = 20;

    EXPECT_EQ(values_[0], 20);
    EXPECT_EQ(trail_.depth(), 2);

    // Backtrack to Level 1
    trail_.backtrack(values_, flags_, -1);
    EXPECT_EQ(values_[0], 10);
    EXPECT_EQ(trail_.depth(), 1);

    // Backtrack to Level 0
    trail_.backtrack(values_, flags_, -1);
    EXPECT_EQ(values_[0], 0);
    EXPECT_EQ(trail_.depth(), 0);
}

TEST_F(SearchTrailTest, CommitCheckpointMerge)
{
    // Scenario: We go deep, then "commit" the changes, effectively
    // merging the deeper node's changes into the parent node.

    // Level 0
    trail_.push_checkpoint(); // CP 1
    trail_.save_value(0, 0);
    values_[0] = 10;

    // Level 1
    trail_.push_checkpoint(); // CP 2
    trail_.save_value(0, 10);
    values_[0] = 20;

    EXPECT_EQ(trail_.depth(), 2);

    // Commit CP 2.
    // This removes the "undo boundary" between L1 and L0.
    // The save_value(0, 10) and save_value(0, 0) are now both part of CP 1.
    trail_.commit_checkpoint();

    EXPECT_EQ(trail_.depth(), 1);
    EXPECT_EQ(values_[0], 20); // State hasn't changed

    // Now if we backtrack, we should go all the way back to 0
    // because both history entries are under the remaining checkpoint.
    trail_.backtrack(values_, flags_, -1);

    EXPECT_EQ(values_[0], 0);
    EXPECT_EQ(trail_.depth(), 0);
}

TEST_F(SearchTrailTest, MemoryTracking)
{
    // 1. Initial State
    EXPECT_EQ(trail_.used_memory_bytes(), 0);
    // Capacity should be > 0 because we reserved 100 in constructor
    EXPECT_GT(trail_.reserved_memory_bytes(), 0);

    const size_t initial_reserved = trail_.reserved_memory_bytes();

    // 2. Add data
    trail_.push_checkpoint();
    trail_.save_value(0, 50);
    trail_.mark_touched(1);

    EXPECT_GT(trail_.used_memory_bytes(), 0);

    // 3. Ensure no reallocation occurred (critical for perf)
    EXPECT_EQ(trail_.reserved_memory_bytes(), initial_reserved);

    // 4. Clear
    trail_.backtrack(values_, flags_, -1);
    EXPECT_EQ(trail_.used_memory_bytes(), 0);
}

// --- Template Tests ---

TEST(SearchTrailTemplateTest, HandlesDoubles)
{
    std::vector<double> d_values(5, 0.0);

    // Instantiate with double
    leviathan::bnb::SearchTrail<double> d_trail(10);

    d_trail.push_checkpoint();
    d_trail.save_value(0, d_values[0]);
    d_values[0] = 3.14159;

    EXPECT_DOUBLE_EQ(d_values[0], 3.14159);

    d_trail.backtrack(d_values, [](size_t){});

    EXPECT_DOUBLE_EQ(d_values[0], 0.0);
}

TEST(SearchTrailTemplateTest, HandlesCustomStructs)
{
    struct Domain { int min; int max; };
    std::vector<Domain> domains(1, {0, 10});

    // Instantiate with custom struct
    leviathan::bnb::SearchTrail<Domain> domain_trail(10);

    domain_trail.push_checkpoint();
    domain_trail.save_value(0, domains[0]);

    // Constrain domain
    domains[0] = {5, 10};

    EXPECT_EQ(domains[0].min, 5);

    domain_trail.backtrack(domains, [](size_t){});

    EXPECT_EQ(domains[0].min, 0);
    EXPECT_EQ(domains[0].max, 10);
}
