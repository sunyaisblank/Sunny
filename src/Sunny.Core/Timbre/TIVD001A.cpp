/**
 * @file TIVD001A.cpp
 * @brief Timbre IR validation implementation
 *
 * Component: TIVD001A
 * Domain: TI (Timbre IR) | Category: VD (Validation)
 *
 * Tests: TSTI003A
 */

#include "TIVD001A.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Sunny::Core {

namespace {

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void add_diagnostic(std::vector<Diagnostic>& out,
                    ValidationSeverity sev,
                    const char* rule,
                    const std::string& msg,
                    int code,
                    std::optional<PartId> part = std::nullopt) {
    out.push_back({sev, rule, msg, std::nullopt, part, code});
}

// -------------------------------------------------------------------------
// T2: SoundSource validity
// -------------------------------------------------------------------------

void validate_source(const SoundSourceData& src,
                     std::vector<Diagnostic>& out,
                     PartId part) {
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, SubtractiveSynth>) {
            if (s.oscillators.empty()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "SubtractiveSynth requires at least one oscillator",
                    TimbreError::InvalidSource, part);
            }
            if (!s.oscillator_mix.empty() &&
                s.oscillator_mix.size() != s.oscillators.size()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "oscillator_mix size must match oscillator count",
                    TimbreError::InvalidSource, part);
            }
        }
        else if constexpr (std::is_same_v<T, FMSynth>) {
            if (s.operators.empty()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "FMSynth requires at least one operator",
                    TimbreError::InvalidSource, part);
            }
            if (s.algorithm.use_preset &&
                (s.algorithm.preset_number < 1 || s.algorithm.preset_number > 32)) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "FM algorithm preset number must be 1–32",
                    TimbreError::InvalidSource, part);
            }
        }
        else if constexpr (std::is_same_v<T, AdditiveSynth>) {
            if (s.partials.empty()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "AdditiveSynth requires at least one partial",
                    TimbreError::InvalidSource, part);
            }
        }
        else if constexpr (std::is_same_v<T, SamplerSource>) {
            if (s.library.empty()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "Sampler requires a non-empty library name",
                    TimbreError::InvalidSource, part);
            }
        }
        else if constexpr (std::is_same_v<T, HybridSource>) {
            if (s.layers.empty()) {
                add_diagnostic(out, ValidationSeverity::Error, "T2",
                    "HybridSource requires at least one layer",
                    TimbreError::InvalidSource, part);
            }
            for (const auto& layer : s.layers) {
                if (layer) {
                    validate_source(*layer, out, part);
                }
            }
        }
        // WavetableSynth, GranularSynth, PhysicalModelSource:
        // no structural minimums beyond default-constructed state
    }, src.data);
}

// -------------------------------------------------------------------------
// T3: Filter cutoff vs Nyquist
// -------------------------------------------------------------------------

void check_filter_nyquist(const Filter& f, float nyquist,
                          std::vector<Diagnostic>& out,
                          PartId part) {
    if (f.cutoff > nyquist) {
        add_diagnostic(out, ValidationSeverity::Warning, "T3",
            "Filter cutoff (" + std::to_string(static_cast<int>(f.cutoff)) +
            " Hz) exceeds Nyquist (" +
            std::to_string(static_cast<int>(nyquist)) + " Hz)",
            TimbreError::CutoffAboveNyquist, part);
    }
}

void check_filters_in_source(const SoundSourceData& src, float nyquist,
                              std::vector<Diagnostic>& out,
                              PartId part) {
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, SubtractiveSynth>) {
            check_filter_nyquist(s.filter, nyquist, out, part);
            if (s.filter_2) check_filter_nyquist(*s.filter_2, nyquist, out, part);
        }
        else if constexpr (std::is_same_v<T, WavetableSynth>) {
            if (s.filter) check_filter_nyquist(*s.filter, nyquist, out, part);
        }
        else if constexpr (std::is_same_v<T, SamplerSource>) {
            if (s.filter) check_filter_nyquist(*s.filter, nyquist, out, part);
        }
        else if constexpr (std::is_same_v<T, HybridSource>) {
            for (const auto& layer : s.layers) {
                if (layer) check_filters_in_source(*layer, nyquist, out, part);
            }
        }
    }, src.data);
}

// -------------------------------------------------------------------------
// T4: Oscillator detune
// -------------------------------------------------------------------------

