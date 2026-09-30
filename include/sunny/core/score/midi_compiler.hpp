/**
 * @file midi_compiler.hpp
 * @brief Score IR MIDI compilation — end-to-end pipeline from Score to MIDI data
 *
 *
 * Consumes temporal conversion, velocity resolution, articulation duration,
 * and tick quantisation to produce MIDI event data from a Score document.
 * The output type CompiledMidi lives in the core score layer
 * to avoid a reverse dependency from Core to Infrastructure.
 *
 * Precondition: is_compilable(score) — structural validation passes. MIDI
 * rendering-domain errors are also blocking; explicitly degradable musical
 * events are retained in CompilationReport instead of being silently lost.
 */

#pragma once

#include <cstdint>
#include <string>
#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

// =============================================================================
// Compilation diagnostics and report
// =============================================================================

/**
 * @brief Diagnostic from compilation pipeline
 *
 * Records a single dropped or degraded event with location context.
 */
struct CompilationDiagnostic {
    std::string message;
    std::optional<ScoreTime> location;
    std::optional<PartId> part;
};

/**
 * @brief Report of dropped or degraded events during compilation
 *
 * Replaces the silent continues that previously absorbed errors in the
 * compilation pipeline. Each counter corresponds to a category of drop;
 * the diagnostics vector provides per-event detail.
 */
struct CompilationReport {
    std::uint32_t dropped_notes = 0;
    std::uint32_t dropped_tempo_events = 0;
    std::uint32_t dropped_time_sig_events = 0;
    std::uint32_t dropped_key_sig_events = 0;
    /// Global plus nonredundant Part-local metre events presented to this target.
    std::uint64_t time_signature_events_requested = 0;
    /// Complete SMF Time Signature meta-events emitted by this target.
    std::uint64_t time_signature_events_written = 0;
    /// Non-default ordered group partitions presented to this target.
    std::uint64_t time_signature_groupings_requested = 0;
    /// Ordered group partitions represented by an actual target grouping field.
    std::uint64_t time_signature_groupings_written = 0;
    std::uint32_t tuning_definitions_requested = 0;
    std::uint32_t tuning_definitions_written = 0;
    std::uint64_t articulation_mappings_applied = 0;
    std::uint64_t articulations_defaulted = 0;
    std::vector<CompilationDiagnostic> diagnostics;

    [[nodiscard]] bool has_drops() const noexcept {
        return dropped_notes > 0 || dropped_tempo_events > 0 || dropped_time_sig_events > 0 ||
               dropped_key_sig_events > 0 ||
               time_signature_events_written < time_signature_events_requested ||
               time_signature_groupings_written < time_signature_groupings_requested ||
               tuning_definitions_written < tuning_definitions_requested;
    }

    /// True for any counted omission or diagnostic-only degradation. A diagnostic
    /// is compilation residual evidence even when the target emitted a usable
    /// approximation and no dropped-event counter was incremented.
    [[nodiscard]] bool has_residuals() const noexcept {
        return has_drops() || !diagnostics.empty();
    }
};

// =============================================================================
// Compiled MIDI data types
// =============================================================================

struct MidiNoteData {
    std::int64_t tick;
    std::int64_t duration_ticks;
    std::uint8_t channel;               ///< From Part::definition.rendering.midi_channel
    std::uint8_t note;                  ///< MIDI note number [0, 127]
    std::uint8_t velocity;              ///< Resolved velocity [1, 127]
    PartId part_id{};                   ///< Source part, retained for per-track deployment
    std::uint8_t release_velocity = 64; ///< Score-owned Note Off intensity [0, 127]
};

struct MidiKeyswitchData {
    std::int64_t tick;
    std::int64_t duration_ticks;
    std::uint8_t channel;  ///< Human-facing MIDI channel [1, 16]
    std::uint8_t note;     ///< Keyswitch MIDI note [0, 127]
    std::uint8_t velocity; ///< Keyswitch attack velocity [1, 127]
    PartId part_id{};
};

struct MidiControlChangeData {
    std::int64_t tick;
    std::uint8_t channel;    ///< Human-facing MIDI channel [1, 16]
    std::uint8_t controller; ///< MIDI controller [0, 127]
    std::uint8_t value;      ///< MIDI controller value [0, 127]
    PartId part_id{};
};

struct MidiProgramChangeData {
    std::int64_t tick;
    std::uint8_t channel; ///< Human-facing MIDI channel [1, 16]
    std::uint8_t program; ///< MIDI program [0, 127]
    PartId part_id{};
};

struct MidiTempoData {
    std::int64_t tick;
    std::uint32_t microseconds_per_beat; ///< SMF microseconds per quarter note, 1..0xFFFFFF
};

