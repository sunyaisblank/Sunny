/**
 * @file VLCN001A.h
 * @brief Voice-leading constraint checker
 *
 * Component: VLCN001A
 * Domain: VL (VoiceLeading) | Category: CN (Constraint)
 *
 * Checks voice-leading transitions against common-practice rules.
 * Each rule can be configured as Error, Warning, or Preference severity.
 *
 * Invariants:
 * - default_constraint_config() matches §7.4 severity table
 * - Rules are checked independently and reported as violations
 * - A clean transition produces an empty violation list
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Pitch/PTPC001A.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Sunny::Core {

/**
 * @brief Severity level for constraint violations
 */
enum class ConstraintSeverity {
    Error,       ///< Must not occur
    Warning,     ///< Should be avoided
    Preference   ///< Stylistic preference
};

/**
 * @brief Voice-leading constraint rules
 */
enum class VLConstraintRule {
    NoParallelFifths,
    NoParallelOctaves,
    NoDirectFifthsOctaves,
    LeadingToneResolution,
    SeventhResolvesDown,
    NoVoiceCrossing,
    NoLargeLeaps,
    StepwiseAfterLeap,
    CompleteChords,
    DoubleTheRoot,
    NoDoubleLeadingTone
};

/**
 * @brief A single constraint violation
 */
struct ConstraintViolation {
    VLConstraintRule rule;
    ConstraintSeverity severity;
    int voice_index;      ///< Primary voice involved (-1 if N/A)
    int voice_index_2;    ///< Second voice involved (-1 if N/A)
    std::string description;
};

/**
 * @brief Voice range specification
 */
struct VoiceRange {
    MidiNote low;
    MidiNote high;
};

/**
 * @brief Configuration for constraint checking
 */
struct VLConstraintConfig {
    std::unordered_map<VLConstraintRule, ConstraintSeverity> severities;
    std::vector<VoiceRange> voice_ranges;
    PitchClass key_root = 0;
    bool is_minor = false;
    int max_leap = 12;         ///< Maximum interval for outer voices
    int max_inner_leap = 7;    ///< Maximum interval for inner voices
};

/**
 * @brief Get default constraint configuration
 *
 * Matches common-practice defaults from §7.4.
 */
[[nodiscard]] VLConstraintConfig default_constraint_config();

/**
 * @brief Check voice-leading between two chord voicings
 *
 * @param prev Previous chord voicing (MIDI notes, ascending)
 * @param next Next chord voicing (MIDI notes, ascending)
 * @param prev_chord Previous ChordVoicing metadata
 * @param next_chord Next ChordVoicing metadata
 * @param config Constraint configuration
 * @return Vector of violations (empty if clean)
 */
[[nodiscard]] std::vector<ConstraintViolation> check_voice_leading(
    const ChordVoicing& prev_chord,
    const ChordVoicing& next_chord,
    const VLConstraintConfig& config
);

/**
 * @brief Check if any violations are errors
 */
[[nodiscard]] bool has_errors(const std::vector<ConstraintViolation>& violations);

/**
 * @brief Filter violations by severity
 */
[[nodiscard]] std::vector<ConstraintViolation> filter_by_severity(
    const std::vector<ConstraintViolation>& violations,
    ConstraintSeverity severity
);

}  // namespace Sunny::Core
