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

#ifndef LEVIATHAN_BNB_SEARCH_STATE_H_
#define LEVIATHAN_BNB_SEARCH_STATE_H_

#include <vector>
#include <concepts>
#include "absl/log/check.h"
#include "leviathan/base/config.h"

namespace leviathan::bnb
{
    /// \brief Represents the mutable state of the Branch and Bound search.
    template <typename TimeType, typename IndexType, typename CostType>
        requires std::integral<TimeType> && std::is_signed_v<TimeType> &&
        std::integral<IndexType> && std::is_signed_v<IndexType> &&
        std::is_arithmetic_v<CostType>
    class SearchState
    {
    public:
        using time_type = TimeType;
        using index_type = IndexType;
        using cost_type = CostType;

        static constexpr IndexType kUnassignedVessel = -1;

        std::vector<TimeType> berth_free_times;
        std::vector<IndexType> vessel_assignments;
        std::vector<TimeType> vessel_start_times;
        IndexType last_assigned_vessel = kUnassignedVessel;
        CostType current_objective = 0;

        SearchState() = default;

        /// \brief Constructs a SearchState with specified numbers of berths and vessels.
        ///
        /// All berths start free at time 0, and all vessels are initially unassigned.
        ///
        /// \param num_berths The total number of berths in the problem instance.
        /// \param num_vessels The total number of vessels in the problem instance.
        LEVIATHAN_FORCE_INLINE explicit SearchState(const size_t num_berths, const size_t num_vessels)
        {
            berth_free_times.assign(num_berths, 0);
            vessel_assignments.assign(num_vessels, kUnassignedVessel);
            vessel_start_times.assign(num_vessels, 0);
        }

        /// \brief Constructs a state from existing collections (e.g., a warm start).
        template <typename BerthTimes, typename Assignments, typename StartTimes>
            requires std::ranges::input_range<BerthTimes> &&
            std::ranges::input_range<Assignments> &&
            std::ranges::input_range<StartTimes>
        LEVIATHAN_FORCE_INLINE explicit SearchState(BerthTimes&& berth_times,
                                                    Assignments&& assignments,
                                                    StartTimes&& start_times)
            : berth_free_times(std::forward<BerthTimes>(berth_times)),
              vessel_assignments(std::forward<Assignments>(assignments)),
              vessel_start_times(std::forward<StartTimes>(start_times))
        {
            DCHECK_EQ(vessel_assignments.size(), vessel_start_times.size());
        }

        /// \brief Checks if a vessel is currently assigned to a berth.
        ///
        /// \param v_idx The index of the vessel to check.
        /// \return \c true if the vessel is assigned to a berth, \c false otherwise.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE bool is_assigned(const IndexType v_idx) const
        {
            DCHECK_GE(v_idx, 0);
            DCHECK_LT(v_idx, static_cast<IndexType>(vessel_assignments.size()));
            return vessel_assignments[v_idx] != kUnassignedVessel;
        }

        /// \brief Retrieves the start time of a vessel's assigned berth.
        ///
        /// \param v_idx The index of the vessel.
        /// \return The start time of the berth to which the vessel is assigned.
        /// \note This should only be called if the vessel is assigned to a berth (i.e., is_assigned(v_idx) == true).
        [[nodiscard]] LEVIATHAN_FORCE_INLINE TimeType get_start_time(const IndexType v_idx) const
        {
            DCHECK(is_assigned(v_idx));
            return vessel_start_times[v_idx];
        }

        /// \brief Retrieves the index of the berth assigned to a vessel.
        ///
        /// \param v_idx The index of the vessel.
        /// \return The index of the berth to which the vessel is assigned.
        [[nodiscard]] LEVIATHAN_FORCE_INLINE IndexType get_assigned_berth(const IndexType v_idx) const
        {
            DCHECK(is_assigned(v_idx));
            return vessel_assignments[v_idx];
        }

        /// \brief Applies a move to the state.
        ///
        /// Updates the berth free times, vessel assignments, start times, and objective value based on the move.
        /// This should only be called with valid moves (i.e., the vessel is not already assigned).
        ///
        /// \param v_idx The index of the vessel being assigned.
        /// \param b_idx The index of the berth to which the vessel is being assigned.
        /// \param start_time The start time of the vessel at the assigned berth.
        /// \param finish_time The finish time of the vessel at the assigned berth (used to update the berth's free time).
        /// \param cost_delta The change in the objective function value resulting from this move (used to update current_objective).
        LEVIATHAN_FORCE_INLINE void apply_move(const IndexType v_idx, const IndexType b_idx, const TimeType start_time,
                                               const TimeType finish_time, const CostType cost_delta)
        {
            DCHECK(!is_assigned(v_idx));

            berth_free_times[b_idx] = finish_time;
            vessel_assignments[v_idx] = b_idx;
            vessel_start_times[v_idx] = start_time;
            current_objective += cost_delta;
            last_assigned_vessel = v_idx;
        }

        /// \brief Backtracks a move (called by SearchTrail).
        ///
        /// Reverts the state using the data stored in a trail entry.
        /// Now includes the old_last_vessel parameter to fix the metadata issue.
        ///
        /// \param v_idx The index of the vessel being unassigned.
        /// \param b_idx The index of the berth from which the vessel is being unassigned
        /// \param old_berth_free_time The previous free time of the berth before the move was applied.
        /// \param old_objective The previous objective value before the move was applied.
        /// \param old_last_vessel The previous last assigned vessel index before the move was applied
        LEVIATHAN_FORCE_INLINE void backtrack_move(const IndexType v_idx, const IndexType b_idx,
                                                   const TimeType old_berth_free_time, const CostType old_objective,
                                                   const IndexType old_last_vessel)
        {
            berth_free_times[b_idx] = old_berth_free_time;
            vessel_assignments[v_idx] = kUnassignedVessel;
            current_objective = old_objective;
            last_assigned_vessel = old_last_vessel;
        }
    };
}

#endif // LEVIATHAN_BNB_SEARCH_STATE_H_
