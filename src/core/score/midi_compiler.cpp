/**
 * @file midi_compiler.cpp
 * @brief Score IR MIDI compilation — implementation
 *
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>
#include <tuple>

namespace sunny::core {

SmfTimeSignatureProjection project_smf_time_signature(const TimeSignature& signature) noexcept {
    const auto& groups = signature.groups();
    int common_group_pulses = groups.front();
    const int first_group = groups.front();
    bool uniform = true;
    for (const int group : groups) {
        common_group_pulses = std::gcd(common_group_pulses, group);
        uniform = uniform && group == first_group;
    }

    // MIDI defines 24 clocks per quarter, hence 96 per Sunny whole note.
    // The greatest common group divisor is the largest uniform click grid on
    // which every stored group boundary lies. This is a click projection,
    // not an ordered-group encoding: SMF gives `cc` metronome semantics.
    const auto clock_numerator =
        std::uint64_t{96} * static_cast<std::uint64_t>(common_group_pulses);
    const auto denominator = static_cast<std::uint64_t>(signature.denominator());
    if (clock_numerator % denominator != 0) return {};
    const auto clocks = clock_numerator / denominator;
    if (clocks == 0 || clocks > std::numeric_limits<std::uint8_t>::max()) return {};
    return {static_cast<std::uint8_t>(clocks), true, uniform};
}

namespace {

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

constexpr std::uint32_t MAX_MIDI_TEMPO_WORD = 0xFF'FF'FFU;
constexpr std::uint64_t MAX_LINEAR_TEMPO_SAMPLES_PER_RAMP = 4096;
constexpr double MAX_LINEAR_TEMPO_HOLD_ERROR_QBPM = 0.5;

Result<std::uint32_t> midi_tempo_word(double quarter_bpm) {
    if (!std::isfinite(quarter_bpm) || quarter_bpm <= 0.0)
        return std::unexpected(ErrorCode::InvalidMidiTempo);
    const double microseconds = 60'000'000.0 / quarter_bpm;
    if (!std::isfinite(microseconds)) return std::unexpected(ErrorCode::InvalidMidiTempo);
    const double rounded = std::round(microseconds);
    if (rounded < 1.0 || rounded > static_cast<double>(MAX_MIDI_TEMPO_WORD))
        return std::unexpected(ErrorCode::InvalidMidiTempo);
    return static_cast<std::uint32_t>(rounded);
}

bool is_minor_key(const KeySignature& key) {
    auto ints = key.mode.get_intervals();
    // Minor third at scale degree 3 distinguishes minor from major
    return ints.size() >= 3 && ints[2] == 3;
}

bool is_native_midi_key_mode(std::string_view mode) {
    return mode == "major" || mode == "ionian" || mode == "minor" || mode == "aeolian";
}

/// Find hairpin active at a position for a part. Returns nullptr if none.
const Hairpin* find_active_hairpin(const Part& part, ScoreTime position) {
    for (const auto& h : part.hairpins) {
        if (h.start <= position && position < h.end) return &h;
    }
    return nullptr;
}

/// Find orchestration balance annotation for a part at a position.
std::optional<DynamicBalance> find_balance(const Score& score, PartId part_id, ScoreTime position) {
    for (const auto& ann : score.orchestration_annotations) {
        if (ann.part_id == part_id && ann.start <= position && position < ann.end &&
            ann.dynamic_balance) {
            return ann.dynamic_balance;
        }
    }
    return std::nullopt;
}

/// Compute interpolation ratio in the score's exact cumulative Beat domain.
double hairpin_ratio(const Hairpin& h, ScoreTime position, const TimeSignatureMap& time_map) {
    const auto start = score_time_to_absolute_beat(h.start, time_map);
    const auto end = score_time_to_absolute_beat(h.end, time_map);
    const auto point = score_time_to_absolute_beat(position, time_map);
    if (!start || !end || !point) return 0.0;
    const auto span = checked_sub(*end, *start);
    const auto elapsed = checked_sub(*point, *start);
    if (!span || !elapsed || *span <= Beat::zero()) return 0.0;
    return std::clamp(elapsed->to_float() / span->to_float(), 0.0, 1.0);
}

DynamicLevel implicit_hairpin_target(DynamicLevel start, HairpinType type) {
    constexpr std::array ladder{DynamicLevel::pppp,
                                DynamicLevel::ppp,
                                DynamicLevel::pp,
                                DynamicLevel::p,
                                DynamicLevel::mp,
                                DynamicLevel::mf,
                                DynamicLevel::f,
                                DynamicLevel::ff,
                                DynamicLevel::fff,
                                DynamicLevel::ffff};
    const auto start_velocity = static_cast<int>(default_velocity(start));
    auto closest = ladder.begin();
    for (auto candidate = std::next(ladder.begin()); candidate != ladder.end(); ++candidate) {
        if (std::abs(static_cast<int>(default_velocity(*candidate)) - start_velocity) <
            std::abs(static_cast<int>(default_velocity(*closest)) - start_velocity))
            closest = candidate;
    }
    if (type == HairpinType::Crescendo && std::next(closest) != ladder.end()) ++closest;
    if (type == HairpinType::Diminuendo && closest != ladder.begin()) --closest;
    return *closest;
}

/// Resolve the dynamic/hairpin stages. Articulation and orchestration balance
/// are applied later because custom articulation mappings replace the default
/// articulation stage but not the other stages.
double resolve_dynamic_velocity(const Note& note,
                                DynamicLevel current_dynamic,
                                const Part& part,
                                ScoreTime position,
                                const TimeSignatureMap& time_map) {
    double velocity = 0.0;
    if (note.dynamic) {
        velocity = static_cast<double>(default_velocity(*note.dynamic));
    } else if (note.velocity.written) {
        velocity = static_cast<double>(default_velocity(*note.velocity.written));
    } else if (is_valid_velocity(note.velocity.value)) {
        velocity = static_cast<double>(note.velocity.value);
    } else {
        velocity = static_cast<double>(default_velocity(current_dynamic));
    }

    // Hairpin interpolation overrides the base if active.
    const Hairpin* hairpin = find_active_hairpin(part, position);
    if (hairpin) {
        DynamicLevel start_dyn = query_dynamics_at(part, hairpin->start).value_or(DynamicLevel::mf);
        double start_vel = static_cast<double>(default_velocity(start_dyn));
        const DynamicLevel target =
            hairpin->target.value_or(implicit_hairpin_target(start_dyn, hairpin->type));
        double target_vel = static_cast<double>(default_velocity(target));
        double ratio = hairpin_ratio(*hairpin, position, time_map);
        velocity = start_vel + ratio * (target_vel - start_vel);
    }
    return velocity;
}

void apply_default_articulation_velocity(ArticulationType articulation, double& velocity) {
    switch (articulation) {
    case ArticulationType::Accent:
        velocity += 20.0;
        break;
    case ArticulationType::Marcato:
        velocity += 30.0;
        break;
    case ArticulationType::Sforzando:
    case ArticulationType::ForzandoPiano:
        velocity = std::min(127.0, velocity + 40.0);
        break;
    default:
        break;
    }
}

std::uint8_t
finalise_velocity(double velocity, const Score& score, const Part& part, ScoreTime position) {
    auto balance = find_balance(score, part.id, position);
    if (balance) velocity *= balance_factor(*balance);

    const int clamped = static_cast<int>(std::round(velocity));
    return static_cast<std::uint8_t>(std::clamp(clamped, 1, 127));
}

Result<void> apply_articulation_mapping(const ArticulationMapping& mapping,
                                        std::int64_t tick,
                                        std::uint8_t channel,
                                        PartId part_id,
                                        double& velocity,
                                        long double& duration_scale,
                                        CompiledMidi& midi) {
    using Type = ArticulationMapping::Type;
    switch (mapping.type) {
    case Type::Keyswitch: {
        const int note = midi_value(mapping.keyswitch_pitch);
        if (!is_valid_midi_note(note)) return std::unexpected(ErrorCode::InvalidRenderingConfig);
        midi.keyswitches.push_back(
            MidiKeyswitchData{tick, 1, channel, static_cast<std::uint8_t>(note), 127, part_id});
        break;
    }
    case Type::CC:
        midi.control_changes.push_back(
            MidiControlChangeData{tick, channel, mapping.cc_number, mapping.cc_value, part_id});
        break;
    case Type::VelocityLayer: {
        const double ratio = (std::clamp(velocity, 1.0, 127.0) - 1.0) / 126.0;
        velocity = static_cast<double>(mapping.velocity_min) +
                   ratio * static_cast<double>(mapping.velocity_max - mapping.velocity_min);
        break;
    }
    case Type::NoteDurationScale:
        duration_scale *= static_cast<long double>(mapping.duration_scale);
        if (!std::isfinite(duration_scale) || duration_scale <= 0.0L)
            return std::unexpected(ErrorCode::InvalidRenderingConfig);
        break;
    case Type::ProgramChange:
        midi.program_changes.push_back(
            MidiProgramChangeData{tick, channel, mapping.program, part_id});
        break;
    case Type::Combined:
        for (const auto& child : mapping.combined) {
            auto result = apply_articulation_mapping(
                child, tick, channel, part_id, velocity, duration_scale, midi);
            if (!result) return result;
        }
        break;
    }
    return {};
}

/// Convert the stored IEEE-754 float value to its exact positive rational
/// when that rational fits Beat's signed 64-bit representation.
Result<Beat> exact_duration_scale(float value) {
    if (!std::isfinite(value) || value <= 0.0F)
        return std::unexpected(ErrorCode::InvalidRenderingConfig);

    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    const std::uint32_t exponent_bits = (bits >> 23U) & 0xffU;
    const std::uint64_t fraction_bits = bits & 0x7fffffU;
    std::uint64_t significand = 0;
    int binary_exponent = 0;
    if (exponent_bits == 0) {
        significand = fraction_bits;
        binary_exponent = -149;
    } else {
        significand = (std::uint64_t{1} << 23U) | fraction_bits;
        binary_exponent = static_cast<int>(exponent_bits) - 127 - 23;
    }
    if (significand == 0) return std::unexpected(ErrorCode::InvalidRenderingConfig);

    while (binary_exponent < 0 && (significand & 1U) == 0U) {
        significand >>= 1U;
        ++binary_exponent;
    }

    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (binary_exponent >= 0) {
        if (binary_exponent >= 63 || significand > (maximum >> binary_exponent))
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        return Beat::from_ratio(static_cast<std::int64_t>(significand << binary_exponent), 1);
    }

    const int denominator_exponent = -binary_exponent;
    if (denominator_exponent >= 63 || significand > maximum)
        return std::unexpected(ErrorCode::TargetValueUnrepresentable);
    return Beat::from_ratio(static_cast<std::int64_t>(significand),
                            std::int64_t{1} << denominator_exponent);
}

struct NoteEventArticulationProjection {
    double velocity;
    Beat duration_scale{1, 1};
    std::uint64_t omitted_control_events = 0;
};

Result<void> apply_note_event_articulation_mapping(const ArticulationMapping& mapping,
                                                   NoteEventArticulationProjection& projection) {
    using Type = ArticulationMapping::Type;
    switch (mapping.type) {
    case Type::Keyswitch:
    case Type::CC:
    case Type::ProgramChange:
        ++projection.omitted_control_events;
        break;
    case Type::VelocityLayer: {
        const double ratio = (std::clamp(projection.velocity, 1.0, 127.0) - 1.0) / 126.0;
        projection.velocity =
            static_cast<double>(mapping.velocity_min) +
            ratio * static_cast<double>(mapping.velocity_max - mapping.velocity_min);
        break;
    }
    case Type::NoteDurationScale: {
        const auto scale = exact_duration_scale(mapping.duration_scale);
        if (!scale) return std::unexpected(scale.error());
        const auto combined = checked_mul(projection.duration_scale, *scale);
        if (!combined) return std::unexpected(combined.error());
        projection.duration_scale = *combined;
        break;
    }
    case Type::Combined:
        for (const auto& child : mapping.combined) {
            const auto applied = apply_note_event_articulation_mapping(child, projection);
            if (!applied) return applied;
        }
        break;
    }
    return {};
}

template <typename Event, typename Projection>
void sort_and_deduplicate_events(std::vector<Event>& events, Projection projection) {
    std::stable_sort(events.begin(), events.end(), [&](const Event& lhs, const Event& rhs) {
        return projection(lhs) < projection(rhs);
    });
    events.erase(std::unique(events.begin(),
                             events.end(),
                             [&](const Event& lhs, const Event& rhs) {
                                 return projection(lhs) == projection(rhs);
                             }),
                 events.end());
}

/// Flat note reference for tie-chain traversal within a voice.
struct FlatNote {
    ScoreTime position;
    const NoteGroup* ng;
    EventId event_id;
};

using ConsumedNoteSet = std::set<std::pair<std::uint64_t, std::size_t>>;

struct TieProjection {
    Beat duration;
    std::uint8_t release_velocity;
};

/// Collapse one structurally validated tie chain into its sounding duration
/// and terminal Note Off intensity.  Point events are absent from FlatNote;
/// S7 has already proved that every continuation is the next measured event.
Result<TieProjection> resolve_tie_projection(const std::vector<FlatNote>& flat,
                                             std::size_t flat_index,
                                             std::size_t note_index,
                                             ConsumedNoteSet& consumed) {
    const Note& head = flat[flat_index].ng->notes[note_index];
    TieProjection projection{flat[flat_index].ng->duration, head.release_velocity};
    if (head.grace || !head.tie_forward) return projection;

    const SpelledPitch tied_pitch = head.pitch;
    std::size_t search = flat_index + 1;
    bool still_tied = true;
    while (still_tied) {
        if (search >= flat.size()) return std::unexpected(ErrorCode::TieMismatch);

        const auto& continuation = flat[search];
        bool found = false;
        for (std::size_t candidate_index = 0; candidate_index < continuation.ng->notes.size();
             ++candidate_index) {
            const auto key = std::pair{continuation.event_id.value, candidate_index};
            if (consumed.contains(key)) continue;
            const Note& candidate = continuation.ng->notes[candidate_index];
            if (candidate.grace || candidate.pitch != tied_pitch) continue;

            const auto duration = checked_add(projection.duration, continuation.ng->duration);
            if (!duration) return std::unexpected(duration.error());
            projection.duration = *duration;
            projection.release_velocity = candidate.release_velocity;
            consumed.insert(key);
            still_tied = candidate.tie_forward;
            found = true;
            break;
        }
        if (!found) return std::unexpected(ErrorCode::TieMismatch);
        if (still_tied) ++search;
    }
    return projection;
}

Result<std::vector<CompilationDiagnostic>> local_meter_performance_residuals(const Score& score) {
    std::vector<CompilationDiagnostic> residuals;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (!measure.local_time) continue;
            const TimeSignatureEntry* global = nullptr;
            for (const auto& entry : score.time_map) {
                if (entry.bar > measure.bar_number) break;
                global = &entry;
            }
            if (global == nullptr) return std::unexpected(ErrorCode::InvalidTimeSignature);
            auto local_duration = checked_measure_duration(*measure.local_time);
            auto global_duration = checked_measure_duration(global->time_signature);
            if (!local_duration || !global_duration)
                return std::unexpected(ErrorCode::InvalidTimeSignature);
            if (*local_duration != *global_duration)
                return std::unexpected(ErrorCode::TargetValueUnrepresentable);
            if (*measure.local_time == global->time_signature) continue;
            residuals.push_back(
                {"Measure-local time signature omitted: the shared MIDI/NoteEvent performance "
                 "projection has one global meter timeline; equal-duration local grouping remains "
                 "notation-only",
                 ScoreTime{measure.bar_number, Beat::zero()},
                 part.id});
        }
    }
    return residuals;
}

Result<std::uint64_t> distinct_meter_grouping_requests(const Score& score) {
    std::uint64_t count = 0;
    const auto increment = [&count]() -> Result<void> {
        if (count == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        count++;
        return {};
    };

    for (const auto& entry : score.time_map) {
        if (!has_distinct_meter_grouping(entry.time_signature)) continue;
        auto counted = increment();
        if (!counted) return std::unexpected(counted.error());
    }
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (!measure.local_time) continue;
            const TimeSignatureEntry* global = nullptr;
            for (const auto& entry : score.time_map) {
                if (entry.bar > measure.bar_number) break;
                global = &entry;
            }
            if (global == nullptr) return std::unexpected(ErrorCode::InvalidTimeSignature);
            if (*measure.local_time == global->time_signature ||
                !has_distinct_meter_grouping(*measure.local_time))
                continue;
            auto counted = increment();
            if (!counted) return std::unexpected(counted.error());
        }
    }
    return count;
}

std::vector<CompilationDiagnostic> local_key_midi_residuals(const Score& score) {
    std::vector<CompilationDiagnostic> residuals;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (!measure.local_key) continue;
            const ScoreTime measure_start{measure.bar_number, Beat::zero()};
            const KeySignatureEntry* global = nullptr;
            for (const auto& entry : score.key_map) {
                if (entry.position > measure_start) break;
                global = &entry;
            }
            if (global != nullptr && *measure.local_key == global->key) continue;
            residuals.push_back(
                {"Measure-local key signature omitted: Standard MIDI key-signature metadata does "
                 "not retain Sunny's Part-local key override",
                 measure_start,
                 part.id});
        }
    }
    return residuals;
}

/// Collapse same-key notes on one channel whose interval ends strictly inside another's.
///
/// SMF identifies a Note Off only by channel and key, and a reader pairs it
/// with the oldest open Note On of that key. An inner interval [s2, e2) with
/// s1 <= s2 and e2 < e1 would therefore end the outer note at e2, so the file
/// writer rejects it. Such a unison already sounds for its whole span through
/// the enclosing note; it is emitted once and the absorbed attack is reported.
/// Identical intervals and intervals sharing only their end remain distinct:
/// FIFO pairing reproduces both exactly. Afterwards the surviving notes of each
/// key have non-decreasing starts and ends.
std::vector<CompilationDiagnostic>
collapse_nested_unisons(std::vector<MidiNoteData>& notes, const std::vector<ScoreTime>& positions) {
    std::vector<std::size_t> order(notes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const auto end_of = [&](std::size_t index) {
        return notes[index].tick + notes[index].duration_ticks;
    };
    // Earlier start first; at equal starts the longer note encloses the shorter.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
        return std::tuple{notes[lhs].channel, notes[lhs].note, notes[lhs].tick, -end_of(lhs)} <
               std::tuple{notes[rhs].channel, notes[rhs].note, notes[rhs].tick, -end_of(rhs)};
    });

    std::vector<bool> absorbed(notes.size(), false);
    std::vector<CompilationDiagnostic> residuals;
    std::optional<std::size_t> carrier;
    for (const auto index : order) {
        const bool same_key = carrier && notes[*carrier].channel == notes[index].channel &&
                              notes[*carrier].note == notes[index].note;
        if (same_key && end_of(index) < end_of(*carrier)) {
            absorbed[index] = true;
            residuals.push_back(
                {"Nested same-key unison merged: MIDI channel " +
                     std::to_string(static_cast<int>(notes[index].channel)) + " key " +
                     std::to_string(static_cast<int>(notes[index].note)) +
                     " already sounds from an enclosing note, and an SMF Note Off cannot be "
                     "paired with the inner attack; the unison sounds as one note",
                 positions[index],
                 notes[index].part_id});
            continue;
        }
        carrier = index;
    }

    std::size_t kept = 0;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        if (!absorbed[index]) notes[kept++] = notes[index];
    }
    notes.resize(kept);
    return residuals;
}

} // anonymous namespace

Result<GraceTiming>
resolve_grace_timing(GraceType type, Beat allocation_start, Beat allocation_duration) {
    if (allocation_start < Beat::zero() || allocation_duration <= Beat::zero())
        return std::unexpected(ErrorCode::InvalidBeat);

    if (type == GraceType::Appoggiatura) return GraceTiming{allocation_start, allocation_duration};

    const Beat sounding_duration = std::min(allocation_duration, MAX_ACCIACCATURA_DURATION);
    auto allocation_end = checked_add(allocation_start, allocation_duration);
    if (!allocation_end) return std::unexpected(allocation_end.error());
    auto sounding_start = checked_sub(*allocation_end, sounding_duration);
    if (!sounding_start) return std::unexpected(sounding_start.error());
    return GraceTiming{*sounding_start, sounding_duration};
}

// =============================================================================
// compile_to_midi
// =============================================================================

Result<CompiledMidiResult> compile_to_midi(const Score& score, int ppq) {
    if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max()) {
        return std::unexpected(ErrorCode::InvalidMidiPPQ);
    }
    if (!is_compilable(score)) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }
    for (const auto& diagnostic : validate_rendering(score)) {
        if (diagnostic.severity == ValidationSeverity::Error)
            return std::unexpected(ErrorCode::InvalidRenderingConfig);
    }
    auto local_meter_residuals = local_meter_performance_residuals(score);
    if (!local_meter_residuals) return std::unexpected(local_meter_residuals.error());
    if (local_meter_residuals->size() > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    auto grouping_requests = distinct_meter_grouping_requests(score);
    if (!grouping_requests) return std::unexpected(grouping_requests.error());
    auto local_key_residuals = local_key_midi_residuals(score);
    if (local_key_residuals.size() > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    CompiledMidi midi;
    CompilationReport report;
    report.tuning_definitions_requested = 1;
    if (is_standard_midi_tuning(score.tuning)) {
        report.tuning_definitions_written = 1;
    } else {
        report.diagnostics.push_back(
            {"Score tuning was not written: the admitted MIDI profile carries only the canonical "
             "12-TET/A4=440 note-index baseline",
             std::nullopt,
             std::nullopt});
    }
    report.dropped_time_sig_events = static_cast<std::uint32_t>(local_meter_residuals->size());
    report.time_signature_events_requested =
        static_cast<std::uint64_t>(score.time_map.size()) +
        static_cast<std::uint64_t>(local_meter_residuals->size());
    report.time_signature_groupings_requested = *grouping_requests;
    report.dropped_key_sig_events = static_cast<std::uint32_t>(local_key_residuals.size());
    report.diagnostics.insert(
        report.diagnostics.end(), local_meter_residuals->begin(), local_meter_residuals->end());
    report.diagnostics.insert(
        report.diagnostics.end(), local_key_residuals.begin(), local_key_residuals.end());
    midi.ppq = static_cast<std::uint16_t>(ppq);

    // --- Tempo meta events ---
    // SMF Set Tempo is a step function with a 24-bit microseconds-per-quarter
    // payload. Immediate and metric-modulation points map directly. A Linear
    // destination is sampled on the target tick lattice with a held-step qBPM
    // error no greater than max(0.5, |delta| / ramp_ticks), before tempo-word
    // quantisation. Pathological ramps that would exceed the explicit sample
    // cap are rejected rather than silently weakened or exhausting memory.
    std::vector<std::int64_t> source_tempo_ticks;
    std::vector<double> source_quarter_bpms;
    source_tempo_ticks.reserve(score.tempo_map.size());
    source_quarter_bpms.reserve(score.tempo_map.size());
    for (const auto& event : score.tempo_map) {
        auto absolute = score_time_to_absolute_beat(event.position, score.time_map);
        if (!absolute) return std::unexpected(absolute.error());
        source_tempo_ticks.push_back(absolute_beat_to_tick(*absolute, ppq));
        source_quarter_bpms.push_back(effective_quarter_bpm(event));
    }

    const auto append_tempo = [&](std::int64_t tick, double quarter_bpm) -> Result<void> {
        auto word = midi_tempo_word(quarter_bpm);
        if (!word) return std::unexpected(word.error());
        midi.tempos.push_back(MidiTempoData{tick, *word});
        return {};
    };

    if (!score.tempo_map.empty()) {
        auto appended = append_tempo(source_tempo_ticks.front(), source_quarter_bpms.front());
        if (!appended) return std::unexpected(appended.error());
    }

    for (std::size_t index = 1; index < score.tempo_map.size(); ++index) {
        const auto start_tick = source_tempo_ticks[index - 1];
        const auto end_tick = source_tempo_ticks[index];
        if (end_tick <= start_tick) return std::unexpected(ErrorCode::InvalidMidiTempo);

        const double start_bpm = source_quarter_bpms[index - 1];
        const double end_bpm = source_quarter_bpms[index];
        if (score.tempo_map[index].transition_type != TempoTransitionType::Linear ||
            start_bpm == end_bpm) {
            auto appended = append_tempo(end_tick, end_bpm);
            if (!appended) return std::unexpected(appended.error());
            continue;
        }

        const auto ramp_ticks = end_tick - start_tick;
        const double delta = end_bpm - start_bpm;
        const double slope_per_tick = std::abs(delta) / static_cast<double>(ramp_ticks);
        std::int64_t sample_stride = 1;
        if (slope_per_tick <= MAX_LINEAR_TEMPO_HOLD_ERROR_QBPM) {
            const double permitted_stride = MAX_LINEAR_TEMPO_HOLD_ERROR_QBPM / slope_per_tick;
            sample_stride = permitted_stride >= static_cast<double>(ramp_ticks)
                                ? ramp_ticks
                                : static_cast<std::int64_t>(std::floor(permitted_stride));
            sample_stride = std::clamp<std::int64_t>(sample_stride, 1, ramp_ticks);
        }

        const auto samples = static_cast<std::uint64_t>((ramp_ticks - 1) / sample_stride) + 1;
        if (samples > MAX_LINEAR_TEMPO_SAMPLES_PER_RAMP)
            return std::unexpected(ErrorCode::InvalidMidiTempo);

        for (std::int64_t tick = start_tick + sample_stride; tick < end_tick;) {
            const double fraction =
                static_cast<double>(tick - start_tick) / static_cast<double>(ramp_ticks);
            auto appended = append_tempo(tick, start_bpm + fraction * delta);
            if (!appended) return std::unexpected(appended.error());
            if (tick > end_tick - sample_stride) break;
            tick += sample_stride;
        }
        auto appended = append_tempo(end_tick, end_bpm);
        if (!appended) return std::unexpected(appended.error());

        report.diagnostics.push_back(
            {"Linear tempo transition sampled into " + std::to_string(samples) +
                 " SMF Set Tempo steps; qBPM hold error is bounded by the tick lattice and "
                 "24-bit tempo-word quantisation",
             score.tempo_map[index].position,
             std::nullopt});
    }

    // --- Time signature meta events ---
    for (const auto& tse : score.time_map) {
        const int source_numerator = tse.time_signature.numerator();
        const int source_denominator = tse.time_signature.denominator();
        if (source_numerator > std::numeric_limits<std::uint8_t>::max() ||
            source_denominator > 128) {
            if (report.dropped_time_sig_events == std::numeric_limits<std::uint32_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            report.dropped_time_sig_events++;
            report.diagnostics.push_back(
                {"Time signature event dropped: Sunny's exact SMF profile represents numerators "
                 "1..255 and power-of-two denominators no larger than 128",
                 ScoreTime{tse.bar, Beat::zero()},
                 std::nullopt});
            continue;
        }

        auto abs_beat =
            score_time_to_absolute_beat(ScoreTime{tse.bar, Beat::zero()}, score.time_map);
        if (!abs_beat) {
            if (report.dropped_time_sig_events == std::numeric_limits<std::uint32_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            report.dropped_time_sig_events++;
            report.diagnostics.push_back(
                {"Time signature event dropped: temporal conversion failed",
                 ScoreTime{tse.bar, Beat::zero()},
                 std::nullopt});
            continue;
        }

        std::int64_t tick = absolute_beat_to_tick(*abs_beat, ppq);
        const auto projection = project_smf_time_signature(tse.time_signature);
        midi.time_signatures.push_back(
            MidiTimeSigData{tick,
                            static_cast<std::uint8_t>(source_numerator),
                            static_cast<std::uint8_t>(source_denominator),
                            projection.clocks_per_metronome_click,
                            8});
        report.time_signature_events_written++;
        if (has_distinct_meter_grouping(tse.time_signature)) {
            const std::string click_description =
                projection.exact_group_boundary_grid
                    ? (projection.uniform_group_span ? "a matching repeated group-span click"
                                                     : "a common boundary-aligned click grid")
                    : "the canonical 24-clock quarter-note click";
            report.diagnostics.push_back(
                {"Explicit time-signature grouping was preserved but not written: SMF `cc` "
                 "expresses one metronome-click interval, not an ordered group partition; "
                 "emitted " +
                     click_description,
                 ScoreTime{tse.bar, Beat::zero()},
                 std::nullopt});
        }
    }

    // --- Key signature meta events ---
    for (const auto& ke : score.key_map) {
        auto abs_beat = score_time_to_absolute_beat(ke.position, score.time_map);
        if (!abs_beat) {
            ++report.dropped_key_sig_events;
            report.diagnostics.push_back({"Key signature event dropped: temporal conversion failed",
                                          ke.position,
                                          std::nullopt});
            continue;
        }

        // Score IR admits coherent traditional signatures beyond seven sharps or flats, but the
        // Standard MIDI File Key Signature meta event closes this field to [-7, 7]. Keep the
        // sounding notes and make the target-specific metadata loss observable.
        if (ke.key.accidentals < -7 || ke.key.accidentals > 7) {
            ++report.dropped_key_sig_events;
            report.diagnostics.push_back(
                {"Key signature event dropped: Standard MIDI File metadata represents only "
                 "seven flats through seven sharps; the Score key has " +
                     std::to_string(static_cast<int>(ke.key.accidentals)) + " fifths",
                 ke.position,
                 std::nullopt});
            continue;
        }

        std::int64_t tick = absolute_beat_to_tick(*abs_beat, ppq);
        bool minor = is_minor_key(ke.key);

        midi.key_signatures.push_back(MidiKeySigData{tick, ke.key.accidentals, minor});
        if (!is_native_midi_key_mode(ke.key.mode.name)) {
            const auto scale_label = ke.key.mode.name.empty()
                                         ? std::string{"an anonymous Sunny scale"}
                                         : "Sunny scale '" + ke.key.mode.name + "'";
            report.diagnostics.push_back(
                {"Standard MIDI key-signature metadata carries only major/minor mode; retained "
                 "the exact fifths count for " +
                     scale_label + " and emitted a " + (minor ? "minor" : "major") + " proxy mode",
                 ke.position,
                 std::nullopt});
        }
    }

    // --- Note events per part ---
    // Source positions run parallel to midi.notes so that a later unison
    // residual can name the Score location it came from.
    std::vector<ScoreTime> note_positions;
    for (const auto& part : score.parts) {
        std::uint8_t channel = part.definition.rendering.midi_channel;

        // MIDI 1.0 defines Damper/Sustain (CC64) and Soft Pedal (CC67) as
        // channel switches. Only directives with an exact range interpretation
        // are projected; every other technique remains explicit report evidence.
        for (const auto& directive : part.part_directives) {
            std::optional<std::uint8_t> controller;
            if (directive.directive == DirectiveType::SustainingPedal)
                controller = 64;
            else if (directive.directive == DirectiveType::UnaCorda)
                controller = 67;

            if (!controller) {
                report.diagnostics.push_back(
                    {"PartDirective has no configured lossless MIDI control projection: " +
                         std::string{directive_name(directive.directive)},
                     directive.start,
                     part.id});
                continue;
            }

            const auto start = score_time_to_absolute_beat(directive.start, score.time_map);
            const auto end = score_time_to_absolute_beat(directive.end, score.time_map);
            if (!start || !end) {
                report.diagnostics.push_back(
                    {"Pedal PartDirective dropped: temporal conversion failed",
                     directive.start,
                     part.id});
                continue;
            }
            midi.control_changes.push_back(MidiControlChangeData{
                absolute_beat_to_tick(*start, ppq), channel, *controller, 127, part.id});
            midi.control_changes.push_back(MidiControlChangeData{
                absolute_beat_to_tick(*end, ppq), channel, *controller, 0, part.id});
        }

        // Voice-anchored pedal marks are notation placements for one physical,
        // Part-scoped damper pedal. S23 proves one coherent range sequence and,
        // when PartDirective carries the same intent, requires exact agreement.
        std::vector<std::pair<ScoreTime, DirectionType>> pedal_points;
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* direction = std::get_if<ScoreDirection>(&event.payload);
                    if (!direction || (direction->type != DirectionType::PedalDown &&
                                       direction->type != DirectionType::PedalUp))
                        continue;
                    pedal_points.emplace_back(ScoreTime{measure.bar_number, event.offset},
                                              direction->type);
                }
            }
        }
        std::sort(pedal_points.begin(), pedal_points.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });
        bool direction_pedal_active = false;
        for (const auto& [position, type] : pedal_points) {
            const auto absolute = score_time_to_absolute_beat(position, score.time_map);
            if (!absolute) {
                report.diagnostics.push_back(
                    {"Pedal Direction dropped: temporal conversion failed", position, part.id});
                continue;
            }
            const bool down = type == DirectionType::PedalDown;
            midi.control_changes.push_back(
                MidiControlChangeData{absolute_beat_to_tick(*absolute, ppq),
                                      channel,
                                      64,
                                      static_cast<std::uint8_t>(down ? 127 : 0),
                                      part.id});
            direction_pedal_active = down;
        }
        if (direction_pedal_active) {
            const ScoreTime score_end{score.metadata.total_bars + 1, Beat::zero()};
            const auto absolute_end = score_time_to_absolute_beat(score_end, score.time_map);
            if (!absolute_end) {
                report.diagnostics.push_back(
                    {"Terminal PedalDown could not be closed at score end", score_end, part.id});
            } else {
                midi.control_changes.push_back(MidiControlChangeData{
                    absolute_beat_to_tick(*absolute_end, ppq), channel, 64, 0, part.id});
            }
        }

        // Collect all voice indices used across this part's measures
        std::set<std::uint8_t> voice_indices;
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                voice_indices.insert(v.voice_index);
            }
        }

        for (std::uint8_t vi : voice_indices) {
            // Build flat event list for this voice across all measures
            std::vector<FlatNote> flat;
            for (const auto& m : part.measures) {
                for (const auto& v : m.voices) {
                    if (v.voice_index != vi) continue;
                    for (const auto& event : v.events) {
                        const auto* ng = event.as_note_group();
                        if (!ng) continue;
                        flat.push_back({ScoreTime{m.bar_number, event.offset}, ng, event.id});
                    }
                }
            }

            // Track consumed notes for tie handling
            std::set<std::pair<std::uint64_t, std::size_t>> consumed;
            DynamicLevel current_dynamic = DynamicLevel::mf;

            for (std::size_t fi = 0; fi < flat.size(); ++fi) {
                const auto& fn = flat[fi];
                const NoteGroup& ng = *fn.ng;

                for (std::size_t ni = 0; ni < ng.notes.size(); ++ni) {
                    const Note& note = ng.notes[ni];

                    // Update the running semantic dynamic tracker. A numeric
                    // explicit velocity does not change later score dynamics.
                    if (note.dynamic)
                        current_dynamic = *note.dynamic;
                    else if (note.velocity.written)
                        current_dynamic = *note.velocity.written;

                    // A tied continuation has no new attack, but a semantic
                    // dynamic stored at its position still governs later
                    // attacks in the Voice.
                    if (consumed.contains({fn.event_id.value, ni})) continue;

                    // Resolve the physical attack/release boundary before a
                    // target-domain drop so one invalid chain is not
                    // miscounted or retriggered as several independent notes.
                    const auto tie = resolve_tie_projection(flat, fi, ni, consumed);
                    if (!tie) return std::unexpected(tie.error());
                    Beat total_duration = tie->duration;

                    int midi_note = midi_value(note.pitch);
                    if (midi_note < 0 || midi_note > 127) {
                        report.dropped_notes++;
                        report.diagnostics.push_back({"Note dropped: MIDI note " +
                                                          std::to_string(midi_note) +
                                                          " out of range [0, 127]",
                                                      fn.position,
                                                      part.id});
                        continue;
                    }

                    // Accumulate duration through tie chain for ordinary notes.
                    // Structurally valid grace notes cannot carry ties. Attack
                    // state comes from the first segment; Note Off intensity is
                    // owned by the terminal segment where release occurs.
                    // Convert to ticks
                    auto structural_start =
                        score_time_to_absolute_beat(fn.position, score.time_map);
                    if (!structural_start) {
                        report.dropped_notes++;
                        report.diagnostics.push_back(
                            {"Note dropped: temporal conversion failed", fn.position, part.id});
                        continue;
                    }

                    Beat sounding_start = *structural_start;
                    if (note.grace) {
                        auto grace = resolve_grace_timing(*note.grace, sounding_start, ng.duration);
                        if (!grace) return std::unexpected(grace.error());
                        sounding_start = grace->start_time;
                        total_duration = grace->duration;
                    }

                    // Round both endpoints on the absolute lattice rather than
                    // rounding the duration on its own: the end tick of one
                    // note is then exactly the start tick of the next, so a
                    // repeated key never overlaps its successor (issue #9).
                    const auto sounding_end = checked_add(sounding_start, total_duration);
                    if (!sounding_end) return std::unexpected(sounding_end.error());
                    std::int64_t start_tick = absolute_beat_to_tick(sounding_start, ppq);
                    std::int64_t dur_ticks = absolute_beat_to_tick(*sounding_end, ppq) - start_tick;

                    double velocity_value = resolve_dynamic_velocity(
                        note, current_dynamic, part, fn.position, score.time_map);
                    long double duration_scale = 1.0L;
                    if (note.articulation) {
                        const auto mapping =
                            part.definition.rendering.articulation_map.find(*note.articulation);
                        if (mapping == part.definition.rendering.articulation_map.end()) {
                            apply_default_articulation_velocity(*note.articulation, velocity_value);
                            duration_scale = articulation_duration_factor(*note.articulation);
                            ++report.articulations_defaulted;
                        } else {
                            auto mapped = apply_articulation_mapping(mapping->second,
                                                                     start_tick,
                                                                     channel,
                                                                     part.id,
                                                                     velocity_value,
                                                                     duration_scale,
                                                                     midi);
                            if (!mapped) return std::unexpected(mapped.error());
                            ++report.articulation_mappings_applied;
                        }
                    }

                    const auto velocity =
                        finalise_velocity(velocity_value, score, part, fn.position);

                    const long double scaled_duration =
                        static_cast<long double>(dur_ticks) * duration_scale;
                    if (!std::isfinite(scaled_duration) ||
                        scaled_duration >
                            static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
                        return std::unexpected(ErrorCode::ArithmeticOverflow);
                    dur_ticks = static_cast<std::int64_t>(std::llround(scaled_duration));
                    if (dur_ticks < 1) dur_ticks = 1;
                    if (start_tick < 0 ||
                        start_tick > std::numeric_limits<std::int64_t>::max() - dur_ticks)
                        return std::unexpected(ErrorCode::ArithmeticOverflow);

                    midi.notes.push_back(MidiNoteData{start_tick,
                                                      dur_ticks,
                                                      channel,
                                                      static_cast<std::uint8_t>(midi_note),
                                                      velocity,
                                                      part.id,
                                                      tie->release_velocity,
                                                      fn.event_id,
                                                      ni});
                    note_positions.push_back(fn.position);
                }
            }
        }
    }

    auto unison_residuals = collapse_nested_unisons(midi.notes, note_positions);
    report.diagnostics.insert(
        report.diagnostics.end(), unison_residuals.begin(), unison_residuals.end());

    // Sort notes by tick for sequential consumption
    std::stable_sort(midi.notes.begin(),
                     midi.notes.end(),
                     [](const MidiNoteData& a, const MidiNoteData& b) { return a.tick < b.tick; });
    sort_and_deduplicate_events(midi.keyswitches, [](const MidiKeyswitchData& event) {
        return std::tuple{event.tick,
                          event.channel,
                          event.note,
                          event.velocity,
                          event.duration_ticks,
                          event.part_id.value};
    });
    sort_and_deduplicate_events(midi.control_changes, [](const MidiControlChangeData& event) {
        return std::tuple{
            event.tick, event.channel, event.controller, event.value, event.part_id.value};
    });
    sort_and_deduplicate_events(midi.program_changes, [](const MidiProgramChangeData& event) {
        return std::tuple{event.tick, event.channel, event.program, event.part_id.value};
    });

    return CompiledMidiResult{std::move(midi), std::move(report)};
}

// =============================================================================
// compile_to_note_events
// =============================================================================

Result<NoteEventResult> compile_to_note_events(const Score& score) {
    if (!is_compilable(score)) return std::unexpected(ErrorCode::InvariantViolation);
    for (const auto& diagnostic : validate_rendering(score)) {
        if (diagnostic.severity == ValidationSeverity::Error)
            return std::unexpected(ErrorCode::InvalidRenderingConfig);
    }
    auto local_meter_residuals = local_meter_performance_residuals(score);
    if (!local_meter_residuals) return std::unexpected(local_meter_residuals.error());
    if (local_meter_residuals->size() > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    std::vector<NoteEvent> events;
    CompilationReport report;
    report.tuning_definitions_requested = 1;
    if (is_standard_midi_tuning(score.tuning)) {
        report.tuning_definitions_written = 1;
    } else {
        report.diagnostics.push_back(
            {"Score tuning was not written: NoteEvent carries a nominal MIDI note index but no "
             "sounding-pitch function",
             std::nullopt,
             std::nullopt});
    }
    report.dropped_time_sig_events = static_cast<std::uint32_t>(local_meter_residuals->size());
    report.diagnostics.insert(
        report.diagnostics.end(), local_meter_residuals->begin(), local_meter_residuals->end());

    for (const auto& part : score.parts) {
        std::set<std::uint8_t> voice_indices;
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                voice_indices.insert(voice.voice_index);

        for (const std::uint8_t voice_index : voice_indices) {
            std::vector<FlatNote> flat;
            for (const auto& measure : part.measures) {
                for (const auto& voice : measure.voices) {
                    if (voice.voice_index != voice_index) continue;
                    for (const auto& event : voice.events) {
                        const auto* group = event.as_note_group();
                        if (!group) continue;
                        flat.push_back(
                            {ScoreTime{measure.bar_number, event.offset}, group, event.id});
                    }
                }
            }

            ConsumedNoteSet consumed;
            DynamicLevel current_dynamic = DynamicLevel::mf;
            for (std::size_t flat_index = 0; flat_index < flat.size(); ++flat_index) {
                const auto& flat_note = flat[flat_index];
                for (std::size_t note_index = 0; note_index < flat_note.ng->notes.size();
                     ++note_index) {
                    const Note& note = flat_note.ng->notes[note_index];
                    if (note.dynamic)
                        current_dynamic = *note.dynamic;
                    else if (note.velocity.written)
                        current_dynamic = *note.velocity.written;

                    if (consumed.contains({flat_note.event_id.value, note_index})) continue;

                    const auto tie = resolve_tie_projection(flat, flat_index, note_index, consumed);
                    if (!tie) return std::unexpected(tie.error());

                    const auto absolute_start =
                        score_time_to_absolute_beat(flat_note.position, score.time_map);
                    if (!absolute_start) {
                        report.dropped_notes++;
                        report.diagnostics.push_back(
                            {"Note event dropped: temporal conversion failed",
                             flat_note.position,
                             part.id});
                        continue;
                    }

                    const int midi_pitch = midi_value(note.pitch);
                    const auto pitch = MidiNote::from_int(midi_pitch);
                    if (!pitch) {
                        report.dropped_notes++;
                        report.diagnostics.push_back({"Note dropped: MIDI note " +
                                                          std::to_string(midi_pitch) +
                                                          " out of range [0, 127]",
                                                      flat_note.position,
                                                      part.id});
                        continue;
                    }

                    NoteEvent event;
                    event.pitch = *pitch;
                    event.start_time = *absolute_start;
                    event.duration = tie->duration;
                    if (note.grace) {
                        const auto grace = resolve_grace_timing(
                            *note.grace, *absolute_start, flat_note.ng->duration);
                        if (!grace) return std::unexpected(grace.error());
                        event.start_time = grace->start_time;
                        event.duration = grace->duration;
                    }

                    NoteEventArticulationProjection articulation{resolve_dynamic_velocity(
                        note, current_dynamic, part, flat_note.position, score.time_map)};
                    if (note.articulation) {
                        const auto mapping =
                            part.definition.rendering.articulation_map.find(*note.articulation);
                        if (mapping == part.definition.rendering.articulation_map.end()) {
                            apply_default_articulation_velocity(*note.articulation,
                                                                articulation.velocity);
                            articulation.duration_scale =
                                articulation_duration_ratio(*note.articulation);
                            ++report.articulations_defaulted;
                        } else {
                            const auto mapped = apply_note_event_articulation_mapping(
                                mapping->second, articulation);
                            if (!mapped) return std::unexpected(mapped.error());
                            ++report.articulation_mappings_applied;
                        }
                    }

                    const auto shaped_duration =
                        checked_mul(event.duration, articulation.duration_scale);
                    if (!shaped_duration) return std::unexpected(shaped_duration.error());
                    if (*shaped_duration <= Beat::zero())
                        return std::unexpected(ErrorCode::InvalidRenderingConfig);
                    event.duration = *shaped_duration;
                    event.velocity =
                        finalise_velocity(articulation.velocity, score, part, flat_note.position);
                    event.muted = false;
                    event.release_velocity = tie->release_velocity;
                    events.push_back(event);

                    if (articulation.omitted_control_events > 0) {
                        report.diagnostics.push_back(
                            {"NoteEvent omitted " +
                                 std::to_string(articulation.omitted_control_events) +
                                 " MIDI articulation control event(s): NoteEvent represents "
                                 "note-local pitch, timing, velocity, mute, and release velocity "
                                 "but has no keyswitch, CC, or Program Change field",
                             flat_note.position,
                             part.id});
                    }
                }
            }
        }
    }

    // Preserve deterministic Part/Voice/note order among simultaneous attacks.
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return a.start_time < b.start_time;
    });

    return NoteEventResult{std::move(events), std::move(report)};
}

} // namespace sunny::core
