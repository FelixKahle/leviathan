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

#include "leviathan/bnb/search_stack.h"
#include <gtest/gtest.h>
#include <ranges>
#include <vector>
#include <array>
#include <string>

struct Decision
{
    int vessel_id;
    int berth_id;

    bool operator==(const Decision& other) const = default;
};


TEST(SearchStackTest, InitialState)
{
    const leviathan::bnb::SearchStack<int> stack;
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.depth(), 0);
    EXPECT_EQ(stack.allocated_memory_bytes(), 0);
}

TEST(SearchStackTest, Preallocation)
{
    constexpr size_t entry_cap = 1024;
    constexpr size_t frame_cap = 64;
    const leviathan::bnb::SearchStack<int> stack(entry_cap, frame_cap);

    EXPECT_GE(stack.allocated_memory_bytes(), entry_cap * sizeof(int));
    EXPECT_TRUE(stack.empty());
}

TEST(SearchStackTest, FrameBasics)
{
    leviathan::bnb::SearchStack<Decision> stack;

    // Depth 1: Vessel 1 on Berths 101, 102
    stack.push_frame();
    stack.push({1, 101});
    stack.emplace(1, 102);

    EXPECT_EQ(stack.depth(), 1);
    const auto frame1 = stack.current_frame_entries();
    ASSERT_EQ(frame1.size(), 2);
    EXPECT_EQ(frame1[0].berth_id, 101);
    EXPECT_EQ(frame1[1].berth_id, 102);

    // Depth 2: Vessel 2 on Berth 201
    stack.push_frame();
    stack.push({2, 201});

    EXPECT_EQ(stack.depth(), 2);
    EXPECT_EQ(stack.top().berth_id, 201);

    // Pop Depth 2 -> Back to Depth 1
    stack.pop_frame();
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.top().berth_id, 102);

    // Pop Depth 1 -> Empty
    stack.pop_frame();
    EXPECT_TRUE(stack.empty());
}

TEST(SearchStackTest, PopEntryInsideFrame)
{
    leviathan::bnb::SearchStack<int> stack;
    stack.push_frame();
    stack.push(10);
    stack.push(20);
    stack.push(30);

    stack.pop_entry();
    EXPECT_EQ(stack.top(), 20);
    EXPECT_EQ(stack.current_frame_entries().size(), 2);
}

TEST(SearchStackTest, FillFrameFromRange)
{
    leviathan::bnb::SearchStack<int> stack;
    std::vector<int> decisions = {1, 2, 3, 4, 5};

    // Test C++20 Range support (automatic frame creation)
    stack.fill_frame(decisions);

    EXPECT_EQ(stack.depth(), 1);
    const auto view = stack.current_frame_entries();
    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view.back(), 5);
}

TEST(SearchStackTest, FillFrameFromIterators)
{
    leviathan::bnb::SearchStack<int> stack;
    std::array<int, 3> data = {100, 200, 300};

    // Test Iterator Pair support
    stack.fill_frame(data.begin(), data.end());

    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.top(), 300);
}

TEST(SearchStackTest, ExtendExistingFrame)
{
    leviathan::bnb::SearchStack<int> stack;
    stack.push_frame();
    stack.push(10);

    std::vector<int> more = {20, 30};
    stack.extend(more);

    EXPECT_EQ(stack.current_frame_entries().size(), 3);
    EXPECT_EQ(stack.top(), 30);
}

TEST(SearchStackTest, FillFrameGenerator)
{
    leviathan::bnb::SearchStack<int> stack;

    // Use the generator to stream decisions directly to the tape
    stack.fill_frame([](auto& s)
    {
        s.push(42);
        s.emplace(84);
    });

    EXPECT_EQ(stack.depth(), 1);
    ASSERT_EQ(stack.current_frame_entries().size(), 2);
    EXPECT_EQ(stack.top(), 84);
}

