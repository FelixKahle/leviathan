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
#include <utility>
#include <concepts>
#include "leviathan/base/config.h"

namespace leviathan::bnb
{
    /// \brief A high-performance state restoration mechanism for Branch and Bound.
    ///
    /// The SearchTrail implements a "trail-based" undo system, which is significantly
    /// faster than copying the entire state at every node. It records only the
    /// specific changes (deltas) made to global data structures and reverses them
    /// when backtracking.
    ///
    /// Key Features:
    /// - **Zero Allocation:** Uses pre-allocated vectors to store history, avoiding
    ///   heap allocation during the critical search loop.
    /// - **Hybrid Tracking:** distinct strategies for "Values" (restore to $X$)
    ///   and "Dirty Indices" (reset to default).
    /// - **Cache Locality:** Restoration iterates strictly in LIFO order (reverse),
    ///   keeping memory access patterns predictable and cache-friendly.
    ///
    /// \tparam T The type of values being tracked (e.g., int64_t for numeric state, or a struct for complex state).
    template<typename T>
    class SearchTrail
    {
    public:
        using size_type = std::size_t;
        using index_type = std::size_t;
        using value_type = T;

        /// \brief Default constructor.
        SearchTrail() noexcept = default;

        /// \brief Constructs a trail with reserved memory.
        ///
        /// Pre-allocating the trail stacks is crucial for preventing vector
        /// resizing during deep tree searches.
        ///
        /// \param depth The estimated maximum depth of the search tree.
        explicit LEVIATHAN_FORCE_INLINE SearchTrail(const size_type depth)
        {
            value_trail_.reserve(depth);
            dirty_indices_.reserve(depth);
            checkpoints_.reserve(depth);
        }

        // SearchTrail manages significant state history; copying is expensive and
        // usually indicative of a logic error in the search strategy.
        SearchTrail(const SearchTrail&) = delete;
        SearchTrail& operator=(const SearchTrail&) = delete;

        SearchTrail(SearchTrail&&) = default;
        SearchTrail& operator=(SearchTrail&&) = default;

        /// \name Recording API
        /// Functions to record changes *before* modifying the global state.
        /// @{

        /// \brief Records a value change for restoration.
        ///
        /// Should be called immediately before overwriting a value in a global array.
        ///
        /// \param index The index of the element being modified.
        /// \param value The *original* value to restore upon backtracking.
        LEVIATHAN_FORCE_INLINE void save_value(const index_type index, const value_type value)
        {
            value_trail_.emplace_back(value, index);
        }

        /// \brief Marks an index as "touched" or "dirty".
        ///
        /// Used for sparse data structures where the specific old value is irrelevant,
        /// but the entry must be reset (e.g., setting a boolean flag to false,
        /// or an ID to -1).
        ///
        /// \param index The index that was modified.
        LEVIATHAN_FORCE_INLINE void mark_touched(const index_type index)
        {
            dirty_indices_.emplace_back(index);
        }

        /// \brief Creates a restoration checkpoint.
        ///
        /// "Bookmarks" the current size of the trail stacks. When \ref backtrack
        /// is called, the state will be reverted to this exact point.
        LEVIATHAN_FORCE_INLINE void push_checkpoint()
        {
            checkpoints_.emplace_back(value_trail_.size(), dirty_indices_.size());
        }

        /// @}

        /// \name Restoration API
        /// Functions to revert state to the previous checkpoint.
        /// @{

        /// \brief Reverts state changes to the most recent checkpoint.
        ///
        /// This generic function allows for complex cleanup logic via a user-provided
        /// callable. It processes "dirty" indices first, followed by value restoration.
        ///
        /// \tparam ValueContainer A container supporting `operator[]` assignment.
        /// \tparam CleanupFunc A callable type (lambda/functor) invocable with `index_type`.
        ///
        /// \param values The container to restore old values to.
        /// \param cleanup_op A callback executed for every dirty index recorded since
        ///                   the last checkpoint.
        template <typename ValueContainer, typename CleanupFunc>
            requires std::invocable<CleanupFunc, index_type>
            && requires(ValueContainer& c, index_type i, value_type v)
            {
                c[i] = v;
            }
        LEVIATHAN_FORCE_INLINE void backtrack(ValueContainer& values, CleanupFunc&& cleanup_op)
        {
            if (checkpoints_.empty()) [[unlikely]]
            {
                return;
            }

            const auto [target_val_size, target_dirty_size] = checkpoints_.back();
            checkpoints_.pop_back();

            // Cleanup Dirty Indices.
            // Iterating backwards maintains LIFO order, which is cache-friendly
            // as recent changes are likely still hot in L1/L2 cache.
            const size_type current_dirty_size = dirty_indices_.size();
            for (size_type i = current_dirty_size; i > target_dirty_size; --i)
            {
                cleanup_op(dirty_indices_[i - 1].index);
            }
            dirty_indices_.resize(target_dirty_size);

            // Restore Old Values.
            const size_type current_val_size = value_trail_.size();
            for (size_type i = current_val_size; i > target_val_size; --i)
            {
                const auto& [old_value, index] = value_trail_[i - 1];
                values[index] = old_value;
            }
            value_trail_.resize(target_val_size);
        }

