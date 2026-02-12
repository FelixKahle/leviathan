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

#ifndef LEVIATHAN_BNB_SEARCH_TRAIL_H_
#define LEVIATHAN_BNB_SEARCH_TRAIL_H_

#include <vector>
#include <concepts>
#include "absl/log/check.h"
#include "leviathan/base/config.h"

namespace leviathan::bnb
{
    /// \brief A generic, high-performance undo trail.
    ///
    /// Acts as a frame-based stack for history entries of type T.
    /// Unlike complex field-based trails, this simply stores "Move Bundles"
    /// linearly. This maximizes cache locality for problems where multiple
    /// state fields change simultaneously (like BAP).
    ///
    /// \tparam T The type of the restoration entry (e.g., a struct capturing old values).
    template <typename T>
    class SearchTrail
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = const T&;

        SearchTrail() = default;

        /// \brief Pre-allocates memory for the trail.
        ///
        /// \param entry_capacity Total number of moves to store (estimated nodes * avg depth).
        /// \param frame_capacity Maximum search depth.
        explicit LEVIATHAN_FORCE_INLINE SearchTrail(const size_type entry_capacity, const size_type frame_capacity)
        {
            entries_.reserve(entry_capacity);
            frames_.reserve(frame_capacity);
        }

        // SearchTrail manages critical state; copying is unsafe.
        SearchTrail(const SearchTrail&) = delete;
        SearchTrail& operator=(const SearchTrail&) = delete;

        SearchTrail(SearchTrail&&) = default;
        SearchTrail& operator=(SearchTrail&&) = default;

        /// \brief Marks the start of a new history frame (decision level).
        LEVIATHAN_FORCE_INLINE void push_frame()
        {
            frames_.push_back(entries_.size());
        }

        /// \brief Pushes a restoration entry onto the current frame.
        LEVIATHAN_FORCE_INLINE void push(const T& entry)
        {
            entries_.push_back(entry);
        }

        /// \brief Pushes a restoration entry (move semantics).
        LEVIATHAN_FORCE_INLINE void push(T&& entry)
        {
            entries_.push_back(std::move(entry));
        }

        /// \brief Emplaces a restoration entry directly on the tape.
        template <typename... Args>
        LEVIATHAN_FORCE_INLINE void emplace(Args&&... args)
        {
            entries_.emplace_back(std::forward<Args>(args)...);
        }

        /// \brief Backtracks the current frame, applying the undo operation to each entry.
        ///
        /// Iterates through the current frame's entries in reverse order (LIFO),
        /// calls the provided undo function for each, and then removes the frame.
        ///
        /// \tparam UndoFunc A callable invocable with `const T&`.
        /// \param undo_func The callback that restores state from an entry.
        template <typename UndoFunc>
            requires std::invocable<UndoFunc, const_reference>
        LEVIATHAN_FORCE_INLINE void backtrack(UndoFunc&& undo_func)
        {
            DCHECK(!frames_.empty());
            const size_type start_index = frames_.back();
            frames_.pop_back();

            // Reverse iteration for LIFO restoration
            // Using a while loop is often cleaner for vector pop_back logic
            // Note that this is not an infinite loop as long as we pop entries in the loop.
            // ReSharper disable once CppDFALoopConditionNotUpdated
            while (entries_.size() > start_index)
            {
                undo_func(entries_.back());
                entries_.pop_back();
            }
        }

        /// \brief Returns the number of active frames (depth).
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type depth() const noexcept
        {
            return frames_.size();
        }

        /// \brief Returns true if there are no active frames.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE bool empty() const noexcept
        {
            return frames_.empty();
        }

        /// \brief Clears all history without releasing memory capacity.
        LEVIATHAN_FORCE_INLINE void clear() noexcept
        {
            entries_.clear();
            frames_.clear();
        }

        /// \name Memory Management
        /// @{

        /// \brief Reserves memory to prevent reallocations during search.
        LEVIATHAN_FORCE_INLINE void reserve(const size_type entry_cap, const size_type frame_cap)
        {
            entries_.reserve(entry_cap);
            frames_.reserve(frame_cap);
        }

        /// \brief Returns the total bytes allocated (capacity) by the vectors.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type allocated_memory_bytes() const noexcept
        {
            return (entries_.capacity() * sizeof(T)) + (frames_.capacity() * sizeof(size_type));
        }

        /// \brief Returns the total bytes currently used by valid history data.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type used_memory_bytes() const noexcept
        {
            return (entries_.size() * sizeof(T)) + (frames_.size() * sizeof(size_type));
        }

        /// \brief Shrinks the capacity of the trail stacks to fit their current size.
        LEVIATHAN_FORCE_INLINE void shrink_to_fit() noexcept
        {
            entries_.shrink_to_fit();
            frames_.shrink_to_fit();
        }

        /// @}

    private:
        std::vector<T> entries_;
        std::vector<size_type> frames_;
    };
}

#endif // LEVIATHAN_BNB_SEARCH_TRAIL_H_
