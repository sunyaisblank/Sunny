/**
 * @file FMLY002A.cpp
 * @brief LilyPond compilation from Score IR — implementation
 *
 * Component: FMLY002A
 */

#include "FMLY002A.h"
#include "FMLY001A.h"

#include "../../Sunny.Core/Score/SIDC001A.h"
#include "../../Sunny.Core/Score/SIVD001A.h"

#include <cmath>
#include <sstream>

namespace Sunny::Infrastructure::Format {

namespace {

using namespace Sunny::Core;

// =============================================================================
// LilyPond string escaping — prevent injection via metadata fields
// =============================================================================

std::string ly_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

// =============================================================================
// Minor key detection — same heuristic as SICM001A
// =============================================================================

bool is_minor_key(const KeySignature& key) {
    auto ints = key.mode.get_intervals();
    return ints.size() >= 3 && ints[2] == 3;
}

// =============================================================================
// Clef mapping
// =============================================================================

const char* ly_clef_name(Clef c) {
    switch (c) {
        case Clef::Treble:     return "treble";
        case Clef::Bass:       return "bass";
        case Clef::Alto:       return "alto";
        case Clef::Tenor:      return "tenor";
        case Clef::Percussion: return "percussion";
        case Clef::Tab:        return "tab";
    }
    return "treble";
}

// =============================================================================
// Dynamic mapping
// =============================================================================

const char* ly_dynamic(DynamicLevel d) {
    switch (d) {
        case DynamicLevel::pppp: return "\\pppp";
        case DynamicLevel::ppp:  return "\\ppp";
        case DynamicLevel::pp:   return "\\pp";
        case DynamicLevel::p:    return "\\p";
        case DynamicLevel::mp:   return "\\mp";
        case DynamicLevel::mf:   return "\\mf";
        case DynamicLevel::f:    return "\\f";
        case DynamicLevel::ff:   return "\\ff";
        case DynamicLevel::fff:  return "\\fff";
        case DynamicLevel::ffff: return "\\ffff";
        case DynamicLevel::sfz:  return "\\sfz";
        case DynamicLevel::fp:   return "\\fp";
        case DynamicLevel::sfp:  return "\\sfp";
        case DynamicLevel::rfz:  return "\\rfz";
    }
    return "\\mf";
}

// =============================================================================
// Articulation mapping
// =============================================================================

const char* ly_articulation(ArticulationType a) {
    switch (a) {
        case ArticulationType::Staccato:       return "-.";
        case ArticulationType::Staccatissimo:  return "-!";
        case ArticulationType::Accent:         return "->";
        case ArticulationType::Marcato:        return "-^";
        case ArticulationType::Tenuto:         return "--";
        case ArticulationType::Portato:        return "-_";
        case ArticulationType::Fermata:        return "\\fermata";
        case ArticulationType::Trill:          return "\\trill";
        case ArticulationType::Mordent:        return "\\mordent";
        case ArticulationType::Turn:           return "\\turn";
        case ArticulationType::Tremolo:        return "\\tremolo";
        case ArticulationType::SnapPizzicato:  return "\\snappizzicato";
        case ArticulationType::DownBow:        return "\\downbow";
        case ArticulationType::UpBow:          return "\\upbow";
        default:                               return nullptr;
    }
}

// =============================================================================
// Duration fallback — round to nearest representable power-of-two
// =============================================================================

std::string nearest_representable_duration(Beat dur) {
    double whole_notes = dur.to_float();
    if (whole_notes <= 0.0) return "4";

    // Convert to reciprocal: a quarter note is 1/4 whole note, LilyPond "4"
    double recip = 1.0 / whole_notes;
    // Round to nearest power of two
    double log2_val = std::log2(recip);
    int exponent = static_cast<int>(std::round(log2_val));
    if (exponent < 0) exponent = 0;  // cap at whole note (1)
    if (exponent > 6) exponent = 6;  // cap at 64th note
    int denom = 1 << exponent;
    return std::to_string(denom);
}

// =============================================================================
// Tuplet duration helper
// =============================================================================

/// Compute the normal-space duration for a note inside a tuplet.
/// A tuplet m:n over normal_type means m notes fit in the space of n.
/// Each note's notated duration in normal space is: normal_type * n / m.
/// LilyPond \tuplet m/n { ... } expects the notes written at normal_type.
std::string tuplet_note_duration(const TupletContext& tc) {
    Beat normal_dur = tc.normal_type;
    auto result = ly_duration(normal_dur);
    if (result) return *result;
    return nearest_representable_duration(normal_dur);
}

// =============================================================================
// Resolve key/time at a given bar from the global maps
// =============================================================================

const KeySignature& key_at_bar(const KeySignatureMap& key_map, std::uint32_t bar) {
    // key_map is ordered by position; find the last entry at or before bar
    // Precondition: key_map is non-empty (enforced by is_compilable)
    const KeySignature* current = &key_map.front().key;
    for (const auto& ke : key_map) {
        if (ke.position.bar > bar) break;
        if (ke.position.bar == bar && ke.position.beat > Beat::zero()) break;
        current = &ke.key;
    }
    return *current;
}

const TimeSignature& time_at_bar(const TimeSignatureMap& time_map, std::uint32_t bar) {
    // Precondition: time_map is non-empty (enforced by is_compilable)
    const TimeSignature* current = &time_map.front().time_signature;
    for (const auto& tse : time_map) {
        if (tse.bar > bar) break;
        current = &tse.time_signature;
    }
    return *current;
}

/// Find the tempo in effect at a given bar (most recent entry at or before bar).
const TempoEvent* tempo_at_bar(const TempoMap& tempo_map, std::uint32_t bar) {
    const TempoEvent* current = nullptr;
    for (const auto& te : tempo_map) {
        if (te.position.bar > bar) break;
        current = &te;
    }
    return current;
}

// =============================================================================
// Hairpin tracking
// =============================================================================

struct HairpinState {
    bool active = false;
    HairpinType type{};
};

/// Check if a hairpin starts at this bar/beat; check if one ends.
void update_hairpins(
    std::ostringstream& out,
    const std::vector<Hairpin>& hairpins,
    ScoreTime position,
    HairpinState& state
) {
    // Check for hairpin ending at this position
    if (state.active) {
        for (const auto& h : hairpins) {
            if (h.end == position) {
                out << "\\! ";
                state.active = false;
                break;
            }
        }
    }

    // Check for hairpin starting at this position
    for (const auto& h : hairpins) {
        if (h.start == position) {
            if (h.type == HairpinType::Crescendo) {
                out << "\\< ";
            } else {
                out << "\\> ";
            }
            state.active = true;
            state.type = h.type;
            break;
        }
    }
}

// =============================================================================
// Event emission
// =============================================================================

/// Emit a single NoteGroup (note or chord) with all annotations.
void emit_note_group(
    std::ostringstream& out,
    const NoteGroup& ng,
    CompilationReport& report,
    ScoreTime position,
    std::optional<PartId> part_id
) {
    bool is_chord = ng.notes.size() > 1;

    // Resolve duration string
    std::string dur_str;
    bool in_tuplet = ng.tuplet_context.has_value();

    if (in_tuplet) {
        // Inside a tuplet, notes are written at the normal_type duration
        dur_str = tuplet_note_duration(*ng.tuplet_context);
    } else {
        auto dur_result = ly_duration(ng.duration);
        if (dur_result) {
            dur_str = *dur_result;
        } else {
            dur_str = nearest_representable_duration(ng.duration);
            report.diagnostics.push_back({
                "Non-representable duration approximated for LilyPond output",
                position,
                part_id
            });
        }
    }

    if (is_chord) {
        // Build pitch vector for ly_chord
        out << "<";
        for (std::size_t i = 0; i < ng.notes.size(); ++i) {
            if (i > 0) out << " ";
            out << ly_pitch(ng.notes[i].pitch);
        }
        out << ">" << dur_str;
    } else {
        // Single note
        const Note& note = ng.notes[0];
        if (note.grace) {
            // Grace notes: acciaccatura uses slashed stem, appoggiatura does not
            if (*note.grace == GraceType::Acciaccatura) {
                out << "\\acciaccatura ";
            } else {
                out << "\\appoggiatura ";
            }
        }
        out << ly_pitch(note.pitch) << dur_str;
    }

    // Annotations are taken from the first note for chords, the only note otherwise
    const Note& primary = ng.notes[0];

    // Tie
    if (primary.tie_forward) {
        out << "~";
    }

    // Articulation
    if (primary.articulation) {
        const char* art = ly_articulation(*primary.articulation);
        if (art) out << art;
    }

    // Dynamic
    if (primary.dynamic) {
        out << ly_dynamic(*primary.dynamic);
    }

    // Slur start/end
    if (ng.slur_start) {
        out << "(";
    }
    if (ng.slur_end) {
        out << ")";
    }
}

/// Emit a rest event. Uses "R" for full-bar rests to enable \compressMMRests.
void emit_rest(
    std::ostringstream& out,
    const RestEvent& rest,
    const Beat& measure_duration,
    CompilationReport& report,
    ScoreTime position,
    std::optional<PartId> part_id
) {
    bool is_full_bar = (rest.duration == measure_duration);

    if (is_full_bar) {
        // Full-bar rest: use R (capitalised) for multi-measure rest compression
        auto dur_result = ly_duration(rest.duration);
        if (dur_result) {
            out << "R" << *dur_result;
        } else {
            // Fallback: express measure duration as fraction
            out << "R" << nearest_representable_duration(rest.duration);
            report.diagnostics.push_back({
                "Non-representable full-bar rest duration approximated",
                position,
                part_id
            });
        }
    } else {
        auto result = ly_rest(rest.duration);
        if (result) {
            out << *result;
        } else {
            out << "r" << nearest_representable_duration(rest.duration);
            report.diagnostics.push_back({
                "Non-representable rest duration approximated",
                position,
                part_id
            });
        }
    }
}

/// Emit a ScoreDirection as a LilyPond directive.
void emit_direction(std::ostringstream& out, const ScoreDirection& dir) {
    switch (dir.type) {
        case DirectionType::Text:
            if (dir.text) out << "^\"" << ly_escape(*dir.text) << "\" ";
            break;
        case DirectionType::TempoText:
            if (dir.text) out << "^\\markup { \\bold \"" << ly_escape(*dir.text) << "\" } ";
            break;
        case DirectionType::ClefChange:
            if (dir.new_clef) out << "\\clef " << ly_clef_name(*dir.new_clef) << " ";
            break;
        case DirectionType::Coda:
            out << "\\coda ";
            break;
        case DirectionType::Segno:
            out << "\\segno ";
            break;
        case DirectionType::DoubleBarline:
            out << "\\bar \"||\" ";
            break;
        case DirectionType::FinalBarline:
            out << "\\bar \"|.\" ";
            break;
        case DirectionType::RepeatStart:
            out << "\\bar \".|:\" ";
            break;
        case DirectionType::RepeatEnd:
            out << "\\bar \":|.\" ";
            break;
        case DirectionType::OttavaStart:
            if (dir.ottava_shift > 0) {
                out << "\\ottava #" << (dir.ottava_shift / 12) << " ";
            } else {
                out << "\\ottava #-" << (-dir.ottava_shift / 12) << " ";
            }
            break;
        case DirectionType::OttavaEnd:
            out << "\\ottava #0 ";
            break;
        case DirectionType::PedalDown:
            out << "\\sustainOn ";
            break;
        case DirectionType::PedalUp:
            out << "\\sustainOff ";
            break;
        case DirectionType::BreathMark:
            out << "\\breathe ";
            break;
        case DirectionType::Caesura:
            out << "\\caesura ";
            break;
    }
}

// =============================================================================
// Voice emission
// =============================================================================

/// Emit all events in a single voice for one measure.
void emit_voice_events(
    std::ostringstream& out,
    const Voice& voice,
    const Beat& measure_duration,
    CompilationReport& report,
    std::uint32_t bar_number,
    const std::vector<Hairpin>& hairpins,
    HairpinState& hp_state,
    std::optional<PartId> part_id
) {
    // Track active tuplet to emit \tuplet brackets
    std::optional<TupletId> active_tuplet;

    for (std::size_t ei = 0; ei < voice.events.size(); ++ei) {
        const Event& event = voice.events[ei];
        ScoreTime position{bar_number, event.offset};

        // Hairpin updates at each event position
        update_hairpins(out, hairpins, position, hp_state);

        if (event.is_note_group()) {
            const NoteGroup& ng = *event.as_note_group();

            // Tuplet bracket management
            if (ng.tuplet_context) {
                if (!active_tuplet || *active_tuplet != ng.tuplet_context->id) {
                    // Close previous tuplet if any
                    if (active_tuplet) {
                        out << "} ";
                    }
                    // Open new tuplet
                    const auto& tc = *ng.tuplet_context;
                    out << "\\tuplet " << static_cast<int>(tc.actual) << "/"
                        << static_cast<int>(tc.normal) << " { ";
                    active_tuplet = tc.id;
                }
            } else if (active_tuplet) {
                out << "} ";
                active_tuplet = std::nullopt;
            }

            emit_note_group(out, ng, report, position, part_id);
            out << " ";

        } else if (event.is_rest()) {
            const RestEvent& rest = *event.as_rest();

            // Close any active tuplet before a rest
            if (active_tuplet) {
                out << "} ";
                active_tuplet = std::nullopt;
            }

            emit_rest(out, rest, measure_duration, report, position, part_id);
            out << " ";

        } else if (event.is_direction()) {
            const auto* dir = std::get_if<ScoreDirection>(&event.payload);
            if (dir) emit_direction(out, *dir);
        }
    }

    // Close any still-open tuplet at end of measure
    if (active_tuplet) {
        out << "} ";
    }
}

}  // anonymous namespace

// =============================================================================
// compile_score_to_lilypond
// =============================================================================

Result<LilyPondCompilationResult> compile_score_to_lilypond(const Score& score) {
    if (!is_compilable(score)) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }

