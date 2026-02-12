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
#include <ranges>
#include <algorithm>
#include "leviathan/bnb/berth_timeline.h"

using Time = int64_t;
using Window = leviathan::bnb::AvailableWindow<Time>;
using Timeline = leviathan::bnb::BerthTimeline<Time>;

TEST(BerthTimelineTest, AssignRange)
{
    Timeline timeline;

    // Test basic assignment
    timeline.assign(10, 100);
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_EQ(timeline.begin()->start_inclusive, 10);
    EXPECT_EQ(timeline.begin()->end_exclusive, 100);

    // Test invalid range (open >= close) results in empty
    timeline.assign(100, 50);
    EXPECT_TRUE(timeline.empty());

    timeline.assign(100, 100);
    EXPECT_TRUE(timeline.empty());
}

TEST(BerthTimelineTest, DirectWindowAssignment)
{
    Timeline timeline;
    std::vector<Window> windows = {{0, 50}, {100, 150}, {200, 250}};

    timeline.assign(windows);
    ASSERT_EQ(timeline.size(), 3);
    EXPECT_EQ(timeline.begin()->end_exclusive, 50);
    EXPECT_EQ(timeline.rbegin()->start_inclusive, 200);
}

TEST(BerthTimelineTest, CarveFixedAssignments)
{
    Timeline timeline;

    // Availability: [0, 500), [600, 1000)
    // Fixed: [100, 200), [400, 700), [900, 1100)
    std::vector<Window> avail = {{0, 500}, {600, 1000}};
    std::vector<Window> fixed = {{100, 200}, {400, 700}, {900, 1100}};

    timeline.assign(avail, fixed);

    // Expected Windows:
    // 1. [0, 100)   (From first avail, before first fixed)
    // 2. [200, 400) (From first avail, between first and second fixed)
    // 3. [700, 800) (From second avail - second fixed ends at 700, third starts at 900)
    // 4. [800, 900) -> Actually [700, 900) (From second avail, before third fixed)

    ASSERT_EQ(timeline.size(), 3);

    // Window 1
    EXPECT_EQ(timeline.begin()->start_inclusive, 0);
    EXPECT_EQ(timeline.begin()->end_exclusive, 100);

    // Window 2
    EXPECT_EQ((timeline.begin() + 1)->start_inclusive, 200);
    EXPECT_EQ((timeline.begin() + 1)->end_exclusive, 400);

    // Window 3
    EXPECT_EQ((timeline.begin() + 2)->start_inclusive, 700);
    EXPECT_EQ((timeline.begin() + 2)->end_exclusive, 900);
}

TEST(BerthTimelineTest, CarveFixedEdgeCases)
{
    Timeline timeline;

    // Fixed assignment exactly matches availability
    timeline.assign(std::vector<Window>{{100, 200}}, std::vector<Window>{{100, 200}});
    EXPECT_TRUE(timeline.empty());

    // Fixed assignment completely covers availability
    timeline.assign(std::vector<Window>{{100, 200}}, std::vector<Window>{{50, 250}});
    EXPECT_TRUE(timeline.empty());

    // Fixed assignment starts before and ends inside
    timeline.assign(std::vector<Window>{{100, 200}}, std::vector<Window>{{50, 150}});
    ASSERT_EQ(timeline.size(), 1);
    EXPECT_EQ(timeline.begin()->start_inclusive, 150);
    EXPECT_EQ(timeline.begin()->end_exclusive, 200);
}

TEST(BerthTimelineTest, MemoryReuse)
{
    Timeline timeline;
    timeline.assign(0, 1000); // Allocates

    timeline.clear();
    EXPECT_TRUE(timeline.empty());

    // Re-assigning something smaller shouldn't trigger new allocations if capacity allows
    timeline.assign(0, 50);
    EXPECT_EQ(timeline.size(), 1);
}

TEST(BerthTimelineTest, FindStartAfterReassign)
{
    Timeline timeline(0, 100);
    EXPECT_EQ(timeline.find_earliest_start(10, 20), 10);

    // Completely change timeline
    timeline.assign(std::vector<Window>{{200, 300}});

    // Old search should now fail or find new window
    EXPECT_EQ(timeline.find_earliest_start(10, 20), 200);
}

TEST(BerthTimelineTest, ConstAccessors)
{
    const Timeline timeline(0, 100);
    EXPECT_FALSE(timeline.cbegin() == timeline.cend());
    EXPECT_EQ(timeline.size(), 1);
}
