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
#include "leviathan/bnb/search_state.h"

// Standard types for BAP
using Time = int64_t;
using Index = int32_t;
using Cost = double;
using State = leviathan::bnb::SearchState<Time, Index, Cost>;

TEST(SearchStateTest, InitialState)
{
    constexpr size_t num_berths = 2;
    constexpr size_t num_vessels = 3;
    const State state(num_berths, num_vessels);

    EXPECT_EQ(state.berth_free_times.size(), num_berths);
    EXPECT_EQ(state.vessel_assignments.size(), num_vessels);
    EXPECT_EQ(state.current_objective, 0.0);
    EXPECT_EQ(state.last_assigned_vessel, State::kUnassignedVessel);

    for (const auto& t : state.berth_free_times)
    {
        EXPECT_EQ(t, 0);
    }
    for (Index v = 0; v < static_cast<Index>(num_vessels); ++v)
    {
        EXPECT_FALSE(state.is_assigned(v));
    }
}

TEST(SearchStateTest, ApplyMoveUpdatesState)
{
    State state(2, 2);

    // Apply move: Vessel 0 -> Berth 1
    // Start: 10, Finish: 25, Cost Delta: 15.5
    state.apply_move(0, 1, 10, 25, 15.5);

    EXPECT_TRUE(state.is_assigned(0));
    EXPECT_EQ(state.get_assigned_berth(0), 1);
    EXPECT_EQ(state.get_start_time(0), 10);
    EXPECT_EQ(state.berth_free_times[1], 25);
    EXPECT_EQ(state.current_objective, 15.5);
    EXPECT_EQ(state.last_assigned_vessel, 0);
}


TEST(SearchStateTest, BacktrackRestoresPreviousValues)
{
    State state(2, 2);

    // Initial Snapshot
    const Time old_berth_time = state.berth_free_times[0];
    const Cost old_objective = state.current_objective;
    const Index old_last_vessel = state.last_assigned_vessel;

    // Apply then Backtrack
    state.apply_move(1, 0, 100, 150, 50.0);
    state.backtrack_move(1, 0, old_berth_time, old_objective, old_last_vessel);

    EXPECT_FALSE(state.is_assigned(1));
    EXPECT_EQ(state.berth_free_times[0], old_berth_time);
    EXPECT_EQ(state.current_objective, old_objective);
    EXPECT_EQ(state.last_assigned_vessel, old_last_vessel);
}

TEST(SearchStateTest, SequentialMovesAndPartialBacktrack)
{
    State state(5, 5);

    // Move 1: Vessel 2 on Berth 0
    state.apply_move(2, 0, 10, 20, 10.0);

    // Snapshot state after Move 1
    const Time berth_0_time_after_m1 = state.berth_free_times[0];
    const Cost obj_after_m1 = state.current_objective;
    const Index last_v_after_m1 = state.last_assigned_vessel;

    // Move 2: Vessel 4 on Berth 0 (stacked)
    state.apply_move(4, 0, 20, 35, 15.0);
    EXPECT_EQ(state.berth_free_times[0], 35);
    EXPECT_EQ(state.current_objective, 25.0);

    // Backtrack Move 2 only
    state.backtrack_move(4, 0, berth_0_time_after_m1, obj_after_m1, last_v_after_m1);

    EXPECT_TRUE(state.is_assigned(2));
    EXPECT_FALSE(state.is_assigned(4));
    EXPECT_EQ(state.berth_free_times[0], 20);
    EXPECT_EQ(state.current_objective, 10.0);
    EXPECT_EQ(state.last_assigned_vessel, 2);
}

#ifndef NDEBUG
TEST(SearchStateDeathTest, AccessUnassignedVessel)
{
    const State state(1, 1);
    // Should trigger DCHECK if vessel is not assigned
    EXPECT_DEATH({
                 [[maybe_unused]] auto r = state.get_start_time(0);
                 }, "");
    EXPECT_DEATH({
                 [[maybe_unused]] auto r = state.get_assigned_berth(0);
                 }, "");
}

TEST(SearchStateDeathTest, DoubleAssignment)
{
    State state(2, 1);
    state.apply_move(0, 0, 0, 10, 5.0);
    // Should trigger DCHECK as vessel 0 is already assigned
    EXPECT_DEATH(state.apply_move(0, 1, 10, 20, 5.0), "");
}
#endif
