/**
 * @file SIMT001A.cpp
 * @brief Score IR mutations — implementation
 *
 * Component: SIMT001A
 * Domain: SI (Score IR) | Category: MT (Mutation)
 */

#include "SIMT001A.h"
#include "SIVD001A.h"

#include <algorithm>

namespace Sunny::Core {

// =============================================================================
// Internal helpers
// =============================================================================

namespace {

/// Find a Part by id; returns nullptr if not found
Part* find_part(Score& score, PartId id) {
    for (auto& part : score.parts) {
        if (part.id == id) return &part;
    }
    return nullptr;
}

/// Find an Event by id across all parts; returns event pointer and context
struct EventLocation {
    Part* part = nullptr;
    Measure* measure = nullptr;
    Voice* voice = nullptr;
    Event* event = nullptr;
};

EventLocation find_event(Score& score, EventId id) {
    for (auto& part : score.parts) {
        for (auto& measure : part.measures) {
            for (auto& voice : measure.voices) {
                for (auto& event : voice.events) {
                    if (event.id == id) {
                        return {&part, &measure, &voice, &event};
                    }
                }
            }
        }
    }
    return {};
}

/// Increment version counter
void bump_version(Score& score) {
    ++score.version;
}

/// Global event id counter (simple monotonic)
std::uint64_t next_event_id = 1000000;

EventId allocate_event_id() {
    return EventId{next_event_id++};
}

/// Create empty measure with a single voice containing a whole-measure rest
Measure make_empty_measure(
    std::uint32_t bar_number,
    const TimeSignature& ts
) {
    RestEvent rest{ts.measure_duration(), true};
    Event event{allocate_event_id(), Beat::zero(), rest};

    Voice voice{0, {event}, {}};
    return Measure{bar_number, {voice}, std::nullopt, std::nullopt};
}

}  // anonymous namespace

// =============================================================================
// Event-Level Mutations
// =============================================================================

Result<MutationResult> insert_note(
    Score& score,
    PartId part_id,
    std::uint32_t bar,
    std::uint8_t voice_index,
    Beat offset,
    Note note,
    Beat duration
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    if (bar < 1 || bar > part->measures.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto& measure = part->measures[bar - 1];

    // Find or create the voice
    Voice* target_voice = nullptr;
    for (auto& voice : measure.voices) {
        if (voice.voice_index == voice_index) {
            target_voice = &voice;
            break;
        }
    }
    if (!target_voice) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    NoteGroup ng;
    ng.notes.push_back(std::move(note));
    ng.duration = duration;

    Event event{allocate_event_id(), offset, std::move(ng)};

    // Insert in offset order
    auto it = std::lower_bound(
        target_voice->events.begin(),
        target_voice->events.end(),
        offset,
        [](const Event& e, const Beat& off) { return e.offset < off; }
    );
    target_voice->events.insert(it, std::move(event));

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> delete_event(
    Score& score,
    EventId event_id
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Beat dur = loc.event->duration();
    if (dur > Beat::zero()) {
        // Replace with rest of equal duration
        loc.event->payload = RestEvent{dur, true};
    } else {
        // Zero-duration event (direction/chord symbol) — remove entirely
        auto& events = loc.voice->events;
        events.erase(
            std::remove_if(events.begin(), events.end(),
                [event_id](const Event& e) { return e.id == event_id; }),
            events.end()
        );
    }

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> modify_pitch(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    SpelledPitch new_pitch
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    ng->notes[note_index].pitch = new_pitch;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> modify_duration(
    Score& score,
    EventId event_id,
    Beat new_duration
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto* ng = std::get_if<NoteGroup>(&loc.event->payload)) {
        ng->duration = new_duration;
    } else if (auto* r = std::get_if<RestEvent>(&loc.event->payload)) {
        r->duration = new_duration;
    } else {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> modify_velocity(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    VelocityValue new_velocity
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    ng->notes[note_index].velocity = new_velocity;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> set_articulation(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    std::optional<ArticulationType> articulation
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    ng->notes[note_index].articulation = articulation;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> set_dynamic(
    Score& score,
    PartId part_id,
    ScoreTime position,
    DynamicLevel level
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    if (position.bar < 1 || position.bar > part->measures.size()) {
        return std::unexpected(ErrorCode::InvalidScoreTime);
    }

    auto& measure = part->measures[position.bar - 1];
    if (measure.voices.empty()) {
        return std::unexpected(ErrorCode::EmptyVoice);
    }

    // Find event at or near the position in voice 0
    auto& voice = measure.voices[0];
    for (auto& event : voice.events) {
        if (event.offset == position.beat) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (ng && !ng->notes.empty()) {
                ng->notes[0].dynamic = level;
                bump_version(score);
                return MutationResult{{}};
            }
        }
    }

    return std::unexpected(ErrorCode::InvalidMutation);
}

Result<MutationResult> insert_hairpin(
    Score& score,
    PartId part_id,
    ScoreTime start,
    ScoreTime end,
    HairpinType type,
    std::optional<DynamicLevel> target
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    part->hairpins.push_back(Hairpin{start, end, type, target});
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> set_tie(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    bool tied
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    ng->notes[note_index].tie_forward = tied;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> transpose_event(
    Score& score,
    EventId event_id,
    DiatonicInterval interval
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng) return std::unexpected(ErrorCode::InvalidMutation);

    for (auto& note : ng->notes) {
        note.pitch = apply_interval(note.pitch, interval);
    }

    bump_version(score);
    return MutationResult{{}};
}

// =============================================================================
// Measure-Level Mutations
// =============================================================================

Result<MutationResult> insert_measures(
    Score& score,
    std::uint32_t after_bar,
    std::uint32_t count
) {
    if (after_bar > score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Determine time signature for new measures
    TimeSignature ts{{4}, 4};  // Default 4/4
    for (const auto& entry : score.time_map) {
        if (entry.bar <= after_bar) {
            ts = entry.time_signature;
        }
    }

    // Insert empty measures into each part
    for (auto& part : score.parts) {
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t new_bar = after_bar + i + 1;
            auto empty = make_empty_measure(new_bar, ts);
            auto it = part.measures.begin() +
                static_cast<std::ptrdiff_t>(after_bar + i);
            part.measures.insert(it, std::move(empty));
        }
        // Renumber subsequent measures
        for (std::size_t m = after_bar + count; m < part.measures.size(); ++m) {
            part.measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
        }
    }

    // Update global map positions
    for (auto& entry : score.tempo_map) {
        if (entry.position.bar > after_bar) {
            entry.position.bar += count;
        }
    }
    for (auto& entry : score.key_map) {
        if (entry.position.bar > after_bar) {
            entry.position.bar += count;
        }
    }
    for (auto& entry : score.time_map) {
        if (entry.bar > after_bar) {
            entry.bar += count;
        }
    }

    score.metadata.total_bars += count;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> delete_measures(
    Score& score,
    std::uint32_t bar,
    std::uint32_t count
) {
    if (bar < 1 || bar + count - 1 > score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (count >= score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::uint32_t first = bar - 1;  // 0-indexed

    for (auto& part : score.parts) {
        part.measures.erase(
            part.measures.begin() + first,
            part.measures.begin() + first + count
        );
        // Renumber
        for (std::size_t m = first; m < part.measures.size(); ++m) {
            part.measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
        }
    }

    // Update global maps: remove entries in deleted range, shift later entries
    auto remove_tempo = std::remove_if(
        score.tempo_map.begin(), score.tempo_map.end(),
        [bar, count](const TempoEvent& e) {
            return e.position.bar >= bar && e.position.bar < bar + count;
        }
    );
    score.tempo_map.erase(remove_tempo, score.tempo_map.end());
    for (auto& entry : score.tempo_map) {
        if (entry.position.bar >= bar + count) {
            entry.position.bar -= count;
        }
    }

    auto remove_key = std::remove_if(
        score.key_map.begin(), score.key_map.end(),
        [bar, count](const KeySignatureEntry& e) {
            return e.position.bar >= bar && e.position.bar < bar + count;
        }
    );
    score.key_map.erase(remove_key, score.key_map.end());
    for (auto& entry : score.key_map) {
        if (entry.position.bar >= bar + count) {
            entry.position.bar -= count;
        }
    }

    auto remove_time = std::remove_if(
        score.time_map.begin(), score.time_map.end(),
        [bar, count](const TimeSignatureEntry& e) {
            return e.bar >= bar && e.bar < bar + count;
        }
    );
    score.time_map.erase(remove_time, score.time_map.end());
    for (auto& entry : score.time_map) {
        if (entry.bar >= bar + count) {
            entry.bar -= count;
        }
    }

    score.metadata.total_bars -= count;
    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> set_time_signature(
    Score& score,
    std::uint32_t bar,
    TimeSignature time_sig
) {
    if (bar < 1 || bar > score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Find existing entry for this bar or insert new one
    bool found = false;
    for (auto& entry : score.time_map) {
        if (entry.bar == bar) {
            entry.time_signature = std::move(time_sig);
            found = true;
            break;
        }
    }
    if (!found) {
        TimeSignatureEntry entry{bar, std::move(time_sig)};
        auto it = std::lower_bound(
            score.time_map.begin(), score.time_map.end(), bar,
            [](const TimeSignatureEntry& e, std::uint32_t b) {
                return e.bar < b;
            }
        );
        score.time_map.insert(it, std::move(entry));
    }

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> set_key_signature(
    Score& score,
    ScoreTime position,
    KeySignature key
) {
    // Find existing entry at this position or insert new one
    bool found = false;
    for (auto& entry : score.key_map) {
        if (entry.position == position) {
            entry.key = key;
            found = true;
            break;
        }
    }
    if (!found) {
        KeySignatureEntry entry{position, key};
        auto it = std::lower_bound(
            score.key_map.begin(), score.key_map.end(), position,
            [](const KeySignatureEntry& e, const ScoreTime& p) {
                return e.position < p;
            }
        );
        score.key_map.insert(it, std::move(entry));
    }

    bump_version(score);
    return MutationResult{{}};
}

// =============================================================================
// Part-Level Mutations
// =============================================================================

Result<MutationResult> add_part(
    Score& score,
    PartDefinition definition,
    std::size_t position_in_order
) {
    // Determine time signature for empty measures
    TimeSignature ts{{4}, 4};
    if (!score.time_map.empty()) {
        ts = score.time_map[0].time_signature;
    }

    Part new_part;
    new_part.id = PartId{next_event_id++};
    new_part.definition = std::move(definition);

    // Create empty measures
    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        // Find applicable time signature
        for (const auto& entry : score.time_map) {
            if (entry.bar <= bar) ts = entry.time_signature;
            else break;
        }
        new_part.measures.push_back(make_empty_measure(bar, ts));
    }

    if (position_in_order >= score.parts.size()) {
        score.parts.push_back(std::move(new_part));
    } else {
        score.parts.insert(
            score.parts.begin() +
                static_cast<std::ptrdiff_t>(position_in_order),
            std::move(new_part)
        );
    }

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> remove_part(
    Score& score,
    PartId part_id
) {
    if (score.parts.size() <= 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto it = std::find_if(
        score.parts.begin(), score.parts.end(),
        [part_id](const Part& p) { return p.id == part_id; }
    );
    if (it == score.parts.end()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    score.parts.erase(it);

    // Remove orchestration annotations for this part
    auto oa_end = std::remove_if(
        score.orchestration_annotations.begin(),
        score.orchestration_annotations.end(),
        [part_id](const OrchestrationAnnotation& a) {
            return a.part_id == part_id;
        }
    );
    score.orchestration_annotations.erase(
        oa_end, score.orchestration_annotations.end()
    );

    bump_version(score);
    return MutationResult{{}};
}

// =============================================================================
// Region-Level Mutations
// =============================================================================

Result<MutationResult> transpose_region(
    Score& score,
    const ScoreRegion& region,
    DiatonicInterval interval
) {
    for (auto& part : score.parts) {
        // Check if this part is in the region
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) { found = true; break; }
            }
            if (!found) continue;
        }

        for (auto& measure : part.measures) {
            ScoreTime measure_start{measure.bar_number, Beat::zero()};
            if (measure_start >= region.end) break;

            for (auto& voice : measure.voices) {
                for (auto& event : voice.events) {
                    ScoreTime event_time{measure.bar_number, event.offset};
                    if (event_time < region.start || event_time >= region.end) {
                        continue;
                    }

                    auto* ng = std::get_if<NoteGroup>(&event.payload);
                    if (!ng) continue;

                    for (auto& note : ng->notes) {
                        note.pitch = apply_interval(note.pitch, interval);
                    }
                }
            }
        }
    }

    bump_version(score);
    return MutationResult{{}};
}

Result<MutationResult> delete_region(
    Score& score,
    const ScoreRegion& region
) {
    for (auto& part : score.parts) {
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) { found = true; break; }
            }
            if (!found) continue;
        }

        for (auto& measure : part.measures) {
            ScoreTime measure_start{measure.bar_number, Beat::zero()};
            if (measure_start >= region.end) break;

            for (auto& voice : measure.voices) {
                for (auto& event : voice.events) {
                    ScoreTime event_time{measure.bar_number, event.offset};
                    if (event_time < region.start || event_time >= region.end) {
                        continue;
                    }

                    Beat dur = event.duration();
                    if (dur > Beat::zero()) {
                        event.payload = RestEvent{dur, true};
                    }
                }
            }
        }
    }

    bump_version(score);
    return MutationResult{{}};
}

// =============================================================================
// Undo/Redo
// =============================================================================

VoidResult undo(Score& score, UndoStack& stack) {
    if (!stack.can_undo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto entry = std::move(stack.undo_entries.back());
    stack.undo_entries.pop_back();

    auto result = entry.inverse();
    if (!result) return result;

    bump_version(score);

    // The inverse of the inverse would be the original operation;
    // for now we push a placeholder — full redo requires capturing
    // the forward operation alongside the inverse.
    stack.redo_entries.push_back(std::move(entry));

    return {};
}

VoidResult redo(Score& score, UndoStack& stack) {
    if (!stack.can_redo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto entry = std::move(stack.redo_entries.back());
    stack.redo_entries.pop_back();

    auto result = entry.inverse();
    if (!result) return result;

    bump_version(score);
    stack.undo_entries.push_back(std::move(entry));

    return {};
}

}  // namespace Sunny::Core
