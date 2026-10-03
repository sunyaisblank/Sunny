/**
 * @file mutations.cpp
 * @brief Score IR mutations — implementation
 *
 */

#include "id_allocator.hpp"
#include "version.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/tuplets.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>
#include <tuple>

namespace sunny::core {

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

/// Create empty measure with a single voice containing a whole-measure rest
Result<Measure> make_empty_measure(std::uint32_t bar_number,
                                   const TimeSignature& ts,
                                   detail::FreshIdAllocator<EventId>& event_ids,
                                   std::uint8_t staff_count = 1) {
    Measure measure{bar_number, {}, std::nullopt, std::nullopt};
    for (std::uint16_t staff = 0; staff < std::max<std::uint8_t>(staff_count, 1); ++staff) {
        RestEvent rest{ts.measure_duration(), true};
        auto event_id = event_ids.allocate();
        if (!event_id) return std::unexpected(event_id.error());
        Event event{*event_id, Beat::zero(), rest};
        Voice voice{static_cast<std::uint8_t>(staff), {event}, {}};
        voice.staff_index = static_cast<std::uint8_t>(staff);
        measure.voices.push_back(std::move(voice));
    }
    return measure;
}

/// Get the time signature in effect at a given bar
const TimeSignatureEntry* find_time_signature(const Score& score, std::uint32_t bar) {
    const TimeSignatureEntry* result = nullptr;
    for (const auto& entry : score.time_map) {
        if (entry.bar <= bar)
            result = &entry;
        else
            break;
    }
    return result;
}

bool valid_part_point(const Score& score, const Part& part, ScoreTime point, bool allow_terminal) {
    if (point.bar < 1) return false;
    if (allow_terminal && score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max() &&
        point.bar == score.metadata.total_bars + 1)
        return point.beat == Beat::zero();
    if (point.bar > score.metadata.total_bars || point.bar > part.measures.size()) return false;
    const auto& measure = part.measures[point.bar - 1];
    const auto* signature = find_time_signature(score, point.bar);
    if (!measure.local_time && !signature) return false;
    const Beat duration = measure.local_time ? measure.local_time->measure_duration()
                                             : signature->time_signature.measure_duration();
    return point.beat >= Beat::zero() && point.beat < duration;
}

bool valid_part_span(const Score& score, const Part& part, ScoreTime start, ScoreTime end) {
    return valid_part_point(score, part, start, false) &&
           valid_part_point(score, part, end, true) && start < end;
}

bool valid_global_point(const Score& score, ScoreTime point) {
    if (point.bar < 1 || point.bar > score.metadata.total_bars) return false;
    const auto* signature = find_time_signature(score, point.bar);
    if (!signature) return false;
    auto duration = checked_measure_duration(signature->time_signature);
    return duration && point.beat >= Beat::zero() && point.beat < *duration;
}

bool valid_score_region(const Score& score, const ScoreRegion& region) {
    if (!valid_global_point(score, region.start) || !(region.start < region.end) ||
        score.metadata.total_bars == std::numeric_limits<std::uint32_t>::max())
        return false;
    const bool valid_end =
        valid_global_point(score, region.end) ||
        (region.end.bar == score.metadata.total_bars + 1 && region.end.beat == Beat::zero());
    if (!valid_end) return false;

    std::set<std::uint64_t> seen_parts;
    for (const auto part_id : region.parts) {
        if (!seen_parts.insert(part_id.value).second) return false;
        if (std::none_of(score.parts.begin(), score.parts.end(), [part_id](const Part& part) {
                return part.id == part_id;
            }))
            return false;
    }
    return true;
}

bool valid_dynamic_level(DynamicLevel level) {
    return static_cast<std::uint8_t>(level) <= static_cast<std::uint8_t>(DynamicLevel::rfz);
}

bool valid_articulation_type(ArticulationType articulation) {
    return static_cast<std::uint8_t>(articulation) <=
           static_cast<std::uint8_t>(ArticulationType::BendDown);
}

bool valid_velocity_value(const VelocityValue& velocity) {
    return velocity.value <= 127 && (!velocity.written || valid_dynamic_level(*velocity.written));
}

bool valid_note_payload(const Note& note) {
    if (note.pitch.letter > 6 || !valid_velocity_value(note.velocity) ||
        note.release_velocity > 127 ||
        (note.articulation && !valid_articulation_type(*note.articulation)) ||
        (note.dynamic && !valid_dynamic_level(*note.dynamic)) ||
        (note.grace && static_cast<std::uint8_t>(*note.grace) >
                           static_cast<std::uint8_t>(GraceType::Appoggiatura)) ||
        (note.notation_head && static_cast<std::uint8_t>(*note.notation_head) >
                                   static_cast<std::uint8_t>(NoteHeadType::Cue)))
        return false;
    if (note.ornament && (static_cast<std::uint8_t>(note.ornament->type) >
                              static_cast<std::uint8_t>(OrnamentType::Arpeggio) ||
                          static_cast<std::uint8_t>(note.ornament->arpeggio_direction) >
                              static_cast<std::uint8_t>(ArpeggioDirection::None)))
        return false;
    for (const auto& technical : note.technical)
        if (static_cast<std::uint8_t>(technical.type) >
                static_cast<std::uint8_t>(TechnicalDirection::Type::Caesura) ||
            static_cast<std::uint8_t>(technical.slide_direction) >
                static_cast<std::uint8_t>(SlideDirection::Descending) ||
            static_cast<std::uint8_t>(technical.vibrato_speed) >
                static_cast<std::uint8_t>(VibratoSpeed::None))
            return false;
    std::uint16_t previous_verse = 0;
    for (const auto& lyric : note.lyrics) {
        if (lyric.text.empty() || lyric.verse == 0 || lyric.verse <= previous_verse ||
            static_cast<std::uint8_t>(lyric.syllabic) >
                static_cast<std::uint8_t>(LyricSyllabic::End) ||
            (lyric.extend && lyric.syllabic != LyricSyllabic::Single &&
             lyric.syllabic != LyricSyllabic::End))
            return false;
        previous_verse = lyric.verse;
    }
    return true;
}

/// Iterate events in a region, calling callback for each
template <typename Fn>
void for_each_event_in_region(Score& score, const ScoreRegion& region, Fn&& fn) {
    for (auto& part : score.parts) {
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) {
                    found = true;
                    break;
                }
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

/// Record a pre-mutation snapshot when the caller supplied an undo stack.
void push_snapshot(UndoStack* undo, std::optional<Score>&& before, std::string desc) {
    if (undo && before) undo->record(std::move(*before), std::move(desc));
}

/// Floor of a non-negative rational.
std::int64_t floor_beat(Beat value) {
    return value.numerator() / value.denominator();
}

/**
 * Prepare a tuplet for a note of non-dyadic duration d = 1 / (2^a m), m odd.
 *
 * The conventional ratio is m : n with n the largest power of two below m; the
 * tuplet span is S = m d and its normal type S / n (1/12 gives 3:2 eighths
 * over a quarter). Structural rule S8 counts one direct event per tuplet
 * unit, so the span [T, T + S), aligned to multiples of S from the bar start,
 * becomes m unit rests under one fresh context and the note later replaces
 * the unit it covers. Nothing changes when d is dyadic, when d is not a unit
 * fraction, when the span leaves the bar or does not align with the note, or
 * when any part of the span is not plain untupleted silence; those cases keep
 * the previous behaviour and the exporters derive the written form.
 */
Result<void> materialise_tuplet_span(std::vector<Event>& events,
                                     Beat offset,
                                     Beat duration,
                                     Beat measure_duration,
                                     detail::FreshIdAllocator<EventId>& event_ids,
                                     const Score& score) {
    std::int64_t odd = duration.denominator();
    while (odd % 2 == 0)
        odd /= 2;
    if (odd == 1 || duration.numerator() != 1 || odd > 255) return {};
    std::int64_t normal = 1;
    while (normal * 2 < odd)
        normal *= 2;

    const auto span = checked_mul(duration, Beat{odd, 1});
    if (!span) return std::unexpected(span.error());
    const auto span_index = checked_div(offset, *span);
    if (!span_index) return std::unexpected(span_index.error());
    const Beat span_start = Beat{floor_beat(*span_index), 1} * *span;
    const Beat span_end = span_start + *span;
    if (span_end > measure_duration) return {};
    const auto unit_index = checked_div(offset - span_start, duration);
    if (!unit_index || unit_index->denominator() != 1) return {};

    std::vector<Event> kept;
    std::optional<bool> visible;
    for (const auto& event : events) {
        const Beat event_end = event.offset + event.duration();
        const bool overlaps =
            event.duration() > Beat::zero() && event.offset < span_end && span_start < event_end;
        if (!overlaps) {
            kept.push_back(event);
            continue;
        }
        const auto* rest = event.as_rest();
        if (!rest || rest->tuplet_context) return {};
        if (!visible) visible = rest->visible;
        if (event.offset < span_start) {
            Event prefix = event;
            prefix.payload = RestEvent{span_start - event.offset, rest->visible, std::nullopt};
            kept.push_back(std::move(prefix));
        }
        if (span_end < event_end) {
            Event suffix = event;
            if (event.offset < span_start) {
                auto id = event_ids.allocate();
                if (!id) return std::unexpected(id.error());
                suffix.id = *id;
            }
            suffix.offset = span_end;
            suffix.payload = RestEvent{event_end - span_end, rest->visible, std::nullopt};
            kept.push_back(std::move(suffix));
        }
    }
    if (!visible) return {};

    detail::FreshIdAllocator<TupletId> tuplet_ids;
    for (const auto& part : score.parts)
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                for (const auto& event : voice.events)
                    if (const auto* context = event_tuplet_context(event))
                        tuplet_ids.include(context->id);
    auto tuplet_id = tuplet_ids.allocate();
    if (!tuplet_id) return std::unexpected(tuplet_id.error());
    const TupletContext context{*tuplet_id,
                                static_cast<std::uint8_t>(odd),
                                static_cast<std::uint8_t>(normal),
                                *span / Beat{normal, 1},
                                std::nullopt};
    for (std::int64_t unit = 0; unit < odd; ++unit) {
        auto id = event_ids.allocate();
        if (!id) return std::unexpected(id.error());
        kept.push_back(Event{
            *id, span_start + duration * Beat{unit, 1}, RestEvent{duration, *visible, context}});
    }
    std::stable_sort(kept.begin(), kept.end(), [](const Event& lhs, const Event& rhs) {
        return lhs.offset < rhs.offset;
    });
    events = std::move(kept);
    return {};
}

/**
 * Lay a copied tuplet's member layout over plain silence in a target voice.
 *
 * Doubling copies notes that belong to a tuplet; insert_group_replacing_rests
 * places a note only over rests of the same tuplet, so the target first needs
 * the source tuplet's units as rests under a fresh context. The span covered by
 * the layout must be untupleted rests; anything else is a target that cannot
 * receive the copy and the caller refuses the whole doubling.
 */
Result<void> lay_tuplet_rests(std::vector<Event>& events,
                              const std::vector<std::pair<Beat, Beat>>& layout,
                              const TupletContext& context,
                              detail::FreshIdAllocator<EventId>& event_ids) {
    if (layout.empty()) return std::unexpected(ErrorCode::InvalidMutation);
    Beat span_start = layout.front().first;
    Beat span_end = layout.front().first + layout.front().second;
    for (const auto& [offset, duration] : layout) {
        span_start = std::min(span_start, offset);
        span_end = std::max(span_end, offset + duration);
    }

    std::vector<Event> kept;
    std::optional<bool> visible;
    for (const auto& event : events) {
        const Beat event_end = event.offset + event.duration();
        const bool overlaps =
            event.duration() > Beat::zero() && event.offset < span_end && span_start < event_end;
        if (!overlaps) {
            kept.push_back(event);
            continue;
        }
        const auto* rest = event.as_rest();
        if (!rest || rest->tuplet_context) return std::unexpected(ErrorCode::InvalidMutation);
        if (!visible) visible = rest->visible;
        if (event.offset < span_start) {
            Event prefix = event;
            prefix.payload = RestEvent{span_start - event.offset, rest->visible, std::nullopt};
            kept.push_back(std::move(prefix));
        }
        if (span_end < event_end) {
            Event suffix = event;
            if (event.offset < span_start) {
                auto id = event_ids.allocate();
                if (!id) return std::unexpected(id.error());
                suffix.id = *id;
            }
            suffix.offset = span_end;
            suffix.payload = RestEvent{event_end - span_end, rest->visible, std::nullopt};
            kept.push_back(std::move(suffix));
        }
    }
    if (!visible) return std::unexpected(ErrorCode::InvalidMutation);

    for (const auto& [offset, duration] : layout) {
        auto id = event_ids.allocate();
        if (!id) return std::unexpected(id.error());
        kept.push_back(Event{*id, offset, RestEvent{duration, *visible, context}});
    }
    std::stable_sort(kept.begin(), kept.end(), [](const Event& lhs, const Event& rhs) {
        return lhs.offset < rhs.offset;
    });
    events = std::move(kept);
    return {};
}

/// apply_interval asserts when the result leaves the SpelledPitch octave or
/// accidental domain; a mutation rejects such input instead.
Result<SpelledPitch> checked_apply_interval(SpelledPitch pitch, DiatonicInterval interval) {
    const long long letter_sum = static_cast<long long>(pitch.letter) + interval.diatonic;
    const long long octave_offset = letter_sum >= 0 ? letter_sum / 7 : (letter_sum - 6) / 7;
    const long long octave = static_cast<long long>(pitch.octave) + octave_offset;
    const long long letter = letter_sum - 7 * octave_offset;
    const long long target = static_cast<long long>(midi_value(pitch)) + interval.chromatic;
    const long long natural =
        12 * (octave + 1) + static_cast<long long>(nat(static_cast<std::uint8_t>(letter)));
    const long long accidental = target - natural;
    constexpr long long low = std::numeric_limits<std::int8_t>::min();
    constexpr long long high = std::numeric_limits<std::int8_t>::max();
    if (octave < low || octave > high || accidental < low || accidental > high)
        return std::unexpected(ErrorCode::InvalidMutation);
    return apply_interval(pitch, interval);
}

/// Mark a region as stale for harmonic re-analysis
void mark_harmonic_stale(Score& score, std::uint32_t bar) {
    score.stale_harmonic_regions.push_back(
        ScoreRegion{ScoreTime{bar, Beat::zero()}, ScoreTime{bar + 1, Beat::zero()}, {}});
}

/// Mark a region as stale for orchestration re-analysis
void mark_orchestration_stale(Score& score, const ScoreRegion& region) {
    score.stale_orchestration_regions.push_back(region);
}

void shift_position_after_bar(ScoreTime& position, std::uint32_t after_bar, std::uint32_t count) {
    if (position.bar > after_bar) position.bar += count;
}

void shift_region_after_bar(ScoreRegion& region, std::uint32_t after_bar, std::uint32_t count) {
    shift_position_after_bar(region.start, after_bar, count);
    shift_position_after_bar(region.end, after_bar, count);
}

void shift_section_tree_after_bar(std::vector<ScoreSection>& sections,
                                  std::uint32_t after_bar,
                                  std::uint32_t count) {
    for (auto& section : sections) {
        shift_position_after_bar(section.start, after_bar, count);
        shift_position_after_bar(section.end, after_bar, count);
        shift_section_tree_after_bar(section.children, after_bar, count);
    }
}

ScoreTime collapse_position_for_deleted_bars(ScoreTime position,
                                             std::uint32_t first_bar,
                                             std::uint32_t count) {
    const std::uint32_t source_splice_bar = first_bar + count;
    if (position.bar >= source_splice_bar) {
        position.bar -= count;
    } else if (position.bar >= first_bar) {
        position = ScoreTime{first_bar, Beat::zero()};
    }
    return position;
}

void splice_section_tree_for_deleted_bars(std::vector<ScoreSection>& sections,
                                          std::uint32_t first_bar,
                                          std::uint32_t count) {
    for (auto& section : sections) {
        section.start = collapse_position_for_deleted_bars(section.start, first_bar, count);
        section.end = collapse_position_for_deleted_bars(section.end, first_bar, count);
        splice_section_tree_for_deleted_bars(section.children, first_bar, count);
    }
    std::erase_if(sections,
                  [](const ScoreSection& section) { return !(section.start < section.end); });
}

template <typename Span>
void splice_spans_for_deleted_bars(std::vector<Span>& spans,
                                   std::uint32_t first_bar,
                                   std::uint32_t count) {
    for (auto& span : spans) {
        span.start = collapse_position_for_deleted_bars(span.start, first_bar, count);
        span.end = collapse_position_for_deleted_bars(span.end, first_bar, count);
    }
    std::erase_if(spans, [](const Span& span) { return !(span.start < span.end); });
}

Result<void> recompute_linear_tempo_durations(TempoMap& tempo_map,
                                              const TimeSignatureMap& time_map) {
    for (std::size_t i = 1; i < tempo_map.size(); ++i) {
        auto& event = tempo_map[i];
        if (event.transition_type != TempoTransitionType::Linear) continue;
        auto previous = score_time_to_absolute_beat(tempo_map[i - 1].position, time_map);
        if (!previous) return std::unexpected(previous.error());
        auto current = score_time_to_absolute_beat(event.position, time_map);
        if (!current) return std::unexpected(current.error());
        auto duration = checked_sub(*current, *previous);
        if (!duration || *duration <= Beat::zero())
            return std::unexpected(duration ? ErrorCode::InvalidMutation : duration.error());
        event.linear_duration = *duration;
    }
    return {};
}

struct NoteSpanPairs {
    std::map<std::uint64_t, std::uint64_t> slur_start_to_end;
    std::map<std::uint64_t, std::uint64_t> slur_end_to_start;
    std::map<std::uint64_t, std::uint64_t> gliss_start_to_end;
    std::map<std::uint64_t, std::uint64_t> gliss_end_to_start;
};

NoteSpanPairs collect_note_span_pairs(const Score& score) {
    NoteSpanPairs pairs;
    for (const auto& part : score.parts) {
        std::map<std::uint8_t, std::optional<EventId>> active_slurs;
        std::map<std::uint8_t, std::optional<EventId>> active_glissandi;
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                auto& active_slur = active_slurs[voice.voice_index];
                auto& active_gliss = active_glissandi[voice.voice_index];
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group) continue;

                    if (group->slur_end && active_slur) {
                        pairs.slur_start_to_end[active_slur->value] = event.id.value;
                        pairs.slur_end_to_start[event.id.value] = active_slur->value;
                        active_slur.reset();
                    }
                    if (group->slur_start) active_slur = event.id;

                    const bool gliss_end = std::ranges::any_of(group->notes, [](const Note& note) {
                        return note.articulation == ArticulationType::GlissandoEnd;
                    });
                    const bool gliss_start =
                        std::ranges::any_of(group->notes, [](const Note& note) {
                            return note.articulation == ArticulationType::GlissandoStart;
                        });
                    if (gliss_end && active_gliss) {
                        pairs.gliss_start_to_end[active_gliss->value] = event.id.value;
                        pairs.gliss_end_to_start[event.id.value] = active_gliss->value;
                        active_gliss.reset();
                    }
                    if (gliss_start) active_gliss = event.id;
                }
            }
        }
    }
    return pairs;
}

