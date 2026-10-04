/** Pure staging of already-compiled native Clip notes, extent and meter. */
#pragma once

#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {

enum class ManagedClipRevisionPhase {
    ExpandGeometry,
    UpdateNotePopulation,
    FinalizeGeometry,
};

struct ManagedClipRevisionStage {
    ManagedClipRevisionPhase phase;
    ManagedClipProjection projection;
    /// One canonical e<EventId>_n<ordinal> key for each corresponding note.
    std::vector<std::string> desired_note_keys;
};

/// Plan at most three complete desired projections, all under the caller's
/// unchanged final owning revision. Times and extent are already-compiled Live
/// quarter-note beats. This function neither recompiles Score nor retimes notes
/// for meter, and it never invents native IDs or authorizes native operations.
///
/// When notes change, extend first if necessary, then revise notes at
/// max(previous_end, final_end) with the previous meter, then finish extent/meter.
/// Shrinking therefore follows removal/shortening of old note tails. Mere array
/// reordering with unchanged key-to-note associations produces no note phase.
/// Requests outside the Clip are copied as explicitly unapplied metadata in each
/// stage; the caller must retain final outside-Clip metadata even for an empty
/// plan. No stage applies those requests.
///
/// ProtocolError denotes malformed projection/keys. TargetValueUnrepresentable
/// denotes retained-Event ordinal/cardinality changes or revisions of probability
/// or velocity deviation, which the existing-ID family cannot author. New Event
/// notes retain the existing closed eight-field addition domain. Native identity,
/// current-object authority, overlap/intermediate-state, delivery, frame capacity
/// and readback admission remain the existing request builders' responsibility.
/// Public Clip reference: https://docs.cycling74.com/apiref/lom/clip/ describes
/// markers and note times in beats. Sunny's compiler has already projected those
/// as quarter-note units. No Python ABI, loop-end alias or host qualification is
/// inferred by this pure plan.
[[nodiscard]] sunny::core::Result<std::vector<ManagedClipRevisionStage>>
plan_managed_clip_revision(const ManagedClipProjection& previous,
                           const std::vector<std::string>& previous_note_keys,
                           const ManagedClipProjection& desired,
                           const std::vector<std::string>& desired_note_keys);

} // namespace sunny::infrastructure
