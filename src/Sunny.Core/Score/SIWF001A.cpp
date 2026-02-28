/**
 * @file SIWF001A.cpp
 * @brief Score IR composition workflow functions — implementation
 *
 * Component: SIWF001A
 * Domain: SI (Score IR) | Category: WF (Workflow)
 */

#include "SIWF001A.h"
#include "SIVD001A.h"
#include "../Harmony/HRRN001A.h"
#include "../Scale/SCDF001A.h"

#include <algorithm>
#include <limits>

namespace Sunny::Core {

namespace {

std::uint64_t g_wf_event_id = 3000000;
std::uint64_t g_wf_part_id  = 500;
std::uint64_t g_wf_section_id = 1;

EventId next_wf_event_id() { return EventId{g_wf_event_id++}; }
PartId  next_wf_part_id()  { return PartId{g_wf_part_id++}; }

void push_undo(UndoStack* undo, Score& score, std::string desc,
               std::function<VoidResult()> forward,
               std::function<VoidResult()> inverse) {
    if (!undo) return;
    UndoEntry entry{score.version, std::move(forward), std::move(inverse), std::move(desc)};
    if (undo->group_depth > 0) {
        undo->pending_group.push_back(std::move(entry));
    } else {
        undo->undo_entries.push_back(std::move(entry));
        undo->redo_entries.clear();
    }
}

}  // namespace

// =============================================================================
// create_score
// =============================================================================

Result<Score> create_score(const ScoreSpec& spec) {
    if (spec.total_bars == 0)
        return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.parts.empty())
        return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.bpm < Constants::TEMPO_MIN_BPM || spec.bpm > Constants::TEMPO_MAX_BPM)
        return std::unexpected(ErrorCode::InvalidBPM);

    auto ts = make_time_signature(spec.time_sig_num, spec.time_sig_den);
    if (!ts) return std::unexpected(ts.error());

    Score score;
    score.id = ScoreId{1};
    score.metadata.title = spec.title;
    score.metadata.total_bars = spec.total_bars;

    // Tempo map: one entry at SCORE_START
    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(static_cast<std::int64_t>(spec.bpm));
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    // Key map: one entry at SCORE_START
    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = spec.key_root;
    key_entry.key.accidentals = spec.key_accidentals;
    score.key_map.push_back(key_entry);

    // Time signature map: one entry at bar 1
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    // Create parts with empty measures (whole-measure rests)
    Beat measure_dur = ts->measure_duration();
    for (const auto& part_def : spec.parts) {
        Part part;
        part.id = next_wf_part_id();
        part.definition = part_def;

        for (std::uint32_t bar = 1; bar <= spec.total_bars; ++bar) {
            RestEvent rest{measure_dur, true};
            Event event{next_wf_event_id(), Beat::zero(), rest};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            part.measures.push_back(std::move(measure));
        }

        score.parts.push_back(std::move(part));
    }

    // Validate structural integrity
    auto diags = validate_structural(score);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error)
            return std::unexpected(ErrorCode::InvariantViolation);
    }

    return score;
}

// =============================================================================
// set_formal_plan
// =============================================================================

Result<MutationResult> set_formal_plan(
    Score& score,
    std::vector<SectionDefinition> sections,
    UndoStack* undo
) {
    // Capture old section map for undo
    auto old_map = score.section_map;

    // Build new section map
    SectionMap new_map;
    for (const auto& def : sections) {
        if (def.start_bar == 0 || def.end_bar < def.start_bar)
            return std::unexpected(ErrorCode::InvalidBarRange);
        if (def.end_bar == std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);

        ScoreSection sec;
        sec.id = SectionId{g_wf_section_id++};
        sec.label = def.label;
        sec.start = ScoreTime{def.start_bar, Beat::zero()};
        sec.end = ScoreTime{def.end_bar + 1, Beat::zero()};
        sec.form_function = def.function;
        new_map.push_back(std::move(sec));
    }

    score.section_map = new_map;
    ++score.version;

    push_undo(undo, score, "set_formal_plan",
        [&score, new_map]() -> VoidResult {
            score.section_map = new_map;
            ++score.version;
            return {};
        },
        [&score, old_map]() -> VoidResult {
            score.section_map = old_map;
            ++score.version;
            return {};
        });

    return MutationResult{{}};
}

// =============================================================================
// set_section_harmony
// =============================================================================

