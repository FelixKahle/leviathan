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

#ifndef LEVIATHAN_BNB_SEARCH_STACK_H_
#define LEVIATHAN_BNB_SEARCH_STACK_H_

#include <vector>
#include <span>
#include <concepts>
#include <ranges>
#include <utility>
#include <initializer_list>
#include "absl/log/check.h"
#include "leviathan/base/config.h"

namespace leviathan::bnb
{
    /// \brief A high-performance, frame-structured decision stack.
    ///
    /// The SearchStack stores all candidate decisions linearly in a single vector ('entries').
    /// It uses a second vector ('frames') to store indices that mark where each search depth begins.
    /// This allows the solver to push multiple sibling decisions for a single node level
    /// and "slice" them as a contiguous view.
    ///
    /// \tparam T The type of Decision/Move being stored.
    template <typename T>
    class SearchStack
    {
    public:
        using container_type = std::vector<T>;
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = const T&;
        using iterator = container_type::iterator;
        using const_iterator = container_type::const_iterator;
        using reverse_iterator = container_type::reverse_iterator;
        using const_reverse_iterator = container_type::const_reverse_iterator;

        SearchStack() = default;

        /// \brief Constructs a stack with pre-allocated capacities.
        ///
        /// \param entry_capacity Total number of potential decisions across all depths.
        /// \param frame_capacity Maximum search depth.
        explicit LEVIATHAN_FORCE_INLINE SearchStack(const size_type entry_capacity, const size_type frame_capacity)
        {
            entries_.reserve(entry_capacity);
            frames_.reserve(frame_capacity);
        }

        /// \brief Starts a new decision level (frame).
        ///
        /// Records the current end of the entries tape. All subsequently pushed
        /// decisions will belong to this new frame until pop_frame() is called.
        LEVIATHAN_FORCE_INLINE void push_frame()
        {
            frames_.push_back(entries_.size());
        }

        /// \brief Removes the current frame and truncates the entry tape.
        ///
        /// Resets the decision tape to the state it was in before push_frame()
        /// was called for this level.
        LEVIATHAN_FORCE_INLINE void pop_frame()
        {
            DCHECK(!frames_.empty());
            const size_type start_index = frames_.back();
            frames_.pop_back();
            entries_.resize(start_index);
        }

        /// \brief Pushes a candidate decision into the current active frame.
        LEVIATHAN_FORCE_INLINE void push(const T& decision)
        {
            DCHECK(!frames_.empty());
            entries_.push_back(decision);
        }

        /// \brief Pushes a candidate decision into the current active frame (move).
        LEVIATHAN_FORCE_INLINE void push(T&& decision)
        {
            DCHECK(!frames_.empty());
            entries_.push_back(std::move(decision));
        }

        /// \brief Constructs a decision in-place in the current frame.
        template <typename... Args>
        LEVIATHAN_FORCE_INLINE reference emplace(Args&&... args)
        {
            DCHECK(!frames_.empty());
            return entries_.emplace_back(std::forward<Args>(args)...);
        }

        /// \brief Pops the last pushed decision (LIFO).
        LEVIATHAN_FORCE_INLINE void pop_entry()
        {
            DCHECK(!entries_.empty());
            DCHECK(entries_.size() > frames_.back());
            entries_.pop_back();
        }

        /// \brief Returns the last decision in the current frame.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE reference top() noexcept
        {
            DCHECK(!entries_.empty());
            DCHECK(entries_.size() > frames_.back());
            return entries_.back();
        }

        /// \brief Returns a slice of all decisions in the current frame level.
        ///
        /// This is the key "Hole Visibility" feature. It allows the solver to
        /// see all alternative berths/gaps generated for the current vessel.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE std::span<T> current_frame_entries() noexcept
        {
            if (frames_.empty())
            {
                return {};
            }
            const size_type start = frames_.back();
            return std::span<T>(entries_.data() + start, entries_.size() - start);
        }

        /// \brief Returns a slice of all decisions in the current frame level.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE std::span<const T> current_frame_entries() const noexcept
        {
            if (frames_.empty())
            {
                return {};
            }
            const size_type start = frames_.back();
            return std::span<const T>(entries_.data() + start, entries_.size() - start);
        }

        /// \brief Returns the current search depth (number of active frames).
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type depth() const noexcept
        {
            return frames_.size();
        }

        /// \brief Checks if the stack is empty (no active frames).
        [[nodiscard]] LEVIATHAN_FORCE_INLINE bool empty() const noexcept
        {
            return frames_.empty();
        }

        /// \brief Reserves memory for entries and frames to prevent reallocations during search.
        ///
        /// The entry_capacity should be the total number of decisions expected across all frames,
        /// while frame_capacity should be the maximum expected search depth.
        LEVIATHAN_FORCE_INLINE void reserve(const size_type entry_capacity, const size_type frame_capacity)
        {
            entries_.reserve(entry_capacity);
            frames_.reserve(frame_capacity);
        }

        /// \brief Shrinks the capacity of entries and frames to fit their current size.
        ///
        /// This should not be called, as it may cause expensive reallocations.
        /// Provided for future or advanced use cases where memory needs to be reclaimed after search.
        LEVIATHAN_FORCE_INLINE void shrink_to_fit() noexcept
        {
            entries_.shrink_to_fit();
            frames_.shrink_to_fit();
        }

        /// \brief Returns the number of entries in the current active frame.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type current_frame_size() const noexcept
        {
            return frames_.empty() ? 0 : entries_.size() - frames_.back();
        }

        /// \brief Resets the entire stack while retaining allocated capacity.
        LEVIATHAN_FORCE_INLINE void clear() noexcept
        {
            entries_.clear();
            frames_.clear();
        }

