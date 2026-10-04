#include <algorithm>
#include <map>
#include <set>
#include <sunny/core/score/projection.hpp>
#include <sunny/core/score/time.hpp>
#include <tuple>

namespace sunny::core {

Result<std::vector<SymbolicNoteSpan>> project_symbolic_notes(const Score& score) {
    std::vector<SymbolicNoteSpan> result;
    struct PendingTie {
        SpelledPitch pitch;
        Beat end;
        EventId attack_event_id;
        std::size_t attack_note_index;
    };
    for (const auto& part : score.parts) {
        std::map<std::uint8_t, std::vector<PendingTie>> pending;
        std::map<std::uint8_t, Beat> previous_ends;
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                auto& ties = pending[voice.voice_index];
                for (const auto& event : voice.events) {
                    if (!event.is_note_group() && !event.is_rest()) continue;
                    const ScoreTime position{measure.bar_number, event.offset};
                    auto start = score_time_to_absolute_beat(position, score.time_map);
                    if (!start) return std::unexpected(start.error());
                    if (*start < Beat::zero() || event.offset < Beat::zero() ||
                        event.duration() <= Beat::zero())
                        return std::unexpected(ErrorCode::InvariantViolation);
                    auto end = checked_add(*start, event.duration());
                    if (!end) return std::unexpected(end.error());
                    auto previous = previous_ends.find(voice.voice_index);
                    if (previous != previous_ends.end() && *start < previous->second)
                        return std::unexpected(ErrorCode::InvariantViolation);
                    previous_ends[voice.voice_index] = *end;

                    const auto* group = event.as_note_group();
                    if (!group) {
                        if (!ties.empty()) return std::unexpected(ErrorCode::InvariantViolation);
                        continue;
                    }
                    std::vector<PendingTie> next_ties;
                    for (std::size_t index = 0; index < group->notes.size(); ++index) {
                        const auto& note = group->notes[index];
                        const auto continuation =
                            std::find_if(ties.begin(), ties.end(), [&](const PendingTie& tie) {
                                return !note.grace && tie.pitch == note.pitch && tie.end == *start;
                            });
                        const bool attack = continuation == ties.end();
                        const auto attack_event = attack ? event.id : continuation->attack_event_id;
                        const auto attack_index = attack ? index : continuation->attack_note_index;
                        if (!attack) ties.erase(continuation);
                        result.push_back({part.id,
                                          voice.voice_index,
                                          event.id,
                                          index,
                                          position,
                                          note.pitch,
                                          *start,
                                          *end,
                                          attack,
                                          note.grace.has_value(),
                                          note.grace,
                                          attack_event,
                                          attack_index});
                        if (note.tie_forward) {
                            if (note.grace) return std::unexpected(ErrorCode::InvariantViolation);
                            next_ties.push_back({note.pitch, *end, attack_event, attack_index});
                        }
                    }
                    if (!ties.empty()) return std::unexpected(ErrorCode::InvariantViolation);
                    ties = std::move(next_ties);
                }
            }
        }
        for (const auto& [voice, ties] : pending) {
            (void)voice;
            if (!ties.empty()) return std::unexpected(ErrorCode::InvariantViolation);
        }
    }
    return result;
}

Result<std::vector<SymbolicNoteSpan>> fold_symbolic_ties(std::span<const SymbolicNoteSpan> notes) {
    using Key = std::tuple<PartId, std::uint8_t, EventId, std::size_t>;
    std::map<Key, std::size_t> heads;
    std::vector<SymbolicNoteSpan> ordered(notes.begin(), notes.end());
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return std::tie(a.part_id, a.voice_index, a.start) <
               std::tie(b.part_id, b.voice_index, b.start);
    });
    std::vector<SymbolicNoteSpan> result;
    for (const auto& note : ordered) {
        if (note.start < Beat::zero() || note.end <= note.start)
            return std::unexpected(ErrorCode::InvariantViolation);
        const Key key{note.part_id,
                      note.voice_index,
                      note.attack ? note.event_id : note.attack_event_id,
                      note.attack ? note.note_index : note.attack_note_index};
        if (note.attack) {
            if (!heads.emplace(key, result.size()).second)
                return std::unexpected(ErrorCode::InvariantViolation);
            result.push_back(note);
        } else {
            const auto head = heads.find(key);
            if (head == heads.end() || result[head->second].end != note.start ||
                result[head->second].pitch != note.pitch || note.grace)
                return std::unexpected(ErrorCode::InvariantViolation);
            result[head->second].end = note.end;
        }
    }
    return result;
}

Result<std::vector<SymbolicSlice>>
partition_symbolic_notes(std::span<const SymbolicNoteSpan> notes,
                         std::span<const Beat> extra_boundaries) {
    struct Boundary {
        std::vector<std::size_t> starts;
        std::vector<std::size_t> ends;
    };
    std::map<Beat, Boundary> boundaries;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        const auto& note = notes[index];
        if (note.start < Beat::zero() || note.end <= note.start)
            return std::unexpected(ErrorCode::InvariantViolation);
        boundaries[note.start].starts.push_back(index);
        boundaries[note.end].ends.push_back(index);
    }
    for (Beat boundary : extra_boundaries) {
        if (boundary < Beat::zero()) return std::unexpected(ErrorCode::InvariantViolation);
        boundaries.try_emplace(boundary);
    }

    std::set<std::size_t> active;
    std::vector<SymbolicSlice> result;
    for (auto it = boundaries.begin(); it != boundaries.end(); ++it) {
        for (auto index : it->second.ends)
            active.erase(index);
        for (auto index : it->second.starts)
            active.insert(index);
        const auto next = std::next(it);
        if (next == boundaries.end()) break;
        SymbolicSlice slice;
        slice.start = it->first;
        slice.end = next->first;
        slice.sounding_indices.assign(active.begin(), active.end());
        for (auto index : it->second.starts) {
            if (notes[index].attack) slice.attack_indices.push_back(index);
        }
        result.push_back(std::move(slice));
    }
    return result;
}

} // namespace sunny::core