Result<MutationResult> set_section_harmony(
    Score& score,
    const ScoreRegion& region,
    std::vector<ChordSymbolEntry> progression,
    UndoStack* undo
) {
    // Capture old annotations in the region for undo
    std::vector<HarmonicAnnotation> old_annotations;
    std::vector<std::size_t> old_indices;
    for (std::size_t i = 0; i < score.harmonic_annotations.size(); ++i) {
        const auto& ha = score.harmonic_annotations[i];
        if (ha.position >= region.start && ha.position < region.end) {
            old_annotations.push_back(ha);
            old_indices.push_back(i);
        }
    }

    // Remove old annotations in the region (reverse order to preserve indices)
    for (auto it = old_indices.rbegin(); it != old_indices.rend(); ++it) {
        score.harmonic_annotations.erase(score.harmonic_annotations.begin()
                                         + static_cast<std::ptrdiff_t>(*it));
    }

    // Determine key context for Roman numeral computation
    KeySignature key_ctx{};
    if (!score.key_map.empty()) {
        key_ctx = score.key_map[0].key;
        for (const auto& ke : score.key_map) {
            if (ke.position <= region.start) key_ctx = ke.key;
            else break;
        }
    }

    PitchClass key_pc = pc(key_ctx.root);
    bool is_minor = (key_ctx.accidentals < 0 && key_ctx.root.letter == 5) ||
                    false;  // Simplified minor detection; rely on caller's context

    // Build new annotations
    std::vector<HarmonicAnnotation> new_annotations;
    for (std::size_t i = 0; i < progression.size(); ++i) {
        const auto& entry = progression[i];

        HarmonicAnnotation ha;
        ha.position = entry.position;

        // Compute duration: extends to next chord or region end
        if (i + 1 < progression.size()) {
            // Approximate duration as difference in bar positions
            auto bar_diff = progression[i + 1].position.bar - entry.position.bar;
            ha.duration = Beat{static_cast<int64_t>(bar_diff > 0 ? bar_diff : 1), 1};
        } else {
            auto bar_diff = region.end.bar - entry.position.bar;
            ha.duration = Beat{static_cast<int64_t>(bar_diff > 0 ? bar_diff : 1), 1};
        }

        // Build chord voicing
        PitchClass root_pc = pc(entry.root);
        ha.chord.root = root_pc;
        ha.chord.quality = entry.quality;

        ha.key_context = key_ctx;

        // Compute Roman numeral
        auto scale = is_minor
            ? std::span<const Interval>(SCALE_MINOR.data(), SCALE_MINOR.size())
            : std::span<const Interval>(SCALE_MAJOR.data(), SCALE_MAJOR.size());

        auto numeral = chord_to_numeral(root_pc, entry.quality, key_pc, scale, is_minor);
        ha.roman_numeral = numeral.has_value() ? *numeral : entry.quality;

        // Classify harmonic function by parsing the Roman numeral to
        // extract the base degree, then mapping degree to function via
        // a lookup table. This handles all suffixed numerals (V7, iii7)
        // and case variations automatically.
        auto parsed = parse_roman_numeral_full(ha.roman_numeral);
        if (parsed.has_value()) {
            // Degree 0-6 → function: T, PD, T, PD, D, T, D
            static constexpr ScoreHarmonicFunction DEGREE_FUNCTION[7] = {
                ScoreHarmonicFunction::Tonic,        // I/i
                ScoreHarmonicFunction::Predominant,   // II/ii
                ScoreHarmonicFunction::Tonic,         // III/iii
                ScoreHarmonicFunction::Predominant,   // IV/iv
                ScoreHarmonicFunction::Dominant,       // V/v
                ScoreHarmonicFunction::Tonic,         // VI/vi
                ScoreHarmonicFunction::Dominant,       // VII/vii
            };
            int deg = parsed->degree;
            if (deg >= 0 && deg < 7)
                ha.function = DEGREE_FUNCTION[deg];
            else
                ha.function = ScoreHarmonicFunction::Ambiguous;
        } else {
            ha.function = ScoreHarmonicFunction::Ambiguous;
        }

        new_annotations.push_back(std::move(ha));
    }

    // Insert new annotations
    for (auto& ha : new_annotations) {
        score.harmonic_annotations.push_back(std::move(ha));
    }

    // Sort annotations by position
    std::sort(score.harmonic_annotations.begin(), score.harmonic_annotations.end(),
        [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
            return a.position < b.position;
        });

    ++score.version;

    // Capture the full post-mutation state for forward replay
    auto post_state = score.harmonic_annotations;

    push_undo(undo, score, "set_section_harmony",
        [&score, post_state]() -> VoidResult {
            score.harmonic_annotations = post_state;
            ++score.version;
            return {};
        },
        [&score, old_annotations, region]() -> VoidResult {
            // Inverse: remove annotations in region, restore old ones
            auto& annotations = score.harmonic_annotations;
            annotations.erase(
                std::remove_if(annotations.begin(), annotations.end(),
                    [&region](const HarmonicAnnotation& ha) {
                        return ha.position >= region.start && ha.position < region.end;
                    }),
                annotations.end());
            for (const auto& ha : old_annotations)
                annotations.push_back(ha);
            std::sort(annotations.begin(), annotations.end(),
                [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
                    return a.position < b.position;
                });
            ++score.version;
            return {};
        });

    return MutationResult{{}};
}

}  // namespace Sunny::Core
