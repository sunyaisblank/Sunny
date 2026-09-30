/**
 * @file workflows.hpp
 * @brief Score IR composition workflow functions
 *
 *
 * Provides entry-point functions for agent-driven composition workflows
 * per spec §12.2. These functions create or structurally reshape scores
 * at the formal level, complementing  (read-only queries) and
 *  (atomic mutations on existing scores).
 *
 * Functions:
 * - create_score: build a valid Score from a compact specification
 * - set_formal_plan: assign section structure to a score
 * - set_section_harmony: write harmonic annotations into a region
 *
 * Invariants:
 * - create_score output passes validate_structural with no Error diagnostics
 * - set_formal_plan and set_section_harmony are undoable
 */

#pragma once

#include <sunny/core/score/document.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/views.hpp>

namespace sunny::core {

// =============================================================================
// Score Creation (§12.2.1)
// =============================================================================

/**
 * @brief Compact specification for creating a new score
 */
struct ScoreSpec {
    /// Repository-selected root identity. One is the standalone default.
    ScoreId id{1};
    /// Complete source sounding-pitch function; defaults to 12-TET/A4=440.
    ScoreTuning tuning;
    std::string title;
    std::uint32_t total_bars;
    double bpm;
    SpelledPitch key_root;
    bool minor = false;
    /// Explicit traditional fifths count; when absent it is derived from root/mode.
    std::optional<std::int8_t> key_accidentals;
    int time_sig_num = 4;
    int time_sig_den = 4;
    std::vector<PartDefinition> parts;
};

/**
 * @brief Build a valid Score from a compact specification
 *
 * Constructs tempo, key, and time signature maps from the spec fields,
 * creates parts with empty measures (whole-measure rests), assigns
 * deterministic document-local sequential IDs, and validates the result.
 *
 * @param spec Score specification
 * @return Valid Score or error if validation fails
 */
[[nodiscard]] Result<Score> create_score(const ScoreSpec& spec);

// =============================================================================
// Formal Plan (§12.2.2)
// =============================================================================

/**
 * @brief Definition of a single formal section
 */
struct SectionDefinition {
    std::string label;
    std::uint32_t start_bar;
    std::uint32_t end_bar;
    std::optional<FormFunction> function;
};

/**
 * @brief Replace the section map with a formal plan
 *
 * Captures the old section map for undo, builds ScoreSection entries
 * from the definitions, and replaces score.section_map.
 *
 * @param score Target score (modified in place)
 * @param sections Section definitions
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_formal_plan(Score& score,
                                                     const std::vector<SectionDefinition>& sections,
                                                     UndoStack* undo = nullptr);

// =============================================================================
// Section Harmony (§12.2.3)
// =============================================================================

/**
 * @brief A chord symbol at a specific position
 */
struct ChordSymbolEntry {
    ScoreTime position;
    SpelledPitch root;                ///< Exact root register for generated voicing
    std::string quality;              ///< Registered chord quality
    std::optional<SpelledPitch> bass; ///< Exact chord-member bass; non-chord bass rejects
};

/**
 * @brief Write harmonic annotations into a region
 *
 * For each entry, creates a complete ascending HarmonicAnnotation voicing
 * from the exact root register and registered quality. An optional exact
 * chord-member bass constructs the corresponding inversion. The active key
 * at each entry—not merely the region start—owns Roman-numeral analysis.
 * Captures old annotations in the region for undo.
 *
 * @param score Target score (modified in place)
 * @param region Region to annotate
 * @param progression Chord progression entries
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_section_harmony(Score& score,
                                                         const ScoreRegion& region,
                                                         std::vector<ChordSymbolEntry> progression,
                                                         UndoStack* undo = nullptr);

// =============================================================================
// Workflow Types (§12.2.4)
// =============================================================================

/**
 * @brief A single melodic note for write_melody
 */
struct MelodyEntry {
    ScoreTime position;
    SpelledPitch pitch;
    Beat duration;
    std::optional<DynamicLevel> dynamic;
    std::optional<ArticulationType> articulation;
};

/**
 * @brief A single harmonic voicing for write_harmony
 *
 * Pitches are ordered bottom to top. Distribution across target parts
 * is handled by the workflow function.
 */
struct HarmonyEntry {
    ScoreTime position;
    std::vector<SpelledPitch> voicing; // bottom to top
    Beat duration;
};

// =============================================================================
// Standard instrument library (Score IR Appendix A)
// =============================================================================

/**
 * @brief Reference definition of an instrument from Score IR Appendix A
 *
 * `range` is in sounding pitch, because Score notes store concert pitch and
 * range validation compares them directly. `transposition` is the
 * written-to-sounding displacement in semitones.
 */
struct StandardInstrumentProfile {
    PitchRange range;
    Interval transposition = 0;
    Clef clef = Clef::Treble;
    bool listed = false; ///< False when Appendix A has no entry for the instrument
};

/**
 * @brief Appendix A profile of an instrument
 *
 * Postcondition: an instrument listed in Appendix A receives its tabulated
 * absolute and comfortable ranges, transposition and clef. An unlisted
 * instrument receives the full MIDI note range (C-1..G9), no transposition and
 * the treble clef, with `listed == false`; the range then constrains nothing
 * rather than inventing a limit the specification does not state.
 */
[[nodiscard]] StandardInstrumentProfile standard_instrument_profile(InstrumentType instrument);

// =============================================================================
// Default rendering channels
// =============================================================================

/// General MIDI reserves channel 10 for unpitched percussion key maps.
inline constexpr std::uint8_t GM_PERCUSSION_CHANNEL = 10;

/// True for instruments whose sound is a General MIDI percussion key map.
[[nodiscard]] constexpr bool uses_gm_percussion_channel(InstrumentType instrument) noexcept {
    return instrument_subfamily(instrument) == InstrumentSubfamily::UnpitchedPercussion ||
           instrument == InstrumentType::DrumMachine;
}

/**
 * @brief Channel a newly created Part receives when its creator names none
 *
 * Postcondition: unpitched percussion receives channel 10. Any other
 * instrument receives the lowest channel in 1..16, other than 10, that no
 * entry of @p channels_in_use occupies; when all fifteen are occupied it
 * shares the least-occupied one (lowest number on ties). Distinct channels
 * keep same-key notes of different Parts from contending for one SMF
 * channel/key pair.
 */
[[nodiscard]] std::uint8_t default_midi_channel(InstrumentType instrument,
                                                const std::vector<std::uint8_t>& channels_in_use);

// =============================================================================
// Composition Tool (§12.2.5)
// =============================================================================

/**
 * @brief Add a new part to the score, appended at the end
 *
 * @param score Target score (modified in place)
 * @param definition Part specification
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult>
add_part(Score& score, PartDefinition definition, UndoStack* undo = nullptr);

// =============================================================================
// Arrangement Tools (§12.2.6)
// =============================================================================

/**
 * @brief Write a melody line into a voice within a part
 *
 * @param score Target score (modified in place)
 * @param part_id Target part
 * @param voice_index Voice within the part
 * @param melody Sequence of melody entries
 * @param undo Optional undo stack
 * @return MutationResult or error if part not found
 */
[[nodiscard]] Result<MutationResult> write_melody(Score& score,
                                                  PartId part_id,
                                                  std::uint8_t voice_index,
                                                  const std::vector<MelodyEntry>& melody,
                                                  UndoStack* undo = nullptr);

/**
 * @brief Distribute harmonic voicings across target parts
 *
 * Assigns pitches from each HarmonyEntry bottom-to-top across the
 * target parts. When voicing.size() exceeds target_parts.size(),
 * surplus pitches are grouped as chords in the last part.
 *
 * @param score Target score (modified in place)
 * @param target_parts Parts to receive the voicing (bottom to top)
 * @param chords Harmonic entries
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> write_harmony(Score& score,
                                                   const std::vector<PartId>& target_parts,
                                                   const std::vector<HarmonyEntry>& chords,
                                                   UndoStack* undo = nullptr);

/**
 * @brief Copy note events from source to target part within a region
 *
 * @param score Target score (modified in place)
 * @param region Time region for the operation
 * @param source Source part
 * @param target Target part
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> reorchestrate(Score& score,
                                                   const ScoreRegion& region,
                                                   PartId source,
                                                   PartId target,
                                                   UndoStack* undo = nullptr);

/**
 * @brief Double a part at a diatonic interval into another part
 *
 * Each doubled pitch is apply_interval(source, interval): the letter advances
 * by interval.diatonic steps and the accidental absorbs the chromatic rest.
 *
 * @param score Target score (modified in place)
 * @param region Time region for the operation
 * @param source Source part
 * @param target Target part
 * @param interval Diatonic transposition interval (octave = 7 steps)
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> double_part(Score& score,
                                                 const ScoreRegion& region,
                                                 PartId source,
                                                 PartId target,
                                                 DiatonicInterval interval,
                                                 UndoStack* undo = nullptr);

/**
 * @brief Set the dynamic level for all note events in a region
 *
 * @param score Target score (modified in place)
 * @param region Region to modify
 * @param level Dynamic level to apply
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_dynamics(Score& score,
                                                  const ScoreRegion& region,
                                                  DynamicLevel level,
                                                  UndoStack* undo = nullptr);

/**
 * @brief Set the articulation for all notes in a region
 *
 * @param score Target score (modified in place)
 * @param region Region to modify
 * @param articulation Articulation to apply
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_articulation(Score& score,
                                                      const ScoreRegion& region,
                                                      ArticulationType articulation,
                                                      UndoStack* undo = nullptr);

// =============================================================================
// Detail Tools (§12.2.7)
// =============================================================================

/**
 * @brief Modify properties of an existing note within a NoteGroup
 *
 * Only the provided optional fields are changed; others are left intact.
 *
 * @param score Target score (modified in place)
 * @param event_id Event containing the note
 * @param note_index Index within the NoteGroup
 * @param pitch New pitch (if provided)
 * @param duration New duration (if provided)
 * @param velocity New velocity (if provided)
 * @param articulation New articulation (if provided)
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> modify_note(Score& score,
                                                 EventId event_id,
                                                 std::uint8_t note_index,
                                                 std::optional<SpelledPitch> pitch,
                                                 std::optional<Beat> duration,
                                                 std::optional<VelocityValue> velocity,
                                                 std::optional<ArticulationType> articulation,
                                                 UndoStack* undo = nullptr);

/**
 * @brief Modify note properties, including Score-owned Note Off velocity
 *
 * This overload retains the shorter signature above as a source-compatible
 * convenience for callers that do not modify release velocity.
 */
[[nodiscard]] Result<MutationResult> modify_note(Score& score,
                                                 EventId event_id,
                                                 std::uint8_t note_index,
                                                 std::optional<SpelledPitch> pitch,
                                                 std::optional<Beat> duration,
                                                 std::optional<VelocityValue> velocity,
                                                 std::optional<std::uint8_t> release_velocity,
                                                 std::optional<ArticulationType> articulation,
                                                 UndoStack* undo = nullptr);

/**
 * @brief Transpose a single event or an entire region by a diatonic interval
 *
 * @param score Target score (modified in place)
 * @param target Either a single EventId or a ScoreRegion
 * @param interval Diatonic interval for transposition
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> transpose(Score& score,
                                               std::variant<EventId, ScoreRegion> target,
                                               DiatonicInterval interval,
                                               UndoStack* undo = nullptr);

// =============================================================================
// Analysis Tools — read-only (§12.2.8)
// =============================================================================

/**
 * @brief Analyse harmonic content within a region
 *
 * @param score Source score (unmodified)
 * @param region Region to analyse
 * @return Harmonic annotations covering the region
 */
[[nodiscard]] std::vector<HarmonicAnnotation> analyze_harmony(const Score& score,
                                                              const ScoreRegion& region);

/**
 * @brief Retrieve orchestration annotations for a region
 *
 * @param score Source score (unmodified)
 * @param region Region to query
 * @return Part-role pairs active in the region
 */
[[nodiscard]] std::vector<std::pair<PartId, TexturalRole>>
get_orchestration(const Score& score, const ScoreRegion& region);

/**
 * @brief Produce a reduced view of the score
 *
 * Supported view types: "piano" (piano reduction), "short" (short score),
 * "skeleton" (harmonic skeleton). Unknown view types and invalid regions
 * are rejected.
 *
 * @param score Source score (unmodified)
 * @param result_id Repository-selected identity for the independent output Score
 * @param view_type Reduction type
 * @param region Optional region restriction
 * @return Reduced Score, or InvalidRegion/InvalidMutation
 */
[[nodiscard]] Result<Score> get_reduction(const Score& score,
                                          ScoreId result_id,
                                          const std::string& view_type,
                                          const std::optional<ScoreRegion>& region = std::nullopt);

/**
 * @brief Produce a condensed form summary of the score's sections
 *
 * @param score Source score (unmodified)
 * @return Form summary entries for each section
 */
[[nodiscard]] std::vector<FormSummaryEntry> get_form_summary(const Score& score);

} // namespace sunny::core
