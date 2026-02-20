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

/// Get the time signature in effect at a given bar
const TimeSignatureEntry* find_time_signature(const Score& score, std::uint32_t bar) {
    const TimeSignatureEntry* result = nullptr;
    for (const auto& entry : score.time_map) {
        if (entry.bar <= bar) result = &entry;
        else break;
    }
    return result;
}

/// Iterate events in a region, calling callback for each
template<typename Fn>
void for_each_event_in_region(Score& score, const ScoreRegion& region, Fn&& fn) {
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
                    if (event_time < region.start || event_time >= region.end) continue;
                    fn(part, measure, voice, event);
                }
            }
        }
    }
}

/// Push an undo entry onto the stack (if provided), clearing redo
void push_undo(UndoStack* undo, Score& score, std::string desc,
               std::function<VoidResult()> forward,
               std::function<VoidResult()> inverse) {
    if (!undo) return;
    undo->undo_entries.push_back(UndoEntry{
        score.version, std::move(forward), std::move(inverse), std::move(desc)
    });
    undo->redo_entries.clear();
}

/// Mark a region as stale for harmonic re-analysis
void mark_harmonic_stale(Score& score, std::uint32_t bar) {
    score.stale_harmonic_regions.push_back(ScoreRegion{
        ScoreTime{bar, Beat::zero()},
        ScoreTime{bar + 1, Beat::zero()},
        {}
    });
}

