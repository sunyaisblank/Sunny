/**
 * @file ableton_score.hpp
 * @brief Ableton Live Compiler — Score IR to LOM commands
 *
 *
 * Compiles a validated Score IR document into a sequence of LOM bridge
 * commands that create the supported parts of an Ableton Live session:
 * tracks, clips, notes, initial tempo, pan, and section markers.
 *
 * Uses CompiledMidi for the musical content and translates
 * it to LOM operations via the abstract LomTransport.
 * Against CommandBuffer this is fully testable without Ableton;
 * against TcpTransport it drives a live session.
 *
 * Compilation steps (per Score IR Spec §9.2):
 *   1. Ensure Session scene 0 exists through Song.create_scene when needed
 *   2. Create one MIDI track per Part
 *   3. Create one clip per Part spanning the full score
 *   4. Inject notes from CompiledMidi into clips
 *   5. Set the initial Song tempo/signature
 *   6. Create section markers through the Sunny bridge adapter
 *   7. Set pan from RenderingConfig
 *
 * LOM cannot author tempo automation or load arbitrary preset paths.
 * Requested unsupported features are retained in the IR and reported in
 * AbletonCompilationResult::warnings rather than translated into fictional
 * LOM operations.
 *
 * Precondition:  Score passes is_compilable() (no Error-level diagnostics)
 * Postcondition: All supported operations are created via transport and any
 * unsupported requested operations are reported as warnings.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sunny/core/score/document.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/infrastructure/formats/ableton_evidence.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

// =============================================================================
// Compilation result
// =============================================================================

enum class AbletonCueAction : std::uint8_t { RecordedOnly, Created, Updated };

enum class AbletonNoteAction : std::uint8_t { RecordedOnly, Unsupported, Inserted };

enum class AbletonClipEnvelopeAction : std::uint8_t { RecordedOnly, Cleared };

[[nodiscard]] constexpr const char*
clip_envelope_action_name(AbletonClipEnvelopeAction action) noexcept {
    switch (action) {
    case AbletonClipEnvelopeAction::RecordedOnly:
        return "recorded_only";
    case AbletonClipEnvelopeAction::Cleared:
        return "cleared";
    }
    return "recorded_only";
}

[[nodiscard]] constexpr const char* note_action_name(AbletonNoteAction action) noexcept {
    switch (action) {
    case AbletonNoteAction::RecordedOnly:
        return "recorded_only";
    case AbletonNoteAction::Unsupported:
        return "unsupported";
    case AbletonNoteAction::Inserted:
        return "inserted";
    }
    return "recorded_only";
}

[[nodiscard]] constexpr const char* cue_action_name(AbletonCueAction action) noexcept {
    switch (action) {
    case AbletonCueAction::RecordedOnly:
        return "recorded_only";
    case AbletonCueAction::Created:
        return "created";
    case AbletonCueAction::Updated:
        return "updated";
    }
    return "recorded_only";
}

struct AbletonCueDeployment {
    double requested_time = 0.0;
    std::string requested_name;
    std::optional<double> observed_time;
    std::optional<std::string> observed_name;
    AbletonCueAction action = AbletonCueAction::RecordedOnly;
    bool verified = false;
};

struct AbletonClipEnvelopeDeployment {
    sunny::core::PartId part_id{};
    int track_index = 0;
    bool requested_has_envelopes = false;
    std::optional<bool> observed_has_envelopes;
    AbletonClipEnvelopeAction action = AbletonClipEnvelopeAction::RecordedOnly;
    bool verified = false;
};

struct AbletonNoteDeployment {
    sunny::core::PartId part_id{};
    int track_index = 0;
    std::uint64_t notes_requested = 0;
    struct RequestedNote {
        int pitch = 0;
        double start_time = 0.0;
        double duration = 0.0;
        double velocity = 0.0;
        bool muted = false;
        double probability = 1.0;
        double velocity_deviation = 0.0;
        double release_velocity = 64.0;
    };
    std::vector<RequestedNote> requested_notes;
    std::vector<int> created_note_ids;
    struct ObservedNote {
        int note_id = 0;
        int pitch = 0;
        double start_time = 0.0;
        double duration = 0.0;
        double velocity = 0.0;
        bool muted = false;
        double probability = 1.0;
        double velocity_deviation = 0.0;
        double release_velocity = 64.0;
    };
    std::vector<ObservedNote> observed_notes;
    AbletonNoteAction action = AbletonNoteAction::RecordedOnly;
    bool cardinality_verified = false;
    bool properties_verified = false;
    /** Live 11.0 ranged readback cannot exclude notes outside the queried interval. */
    bool entire_clip_population_observed = false;
    /** When present, readback covered [0, time_span) in quarter-note beats, all pitches. */
    std::optional<double> observed_time_span;
};

