/**
 * @file SIMT001A.cpp
 * @brief Score IR mutations — implementation
 *
 * Component: SIMT001A
 * Domain: SI (Score IR) | Category: MT (Mutation)
 */

#include "SIMT001A.h"
#include "SIVD001A.h"
#include "SIQR001A.h"
#include "../VoiceLeading/VLNT001A.h"

#include <algorithm>
#include <memory>
#include <set>

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

/// Global event id counter (simple monotonic).
/// Single-threaded contract: Score mutations are not thread-safe.
/// This counter is monotonically increasing within a process lifetime.
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

/// Push a pre-mutation snapshot onto the undo stack (if provided), clearing
/// redo. When group_depth > 0, only the first snapshot of the group is kept:
/// the earliest 'before' state covers the whole group.
void push_snapshot(UndoStack* undo, std::optional<Score>&& before,
                   std::string desc) {
    if (!undo || !before) return;
    const std::uint64_t snapshot_version = before->version;
    UndoEntry entry{
        snapshot_version,
        std::make_shared<const Score>(std::move(*before)),
        std::move(desc)
    };
    if (undo->group_depth > 0) {
        if (!undo->pending_group) {
            undo->pending_group = std::move(entry);
        }
    } else {
        undo->undo_entries.push_back(std::move(entry));
        undo->redo_entries.clear();
    }
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

    std::optional<Score> before;
    if (undo) before = score;

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

    push_snapshot(undo, std::move(before), "insert_note");

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

    std::optional<Score> before;
    if (undo) before = score;

    auto old_bar = loc.measure->bar_number;

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

    push_snapshot(undo, std::move(before), "delete_event");

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

    std::optional<Score> before;
    if (undo) before = score;

    ng->notes[note_index].pitch = new_pitch;
    bump_version(score);

    push_snapshot(undo, std::move(before), "modify_pitch");

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

    std::optional<Score> before;
    if (undo) before = score;

    if (auto* ng = std::get_if<NoteGroup>(&loc.event->payload)) {
        ng->duration = new_duration;
    } else if (auto* r = std::get_if<RestEvent>(&loc.event->payload)) {
        r->duration = new_duration;
    } else {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    bump_version(score);

    push_snapshot(undo, std::move(before), "modify_duration");

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

    std::optional<Score> before;
    if (undo) before = score;

    ng->notes[note_index].velocity = new_velocity;
    bump_version(score);

    push_snapshot(undo, std::move(before), "modify_velocity");

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

    std::optional<Score> before;
    if (undo) before = score;

    ng->notes[note_index].articulation = articulation;
    bump_version(score);

    push_snapshot(undo, std::move(before), "set_articulation");

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
                std::optional<Score> before;
                if (undo) before = score;
                ng->notes[0].dynamic = level;
                bump_version(score);
                push_snapshot(undo, std::move(before), "set_dynamic");
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

    std::optional<Score> before;
    if (undo) before = score;

    part->hairpins.push_back(Hairpin{start, end, type, target});
    bump_version(score);
    push_snapshot(undo, std::move(before), "insert_hairpin");
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

    std::optional<Score> before;
    if (undo) before = score;

    ng->notes[note_index].tie_forward = tied;
    bump_version(score);

    push_snapshot(undo, std::move(before), "set_tie");

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

    std::optional<Score> before;
    if (undo) before = score;

    for (auto& note : ng->notes) {
        note.pitch = apply_interval(note.pitch, interval);
    }

    bump_version(score);

    push_snapshot(undo, std::move(before), "transpose_event");

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

    // Pre-validate: after_bar must not exceed any part's actual measure count
    for (const auto& part : score.parts) {
        if (after_bar > part.measures.size())
            return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

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

    // Shift position-bearing annotation structures
    for (auto& s : score.section_map) {
        if (s.start.bar > after_bar) s.start.bar += count;
        if (s.end.bar > after_bar) s.end.bar += count;
    }
    for (auto& ha : score.harmonic_annotations) {
        if (ha.position.bar > after_bar) ha.position.bar += count;
    }
    for (auto& oa : score.orchestration_annotations) {
        if (oa.start.bar > after_bar) oa.start.bar += count;
        if (oa.end.bar > after_bar) oa.end.bar += count;
    }
    for (auto& rm : score.rehearsal_marks) {
        if (rm.position.bar > after_bar) rm.position.bar += count;
    }
    for (auto& part : score.parts) {
        for (auto& hp : part.hairpins) {
            if (hp.start.bar > after_bar) hp.start.bar += count;
            if (hp.end.bar > after_bar) hp.end.bar += count;
        }
    }

    score.metadata.total_bars += count;
    bump_version(score);

    push_snapshot(undo, std::move(before), "insert_measures");

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

    std::optional<Score> before;
    if (undo) before = score;

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

    // Remove and shift annotation structures
    score.section_map.erase(
        std::remove_if(score.section_map.begin(), score.section_map.end(),
            [bar, count](const ScoreSection& s) {
                return s.start.bar >= bar && s.start.bar < bar + count;
            }),
        score.section_map.end());
    for (auto& s : score.section_map) {
        if (s.start.bar >= bar + count) s.start.bar -= count;
        if (s.end.bar >= bar + count) s.end.bar -= count;
    }

    score.harmonic_annotations.erase(
        std::remove_if(score.harmonic_annotations.begin(), score.harmonic_annotations.end(),
            [bar, count](const HarmonicAnnotation& ha) {
                return ha.position.bar >= bar && ha.position.bar < bar + count;
            }),
        score.harmonic_annotations.end());
    for (auto& ha : score.harmonic_annotations) {
        if (ha.position.bar >= bar + count) ha.position.bar -= count;
    }

    score.orchestration_annotations.erase(
        std::remove_if(score.orchestration_annotations.begin(), score.orchestration_annotations.end(),
            [bar, count](const OrchestrationAnnotation& oa) {
                return oa.start.bar >= bar && oa.start.bar < bar + count;
            }),
        score.orchestration_annotations.end());
    for (auto& oa : score.orchestration_annotations) {
        if (oa.start.bar >= bar + count) oa.start.bar -= count;
        if (oa.end.bar >= bar + count) oa.end.bar -= count;
    }

    score.rehearsal_marks.erase(
        std::remove_if(score.rehearsal_marks.begin(), score.rehearsal_marks.end(),
            [bar, count](const RehearsalMark& rm) {
                return rm.position.bar >= bar && rm.position.bar < bar + count;
            }),
        score.rehearsal_marks.end());
    for (auto& rm : score.rehearsal_marks) {
        if (rm.position.bar >= bar + count) rm.position.bar -= count;
    }

    for (auto& part : score.parts) {
        part.hairpins.erase(
            std::remove_if(part.hairpins.begin(), part.hairpins.end(),
                [bar, count](const Hairpin& hp) {
                    return hp.start.bar >= bar && hp.start.bar < bar + count;
                }),
            part.hairpins.end());
        for (auto& hp : part.hairpins) {
            if (hp.start.bar >= bar + count) hp.start.bar -= count;
            if (hp.end.bar >= bar + count) hp.end.bar -= count;
        }
    }

    score.metadata.total_bars -= count;
    bump_version(score);

    push_snapshot(undo, std::move(before), "delete_measures");

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

    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "set_time_signature");
    return MutationResult{{}};
}

Result<MutationResult> set_key_signature(
    Score& score,
    ScoreTime position,
    KeySignature key,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "set_key_signature");
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
    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "add_part");
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

    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "remove_part");
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
    std::optional<Score> before;
    if (undo) before = score;

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

    push_snapshot(undo, std::move(before), "transpose_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> delete_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "delete_region");

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

    std::optional<Score> before;
    if (undo) before = score;

    // Determine measure duration from time_map
    const auto* ts_entry = find_time_signature(score, bar);
    Beat dur = ts_entry ? ts_entry->time_signature.measure_duration()
                        : Beat{4, 4};

    RestEvent rest{dur, true};
    Event event{allocate_event_id(), Beat::zero(), rest};
    Voice new_voice{voice_number, {event}, {}};
    measure.voices.push_back(std::move(new_voice));

    bump_version(score);
    push_snapshot(undo, std::move(before), "add_voice");
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

    std::optional<Score> before;
    if (undo) before = score;

    measure.voices.erase(it);

    bump_version(score);
    push_snapshot(undo, std::move(before), "remove_voice");
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

    // Reject duplicate PartIds — each part must appear exactly once
    {
        std::set<std::uint64_t> seen;
        for (const auto& pid : new_order) {
            if (!seen.insert(pid.value).second) {
                return std::unexpected(ErrorCode::InvalidMutation);
            }
        }
    }

    // Validate all ids exist
    for (const auto& pid : new_order) {
        bool found = false;
        for (const auto& part : score.parts) {
            if (part.id == pid) { found = true; break; }
        }
        if (!found) return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "reorder_parts");
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

    std::optional<Score> before;
    if (undo) before = score;

    part->part_directives.push_back(std::move(directive));

    bump_version(score);
    push_snapshot(undo, std::move(before), "set_part_directive");
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

    std::optional<Score> before;
    if (undo) before = score;

    part->definition.instrument_type = instrument;

    bump_version(score);
    push_snapshot(undo, std::move(before), "assign_instrument");
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
    std::optional<Score> before;
    if (undo) before = score;

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

    bump_version(score);
    push_snapshot(undo, std::move(before), "copy_region");

    score.stale_harmonic_regions.push_back(src);

    return MutationResult{{}};
}

