/**
 * @file INOR001A.cpp
 * @brief Operation Orchestrator implementation
 *
 * Component: INOR001A
 */

#include "INOR001A.h"

#include "Pitch/PTPC001A.h"
#include "Scale/SCGN001A.h"
#include "Harmony/HRRN001A.h"
#include "VoiceLeading/VLNT001A.h"
#include "Rhythm/RHEU001A.h"
#include "Arpeggio/RDAP001A.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Sunny::Infrastructure {

Orchestrator::Orchestrator() = default;

OrchestratorResult Orchestrator::create_progression_clip(
    int track_index,
    int slot_index,
    const std::string& root,
    const std::string& scale,
    const std::vector<std::string>& numerals,
    int octave,
    double duration_beats
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Parse root note
    auto root_result = Core::note_to_pitch_class(root);
    if (!root_result) {
        return {false, "", "Invalid root note: " + root};
    }
    Core::PitchClass root_pc = *root_result;

    // Get scale intervals
    auto scale_def = Core::find_scale(scale);
    if (!scale_def) {
        return {false, "", "Unknown scale: " + scale};
    }

    // Generate chords
    std::vector<Core::ChordVoicing> chords;
    for (const auto& numeral : numerals) {
        auto chord_result = Core::generate_chord_from_numeral(
            numeral, root_pc, scale_def->intervals, octave
        );
        if (chord_result) {
            chords.push_back(*chord_result);
        }
    }

    if (chords.empty()) {
        return {false, "", "No valid chords generated"};
    }

    // Apply voice leading
    for (std::size_t i = 1; i < chords.size(); ++i) {
        std::vector<Core::PitchClass> target_pcs;
        for (auto note : chords[i].notes) {
            target_pcs.push_back(Core::pitch_class(note));
        }

        auto vl_result = Core::voice_lead_nearest_tone(
            chords[i - 1].notes, target_pcs, true
        );
        if (vl_result) {
            chords[i].notes = vl_result->voiced_notes;
        }
    }

    // Convert to note events
    std::vector<Core::NoteEvent> events;
    double beat_per_chord = duration_beats / static_cast<double>(chords.size());

    for (std::size_t i = 0; i < chords.size(); ++i) {
        double start = static_cast<double>(i) * beat_per_chord;
        for (auto note : chords[i].notes) {
            Core::NoteEvent event;
            event.pitch = note;
            event.start_time = Core::Beat::from_float(start);
            event.duration = Core::Beat::from_float(beat_per_chord * 0.9);
            event.velocity = 100;
            events.push_back(event);
        }
    }

    // Queue messages
    std::string op_id = generate_operation_id();

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "tracks/" + std::to_string(track_index) +
                      "/clip_slots/" + std::to_string(slot_index);
    create_msg.args.push_back(std::to_string(duration_beats));
    queue_message(create_msg);

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = events;
    queue_message(notes_msg);

    // Create operation for undo
    Operation op;
    op.id = op_id;
    op.description = "Create progression " + root + " " + scale;
    op.execute = [this, create_msg, notes_msg]() {
        // Re-queue messages
    };
    op.undo = [this, track_index, slot_index]() {
        BridgeMessage delete_msg;
        delete_msg.type = BridgeMessageType::CallMethod;
        delete_msg.path = "tracks/" + std::to_string(track_index) +
                          "/clip_slots/" + std::to_string(slot_index);
        delete_msg.args.push_back("delete_clip");
        queue_message(delete_msg);
    };
    push_operation(std::move(op));

    return {true, op_id, "Created progression with " +
            std::to_string(chords.size()) + " chords"};
}

OrchestratorResult Orchestrator::apply_euclidean_rhythm(
    int track_index,
    int slot_index,
    int pulses,
    int steps,
    Core::MidiNote pitch,
    double step_duration
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto pattern_result = Core::euclidean_rhythm(pulses, steps);
    if (!pattern_result) {
        return {false, "", "Invalid Euclidean parameters"};
    }

    auto& pattern = *pattern_result;

    // Convert to note events
    std::vector<Core::NoteEvent> events;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i]) {
            Core::NoteEvent event;
            event.pitch = pitch;
            event.start_time = Core::Beat::from_float(
                static_cast<double>(i) * step_duration
            );
            event.duration = Core::Beat::from_float(step_duration * 0.8);
            event.velocity = 100;
            events.push_back(event);
        }
    }

    std::string op_id = generate_operation_id();

    double total_duration = static_cast<double>(steps) * step_duration;

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "tracks/" + std::to_string(track_index) +
                      "/clip_slots/" + std::to_string(slot_index);
    create_msg.args.push_back(std::to_string(total_duration));
    queue_message(create_msg);

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = events;
    queue_message(notes_msg);

    return {true, op_id, "Created Euclidean rhythm E(" +
            std::to_string(pulses) + "," + std::to_string(steps) + ")"};
}

