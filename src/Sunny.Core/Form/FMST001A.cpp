/**
 * @file FMST001A.cpp
 * @brief Form Structure Types and Classification implementation
 *
 * Component: FMST001A
 */

#include "FMST001A.h"

#include <algorithm>
#include <unordered_set>

namespace Sunny::Core {

namespace {

// Check if label is a variant of base (e.g., "A'" or "A''" is a variant of "A")
bool is_variant_of(const std::string& label, const std::string& base) {
    if (label == base) return true;
    if (label.size() <= base.size()) return false;
    // Check if label starts with base followed by only apostrophes
    if (label.substr(0, base.size()) != base) return false;
    for (std::size_t i = base.size(); i < label.size(); ++i) {
        if (label[i] != '\'') return false;
    }
    return true;
}

// Get the base label (strip trailing apostrophes)
std::string base_label(const std::string& label) {
    auto end = label.find_last_not_of('\'');
    if (end == std::string::npos) return label;
    return label.substr(0, end + 1);
}

// Count pitch similarity between two motifs/phrases (ratio of matching pitches)
double pitch_similarity(const Phrase& a, const Phrase& b) {
    // Compare first few pitches from each phrase
    std::vector<MidiNote> pitches_a, pitches_b;
    for (const auto& sp : a.subphrases) {
        for (const auto& m : sp.motifs) {
            for (auto p : m.pitches) pitches_a.push_back(p);
        }
    }
    for (const auto& sp : b.subphrases) {
        for (const auto& m : sp.motifs) {
            for (auto p : m.pitches) pitches_b.push_back(p);
        }
    }

    if (pitches_a.empty() || pitches_b.empty()) return 0.0;

    // Compare up to the shorter length
    std::size_t compare_len = std::min(pitches_a.size(), pitches_b.size());
    // For parallel period detection, compare roughly the first half
    compare_len = std::min(compare_len, std::max<std::size_t>(compare_len / 2, 1));

    int matches = 0;
    for (std::size_t i = 0; i < compare_len; ++i) {
        if (pitches_a[i] == pitches_b[i]) ++matches;
    }
    return static_cast<double>(matches) / static_cast<double>(compare_len);
}

}  // namespace

// =============================================================================
// §10.2 Phrase Classification
// =============================================================================

PhraseType classify_period(
    const Phrase& antecedent,
    const Phrase& consequent,
    double similarity_threshold
) {
    // A period requires HC→PAC pattern
    if (antecedent.cadence != CadenceType::HC) return PhraseType::Unclassified;
    if (consequent.cadence != CadenceType::PAC) return PhraseType::Unclassified;

    // Check if parallel or contrasting
    double sim = pitch_similarity(antecedent, consequent);
    if (sim >= similarity_threshold) {
        return PhraseType::ParallelPeriod;
    }
    return PhraseType::ContrastingPeriod;
}

bool is_sentence(const PhraseGroup& group) {
    // A sentence requires at least 2 phrases:
    //   presentation (basic idea + repetition) and continuation
    if (group.phrases.size() < 2) return false;

    // Total bar count should be 8 (or a multiple/proportion)
    int total_bars = 0;
    for (const auto& p : group.phrases) {
        total_bars += p.bar_count;
    }

    // Standard sentence is 8 bars; accept 4 or 8 or 16
    if (total_bars != 4 && total_bars != 8 && total_bars != 16) return false;

    // Presentation is first half, continuation is second half
    int presentation_bars = 0;
    std::size_t split = 0;
    for (std::size_t i = 0; i < group.phrases.size(); ++i) {
        presentation_bars += group.phrases[i].bar_count;
        if (presentation_bars >= total_bars / 2) {
            split = i + 1;
            break;
        }
    }

    // Presentation should be exactly half
    if (presentation_bars != total_bars / 2) return false;

    // The last phrase (continuation) should end with a cadence
    if (group.phrases.back().cadence == CadenceType::None) return false;

    // Presentation should contain a basic idea and its repetition/variant
    // (at least 2 subphrases or motifs in the first half)
    int pres_motif_count = 0;
    for (std::size_t i = 0; i < split; ++i) {
        for (const auto& sp : group.phrases[i].subphrases) {
            pres_motif_count += static_cast<int>(sp.motifs.size());
        }
    }
    if (pres_motif_count < 2) return false;

    return true;
}

// =============================================================================
// §10.3 Sectional Form Classification
// =============================================================================

SectionalForm classify_form(std::span<const std::string> section_labels) {
    if (section_labels.empty()) return SectionalForm::Unknown;

    const auto n = section_labels.size();

    // Single section
    if (n == 1) return SectionalForm::Unknown;

    // Check if all labels are identical → Strophic
    bool all_same = true;
    for (std::size_t i = 1; i < n; ++i) {
        if (section_labels[i] != section_labels[0]) {
            all_same = false;
            break;
        }
    }
    if (all_same) return SectionalForm::Strophic;

    // Check if all labels are variants of the first → Theme and Variations
    bool all_variants = true;
    std::string first_base = base_label(std::string(section_labels[0]));
    for (std::size_t i = 1; i < n; ++i) {
        if (!is_variant_of(std::string(section_labels[i]), first_base)) {
            all_variants = false;
            break;
        }
    }
    if (all_variants && n >= 2) return SectionalForm::ThemeVariations;

    // Check for Sonata: look for "Exposition", "Development", "Recapitulation" labels
    bool has_expo = false, has_dev = false, has_recap = false;
    for (const auto& label : section_labels) {
        if (label == "Exposition") has_expo = true;
        if (label == "Development") has_dev = true;
        if (label == "Recapitulation") has_recap = true;
    }
    if (has_expo && has_dev && has_recap) return SectionalForm::Sonata;

    // Count how many times the first section's base appears
    int first_count = 0;
    for (const auto& label : section_labels) {
        if (is_variant_of(std::string(label), first_base)) ++first_count;
    }

    // Rondo: first section appears 3+ times, alternating with contrasting sections
    if (first_count >= 3) return SectionalForm::Rondo;

    // A B → Binary
    if (n == 2) return SectionalForm::Binary;

    // A B A → Ternary or Rounded Binary
    if (n == 3) {
        std::string lb0 = base_label(std::string(section_labels[0]));
        std::string lb2 = base_label(std::string(section_labels[2]));
        std::string lb1 = base_label(std::string(section_labels[1]));
        if (lb0 == lb2 && lb0 != lb1) {
            if (is_variant_of(std::string(section_labels[2]), lb0)
                && section_labels[2] != section_labels[0]) {
                return SectionalForm::RoundedBinary;
            }
            return SectionalForm::Ternary;
        }
    }

    // Longer: check for A X A pattern
    if (n > 3) {
        std::string lb0 = base_label(std::string(section_labels[0]));
        std::string lb_last = base_label(std::string(section_labels[n - 1]));
        if (lb0 == lb_last && first_count == 2) {
            if (is_variant_of(std::string(section_labels[n - 1]), lb0)
                && section_labels[n - 1] != section_labels[0]) {
                return SectionalForm::RoundedBinary;
            }
            return SectionalForm::Ternary;
        }
    }

    // No repetition at all → Through-composed
    std::unordered_set<std::string> unique_bases;
    for (const auto& label : section_labels) {
        unique_bases.insert(base_label(std::string(label)));
    }
    if (unique_bases.size() == n) return SectionalForm::ThroughComposed;

    return SectionalForm::Unknown;
}

}  // namespace Sunny::Core