void strip_unselected_note_span_endpoints(NoteGroup& group,
                                          EventId source_id,
                                          const std::set<std::uint64_t>& selected,
                                          const NoteSpanPairs& pairs) {
    const auto mate_is_selected = [&](const auto& mapping) {
        const auto mate = mapping.find(source_id.value);
        return mate != mapping.end() && selected.contains(mate->second);
    };
    if (group.slur_start && !mate_is_selected(pairs.slur_start_to_end)) group.slur_start = false;
    if (group.slur_end && !mate_is_selected(pairs.slur_end_to_start)) group.slur_end = false;

    const bool retain_gliss_start = mate_is_selected(pairs.gliss_start_to_end);
    const bool retain_gliss_end = mate_is_selected(pairs.gliss_end_to_start);
    for (auto& note : group.notes) {
        if (note.articulation == ArticulationType::GlissandoStart && !retain_gliss_start)
            note.articulation = std::nullopt;
        if (note.articulation == ArticulationType::GlissandoEnd && !retain_gliss_end)
            note.articulation = std::nullopt;
    }
}

struct DirectionSpanPairs {
    std::map<std::uint64_t, std::uint64_t> mates;
    std::map<std::uint64_t, std::pair<ScoreTime, ScoreTime>> pedal_ranges;
};

DirectionSpanPairs collect_direction_span_pairs(const Score& score) {
    struct Endpoint {
        ScoreTime position;
        std::uint8_t voice_index;
        std::size_t event_order;
        std::uint8_t staff_index;
        EventId id;
        DirectionType type;
    };

    DirectionSpanPairs pairs;
    for (const auto& part : score.parts) {
        std::map<std::uint8_t, std::vector<Endpoint>> ottavas;
        std::vector<Endpoint> pedals;
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (std::size_t order = 0; order < voice.events.size(); ++order) {
                    const auto& event = voice.events[order];
                    const auto* direction = std::get_if<ScoreDirection>(&event.payload);
                    if (!direction) continue;
                    Endpoint endpoint{ScoreTime{measure.bar_number, event.offset},
                                      voice.voice_index,
                                      order,
                                      voice.staff_index,
                                      event.id,
                                      direction->type};
                    if (direction->type == DirectionType::OttavaStart ||
                        direction->type == DirectionType::OttavaEnd)
                        ottavas[voice.staff_index].push_back(endpoint);
                    if (direction->type == DirectionType::PedalDown ||
                        direction->type == DirectionType::PedalUp)
                        pedals.push_back(endpoint);
                }
            }
        }

        const auto sort_endpoints = [](std::vector<Endpoint>& endpoints) {
            std::stable_sort(
                endpoints.begin(), endpoints.end(), [](const Endpoint& lhs, const Endpoint& rhs) {
                    if (lhs.position != rhs.position) return lhs.position < rhs.position;
                    if (lhs.voice_index != rhs.voice_index)
                        return lhs.voice_index < rhs.voice_index;
                    return lhs.event_order < rhs.event_order;
                });
        };

        for (auto& [staff, endpoints] : ottavas) {
            (void)staff;
            sort_endpoints(endpoints);
            std::optional<EventId> active;
            for (const auto& endpoint : endpoints) {
                if (endpoint.type == DirectionType::OttavaStart) {
                    active = endpoint.id;
                } else if (active) {
                    pairs.mates[active->value] = endpoint.id.value;
                    pairs.mates[endpoint.id.value] = active->value;
                    active.reset();
                }
            }
        }

        sort_endpoints(pedals);
        std::optional<Endpoint> active_pedal;
        for (const auto& endpoint : pedals) {
            if (endpoint.type == DirectionType::PedalDown) {
                active_pedal = endpoint;
            } else if (active_pedal) {
                pairs.mates[active_pedal->id.value] = endpoint.id.value;
                pairs.mates[endpoint.id.value] = active_pedal->id.value;
                const auto range = std::pair{active_pedal->position, endpoint.position};
                pairs.pedal_ranges[active_pedal->id.value] = range;
                pairs.pedal_ranges[endpoint.id.value] = range;
                active_pedal.reset();
            }
        }
        if (active_pedal && score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max())
            pairs.pedal_ranges[active_pedal->id.value] = {
                active_pedal->position, ScoreTime{score.metadata.total_bars + 1, Beat::zero()}};
    }
    return pairs;
}

void clear_note_endpoint(Score& score,
                         std::uint64_t event_value,
                         bool slur_start,
                         bool slur_end,
                         std::optional<ArticulationType> glissando) {
    auto location = find_event(score, EventId{event_value});
    if (!location.event) return;
    auto* group = std::get_if<NoteGroup>(&location.event->payload);
    if (!group) return;
    if (slur_start) group->slur_start = false;
    if (slur_end) group->slur_end = false;
    if (glissando) {
        for (auto& note : group->notes)
            if (note.articulation == *glissando) note.articulation = std::nullopt;
    }
}

void erase_event_without_repair(Score& score, EventId id) {
    for (auto& part : score.parts)
        for (auto& measure : part.measures)
            for (auto& voice : measure.voices)
                std::erase_if(voice.events, [&](const Event& event) { return event.id == id; });
}

