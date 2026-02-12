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

#ifndef LEVIATHAN_BNB_BERTH_TIMELINE_H_
#define LEVIATHAN_BNB_BERTH_TIMELINE_H_

#include <algorithm>
#include <utility>
#include <vector>
#include <optional>
#include <ranges>
#include <iterator>
#include "leviathan/base/config.h"

namespace leviathan::bnb
{
    /// \brief Represents an interval where a berth is available for service.
    template <typename TimeType>
    struct AvailableWindow
    {
        TimeType start_inclusive;
        TimeType end_exclusive;

        /// \brief Used for binary search to find the window containing or following a time point.
        bool operator<(TimeType t) const noexcept { return end_exclusive <= t; }
    };

    /// \brief Manages static availability constraints, supporting range and fixed-assignment logic.
    template <typename TimeType>
    class BerthTimeline
    {
    public:
        using window_type = AvailableWindow<TimeType>;
        using container_type = std::vector<window_type>;
        using iterator = container_type::iterator;
        using const_iterator = container_type::const_iterator;
        using reverse_iterator = container_type::reverse_iterator;
        using const_reverse_iterator = container_type::const_reverse_iterator;

        BerthTimeline() = default;

        /// \name Constructors
        /// @{

        /// \brief Construct from a simple range.
        LEVIATHAN_FORCE_INLINE BerthTimeline(TimeType open, TimeType close)
        {
            assign(open, close);
        }

        /// \brief Construct from a direct list of availability windows.
        template <typename R>
            requires std::ranges::input_range<R> && (!std::same_as<std::remove_cvref_t<R>, BerthTimeline>)
        explicit LEVIATHAN_FORCE_INLINE BerthTimeline(R&& windows)
        {
            assign(std::forward<R>(windows));
        }

        /// \brief Construct from availability windows and fixed assignments (carving logic).
        template <typename Windows, typename Fixed>
            requires std::ranges::input_range<Windows> && std::ranges::input_range<Fixed>
        LEVIATHAN_FORCE_INLINE BerthTimeline(Windows&& availability, Fixed&& fixed_assignments)
        {
            assign(std::forward<Windows>(availability), std::forward<Fixed>(fixed_assignments));
        }

        /// @}

        /// \name Assignment API (Memory Reuse)
        /// @{

        /// \brief Reuse memory for a simple range.
        LEVIATHAN_FORCE_INLINE void assign(TimeType open, TimeType close)
        {
            windows_.clear();
            if (open < close)
            {
                windows_.push_back({open, close});
            }
        }

        /// \brief Reuse memory for a direct list of windows.
        template <typename R>
            requires std::ranges::input_range<R>
        LEVIATHAN_FORCE_INLINE void assign(R&& windows)
        {
            windows_.assign(std::begin(windows), std::end(windows));
        }

        /// \brief Reuse memory by carving fixed assignments out of availability windows.
        template <typename Windows, typename Fixed>
            requires std::ranges::input_range<Windows> && std::ranges::input_range<Fixed>
        void assign(Windows&& availability, Fixed&& fixed_assignments)
        {
            windows_.clear();
            auto fixed_it = std::begin(fixed_assignments);
            const auto fixed_end = std::end(fixed_assignments);

            for (const auto& avail : availability)
            {
                TimeType current_start = avail.start_inclusive;

                while (fixed_it != fixed_end && fixed_it->start_inclusive < avail.end_exclusive)
                {
                    if (fixed_it->end_exclusive <= current_start)
                    {
                        ++fixed_it;
                        continue;
                    }

                    if (fixed_it->start_inclusive > current_start)
                    {
                        windows_.push_back({current_start, fixed_it->start_inclusive});
                    }

                    current_start = std::max(current_start, fixed_it->end_exclusive);

                    if (current_start >= avail.end_exclusive) break;

                    if (fixed_it->end_exclusive < avail.end_exclusive)
                    {
                        ++fixed_it;
                    }
                    else break;
                }

                if (current_start < avail.end_exclusive)
                {
                    windows_.push_back({current_start, avail.end_exclusive});
                }
            }
        }

        /// \brief Clears windows while retaining capacity.
        LEVIATHAN_FORCE_INLINE void clear() noexcept
        {
            windows_.clear();
        }

        /// @}

        /// \name Search API
        /// @{

        /// \brief Finds the earliest possible start time for a vessel on this berth.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE std::optional<TimeType> find_earliest_start(
            const TimeType ready_time,
            const TimeType duration) const noexcept
        {
            if (windows_.empty())
            {
                return std::nullopt;
            }

            auto it = std::lower_bound(windows_.begin(), windows_.end(), ready_time);

            for (; it != windows_.end(); ++it)
            {
                const TimeType actual_start = std::max(ready_time, it->start_inclusive);
                if (duration <= (it->end_exclusive - actual_start))
                {
                    return actual_start;
                }
            }
            return std::nullopt;
        }

        /// @}

        /// \name Iterator Access
        /// @{
        [[nodiscard]] LEVIATHAN_FORCE_INLINE iterator begin() noexcept
        {
            return windows_.begin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE iterator end() noexcept
        {
            return windows_.end();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator begin() const noexcept
        {
            return windows_.begin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator end() const noexcept
        {
            return windows_.end();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator cbegin() const noexcept
        {
            return windows_.cbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_iterator cend() const noexcept
        {
            return windows_.cend();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE reverse_iterator rbegin() noexcept
        {
            return windows_.rbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE reverse_iterator rend() noexcept
        {
            return windows_.rend();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator crbegin() const noexcept
        {
            return windows_.crbegin();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE const_reverse_iterator crend() const noexcept
        {
            return windows_.crend();
        }

        /// @}

        [[nodiscard]] LEVIATHAN_FORCE_INLINE size_t size() const noexcept
        {
            return windows_.size();
        }

        [[nodiscard]] LEVIATHAN_FORCE_INLINE bool empty() const noexcept
        {
            return windows_.empty();
        }

    private:
        container_type windows_;
    };
}

#endif // LEVIATHAN_BNB_BERTH_TIMELINE_H_