Result<MutationResult> move_region(
    Score& score,
    const ScoreRegion& src,
    ScoreTime dest,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

    auto copy_result = copy_region(score, src, dest);
    if (!copy_result) {
        // Mid-mutation failure: restore the pre-mutation snapshot
        if (before) score = *before;
        return copy_result;
    }

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
    push_snapshot(undo, std::move(before), "move_region");

    score.stale_harmonic_regions.push_back(src);

    return MutationResult{{}};
}

Result<MutationResult> set_dynamic_region(
    Score& score,
    const ScoreRegion& region,
    DynamicLevel level,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (ng && !ng->notes.empty()) {
                ng->notes[0].dynamic = level;
            }
        }
    );

    bump_version(score);
    push_snapshot(undo, std::move(before), "set_dynamic_region");
    return MutationResult{{}};
}

Result<MutationResult> scale_velocity_region(
    Score& score,
    const ScoreRegion& region,
    double factor,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

    for_each_event_in_region(score, region,
        [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            for (auto& note : ng->notes) {
                double scaled = static_cast<double>(note.velocity.value) * factor;
                if (scaled < 0.0) scaled = 0.0;
                if (scaled > 127.0) scaled = 127.0;
                note.velocity.value = static_cast<std::uint8_t>(scaled);
            }
        }
    );

    bump_version(score);
    push_snapshot(undo, std::move(before), "scale_velocity_region");
    return MutationResult{{}};
}