struct MidiTimeSigData {
    std::int64_t tick;
    std::uint8_t numerator;
    std::uint8_t denominator;                ///< Actual denominator (for example, 4 for 4/4)
    std::uint8_t clocks_per_metronome_click; ///< SMF `cc`; 24 clocks equal one MIDI quarter
    std::uint8_t notated_32nds_per_quarter;  ///< SMF `bb`: normally 8
};

/**
 * @brief Deterministic lowering of Sunny metre into SMF's metronome-click field
 *
 * `cc` is a metronome interval, not an ordered grouping field. The booleans
 * describe how closely that click grid follows Sunny's group boundaries; they
 * do not claim that an SMF consumer can recover TimeSignature::groups().
 */
struct SmfTimeSignatureProjection {
    std::uint8_t clocks_per_metronome_click = 24;
    bool exact_group_boundary_grid = false;
    bool uniform_group_span = false;
};

[[nodiscard]] SmfTimeSignatureProjection
project_smf_time_signature(const TimeSignature& signature) noexcept;

struct MidiKeySigData {
    std::int64_t tick;
    std::int8_t accidentals; ///< SMF signed fifths count in [-7, 7]
    bool minor;              ///< SMF mode bit: false = major, true = minor
};

struct CompiledMidi {
    std::uint16_t ppq = 480;
    std::vector<MidiNoteData> notes;
    std::vector<MidiKeyswitchData> keyswitches;
    std::vector<MidiControlChangeData> control_changes;
    std::vector<MidiProgramChangeData> program_changes;
    std::vector<MidiTempoData> tempos;
    std::vector<MidiTimeSigData> time_signatures;
    std::vector<MidiKeySigData> key_signatures;
};

/**
 * @brief Result of MIDI compilation, including both data and report
 */
struct CompiledMidiResult {
    CompiledMidi midi;
    CompilationReport report;
};

/**
 * @brief Result of NoteEvent compilation
 */
struct NoteEventResult {
    std::vector<NoteEvent> events;
    CompilationReport report;
};

/**
 * @brief Sounding interval selected for a grace-note allocation
 *
 * Score IR stores grace notes inside an ordinary positive structural slot.
 * The renderer resolves that slot into an absolute sounding interval.
 */
struct GraceTiming {
    Beat start_time;
    Beat duration;
};

/// Maximum sounding duration of an acciaccatura: one thirty-second note.
inline constexpr Beat MAX_ACCIACCATURA_DURATION{1, 32};

/**
 * @brief Resolve the normative grace-note sounding-time policy
 *
 * An appoggiatura occupies its full structural allocation. An acciaccatura
 * occupies at most MAX_ACCIACCATURA_DURATION and is right-aligned to the end
 * of its allocation.
 *
 * @param type Grace-note class
 * @param allocation_start Absolute start of the structural allocation
 * @param allocation_duration Positive structural/written duration
 * @return Exact absolute sounding start and duration
 */
[[nodiscard]] Result<GraceTiming>
resolve_grace_timing(GraceType type, Beat allocation_start, Beat allocation_duration);

// =============================================================================
// Compilation API
// =============================================================================

/**
 * @brief Compile a Score to MIDI event data
 *
 * Traverses all parts, resolving velocity (base dynamic + hairpin
 * interpolation + articulation offset + orchestration balance),
 * duration (articulation factor, tie chain accumulation), and
 * position (ScoreTime -> AbsoluteBeat -> tick).
 *
 * @param score Source score; must satisfy is_compilable()
 * @param ppq Pulses per quarter note in [1, 65535] (default 480)
 * @return CompiledMidi, InvalidMidiPPQ, or an invariant error
 */
[[nodiscard]] Result<CompiledMidiResult> compile_to_midi(const Score& score, int ppq = 480);

/**
 * @brief Flatten a Score to a sequence of NoteEvents
 *
 * Walks all parts, measures, voices, and events; converts each NoteGroup note
 * to a NoteEvent with absolute beat timing via score_time_to_absolute_beat,
 * pitch via midi_value, and the same dynamic/hairpin/balance velocity stages
 * as MIDI compilation. Grace timing, default articulation velocity/duration,
 * and custom velocity/duration mapping stages are applied to the note-local
 * fields. Custom float duration factors must have an exact Beat representation;
 * MIDI-only keyswitch, CC, and Program Change mapping events are reported as
 * explicit residuals. Each validated tie chain becomes one event whose attack
 * comes from the first segment and whose duration/release velocity reach the
 * terminal segment. NoteEvent has no tie metadata, so emitting each segment
 * would create false retriggers. Rest-only bars produce zero events.
 *
 * @param score Source score
 * @return Sorted vector of NoteEvents, or error if temporal conversion fails
 */
[[nodiscard]] Result<NoteEventResult> compile_to_note_events(const Score& score);

} // namespace sunny::core