bool delete_event_content_and_repair_spans(Score& score, EventId id) {
    auto location = find_event(score, id);
    if (!location.event) return false;

    const PartId part_id = location.part->id;
    const Beat duration = location.event->duration();
    std::optional<TupletContext> tuplet_context;
    if (const auto* group = location.event->as_note_group())
        tuplet_context = group->tuplet_context;
    else if (const auto* rest = location.event->as_rest())
        tuplet_context = rest->tuplet_context;

    if (const auto* group = location.event->as_note_group()) {
        const auto pairs = collect_note_span_pairs(score);
        if (group->slur_start) {
            if (const auto mate = pairs.slur_start_to_end.find(id.value);
                mate != pairs.slur_start_to_end.end())
                clear_note_endpoint(score, mate->second, false, true, std::nullopt);
        }
        if (group->slur_end) {
            if (const auto mate = pairs.slur_end_to_start.find(id.value);
                mate != pairs.slur_end_to_start.end())
                clear_note_endpoint(score, mate->second, true, false, std::nullopt);
        }
        const bool gliss_start = std::ranges::any_of(group->notes, [](const Note& note) {
            return note.articulation == ArticulationType::GlissandoStart;
        });
        const bool gliss_end = std::ranges::any_of(group->notes, [](const Note& note) {
            return note.articulation == ArticulationType::GlissandoEnd;
        });
        if (gliss_start) {
            if (const auto mate = pairs.gliss_start_to_end.find(id.value);
                mate != pairs.gliss_start_to_end.end())
                clear_note_endpoint(
                    score, mate->second, false, false, ArticulationType::GlissandoEnd);
        }
        if (gliss_end) {
            if (const auto mate = pairs.gliss_end_to_start.find(id.value);
                mate != pairs.gliss_end_to_start.end())
                clear_note_endpoint(
                    score, mate->second, false, false, ArticulationType::GlissandoStart);
        }
    }

    if (const auto* direction = std::get_if<ScoreDirection>(&location.event->payload);
        direction && (direction->type == DirectionType::OttavaStart ||
                      direction->type == DirectionType::OttavaEnd ||
                      direction->type == DirectionType::PedalDown ||
                      direction->type == DirectionType::PedalUp)) {
        const auto pairs = collect_direction_span_pairs(score);
        if (const auto mate = pairs.mates.find(id.value); mate != pairs.mates.end())
            erase_event_without_repair(score, EventId{mate->second});
        if (const auto range = pairs.pedal_ranges.find(id.value);
            range != pairs.pedal_ranges.end()) {
            if (auto* part = find_part(score, part_id))
                std::erase_if(part->part_directives, [&](const PartDirective& directive) {
                    return directive.directive == DirectiveType::SustainingPedal &&
                           directive.start == range->second.first &&
                           directive.end == range->second.second;
                });
        }
    }

    location = find_event(score, id); // A paired Direction erase may invalidate vector pointers.
    if (!location.event) return true;
    if (duration > Beat::zero()) {
        location.event->payload = RestEvent{duration, true, tuplet_context};
    } else {
        erase_event_without_repair(score, id);
    }
    return true;
}

bool same_tuplet_context(const std::optional<TupletContext>& lhs,
                         const std::optional<TupletContext>& rhs) {
    if (lhs.has_value() != rhs.has_value()) return false;
    if (!lhs) return true;
    return lhs->id == rhs->id && lhs->actual == rhs->actual && lhs->normal == rhs->normal &&
           lhs->normal_type == rhs->normal_type && lhs->nested_in == rhs->nested_in;
}

bool has_glissando_endpoint(const NoteGroup& group) {
    return std::ranges::any_of(group.notes, [](const Note& note) {
        return note.articulation == ArticulationType::GlissandoStart ||
               note.articulation == ArticulationType::GlissandoEnd;
    });
}

Result<void> insert_group_replacing_rests(Voice& voice,
                                          Beat measure_duration,
                                          Event inserted,
                                          detail::FreshIdAllocator<EventId>& event_ids,
                                          std::optional<EventId> replacing = std::nullopt) {
    auto* inserted_group = std::get_if<NoteGroup>(&inserted.payload);
    auto* inserted_rest = std::get_if<RestEvent>(&inserted.payload);
    if ((!inserted_group && !inserted_rest) || inserted.offset < Beat::zero() ||
        inserted.duration() <= Beat::zero())
        return std::unexpected(ErrorCode::InvalidMutation);
    auto insertion_end = checked_add(inserted.offset, inserted.duration());
    if (!insertion_end || inserted.offset >= measure_duration || *insertion_end > measure_duration)
        return std::unexpected(ErrorCode::InvalidMutation);

    const EventId inserted_id = inserted.id;
    const auto inserted_context =
        inserted_group ? inserted_group->tuplet_context : inserted_rest->tuplet_context;

    std::vector<Event> replacement;
    replacement.reserve(voice.events.size() + 2);
    bool consumed = false;
    bool found_replaced = false;
    std::optional<std::optional<TupletContext>> covered_context;
    for (const auto& existing : voice.events) {
        auto existing_end = checked_add(existing.offset, existing.duration());
        if (!existing_end) return std::unexpected(existing_end.error());
        const bool overlaps = existing.duration() > Beat::zero() &&
                              existing.offset < *insertion_end && inserted.offset < *existing_end;
        if (!overlaps) {
            replacement.push_back(existing);
            continue;
        }

        const bool is_replaced = replacing && existing.id == *replacing;
        if (is_replaced) found_replaced = true;

        const auto* existing_group = existing.as_note_group();
        if (existing_group && !is_replaced) {
            if (!inserted_group) return std::unexpected(ErrorCode::InvalidMutation);
            if (consumed || existing.offset != inserted.offset ||
                existing_group->duration != inserted_group->duration ||
                !same_tuplet_context(existing_group->tuplet_context,
                                     inserted_group->tuplet_context) ||
                existing_group->slur_start || existing_group->slur_end ||
                inserted_group->slur_start || inserted_group->slur_end ||
                has_glissando_endpoint(*existing_group) || has_glissando_endpoint(*inserted_group))
                return std::unexpected(ErrorCode::InvalidMutation);
            Event chord = existing;
            auto& chord_group = std::get<NoteGroup>(chord.payload);
            chord_group.notes.insert(chord_group.notes.end(),
                                     inserted_group->notes.begin(),
                                     inserted_group->notes.end());
            replacement.push_back(std::move(chord));
            consumed = true;
            continue;
        }

        const auto* rest = existing.as_rest();
        const auto existing_context = rest ? rest->tuplet_context : existing_group->tuplet_context;
        const bool visible = rest ? rest->visible : true;
        if (!covered_context) {
            covered_context = existing_context;
        } else if (!same_tuplet_context(*covered_context, existing_context)) {
            return std::unexpected(ErrorCode::InvalidMutation);
        }
        if (existing.offset < inserted.offset) {
            Event prefix = existing;
            if (prefix.id == inserted_id) {
                auto event_id = event_ids.allocate();
                if (!event_id) return std::unexpected(event_id.error());
                prefix.id = *event_id;
            }
            prefix.payload =
                RestEvent{inserted.offset - existing.offset, visible, existing_context};
            replacement.push_back(std::move(prefix));
        }
        if (*insertion_end < *existing_end) {
            Event suffix = existing;
            if (existing.offset < inserted.offset || suffix.id == inserted_id) {
                auto event_id = event_ids.allocate();
                if (!event_id) return std::unexpected(event_id.error());
                suffix.id = *event_id;
            }
            suffix.offset = *insertion_end;
            suffix.payload = RestEvent{*existing_end - *insertion_end, visible, existing_context};
            replacement.push_back(std::move(suffix));
        }
    }

    if (replacing && !found_replaced) return std::unexpected(ErrorCode::InvalidMutation);
    if (!consumed) {
        if (!covered_context || !same_tuplet_context(*covered_context, inserted_context))
            return std::unexpected(ErrorCode::InvalidMutation);
        replacement.push_back(std::move(inserted));
    }
    std::stable_sort(replacement.begin(),
                     replacement.end(),
                     [](const Event& lhs, const Event& rhs) { return lhs.offset < rhs.offset; });

    Beat cursor = Beat::zero();
    for (const auto& event : replacement) {
        if (event.duration() == Beat::zero()) continue;
        if (event.offset != cursor) return std::unexpected(ErrorCode::InvalidMutation);
        auto next = checked_add(cursor, event.duration());
        if (!next) return std::unexpected(next.error());
        cursor = *next;
    }
    if (cursor != measure_duration) return std::unexpected(ErrorCode::InvalidMutation);
    voice.events = std::move(replacement);
    return {};
}

Result<void> retile_voice_to_measure_duration(Voice& voice,
                                              Beat measure_duration,
                                              detail::FreshIdAllocator<EventId>& event_ids) {
    if (measure_duration <= Beat::zero()) return std::unexpected(ErrorCode::InvalidTimeSignature);

    std::vector<Event> replacement;
    replacement.reserve(voice.events.size() + 1);
    for (const auto& event : voice.events) {
        if (event.duration() == Beat::zero()) {
            if (event.offset < Beat::zero() || event.offset >= measure_duration)
                return std::unexpected(ErrorCode::InvalidMutation);
            replacement.push_back(event);
            continue;
        }

        auto event_end = checked_add(event.offset, event.duration());
        if (!event_end) return std::unexpected(event_end.error());
        if (event.offset >= measure_duration) {
            if (!event.is_rest()) return std::unexpected(ErrorCode::InvalidMutation);
            continue;
        }
        if (*event_end > measure_duration) {
            const auto* rest = event.as_rest();
            if (!rest) return std::unexpected(ErrorCode::InvalidMutation);
            Event trimmed = event;
            trimmed.payload =
                RestEvent{measure_duration - event.offset, rest->visible, rest->tuplet_context};
            replacement.push_back(std::move(trimmed));
            continue;
        }
        replacement.push_back(event);
    }

    Beat cursor = Beat::zero();
    Event* trailing_rest = nullptr;
    for (auto& event : replacement) {
        if (event.duration() == Beat::zero()) continue;
        if (event.offset != cursor) return std::unexpected(ErrorCode::InvalidMutation);
        auto end = checked_add(cursor, event.duration());
        if (!end) return std::unexpected(end.error());
        cursor = *end;
        trailing_rest = event.as_rest() ? &event : nullptr;
    }
    if (cursor > measure_duration) return std::unexpected(ErrorCode::InvalidMutation);
    if (cursor < measure_duration) {
        const Beat extension = measure_duration - cursor;
        if (trailing_rest) {
            auto& rest = std::get<RestEvent>(trailing_rest->payload);
            auto duration = checked_add(rest.duration, extension);
            if (!duration) return std::unexpected(duration.error());
            rest.duration = *duration;
        } else {
            auto event_id = event_ids.allocate();
            if (!event_id) return std::unexpected(event_id.error());
            replacement.push_back(Event{*event_id, cursor, RestEvent{extension, true}});
        }
    }

    std::stable_sort(replacement.begin(),
                     replacement.end(),
                     [](const Event& lhs, const Event& rhs) { return lhs.offset < rhs.offset; });
    voice.events = std::move(replacement);
    return {};
}

bool strip_voice_span_metadata(NoteGroup& group) {
    bool stripped = group.slur_start || group.slur_end || group.beam_group.has_value();
    group.slur_start = false;
    group.slur_end = false;
    group.beam_group.reset();
    for (auto& note : group.notes) {
        if (note.articulation == ArticulationType::GlissandoStart ||
            note.articulation == ArticulationType::GlissandoEnd) {
            stripped = true;
            note.articulation = std::nullopt;
        }
    }
    return stripped;
}

std::set<std::uint64_t> beam_group_ids_touching(const Voice& voice,
                                                const std::set<std::uint64_t>& event_ids) {
    std::set<std::uint64_t> result;
    for (const auto& group : voice.beam_groups) {
        if (std::ranges::any_of(group.event_ids, [&](EventId event_id) {
                return event_ids.contains(event_id.value);
            }))
            result.insert(group.id.value);
    }
    return result;
}

void remove_beam_groups(Voice& voice, const std::set<std::uint64_t>& group_ids) {
    if (group_ids.empty()) return;
    std::erase_if(voice.beam_groups,
                  [&](const BeamGroup& group) { return group_ids.contains(group.id.value); });
    for (auto& event : voice.events) {
        auto* group = std::get_if<NoteGroup>(&event.payload);
        if (group && group->beam_group && group_ids.contains(group->beam_group->value))
            group->beam_group.reset();
    }
}

bool remove_beam_groups_touching(Score& score, const std::set<std::uint64_t>& event_ids) {
    bool removed = false;
    for (auto& part : score.parts) {
        for (auto& measure : part.measures) {
            for (auto& voice : measure.voices) {
                const auto group_ids = beam_group_ids_touching(voice, event_ids);
                removed = removed || !group_ids.empty();
                remove_beam_groups(voice, group_ids);
            }
        }
    }
    return removed;
}

bool remove_note_spans_touching(Score& score, const std::set<std::uint64_t>& selected) {
    const auto pairs = collect_note_span_pairs(score);
    bool removed = false;
    for (const auto& [start, end] : pairs.slur_start_to_end) {
        if (!selected.contains(start) && !selected.contains(end)) continue;
        clear_note_endpoint(score, start, true, false, std::nullopt);
        clear_note_endpoint(score, end, false, true, std::nullopt);
        removed = true;
    }
    for (const auto& [start, end] : pairs.gliss_start_to_end) {
        if (!selected.contains(start) && !selected.contains(end)) continue;
        clear_note_endpoint(score, start, false, false, ArticulationType::GlissandoStart);
        clear_note_endpoint(score, end, false, false, ArticulationType::GlissandoEnd);
        removed = true;
    }
    return removed;
}

} // anonymous namespace