OrchestratorResult Orchestrator::apply_arpeggio(
    int track_index,
    int slot_index,
    const std::vector<std::string>& numerals,
    const std::string& direction,
    double step_duration
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Parse direction
    Render::ArpDirection arp_dir = Render::ArpDirection::Up;
    if (direction == "down") {
        arp_dir = Render::ArpDirection::Down;
    } else if (direction == "updown" || direction == "up_down") {
        arp_dir = Render::ArpDirection::UpDown;
    } else if (direction == "downup" || direction == "down_up") {
        arp_dir = Render::ArpDirection::DownUp;
    } else if (direction == "random") {
        arp_dir = Render::ArpDirection::Random;
    } else if (direction == "order") {
        arp_dir = Render::ArpDirection::Order;
    }

    // Build a chord from each numeral (using C major as default context)
    // The orchestrator's create_progression_clip accepts root/scale;
    // arpeggio uses the same approach internally
    auto scale_def = Core::find_scale("major");
    if (!scale_def) {
        return {false, "", "Scale lookup failed"};
    }

    // Collect all notes from all chords into a single voicing
    Core::ChordVoicing combined;
    for (const auto& numeral : numerals) {
        auto chord_result = Core::generate_chord_from_numeral(
            numeral, 0, scale_def->intervals, 4
        );
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
    auto beat_dur = Core::Beat::from_float(step_duration);
    auto events = Render::generate_arpeggio(
        combined, arp_dir, beat_dur, 0.8, 1
    );

    if (events.empty()) {
        return {false, "", "Arpeggio generated no events"};
    }

    std::string op_id = generate_operation_id();

    // Calculate total duration
    double total_duration = 0.0;
    for (const auto& ev : events) {
        double end = ev.start_time.to_float() + ev.duration.to_float();
        if (end > total_duration) {
            total_duration = end;
        }
    }

    BridgeMessage create_msg;
    create_msg.type = BridgeMessageType::CreateClip;
    create_msg.path = "tracks/" + std::to_string(track_index) +
                      "/clip_slots/" + std::to_string(slot_index);
    create_msg.args.push_back(std::to_string(total_duration));
    queue_message(create_msg);

    BridgeMessage notes_msg;
    notes_msg.type = BridgeMessageType::AddNotes;
    notes_msg.path = create_msg.path + "/clip";
    notes_msg.notes = events;
    queue_message(notes_msg);

    // Undo operation
    Operation op;
    op.id = op_id;
    op.description = "Apply arpeggio " + direction;
    op.execute = [this, create_msg, notes_msg]() {
        queue_message(create_msg);
        queue_message(notes_msg);
    };
    op.undo = [this, track_index, slot_index]() {
        BridgeMessage delete_msg;
        delete_msg.type = BridgeMessageType::CallMethod;
        delete_msg.path = "tracks/" + std::to_string(track_index) +
                          "/clip_slots/" + std::to_string(slot_index);
        delete_msg.args.push_back("delete_clip");
        queue_message(delete_msg);
    };
    push_operation(std::move(op));

    return {true, op_id, "Created arpeggio with " +
            std::to_string(events.size()) + " notes"};
}

bool Orchestrator::undo() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (undo_stack_.empty()) {
        return false;
    }

    auto op = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    if (op.undo) {
        op.undo();
    }

    redo_stack_.push_back(std::move(op));
    return true;
}

bool Orchestrator::redo() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (redo_stack_.empty()) {
        return false;
    }

    auto op = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    if (op.execute) {
        op.execute();
    }

    undo_stack_.push_back(std::move(op));
    return true;
}

bool Orchestrator::can_undo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !undo_stack_.empty();
}

bool Orchestrator::can_redo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !redo_stack_.empty();
}

void Orchestrator::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    undo_stack_.clear();
    redo_stack_.clear();
}

std::vector<BridgeMessage> Orchestrator::drain_messages() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto messages = std::move(pending_messages_);
    pending_messages_.clear();
    return messages;
}

std::size_t Orchestrator::pending_message_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_messages_.size();
}

void Orchestrator::set_max_undo_levels(std::size_t levels) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_undo_levels_ = levels;
}

std::string Orchestrator::generate_operation_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    std::ostringstream oss;
    oss << "op_" << std::hex << ms;
    return oss.str();
}

void Orchestrator::push_operation(Operation op) {
    undo_stack_.push_back(std::move(op));
    redo_stack_.clear();

    while (undo_stack_.size() > max_undo_levels_) {
        undo_stack_.pop_front();
    }
}

void Orchestrator::queue_message(BridgeMessage msg) {
    pending_messages_.push_back(std::move(msg));
}

}  // namespace Sunny::Infrastructure