TEST(SearchStackTest, FillFrameWithCapacityHint)
{
    leviathan::bnb::SearchStack<int> stack;
    stack.fill_frame(1000, [](auto& s)
    {
        for (int i = 0; i < 1000; ++i) s.push(i);
    });

    EXPECT_EQ(stack.current_frame_entries().size(), 1000);
}

TEST(SearchStackTest, ClearRetainsMemory)
{
    leviathan::bnb::SearchStack<int> stack(500, 50);
    stack.fill_frame({1, 2, 3});

    const size_t cap = stack.allocated_memory_bytes();
    stack.clear();

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.allocated_memory_bytes(), cap);
}

TEST(SearchStackTest, DeepNestingReallocation)
{
    // Start with small capacity to force growth
    leviathan::bnb::SearchStack<int> stack(2, 2);

    for (int i = 1; i <= 100; ++i)
    {
        stack.push_frame();
        stack.push(i);
    }

    EXPECT_EQ(stack.depth(), 100);
    EXPECT_EQ(stack.top(), 100);

    stack.pop_frame();
    EXPECT_EQ(stack.top(), 99);
}

#ifndef NDEBUG
TEST(SearchStackDeathTest, InvalidOperations)
{
    leviathan::bnb::SearchStack<int> stack;

    // Accessing top of empty stack
    EXPECT_DEATH([[maybe_unused]] auto& t = stack.top(), "");

    // Popping without a frame
    EXPECT_DEATH(stack.pop_frame(), "");

    // Popping entry from empty active frame
    stack.push_frame();
    EXPECT_DEATH(stack.pop_entry(), "");
}
#endif

TEST(SearchStackTest, GlobalIterationCoversAllFrames)
{
    leviathan::bnb::SearchStack<int> stack;

    // Depth 1: Push 10, 20
    stack.push_frame();
    stack.extend({10, 20});

    // Depth 2: Push 30, 40
    stack.push_frame();
    stack.extend({30, 40});

    // Depth 3: Push 50
    stack.push_frame();
    stack.push(50);

    // 1. Verify Current Frame (Local)
    const auto current_view = stack.current_frame_entries();
    EXPECT_EQ(current_view.size(), 1);
    EXPECT_EQ(current_view[0], 50);

    // 2. Verify Global Iterators (Root -> Leaf)
    std::vector<int> full_history;
    for (const auto& val : stack)
    {
        full_history.push_back(val);
    }

    // Should contain ALL values: 10, 20, 30, 40, 50
    ASSERT_EQ(full_history.size(), 5);
    EXPECT_EQ(full_history[0], 10); // Root
    EXPECT_EQ(full_history[4], 50); // Leaf

    // 3. Verify Standard Algorithms
    const auto it = std::ranges::find(stack, 30);
    EXPECT_NE(it, stack.end());
    EXPECT_EQ(*it, 30);
}

TEST(SearchStackTest, ReverseIteration)
{
    leviathan::bnb::SearchStack<int> stack;

    // Depth 1
    stack.fill_frame({1, 2});
    // Depth 2
    stack.fill_frame({3, 4});

    // Iterate backwards: should be 4, 3, 2, 1
    std::vector<int> reverse_history;
    for (int & it : std::ranges::reverse_view(stack))
    {
        reverse_history.push_back(it);
    }

    ASSERT_EQ(reverse_history.size(), 4);
    EXPECT_EQ(reverse_history[0], 4);
    EXPECT_EQ(reverse_history[3], 1);
}

TEST(SearchStackTest, InitializerListSyntax)
{
    leviathan::bnb::SearchStack<std::string> stack;

    // Test fill_frame({ ... })
    stack.fill_frame({"Vessel1", "Vessel2"});
    EXPECT_EQ(stack.depth(), 1);
    EXPECT_EQ(stack.top(), "Vessel2");

    // Test extend({ ... })
    stack.extend({"Vessel3"});
    EXPECT_EQ(stack.current_frame_size(), 3);
    EXPECT_EQ(stack.top(), "Vessel3");
}