// =============================================================================
// Event-Level Mutations
// =============================================================================

Result<MutationResult> insert_note(Score& score,
                                   PartId part_id,
                                   std::uint32_t bar,
                                   std::uint8_t voice_index,
                                   Beat offset,
                                   Note note,
                                   Beat duration,
                                   UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_note_payload(note)) return std::unexpected(ErrorCode::InvalidMutation);
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

    if (offset < Beat::zero() || duration <= Beat::zero())
        return std::unexpected(ErrorCode::InvalidMutation);
    const auto measure_duration = checked_measure_duration(
        measure.local_time ? *measure.local_time : query_time_signature_at(score, bar));
    auto insertion_end = checked_add(offset, duration);
    if (!measure_duration || !insertion_end || offset >= *measure_duration ||
        *insertion_end > *measure_duration)
        return std::unexpected(ErrorCode::InvalidMutation);

    auto event_ids = detail::event_id_allocator(score);

    // A tuplet duration landing in plain silence first divides its tuplet
    // span into unit rests under a fresh context, so the note joins a
    // complete, marked tuplet instead of floating without a written ratio.
    auto source_events = target_voice->events;
    auto tupleted = materialise_tuplet_span(
        source_events, offset, duration, *measure_duration, event_ids, score);
    if (!tupleted) return std::unexpected(tupleted.error());

    // Construct the complete replacement before mutating the document. A
    // note may replace silence or join an exactly coincident NoteGroup; it
    // may not create partially overlapping musical events.
    std::vector<Event> replacement;
    replacement.reserve(source_events.size() + 2);
    bool merged_into_chord = false;
    std::optional<std::optional<TupletContext>> replacement_tuplet;
    std::set<std::uint64_t> overlapped_event_ids;
    for (const auto& existing : source_events) {
        const Beat existing_end = existing.offset + existing.duration();
        const bool overlaps = existing.duration() > Beat::zero() &&
                              existing.offset < *insertion_end && offset < existing_end;
        if (!overlaps) {
            replacement.push_back(existing);
            continue;
        }
        overlapped_event_ids.insert(existing.id.value);

        if (existing.is_note_group()) {
            if (merged_into_chord || existing.offset != offset || existing.duration() != duration)
                return std::unexpected(ErrorCode::InvalidMutation);
            Event chord = existing;
            std::get<NoteGroup>(chord.payload).notes.push_back(note);
            replacement.push_back(std::move(chord));
            merged_into_chord = true;
            continue;
        }

        const auto* rest = existing.as_rest();
        if (!rest) return std::unexpected(ErrorCode::InvalidMutation);
        if (!replacement_tuplet) {
            replacement_tuplet = rest->tuplet_context;
        } else {
            const auto same_context = [](const std::optional<TupletContext>& lhs,
                                         const std::optional<TupletContext>& rhs) {
                if (lhs.has_value() != rhs.has_value()) return false;
                if (!lhs) return true;
                return lhs->id == rhs->id && lhs->actual == rhs->actual &&
                       lhs->normal == rhs->normal &&
                       lhs->normal_type.numerator() == rhs->normal_type.numerator() &&
                       lhs->normal_type.denominator() == rhs->normal_type.denominator() &&
                       lhs->nested_in == rhs->nested_in;
            };
            if (!same_context(*replacement_tuplet, rest->tuplet_context))
                return std::unexpected(ErrorCode::InvalidMutation);
        }
        if (existing.offset < offset) {
            Event prefix = existing;
            prefix.payload =
                RestEvent{offset - existing.offset, rest->visible, rest->tuplet_context};
            replacement.push_back(std::move(prefix));
        }
        if (*insertion_end < existing_end) {
            Event suffix = existing;
            if (existing.offset < offset) {
                auto event_id = event_ids.allocate();
                if (!event_id) return std::unexpected(event_id.error());
                suffix.id = *event_id;
            }
            suffix.offset = *insertion_end;
            suffix.payload =
                RestEvent{existing_end - *insertion_end, rest->visible, rest->tuplet_context};
            replacement.push_back(std::move(suffix));
        }
    }

    if (!merged_into_chord) {
        NoteGroup group;
        group.notes.push_back(std::move(note));
        group.duration = duration;
        if (replacement_tuplet) group.tuplet_context = *replacement_tuplet;
        auto event_id = event_ids.allocate();
        if (!event_id) return std::unexpected(event_id.error());
        replacement.push_back(Event{*event_id, offset, std::move(group)});
    }
    std::stable_sort(replacement.begin(),
                     replacement.end(),
                     [](const Event& lhs, const Event& rhs) { return lhs.offset < rhs.offset; });

    Beat cursor = Beat::zero();
    for (const auto& event : replacement) {
        if (event.duration() == Beat::zero()) continue;
        if (event.offset != cursor) return std::unexpected(ErrorCode::InvalidMutation);
        cursor = cursor + event.duration();
    }
    if (cursor != *measure_duration) return std::unexpected(ErrorCode::InvalidMutation);

    Score before_state = score;
    const auto removed_beam_groups = beam_group_ids_touching(*target_voice, overlapped_event_ids);
    remove_beam_groups(*target_voice, removed_beam_groups);
    for (auto& event : replacement) {
        auto* group = std::get_if<NoteGroup>(&event.payload);
        if (group && group->beam_group && removed_beam_groups.contains(group->beam_group->value))
            group->beam_group.reset();
    }
    target_voice->events = std::move(replacement);

    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(
        undo, undo ? std::optional<Score>{std::move(before_state)} : std::nullopt, "insert_note");

    mark_harmonic_stale(score, bar);

    MutationResult result;
    if (!removed_beam_groups.empty())
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Note insertion removed an intersected BeamGroup because splitting or merging its "
             "measured members changes primary-beam topology",
             ScoreTime{bar, offset},
             part_id,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult> insert_chord_symbol(Score& score,
                                           PartId part_id,
                                           std::uint32_t bar,
                                           std::uint8_t voice_index,
                                           Beat offset,
                                           ChordSymbolEvent symbol,
                                           UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    Score candidate = score;
    Part* part = find_part(candidate, part_id);
    if (!part || bar < 1 || bar > part->measures.size())
        return std::unexpected(ErrorCode::InvalidMutation);

    auto& measure = part->measures[bar - 1];
    auto voice =
        std::find_if(measure.voices.begin(),
                     measure.voices.end(),
                     [voice_index](const Voice& item) { return item.voice_index == voice_index; });
    if (voice == measure.voices.end()) return std::unexpected(ErrorCode::InvalidMutation);

    const auto duration = checked_measure_duration(
        measure.local_time ? *measure.local_time : query_time_signature_at(candidate, bar));
    if (!duration || offset < Beat::zero() || offset >= *duration)
        return std::unexpected(ErrorCode::InvalidMutation);

    auto event_ids = detail::event_id_allocator(candidate);
    auto event_id = event_ids.allocate();
    if (!event_id) return std::unexpected(event_id.error());
    voice->events.push_back(Event{*event_id, offset, std::move(symbol)});
    std::stable_sort(voice->events.begin(),
                     voice->events.end(),
                     [](const Event& lhs, const Event& rhs) { return lhs.offset < rhs.offset; });

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());

    auto before = undo ? std::optional<Score>{score} : std::nullopt;
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "insert_chord_symbol");
    mark_harmonic_stale(score, bar);
    return MutationResult{{}};
}

Result<MutationResult> delete_event(Score& score, EventId event_id, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    auto loc = find_event(score, event_id);
    if (!loc.event) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

    auto old_bar = loc.measure->bar_number;
    if (!delete_event_content_and_repair_spans(score, event_id))
        return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo, std::move(before), "delete_event");

    mark_harmonic_stale(score, old_bar);

    return MutationResult{{}};
}

Result<MutationResult> modify_pitch(Score& score,
                                    EventId event_id,
                                    std::uint8_t note_index,
                                    SpelledPitch new_pitch,
                                    UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (new_pitch.letter > 6) return std::unexpected(ErrorCode::InvalidMutation);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Score before_state = score;

    ng->notes[note_index].pitch = new_pitch;
    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(
        undo, undo ? std::optional<Score>{std::move(before_state)} : std::nullopt, "modify_pitch");

    mark_harmonic_stale(score, loc.measure->bar_number);

    return MutationResult{{}};
}

Result<MutationResult>
modify_duration(Score& score, EventId event_id, Beat new_duration, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (new_duration <= Beat::zero()) return std::unexpected(ErrorCode::InvalidMutation);

    Score candidate = score;
    auto loc = find_event(candidate, event_id);
    if (!loc.event || (!loc.event->is_note_group() && !loc.event->is_rest()))
        return std::unexpected(ErrorCode::InvalidMutation);

    std::optional<Score> before;
    if (undo) before = score;

    Event replacement = *loc.event;
    if (auto* ng = std::get_if<NoteGroup>(&replacement.payload)) {
        ng->duration = new_duration;
    } else if (auto* r = std::get_if<RestEvent>(&replacement.payload)) {
        r->duration = new_duration;
    }

    const std::uint32_t affected_bar = loc.measure->bar_number;
    const Beat measure_duration =
        loc.measure->local_time
            ? loc.measure->local_time->measure_duration()
            : query_time_signature_at(candidate, affected_bar).measure_duration();
    auto event_ids = detail::event_id_allocator(candidate);
    auto resized = insert_group_replacing_rests(
        *loc.voice, measure_duration, std::move(replacement), event_ids, event_id);
    if (!resized || !is_compilable(candidate))
        return std::unexpected(resized ? ErrorCode::InvalidMutation : resized.error());

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    mark_harmonic_stale(candidate, affected_bar);
    score = std::move(candidate);

    push_snapshot(undo, std::move(before), "modify_duration");

    return MutationResult{{}};
}

Result<MutationResult> modify_velocity(Score& score,
                                       EventId event_id,
                                       std::uint8_t note_index,
                                       VelocityValue new_velocity,
                                       UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_velocity_value(new_velocity)) return std::unexpected(ErrorCode::InvalidMutation);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Score before_state = score;

    ng->notes[note_index].velocity = new_velocity;
    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo,
                  undo ? std::optional<Score>{std::move(before_state)} : std::nullopt,
                  "modify_velocity");

    return MutationResult{{}};
}

Result<MutationResult> modify_release_velocity(Score& score,
                                               EventId event_id,
                                               std::uint8_t note_index,
                                               std::uint8_t new_release_velocity,
                                               UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (new_release_velocity > 127) return std::unexpected(ErrorCode::InvalidMutation);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) return std::unexpected(ErrorCode::InvalidMutation);

    Score before_state = score;
    ng->notes[note_index].release_velocity = new_release_velocity;
    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo,
                  undo ? std::optional<Score>{std::move(before_state)} : std::nullopt,
                  "modify_release_velocity");
    return MutationResult{{}};
}

Result<MutationResult> set_articulation(Score& score,
                                        EventId event_id,
                                        std::uint8_t note_index,
                                        std::optional<ArticulationType> articulation,
                                        UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (articulation && !valid_articulation_type(*articulation))
        return std::unexpected(ErrorCode::InvalidMutation);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Score before_state = score;

    const auto previous = ng->notes[note_index].articulation;
    if (previous != articulation && previous == ArticulationType::GlissandoStart) {
        const auto pairs = collect_note_span_pairs(score);
        if (const auto mate = pairs.gliss_start_to_end.find(event_id.value);
            mate != pairs.gliss_start_to_end.end())
            clear_note_endpoint(score, mate->second, false, false, ArticulationType::GlissandoEnd);
    }
    if (previous != articulation && previous == ArticulationType::GlissandoEnd) {
        const auto pairs = collect_note_span_pairs(score);
        if (const auto mate = pairs.gliss_end_to_start.find(event_id.value);
            mate != pairs.gliss_end_to_start.end())
            clear_note_endpoint(
                score, mate->second, false, false, ArticulationType::GlissandoStart);
    }

    ng->notes[note_index].articulation = articulation;
    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo,
                  undo ? std::optional<Score>{std::move(before_state)} : std::nullopt,
                  "set_articulation");

    return MutationResult{{}};
}

Result<MutationResult>
set_dynamic(Score& score, PartId part_id, ScoreTime position, DynamicLevel level, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_dynamic_level(level)) return std::unexpected(ErrorCode::InvalidMutation);
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
                if (auto advanced = detail::advance_score_version(score); !advanced)
                    return std::unexpected(advanced.error());
                push_snapshot(undo, std::move(before), "set_dynamic");
                return MutationResult{{}};
            }
        }
    }

    return std::unexpected(ErrorCode::InvalidMutation);
}