void check_detune(const SoundSourceData& src,
                  std::vector<Diagnostic>& out,
                  PartId part) {
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, SubtractiveSynth>) {
            for (const auto& osc : s.oscillators) {
                if (std::abs(osc.tune_cents) > 100.0f) {
                    add_diagnostic(out, ValidationSeverity::Warning, "T4",
                        "Oscillator detune exceeds +/-100 cents",
                        TimbreError::ExcessiveDetune, part);
                }
            }
        }
        else if constexpr (std::is_same_v<T, HybridSource>) {
            for (const auto& layer : s.layers) {
                if (layer) check_detune(*layer, out, part);
            }
        }
    }, src.data);
}

// -------------------------------------------------------------------------
// T5: FM feedback stability
// -------------------------------------------------------------------------

constexpr float FM_FEEDBACK_THRESHOLD = 0.95f;

void check_fm_feedback(const SoundSourceData& src,
                       std::vector<Diagnostic>& out,
                       PartId part) {
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, FMSynth>) {
            if (std::abs(s.feedback) > FM_FEEDBACK_THRESHOLD) {
                add_diagnostic(out, ValidationSeverity::Warning, "T5",
                    "FM feedback (" + std::to_string(s.feedback) +
                    ") exceeds stability threshold (" +
                    std::to_string(FM_FEEDBACK_THRESHOLD) + ")",
                    TimbreError::FMFeedbackUnstable, part);
            }
        }
        else if constexpr (std::is_same_v<T, HybridSource>) {
            for (const auto& layer : s.layers) {
                if (layer) check_fm_feedback(*layer, out, part);
            }
        }
    }, src.data);
}

// -------------------------------------------------------------------------
// T6b: Envelope loop stage indices
// -------------------------------------------------------------------------

void check_envelope_loop(const Envelope& env, const char* context,
                         std::vector<Diagnostic>& out, PartId part) {
    if (!env.loop) return;
    auto stage_count = static_cast<std::uint8_t>(env.stages.size());
    if (env.loop->start_stage >= stage_count || env.loop->end_stage >= stage_count) {
        add_diagnostic(out, ValidationSeverity::Error, "T6b",
            std::string(context) + " envelope loop indices ("
            + std::to_string(env.loop->start_stage) + ", "
            + std::to_string(env.loop->end_stage)
            + ") exceed stage count " + std::to_string(stage_count),
            TimbreError::InvalidSource, part);
    }
    if (env.loop->start_stage > env.loop->end_stage) {
        add_diagnostic(out, ValidationSeverity::Error, "T6b",
            std::string(context) + " envelope loop start_stage > end_stage",
            TimbreError::InvalidSource, part);
    }
}

void check_envelope_loops(const TimbreProfile& profile,
                          std::vector<Diagnostic>& out) {
    PartId part = profile.part_id;
    // Check source envelopes
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>) {
            check_envelope_loop(s.amplifier, "SubtractiveSynth amplifier", out, part);
            if (s.filter.envelope) check_envelope_loop(*s.filter.envelope, "SubtractiveSynth filter", out, part);
            if (s.filter_2 && s.filter_2->envelope)
                check_envelope_loop(*s.filter_2->envelope, "SubtractiveSynth filter_2", out, part);
        }
        else if constexpr (std::is_same_v<T, FMSynth>) {
            for (std::size_t i = 0; i < s.operators.size(); ++i)
                check_envelope_loop(s.operators[i].envelope,
                    ("FMOperator[" + std::to_string(i) + "]").c_str(), out, part);
        }
    }, profile.source.data);
    // Check LFO envelopes not applicable (LFOs have no Envelope with loop)
}

// -------------------------------------------------------------------------
// T7: Modulation target path validation
//
// Checks that paths begin with a known prefix. Full resolution of
// indexed paths (e.g. source.oscillators[2]) requires knowing the
// current source variant and its array sizes; that is deferred to
// a future component.
// -------------------------------------------------------------------------

// Known second-level field names per domain prefix, validated to catch
// paths with valid prefixes but nonsensical continuations (e.g.
// "source.nonexistent"). Full indexed-path resolution (e.g.
// source.oscillators[2].tune_cents against the actual oscillator count)
// requires the source variant and is deferred to a future component.
bool is_valid_target_path(const std::string& path) {
    if (path.rfind("source.", 0) == 0) {
        static const char* known[] = {
            "oscillators", "filter", "filter_2", "amplifier", "noise",
            "operators", "algorithm", "wavetable", "position",
            "grain_size", "grain_density", "partials", "partial_count",
            "unison", "portamento", "oscillator_mix", "feedback",
            "layers", "library", "key_map"
        };
        auto rest = path.substr(7);
        for (const auto* k : known) {
            if (rest.rfind(k, 0) == 0) return true;
        }
        return false;
    }
    if (path.rfind("insert_chain.", 0) == 0) {
        return path.substr(13).rfind("effects", 0) == 0;
    }
    if (path.rfind("modulation.", 0) == 0) {
        static const char* known[] = {"lfos", "routings"};
        auto rest = path.substr(11);
        for (const auto* k : known) {
            if (rest.rfind(k, 0) == 0) return true;
        }
        return false;
    }
    if (path.rfind("semantic_descriptors.", 0) == 0) {
        return true;
    }
    return false;
}

