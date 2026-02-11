/**
 * @file FMST001A.h
 * @brief Form Structure Types and Classification
 *
 * Component: FMST001A
 * Domain: FM (Form) | Category: ST (Structure)
 *
 * Formal Spec §10.1–10.3: Musical form modelled as a recursive grammar
 * over structural units. Provides types for representing formal units
 * (composition, section, phrase, motif) and classification of phrase
 * structures and sectional forms.
 *
 * Grammar (context-free):
 *   composition ::= section+
 *   section     ::= label ":" phrase_group+
 *   phrase_group ::= phrase+
 *   phrase      ::= subphrase+ cadence
 *   subphrase   ::= motif+
 *
 * Invariants:
 * - A phrase always terminates with a cadence type
 * - Sectional form classification is deterministic given section labels
 * - Sentence structure requires 1+1+2 bar grouping
 * - Period structure requires antecedent (HC) + consequent (PAC)
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// §10.3 Sectional Form Types
// =============================================================================

/**
 * @brief Classification of large-scale musical forms
 */
enum class SectionalForm {
    Binary,            ///< A B or A A' B B'
    RoundedBinary,     ///< A B A'
    Ternary,           ///< A B A
    Rondo,             ///< A B A C A ...
    Sonata,            ///< Exposition–Development–Recapitulation
    ThemeVariations,   ///< A A' A'' A''' ...
    Strophic,          ///< A A A A ... (same music, different text)
    ThroughComposed,   ///< A B C D ... (no large-scale repetition)
    Unknown            ///< Cannot classify
};

// =============================================================================
// §10.2 Phrase Structure Types
// =============================================================================

/**
 * @brief Cadence type terminating a phrase
 */
enum class CadenceType {
    PAC,       ///< Perfect authentic cadence
    IAC,       ///< Imperfect authentic cadence
    HC,        ///< Half cadence
    PC,        ///< Plagal cadence
    DC,        ///< Deceptive cadence
    Phrygian,  ///< Phrygian half cadence
    None       ///< No cadence identified
};

/**
 * @brief Phrase structure classification
 */
enum class PhraseType {
    Sentence,           ///< Satz: basic idea + repetition + continuation
    ParallelPeriod,     ///< Antecedent (HC) + consequent (PAC), similar opening
    ContrastingPeriod,  ///< Antecedent (HC) + consequent (PAC), different opening
    Hybrid,             ///< Mixed elements
    Unclassified        ///< Cannot classify
};

// =============================================================================
// §10.1 Formal Unit Types (Recursive Grammar)
// =============================================================================

/**
 * @brief A time span in beats
 */
struct TimeSpan {
    Beat start;
    Beat end;
};

/**
 * @brief A motif (smallest thematic unit)
 */
struct Motif {
    std::string label;                ///< Identifier (e.g., "a", "b", "a'")
    TimeSpan span;
    std::vector<MidiNote> pitches;    ///< Pitch content
    std::vector<Beat> durations;      ///< Rhythmic content
};

/**
 * @brief A subphrase (one or more motifs)
 */
struct SubPhrase {
    std::vector<Motif> motifs;
    TimeSpan span;
};

/**
 * @brief A phrase (subphrases + cadence)
 */
struct Phrase {
    std::vector<SubPhrase> subphrases;
    CadenceType cadence;
    TimeSpan span;
    int bar_count;                    ///< Number of bars in this phrase
};

/**
 * @brief A phrase group (one or more phrases)
 */
struct PhraseGroup {
    std::vector<Phrase> phrases;
    PhraseType phrase_type;           ///< Sentence, period, etc.
    TimeSpan span;
};

/**
 * @brief A section (labelled formal unit)
 */
struct Section {
    std::string label;                ///< "A", "B", "Development", etc.
    std::vector<PhraseGroup> phrase_groups;
    TimeSpan span;
};

/**
 * @brief A complete composition structure
 */
struct FormAnalysis {
    std::vector<Section> sections;
    SectionalForm form;
};

// =============================================================================
// §10.2 Phrase Classification
// =============================================================================

/**
 * @brief Classify a pair of phrases as a period
 *
 * A period requires:
 * - Antecedent ending with a half cadence
 * - Consequent ending with a perfect authentic cadence
 *
 * Parallel period: both phrases begin with similar material.
 * Contrasting period: phrases begin with different material.
 *
 * @param antecedent First phrase
 * @param consequent Second phrase
 * @param similarity_threshold Pitch similarity ratio [0,1] for parallel detection
 * @return Period type, or Unclassified if not a period
 */
[[nodiscard]] PhraseType classify_period(
    const Phrase& antecedent,
    const Phrase& consequent,
    double similarity_threshold = 0.75
);

/**
 * @brief Classify a phrase group as a sentence (Satz)
 *
 * A sentence requires:
 * - 8-bar total (or proportional grouping)
 * - Basic idea (2 bars) + repetition/variant (2 bars) = presentation (4 bars)
 * - Continuation (4 bars) ending with cadence
 *
 * @param group Phrase group to classify
 * @return true if the group matches sentence structure
 */
[[nodiscard]] bool is_sentence(const PhraseGroup& group);

// =============================================================================
// §10.3 Sectional Form Classification
// =============================================================================

/**
 * @brief Classify the sectional form from a sequence of section labels
 *
 * Uses pattern matching on the label sequence:
 * - "A B" → Binary
 * - "A B A'" → RoundedBinary
 * - "A B A" → Ternary
 * - "A B A C A" → Rondo (requires A appearing 3+ times)
 * - "A A' A'' ..." → ThemeVariations (all labels are variants of A)
 * - "A A A ..." → Strophic (all labels identical)
 * - No repetition → ThroughComposed
 *
 * @param section_labels Ordered sequence of section labels
 * @return Classified form
 */
[[nodiscard]] SectionalForm classify_form(
    std::span<const std::string> section_labels
);

/**
 * @brief Get the standard name for a sectional form
 */
[[nodiscard]] constexpr std::string_view form_name(SectionalForm form) noexcept {
    switch (form) {
        case SectionalForm::Binary:          return "Binary";
        case SectionalForm::RoundedBinary:   return "Rounded Binary";
        case SectionalForm::Ternary:         return "Ternary";
        case SectionalForm::Rondo:           return "Rondo";
        case SectionalForm::Sonata:          return "Sonata";
        case SectionalForm::ThemeVariations: return "Theme and Variations";
        case SectionalForm::Strophic:        return "Strophic";
        case SectionalForm::ThroughComposed: return "Through-composed";
        case SectionalForm::Unknown:         return "Unknown";
    }
    return "Unknown";
}

}  // namespace Sunny::Core