Result<MutationResult> insert_hairpin(Score& score,
                                      PartId part_id,
                                      ScoreTime start,
                                      ScoreTime end,
                                      HairpinType type,
                                      std::optional<DynamicLevel> target,
                                      UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (static_cast<std::uint8_t>(type) > static_cast<std::uint8_t>(HairpinType::Diminuendo) ||
        (target && !valid_dynamic_level(*target)))
        return std::unexpected(ErrorCode::InvalidMutation);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);
    if (!valid_part_span(score, *part, start, end))
        return std::unexpected(ErrorCode::InvalidScoreTime);
    if (std::any_of(part->hairpins.begin(), part->hairpins.end(), [&](const Hairpin& hairpin) {
            return start < hairpin.end && hairpin.start < end;
        }))
        return std::unexpected(ErrorCode::OverlappingAnnotation);

    std::optional<Score> before;
    if (undo) before = score;

    part->hairpins.push_back(Hairpin{start, end, type, target});
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "insert_hairpin");
    return MutationResult{{}};
}

Result<MutationResult>
set_tie(Score& score, EventId event_id, std::uint8_t note_index, bool tied, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng || note_index >= ng->notes.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    Score before_state = score;

    ng->notes[note_index].tie_forward = tied;
    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(
        undo, undo ? std::optional<Score>{std::move(before_state)} : std::nullopt, "set_tie");

    return MutationResult{{}};
}

Result<MutationResult>
transpose_event(Score& score, EventId event_id, DiatonicInterval interval, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    auto loc = find_event(score, event_id);
    if (!loc.event) return std::unexpected(ErrorCode::InvalidMutation);

    auto* ng = std::get_if<NoteGroup>(&loc.event->payload);
    if (!ng) return std::unexpected(ErrorCode::InvalidMutation);

    Score before_state = score;

    for (auto& note : ng->notes) {
        note.pitch = apply_interval(note.pitch, interval);
    }

    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo,
                  undo ? std::optional<Score>{std::move(before_state)} : std::nullopt,
                  "transpose_event");

    mark_harmonic_stale(score, loc.measure->bar_number);

    return MutationResult{{}};
}

// =============================================================================
// Measure-Level Mutations
// =============================================================================

Result<MutationResult>
insert_measures(Score& score, std::uint32_t after_bar, std::uint32_t count, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    constexpr auto max_bar = std::numeric_limits<std::uint32_t>::max();
    if (count == 0 || after_bar < 1 || after_bar > score.metadata.total_bars ||
        score.metadata.total_bars >= max_bar || count >= max_bar - score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Pre-validate: after_bar must not exceed any part's actual measure count
    for (const auto& part : score.parts) {
        if (after_bar > part.measures.size()) return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    auto event_ids = detail::event_id_allocator(candidate);

    // Determine time signature for new measures
    TimeSignature ts{{4}, 4}; // Default 4/4
    for (const auto& entry : candidate.time_map) {
        if (entry.bar <= after_bar) {
            ts = entry.time_signature;
        }
    }

    // Insert empty measures into each part
    for (auto& part : candidate.parts) {
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t new_bar = after_bar + i + 1;
            auto empty = make_empty_measure(new_bar, ts, event_ids, part.definition.staff_count);
            if (!empty) return std::unexpected(empty.error());
            const auto insertion_offset =
                static_cast<std::ptrdiff_t>(after_bar) + static_cast<std::ptrdiff_t>(i);
            auto it = part.measures.begin() + insertion_offset;
            part.measures.insert(it, std::move(*empty));
        }
        // Renumber subsequent measures
        for (std::size_t m = after_bar + count; m < part.measures.size(); ++m) {
            part.measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
        }
    }

    // Update global map positions
    for (auto& entry : candidate.tempo_map) {
        if (entry.position.bar > after_bar) {
            entry.position.bar += count;
        }
    }
    for (auto& entry : candidate.key_map) {
        if (entry.position.bar > after_bar) {
            entry.position.bar += count;
        }
    }
    for (auto& entry : candidate.time_map) {
        if (entry.bar > after_bar) {
            entry.bar += count;
        }
    }
    auto tempo_durations =
        recompute_linear_tempo_durations(candidate.tempo_map, candidate.time_map);
    if (!tempo_durations) return std::unexpected(tempo_durations.error());

    // Shift position-bearing annotation structures
    shift_section_tree_after_bar(candidate.section_map, after_bar, count);
    for (auto& ha : candidate.harmonic_annotations) {
        shift_position_after_bar(ha.position, after_bar, count);
    }
    for (auto& oa : candidate.orchestration_annotations) {
        shift_position_after_bar(oa.start, after_bar, count);
        shift_position_after_bar(oa.end, after_bar, count);
    }
    for (auto& rm : candidate.rehearsal_marks) {
        shift_position_after_bar(rm.position, after_bar, count);
    }
    for (auto& part : candidate.parts) {
        for (auto& hp : part.hairpins) {
            shift_position_after_bar(hp.start, after_bar, count);
            shift_position_after_bar(hp.end, after_bar, count);
        }
        for (auto& directive : part.part_directives) {
            shift_position_after_bar(directive.start, after_bar, count);
            shift_position_after_bar(directive.end, after_bar, count);
        }
    }
    for (auto& region : candidate.stale_harmonic_regions)
        shift_region_after_bar(region, after_bar, count);
    for (auto& region : candidate.stale_orchestration_regions)
        shift_region_after_bar(region, after_bar, count);

    candidate.metadata.total_bars += count;
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);

    push_snapshot(undo, std::move(before), "insert_measures");

    return MutationResult{{}};
}

Result<MutationResult>
delete_measures(Score& score, std::uint32_t bar, std::uint32_t count, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (count == 0 || bar < 1 || bar > score.metadata.total_bars ||
        count > score.metadata.total_bars - bar + 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (count >= score.metadata.total_bars) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::uint32_t first = bar - 1; // 0-indexed

    std::optional<Score> before;
    if (undo) before = score;

    const auto source_tempo_map = score.tempo_map;
    const auto source_key_map = score.key_map;
    const auto source_time_map = score.time_map;
    const std::uint32_t source_splice_bar = bar + count;
    const bool retains_following_bar = source_splice_bar <= score.metadata.total_bars;
    std::optional<PositiveRational> splice_quarter_tempo;
    std::optional<KeySignature> splice_key;
    std::optional<TimeSignature> splice_time;
    if (retains_following_bar) {
        auto tempo = effective_quarter_tempo_at(
            ScoreTime{source_splice_bar, Beat::zero()}, source_tempo_map, source_time_map);
        if (!tempo) return std::unexpected(tempo.error());
        splice_quarter_tempo = *tempo;
        splice_key = query_key_at(score, ScoreTime{source_splice_bar, Beat::zero()});
        splice_time = query_time_signature_at(score, source_splice_bar);
    }

    std::vector<EventId> removed_events;
    for (const auto& part : score.parts)
        for (std::uint32_t source_bar = bar; source_bar < bar + count; ++source_bar)
            for (const auto& voice : part.measures[source_bar - 1].voices)
                for (const auto& event : voice.events)
                    removed_events.push_back(event.id);
    for (const auto id : removed_events)
        delete_event_content_and_repair_spans(score, id);

    for (auto& part : score.parts) {
        part.measures.erase(part.measures.begin() + first, part.measures.begin() + first + count);
        for (std::size_t m = first; m < part.measures.size(); ++m) {
            part.measures[m].bar_number = static_cast<std::uint32_t>(m + 1);
        }
    }

    const ScoreTime source_splice{source_splice_bar, Beat::zero()};
    score.time_map.clear();
    for (const auto& entry : source_time_map)
        if (entry.bar < bar) score.time_map.push_back(entry);
    if (retains_following_bar && splice_time) {
        score.time_map.push_back(TimeSignatureEntry{bar, *splice_time});
        for (const auto& entry : source_time_map) {
            if (entry.bar <= source_splice_bar) continue;
            auto adjusted = entry;
            adjusted.bar -= count;
            score.time_map.push_back(std::move(adjusted));
        }
    }

    score.key_map.clear();
    for (const auto& entry : source_key_map)
        if (entry.position.bar < bar) score.key_map.push_back(entry);
    if (retains_following_bar && splice_key) {
        score.key_map.push_back(KeySignatureEntry{ScoreTime{bar, Beat::zero()}, *splice_key});
        for (const auto& entry : source_key_map) {
            if (entry.position <= source_splice) continue;
            auto adjusted = entry;
            adjusted.position.bar -= count;
            score.key_map.push_back(std::move(adjusted));
        }
    }

    score.tempo_map.clear();
    for (const auto& entry : source_tempo_map)
        if (entry.position.bar < bar) score.tempo_map.push_back(entry);
    if (retains_following_bar && splice_quarter_tempo) {
        score.tempo_map.push_back(TempoEvent{ScoreTime{bar, Beat::zero()},
                                             *splice_quarter_tempo,
                                             BeatUnit::Quarter,
                                             TempoTransitionType::Immediate,
                                             Beat::zero(),
                                             BeatUnit::Quarter,
                                             BeatUnit::Quarter});
        bool first_after_splice = true;
        auto splice_absolute = score_time_to_absolute_beat(source_splice, source_time_map);
        if (!splice_absolute) return std::unexpected(splice_absolute.error());
        for (const auto& entry : source_tempo_map) {
            if (entry.position <= source_splice) continue;
            auto adjusted = entry;
            adjusted.position.bar -= count;
            if (first_after_splice && adjusted.transition_type == TempoTransitionType::Linear) {
                auto target_absolute = score_time_to_absolute_beat(entry.position, source_time_map);
                if (!target_absolute) return std::unexpected(target_absolute.error());
                auto remaining = checked_sub(*target_absolute, *splice_absolute);
                if (!remaining) return std::unexpected(remaining.error());
                adjusted.linear_duration = *remaining;
            }
            score.tempo_map.push_back(std::move(adjusted));
            first_after_splice = false;
        }
    }

    // Remove and shift annotation structures
    splice_section_tree_for_deleted_bars(score.section_map, bar, count);

    score.harmonic_annotations.erase(std::remove_if(score.harmonic_annotations.begin(),
                                                    score.harmonic_annotations.end(),
                                                    [bar, count](const HarmonicAnnotation& ha) {
                                                        return ha.position.bar >= bar &&
                                                               ha.position.bar < bar + count;
                                                    }),
                                     score.harmonic_annotations.end());
    for (auto& ha : score.harmonic_annotations) {
        if (ha.position.bar >= bar + count) ha.position.bar -= count;
    }

    splice_spans_for_deleted_bars(score.orchestration_annotations, bar, count);

    score.rehearsal_marks.erase(std::remove_if(score.rehearsal_marks.begin(),
                                               score.rehearsal_marks.end(),
                                               [bar, count](const RehearsalMark& rm) {
                                                   return rm.position.bar >= bar &&
                                                          rm.position.bar < bar + count;
                                               }),
                                score.rehearsal_marks.end());
    for (auto& rm : score.rehearsal_marks) {
        if (rm.position.bar >= bar + count) rm.position.bar -= count;
    }

    for (auto& part : score.parts) {
        splice_spans_for_deleted_bars(part.hairpins, bar, count);
        splice_spans_for_deleted_bars(part.part_directives, bar, count);
    }

    splice_spans_for_deleted_bars(score.stale_harmonic_regions, bar, count);
    splice_spans_for_deleted_bars(score.stale_orchestration_regions, bar, count);

    score.metadata.total_bars -= count;
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo, std::move(before), "delete_measures");

    return MutationResult{{}};
}

Result<MutationResult> set_score_tuning(Score& score, ScoreTuning tuning, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (auto valid = validate_score_tuning(tuning); !valid) return std::unexpected(valid.error());

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    candidate.tuning = std::move(tuning);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_score_tuning");
    return MutationResult{{}};
}