    CompilationReport report;
    std::ostringstream out;

    // -------------------------------------------------------------------------
    // Header block
    // -------------------------------------------------------------------------

    out << "\\version \"2.24.0\"\n\n";

    out << "\\header {\n";
    out << "  title = \"" << ly_escape(score.metadata.title) << "\"\n";
    if (score.metadata.composer) {
        out << "  composer = \"" << ly_escape(*score.metadata.composer) << "\"\n";
    }
    if (score.metadata.subtitle) {
        out << "  subtitle = \"" << ly_escape(*score.metadata.subtitle) << "\"\n";
    }
    if (score.metadata.arranger) {
        out << "  arranger = \"" << ly_escape(*score.metadata.arranger) << "\"\n";
    }
    if (score.metadata.opus) {
        out << "  opus = \"" << ly_escape(*score.metadata.opus) << "\"\n";
    }
    out << "}\n\n";

    // -------------------------------------------------------------------------
    // Preamble — multi-measure rest compression
    // -------------------------------------------------------------------------

    out << "\\compressMMRests ##t\n\n";

    // -------------------------------------------------------------------------
    // Part definitions — each Part becomes a Staff
    // -------------------------------------------------------------------------

    // Collect staff variable names for the score block
    std::vector<std::string> staff_var_names;