/** Parse one exact, closed Clip note-readback response without imposing expected membership. */
[[nodiscard]] sunny::core::Result<std::vector<AbletonNoteDeployment::ObservedNote>>
parse_ableton_note_readback(const LomResponse& response);

/** Select documented full readback (11.1+) or finite ranged readback (11.0). */
[[nodiscard]] LomRequest ableton_note_population_request(const LomPath& clip_path,
                                                         const AbletonTargetProfile& profile,
                                                         double clip_end);

/**
 * @brief Summary of an Ableton compilation
 */
struct AbletonCompilationResult {
    AbletonTargetProfile target_profile;
    sunny::core::ScoreTuning requested_tuning;
    std::uint64_t scenes_created = 0;
    std::uint64_t tracks_created = 0;
    std::uint64_t clips_created = 0;
    std::uint64_t clip_envelope_clears_requested = 0;
    std::uint64_t clip_envelope_clears_executed = 0;
    std::uint64_t clip_envelope_clears_verified = 0;
    std::uint64_t notes_requested = 0;
    std::uint64_t notes_written = 0;
    std::uint64_t note_batches_requested = 0;
    std::uint64_t note_batches_executed = 0;
    std::uint64_t note_ids_returned = 0;
    std::uint64_t note_batches_verified = 0;
    std::uint64_t notes_verified = 0;
    std::uint64_t articulation_control_events_requested = 0;
    std::uint64_t articulation_control_events_written = 0;
    std::uint64_t tempo_events_requested = 0;
    std::uint64_t tempo_events_written = 0;
    std::uint64_t time_signature_events_requested = 0;
    std::uint64_t time_signature_events_written = 0;
    std::uint64_t time_signature_groupings_requested = 0;
    std::uint64_t time_signature_groupings_written = 0;
    std::uint64_t key_signature_events_requested = 0;
    std::uint64_t key_signature_events_written = 0;
    std::uint64_t tuning_definitions_requested = 0;
    std::uint64_t tuning_definitions_written = 0;
    std::uint64_t section_nodes_total = 0;
    std::uint64_t section_nodes_projected = 0;
    std::uint64_t section_nodes_unprojected = 0;
    std::uint64_t markers_requested = 0;
    std::uint64_t markers_created = 0;
    std::uint64_t markers_updated = 0;
    std::uint64_t markers_verified = 0;
    std::uint64_t property_writes = 0;
    std::uint64_t property_writes_verified = 0;
    std::vector<AbletonPropertyDeployment> property_deployments;
    std::vector<AbletonClipEnvelopeDeployment> clip_envelope_deployments;
    std::vector<AbletonNoteDeployment> note_deployments;
    std::vector<AbletonCueDeployment> marker_deployments;
    sunny::core::CompilationReport midi_report;
    std::vector<std::string> warnings;
};

// =============================================================================
// Compiler API
// =============================================================================

/**
 * @brief Compile a Score IR document to Ableton Live via LOM transport
 *
 * @param score     Validated Score IR document
 * @param transport LOM transport (CommandBuffer for testing, TcpTransport for live)
 * @param ppq       Pulses per quarter note (default 480)
 * @return Compilation result or error
 */
[[nodiscard]] sunny::core::Result<AbletonCompilationResult>
compile_to_ableton(const sunny::core::Score& score, LomTransport& transport, int ppq = 480);

} // namespace sunny::infrastructure::formats