void check_modulation_targets(const ModulationMatrix& matrix,
                               std::vector<Diagnostic>& out,
                               PartId part) {
    for (const auto& routing : matrix.routings) {
        if (!routing.target.empty() && !is_valid_target_path(routing.target)) {
            add_diagnostic(out, ValidationSeverity::Warning, "T7",
                "Modulation target '" + routing.target +
                "' does not match any known parameter path prefix",
                TimbreError::InvalidModTarget, part);
        }
    }
}

// -------------------------------------------------------------------------
// T9: Unmapped rendering parameters
//
// If a TimbreRenderingConfig has a device_type set but parameter_map
// is empty, warn that the render may not produce correct results.
// -------------------------------------------------------------------------

void check_rendering_config(const TimbreRenderingConfig& cfg,
                             std::vector<Diagnostic>& out,
                             PartId part) {
    if (!cfg.device_type.device_name.empty() && cfg.parameter_map.empty()) {
        add_diagnostic(out, ValidationSeverity::Warning, "T9",
            "TimbreRenderingConfig specifies a device but has no parameter mappings",
            TimbreError::UnmappedParameters, part);
    }
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::vector<Diagnostic> validate_timbre(
    const TimbreProfile& profile,
    float sample_rate_hz
) {
    std::vector<Diagnostic> diags;
    const float nyquist = sample_rate_hz / 2.0f;

    // T2: SoundSource validity
    validate_source(profile.source, diags, profile.part_id);

    // T3: Filter cutoff vs Nyquist (also check effect chain filters)
    check_filters_in_source(profile.source, nyquist, diags, profile.part_id);
    for (const auto& fx : profile.insert_chain.effects) {
        std::visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, DelayEffect>) {
                if (p.filter) check_filter_nyquist(*p.filter, nyquist,
                                                    diags, profile.part_id);
            }
            else if constexpr (std::is_same_v<T, CompressorEffect>) {
                if (p.sidechain && p.sidechain->filter) {
                    check_filter_nyquist(*p.sidechain->filter, nyquist,
                                         diags, profile.part_id);
                }
            }
        }, fx.parameters);
    }

    // T4: Oscillator detune
    check_detune(profile.source, diags, profile.part_id);

    // T5: FM feedback
    check_fm_feedback(profile.source, diags, profile.part_id);

    // T6: Effect chain cycle — structurally impossible with Vec; no-op check
    // Included for spec completeness.

    // T6b: Envelope loop stage indices
    check_envelope_loops(profile, diags);

    // T7: Modulation target paths
    check_modulation_targets(profile.modulation, diags, profile.part_id);

    // T8: Stale semantic descriptors (requires tracking parameter
    // change timestamps; emit Info if derivation is not Manual and
    // no mechanism exists to verify staleness yet)

    // T9: Rendering config
    check_rendering_config(profile.rendering, diags, profile.part_id);

    // Sort: Error first, then Warning, then Info
    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_timbre_correspondence(
    const Score& score,
    const std::vector<TimbreProfile>& profiles
) {
    std::vector<Diagnostic> diags;

    std::unordered_set<std::uint64_t> covered;
    for (const auto& p : profiles) {
        covered.insert(p.part_id.value);
    }

    for (const auto& part : score.parts) {
        if (covered.find(part.id.value) == covered.end()) {
            add_diagnostic(diags, ValidationSeverity::Error, "T1",
                "Part '" + part.definition.name +
                "' has no corresponding TimbreProfile",
                TimbreError::MissingProfile, part.id);
        }
    }

    return diags;
}

bool is_timbre_valid(const TimbreProfile& profile, float sample_rate_hz) {
    auto diags = validate_timbre(profile, sample_rate_hz);
    return std::none_of(diags.begin(), diags.end(), [](const Diagnostic& d) {
        return d.severity == ValidationSeverity::Error;
    });
}

}  // namespace Sunny::Core