        /// \brief Starts a new search frame and populates it from a range of decisions.
        ///
        /// \tparam R A type satisfying the std::ranges::input_range concept.
        /// \param range The range of decisions to append to the new frame.
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_value_t<R>, T>
        LEVIATHAN_FORCE_INLINE void fill_frame(R&& range)
        {
            push_frame();
            extend(std::forward<R>(range));
        }

        /// \brief Appends a range of decisions to the current active frame.
        ///
        /// \tparam R A type satisfying the std::ranges::input_range concept.
        /// \param range The range of decisions to append.
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_value_t<R>, T>
        LEVIATHAN_FORCE_INLINE void extend(R&& range)
        {
            DCHECK(!frames_.empty());

#if LEVIATHAN_SUPPORTS_CONTAINER_RANGE_INSERT
            entries_.append_range(std::forward<R>(range));
#else
            auto&& r = std::forward<R>(range);
            entries_.insert(entries_.end(), std::begin(r), std::end(r));
#endif
        }

        /// \brief Starts a new search frame and populates it from an iterator range.
        ///
        /// \tparam InputIt A type satisfying the std::input_iterator concept.
        /// \param first The beginning of the range of candidates.
        /// \param last The end of the range of candidates.
        template <std::input_iterator InputIt>
            requires std::convertible_to<typename std::iterator_traits<InputIt>::value_type, T>
        LEVIATHAN_FORCE_INLINE void fill_frame(InputIt first, InputIt last)
        {
            push_frame();
            extend(first, last);
        }

        /// \brief Appends a range of decisions to the current active frame using iterators.
        ///
        /// This is the backbone for all bulk-loading operations. It detects
        /// random-access iterators to perform a single reservation for the tape.
        ///
        /// \tparam InputIt A type satisfying the std::input_iterator concept.
        /// \param first The beginning of the range.
        /// \param last The end of the range.
        template <std::input_iterator InputIt>
            requires std::convertible_to<typename std::iterator_traits<InputIt>::value_type, T>
        LEVIATHAN_FORCE_INLINE void extend(InputIt first, InputIt last)
        {
            DCHECK(!frames_.empty());

            if constexpr (std::random_access_iterator<InputIt>)
            {
                const auto count = std::distance(first, last);
                entries_.reserve(entries_.size() + static_cast<size_type>(count));
            }
            entries_.insert(entries_.end(), first, last);
        }

        /// \brief Starts a new search frame and populates it using a generator callback.
        ///
        /// This is the highest-performance way to populate a frame when decisions are
        /// calculated on-the-fly (e.g., scanning for gaps). The generator writes
        /// directly to the stack's linear tape, bypassing intermediate containers.
        ///
        /// \tparam Generator A callable type invocable with `SearchStack<T>&`.
        /// \param gen The generator function (usually a lambda).
        template <typename Generator>
            requires std::invocable<Generator, SearchStack<T>&>
        LEVIATHAN_FORCE_INLINE void fill_frame(Generator&& gen)
        {
            push_frame();
            gen(*this);
        }

        /// \brief Starts a new search frame with a capacity hint and a generator.
        ///
        /// Same as the standard generator overload, but performs a single `reserve`
        /// call before execution to prevent multiple reallocations within the frame.
        ///
        /// \tparam Generator A callable type invocable with `SearchStack<T>&`.
        /// \param count_hint The expected number of entries to be added.
        /// \param gen The generator function.
        template <typename Generator>
            requires std::invocable<Generator, SearchStack<T>&>
        LEVIATHAN_FORCE_INLINE void fill_frame(const size_type count_hint, Generator&& gen)
        {
            push_frame();
            entries_.reserve(entries_.size() + count_hint);
            gen(*this);
        }

        /// \brief Starts a new search frame and populates it from an initializer list.
        ///
        /// This allows for convenient syntax like stack.fill_frame({move1, move2}).
        ///
        /// \param list The initializer list of decisions.
        LEVIATHAN_FORCE_INLINE void fill_frame(std::initializer_list<T> list)
        {
            push_frame();
            extend(list);
        }

        /// \brief Appends an initializer list of decisions to the current active frame.
        ///
        /// \param list The initializer list to append.
        LEVIATHAN_FORCE_INLINE void extend(std::initializer_list<T> list)
        {
            DCHECK(!frames_.empty());
#if LEVIATHAN_SUPPORTS_CONTAINER_RANGE_INSERT
            entries_.append_range(list);
#else
            entries_.insert(entries_.end(), list.begin(), list.end());
#endif
        }

        /// \brief Returns total allocated memory in bytes.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_type allocated_memory_bytes() const noexcept
        {
            return (entries_.capacity() * sizeof(T)) + (frames_.capacity() * sizeof(size_type));
        }

        /// \name Global Iterators
        /// Iterates over the ENTIRE stack history (Root -> Leaf).
        /// @{

        [[nodiscard]] LEVIATHAN_FORCE_INLINE iterator begin() noexcept
        {
            return entries_.begin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator begin() const noexcept
        {
            return entries_.begin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator cbegin() const noexcept
        {
            return entries_.cbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE iterator end() noexcept
        {
            return entries_.end();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator end() const noexcept
        {
            return entries_.end();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator cend() const noexcept
        {
            return entries_.cend();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE reverse_iterator rbegin() noexcept
        {
            return entries_.rbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator rbegin() const noexcept
        {
            return entries_.rbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator crbegin() const noexcept
        {
            return entries_.crbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE reverse_iterator rend() noexcept
        {
            return entries_.rend();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator rend() const noexcept
        {
            return entries_.rend();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator crend() const noexcept
        {
            return entries_.crend();
        }

        /// @}

    private:
        std::vector<T> entries_;
        std::vector<size_type> frames_;
    };
}

#endif // LEVIATHAN_BNB_SEARCH_STACK_H_
