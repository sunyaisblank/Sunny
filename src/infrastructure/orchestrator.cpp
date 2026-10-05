/**
 * @file orchestrator.cpp
 * @brief Operation Orchestrator implementation
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/rhythm/euclidean.hpp>
#include <sunny/core/scale/generation.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <sunny/render/arpeggiator.hpp>
#include <utility>

namespace sunny::infrastructure {

DispatchReport BridgeDelivery::dispatch_clip(const std::vector<BridgeMessage>&,
                                             const std::string&,
                                             const std::optional<OrdinaryClipReceipt>&,
                                             const std::string&) {
    return {0, 1, false, {"This delivery does not implement native Clip authority"}};
}
DispatchReport BridgeDelivery::reconcile_clip(const OrdinaryClipReceipt&, const std::string&) {
    return {0, 1, true, {"This delivery cannot query a retained native Clip attempt"}};
}
namespace {

[[nodiscard]] std::string clip_slot_path(int track_index, int slot_index) {
    return "song/tracks/" + std::to_string(track_index) + "/clip_slots/" +
           std::to_string(slot_index);
}

[[nodiscard]] BridgeMessage delete_clip_message(int track_index, int slot_index) {
    return {BridgeMessageType::CallMethod,
            clip_slot_path(track_index, slot_index),
            {"delete_clip"},
            {}};
}

[[nodiscard]] sunny::core::Result<sunny::core::Beat>
live_beats_to_sunny_beat(double quarter_notes) {
    return sunny::core::Beat::from_float(quarter_notes / 4.0);
}

/// Create a clip of the given length in the slot, then write the notes into it.
[[nodiscard]] std::vector<BridgeMessage> clip_messages(int track_index,
                                                       int slot_index,
                                                       double clip_length_beats,
                                                       std::vector<sunny::core::NoteEvent> events) {
    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = clip_slot_path(track_index, slot_index);
    create_msg.args.push_back(std::to_string(clip_length_beats));
    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = std::move(events);
    return {std::move(create_msg), std::move(notes_msg)};
}

[[nodiscard]] OrchestratorResult rejected(std::string message) {
    return {OperationOutcome::NotAttempted, "", std::move(message), 0, {}};
}

void append_errors(std::vector<std::string>& errors,
                   const std::vector<std::string>& more,
                   std::string_view prefix) {
    for (const auto& error : more)
        errors.push_back(std::string(prefix) + error);
}

} // namespace

std::string_view to_string(OperationOutcome outcome) noexcept {
    switch (outcome) {
    case OperationOutcome::NotAttempted:
        return "not_attempted";
    case OperationOutcome::Applied:
        return "applied";
    case OperationOutcome::NotApplied:
        return "not_applied";
    case OperationOutcome::RolledBack:
        return "rolled_back";
    case OperationOutcome::PartiallyApplied:
        return "partially_applied";
    case OperationOutcome::Indeterminate:
        return "indeterminate";
    }
    return "indeterminate";
}

DispatchReport RecordingDelivery::dispatch(const std::vector<BridgeMessage>& messages) {
    recorded_.insert(recorded_.end(), messages.begin(), messages.end());
    return {messages.size(), 0, false, {}};
}

std::vector<BridgeMessage> RecordingDelivery::drain_messages() {
    return std::exchange(recorded_, {});
}

OrchestratorResult Orchestrator::create_progression_clip(BridgeDelivery& delivery,
                                                         int track_index,
                                                         int slot_index,
                                                         const std::string& root,
                                                         const std::string& scale,
                                                         const std::vector<std::string>& numerals,
                                                         int octave,
                                                         double duration_beats) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return rejected("Track and clip-slot indices must be non-negative");
    }
    if (!std::isfinite(duration_beats) || duration_beats <= 0.0) {
        return rejected("Duration must be a positive finite beat count");
    }
    if (numerals.empty()) {
        return rejected("At least one chord numeral is required");
    }

    // Parse root note
    auto root_result = sunny::core::note_to_pitch_class(root);
    if (!root_result) {
        return rejected("Invalid root note: " + root);
    }
    sunny::core::PitchClass root_pc = *root_result;

    // Get scale intervals
    auto scale_def = sunny::core::find_scale(scale);
    if (!scale_def) {
        return rejected("Unknown scale: " + scale);
    }

    // A numeral that names no degree of this scale is rejected rather than
    // dropped, so the clip always holds exactly the progression requested.
    std::vector<sunny::core::ChordVoicing> chords;
    for (const auto& numeral : numerals) {
        auto chord_result = sunny::core::generate_chord_from_numeral(
            numeral, root_pc, scale_def->get_intervals(), octave);
        if (!chord_result) {
            return rejected("Numeral " + numeral + " is not a chord of scale " + scale);
        }
        chords.push_back(*chord_result);
    }

    // Apply voice leading. Pad or trim target pitch classes to match
    // source cardinality, since successive chords may differ in size.
    for (std::size_t i = 1; i < chords.size(); ++i) {
        std::vector<sunny::core::PitchClass> target_pcs;
        for (auto note : chords[i].notes) {
            target_pcs.push_back(sunny::core::pitch_class(note));
        }

        auto& source = chords[i - 1].notes;
        while (target_pcs.size() < source.size() && !target_pcs.empty()) {
            target_pcs.push_back(target_pcs[target_pcs.size() % chords[i].notes.size()]);
        }
        if (target_pcs.size() > source.size()) {
            target_pcs.resize(source.size());
        }

        auto vl_result = sunny::core::voice_lead_nearest_tone(source, target_pcs, true);
        if (vl_result) {
            chords[i].notes = vl_result->voiced_notes;
        }
    }

    // Convert to note events
    std::vector<sunny::core::NoteEvent> events;
    double beat_per_chord = duration_beats / static_cast<double>(chords.size());
    auto event_duration = live_beats_to_sunny_beat(beat_per_chord * 0.9);
    if (!event_duration) return rejected("Chord duration is not representable as a Beat");

    for (std::size_t i = 0; i < chords.size(); ++i) {
        double start = static_cast<double>(i) * beat_per_chord;
        auto event_start = live_beats_to_sunny_beat(start);
        if (!event_start) return rejected("Chord onset is not representable as a Beat");
        for (auto note : chords[i].notes) {
            sunny::core::NoteEvent event;
            event.pitch = note;
            event.start_time = *event_start;
            event.duration = *event_duration;
            event.velocity = 100;
            events.push_back(event);
        }
    }

    return record_clip_operation(
        delivery,
        {clip_messages(track_index, slot_index, duration_beats, std::move(events)),
         delete_clip_message(track_index, slot_index)},
        "Created progression with " + std::to_string(chords.size()) + " chords");
}

OrchestratorResult Orchestrator::apply_euclidean_rhythm(BridgeDelivery& delivery,
                                                        int track_index,
                                                        int slot_index,
                                                        int pulses,
                                                        int steps,
                                                        sunny::core::MidiNote pitch,
                                                        double step_duration) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return rejected("Track and clip-slot indices must be non-negative");
    }
    if (!std::isfinite(step_duration) || step_duration <= 0.0) {
        return rejected("Step duration must be positive and finite");
    }

    auto pattern_result = sunny::core::euclidean_rhythm(pulses, steps);
    if (!pattern_result) {
        return rejected("Invalid Euclidean parameters");
    }

    auto& pattern = *pattern_result;
    auto event_duration = live_beats_to_sunny_beat(step_duration * 0.8);
    if (!event_duration) return rejected("Step duration is not representable as a Beat");

    // Convert to note events
    std::vector<sunny::core::NoteEvent> events;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i]) {
            sunny::core::NoteEvent event;
            event.pitch = pitch;
            auto event_start = live_beats_to_sunny_beat(static_cast<double>(i) * step_duration);
            if (!event_start) return rejected("Step onset is not representable as a Beat");
            event.start_time = *event_start;
            event.duration = *event_duration;
            event.velocity = 100;
            events.push_back(event);
        }
    }

    return record_clip_operation(delivery,
                                 {clip_messages(track_index,
                                                slot_index,
                                                static_cast<double>(steps) * step_duration,
                                                std::move(events)),
                                  delete_clip_message(track_index, slot_index)},
                                 "Created Euclidean rhythm E(" + std::to_string(pulses) + "," +
                                     std::to_string(steps) + ")");
}

OrchestratorResult Orchestrator::apply_arpeggio(BridgeDelivery& delivery,
                                                int track_index,
                                                int slot_index,
                                                const std::string& root,
                                                const std::string& scale,
                                                const std::vector<std::string>& numerals,
                                                const std::string& direction,
                                                double step_duration) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return rejected("Track and clip-slot indices must be non-negative");
    }
    if (!std::isfinite(step_duration) || step_duration <= 0.0) {
        return rejected("Step duration must be positive and finite");
    }
    if (numerals.empty()) {
        return rejected("At least one chord numeral is required");
    }
    if (direction != "up" && direction != "down" && direction != "updown" &&
        direction != "up_down" && direction != "downup" && direction != "down_up" &&
        direction != "random" && direction != "order") {
        return rejected("Unknown arpeggio direction: " + direction);
    }

    // Parse direction
    sunny::render::ArpDirection arp_dir = sunny::render::ArpDirection::Up;
    if (direction == "down") {
        arp_dir = sunny::render::ArpDirection::Down;
    } else if (direction == "updown" || direction == "up_down") {
        arp_dir = sunny::render::ArpDirection::UpDown;
    } else if (direction == "downup" || direction == "down_up") {
        arp_dir = sunny::render::ArpDirection::DownUp;
    } else if (direction == "random") {
        arp_dir = sunny::render::ArpDirection::Random;
    } else if (direction == "order") {
        arp_dir = sunny::render::ArpDirection::Order;
    }

    auto root_pc = sunny::core::note_to_pitch_class(root);
    if (!root_pc) return rejected("Invalid root note: " + root);
    auto scale_def = sunny::core::find_scale(scale);
    if (!scale_def) return rejected("Unknown scale: " + scale);

    auto step = live_beats_to_sunny_beat(step_duration);
    if (!step) return rejected("Step duration is not representable as a Beat");

    // Each chord is arpeggiated on its own and placed after the previous
    // chord's steps, so the pattern walks the progression chord by chord
    // rather than one sorted pool of every chord tone.
    std::vector<sunny::core::NoteEvent> events;
    std::int64_t steps_so_far = 0;
    for (const auto& numeral : numerals) {
        auto chord = sunny::core::generate_chord_from_numeral(
            numeral, *root_pc, scale_def->get_intervals(), 4);
        if (!chord) return rejected("Numeral " + numeral + " is not a chord of scale " + scale);

        auto chord_events = sunny::render::generate_arpeggio(*chord, arp_dir, *step, 0.8, 1);
        if (!chord_events) return rejected("Arpeggio generation rejected invalid render input");

        auto offset = sunny::core::checked_mul(*step, sunny::core::Beat{steps_so_far, 1});
        if (!offset) return rejected("Arpeggio onset is not representable as a Beat");
        for (auto& event : *chord_events) {
            auto start = sunny::core::checked_add(event.start_time, *offset);
            if (!start) return rejected("Arpeggio onset is not representable as a Beat");
            event.start_time = *start;
            events.push_back(event);
        }
        steps_so_far += static_cast<std::int64_t>(chord_events->size());
    }

    // The clip loops with period steps x step duration; the last note's end
    // falls short of that by the gate.
    const auto note_count = events.size();
    const double clip_length = static_cast<double>(note_count) * step_duration;
    return record_clip_operation(
        delivery,
        {clip_messages(track_index, slot_index, clip_length, std::move(events)),
         delete_clip_message(track_index, slot_index)},
        "Created arpeggio with " + std::to_string(note_count) + " notes");
}

OrchestratorResult Orchestrator::deliver_forward(BridgeDelivery& delivery,
                                                 const HistoryEntry& entry) {
    const auto& slot = entry.inverse.path;
    OrchestratorResult result;
    const auto forward = delivery.dispatch(entry.forward_messages);
    result.commands_sent = forward.sent;
    result.errors = forward.errors;
    if (forward.all_ok()) {
        result.outcome = OperationOutcome::Applied;
        return result;
    }

    if (forward.sent == 0) {
        // Nothing was acknowledged, so there is nothing of Sunny's to revert.
        // Compensating here would delete whatever clip already occupied the
        // slot, which is how a refused create_clip used to destroy user work.
        if (forward.indeterminate) {
            result.outcome = OperationOutcome::Indeterminate;
            result.message = "The bridge lost the response to the first command; " + slot +
                             " may or may not hold a new clip. Inspect it before retrying.";
        } else {
            result.outcome = OperationOutcome::NotApplied;
            result.message = "Live refused the first command; " + slot + " was not changed.";
        }
        return result;
    }

    // The clip Sunny created exists; deleting it reverts every acknowledged
    // message and any partial effect of the failed one (HistoryEntry invariant).
    const auto compensation = delivery.dispatch({entry.inverse});
    append_errors(result.errors, compensation.errors, "compensation: ");
    if (compensation.all_ok()) {
        result.outcome = OperationOutcome::RolledBack;
        result.message = "A later command failed; the clip Sunny created in " + slot +
                         " was deleted, so the set is unchanged.";
    } else if (compensation.indeterminate) {
        result.outcome = OperationOutcome::Indeterminate;
        result.message = "A later command failed and the response to deleting the clip Sunny "
                         "created in " +
                         slot + " was lost. Inspect the slot before retrying.";
    } else {
        result.outcome = OperationOutcome::PartiallyApplied;
        result.message = "A later command failed and the clip Sunny created in " + slot +
                         " could not be deleted; it remains incomplete and is not in the undo "
                         "history.";
    }
    return result;
}

OrchestratorResult Orchestrator::record_clip_operation(BridgeDelivery& delivery,
                                                       HistoryEntry entry,
                                                       std::string applied_message) {
    if (auto recovered = recover_pending_create(delivery)) return *recovered;
    if (!delivery.records_without_execution()) {
        const auto report =
            delivery.dispatch_clip(entry.forward_messages, "create", std::nullopt, "");
        OrchestratorResult result;
        result.commands_sent = report.sent;
        result.errors = report.errors;
        if (report.ordinary_receipt)
            result.operation_id =
                report.ordinary_receipt->intent.at("operation_id").get<std::string>();
        if (!report.all_ok()) {
            result.outcome = report.ordinary_receipt && report.ordinary_receipt->outcome ==
                                                            OrdinaryClipOutcome::Partial
                                 ? OperationOutcome::PartiallyApplied
                             : report.indeterminate ? OperationOutcome::Indeterminate
                                                    : OperationOutcome::NotApplied;
            result.message = "Native Clip transaction was not acknowledged. Retain its original "
                             "attempt for read-only reconciliation.";
            if (report.indeterminate && report.ordinary_receipt) {
                entry.pending = report.ordinary_receipt;
                entry.workspace_namespace = report.workspace_namespace;
                pending_create_ = std::move(entry);
            }
            return result;
        }
        if (!report.ordinary_receipt ||
            report.ordinary_receipt->outcome != OrdinaryClipOutcome::Acknowledged)
            return rejected("Native delivery omitted the typed Clip acknowledgement");
        entry.authority = report.ordinary_receipt;
        entry.workspace_namespace = report.workspace_namespace;
        result.outcome = OperationOutcome::Applied;
        result.message = std::move(applied_message);
        push_undo(std::move(entry));
        redo_stack_.clear();
        return result;
    }
    auto result = deliver_forward(delivery, entry);
    if (!result.success()) return result;

    result.operation_id = generate_operation_id();
    result.message = std::move(applied_message);
    push_undo(std::move(entry));
    redo_stack_.clear();
    return result;
}

OrchestratorResult Orchestrator::undo(BridgeDelivery& delivery) {
    std::lock_guard lock(mutex_);
    if (auto recovered = recover_pending_create(delivery)) return *recovered;

    if (undo_stack_.empty()) {
        return rejected("No Sunny Ableton operation is available to undo");
    }

    auto& entry = undo_stack_.back();
    if (entry.authority) {
        const auto report =
            entry.pending
                ? delivery.reconcile_clip(*entry.pending, entry.workspace_namespace)
                : delivery.dispatch_clip({}, "undo", entry.authority, entry.workspace_namespace);
        OrchestratorResult result;
        result.commands_sent = report.sent;
        result.errors = report.errors;
        if (report.ordinary_receipt)
            result.operation_id =
                report.ordinary_receipt->intent.at("operation_id").get<std::string>();
        if (!report.all_ok()) {
            result.outcome = report.ordinary_receipt && report.ordinary_receipt->outcome ==
                                                            OrdinaryClipOutcome::Partial
                                 ? OperationOutcome::PartiallyApplied
                             : report.indeterminate ? OperationOutcome::Indeterminate
                                                    : OperationOutcome::NotApplied;
            result.message = "Native undo was not acknowledged; uncertain attempts are queried "
                             "using their original token.";
            if (report.ordinary_receipt &&
                (report.indeterminate ||
                 report.ordinary_receipt->outcome == OrdinaryClipOutcome::Partial))
                entry.pending = report.ordinary_receipt;
            return result;
        }
        if (!report.ordinary_receipt) return rejected("Native undo omitted its acknowledgement");
        entry.authority = report.ordinary_receipt;
        entry.pending.reset();
        result.outcome = OperationOutcome::Applied;
        result.message = "Undid the retained native Clip";
        redo_stack_.push_back(std::move(entry));
        undo_stack_.pop_back();
        return result;
    }
    if (!delivery.records_without_execution())
        return rejected("Recorded-only history cannot authorize a native undo");
    const auto report = delivery.dispatch({entry.inverse});
    OrchestratorResult result;
    result.commands_sent = report.sent;
    result.errors = report.errors;
    if (!report.all_ok()) {
        // The entry stays on the undo stack, so a retry reverts this
        // operation rather than the one before it.
        result.outcome =
            report.indeterminate ? OperationOutcome::Indeterminate : OperationOutcome::NotApplied;
        result.message = report.indeterminate
                             ? "The bridge lost the response to the undo; inspect " +
                                   entry.inverse.path + " before retrying."
                             : "Undo was refused; the operation remains in the undo history.";
        return result;
    }

    result.outcome = OperationOutcome::Applied;
    result.message = "Undid the operation in " + entry.inverse.path;
    redo_stack_.push_back(std::move(undo_stack_.back()));
    undo_stack_.pop_back();
    return result;
}

OrchestratorResult Orchestrator::redo(BridgeDelivery& delivery) {
    std::lock_guard lock(mutex_);
    if (auto recovered = recover_pending_create(delivery)) return *recovered;

    if (redo_stack_.empty()) {
        return rejected("No Sunny Ableton operation is available to redo");
    }

    auto& entry = redo_stack_.back();
    if (entry.authority) {
        const auto report =
            entry.pending
                ? delivery.reconcile_clip(*entry.pending, entry.workspace_namespace)
                : delivery.dispatch_clip({}, "redo", entry.authority, entry.workspace_namespace);
        OrchestratorResult result;
        result.commands_sent = report.sent;
        result.errors = report.errors;
        if (report.ordinary_receipt)
            result.operation_id =
                report.ordinary_receipt->intent.at("operation_id").get<std::string>();
        if (!report.all_ok()) {
            result.outcome = report.ordinary_receipt && report.ordinary_receipt->outcome ==
                                                            OrdinaryClipOutcome::Partial
                                 ? OperationOutcome::PartiallyApplied
                             : report.indeterminate ? OperationOutcome::Indeterminate
                                                    : OperationOutcome::NotApplied;
            result.message = "Native redo was not acknowledged; uncertain attempts are queried "
                             "using their original token.";
            if (report.ordinary_receipt &&
                (report.indeterminate ||
                 report.ordinary_receipt->outcome == OrdinaryClipOutcome::Partial))
                entry.pending = report.ordinary_receipt;
            return result;
        }
        if (!report.ordinary_receipt) return rejected("Native redo omitted its acknowledgement");
        entry.authority = report.ordinary_receipt;
        entry.pending.reset();
        result.outcome = OperationOutcome::Applied;
        result.message = "Redid the retained native Clip";
        push_undo(std::move(entry));
        redo_stack_.pop_back();
        return result;
    }
    if (!delivery.records_without_execution())
        return rejected("Recorded-only history cannot authorize a native redo");
    // A failed redo leaves the entry on the redo stack; the forward delivery
    // compensated for, or reported, whatever it partially applied.
    auto result = deliver_forward(delivery, redo_stack_.back());
    if (!result.success()) return result;

    result.message = "Redid the operation in " + redo_stack_.back().inverse.path;
    push_undo(std::move(redo_stack_.back()));
    redo_stack_.pop_back();
    return result;
}

bool Orchestrator::can_undo() const {
    std::lock_guard lock(mutex_);
    return !undo_stack_.empty();
}

bool Orchestrator::can_redo() const {
    std::lock_guard lock(mutex_);
    return !redo_stack_.empty();
}

void Orchestrator::clear_history() {
    std::lock_guard lock(mutex_);
    undo_stack_.clear();
    redo_stack_.clear();
    pending_create_.reset();
}

void Orchestrator::set_max_undo_levels(std::size_t levels) {
    std::lock_guard lock(mutex_);
    max_undo_levels_ = levels;
    while (undo_stack_.size() > max_undo_levels_)
        undo_stack_.pop_front();
}

std::string Orchestrator::generate_operation_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "op_" << std::hex << ms << "_" << next_operation_id_++;
    return oss.str();
}

void Orchestrator::push_undo(HistoryEntry entry) {
    undo_stack_.push_back(std::move(entry));
    while (undo_stack_.size() > max_undo_levels_) {
        undo_stack_.pop_front();
    }
}

std::optional<OrchestratorResult> Orchestrator::recover_pending_create(BridgeDelivery& delivery) {
    if (!pending_create_) return std::nullopt;
    auto& entry = *pending_create_;
    const auto report = delivery.reconcile_clip(*entry.pending, entry.workspace_namespace);
    OrchestratorResult result;
    result.operation_id = entry.pending->intent.at("operation_id").get<std::string>();
    result.commands_sent = report.sent;
    result.errors = report.errors;
    result.outcome = OperationOutcome::Indeterminate;
    result.message =
        "Queried the earlier Clip attempt; this call did not dispatch a new musical mutation.";
    if (report.ordinary_receipt) entry.pending = report.ordinary_receipt;
    if (report.all_ok() && report.ordinary_receipt) {
        entry.authority = report.ordinary_receipt;
        entry.pending.reset();
        push_undo(std::move(entry));
        redo_stack_.clear();
        pending_create_.reset();
        result.outcome = OperationOutcome::Applied;
    } else if (report.ordinary_receipt &&
               (report.ordinary_receipt->outcome == OrdinaryClipOutcome::Declined ||
                report.ordinary_receipt->outcome == OrdinaryClipOutcome::Partial)) {
        result.outcome = report.ordinary_receipt->outcome == OrdinaryClipOutcome::Partial
                             ? OperationOutcome::PartiallyApplied
                             : OperationOutcome::NotApplied;
        pending_create_.reset();
    }
    return result;
}

} // namespace sunny::infrastructure