    for (std::size_t pi = 0; pi < score.parts.size(); ++pi) {
        const Part& part = score.parts[pi];
        const PartDefinition& def = part.definition;

        std::string var_name = "part" + std::string(1, static_cast<char>('A' + pi));
        staff_var_names.push_back(var_name);

        out << var_name << " = ";

        // If transposing instrument, wrap in \transpose
        bool transposing = def.transposition != 0;
        if (transposing) {
            // \transpose c' <written_pitch> means "written C sounds as <written_pitch>"
            // transposition is the interval from written to sounding: sounding = written + transposition
            // LilyPond \transpose c' bes means "what is written as C sounds as Bb"
            // We need: \transpose <sounding> c' { <concert pitch music> }
            // which reads concert pitch and writes it transposed.
            // Actually for display purposes: \transpose c' <sounding_equivalent_of_c>
            // The sounding pitch of written middle C is C + transposition semitones.
            // Build a SpelledPitch for the sounding equivalent.
            // For simplicity, use the transposition to determine the written key concert pitch.
            // LilyPond: \transpose c' <concert_c_sounds_as> { music_in_concert }
            // If transposition = -2 (Bb instrument), concert C sounds as C, written C sounds as Bb.
            // So we want: \transpose bes c' { concert_music }
            // which means: read notes as concert, write them for Bb instrument.
            SpelledPitch concert_c{0, 0, 4};  // C4
            int sounding_midi = midi_value(concert_c) + def.transposition;
            int8_t sounding_octave = static_cast<int8_t>(sounding_midi / 12 - 1);
            auto sounding_pc = static_cast<PitchClass>(((sounding_midi % 12) + 12) % 12);
            SpelledPitch sounding_pitch = default_spelling(sounding_pc, 0, sounding_octave);

            out << "\\transpose " << ly_pitch(sounding_pitch) << " "
                << ly_pitch(concert_c) << " ";
        }

        out << "\\new Staff \\with {\n";
        out << "  instrumentName = \"" << def.name << "\"\n";
        if (!def.abbreviation.empty()) {
            out << "  shortInstrumentName = \"" << def.abbreviation << "\"\n";
        }
        out << "} {\n";

        // Initial clef
        out << "  \\clef " << ly_clef_name(def.clef) << "\n";

        // Initial key from key_map (bar 1)
        if (!score.key_map.empty()) {
            const KeySignature& initial_key = score.key_map.front().key;
            bool major = !is_minor_key(initial_key);
            out << "  " << ly_key(initial_key.root, major) << "\n";
        }

        // Initial time signature from time_map (bar 1)
        if (!score.time_map.empty()) {
            const TimeSignature& initial_ts = score.time_map.front().time_signature;
            out << "  " << ly_time_signature(initial_ts.numerator(), initial_ts.denominator) << "\n";
        }

        // Initial tempo
        if (!score.tempo_map.empty()) {
            const TempoEvent& initial_tempo = score.tempo_map.front();
            int bpm = static_cast<int>(std::round(initial_tempo.bpm.to_float()));
            out << "  \\tempo 4 = " << bpm << "\n";
        }

        out << "  ";

        // Track previous key/time/tempo for change detection
        const KeySignature* prev_key = score.key_map.empty()
            ? nullptr : &score.key_map.front().key;
        const TimeSignature* prev_ts = score.time_map.empty()
            ? nullptr : &score.time_map.front().time_signature;
        const TempoEvent* prev_tempo = score.tempo_map.empty()
            ? nullptr : &score.tempo_map.front();

        HairpinState hp_state;

        // Iterate measures
        for (const auto& measure : part.measures) {
            std::uint32_t bar = measure.bar_number;

            // Check for key change at this bar
            const KeySignature& current_key = key_at_bar(score.key_map, bar);
            if (prev_key && !(current_key == *prev_key)) {
                bool major = !is_minor_key(current_key);
                out << ly_key(current_key.root, major) << " ";
            }
            prev_key = &current_key;

            // Check for time signature change at this bar
            const TimeSignature& current_ts = time_at_bar(score.time_map, bar);
            if (prev_ts && current_ts.numerator() != prev_ts->numerator()) {
                out << ly_time_signature(current_ts.numerator(), current_ts.denominator) << " ";
            } else if (prev_ts && current_ts.denominator != prev_ts->denominator) {
                out << ly_time_signature(current_ts.numerator(), current_ts.denominator) << " ";
            }
            prev_ts = &current_ts;

            // Check for tempo change at this bar (only emit for first part)
            if (pi == 0) {
                const TempoEvent* current_tempo = tempo_at_bar(score.tempo_map, bar);
                if (current_tempo && prev_tempo && current_tempo != prev_tempo) {
                    int bpm = static_cast<int>(std::round(current_tempo->bpm.to_float()));
                    out << "\\tempo 4 = " << bpm << " ";
                }
                prev_tempo = current_tempo;
            }

            Beat measure_dur = current_ts.measure_duration();

            if (measure.voices.size() == 1) {
                // Single voice: emit events inline
                emit_voice_events(
                    out, measure.voices[0], measure_dur, report,
                    bar, part.hairpins, hp_state, part.id);
            } else {
                // Multiple voices: polyphonic syntax
                out << "<< ";
                for (std::size_t vi = 0; vi < measure.voices.size(); ++vi) {
                    if (vi > 0) out << "\\\\ ";
                    out << "{ ";
                    emit_voice_events(
                        out, measure.voices[vi], measure_dur, report,
                        bar, part.hairpins, hp_state, part.id);
                    out << "} ";
                }
                out << ">> ";
            }

            // Bar check (LilyPond uses | as a bar check)
            out << "| ";
        }

        // Close any active hairpin at end of part
        if (hp_state.active) {
            out << "\\! ";
        }

        out << "\n";

        if (transposing) {
            out << "}\n";
        }
        out << "}\n\n";
    }

    // -------------------------------------------------------------------------
    // Score block
    // -------------------------------------------------------------------------

    out << "\\score {\n";
    out << "  \\new StaffGroup <<\n";
    for (const auto& var : staff_var_names) {
        out << "    \\" << var << "\n";
    }
    out << "  >>\n";
    out << "  \\layout { }\n";
    out << "}\n";

    return LilyPondCompilationResult{out.str(), std::move(report)};
}

}  // namespace Sunny::Infrastructure::Format