Result<MutationResult> set_time_signature(Score& score,
                                          std::uint32_t bar,
                                          const TimeSignature& time_sig,
                                          UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    auto requested_duration = checked_measure_duration(time_sig);
    if (bar < 1 || bar > score.metadata.total_bars || !requested_duration) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    auto event_ids = detail::event_id_allocator(candidate);

    bool found = false;
    for (auto& entry : candidate.time_map) {
        if (entry.bar == bar) {
            entry.time_signature = time_sig;
            found = true;
            break;
        }
    }
    if (!found) {
        TimeSignatureEntry entry{bar, time_sig};
        auto it = std::lower_bound(
            candidate.time_map.begin(),
            candidate.time_map.end(),
            bar,
            [](const TimeSignatureEntry& e, std::uint32_t b) { return e.bar < b; });
        candidate.time_map.insert(it, std::move(entry));
    }

    std::uint32_t final_affected_bar = candidate.metadata.total_bars;
    for (const auto& entry : candidate.time_map) {
        if (entry.bar > bar) {
            final_affected_bar = entry.bar - 1;
            break;
        }
    }
    for (auto& part : candidate.parts) {
        for (std::uint32_t affected = bar;; ++affected) {
            auto& measure = part.measures[affected - 1];
            if (!measure.local_time) {
                for (auto& voice : measure.voices) {
                    auto retiled =
                        retile_voice_to_measure_duration(voice, *requested_duration, event_ids);
                    if (!retiled) return std::unexpected(retiled.error());
                }
            }
            if (affected == final_affected_bar) break;
        }
    }

    auto tempo_durations =
        recompute_linear_tempo_durations(candidate.tempo_map, candidate.time_map);
    if (!tempo_durations) return std::unexpected(tempo_durations.error());
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_time_signature");
    return MutationResult{{}};
}

Result<MutationResult>
set_key_signature(Score& score, ScoreTime position, KeySignature key, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_global_point(score, position)) return std::unexpected(ErrorCode::InvalidScoreTime);

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    bool found = false;
    for (auto& entry : candidate.key_map) {
        if (entry.position == position) {
            entry.key = key;
            found = true;
            break;
        }
    }
    if (!found) {
        KeySignatureEntry entry{position, key};
        auto it = std::lower_bound(
            candidate.key_map.begin(),
            candidate.key_map.end(),
            position,
            [](const KeySignatureEntry& e, const ScoreTime& p) { return e.position < p; });
        candidate.key_map.insert(it, entry);
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_key_signature");
    return MutationResult{{}};
}

// =============================================================================
// Part-Level Mutations
// =============================================================================

Result<MutationResult>
add_part(Score& score, PartDefinition definition, std::size_t position_in_order, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    auto part_ids = detail::part_id_allocator(candidate);
    auto event_ids = detail::event_id_allocator(candidate);

    // Determine time signature for empty measures
    TimeSignature ts{{4}, 4};
    if (!candidate.time_map.empty()) {
        ts = candidate.time_map[0].time_signature;
    }

    Part new_part;
    auto part_id = part_ids.allocate();
    if (!part_id) return std::unexpected(part_id.error());
    new_part.id = *part_id;
    new_part.definition = std::move(definition);

    // Create empty measures
    for (std::uint32_t bar = 1; bar <= candidate.metadata.total_bars; ++bar) {
        // Find applicable time signature
        for (const auto& entry : candidate.time_map) {
            if (entry.bar <= bar)
                ts = entry.time_signature;
            else
                break;
        }
        auto measure = make_empty_measure(bar, ts, event_ids, new_part.definition.staff_count);
        if (!measure) return std::unexpected(measure.error());
        new_part.measures.push_back(std::move(*measure));
    }

    if (position_in_order >= candidate.parts.size()) {
        candidate.parts.push_back(std::move(new_part));
    } else {
        candidate.parts.insert(candidate.parts.begin() +
                                   static_cast<std::ptrdiff_t>(position_in_order),
                               std::move(new_part));
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "add_part");
    return MutationResult{{}};
}

Result<MutationResult> remove_part(Score& score, PartId part_id, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (score.parts.size() <= 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto it = std::find_if(score.parts.begin(), score.parts.end(), [part_id](const Part& p) {
        return p.id == part_id;
    });
    if (it == score.parts.end()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    std::set<std::uint64_t> removed_event_ids;
    const auto candidate_part =
        std::find_if(candidate.parts.begin(), candidate.parts.end(), [part_id](const Part& part) {
            return part.id == part_id;
        });
    if (candidate_part == candidate.parts.end()) return std::unexpected(ErrorCode::InvalidMutation);
    for (const auto& measure : candidate_part->measures)
        for (const auto& voice : measure.voices)
            for (const auto& event : voice.events)
                removed_event_ids.insert(event.id.value);

    candidate.parts.erase(candidate_part);

    std::erase_if(candidate.orchestration_annotations,
                  [part_id](const OrchestrationAnnotation& annotation) {
                      return annotation.part_id == part_id || annotation.doubled_part == part_id ||
                             annotation.dialogue_partner == part_id;
                  });
    for (auto& annotation : candidate.harmonic_annotations)
        std::erase_if(annotation.non_chord_tones,
                      [&](const NonChordToneAnnotation& non_chord_tone) {
                          return removed_event_ids.contains(non_chord_tone.event_id.value);
                      });

    const auto remove_part_from_regions = [part_id](std::vector<ScoreRegion>& regions) {
        std::erase_if(regions, [part_id](ScoreRegion& region) {
            const auto member = std::find(region.parts.begin(), region.parts.end(), part_id);
            if (member == region.parts.end()) return false;
            if (region.parts.size() == 1) return true;
            region.parts.erase(member);
            return false;
        });
    };
    remove_part_from_regions(candidate.stale_harmonic_regions);
    remove_part_from_regions(candidate.stale_orchestration_regions);

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "remove_part");
    return MutationResult{{}};
}

// =============================================================================
// Region-Level Mutations
// =============================================================================

Result<MutationResult> transpose_region(Score& score,
                                        const ScoreRegion& region,
                                        DiatonicInterval interval,
                                        UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    Score before_state = score;

    for (auto& part : score.parts) {
        // Check if this part is in the region
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) {
                    found = true;
                    break;
                }
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

    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo,
                  undo ? std::optional<Score>{std::move(before_state)} : std::nullopt,
                  "transpose_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult> delete_region(Score& score, const ScoreRegion& region, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    std::optional<Score> before;
    if (undo) before = score;

    std::vector<EventId> selected;
    for_each_event_in_region(score, region, [&](Part&, Measure&, Voice&, Event& event) {
        selected.push_back(event.id);
    });
    for (const auto id : selected)
        delete_event_content_and_repair_spans(score, id);

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "delete_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

// =============================================================================
// Voice-Level Mutations
// =============================================================================

Result<MutationResult> add_voice(
    Score& score, std::uint32_t bar, PartId part_id, std::uint8_t voice_number, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    return add_voice(score, bar, part_id, voice_number, 0, undo);
}

Result<MutationResult> add_voice(Score& score,
                                 std::uint32_t bar,
                                 PartId part_id,
                                 std::uint8_t voice_number,
                                 std::uint8_t staff_index,
                                 UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);
    if (staff_index >= part->definition.staff_count)
        return std::unexpected(ErrorCode::InvalidMutation);

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
    Beat dur = ts_entry ? ts_entry->time_signature.measure_duration() : Beat{4, 4};

    RestEvent rest{dur, true};
    auto event_ids = detail::event_id_allocator(score);
    auto event_id = event_ids.allocate();
    if (!event_id) return std::unexpected(event_id.error());
    Event event{*event_id, Beat::zero(), rest};
    Voice new_voice{voice_number, {event}, {}};
    new_voice.staff_index = staff_index;
    auto insertion = std::lower_bound(
        measure.voices.begin(),
        measure.voices.end(),
        voice_number,
        [](const Voice& voice, std::uint8_t index) { return voice.voice_index < index; });
    measure.voices.insert(insertion, std::move(new_voice));

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "add_voice");
    return MutationResult{{}};
}

Result<MutationResult> remove_voice(
    Score& score, std::uint32_t bar, PartId part_id, std::uint8_t voice_number, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    if (bar < 1 || bar > part->measures.size()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto& measure = part->measures[bar - 1];

    if (measure.voices.size() <= 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto it =
        std::find_if(measure.voices.begin(), measure.voices.end(), [voice_number](const Voice& v) {
            return v.voice_index == voice_number;
        });
    if (it == measure.voices.end()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

    std::vector<EventId> removed_events;
    removed_events.reserve(it->events.size());
    for (const auto& event : it->events)
        removed_events.push_back(event.id);
    for (const auto id : removed_events)
        delete_event_content_and_repair_spans(score, id);
    it = std::find_if(measure.voices.begin(), measure.voices.end(), [voice_number](const Voice& v) {
        return v.voice_index == voice_number;
    });
    if (it == measure.voices.end()) return std::unexpected(ErrorCode::InvalidMutation);
    measure.voices.erase(it);

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "remove_voice");
    return MutationResult{{}};
}

// =============================================================================
// Part Management Mutations
// =============================================================================

Result<MutationResult>
reorder_parts(Score& score, const std::vector<PartId>& new_order, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
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
            if (part.id == pid) {
                found = true;
                break;
            }
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

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "reorder_parts");
    return MutationResult{{}};
}

Result<MutationResult>
set_part_directive(Score& score, PartId part_id, PartDirective directive, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);
    if (!valid_part_span(score, *part, directive.start, directive.end))
        return std::unexpected(ErrorCode::InvalidScoreTime);
    if (static_cast<std::uint8_t>(directive.directive) >
        static_cast<std::uint8_t>(DirectiveType::TreCorde))
        return std::unexpected(ErrorCode::InvalidMutation);
    if (directive.directive == DirectiveType::Divisi) {
        if (directive.divisi_count < 2) return std::unexpected(ErrorCode::InvalidMutation);
    } else if (directive.divisi_count != 0) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    Part* candidate_part = find_part(candidate, part_id);
    if (!candidate_part) return std::unexpected(ErrorCode::InvalidMutation);

    candidate_part->part_directives.push_back(directive);

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_part_directive");
    return MutationResult{{}};
}

Result<MutationResult>
assign_instrument(Score& score, PartId part_id, InstrumentType instrument, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    std::optional<Score> before;
    if (undo) before = score;

    part->definition.instrument_type = instrument;

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "assign_instrument");
    return MutationResult{{}};
}

Result<MutationResult> set_articulation_mapping(Score& score,
                                                PartId part_id,
                                                ArticulationType articulation,
                                                std::optional<ArticulationMapping> mapping,
                                                UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    Part* part = find_part(score, part_id);
    if (!part || static_cast<std::uint8_t>(articulation) >
                     static_cast<std::uint8_t>(ArticulationType::BendDown))
        return std::unexpected(ErrorCode::InvalidMutation);
    if (mapping && !validate_articulation_mapping(*mapping))
        return std::unexpected(ErrorCode::InvalidRenderingConfig);

    std::optional<Score> before;
    if (undo) before = score;

    if (mapping) {
        part->definition.rendering.articulation_map[articulation] = std::move(*mapping);
    } else {
        part->definition.rendering.articulation_map.erase(articulation);
    }

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "set_articulation_mapping");
    return MutationResult{{}};
}

// =============================================================================
// Region-Level Mutations (extended)
// =============================================================================

Result<MutationResult>
copy_region(Score& score, const ScoreRegion& src, ScoreTime dest, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, src) || !valid_global_point(score, dest))
        return std::unexpected(ErrorCode::InvalidRegion);
    std::optional<Score> before;
    if (undo) before = score;

    // Collect events to copy: (part_id, bar_offset, event_clone)
    struct EventRecord {
        PartId part_id;
        std::uint32_t bar_offset; // relative bar from region start
        std::uint8_t voice_index;
        Beat offset_in_measure;
        Event event;
    };
    std::vector<EventRecord> records;
    bool stripped_beams = false;
    auto event_ids = detail::event_id_allocator(score);
    std::optional<ErrorCode> allocation_error;

    std::set<std::uint64_t> selected_note_events;
    for_each_event_in_region(score, src, [&](Part&, Measure&, Voice&, Event& event) {
        if (event.is_note_group()) selected_note_events.insert(event.id.value);
    });
    const auto span_pairs = collect_note_span_pairs(score);

    for_each_event_in_region(
        score, src, [&](Part& part, Measure& measure, Voice& voice, Event& event) {
            if (allocation_error) return;
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return; // Only copy note events

            NoteGroup cloned_group = *ng;
            strip_unselected_note_span_endpoints(
                cloned_group, event.id, selected_note_events, span_pairs);
            if (cloned_group.beam_group) {
                cloned_group.beam_group.reset();
                stripped_beams = true;
            }
            auto event_id = event_ids.allocate();
            if (!event_id) {
                allocation_error = event_id.error();
                return;
            }
            Event clone{*event_id, event.offset, std::move(cloned_group)};
            std::uint32_t bar_off = measure.bar_number - src.start.bar;
            records.push_back(
                {part.id, bar_off, voice.voice_index, event.offset, std::move(clone)});
        });
    if (allocation_error) return std::unexpected(*allocation_error);

    Score candidate = score;

    // Insert cloned events at dest
    for (auto& rec : records) {
        std::uint32_t target_bar = dest.bar + rec.bar_offset;

        Part* part = find_part(candidate, rec.part_id);
        if (!part || target_bar < 1 || target_bar > part->measures.size())
            return std::unexpected(ErrorCode::InvalidMutation);

        auto& measure = part->measures[target_bar - 1];

        // Find or skip if voice doesn't exist
        Voice* target_voice = nullptr;
        for (auto& v : measure.voices) {
            if (v.voice_index == rec.voice_index) {
                target_voice = &v;
                break;
            }
        }
        if (!target_voice) return std::unexpected(ErrorCode::InvalidMutation);

        // Adjust offset: for the first bar, add dest.beat offset
        Beat new_offset = rec.offset_in_measure;
        if (rec.bar_offset == 0) {
            new_offset = new_offset - src.start.beat + dest.beat;
        }
        rec.event.offset = new_offset;

        const Beat measure_duration =
            query_time_signature_at(candidate, target_bar).measure_duration();
        auto inserted = insert_group_replacing_rests(
            *target_voice, measure_duration, std::move(rec.event), event_ids);
        if (!inserted) return std::unexpected(inserted.error());
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    candidate.stale_harmonic_regions.push_back(src);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "copy_region");

    MutationResult result;
    if (stripped_beams)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Region copy removed Voice-local BeamGroup references because the copied event IDs "
             "do not identify a BeamGroup in the destination voice",
             src.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult>