Result<MutationResult> retrograde_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "retrograde_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> invert_region(
    Score& score,
    const ScoreRegion& region,
    SpelledPitch axis,
    UndoStack* undo
) {
    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "invert_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> augment_region(
    Score& score,
    const ScoreRegion& region,
    Beat factor,
    UndoStack* undo
) {
    if (factor.numerator <= 0 || factor.denominator <= 0) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "augment_region");

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

    std::optional<Score> before;
    if (undo) before = score;

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
    push_snapshot(undo, std::move(before), "diminute_region");

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

    std::optional<Score> before;
    if (undo) before = score;

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

    bump_version(score);
    push_snapshot(undo, std::move(before), "reorchestrate");

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

    std::optional<Score> before;
    if (undo) before = score;

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

    bump_version(score);
    push_snapshot(undo, std::move(before), "double_at_interval");

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

    std::optional<Score> before;
    if (undo) before = score;

    // Update existing annotation if one overlaps, otherwise add new
    bool found = false;
    for (auto& ann : score.orchestration_annotations) {
        if (ann.part_id == part_id &&
            ann.start == region.start && ann.end == region.end) {
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
    push_snapshot(undo, std::move(before), "set_texture_role");

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

Result<MutationResult> apply_voice_leading(
    Score& score,
    const ScoreRegion& region,
    VoiceLeadingStyle style,
    UndoStack* undo
) {
    // Require harmonic annotations in the region
    auto annotations = query_harmony_range(
        score, region.start, region.end);
    if (annotations.empty()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Determine voice leading constraints from style
    bool lock_bass = (style == VoiceLeadingStyle::SmoothBach);
    bool allow_par_fifths = (style == VoiceLeadingStyle::NearestTone ||
                             style == VoiceLeadingStyle::ParallelMotion);
    bool allow_par_octaves = (style == VoiceLeadingStyle::NearestTone ||
                              style == VoiceLeadingStyle::ParallelMotion);

    std::optional<Score> before;
    if (undo) before = score;

    // Process consecutive annotation pairs: for each transition from
    // one chord to the next, voice-lead the notes at the boundary.
    for (std::size_t ai = 1; ai < annotations.size(); ++ai) {
        const auto& target_ann = annotations[ai];
        auto target_pcs = target_ann.chord.pitch_classes();
        if (target_pcs.empty()) continue;

        // Deduplicate pitch classes, placing chord root first when lock_bass
        // is active so voice_lead_nearest_tone assigns the bass voice to it.
        std::sort(target_pcs.begin(), target_pcs.end());
        target_pcs.erase(
            std::unique(target_pcs.begin(), target_pcs.end()),
            target_pcs.end());

        if (lock_bass) {
            PitchClass root = target_ann.chord.root;
            auto root_it = std::find(target_pcs.begin(), target_pcs.end(), root);
            if (root_it != target_pcs.end() && root_it != target_pcs.begin()) {
                std::rotate(target_pcs.begin(), root_it, root_it + 1);
            }
        }

        // Collect current notes at the target annotation position
        for_each_event_in_region(score, ScoreRegion{
            target_ann.position,
            ScoreTime{target_ann.position.bar,
                      target_ann.position.beat + target_ann.duration},
            region.parts
        }, [&](Part&, Measure&, Voice&, Event& event) {
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng || ng->notes.empty()) return;

            // Gather source MIDI pitches
            std::vector<MidiNote> source;
            source.reserve(ng->notes.size());
            for (const auto& note : ng->notes) {
                if (auto mv = MidiNote::from_int(midi_value(note.pitch))) {
                    source.push_back(*mv);
                }
            }
            if (source.empty()) return;

            // voice_lead_nearest_tone requires equal cardinalities.
            // Pad target pitch classes by cycling when source is larger,
            // or truncate when target is larger, to match source size.
            std::vector<PitchClass> matched_pcs = target_pcs;
            while (matched_pcs.size() < source.size() && !target_pcs.empty()) {
                matched_pcs.push_back(
                    target_pcs[matched_pcs.size() % target_pcs.size()]);
            }
            if (matched_pcs.size() > source.size()) {
                matched_pcs.resize(source.size());
            }

            auto vl_result = voice_lead_nearest_tone(
                source, matched_pcs,
                lock_bass, allow_par_fifths, allow_par_octaves);
            if (!vl_result) return;

            const auto& voiced = vl_result->voiced_notes;
            std::size_t count = std::min(voiced.size(), ng->notes.size());
            for (std::size_t ni = 0; ni < count; ++ni) {
                int new_midi = voiced[ni];
                int old_midi = midi_value(ng->notes[ni].pitch);
                if (new_midi == old_midi) continue;

                // Reconstruct SpelledPitch from MIDI value
                // Preserve the letter name; adjust octave and accidental
                auto& note = ng->notes[ni];
                int base_midi_no_acc = midi_value(SpelledPitch{
                    note.pitch.letter, 0,
                    static_cast<std::int8_t>((new_midi / 12) - 1)});
                std::int8_t new_acc = static_cast<std::int8_t>(
                    new_midi - base_midi_no_acc);
                // Clamp accidental range and adjust octave if needed
                while (new_acc > 2) { new_acc -= 12; }
                while (new_acc < -2) { new_acc += 12; }
                note.pitch = SpelledPitch{
                    note.pitch.letter,
                    new_acc,
                    static_cast<std::int8_t>((new_midi / 12) - 1)};
            }
        });
    }

    bump_version(score);

    push_snapshot(undo, std::move(before), "apply_voice_leading");

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

// =============================================================================
// Undo Grouping
// =============================================================================

void UndoStack::begin_group(std::string description) {
    if (group_depth == 0) {
        pending_group.reset();
        group_description = std::move(description);
    }
    ++group_depth;
}

void UndoStack::end_group() {
    if (group_depth == 0) return;
    --group_depth;
    if (group_depth > 0) return;

    if (!pending_group) return;

    // The first snapshot of the group is the pre-group state; publishing
    // it as one entry makes the whole group a single undo step.
    pending_group->description = std::move(group_description);
    undo_entries.push_back(std::move(*pending_group));
    redo_entries.clear();
    pending_group.reset();
}

// =============================================================================
// Undo/Redo
// =============================================================================

VoidResult undo(Score& score, UndoStack& stack) {
    if (!stack.can_undo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    UndoEntry entry = std::move(stack.undo_entries.back());
    stack.undo_entries.pop_back();

    // Save the live document for redo, then restore the snapshot.
    // Restoration cannot fail, so the swap needs no failure path.
    stack.redo_entries.push_back(UndoEntry{
        score.version,
        std::make_shared<const Score>(score),
        entry.description
    });

    const std::uint64_t next_version = score.version + 1;
    score = *entry.state;
    score.version = next_version;

    return {};
}

VoidResult redo(Score& score, UndoStack& stack) {
    if (!stack.can_redo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    UndoEntry entry = std::move(stack.redo_entries.back());
    stack.redo_entries.pop_back();

    // Save the live document for undo, then restore the snapshot.
    stack.undo_entries.push_back(UndoEntry{
        score.version,
        std::make_shared<const Score>(score),
        entry.description
    });

    const std::uint64_t next_version = score.version + 1;
    score = *entry.state;
    score.version = next_version;

    return {};
}

}  // namespace Sunny::Core