/// Mark a region as stale for orchestration re-analysis
void mark_orchestration_stale(Score& score, const ScoreRegion& region) {
    score.stale_orchestration_regions.push_back(region);
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
    Beat duration,
    UndoStack* undo
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
    EventId new_id = event.id;

    // Insert in offset order
    auto it = std::lower_bound(
        target_voice->events.begin(),
        target_voice->events.end(),
        offset,
        [](const Event& e, const Beat& off) { return e.offset < off; }
    );
    target_voice->events.insert(it, std::move(event));

    bump_version(score);

    // Capture the inserted event for forward replay
    auto inserted_payload = target_voice->events.back().payload;
    for (const auto& ev : target_voice->events) {
        if (ev.id == new_id) { inserted_payload = ev.payload; break; }
    }

    push_undo(undo, score, "insert_note",
        [&score, part_id, bar, voice_index, offset, inserted_payload, duration]() -> VoidResult {
            // Forward: re-insert a note with the same payload
            auto* p = find_part(score, part_id);
            if (!p || bar < 1 || bar > p->measures.size())
                return std::unexpected(ErrorCode::InvalidMutation);
            auto& m = p->measures[bar - 1];
            Voice* v = nullptr;
            for (auto& vc : m.voices) {
                if (vc.voice_index == voice_index) { v = &vc; break; }
            }
            if (!v) return std::unexpected(ErrorCode::InvalidMutation);
            Event event{allocate_event_id(), offset, inserted_payload};
            auto it = std::lower_bound(v->events.begin(), v->events.end(), offset,
                [](const Event& e, const Beat& off) { return e.offset < off; });
            v->events.insert(it, std::move(event));
            bump_version(score);
            return {};
        },
        [&score, new_id]() -> VoidResult {
            auto result = delete_event(score, new_id);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    mark_harmonic_stale(score, bar);

    return MutationResult{{}};
}

Result<MutationResult> delete_event(
    Score& score,
    EventId event_id,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Capture state before mutation for undo
    auto old_payload = loc.event->payload;
    auto old_offset = loc.event->offset;
    auto old_bar = loc.measure->bar_number;
    auto old_voice_index = loc.voice->voice_index;
    auto old_part_id = loc.part->id;

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

    push_undo(undo, score, "delete_event",
        [&score, event_id]() -> VoidResult {
            // Forward: re-delete the event
            auto result = delete_event(score, event_id);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, old_payload, old_offset, old_bar, old_voice_index, old_part_id]() -> VoidResult {
            // Inverse: re-insert the event at the original location
            Part* part = find_part(score, old_part_id);
            if (!part) return std::unexpected(ErrorCode::InvalidMutation);
            if (old_bar < 1 || old_bar > part->measures.size())
                return std::unexpected(ErrorCode::InvalidMutation);
            auto& measure = part->measures[old_bar - 1];
            Voice* voice = nullptr;
            for (auto& v : measure.voices) {
                if (v.voice_index == old_voice_index) { voice = &v; break; }
            }
            if (!voice) return std::unexpected(ErrorCode::InvalidMutation);
            Event event{allocate_event_id(), old_offset, old_payload};
            auto it = std::lower_bound(
                voice->events.begin(), voice->events.end(), old_offset,
                [](const Event& e, const Beat& off) { return e.offset < off; }
            );
            voice->events.insert(it, std::move(event));
            bump_version(score);
            return {};
        }
    );

    mark_harmonic_stale(score, old_bar);

    return MutationResult{{}};
}

Result<MutationResult> modify_pitch(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    SpelledPitch new_pitch,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    SpelledPitch old_pitch = ng->notes[note_index].pitch;

    ng->notes[note_index].pitch = new_pitch;
    bump_version(score);

    push_undo(undo, score, "modify_pitch",
        [&score, event_id, note_index, new_pitch]() -> VoidResult {
            auto result = modify_pitch(score, event_id, note_index, new_pitch);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, note_index, old_pitch]() -> VoidResult {
            auto result = modify_pitch(score, event_id, note_index, old_pitch);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    mark_harmonic_stale(score, loc.measure->bar_number);

    return MutationResult{{}};
}

Result<MutationResult> modify_duration(
    Score& score,
    EventId event_id,
    Beat new_duration,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    Beat old_dur = loc.event->duration();

    if (auto* ng = std::get_if<NoteGroup>(&loc.event->payload)) {
        ng->duration = new_duration;
    } else if (auto* r = std::get_if<RestEvent>(&loc.event->payload)) {
        r->duration = new_duration;
    } else {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    bump_version(score);

    push_undo(undo, score, "modify_duration",
        [&score, event_id, new_duration]() -> VoidResult {
            auto result = modify_duration(score, event_id, new_duration);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, old_dur]() -> VoidResult {
            auto result = modify_duration(score, event_id, old_dur);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    mark_harmonic_stale(score, loc.measure->bar_number);

    return MutationResult{{}};
}

Result<MutationResult> modify_velocity(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    VelocityValue new_velocity,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    VelocityValue old_vel = ng->notes[note_index].velocity;

    ng->notes[note_index].velocity = new_velocity;
    bump_version(score);

    push_undo(undo, score, "modify_velocity",
        [&score, event_id, note_index, new_velocity]() -> VoidResult {
            auto result = modify_velocity(score, event_id, note_index, new_velocity);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, note_index, old_vel]() -> VoidResult {
            auto result = modify_velocity(score, event_id, note_index, old_vel);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    return MutationResult{{}};
}

Result<MutationResult> set_articulation(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    std::optional<ArticulationType> articulation,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto old_art = ng->notes[note_index].articulation;

    ng->notes[note_index].articulation = articulation;
    bump_version(score);

    push_undo(undo, score, "set_articulation",
        [&score, event_id, note_index, articulation]() -> VoidResult {
            auto result = set_articulation(score, event_id, note_index, articulation);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, note_index, old_art]() -> VoidResult {
            auto result = set_articulation(score, event_id, note_index, old_art);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    return MutationResult{{}};
}

Result<MutationResult> set_dynamic(
    Score& score,
    PartId part_id,
    ScoreTime position,
    DynamicLevel level,
    UndoStack* undo
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
                auto old_dynamic = ng->notes[0].dynamic;
                ng->notes[0].dynamic = level;
                bump_version(score);
                push_undo(undo, score, "set_dynamic",
                    [&score, part_id, position, level]() -> VoidResult {
                        auto result = set_dynamic(score, part_id, position, level);
                        if (!result) return std::unexpected(result.error());
                        return {};
                    },
                    [&score, part_id, position, old_dynamic]() -> VoidResult {
                        Part* p = find_part(score, part_id);
                        if (!p || position.bar < 1 || position.bar > p->measures.size())
                            return std::unexpected(ErrorCode::InvalidMutation);
                        auto& m = p->measures[position.bar - 1];
                        if (m.voices.empty()) return std::unexpected(ErrorCode::InvalidMutation);
                        for (auto& ev : m.voices[0].events) {
                            if (ev.offset == position.beat) {
                                auto* g = std::get_if<NoteGroup>(&ev.payload);
                                if (g && !g->notes.empty()) {
                                    g->notes[0].dynamic = old_dynamic;
                                    bump_version(score);
                                    return {};
                                }
                            }
                        }
                        return std::unexpected(ErrorCode::InvalidMutation);
                    }
                );
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
    std::optional<DynamicLevel> target,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    auto hairpin_index = part->hairpins.size();
    part->hairpins.push_back(Hairpin{start, end, type, target});
    bump_version(score);
    push_undo(undo, score, "insert_hairpin",
        [&score, part_id, start, end, type, target]() -> VoidResult {
            auto result = insert_hairpin(score, part_id, start, end, type, target);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, part_id, hairpin_index]() -> VoidResult {
            Part* p = find_part(score, part_id);
            if (!p || hairpin_index >= p->hairpins.size())
                return std::unexpected(ErrorCode::InvalidMutation);
            p->hairpins.erase(p->hairpins.begin() +
                static_cast<std::ptrdiff_t>(hairpin_index));
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> set_tie(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    bool tied,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    bool old_tie = ng->notes[note_index].tie_forward;

    ng->notes[note_index].tie_forward = tied;
    bump_version(score);

    push_undo(undo, score, "set_tie",
        [&score, event_id, note_index, tied]() -> VoidResult {
            auto result = set_tie(score, event_id, note_index, tied);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, note_index, old_tie]() -> VoidResult {
            auto result = set_tie(score, event_id, note_index, old_tie);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    return MutationResult{{}};
}

Result<MutationResult> transpose_event(
    Score& score,
    EventId event_id,
    DiatonicInterval interval,
    UndoStack* undo
) {
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng) return std::unexpected(ErrorCode::InvalidMutation);

    for (auto& note : ng->notes) {
        note.pitch = apply_interval(note.pitch, interval);
    }

    bump_version(score);

    push_undo(undo, score, "transpose_event",
        [&score, event_id, interval]() -> VoidResult {
            auto result = transpose_event(score, event_id, interval);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, event_id, interval]() -> VoidResult {
            auto result = transpose_event(score, event_id, interval_negate(interval));
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    mark_harmonic_stale(score, loc.measure->bar_number);

    return MutationResult{{}};
}

// =============================================================================
// Measure-Level Mutations
// =============================================================================

Result<MutationResult> insert_measures(
    Score& score,
    std::uint32_t after_bar,
    std::uint32_t count,
    UndoStack* undo
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

    push_undo(undo, score, "insert_measures",
        [&score, after_bar, count]() -> VoidResult {
            auto result = insert_measures(score, after_bar, count);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, after_bar, count]() -> VoidResult {
            auto result = delete_measures(score, after_bar + 1, count);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    return MutationResult{{}};
}

Result<MutationResult> delete_measures(
    Score& score,
    std::uint32_t bar,
    std::uint32_t count,
    UndoStack* undo
) {
    if (bar < 1 || bar + count - 1 > score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (count >= score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::uint32_t first = bar - 1;  // 0-indexed

    // Capture deleted measures per part for undo
    struct PartMeasures {
        PartId part_id;
        std::vector<Measure> measures;
    };
    std::vector<PartMeasures> saved_measures;
    for (const auto& part : score.parts) {
        PartMeasures pm{part.id, {}};
        for (std::uint32_t i = first; i < first + count && i < part.measures.size(); ++i) {
            pm.measures.push_back(part.measures[i]);
        }
        saved_measures.push_back(std::move(pm));
    }

    // Capture deleted map entries
    std::vector<TempoEvent> saved_tempo;
    for (const auto& e : score.tempo_map) {
        if (e.position.bar >= bar && e.position.bar < bar + count)
            saved_tempo.push_back(e);
    }
    std::vector<KeySignatureEntry> saved_keys;
    for (const auto& e : score.key_map) {
        if (e.position.bar >= bar && e.position.bar < bar + count)
            saved_keys.push_back(e);
    }
    std::vector<TimeSignatureEntry> saved_time;
    for (const auto& e : score.time_map) {
        if (e.bar >= bar && e.bar < bar + count)
            saved_time.push_back(e);
    }

    for (auto& part : score.parts) {
        part.measures.erase(
            part.measures.begin() + first,
            part.measures.begin() + first + count
        );
        for (std::size_t m = first; m < part.measures.size(); ++m) {
            part.measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
        }
    }

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

    push_undo(undo, score, "delete_measures",
        [&score, bar, count]() -> VoidResult {
            auto result = delete_measures(score, bar, count);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, bar, count, saved_measures, saved_tempo, saved_keys, saved_time]() -> VoidResult {
            std::uint32_t ins = bar - 1;
            // Re-insert measures into each part
            for (const auto& pm : saved_measures) {
                Part* part = find_part(score, pm.part_id);
                if (!part) continue;
                part->measures.insert(
                    part->measures.begin() + static_cast<std::ptrdiff_t>(ins),
                    pm.measures.begin(), pm.measures.end()
                );
                for (std::size_t m = 0; m < part->measures.size(); ++m) {
                    part->measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
                }
            }
            // Re-insert map entries and shift later entries back
            for (auto& entry : score.tempo_map) {
                if (entry.position.bar >= bar) entry.position.bar += count;
            }
            for (const auto& e : saved_tempo) score.tempo_map.push_back(e);
            std::sort(score.tempo_map.begin(), score.tempo_map.end(),
                [](const TempoEvent& a, const TempoEvent& b) {
                    return a.position < b.position;
                });

            for (auto& entry : score.key_map) {
                if (entry.position.bar >= bar) entry.position.bar += count;
            }
            for (const auto& e : saved_keys) score.key_map.push_back(e);
            std::sort(score.key_map.begin(), score.key_map.end(),
                [](const KeySignatureEntry& a, const KeySignatureEntry& b) {
                    return a.position < b.position;
                });

            for (auto& entry : score.time_map) {
                if (entry.bar >= bar) entry.bar += count;
            }
            for (const auto& e : saved_time) score.time_map.push_back(e);
            std::sort(score.time_map.begin(), score.time_map.end(),
                [](const TimeSignatureEntry& a, const TimeSignatureEntry& b) {
                    return a.bar < b.bar;
                });

            score.metadata.total_bars += count;
            bump_version(score);
            return {};
        }
    );

    return MutationResult{{}};
}

Result<MutationResult> set_time_signature(
    Score& score,
    std::uint32_t bar,
    TimeSignature time_sig,
    UndoStack* undo
) {
    if (bar < 1 || bar > score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Capture old state for undo
    std::optional<TimeSignature> old_ts;
    for (const auto& entry : score.time_map) {
        if (entry.bar == bar) { old_ts = entry.time_signature; break; }
    }

    bool found = false;
    for (auto& entry : score.time_map) {
        if (entry.bar == bar) {
            entry.time_signature = time_sig;
            found = true;
            break;
        }
    }
    if (!found) {
        TimeSignatureEntry entry{bar, time_sig};
        auto it = std::lower_bound(
            score.time_map.begin(), score.time_map.end(), bar,
            [](const TimeSignatureEntry& e, std::uint32_t b) {
                return e.bar < b;
            }
        );
        score.time_map.insert(it, std::move(entry));
    }

    bump_version(score);
    push_undo(undo, score, "set_time_signature",
        [&score, bar, time_sig]() -> VoidResult {
            auto result = set_time_signature(score, bar, time_sig);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, bar, old_ts]() -> VoidResult {
            if (old_ts) {
                for (auto& entry : score.time_map) {
                    if (entry.bar == bar) { entry.time_signature = *old_ts; break; }
                }
            } else {
                auto it = std::remove_if(score.time_map.begin(), score.time_map.end(),
                    [bar](const TimeSignatureEntry& e) { return e.bar == bar; });
                score.time_map.erase(it, score.time_map.end());
            }
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> set_key_signature(
    Score& score,
    ScoreTime position,
    KeySignature key,
    UndoStack* undo
) {
    std::optional<KeySignature> old_key;
    for (const auto& entry : score.key_map) {
        if (entry.position == position) { old_key = entry.key; break; }
    }

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
    push_undo(undo, score, "set_key_signature",
        [&score, position, key]() -> VoidResult {
            auto result = set_key_signature(score, position, key);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, position, old_key]() -> VoidResult {
            if (old_key) {
                for (auto& entry : score.key_map) {
                    if (entry.position == position) { entry.key = *old_key; break; }
                }
            } else {
                auto it = std::remove_if(score.key_map.begin(), score.key_map.end(),
                    [&position](const KeySignatureEntry& e) { return e.position == position; });
                score.key_map.erase(it, score.key_map.end());
            }
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

// =============================================================================
// Part-Level Mutations
// =============================================================================

Result<MutationResult> add_part(
    Score& score,
    PartDefinition definition,
    std::size_t position_in_order,
    UndoStack* undo
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

    PartId new_part_id = new_part.id;

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
    push_undo(undo, score, "add_part",
        [&score, definition, position_in_order]() -> VoidResult {
            auto result = add_part(score, definition, position_in_order);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, new_part_id]() -> VoidResult {
            auto it = std::find_if(score.parts.begin(), score.parts.end(),
                [new_part_id](const Part& p) { return p.id == new_part_id; });
            if (it == score.parts.end())
                return std::unexpected(ErrorCode::InvalidMutation);
            score.parts.erase(it);
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> remove_part(
    Score& score,
    PartId part_id,
    UndoStack* undo
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

    // Capture state for undo
    Part saved_part = *it;
    auto position = static_cast<std::size_t>(
        std::distance(score.parts.begin(), it));

    std::vector<OrchestrationAnnotation> saved_orch;
    for (const auto& a : score.orchestration_annotations) {
        if (a.part_id == part_id) saved_orch.push_back(a);
    }

    score.parts.erase(it);

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
    push_undo(undo, score, "remove_part",
        [&score, part_id]() -> VoidResult {
            auto result = remove_part(score, part_id);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, saved_part, position, saved_orch]() -> VoidResult {
            if (position <= score.parts.size()) {
                score.parts.insert(
                    score.parts.begin() +
                        static_cast<std::ptrdiff_t>(position),
                    saved_part
                );
            } else {
                score.parts.push_back(saved_part);
            }
            for (const auto& oa : saved_orch) {
                score.orchestration_annotations.push_back(oa);
            }
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

// =============================================================================
// Region-Level Mutations
// =============================================================================

Result<MutationResult> transpose_region(
    Score& score,
    const ScoreRegion& region,
    DiatonicInterval interval,
    UndoStack* undo
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

    push_undo(undo, score, "transpose_region",
        [&score, region, interval]() -> VoidResult {
            auto result = transpose_region(score, region, interval);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region, interval]() -> VoidResult {
            auto result = transpose_region(score, region, interval_negate(interval));
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> delete_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo
) {
    // Capture events that will be replaced for undo
    struct SavedEvent {
        EventId event_id;
        EventPayload old_payload;
    };
    std::vector<SavedEvent> saved;

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
                        saved.push_back({event.id, event.payload});
                        event.payload = RestEvent{dur, true};
                    }
                }
            }
        }
    }

    bump_version(score);
    push_undo(undo, score, "delete_region",
        [&score, region]() -> VoidResult {
            auto result = delete_region(score, region);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, saved]() -> VoidResult {
            for (const auto& s : saved) {
                auto loc = find_event(score, s.event_id);
                if (!loc.event) continue;
                loc.event->payload = s.old_payload;
            }
            bump_version(score);
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

// =============================================================================
// Voice-Level Mutations
// =============================================================================

Result<MutationResult> add_voice(
    Score& score,
    std::uint32_t bar,
    PartId part_id,
    std::uint8_t voice_number,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    if (bar < 1 || bar > part->measures.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto& measure = part->measures[bar - 1];

    // Check that a voice with this number does not already exist
    for (const auto& v : measure.voices) {
        if (v.voice_index == voice_number) {
            return std::unexpected(ErrorCode::InvalidMutation);
        }
    }

    // Determine measure duration from time_map
    const auto* ts_entry = find_time_signature(score, bar);
    Beat dur = ts_entry ? ts_entry->time_signature.measure_duration()
                        : Beat{4, 4};

    RestEvent rest{dur, true};
    Event event{allocate_event_id(), Beat::zero(), rest};
    Voice new_voice{voice_number, {event}, {}};
    measure.voices.push_back(std::move(new_voice));

    bump_version(score);
    push_undo(undo, score, "add_voice",
        [&score, bar, part_id, voice_number]() -> VoidResult {
            auto result = add_voice(score, bar, part_id, voice_number);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, bar, part_id, voice_number]() -> VoidResult {
            auto result = remove_voice(score, bar, part_id, voice_number);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> remove_voice(
    Score& score,
    std::uint32_t bar,
    PartId part_id,
    std::uint8_t voice_number,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    if (bar < 1 || bar > part->measures.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto& measure = part->measures[bar - 1];

    if (measure.voices.size() <= 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto it = std::find_if(
        measure.voices.begin(), measure.voices.end(),
        [voice_number](const Voice& v) { return v.voice_index == voice_number; }
    );
    if (it == measure.voices.end()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Voice saved_voice = *it;
    measure.voices.erase(it);

    bump_version(score);
    push_undo(undo, score, "remove_voice",
        [&score, bar, part_id, voice_number]() -> VoidResult {
            auto result = remove_voice(score, bar, part_id, voice_number);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, bar, part_id, saved_voice]() -> VoidResult {
            Part* p = find_part(score, part_id);
            if (!p || bar < 1 || bar > p->measures.size())
                return std::unexpected(ErrorCode::InvalidMutation);
            p->measures[bar - 1].voices.push_back(saved_voice);
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

// =============================================================================
// Part Management Mutations
// =============================================================================

Result<MutationResult> reorder_parts(
    Score& score,
    std::vector<PartId> new_order,
    UndoStack* undo
) {
    if (new_order.size() != score.parts.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Validate all ids exist
    for (const auto& pid : new_order) {
        bool found = false;
        for (const auto& part : score.parts) {
            if (part.id == pid) { found = true; break; }
        }
        if (!found) return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Capture old order before reordering
    std::vector<PartId> old_order;
    old_order.reserve(score.parts.size());
    for (const auto& part : score.parts) {
        old_order.push_back(part.id);
    }

    // Build reordered vector
    std::vector<Part> reordered;
    reordered.reserve(score.parts.size());
    for (const auto& pid : new_order) {
        for (auto& part : score.parts) {
            if (part.id == pid) {
                reordered.push_back(std::move(part));
                break;
            }
        }
    }
    score.parts = std::move(reordered);

    bump_version(score);
    push_undo(undo, score, "reorder_parts",
        [&score, new_order]() -> VoidResult {
            auto result = reorder_parts(score, new_order);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, old_order]() -> VoidResult {
            auto result = reorder_parts(score, old_order);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> set_part_directive(
    Score& score,
    PartId part_id,
    PartDirective directive,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    auto directive_copy = directive;
    part->part_directives.push_back(std::move(directive));
    auto dir_index = part->part_directives.size() - 1;

    bump_version(score);
    push_undo(undo, score, "set_part_directive",
        [&score, part_id, directive_copy]() -> VoidResult {
            auto result = set_part_directive(score, part_id, directive_copy);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, part_id, dir_index]() -> VoidResult {
            Part* p = find_part(score, part_id);
            if (!p || dir_index >= p->part_directives.size())
                return std::unexpected(ErrorCode::InvalidMutation);
            p->part_directives.erase(
                p->part_directives.begin() +
                    static_cast<std::ptrdiff_t>(dir_index));
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> assign_instrument(
    Score& score,
    PartId part_id,
    InstrumentType instrument,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    auto old_instrument = part->definition.instrument_type;
    part->definition.instrument_type = instrument;

    bump_version(score);
    push_undo(undo, score, "assign_instrument",
        [&score, part_id, instrument]() -> VoidResult {
            auto result = assign_instrument(score, part_id, instrument);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, part_id, old_instrument]() -> VoidResult {
            auto result = assign_instrument(score, part_id, old_instrument);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );
    return MutationResult{{}};
}

// =============================================================================
// Region-Level Mutations (extended)
// =============================================================================

Result<MutationResult> copy_region(
    Score& score,
    const ScoreRegion& src,
    ScoreTime dest,
    UndoStack* undo
) {
    // Collect events to copy: (part_id, bar_offset, event_clone)
    struct EventRecord {
        PartId part_id;
        std::uint32_t bar_offset;   // relative bar from region start
        std::uint8_t voice_index;
        Beat offset_in_measure;
        Event event;
    };
    std::vector<EventRecord> records;

    for_each_event_in_region(score, src,
        [&](Part& part, Measure& measure, Voice& voice, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;  // Only copy note events

            Event clone{allocate_event_id(), event.offset, event.payload};
            std::uint32_t bar_off = measure.bar_number - src.start.bar;
            records.push_back({part.id, bar_off, voice.voice_index,
                               event.offset, std::move(clone)});
        }
    );

    // Insert cloned events at dest
    for (auto& rec : records) {
        std::uint32_t target_bar = dest.bar + rec.bar_offset;

        Part* part = find_part(score, rec.part_id);
        if (!part) continue;
        if (target_bar < 1 || target_bar > part->measures.size()) continue;

        auto& measure = part->measures[target_bar - 1];

        // Find or skip if voice doesn't exist
        Voice* target_voice = nullptr;
        for (auto& v : measure.voices) {
            if (v.voice_index == rec.voice_index) {
                target_voice = &v;
                break;
            }
        }
        if (!target_voice) continue;

        // Adjust offset: for the first bar, add dest.beat offset
        Beat new_offset = rec.offset_in_measure;
        if (rec.bar_offset == 0) {
            new_offset = new_offset - src.start.beat + dest.beat;
        }
        rec.event.offset = new_offset;

        auto it = std::lower_bound(
            target_voice->events.begin(),
            target_voice->events.end(),
            new_offset,
            [](const Event& e, const Beat& off) { return e.offset < off; }
        );
        target_voice->events.insert(it, std::move(rec.event));
    }

    // Collect IDs of inserted events for undo
    std::vector<EventId> inserted_ids;
    for (const auto& rec : records) {
        inserted_ids.push_back(rec.event.id);
    }

    bump_version(score);
    push_undo(undo, score, "copy_region",
        [&score, src, dest]() -> VoidResult {
            auto result = copy_region(score, src, dest);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, inserted_ids]() -> VoidResult {
            for (const auto& eid : inserted_ids) {
                auto loc = find_event(score, eid);
                if (!loc.event || !loc.voice) continue;
                auto& events = loc.voice->events;
                events.erase(
                    std::remove_if(events.begin(), events.end(),
                        [eid](const Event& e) { return e.id == eid; }),
                    events.end()
                );
            }
            bump_version(score);
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(src);

    return MutationResult{{}};
}

Result<MutationResult> move_region(
    Score& score,
    const ScoreRegion& src,
    ScoreTime dest,
    UndoStack* undo
) {
    // Capture source events before the move for undo
    struct SavedSourceEvent {
        EventId event_id;
        EventPayload old_payload;
    };
    std::vector<SavedSourceEvent> saved_source;

    for_each_event_in_region(score, src,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (ng) {
                saved_source.push_back({event.id, event.payload});
            }
        }
    );

    // Track event IDs allocated by the copy for undo
    auto id_before_copy = next_event_id;
    auto copy_result = copy_region(score, src, dest);
    if (!copy_result) return copy_result;
    auto id_after_copy = next_event_id;

    // Delete source region (replace with rests), but do not double-bump
    for (auto& part : score.parts) {
        if (!src.parts.empty()) {
            bool found = false;
            for (const auto& pid : src.parts) {
                if (pid == part.id) { found = true; break; }
            }
            if (!found) continue;
        }

        for (auto& measure : part.measures) {
            ScoreTime measure_start{measure.bar_number, Beat::zero()};
            if (measure_start >= src.end) break;

            for (auto& voice : measure.voices) {
                for (auto& event : voice.events) {
                    ScoreTime event_time{measure.bar_number, event.offset};
                    if (event_time < src.start || event_time >= src.end) continue;

                    Beat dur = event.duration();
                    if (dur > Beat::zero()) {
                        event.payload = RestEvent{dur, true};
                    }
                }
            }
        }
    }

    // copy_region already bumped version; no extra bump needed
    push_undo(undo, score, "move_region",
        [&score, src, dest]() -> VoidResult {
            auto result = move_region(score, src, dest);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, saved_source, id_before_copy, id_after_copy]() -> VoidResult {
            // Remove events inserted by the copy (IDs in [id_before, id_after))
            for (auto& part : score.parts) {
                for (auto& measure : part.measures) {
                    for (auto& voice : measure.voices) {
                        voice.events.erase(
                            std::remove_if(voice.events.begin(), voice.events.end(),
                                [id_before_copy, id_after_copy](const Event& e) {
                                    return e.id.value >= id_before_copy &&
                                           e.id.value < id_after_copy;
                                }),
                            voice.events.end()
                        );
                    }
                }
            }
            // Restore source events from saved payloads
            for (const auto& s : saved_source) {
                auto loc = find_event(score, s.event_id);
                if (!loc.event) continue;
                loc.event->payload = s.old_payload;
            }
            bump_version(score);
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(src);

    return MutationResult{{}};
}

Result<MutationResult> set_dynamic_region(
    Score& score,
    const ScoreRegion& region,
    DynamicLevel level,
    UndoStack* undo
) {
    // Capture old dynamics for undo
    struct SavedDynamic {
        EventId event_id;
        std::optional<DynamicLevel> old_dynamic;
    };
    std::vector<SavedDynamic> saved;

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (ng && !ng->notes.empty()) {
                saved.push_back({event.id, ng->notes[0].dynamic});
                ng->notes[0].dynamic = level;
            }
        }
    );

    bump_version(score);
    push_undo(undo, score, "set_dynamic_region",
        [&score, region, level]() -> VoidResult {
            auto result = set_dynamic_region(score, region, level);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, saved]() -> VoidResult {
            for (const auto& s : saved) {
                auto loc = find_event(score, s.event_id);
                if (!loc.event) continue;
                auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
                if (ng && !ng->notes.empty()) {
                    ng->notes[0].dynamic = s.old_dynamic;
                }
            }
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> scale_velocity_region(
    Score& score,
    const ScoreRegion& region,
    double factor,
    UndoStack* undo
) {
    // Capture old velocities for undo
    struct SavedVelocity {
        EventId event_id;
        std::vector<std::uint8_t> old_values;
    };
    std::vector<SavedVelocity> saved;

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            SavedVelocity sv{event.id, {}};
            for (auto& note : ng->notes) {
                sv.old_values.push_back(note.velocity.value);
                double scaled = static_cast<double>(note.velocity.value) * factor;
                if (scaled < 0.0) scaled = 0.0;
                if (scaled > 127.0) scaled = 127.0;
                note.velocity.value = static_cast<std::uint8_t>(scaled);
            }
            saved.push_back(std::move(sv));
        }
    );

    bump_version(score);
    push_undo(undo, score, "scale_velocity_region",
        [&score, region, factor]() -> VoidResult {
            auto result = scale_velocity_region(score, region, factor);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, saved]() -> VoidResult {
            for (const auto& sv : saved) {
                auto loc = find_event(score, sv.event_id);
                if (!loc.event) continue;
                auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
                if (!ng) continue;
                for (std::size_t i = 0; i < sv.old_values.size() && i < ng->notes.size(); ++i) {
                    ng->notes[i].velocity.value = sv.old_values[i];
                }
            }
            bump_version(score);
            return {};
        }
    );
    return MutationResult{{}};
}

Result<MutationResult> retrograde_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo
) {
    // For each part/voice, collect events in the region, reverse their
    // order, and reassign offsets to match the reversed sequence.
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
                // Collect indices of events within the region
                std::vector<std::size_t> indices;
                for (std::size_t i = 0; i < voice.events.size(); ++i) {
                    ScoreTime event_time{measure.bar_number, voice.events[i].offset};
                    if (event_time >= region.start && event_time < region.end) {
                        indices.push_back(i);
                    }
                }

                if (indices.size() < 2) continue;

                // Save original offsets
                std::vector<Beat> original_offsets;
                original_offsets.reserve(indices.size());
                for (auto idx : indices) {
                    original_offsets.push_back(voice.events[idx].offset);
                }

                // Reverse the payloads among selected events
                for (std::size_t i = 0; i < indices.size() / 2; ++i) {
                    std::size_t j = indices.size() - 1 - i;
                    std::swap(voice.events[indices[i]].payload,
                              voice.events[indices[j]].payload);
                }

                // Reassign original offsets (events stay in positional order)
                for (std::size_t i = 0; i < indices.size(); ++i) {
                    voice.events[indices[i]].offset = original_offsets[i];
                }
            }
        }
    }

    bump_version(score);
    push_undo(undo, score, "retrograde_region",
        [&score, region]() -> VoidResult {
            auto result = retrograde_region(score, region);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region]() -> VoidResult {
            auto result = retrograde_region(score, region);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> invert_region(
    Score& score,
    const ScoreRegion& region,
    SpelledPitch axis,
    UndoStack* undo
) {
    int axis_midi = midi_value(axis);

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            for (auto& note : ng->notes) {
                int note_midi = midi_value(note.pitch);
                int new_midi = 2 * axis_midi - note_midi;

                // Mirror the letter name around the axis letter
                int mirror_letter = static_cast<int>(
                    (2 * axis.letter - note.pitch.letter + 700) % 7
                );

                // Approximate octave from new midi value
                int new_octave = (new_midi / 12) - 1;

                // Compute expected midi for this letter+octave with no accidental
                SpelledPitch candidate{
                    static_cast<std::uint8_t>(mirror_letter),
                    0,
                    static_cast<std::int8_t>(new_octave)
                };
                int candidate_midi = midi_value(candidate);
                std::int8_t new_acc = static_cast<std::int8_t>(new_midi - candidate_midi);

                note.pitch = SpelledPitch{
                    static_cast<std::uint8_t>(mirror_letter),
                    new_acc,
                    static_cast<std::int8_t>(new_octave)
                };
            }
        }
    );

    bump_version(score);
    push_undo(undo, score, "invert_region",
        [&score, region, axis]() -> VoidResult {
            auto result = invert_region(score, region, axis);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region, axis]() -> VoidResult {
            auto result = invert_region(score, region, axis);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> augment_region(
    Score& score,
    const ScoreRegion& region,
    Beat factor,
    UndoStack* undo
) {
    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            if (auto* ng = std::get_if<NoteGroup>(&event.payload)) {
                ng->duration = ng->duration * factor;
            } else if (auto* r = std::get_if<RestEvent>(&event.payload)) {
                r->duration = r->duration * factor;
            }
        }
    );

    bump_version(score);
    push_undo(undo, score, "augment_region",
        [&score, region, factor]() -> VoidResult {
            auto result = augment_region(score, region, factor);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region, factor]() -> VoidResult {
            auto result = diminute_region(score, region, factor);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> diminute_region(
    Score& score,
    const ScoreRegion& region,
    Beat factor,
    UndoStack* undo
) {
    if (factor.numerator == 0) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            if (auto* ng = std::get_if<NoteGroup>(&event.payload)) {
                ng->duration = ng->duration / factor;
            } else if (auto* r = std::get_if<RestEvent>(&event.payload)) {
                r->duration = r->duration / factor;
            }
        }
    );

    bump_version(score);
    push_undo(undo, score, "diminute_region",
        [&score, region, factor]() -> VoidResult {
            auto result = diminute_region(score, region, factor);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region, factor]() -> VoidResult {
            auto result = augment_region(score, region, factor);
            if (!result) return std::unexpected(result.error());
            return {};
        }
    );

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

// =============================================================================
// Orchestration Mutations
// =============================================================================

Result<MutationResult> reorchestrate(
    Score& score,
    const ScoreRegion& region,
    PartId target,
    UndoStack* undo
) {
    Part* target_part = find_part(score, target);
    if (!target_part) return std::unexpected(ErrorCode::InvalidMutation);

    // Collect note events from the region (excluding the target part itself)
    struct EventRecord {
        std::uint32_t bar_number;
        std::uint8_t voice_index;
        Event event;
    };
    std::vector<EventRecord> records;

    for_each_event_in_region(score, region,
        [&](Part& part, Measure& measure, Voice& voice, Event& event) {
            if (part.id == target) return;  // Skip target part
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            Event clone{allocate_event_id(), event.offset, event.payload};
            records.push_back({measure.bar_number, voice.voice_index,
                               std::move(clone)});
        }
    );

    // Insert into target part
    for (auto& rec : records) {
        if (rec.bar_number < 1 || rec.bar_number > target_part->measures.size()) {
            continue;
        }

        auto& measure = target_part->measures[rec.bar_number - 1];

        Voice* target_voice = nullptr;
        for (auto& v : measure.voices) {
            if (v.voice_index == rec.voice_index) {
                target_voice = &v;
                break;
            }
        }
        if (!target_voice && !measure.voices.empty()) {
            target_voice = &measure.voices[0];
        }
        if (!target_voice) continue;

        auto it = std::lower_bound(
            target_voice->events.begin(),
            target_voice->events.end(),
            rec.event.offset,
            [](const Event& e, const Beat& off) { return e.offset < off; }
        );
        target_voice->events.insert(it, std::move(rec.event));
    }

    // Collect IDs of inserted events for undo
    std::vector<EventId> inserted_ids;
    for (const auto& rec : records) {
        inserted_ids.push_back(rec.event.id);
    }

    bump_version(score);
    push_undo(undo, score, "reorchestrate",
        [&score, region, target]() -> VoidResult {
            auto result = reorchestrate(score, region, target);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, inserted_ids]() -> VoidResult {
            for (const auto& eid : inserted_ids) {
                auto loc = find_event(score, eid);
                if (!loc.event || !loc.voice) continue;
                auto& events = loc.voice->events;
                events.erase(
                    std::remove_if(events.begin(), events.end(),
                        [eid](const Event& e) { return e.id == eid; }),
                    events.end()
                );
            }
            bump_version(score);
            return {};
        }
    );

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

Result<MutationResult> double_at_interval(
    Score& score,
    const ScoreRegion& region,
    PartId target,
    std::int8_t interval,
    UndoStack* undo
) {
    Part* target_part = find_part(score, target);
    if (!target_part) return std::unexpected(ErrorCode::InvalidMutation);

    struct EventRecord {
        std::uint32_t bar_number;
        std::uint8_t voice_index;
        Event event;
    };
    std::vector<EventRecord> records;

    for_each_event_in_region(score, region,
        [&](Part& part, Measure& measure, Voice& voice, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            // Clone and transpose each note by the semitone interval
            NoteGroup transposed = *ng;
            for (auto& note : transposed.notes) {
                int new_midi = midi_value(note.pitch) + interval;
                int new_octave = (new_midi / 12) - 1;
                // Keep the same letter name; adjust accidental
                SpelledPitch candidate{
                    note.pitch.letter,
                    0,
                    static_cast<std::int8_t>(new_octave)
                };
                int candidate_midi = midi_value(candidate);
                std::int8_t new_acc = static_cast<std::int8_t>(
                    new_midi - candidate_midi
                );
                note.pitch = SpelledPitch{
                    note.pitch.letter,
                    new_acc,
                    static_cast<std::int8_t>(new_octave)
                };
            }

            Event clone{allocate_event_id(), event.offset, std::move(transposed)};
            records.push_back({measure.bar_number, voice.voice_index,
                               std::move(clone)});
        }
    );

    for (auto& rec : records) {
        if (rec.bar_number < 1 || rec.bar_number > target_part->measures.size()) {
            continue;
        }

        auto& measure = target_part->measures[rec.bar_number - 1];

        Voice* target_voice = nullptr;
        for (auto& v : measure.voices) {
            if (v.voice_index == rec.voice_index) {
                target_voice = &v;
                break;
            }
        }
        if (!target_voice && !measure.voices.empty()) {
            target_voice = &measure.voices[0];
        }
        if (!target_voice) continue;

        auto it = std::lower_bound(
            target_voice->events.begin(),
            target_voice->events.end(),
            rec.event.offset,
            [](const Event& e, const Beat& off) { return e.offset < off; }
        );
        target_voice->events.insert(it, std::move(rec.event));
    }

    // Collect IDs of inserted events for undo
    std::vector<EventId> dbl_inserted_ids;
    for (const auto& rec : records) {
        dbl_inserted_ids.push_back(rec.event.id);
    }

    bump_version(score);
    push_undo(undo, score, "double_at_interval",
        [&score, region, target, interval]() -> VoidResult {
            auto result = double_at_interval(score, region, target, interval);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, dbl_inserted_ids]() -> VoidResult {
            for (const auto& eid : dbl_inserted_ids) {
                auto loc = find_event(score, eid);
                if (!loc.event || !loc.voice) continue;
                auto& events = loc.voice->events;
                events.erase(
                    std::remove_if(events.begin(), events.end(),
                        [eid](const Event& e) { return e.id == eid; }),
                    events.end()
                );
            }
            bump_version(score);
            return {};
        }
    );

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

Result<MutationResult> set_texture_role(
    Score& score,
    const ScoreRegion& region,
    PartId part_id,
    TexturalRole role,
    UndoStack* undo
) {
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    // Capture old annotation for undo
    std::optional<TexturalRole> old_role;
    bool was_existing = false;

    // Update existing annotation if one overlaps, otherwise add new
    bool found = false;
    for (auto& ann : score.orchestration_annotations) {
        if (ann.part_id == part_id &&
            ann.start == region.start && ann.end == region.end) {
            old_role = ann.role;
            was_existing = true;
            ann.role = role;
            found = true;
            break;
        }
    }

    if (!found) {
        OrchestrationAnnotation ann;
        ann.part_id = part_id;
        ann.start = region.start;
        ann.end = region.end;
        ann.role = role;
        score.orchestration_annotations.push_back(std::move(ann));
    }

    bump_version(score);
    push_undo(undo, score, "set_texture_role",
        [&score, region, part_id, role]() -> VoidResult {
            auto result = set_texture_role(score, region, part_id, role);
            if (!result) return std::unexpected(result.error());
            return {};
        },
        [&score, region, part_id, old_role, was_existing]() -> VoidResult {
            if (was_existing && old_role) {
                for (auto& ann : score.orchestration_annotations) {
                    if (ann.part_id == part_id &&
                        ann.start == region.start && ann.end == region.end) {
                        ann.role = *old_role;
                        break;
                    }
                }
            } else {
                auto it = std::remove_if(
                    score.orchestration_annotations.begin(),
                    score.orchestration_annotations.end(),
                    [&](const OrchestrationAnnotation& a) {
                        return a.part_id == part_id &&
                               a.start == region.start && a.end == region.end;
                    }
                );
                score.orchestration_annotations.erase(
                    it, score.orchestration_annotations.end());
            }
            bump_version(score);
            return {};
        }
    );

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

Result<MutationResult> apply_voice_leading(
    Score& score,
    const ScoreRegion& region,
    VoiceLeadingStyle /*style*/,
    UndoStack* undo
) {
    // Placeholder: full implementation requires harmonic analysis context
    bump_version(score);
    push_undo(undo, score, "apply_voice_leading",
        [&score]() -> VoidResult {
            bump_version(score);
            return {};
        },
        [&score]() -> VoidResult {
            bump_version(score);
            return {};
        }
    );

    mark_orchestration_stale(score, region);

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

    stack.redo_entries.push_back(std::move(entry));

    return {};
}

VoidResult redo(Score& score, UndoStack& stack) {
    if (!stack.can_redo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto entry = std::move(stack.redo_entries.back());
    stack.redo_entries.pop_back();

    auto result = entry.forward();
    if (!result) return result;

    bump_version(score);
    stack.undo_entries.push_back(std::move(entry));

    return {};
}

}  // namespace Sunny::Core
