/**
 * @file CIIN001A.cpp
 * @brief Corpus IR ingestion pipeline — implementation
 *
 * Component: CIIN001A
 *
 * Tests: TSCI007A
 */

#include "CIIN001A.h"

#include "../Format/FMMI001A.h"
#include "../Format/FMMX001A.h"

#include "Corpus/CIAN001A.h"
#include "Pitch/PTSP001A.h"
#include "Pitch/PTMN001A.h"
#include "Score/SIWF001A.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>
#include <set>

namespace Sunny::Infrastructure::Corpus {

using namespace Sunny::Core;
using namespace Sunny::Infrastructure::Format;

namespace {

// =========================================================================
// Krumhansl-Kessler key profiles
// =========================================================================

// Major key profile (Krumhansl-Kessler 1990)
constexpr std::array<float, 12> KK_MAJOR = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
    2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

// Minor key profile (Krumhansl-Kessler 1990)
constexpr std::array<float, 12> KK_MINOR = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
    2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

struct KeyEstimate {
    PitchClass tonic;
    bool is_minor;
    float confidence;
};

/// Pearson correlation between two arrays of length 12
float pearson12(const std::array<float, 12>& x, const std::array<float, 12>& y) {
    float mx = 0.0f, my = 0.0f;
    for (int i = 0; i < 12; ++i) { mx += x[i]; my += y[i]; }
    mx /= 12.0f; my /= 12.0f;

    float num = 0.0f, dx = 0.0f, dy = 0.0f;
    for (int i = 0; i < 12; ++i) {
        float a = x[i] - mx;
        float b = y[i] - my;
        num += a * b;
        dx += a * a;
        dy += b * b;
    }
    float denom = std::sqrt(dx * dy);
    return denom > 0.0f ? num / denom : 0.0f;
}

/// Estimate key from a pitch class histogram using Krumhansl-Schmuckler.
/// Correlates the observed PC distribution against all 24 major/minor
/// key profiles (one rotation per semitone) and returns the best match.
KeyEstimate estimate_key(const std::array<std::uint32_t, 12>& pc_hist) {
    // Convert histogram to float
    std::array<float, 12> observed{};
    std::uint32_t total = 0;
    for (int i = 0; i < 12; ++i) { observed[i] = static_cast<float>(pc_hist[i]); total += pc_hist[i]; }
    if (total == 0) return {0, false, 0.0f};

    float best_r = -2.0f;
    PitchClass best_tonic = 0;
    bool best_minor = false;

    for (int rotation = 0; rotation < 12; ++rotation) {
        // Rotate the profile to match this tonic
        std::array<float, 12> rotated_major{};
        std::array<float, 12> rotated_minor{};
        for (int i = 0; i < 12; ++i) {
            rotated_major[(i + rotation) % 12] = KK_MAJOR[i];
            rotated_minor[(i + rotation) % 12] = KK_MINOR[i];
        }

        float r_major = pearson12(observed, rotated_major);
        float r_minor = pearson12(observed, rotated_minor);

        if (r_major > best_r) {
            best_r = r_major;
            best_tonic = static_cast<PitchClass>(rotation);
            best_minor = false;
        }
        if (r_minor > best_r) {
            best_r = r_minor;
            best_tonic = static_cast<PitchClass>(rotation);
            best_minor = true;
        }
    }

    // Map correlation to confidence [0, 1]
    float confidence = std::clamp((best_r + 1.0f) / 2.0f, 0.0f, 1.0f);
    return {best_tonic, best_minor, confidence};
}

// =========================================================================
// Quantisation
// =========================================================================

struct QuantisedNote {
    Beat start;
    Beat duration;
    MidiNote pitch;
    Velocity velocity;
};

/// Quantise a beat value to the nearest grid division.
/// Grid is expressed as subdivisions per whole note (e.g. 16 = sixteenth notes).
Beat quantise_beat(Beat b, int grid) {
    // Grid in whole-note units: 1/grid
    Beat grid_unit{1, static_cast<std::int64_t>(grid)};
    // Round to nearest grid point
    double val = b.to_float();
    double grid_val = grid_unit.to_float();
    std::int64_t rounded = static_cast<std::int64_t>(std::round(val / grid_val));
    return Beat{rounded, static_cast<std::int64_t>(grid)};
}

/// Quantise NoteEvents and compute RMS residual.
std::pair<std::vector<QuantisedNote>, float> quantise_notes(
    const std::vector<NoteEvent>& events, int grid
) {
    std::vector<QuantisedNote> result;
    double sum_sq = 0.0;

    for (const auto& ev : events) {
        Beat q_start = quantise_beat(ev.start_time, grid);
        Beat q_dur = quantise_beat(ev.duration, grid);
        if (q_dur <= Beat::zero()) q_dur = Beat{1, static_cast<std::int64_t>(grid)};

        double residual = std::abs(ev.start_time.to_float() - q_start.to_float());
        sum_sq += residual * residual;

        result.push_back({q_start, q_dur, ev.pitch, ev.velocity});
    }

    float rms = events.empty() ? 0.0f :
        static_cast<float>(std::sqrt(sum_sq / static_cast<double>(events.size())));
    return {result, rms};
}

// =========================================================================
// Voice separation
// =========================================================================

struct VoicedNote {
    Beat start;
    Beat duration;
    MidiNote pitch;
    Velocity velocity;
    std::uint8_t voice;
};

/// Register-based voice separation: simultaneous notes sorted by pitch,
/// assigned to voices top-down. Returns confidence from overlap ratio.
std::pair<std::vector<VoicedNote>, float> separate_voices(
    const std::vector<QuantisedNote>& notes
) {
    if (notes.empty()) return {{}, 1.0f};

    // Group notes by start time
    std::map<Beat, std::vector<const QuantisedNote*>> by_onset;
    for (const auto& n : notes)
        by_onset[n.start].push_back(&n);

    std::vector<VoicedNote> result;
    std::uint32_t max_simul = 0;
    std::uint32_t overlap_count = 0;

    for (const auto& [onset, group] : by_onset) {
        // Sort by pitch descending (highest = voice 0)
        auto sorted = group;
        std::sort(sorted.begin(), sorted.end(),
            [](const QuantisedNote* a, const QuantisedNote* b) {
                return a->pitch > b->pitch;
            });

        if (sorted.size() > max_simul)
            max_simul = static_cast<std::uint32_t>(sorted.size());

        for (std::uint8_t v = 0; v < sorted.size(); ++v) {
            result.push_back({
                sorted[v]->start, sorted[v]->duration,
                sorted[v]->pitch, sorted[v]->velocity, v
            });
        }

        if (sorted.size() > 1) overlap_count++;
    }

    float confidence = by_onset.empty() ? 1.0f :
        1.0f - static_cast<float>(overlap_count) / static_cast<float>(by_onset.size());
    confidence = std::clamp(confidence, 0.3f, 1.0f);
    return {result, confidence};
}

// =========================================================================
// Score construction helpers
// =========================================================================

/// Build a Score from voiced, quantised notes with an estimated key.
Result<Score> build_score_from_notes(
    const std::vector<VoicedNote>& notes,
    const KeyEstimate& key_est,
    const std::string& title,
    const std::string& instrumentation,
    double bpm,
    int time_num, int time_den,
    std::uint32_t total_bars
) {
    // Determine key root spelling
    SpelledPitch key_root = default_spelling(key_est.tonic, 0, 4);

    // Build ScoreSpec
    ScoreSpec spec;
    spec.title = title;
    spec.total_bars = total_bars;
    spec.bpm = bpm;
    spec.key_root = key_root;
    spec.minor = key_est.is_minor;
    spec.time_sig_num = time_num;
    spec.time_sig_den = time_den;

    // Single part
    PartDefinition pd;
    pd.name = instrumentation;
    pd.abbreviation = instrumentation.substr(0, 3);
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    if (!score_result)
        return std::unexpected(score_result.error());

    Score score = std::move(*score_result);
    PartId part_id = score.parts[0].id;
    int key_lof = line_of_fifths_position(key_root);

    // Insert each note into the score
    for (const auto& vn : notes) {
        // Convert start beat to bar/offset
        // Beats are in whole-note units; bar duration = time_num / time_den whole notes
        double bar_dur = static_cast<double>(time_num) / static_cast<double>(time_den);
        double start_wn = vn.start.to_float();
        std::uint32_t bar_idx = static_cast<std::uint32_t>(start_wn / bar_dur);
        double offset_wn = start_wn - bar_idx * bar_dur;

        if (bar_idx >= total_bars) continue;

        Beat offset = Beat::from_float(offset_wn);
        SpelledPitch sp = default_spelling(
            static_cast<PitchClass>(vn.pitch % 12),
            key_lof,
            static_cast<std::int8_t>(vn.pitch / 12 - 1));

        Note note;
        note.pitch = sp;
        note.velocity = VelocityValue{std::nullopt, vn.velocity};

        // Clamp duration to not exceed measure
        Beat max_dur = Beat::from_float(bar_dur) - offset;
        Beat dur = vn.duration;
        if (dur > max_dur) dur = max_dur;
        if (dur <= Beat::zero()) dur = Beat{1, 16};

        auto insert_result = wf_insert_note(
            score, part_id, bar_idx + 1, vn.voice,
            offset, note, dur);
        // Insertion failures are non-fatal; skip the note
        (void)insert_result;
    }

    return score;
}

}  // anonymous namespace


// =========================================================================
// MIDI Ingestion
// =========================================================================

Result<IngestedWork> ingest_midi(
    std::span<const std::uint8_t> midi_data,
    IngestedWorkId work_id,
    const IngestionOptions& options
) {
    // Parse MIDI
    auto midi_result = parse_midi(midi_data);
    if (!midi_result)
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    const auto& midi = *midi_result;
    if (midi.notes.empty())
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    // Convert to NoteEvents
    auto note_events = midi_to_note_events(midi);
    if (note_events.empty())
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    // Build PC histogram for key estimation
    std::array<std::uint32_t, 12> pc_hist{};
    for (const auto& ev : note_events)
        pc_hist[ev.pitch % 12]++;

    auto key_est = estimate_key(pc_hist);

    // Quantise
    auto [quantised, rms_residual] = quantise_notes(note_events, options.quantise_grid);

    // Voice separation
    auto [voiced, voice_confidence] = separate_voices(quantised);

    // Determine timing from MIDI metadata
    double bpm = 120.0;
    if (!midi.tempos.empty())
        bpm = 60000000.0 / static_cast<double>(midi.tempos[0].microseconds_per_beat);

    int time_num = 4, time_den = 4;
    if (!midi.time_signatures.empty()) {
        time_num = midi.time_signatures[0].numerator;
        time_den = midi.time_signatures[0].denominator;
    }

    // Compute total bars from note extent
    double bar_dur = static_cast<double>(time_num) / static_cast<double>(time_den);
    double max_end = 0.0;
    for (const auto& vn : voiced) {
        double end = (vn.start + vn.duration).to_float();
        if (end > max_end) max_end = end;
    }
    auto total_bars = static_cast<std::uint32_t>(std::ceil(max_end / bar_dur));
    if (total_bars == 0) total_bars = 1;

    // Build Score
    auto score_result = build_score_from_notes(
        voiced, key_est, options.title, options.instrumentation,
        bpm, time_num, time_den, total_bars);

    if (!score_result)
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    // Populate IngestedWork
    IngestedWork work;
    work.id = work_id;
    work.score = std::move(*score_result);

    // Metadata
    work.metadata.title = options.title;
    work.metadata.instrumentation = options.instrumentation;
    work.metadata.source_format = "midi";

    // Confidence
    work.ingestion_confidence.key_confidence = key_est.confidence;
    work.ingestion_confidence.metre_confidence =
        midi.time_signatures.empty() ? 0.8f : 1.0f;
    work.ingestion_confidence.spelling_confidence =
        std::clamp(key_est.confidence * 0.9f, 0.5f, 1.0f);
    work.ingestion_confidence.voice_separation_confidence = voice_confidence;
    work.ingestion_confidence.quantisation_residual = rms_residual;
    work.ingestion_confidence.source_format = "midi";

    // Run analysis
    work.analysis = analyze_score(*work.score);
    work.analysis_complete = true;

    return work;
}


// =========================================================================
// MusicXML Ingestion
// =========================================================================

Result<IngestedWork> ingest_musicxml(
    std::string_view musicxml,
    IngestedWorkId work_id,
    const IngestionOptions& options
) {
    auto xml_result = parse_musicxml(musicxml);
    if (!xml_result)
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    const auto& xml_score = *xml_result;
    if (xml_score.parts.empty())
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    // Extract key from first measure if available
    KeyEstimate key_est{0, false, 1.0f};
    int time_num = 4, time_den = 4;
    bool found_key = false;

    for (const auto& part : xml_score.parts) {
        for (const auto& measure : part.measures) {
            if (measure.key_tonic && !found_key) {
                key_est.tonic = pc(*measure.key_tonic);
                key_est.is_minor = measure.key_is_major.has_value() && !*measure.key_is_major;
                key_est.confidence = 1.0f;
                found_key = true;
            }
            if (measure.time_signature) {
                time_num = measure.time_signature->first;
                time_den = measure.time_signature->second;
            }
        }
    }

    // If no explicit key, estimate from note content
    if (!found_key) {
        std::array<std::uint32_t, 12> pc_hist{};
        for (const auto& part : xml_score.parts)
            for (const auto& measure : part.measures)
                for (const auto& note : measure.notes)
                    if (!note.is_rest) pc_hist[pc(note.pitch)]++;
        key_est = estimate_key(pc_hist);
    }

    // Determine total bars from the first part
    std::uint32_t total_bars = 0;
    if (!xml_score.parts.empty())
        total_bars = static_cast<std::uint32_t>(xml_score.parts[0].measures.size());
    if (total_bars == 0) total_bars = 1;

    // Build Score via ScoreSpec
    SpelledPitch key_root = default_spelling(key_est.tonic, 0, 4);

    ScoreSpec spec;
    spec.title = options.title.empty() ? xml_score.title : options.title;
    spec.total_bars = total_bars;
    spec.bpm = 120.0;
    spec.key_root = key_root;
    spec.minor = key_est.is_minor;
    spec.time_sig_num = time_num;
    spec.time_sig_den = time_den;

    // Create one part per MusicXML part
    for (const auto& xml_part : xml_score.parts) {
        PartDefinition pd;
        pd.name = xml_part.name.empty() ? options.instrumentation : xml_part.name;
        pd.abbreviation = pd.name.substr(0, 3);
        pd.instrument_type = InstrumentType::Piano;
        pd.clef = Clef::Treble;
        spec.parts.push_back(pd);
    }
    if (spec.parts.empty()) {
        PartDefinition pd;
        pd.name = options.instrumentation;
        pd.abbreviation = pd.name.substr(0, 3);
        pd.instrument_type = InstrumentType::Piano;
        pd.clef = Clef::Treble;
        spec.parts.push_back(pd);
    }

    auto score_result = create_score(spec);
    if (!score_result)
        return std::unexpected(static_cast<ErrorCode>(CorpusError::IngestionFailed));

    Score score = std::move(*score_result);

    // Populate notes from MusicXML into Score
    for (std::size_t pi = 0; pi < xml_score.parts.size() && pi < score.parts.size(); ++pi) {
        PartId part_id = score.parts[pi].id;
        const auto& xml_part = xml_score.parts[pi];

        for (std::size_t mi = 0; mi < xml_part.measures.size() && mi < total_bars; ++mi) {
            const auto& xml_measure = xml_part.measures[mi];
            Beat offset = Beat::zero();

            for (const auto& xml_note : xml_measure.notes) {
                if (xml_note.is_rest) {
                    if (!xml_note.is_chord)
                        offset = offset + xml_note.duration;
                    continue;
                }

                Note note;
                note.pitch = xml_note.pitch;
                note.velocity = VelocityValue{std::nullopt, 80};

                auto v = static_cast<std::uint8_t>(std::max(0, xml_note.voice - 1));

                auto ins = wf_insert_note(
                    score, part_id,
                    static_cast<std::uint32_t>(mi + 1),
                    v, offset, note, xml_note.duration);
                (void)ins;

                if (!xml_note.is_chord)
                    offset = offset + xml_note.duration;
            }
        }
    }

    // Populate IngestedWork
    IngestedWork work;
    work.id = work_id;
    work.score = std::move(score);

    work.metadata.title = spec.title;
    work.metadata.instrumentation = options.instrumentation;
    work.metadata.source_format = "musicxml";

    work.ingestion_confidence.key_confidence = key_est.confidence;
    work.ingestion_confidence.metre_confidence = 1.0f;
    work.ingestion_confidence.spelling_confidence = 1.0f;
    work.ingestion_confidence.voice_separation_confidence = 1.0f;
    work.ingestion_confidence.quantisation_residual = 0.0f;
    work.ingestion_confidence.source_format = "musicxml";

    work.analysis = analyze_score(*work.score);
    work.analysis_complete = true;

    return work;
}

}  // namespace Sunny::Infrastructure::Corpus
