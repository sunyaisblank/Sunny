/**
 * @file FMAL001A.cpp
 * @brief Ableton Live Compiler — Score IR to LOM commands
 *
 * Component: FMAL001A
 *
 * Compiles Score IR through two phases:
 *   Phase 1: compile_to_midi (SICM001A) handles velocity resolution,
 *            articulation, temporal conversion — all musical logic.
 *   Phase 2: this file translates CompiledMidi into LOM commands
 *            that create the Ableton session structure.
 *
 * The separation means phase 2 contains no musical decision-making;
 * it is a structural mapping from MIDI data to DAW operations.
 */

#include "FMAL001A.h"

#include "../../Sunny.Core/Score/SITM001A.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Sunny::Infrastructure::Format {

using namespace Sunny::Core;

namespace {

/// Compute the total score length in ticks for clip duration
std::int64_t score_length_ticks(const Score& score, int ppq) {
    // Sum measure durations
    Beat total = Beat::zero();
    for (const auto& ts_entry : score.time_map) {
        // Each time signature entry governs from its bar until the next entry
        Beat measure_dur = ts_entry.time_signature.measure_duration();
        std::uint32_t next_bar = score.metadata.total_bars + 1;
        for (const auto& other : score.time_map) {
            if (other.bar > ts_entry.bar && other.bar < next_bar) {
                next_bar = other.bar;
            }
        }
        std::uint32_t bar_count = next_bar - ts_entry.bar;
        // multiply measure_dur by bar_count
        total = total + Beat{measure_dur.numerator * static_cast<int64_t>(bar_count),
                             measure_dur.denominator};
    }
    return absolute_beat_to_tick(total, ppq);
}

/// Convert tick to Ableton beat position (Ableton uses quarter notes)
double tick_to_ableton_beats(std::int64_t tick, int ppq) {
    return static_cast<double>(tick) / static_cast<double>(ppq);
}

/// Convert MidiNoteData to LomNoteData
LomNoteData to_lom_note(const MidiNoteData& n, int ppq) {
    LomNoteData data;
    data.pitch = n.note;
    data.start_time = tick_to_ableton_beats(n.tick, ppq);
    data.duration = tick_to_ableton_beats(n.duration_ticks, ppq);
    data.velocity = n.velocity;
    data.muted = false;
    return data;
}

}  // anonymous namespace


Result<AbletonCompilationResult> compile_to_ableton(
    const Score& score,
    LomTransport& transport,
    int ppq
) {
    AbletonCompilationResult result;

    // Phase 1: MIDI compilation (all musical logic)
    auto midi_result = compile_to_midi(score, ppq);
    if (!midi_result) return std::unexpected(midi_result.error());

    auto& compiled = midi_result->midi;
    result.midi_report = midi_result->report;

    // Compute score duration for clip lengths
    std::int64_t total_ticks = score_length_ticks(score, ppq);
    double clip_length_beats = tick_to_ableton_beats(total_ticks, ppq);
    if (clip_length_beats <= 0.0) clip_length_beats = 4.0;  // minimum 1 bar

    // Phase 2: Group notes by MIDI channel (each Part has a unique channel)
    // Channel → list of MIDI notes
    std::map<std::uint8_t, std::vector<const MidiNoteData*>> notes_by_channel;
    for (const auto& note : compiled.notes) {
        notes_by_channel[note.channel].push_back(&note);
    }

    // Build track index: Part index → (channel, track name)
    struct TrackInfo {
        int track_index;
        std::string name;
        std::uint8_t channel;
        const RenderingConfig* rendering;
    };
    std::vector<TrackInfo> tracks;
    for (std::size_t i = 0; i < score.parts.size(); ++i) {
        const auto& part = score.parts[i];
        TrackInfo ti;
        ti.track_index = static_cast<int>(i);
        ti.name = part.definition.name;
        ti.channel = part.definition.rendering.midi_channel;
        ti.rendering = &part.definition.rendering;
        tracks.push_back(ti);
    }

    // Step 1: Set tempo
    if (!score.tempo_map.empty()) {
        const auto& first_tempo = score.tempo_map[0];
        double bpm = first_tempo.bpm.to_float();
        transport.send(LomProtocol::set_property(
            LomPaths::song(), "tempo", bpm));
        result.tempo_events_written++;

        // Additional tempo changes as automation
        for (std::size_t i = 1; i < compiled.tempos.size(); ++i) {
            // Tempo automation would be written as clip automation
            // For now, we record each tempo change as a set_property at the
            // appropriate position. Full automation envelope support requires
            // Ableton's clip automation API.
            double tempo_bpm = 60000000.0 / compiled.tempos[i].microseconds_per_beat;
            transport.send(LomProtocol::set_property(
                LomPaths::song(), "tempo", tempo_bpm));
            result.tempo_events_written++;
        }
    }

    // Step 2: Create tracks and clips
    for (const auto& ti : tracks) {
        // Create MIDI track
        transport.send(LomProtocol::call_method(
            LomPaths::song(), "create_midi_track",
            {ti.track_index}));
        result.tracks_created++;

        // Set track name
        transport.send(LomProtocol::set_property(
            LomPaths::track(ti.track_index), "name",
            ti.name));

        // Load instrument preset if specified
        if (ti.rendering->instrument_preset) {
            transport.send(LomProtocol::set_property(
                LomPaths::track(ti.track_index), "instrument_preset",
                *ti.rendering->instrument_preset));
        }

        // Set pan if specified
        if (ti.rendering->pan) {
            transport.send(LomProtocol::set_property(
                LomPaths::track(ti.track_index).child("mixer"), "panning",
                static_cast<double>(*ti.rendering->pan)));
        }

        // Create clip in slot 0 spanning full score
        transport.send(LomProtocol::call_method(
            LomPaths::clip_slot(ti.track_index, 0), "create_clip",
            {clip_length_beats}));
        result.clips_created++;

        // Set clip name
        transport.send(LomProtocol::set_property(
            LomPaths::clip(ti.track_index, 0), "name",
            ti.name));

        // Inject notes for this channel
        auto ch_it = notes_by_channel.find(ti.channel);
        if (ch_it != notes_by_channel.end()) {
            std::vector<LomNoteData> lom_notes;
            lom_notes.reserve(ch_it->second.size());
            for (const auto* n : ch_it->second) {
                lom_notes.push_back(to_lom_note(*n, ppq));
            }
            transport.send_notes(
                LomPaths::clip(ti.track_index, 0),
                lom_notes);
            result.notes_written += static_cast<std::uint32_t>(lom_notes.size());
        }
    }

    // Step 3: Section markers from SectionMap
    for (const auto& section : score.section_map) {
        auto tick_result = score_time_to_tick(section.start, score.time_map, ppq);
        if (tick_result) {
            double beat_pos = tick_to_ableton_beats(*tick_result, ppq);
            transport.send(LomProtocol::call_method(
                LomPaths::song(), "set_or_delete_cue",
                {beat_pos, section.label}));
            result.markers_created++;
        }
    }

    return result;
}

}  // namespace Sunny::Infrastructure::Format