        /// \brief Merges the changes of the current level into the previous level.
        ///
        /// Removes the top checkpoint without restoring values. The changes recorded
        /// since that checkpoint effectively become part of the parent scope.
        LEVIATHAN_FORCE_INLINE void commit_checkpoint()
        {
            if (!checkpoints_.empty()) [[likely]]
            {
                checkpoints_.pop_back();
            }
        }

        /// \brief Convenience overload for resetting dirty indices to a fixed value.
        ///
        /// Useful for resetting boolean flags (to false) or IDs (to -1/0) without
        /// writing a custom lambda.
        ///
        /// \tparam ValueContainer Type of the value container.
        /// \tparam DirtyContainer Type of the container tracking dirty indices.
        /// \tparam ResetValueType Type of the value to assign during cleanup.
        ///
        /// \param values The container to restore old values to.
        /// \param dirty_target The container where dirty indices will be reset.
        /// \param reset_val The constant value to assign to all dirty indices.
        template <typename ValueContainer, typename DirtyContainer, typename ResetValueType>
            requires requires(ValueContainer& vc, DirtyContainer& dc, index_type i, value_type v, ResetValueType rv)
            {
                vc[i] = v;
                dc[i] = rv;
            }
        LEVIATHAN_FORCE_INLINE void backtrack(ValueContainer& values, DirtyContainer& dirty_target,
                                              const ResetValueType reset_val)
        {
            backtrack(values, [&](const index_type idx)
            {
                dirty_target[idx] = reset_val;
            });
        }

        /// @}

        /// \name Utility
        /// @{

        /// \brief Clears all history and checkpoints.
        ///
        /// Resets the trail to an empty state. Does not release reserved memory capacity.
        LEVIATHAN_FORCE_INLINE void clear() noexcept
        {
            value_trail_.clear();
            dirty_indices_.clear();
            checkpoints_.clear();
        }

        /// \brief Returns the current depth of the checkpoint stack.
        /// \return The number of active checkpoints.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type depth() const noexcept
        {
            return checkpoints_.size();
        }

        /// \brief Checks if there are no active checkpoints.
        /// \return true if the trail is empty.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE bool empty() const noexcept
        {
            return checkpoints_.empty();
        }

        /// @}

        /// \brief Returns the size in bytes of the valid history entries.
        ///
        /// \return The size of the data currently active in the trail.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type used_memory_bytes() const noexcept
        {
            return (value_trail_.size() * sizeof(ValueEntry)) +
                   (dirty_indices_.size() * sizeof(DirtyEntry)) +
                   (checkpoints_.size() * sizeof(std::pair<size_type, size_type>));
        }

        /// \brief Returns the total reserved memory in bytes.
        ///
        /// Useful for debugging to ensure vectors are not reallocating during search.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type reserved_memory_bytes() const noexcept
        {
            return (value_trail_.capacity() * sizeof(ValueEntry)) +
                   (dirty_indices_.capacity() * sizeof(DirtyEntry)) +
                   (checkpoints_.capacity() * sizeof(std::pair<size_type, size_type>));
        }

        /// \brief Reserves memory for the trail stacks.
        ///
        /// Should be called before search to prevent reallocations. The size should be
        /// estimated based on the expected maximum depth and branching factor of the search tree.
        LEVIATHAN_FORCE_INLINE void reserve(const size_type size)
        {
            value_trail_.reserve(size);
            dirty_indices_.reserve(size);
            checkpoints_.reserve(size);
        }

        /// \brief Shrinks the capacity of the trail stacks to fit their current size.
        ///
        /// This should not be called as it may cause expensive reallocations.
        /// Provided for future or advanced use cases where memory needs to be reclaimed after search.
        LEVIATHAN_FORCE_INLINE void shrink_to_fit() noexcept
        {
            value_trail_.shrink_to_fit();
            dirty_indices_.shrink_to_fit();
            checkpoints_.shrink_to_fit();
        }

    private:
        /// \brief Internal structure to record value changes.
        struct ValueEntry
        {
            value_type old_value;
            index_type index;
        };

        /// \brief Internal structure to record dirty indices.
        struct DirtyEntry
        {
            index_type index;
        };

        std::vector<ValueEntry> value_trail_;
        std::vector<DirtyEntry> dirty_indices_;
        std::vector<std::pair<size_type, size_type>> checkpoints_;
    };
}

#endif // LEVIATHAN_BNB_SEARCH_TRAIL_H_