move_region(Score& score, const ScoreRegion& src, ScoreTime dest, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, src) || !valid_global_point(score, dest))
        return std::unexpected(ErrorCode::InvalidRegion);
    std::optional<Score> before;
    if (undo) before = score;

    auto copy_result = copy_region(score, src, dest);
    if (!copy_result) {
        // Mid-mutation failure: restore the pre-mutation snapshot
        if (before) score = *before;
        return copy_result;
    }

    MutationResult result = std::move(*copy_result);

    // Delete source content through the same span-aware primitive, but do not
    // double-bump the version already advanced by copy_region.
    std::vector<EventId> selected;
    for_each_event_in_region(
        score, src, [&](Part&, Measure&, Voice&, Event& event) { selected.push_back(event.id); });
    std::set<std::uint64_t> selected_ids;
    for (const auto id : selected)
        selected_ids.insert(id.value);
    const bool removed_beams = remove_beam_groups_touching(score, selected_ids);
    for (const auto id : selected)
        delete_event_content_and_repair_spans(score, id);

    // copy_region already bumped version; no extra bump needed
    push_snapshot(undo, std::move(before), "move_region");

    score.stale_harmonic_regions.push_back(src);

    if (removed_beams)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Region move removed source BeamGroups touching moved content; the destination copy "
             "has new event identity and carries no inferred beam topology",
             src.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult>
set_dynamic_region(Score& score, const ScoreRegion& region, DynamicLevel level, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (!valid_dynamic_level(level)) return std::unexpected(ErrorCode::InvalidMutation);
    std::optional<Score> before;
    if (undo) before = score;

    for_each_event_in_region(score, region, [&](Part&, Measure&, Voice&, Event& event) {
        auto* ng = std::get_if<NoteGroup>(&event.payload);
        if (ng && !ng->notes.empty()) {
            ng->notes[0].dynamic = level;
        }
    });

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "set_dynamic_region");
    return MutationResult{{}};
}

Result<MutationResult>
scale_velocity_region(Score& score, const ScoreRegion& region, double factor, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (!std::isfinite(factor) || factor < 0.0) return std::unexpected(ErrorCode::InvalidMutation);
    std::optional<Score> before;
    if (undo) before = score;

    for_each_event_in_region(score, region, [&](Part&, Measure&, Voice&, Event& event) {
        auto* ng = std::get_if<NoteGroup>(&event.payload);
        if (!ng) return;

        for (auto& note : ng->notes) {
            double scaled = static_cast<double>(note.velocity.value) * factor;
            if (scaled < 0.0) scaled = 0.0;
            if (scaled > 127.0) scaled = 127.0;
            note.velocity.value = static_cast<std::uint8_t>(scaled);
        }
    });

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(undo, std::move(before), "scale_velocity_region");
    return MutationResult{{}};
}

Result<MutationResult> retrograde_region(Score& score, const ScoreRegion& region, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    std::optional<Score> before;
    if (undo) before = score;

    Score candidate = score;
    std::set<std::uint64_t> selected_measured_events;
    for_each_event_in_region(candidate, region, [&](Part&, Measure&, Voice&, Event& event) {
        if (event.duration() > Beat::zero()) selected_measured_events.insert(event.id.value);
    });
    const bool removed_spans = remove_note_spans_touching(candidate, selected_measured_events);
    const bool removed_beams = remove_beam_groups_touching(candidate, selected_measured_events);

    // Reverse measured payloads within each affected measure. Point events
    // retain their exact semantic time; moving them through measured payloads
    // would conflate annotations with rhythmic material.
    for (auto& part : candidate.parts) {
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) {
                    found = true;
                    break;
                }
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
                    if (event_time >= region.start && event_time < region.end &&
                        voice.events[i].duration() > Beat::zero()) {
                        indices.push_back(i);
                    }
                }

                if (indices.size() < 2) continue;

                const Beat first_offset = voice.events[indices.front()].offset;

                // Reverse the payloads among selected events
                for (std::size_t i = 0; i < indices.size() / 2; ++i) {
                    std::size_t j = indices.size() - 1 - i;
                    std::swap(voice.events[indices[i]].payload, voice.events[indices[j]].payload);
                }

                // Pack the reversed durations from the selection's original
                // first onset, preserving its total measured span.
                Beat cursor = first_offset;
                for (std::size_t i = 0; i < indices.size(); ++i) {
                    voice.events[indices[i]].offset = cursor;
                    auto next = checked_add(cursor, voice.events[indices[i]].duration());
                    if (!next) return std::unexpected(next.error());
                    cursor = *next;
                }
                std::stable_sort(
                    voice.events.begin(),
                    voice.events.end(),
                    [](const Event& lhs, const Event& rhs) { return lhs.offset < rhs.offset; });
            }
        }
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    candidate.stale_harmonic_regions.push_back(region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "retrograde_region");

    MutationResult result;
    if (removed_spans)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Retrograde removed slur/glissando spans touching the transformed region because "
             "the current span model has no transformation-stable identity",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    if (removed_beams)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Retrograde removed BeamGroups touching the transformed region because reversing "
             "payload identity does not preserve their ordered primary-beam membership",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult>
invert_region(Score& score, const ScoreRegion& region, SpelledPitch axis, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (axis.letter > 6) return std::unexpected(ErrorCode::InvalidMutation);
    Score before_state = score;

    int axis_midi = midi_value(axis);

    for_each_event_in_region(score, region, [&](Part&, Measure&, Voice&, Event& event) {
        auto* ng = std::get_if<NoteGroup>(&event.payload);
        if (!ng) return;

        for (auto& note : ng->notes) {
            int note_midi = midi_value(note.pitch);
            int new_midi = 2 * axis_midi - note_midi;

            // Mirror the letter name around the axis letter
            int mirror_letter = static_cast<int>((2 * axis.letter - note.pitch.letter + 700) % 7);

            // Approximate octave from new midi value
            int new_octave = (new_midi / 12) - 1;

            // Compute expected midi for this letter+octave with no accidental
            SpelledPitch candidate{
                static_cast<std::uint8_t>(mirror_letter), 0, static_cast<std::int8_t>(new_octave)};
            int candidate_midi = midi_value(candidate);
            std::int8_t new_acc = static_cast<std::int8_t>(new_midi - candidate_midi);

            note.pitch = SpelledPitch{static_cast<std::uint8_t>(mirror_letter),
                                      new_acc,
                                      static_cast<std::int8_t>(new_octave)};
        }
    });

    if (!is_compilable(score)) {
        score = std::move(before_state);
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());
    push_snapshot(
        undo, undo ? std::optional<Score>{std::move(before_state)} : std::nullopt, "invert_region");

    score.stale_harmonic_regions.push_back(region);

    return MutationResult{{}};
}

