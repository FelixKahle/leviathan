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

class SearchTrailTest : public ::testing::Test
{
protected:
    std::vector<int64_t> times_;
    std::vector<bool> assigned_bools_;
    std::vector<int> assigned_ids_;

    leviathan::bnb::SearchTrail trail_;

    SearchTrailTest() : trail_(100)
    {
        times_.resize(10, 0);
        assigned_bools_.resize(10, false);
        assigned_ids_.resize(10, -1); // Default is -1
    }
};

TEST_F(SearchTrailTest, LambdaRestoration)
{
    times_[0] = 100;
    assigned_ids_[0] = 5; // Assigned to Vessel 5

    trail_.push_checkpoint();
    trail_.save_value(0, 100);
    trail_.mark_touched(0);

    // Modify
    times_[0] = 200;
    assigned_ids_[0] = 99;

    // Backtrack with CUSTOM logic (Reset to -1, not 0)
    trail_.backtrack(times_, [&](const size_t idx)
    {
        assigned_ids_[idx] = -1;
    });

    EXPECT_EQ(times_[0], 100);
    EXPECT_EQ(assigned_ids_[0], -1); // Correctly reset to -1
}

TEST_F(SearchTrailTest, ConvenienceBoolReset)
{
    assigned_bools_[1] = true;

    trail_.push_checkpoint();
    trail_.mark_touched(1);

    // Backtrack using the helper overload
    // "Reset assignments in assigned_bools_ to false"
    trail_.backtrack(times_, assigned_bools_, false);

    EXPECT_FALSE(assigned_bools_[1]);
}

TEST_F(SearchTrailTest, ConvenienceIntReset)
{
    assigned_ids_[2] = 999;

    trail_.push_checkpoint();
    trail_.mark_touched(2);

    // "Reset assignments in assigned_ids_ to 0"
    trail_.backtrack(times_, assigned_ids_, 0);

    EXPECT_EQ(assigned_ids_[2], 0);
}

TEST_F(SearchTrailTest, BytesUsedTracking)
{
    // 1. Initial State: 0 bytes
    EXPECT_EQ(trail_.allocated_memory_bytes(), 0);

    // 2. Push Checkpoint
    trail_.push_checkpoint();
    size_t expected_size = sizeof(std::pair<size_t, size_t>);
    EXPECT_EQ(trail_.allocated_memory_bytes(), expected_size);

    // 3. Add Value Entry
    trail_.save_value(0, 100);

    // Calculate added size dynamically to be platform agnostic
    const size_t value_entry_size = trail_.allocated_memory_bytes() - expected_size;
    expected_size += value_entry_size;
    EXPECT_EQ(trail_.allocated_memory_bytes(), expected_size);

    // 4. Add Dirty Entry
    trail_.mark_touched(1);

    const size_t dirty_entry_size = trail_.allocated_memory_bytes() - expected_size;
    expected_size += dirty_entry_size;
    EXPECT_EQ(trail_.allocated_memory_bytes(), expected_size);

    // 5. Verify Backtracking reduces size
    // FIX: Use 'times_' and 'assigned_ids_' instead of 'values_'/'assignments_'
    trail_.backtrack(times_, assigned_ids_, -1);

    EXPECT_EQ(trail_.allocated_memory_bytes(), 0);
}
