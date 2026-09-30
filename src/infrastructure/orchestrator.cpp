/**
 * @file orchestrator.cpp
 * @brief Operation Orchestrator implementation
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/rhythm/euclidean.hpp>
#include <sunny/core/scale/generation.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <sunny/render/arpeggiator.hpp>

namespace sunny::infrastructure {
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

} // namespace

OrchestratorResult Orchestrator::create_progression_clip(int track_index,
                                                         int slot_index,
                                                         const std::string& root,
                                                         const std::string& scale,
                                                         const std::vector<std::string>& numerals,
                                                         int octave,
                                                         double duration_beats) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return {false, "", "Track and clip-slot indices must be non-negative"};
    }
    if (!std::isfinite(duration_beats) || duration_beats <= 0.0) {
        return {false, "", "Duration must be a positive finite beat count"};
    }
    if (numerals.empty()) {
        return {false, "", "At least one chord numeral is required"};
    }

    // Parse root note
    auto root_result = sunny::core::note_to_pitch_class(root);
    if (!root_result) {
        return {false, "", "Invalid root note: " + root};
    }
    sunny::core::PitchClass root_pc = *root_result;

    // Get scale intervals
    auto scale_def = sunny::core::find_scale(scale);
    if (!scale_def) {
        return {false, "", "Unknown scale: " + scale};
    }

    // A numeral that names no degree of this scale is rejected rather than
    // dropped, so the clip always holds exactly the progression requested.
    std::vector<sunny::core::ChordVoicing> chords;
    for (const auto& numeral : numerals) {
        auto chord_result = sunny::core::generate_chord_from_numeral(
            numeral, root_pc, scale_def->get_intervals(), octave);
        if (!chord_result) {
            return {false, "", "Numeral " + numeral + " is not a chord of scale " + scale};
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
    if (!event_duration) return {false, "", "Chord duration is not representable as a Beat"};

    for (std::size_t i = 0; i < chords.size(); ++i) {
        double start = static_cast<double>(i) * beat_per_chord;
        auto event_start = live_beats_to_sunny_beat(start);
        if (!event_start) return {false, "", "Chord onset is not representable as a Beat"};
        for (auto note : chords[i].notes) {
            sunny::core::NoteEvent event;
            event.pitch = note;
            event.start_time = *event_start;
            event.duration = *event_duration;
            event.velocity = 100;
            events.push_back(event);
        }
    }

    // Queue messages
    std::string op_id = generate_operation_id();

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = clip_slot_path(track_index, slot_index);
    create_msg.args.push_back(std::to_string(duration_beats));
    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = std::move(events);
    HistoryEntry history{{std::move(create_msg), std::move(notes_msg)},
                         {delete_clip_message(track_index, slot_index)}};
    queue_messages(history.forward_messages);
    push_history(std::move(history));

    return {true, op_id, "Created progression with " + std::to_string(chords.size()) + " chords"};
}

OrchestratorResult Orchestrator::apply_euclidean_rhythm(int track_index,
                                                        int slot_index,
                                                        int pulses,
                                                        int steps,
                                                        sunny::core::MidiNote pitch,
                                                        double step_duration) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return {false, "", "Track and clip-slot indices must be non-negative"};
    }
    if (!std::isfinite(step_duration) || step_duration <= 0.0) {
        return {false, "", "Step duration must be positive and finite"};
    }

    auto pattern_result = sunny::core::euclidean_rhythm(pulses, steps);
    if (!pattern_result) {
        return {false, "", "Invalid Euclidean parameters"};
    }

    auto& pattern = *pattern_result;
    auto event_duration = live_beats_to_sunny_beat(step_duration * 0.8);
    if (!event_duration) return {false, "", "Step duration is not representable as a Beat"};

    // Convert to note events
    std::vector<sunny::core::NoteEvent> events;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i]) {
            sunny::core::NoteEvent event;
            event.pitch = pitch;
            auto event_start = live_beats_to_sunny_beat(static_cast<double>(i) * step_duration);
            if (!event_start) return {false, "", "Step onset is not representable as a Beat"};
            event.start_time = *event_start;
            event.duration = *event_duration;
            event.velocity = 100;
            events.push_back(event);
        }
    }

    std::string op_id = generate_operation_id();

    double total_duration = static_cast<double>(steps) * step_duration;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = clip_slot_path(track_index, slot_index);
    create_msg.args.push_back(std::to_string(total_duration));
    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = std::move(events);
    HistoryEntry history{{std::move(create_msg), std::move(notes_msg)},
                         {delete_clip_message(track_index, slot_index)}};
    queue_messages(history.forward_messages);
    push_history(std::move(history));

    return {true,
            op_id,
            "Created Euclidean rhythm E(" + std::to_string(pulses) + "," + std::to_string(steps) +
                ")"};
}

OrchestratorResult Orchestrator::apply_arpeggio(int track_index,
                                                int slot_index,
                                                const std::vector<std::string>& numerals,
                                                const std::string& direction,
                                                double step_duration) {
    std::lock_guard lock(mutex_);

    if (track_index < 0 || slot_index < 0) {
        return {false, "", "Track and clip-slot indices must be non-negative"};
    }
    if (!std::isfinite(step_duration) || step_duration <= 0.0) {
        return {false, "", "Step duration must be positive and finite"};
    }
    if (numerals.empty()) {
        return {false, "", "At least one chord numeral is required"};
    }
    if (direction != "up" && direction != "down" && direction != "updown" &&
        direction != "up_down" && direction != "downup" && direction != "down_up" &&
        direction != "random" && direction != "order") {
        return {false, "", "Unknown arpeggio direction: " + direction};
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

    // Build a chord from each numeral (using C major as default context)
    // The orchestrator's create_progression_clip accepts root/scale;
    // arpeggio uses the same approach internally
    auto scale_def = sunny::core::find_scale("major");
    if (!scale_def) {
        return {false, "", "Scale lookup failed"};
    }

    // Collect all notes from all chords into a single voicing
    sunny::core::ChordVoicing combined;
    for (const auto& numeral : numerals) {
        auto chord_result =
            sunny::core::generate_chord_from_numeral(numeral, 0, scale_def->get_intervals(), 4);
        if (chord_result) {
            for (auto note : chord_result->notes) {
                combined.notes.push_back(note);
            }
        }
    }

    if (combined.notes.empty()) {
        return {false, "", "No valid chords for arpeggio"};
    }

    // Generate arpeggio pattern
    auto beat_dur = live_beats_to_sunny_beat(step_duration);
    if (!beat_dur) return {false, "", "Step duration is not representable as a Beat"};
    auto events = sunny::render::generate_arpeggio(combined, arp_dir, *beat_dur, 0.8, 1);

    if (!events) return {false, "", "Arpeggio generation rejected invalid render input"};

    std::string op_id = generate_operation_id();

    // Calculate total duration
    double total_duration = 0.0;
    for (const auto& ev : *events) {
        double end = 4.0 * (ev.start_time.to_float() + ev.duration.to_float());
        if (end > total_duration) {
            total_duration = end;
        }
    }

    const auto note_count = events->size();

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = clip_slot_path(track_index, slot_index);
    create_msg.args.push_back(std::to_string(total_duration));
    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = std::move(*events);
    HistoryEntry history{{std::move(create_msg), std::move(notes_msg)},
                         {delete_clip_message(track_index, slot_index)}};
    queue_messages(history.forward_messages);
    push_history(std::move(history));

    return {true, op_id, "Created arpeggio with " + std::to_string(note_count) + " notes"};
}

bool Orchestrator::undo() {
    std::lock_guard lock(mutex_);

    if (undo_stack_.empty()) {
        return false;
    }

    auto op = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    queue_messages(op.inverse_messages);

    redo_stack_.push_back(std::move(op));
    return true;
}

bool Orchestrator::redo() {
    std::lock_guard lock(mutex_);

    if (redo_stack_.empty()) {
        return false;
    }

    auto op = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    queue_messages(op.forward_messages);

    undo_stack_.push_back(std::move(op));
    return true;
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
}

std::vector<BridgeMessage> Orchestrator::drain_messages() {
    std::lock_guard lock(mutex_);
    auto messages = std::move(pending_messages_);
    pending_messages_.clear();
    return messages;
}

std::size_t Orchestrator::pending_message_count() const {
    std::lock_guard lock(mutex_);
    return pending_messages_.size();
}

void Orchestrator::set_max_undo_levels(std::size_t levels) {
    std::lock_guard lock(mutex_);
    max_undo_levels_ = levels;
}

std::string Orchestrator::generate_operation_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "op_" << std::hex << ms << "_" << next_operation_id_++;
    return oss.str();
}

void Orchestrator::push_history(HistoryEntry entry) {
    undo_stack_.push_back(std::move(entry));
    redo_stack_.clear();

    while (undo_stack_.size() > max_undo_levels_) {
        undo_stack_.pop_front();
    }
}

void Orchestrator::queue_messages(std::span<const BridgeMessage> messages) {
    pending_messages_.insert(pending_messages_.end(), messages.begin(), messages.end());
}

} // namespace sunny::infrastructure