Result<MutationResult>
augment_region(Score& score, const ScoreRegion& region, Beat factor, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (factor.numerator() <= 0) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

    Score candidate = score;
    auto event_ids = detail::event_id_allocator(candidate);
    std::vector<std::pair<EventId, Beat>> changes;
    std::optional<ErrorCode> arithmetic_error;
    for_each_event_in_region(candidate, region, [&](Part&, Measure&, Voice&, Event& event) {
        const auto* group = event.as_note_group();
        if (!group || arithmetic_error) return;
        if (group->tuplet_context) {
            arithmetic_error = ErrorCode::InvalidMutation;
            return;
        }
        auto duration = checked_mul(group->duration, factor);
        if (!duration) {
            arithmetic_error = duration.error();
            return;
        }
        changes.emplace_back(event.id, *duration);
    });
    if (arithmetic_error) return std::unexpected(*arithmetic_error);
    std::set<std::uint64_t> changed_event_ids;
    for (const auto& [id, unused] : changes) {
        (void)unused;
        changed_event_ids.insert(id.value);
    }
    const bool removed_beams = remove_beam_groups_touching(candidate, changed_event_ids);
    for (const auto& [id, duration] : changes) {
        auto location = find_event(candidate, id);
        if (!location.event) return std::unexpected(ErrorCode::InvalidMutation);
        Event replacement = *location.event;
        std::get<NoteGroup>(replacement.payload).duration = duration;
        const Beat measure_duration =
            location.measure->local_time
                ? location.measure->local_time->measure_duration()
                : query_time_signature_at(candidate, location.measure->bar_number)
                      .measure_duration();
        auto resized = insert_group_replacing_rests(
            *location.voice, measure_duration, std::move(replacement), event_ids, id);
        if (!resized) return std::unexpected(resized.error());
    }
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    candidate.stale_harmonic_regions.push_back(region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "augment_region");

    MutationResult result;
    if (removed_beams)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Augmentation removed BeamGroups touching resized events because written-duration "
             "changes require a new beaming decision",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult>
diminute_region(Score& score, const ScoreRegion& region, Beat factor, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (factor.numerator() <= 0) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    std::optional<Score> before;
    if (undo) before = score;

    Score candidate = score;
    auto event_ids = detail::event_id_allocator(candidate);
    std::vector<std::pair<EventId, Beat>> changes;
    std::optional<ErrorCode> arithmetic_error;
    for_each_event_in_region(candidate, region, [&](Part&, Measure&, Voice&, Event& event) {
        const auto* group = event.as_note_group();
        if (!group || arithmetic_error) return;
        if (group->tuplet_context) {
            arithmetic_error = ErrorCode::InvalidMutation;
            return;
        }
        auto duration = checked_div(group->duration, factor);
        if (!duration || *duration <= Beat::zero()) {
            arithmetic_error = duration ? ErrorCode::InvalidMutation : duration.error();
            return;
        }
        changes.emplace_back(event.id, *duration);
    });
    if (arithmetic_error) return std::unexpected(*arithmetic_error);
    std::set<std::uint64_t> changed_event_ids;
    for (const auto& [id, unused] : changes) {
        (void)unused;
        changed_event_ids.insert(id.value);
    }
    const bool removed_beams = remove_beam_groups_touching(candidate, changed_event_ids);
    for (const auto& [id, duration] : changes) {
        auto location = find_event(candidate, id);
        if (!location.event) return std::unexpected(ErrorCode::InvalidMutation);
        Event replacement = *location.event;
        std::get<NoteGroup>(replacement.payload).duration = duration;
        const Beat measure_duration =
            location.measure->local_time
                ? location.measure->local_time->measure_duration()
                : query_time_signature_at(candidate, location.measure->bar_number)
                      .measure_duration();
        auto resized = insert_group_replacing_rests(
            *location.voice, measure_duration, std::move(replacement), event_ids, id);
        if (!resized) return std::unexpected(resized.error());
    }
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    candidate.stale_harmonic_regions.push_back(region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "diminute_region");

    MutationResult result;
    if (removed_beams)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Diminution removed BeamGroups touching resized events because written-duration "
             "changes require a new beaming decision",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

// =============================================================================
// Orchestration Mutations
// =============================================================================

Result<MutationResult>
reorchestrate(Score& score, const ScoreRegion& region, PartId target, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
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
    bool stripped_spans = false;
    auto event_ids = detail::event_id_allocator(score);
    std::optional<ErrorCode> allocation_error;

    for_each_event_in_region(
        score, region, [&](Part& part, Measure& measure, Voice& voice, Event& event) {
            if (allocation_error) return;
            if (part.id == target) return; // Skip target part
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;

            NoteGroup cloned_group = *ng;
            stripped_spans = strip_voice_span_metadata(cloned_group) || stripped_spans;
            auto event_id = event_ids.allocate();
            if (!event_id) {
                allocation_error = event_id.error();
                return;
            }
            Event clone{*event_id, event.offset, std::move(cloned_group)};
            records.push_back({measure.bar_number, voice.voice_index, std::move(clone)});
        });
    if (allocation_error) return std::unexpected(*allocation_error);

    Score candidate = score;
    Part* candidate_target = find_part(candidate, target);
    if (!candidate_target) return std::unexpected(ErrorCode::InvalidMutation);

    // Insert into target part
    for (auto& rec : records) {
        if (rec.bar_number < 1 || rec.bar_number > candidate_target->measures.size())
            return std::unexpected(ErrorCode::InvalidMutation);

        auto& measure = candidate_target->measures[rec.bar_number - 1];

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
        if (!target_voice) return std::unexpected(ErrorCode::InvalidMutation);

        const Beat measure_duration =
            query_time_signature_at(candidate, rec.bar_number).measure_duration();
        auto inserted = insert_group_replacing_rests(
            *target_voice, measure_duration, std::move(rec.event), event_ids);
        if (!inserted) return std::unexpected(inserted.error());
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    mark_orchestration_stale(candidate, region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "reorchestrate");

    MutationResult result;
    if (stripped_spans)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Reorchestration removed slur/glissando endpoints and BeamGroup references because "
             "Voice-scoped notation identity cannot be transferred across Parts",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult> double_at_interval(Score& score,
                                          const ScoreRegion& region,
                                          PartId target,
                                          DiatonicInterval interval,
                                          UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
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
    bool stripped_spans = false;
    auto event_ids = detail::event_id_allocator(score);
    std::optional<ErrorCode> allocation_error;

    // Each source tuplet, keyed by bar, voice and tuplet id, with the offsets
    // and durations of all its members (rests included) in the source voice.
    using TupletKey = std::tuple<std::uint32_t, std::uint8_t, std::uint64_t>;
    std::map<TupletKey, std::vector<std::pair<Beat, Beat>>> source_tuplets;

    for_each_event_in_region(
        score, region, [&](Part&, Measure& measure, Voice& voice, Event& event) {
            if (allocation_error) return;
            auto* ng = std::get_if<NoteGroup>(&event.payload);
            if (!ng) return;
            if (ng->tuplet_context) {
                // Nested tuplets would need their parent copied as well; refuse
                // rather than flatten the rhythm.
                if (ng->tuplet_context->nested_in) {
                    allocation_error = ErrorCode::InvalidMutation;
                    return;
                }
                const TupletKey key{
                    measure.bar_number, voice.voice_index, ng->tuplet_context->id.value};
                if (!source_tuplets.contains(key)) {
                    auto& layout = source_tuplets[key];
                    for (const auto& member : voice.events) {
                        const auto* context = event_tuplet_context(member);
                        if (context && context->id == ng->tuplet_context->id)
                            layout.emplace_back(member.offset, member.duration());
                    }
                }
            }

            // Clone and transpose each note: the letter advances by the
            // diatonic step count and the accidental carries the remainder.
            NoteGroup transposed = *ng;
            for (auto& note : transposed.notes) {
                auto doubled = checked_apply_interval(note.pitch, interval);
                if (!doubled) {
                    allocation_error = doubled.error();
                    return;
                }
                note.pitch = *doubled;
            }
            stripped_spans = strip_voice_span_metadata(transposed) || stripped_spans;

            auto event_id = event_ids.allocate();
            if (!event_id) {
                allocation_error = event_id.error();
                return;
            }
            Event clone{*event_id, event.offset, std::move(transposed)};
            records.push_back({measure.bar_number, voice.voice_index, std::move(clone)});
        });
    if (allocation_error) return std::unexpected(*allocation_error);

    Score candidate = score;
    Part* candidate_target = find_part(candidate, target);
    if (!candidate_target) return std::unexpected(ErrorCode::InvalidMutation);

    detail::FreshIdAllocator<TupletId> tuplet_ids;
    for (const auto& part : candidate.parts)
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                for (const auto& event : voice.events)
                    if (const auto* context = event_tuplet_context(event))
                        tuplet_ids.include(context->id);
    std::map<TupletKey, TupletContext> copied_tuplets;

    for (auto& rec : records) {
        if (rec.bar_number < 1 || rec.bar_number > candidate_target->measures.size())
            return std::unexpected(ErrorCode::InvalidMutation);

        auto& measure = candidate_target->measures[rec.bar_number - 1];

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
        if (!target_voice) return std::unexpected(ErrorCode::InvalidMutation);

        // A tuplet member lands in a copy of its source tuplet: the first
        // member lays that tuplet's units as rests under a fresh context, and
        // every member then replaces its own unit.
        auto& group = std::get<NoteGroup>(rec.event.payload);
        if (group.tuplet_context) {
            const TupletKey key{rec.bar_number, rec.voice_index, group.tuplet_context->id.value};
            auto copied = copied_tuplets.find(key);
            if (copied == copied_tuplets.end()) {
                auto tuplet_id = tuplet_ids.allocate();
                if (!tuplet_id) return std::unexpected(tuplet_id.error());
                TupletContext fresh = *group.tuplet_context;
                fresh.id = *tuplet_id;
                auto laid = lay_tuplet_rests(
                    target_voice->events, source_tuplets.at(key), fresh, event_ids);
                if (!laid) return std::unexpected(laid.error());
                copied = copied_tuplets.emplace(key, fresh).first;
            }
            group.tuplet_context = copied->second;
        }

        const Beat measure_duration =
            query_time_signature_at(candidate, rec.bar_number).measure_duration();
        auto inserted = insert_group_replacing_rests(
            *target_voice, measure_duration, std::move(rec.event), event_ids);
        if (!inserted) return std::unexpected(inserted.error());
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);

    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    mark_orchestration_stale(candidate, region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "double_at_interval");

    MutationResult result;
    if (stripped_spans)
        result.diagnostics.push_back(
            {ValidationSeverity::Warning,
             "MUT1",
             "Doubling removed slur/glissando endpoints and BeamGroup references because "
             "Voice-scoped notation identity cannot be transferred across Parts",
             region.start,
             std::nullopt,
             ErrorCode::InvariantViolation});
    return result;
}

Result<MutationResult> set_texture_role(
    Score& score, const ScoreRegion& region, PartId part_id, TexturalRole role, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    if (static_cast<std::uint8_t>(role) > static_cast<std::uint8_t>(TexturalRole::Accompagnato) ||
        (!region.parts.empty() &&
         std::find(region.parts.begin(), region.parts.end(), part_id) == region.parts.end()))
        return std::unexpected(ErrorCode::InvalidMutation);
    Part* part = find_part(score, part_id);
    if (!part) return std::unexpected(ErrorCode::InvalidMutation);

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    // Update existing annotation if one overlaps, otherwise add new
    bool found = false;
    for (auto& ann : candidate.orchestration_annotations) {
        if (ann.part_id == part_id && ann.start == region.start && ann.end == region.end) {
            ann.role = role;
            if (role != TexturalRole::Doubling) ann.doubled_part.reset();
            if (role != TexturalRole::PedalTone) ann.pedal_pitch.reset();
            if (role != TexturalRole::Dialogue) ann.dialogue_partner.reset();
            found = true;
            break;
        }
    }

    if (!found) {
        if (role == TexturalRole::Doubling || role == TexturalRole::PedalTone ||
            role == TexturalRole::Dialogue)
            return std::unexpected(ErrorCode::InvalidMutation);
        OrchestrationAnnotation ann;
        ann.part_id = part_id;
        ann.start = region.start;
        ann.end = region.end;
        ann.role = role;
        candidate.orchestration_annotations.push_back(ann);
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    mark_orchestration_stale(candidate, region);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_texture_role");

    return MutationResult{{}};
}

Result<MutationResult> apply_voice_leading(Score& score,
                                           const ScoreRegion& region,
                                           VoiceLeadingStyle style,
                                           UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_score_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    // Require harmonic annotations in the region
    auto annotations = query_harmony_range(score, region.start, region.end);
    if (annotations.empty()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    // Determine voice leading constraints from style
    bool lock_bass = (style == VoiceLeadingStyle::SmoothBach);
    bool allow_par_fifths =
        (style == VoiceLeadingStyle::NearestTone || style == VoiceLeadingStyle::ParallelMotion);
    bool allow_par_octaves =
        (style == VoiceLeadingStyle::NearestTone || style == VoiceLeadingStyle::ParallelMotion);

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
        target_pcs.erase(std::unique(target_pcs.begin(), target_pcs.end()), target_pcs.end());

        if (lock_bass) {
            PitchClass root = target_ann.chord.root;
            auto root_it = std::find(target_pcs.begin(), target_pcs.end(), root);
            if (root_it != target_pcs.end() && root_it != target_pcs.begin()) {
                std::rotate(target_pcs.begin(), root_it, root_it + 1);
            }
        }

        // Collect current notes at the target annotation position
        for_each_event_in_region(
            score,
            ScoreRegion{
                target_ann.position,
                ScoreTime{target_ann.position.bar, target_ann.position.beat + target_ann.duration},
                region.parts},
            [&](Part&, Measure&, Voice&, Event& event) {
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
                    matched_pcs.push_back(target_pcs[matched_pcs.size() % target_pcs.size()]);
                }
                if (matched_pcs.size() > source.size()) {
                    matched_pcs.resize(source.size());
                }

                auto vl_result = voice_lead_nearest_tone(
                    source, matched_pcs, lock_bass, allow_par_fifths, allow_par_octaves);
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
                        note.pitch.letter, 0, static_cast<std::int8_t>((new_midi / 12) - 1)});
                    std::int8_t new_acc = static_cast<std::int8_t>(new_midi - base_midi_no_acc);
                    // Clamp accidental range and adjust octave if needed
                    while (new_acc > 2) {
                        new_acc -= 12;
                    }
                    while (new_acc < -2) {
                        new_acc += 12;
                    }
                    note.pitch = SpelledPitch{
                        note.pitch.letter, new_acc, static_cast<std::int8_t>((new_midi / 12) - 1)};
                }
            });
    }

    if (auto advanced = detail::advance_score_version(score); !advanced)
        return std::unexpected(advanced.error());

    push_snapshot(undo, std::move(before), "apply_voice_leading");

    mark_orchestration_stale(score, region);

    return MutationResult{{}};
}

// =============================================================================
// Undo Grouping
// =============================================================================

void UndoStack::record(Score before, std::string description) {
    if (group_depth > 0) {
        if (group_has_snapshot) return;
        description = group_description;
        group_has_snapshot = true;
    }
    redo_entries.clear();
    if (capacity == 0) return;
    const std::uint64_t snapshot_version = before.version;
    undo_entries.push_back(UndoEntry{snapshot_version,
                                     std::make_shared<const Score>(std::move(before)),
                                     std::move(description)});
    if (undo_entries.size() > capacity) {
        const auto excess = static_cast<std::ptrdiff_t>(undo_entries.size() - capacity);
        undo_entries.erase(undo_entries.begin(), undo_entries.begin() + excess);
    }
}

void UndoStack::begin_group(std::string description) {
    if (group_depth == 0) {
        // end_group is invoked by a noexcept destructor. Reserve its single
        // publication slot up front, while allocation failure can propagate.
        undo_entries.reserve(undo_entries.size() + 1);
        group_has_snapshot = false;
        group_description = std::move(description);
    }
    ++group_depth;
}

void UndoStack::end_group() noexcept {
    if (group_depth == 0) return;
    --group_depth;
    if (group_depth > 0) return;

    group_has_snapshot = false;
    group_description.clear();
}

// =============================================================================
// Undo/Redo
// =============================================================================

VoidResult undo(Score& score, UndoStack& stack) {
    if (!stack.can_undo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    UndoEntry entry = std::move(stack.undo_entries.back());
    stack.undo_entries.pop_back();

    // Save the live document for redo, then restore the snapshot.
    // Restoration cannot fail, so the swap needs no failure path.
    stack.redo_entries.push_back(
        UndoEntry{score.version, std::make_shared<const Score>(score), entry.description});

    const std::uint64_t next_version = score.version + 1;
    score = *entry.state;
    score.version = next_version;

    return {};
}

VoidResult redo(Score& score, UndoStack& stack) {
    if (!stack.can_redo()) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    UndoEntry entry = std::move(stack.redo_entries.back());
    stack.redo_entries.pop_back();

    // Save the live document for undo, then restore the snapshot.
    stack.undo_entries.push_back(
        UndoEntry{score.version, std::make_shared<const Score>(score), entry.description});

    const std::uint64_t next_version = score.version + 1;
    score = *entry.state;
    score.version = next_version;

    return {};
}

} // namespace sunny::core
