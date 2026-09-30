/**
 * @file score_to_lilypond.cpp
 * @brief LilyPond compilation from Score IR — implementation
 *
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <set>
#include <sstream>
#include <string_view>
#include <sunny/core/score/document.hpp>
#include <sunny/core/score/tuplets.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/written_duration.hpp>
#include <sunny/infrastructure/formats/lilypond.hpp>
#include <sunny/infrastructure/formats/score_to_lilypond.hpp>

namespace sunny::infrastructure::formats {

namespace {

using namespace sunny::core;

std::string compilable_ly_pitch(SpelledPitch pitch) {
    const auto result = ly_pitch(pitch);
    assert(result.has_value() && "compilable Score produced an invalid LilyPond pitch");
    return result.value_or(std::string{});
}

std::string compilable_ly_time_signature(int numerator, int denominator) {
    const auto result = ly_time_signature(numerator, denominator);
    assert(result.has_value() && "compilable Score produced an invalid LilyPond time signature");
    return result.value_or(std::string{});
}

// =============================================================================
// LilyPond string escaping — prevent injection via metadata fields
// =============================================================================

std::string ly_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else
            out += c;
    }
    return out;
}

// =============================================================================
// Clef mapping
// =============================================================================

const char* ly_clef_name(Clef c) {
    switch (c) {
    case Clef::Treble:
        return "treble";
    case Clef::Bass:
        return "bass";
    case Clef::Alto:
        return "alto";
    case Clef::Tenor:
        return "tenor";
    case Clef::Percussion:
        return "percussion";
    case Clef::Tab:
        return "tab";
    }
    return "treble";
}

// =============================================================================
// Dynamic mapping
// =============================================================================

const char* ly_dynamic(DynamicLevel d) {
    switch (d) {
    case DynamicLevel::pppp:
        return "\\pppp";
    case DynamicLevel::ppp:
        return "\\ppp";
    case DynamicLevel::pp:
        return "\\pp";
    case DynamicLevel::p:
        return "\\p";
    case DynamicLevel::mp:
        return "\\mp";
    case DynamicLevel::mf:
        return "\\mf";
    case DynamicLevel::f:
        return "\\f";
    case DynamicLevel::ff:
        return "\\ff";
    case DynamicLevel::fff:
        return "\\fff";
    case DynamicLevel::ffff:
        return "\\ffff";
    case DynamicLevel::sfz:
        return "\\sfz";
    case DynamicLevel::fp:
        return "\\fp";
    case DynamicLevel::sfp:
        return "\\sfp";
    case DynamicLevel::rfz:
        return "\\rfz";
    }
    return "\\mf";
}

std::optional<DynamicLevel> semantic_dynamic(const Note& note) {
    if (note.dynamic) return note.dynamic;
    return note.velocity.written;
}

// =============================================================================
// Articulation mapping
// =============================================================================

enum class ArticulationDisposition : std::uint8_t {
    NoteSuffix,
    ChordDynamic,
    TextFallback,
    ImplicitEndpoint,
};

struct ArticulationMapping {
    ArticulationType type;
    ArticulationDisposition disposition;
    std::string_view lilypond;
    std::string_view fallback_text;
};

constexpr std::array ARTICULATION_MAP{
    ArticulationMapping{ArticulationType::Staccato, ArticulationDisposition::NoteSuffix, "-.", ""},
    ArticulationMapping{
        ArticulationType::Staccatissimo, ArticulationDisposition::NoteSuffix, "-!", ""},
    ArticulationMapping{ArticulationType::Tenuto, ArticulationDisposition::NoteSuffix, "--", ""},
    ArticulationMapping{ArticulationType::Portato, ArticulationDisposition::NoteSuffix, "-_", ""},
    ArticulationMapping{ArticulationType::Accent, ArticulationDisposition::NoteSuffix, "->", ""},
    ArticulationMapping{ArticulationType::Marcato, ArticulationDisposition::NoteSuffix, "-^", ""},
    ArticulationMapping{
        ArticulationType::Sforzando, ArticulationDisposition::ChordDynamic, "\\sfz", ""},
    ArticulationMapping{
        ArticulationType::ForzandoPiano, ArticulationDisposition::ChordDynamic, "\\fp", ""},
    ArticulationMapping{
        ArticulationType::Fermata, ArticulationDisposition::NoteSuffix, "\\fermata", ""},
    ArticulationMapping{
        ArticulationType::Trill, ArticulationDisposition::NoteSuffix, "\\trill", ""},
    ArticulationMapping{
        ArticulationType::Mordent, ArticulationDisposition::NoteSuffix, "\\mordent", ""},
    ArticulationMapping{
        ArticulationType::InvertedMordent, ArticulationDisposition::NoteSuffix, "\\upmordent", ""},
    ArticulationMapping{ArticulationType::Turn, ArticulationDisposition::NoteSuffix, "\\turn", ""},
    ArticulationMapping{
        ArticulationType::InvertedTurn, ArticulationDisposition::NoteSuffix, "\\reverseturn", ""},
    ArticulationMapping{
        ArticulationType::Tremolo, ArticulationDisposition::TextFallback, "", "trem."},
    ArticulationMapping{
        ArticulationType::Harmonic, ArticulationDisposition::NoteSuffix, "\\flageolet", ""},
    ArticulationMapping{
        ArticulationType::GlissandoStart, ArticulationDisposition::NoteSuffix, "\\glissando", ""},
    ArticulationMapping{
        ArticulationType::GlissandoEnd, ArticulationDisposition::ImplicitEndpoint, "", ""},
    ArticulationMapping{ArticulationType::SnapPizzicato,
                        ArticulationDisposition::NoteSuffix,
                        "\\snappizzicato",
                        ""},
    ArticulationMapping{
        ArticulationType::DownBow, ArticulationDisposition::NoteSuffix, "\\downbow", ""},
    ArticulationMapping{
        ArticulationType::UpBow, ArticulationDisposition::NoteSuffix, "\\upbow", ""},
    ArticulationMapping{
        ArticulationType::OpenString, ArticulationDisposition::NoteSuffix, "\\open", ""},
    ArticulationMapping{
        ArticulationType::Stopped, ArticulationDisposition::NoteSuffix, "\\stopped", ""},
    ArticulationMapping{
        ArticulationType::Muted, ArticulationDisposition::TextFallback, "", "muted"},
    ArticulationMapping{
        ArticulationType::BendUp, ArticulationDisposition::TextFallback, "", "bend up"},
    ArticulationMapping{
        ArticulationType::BendDown, ArticulationDisposition::TextFallback, "", "bend down"},
};

constexpr bool articulation_map_is_total_and_unique() {
    constexpr auto count = static_cast<std::size_t>(ArticulationType::BendDown) + 1;
    if (ARTICULATION_MAP.size() != count) return false;
    for (std::size_t value = 0; value < count; ++value) {
        const auto type = static_cast<ArticulationType>(value);
        if (std::count_if(ARTICULATION_MAP.begin(),
                          ARTICULATION_MAP.end(),
                          [type](const auto& mapping) { return mapping.type == type; }) != 1)
            return false;
    }
    return true;
}

static_assert(articulation_map_is_total_and_unique(),
              "Every ArticulationType needs exactly one LilyPond disposition");

const ArticulationMapping& ly_articulation(ArticulationType type) {
    const auto found = std::find_if(ARTICULATION_MAP.begin(),
                                    ARTICULATION_MAP.end(),
                                    [type](const auto& mapping) { return mapping.type == type; });
    return *found;
}

/// Preserve an arbitrary positive Score-IR duration exactly. LilyPond duration
/// multipliers affect musical time even when no conventional note-value glyph
/// exists for the rational duration.
std::string exact_duration(Beat duration) {
    if (auto conventional = ly_duration(duration)) return *conventional;

    std::string result = "1*" + std::to_string(duration.numerator());
    if (duration.denominator() != 1) result += "/" + std::to_string(duration.denominator());
    return result;
}

/// LilyPond's shortest documented duration is the 128th note.
constexpr int LILYPOND_MIN_EXPONENT = -7;

/// LilyPond token of one written glyph: \breve, 1, 2, 4 ... with dots.
std::string ly_written_value(WrittenNoteValue value) {
    std::string token =
        value.exponent > 0 ? std::string{"\\breve"} : std::to_string(1LL << -value.exponent);
    token.append(static_cast<std::size_t>(value.dots), '.');
    return token;
}

/// Open a \tuplet for a ratio implied by the duration itself (an event with
/// no TupletContext whose written value is non-dyadic, such as 1/12).
std::string inferred_tuplet_open(const WrittenDuration& written) {
    if (written.inferred_actual == written.inferred_normal) return {};
    return "\\tuplet " + std::to_string(written.inferred_actual) + "/" +
           std::to_string(written.inferred_normal) + " { ";
}

std::string inferred_tuplet_close(const WrittenDuration& written) {
    return written.inferred_actual == written.inferred_normal ? std::string{} : std::string{" }"};
}

void emit_spacer(std::ostringstream& out, Beat duration) {
    if (duration <= Beat::zero()) return;
    out << "s" << exact_duration(duration) << " ";
}

// =============================================================================
// Tuplet duration helper
// =============================================================================

/// Compute the normal-space duration for a note inside a tuplet.
/// A tuplet m:n over normal_type means m notes fit in the space of n.
/// Each note's notated duration in normal space is: normal_type * n / m.
/// LilyPond \tuplet m/n {... } expects the notes written at normal_type.
std::string tuplet_note_duration(const TupletContext& tc) {
    return exact_duration(tc.normal_type);
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

std::string score_time_signature_to_lilypond(const TimeSignature& time) {
    const auto& groups = time.groups();
    const bool additive =
        groups.size() > 1 && !std::all_of(groups.begin() + 1, groups.end(), [&](int group) {
            return group == groups.front();
        });
    if (!additive) return compilable_ly_time_signature(time.numerator(), time.denominator());

    std::ostringstream out;
    out << "\\compoundMeter #'(";
    for (const auto group : groups)
        out << group << " ";
    out << time.denominator() << ")";
    return out.str();
}

// =============================================================================
// Event emission
// =============================================================================

void add_loss_diagnostic(CompilationReport& report,
                         std::string message,
                         std::optional<ScoreTime> position = std::nullopt,
                         std::optional<PartId> part_id = std::nullopt) {
    report.diagnostics.push_back({std::move(message), position, part_id});
}

std::optional<std::string_view> native_lilypond_mode(std::string_view mode) {
    if (mode == "major") return "major";
    if (mode == "minor") return "minor";
    if (mode == "ionian") return "ionian";
    if (mode == "dorian") return "dorian";
    if (mode == "phrygian") return "phrygian";
    if (mode == "lydian") return "lydian";
    if (mode == "mixolydian") return "mixolydian";
    if (mode == "aeolian") return "aeolian";
    if (mode == "locrian") return "locrian";
    return std::nullopt;
}

bool same_key_signature_state(const KeySignature& lhs, const KeySignature& rhs) {
    return lhs == rhs;
}

std::string lily_pitch_alteration(int semitones) {
    if (semitones % 2 == 0) return std::to_string(semitones / 2);
    return std::to_string(semitones) + "/2";
}

void emit_stored_fifths(std::ostringstream& out, int fifths) {
    constexpr std::array sharp_order{3, 0, 4, 1, 5, 2, 6}; // F C G D A E B
    constexpr std::array flat_order{6, 2, 5, 1, 4, 0, 3};  // B E A D G C F
    const auto& order = fifths < 0 ? flat_order : sharp_order;
    std::array<int, 7> alterations{};
    const auto count = std::abs(fifths);
    for (int i = 0; i < count; ++i)
        alterations[order[static_cast<std::size_t>(i) % order.size()]] += fifths < 0 ? -1 : 1;

    // LilyPond pitch alterations are exact fractions of a whole tone.  Sunny's
    // stored fifths expand through the traditional accidental order, including
    // repeated passes for double and higher alterations.
    out << "\\set Staff.keyAlterations = #`(";
    bool first = true;
    for (const auto step : order) {
        if (alterations[step] == 0) continue;
        if (!first) out << " ";
        out << "(" << step << " . " << lily_pitch_alteration(alterations[step]) << ")";
        first = false;
    }
    out << ") ";
}

void emit_key_signature(std::ostringstream& out,
                        const KeySignature& key,
                        CompilationReport& report,
                        ScoreTime position,
                        std::optional<PartId> part_id) {
    if (const auto mode = native_lilypond_mode(key.mode.name)) {
        SpelledPitch tonic{key.root.letter, key.root.accidental, 3};
        out << "\\key " << compilable_ly_pitch(tonic) << " \\" << *mode << " ";
        return;
    }

    emit_stored_fifths(out, key.accidentals);
    const auto scale_label = key.mode.name.empty() ? std::string{"an anonymous Sunny scale"}
                                                   : "Sunny scale '" + key.mode.name + "'";
    add_loss_diagnostic(
        report,
        "LilyPond has no predefined mode command for " + scale_label +
            "; emitted the exact stored traditional signature via Staff.keyAlterations, while "
            "the analytical tonic and scale remain target-external",
        position,
        part_id);
}

void emit_text_post_event(std::ostringstream& out,
                          std::string_view text,
                          bool below = false,
                          bool italic = false) {
    out << (below ? "_" : "-") << "\\markup { ";
    if (italic) out << "\\italic ";
    out << "\"" << ly_escape(std::string{text}) << "\" }";
}

bool ornament_supersedes_articulation(const Ornament& ornament, ArticulationType articulation) {
    switch (ornament.type) {
    case OrnamentType::Trill:
        return articulation == ArticulationType::Trill;
    case OrnamentType::Mordent:
        return articulation == ArticulationType::Mordent;
    case OrnamentType::InvertedMordent:
        return articulation == ArticulationType::InvertedMordent;
    case OrnamentType::Turn:
        return articulation == ArticulationType::Turn;
    case OrnamentType::InvertedTurn:
        return articulation == ArticulationType::InvertedTurn;
    case OrnamentType::Shake:
    case OrnamentType::Arpeggio:
        return false;
    }
    return false;
}

void emit_notehead_prefix(std::ostringstream& out,
                          const Note& note,
                          CompilationReport& report,
                          ScoreTime position,
                          std::optional<PartId> part_id) {
    if (!note.notation_head) return;
    switch (*note.notation_head) {
    case NoteHeadType::Normal:
        break;
    case NoteHeadType::Diamond:
        out << "\\tweak style #'diamond ";
        break;
    case NoteHeadType::Cross:
        out << "\\tweak style #'cross ";
        break;
    case NoteHeadType::Slash:
        out << "\\tweak style #'slash ";
        break;
    case NoteHeadType::Triangle:
        out << "\\tweak style #'triangle ";
        break;
    case NoteHeadType::CircleX:
        out << "\\tweak style #'xcircle ";
        break;
    case NoteHeadType::Cue:
        out << "\\tweak font-size #-2 ";
        break;
    case NoteHeadType::Square:
        add_loss_diagnostic(
            report,
            "LilyPond 2.24 has no standard square NoteHead.style; retained an ordinary "
            "sounding notehead and added a square-notehead text marker",
            position,
            part_id);
        break;
    }
}

const char* ornament_suffix(OrnamentType type) {
    switch (type) {
    case OrnamentType::Trill:
        return "\\trill";
    case OrnamentType::Mordent:
        return "\\mordent";
    case OrnamentType::InvertedMordent:
        return "\\upmordent";
    case OrnamentType::Turn:
        return "\\turn";
    case OrnamentType::InvertedTurn:
        return "\\reverseturn";
    case OrnamentType::Shake:
        return "\\prallprall";
    case OrnamentType::Arpeggio:
        return nullptr;
    }
    return nullptr;
}

std::string slide_name(SlideDirection direction) {
    switch (direction) {
    case SlideDirection::Into:
        return "slide into";
    case SlideDirection::OutOf:
        return "slide out of";
    case SlideDirection::Ascending:
        return "slide ascending";
    case SlideDirection::Descending:
        return "slide descending";
    }
    return "slide";
}

std::string vibrato_name(VibratoSpeed speed) {
    switch (speed) {
    case VibratoSpeed::Slow:
        return "slow vibrato";
    case VibratoSpeed::Normal:
        return "vibrato";
    case VibratoSpeed::Fast:
        return "fast vibrato";
    case VibratoSpeed::None:
        return "non vibrato";
    }
    return "vibrato";
}

void emit_lossy_technical_text(std::ostringstream& out,
                               CompilationReport& report,
                               const std::string& text,
                               ScoreTime position,
                               std::optional<PartId> part_id) {
    emit_text_post_event(out, text);
    add_loss_diagnostic(report,
                        "Sunny TechnicalDirection has no lossless standard LilyPond 2.24 "
                        "post-event; preserved it as note text: " +
                            text,
                        position,
                        part_id);
}

/// Emit post-events that LilyPond permits on an individual note inside a chord.
void emit_note_post_events(std::ostringstream& out,
                           const Note& note,
                           CompilationReport& report,
                           ScoreTime position,
                           std::optional<PartId> part_id) {
    if (note.tie_forward) out << "~";

    if (note.articulation &&
        !(note.ornament && ornament_supersedes_articulation(*note.ornament, *note.articulation))) {
        const auto& mapping = ly_articulation(*note.articulation);
        switch (mapping.disposition) {
        case ArticulationDisposition::NoteSuffix:
            out << mapping.lilypond;
            break;
        case ArticulationDisposition::TextFallback:
            emit_text_post_event(out, mapping.fallback_text);
            add_loss_diagnostic(
                report,
                "Articulation has no lossless standard LilyPond 2.24 encoding; preserved it "
                "as note text: " +
                    std::string{mapping.fallback_text},
                position,
                part_id);
            break;
        case ArticulationDisposition::ChordDynamic:
        case ArticulationDisposition::ImplicitEndpoint:
            break;
        }
    }

    if (note.ornament && note.ornament->type != OrnamentType::Arpeggio) {
        if (const char* suffix = ornament_suffix(note.ornament->type)) out << suffix;
        if (note.ornament->type == OrnamentType::Trill) {
            add_loss_diagnostic(
                report,
                "LilyPond trill script preserves the ornament family but not Sunny's explicit "
                "trill interval/accidental payload",
                position,
                part_id);
        }
    }

    for (const auto& technical : note.technical) {
        switch (technical.type) {
        case TechnicalDirection::Type::Fingering:
            if (technical.fingering.empty()) {
                emit_lossy_technical_text(out, report, "fingering unspecified", position, part_id);
            } else {
                for (auto finger : technical.fingering)
                    out << "-" << static_cast<unsigned>(finger);
            }
            break;
        case TechnicalDirection::Type::StringNumber:
            if (technical.number >= 1 && technical.number <= 9) {
                out << "\\" << static_cast<unsigned>(technical.number);
            } else {
                emit_lossy_technical_text(
                    out, report, "string " + std::to_string(technical.number), position, part_id);
            }
            break;
        case TechnicalDirection::Type::Position:
            emit_lossy_technical_text(
                out, report, "position " + std::to_string(technical.number), position, part_id);
            break;
        case TechnicalDirection::Type::BowingPattern:
            emit_lossy_technical_text(out,
                                      report,
                                      technical.pattern.empty() ? "bowing pattern unspecified"
                                                                : technical.pattern,
                                      position,
                                      part_id);
            break;
        case TechnicalDirection::Type::BreathMark:
        case TechnicalDirection::Type::Caesura:
            break;
        case TechnicalDirection::Type::Slide:
            emit_lossy_technical_text(
                out, report, slide_name(technical.slide_direction), position, part_id);
            break;
        case TechnicalDirection::Type::HammerOn:
            emit_lossy_technical_text(out, report, "hammer-on", position, part_id);
            break;
        case TechnicalDirection::Type::PullOff:
            emit_lossy_technical_text(out, report, "pull-off", position, part_id);
            break;
        case TechnicalDirection::Type::Bend:
            out << "\\bendAfter #(/ " << technical.bend_cents << " 100)";
            break;
        case TechnicalDirection::Type::Vibrato:
            emit_lossy_technical_text(
                out, report, vibrato_name(technical.vibrato_speed), position, part_id);
            break;
        }
    }

    if (note.notation_head == NoteHeadType::Square) emit_text_post_event(out, "square notehead");
}

struct BeamBoundary {
    bool start = false;
    bool end = false;
};

BeamBoundary beam_boundary_for(const Voice& voice, EventId event_id) {
    BeamBoundary result;
    for (const auto& group : voice.beam_groups) {
        const auto found = std::find(group.event_ids.begin(), group.event_ids.end(), event_id);
        if (found == group.event_ids.end()) continue;
        if (group.event_ids.size() < 2) continue;
        result.start = found == group.event_ids.begin();
        result.end = std::next(found) == group.event_ids.end();
    }
    return result;
}

void emit_beam_boundary(std::ostringstream& out, BeamBoundary boundary) {
    if (boundary.start) out << "[";
    if (boundary.end) out << "]";
}

/// Which part of a sounding event one written piece carries. The attack owns
/// marks that belong to the onset; the release owns those that close it.
struct PieceRole {
    bool attack = true;
    bool release = true;
};

/// Build the text of one written piece of a NoteGroup (note or chord).
std::string note_group_piece_text(const NoteGroup& ng,
                                  const std::string& dur_str,
                                  PieceRole role,
                                  CompilationReport& report,
                                  ScoreTime position,
                                  std::optional<PartId> part_id,
                                  BeamBoundary beam_boundary) {
    bool is_chord = ng.notes.size() > 1;
    std::ostringstream event_out;

    const Ornament* arpeggio = nullptr;
    std::size_t arpeggio_notes = 0;
    for (const auto& note : ng.notes) {
        if (!role.attack) break;
        if (note.ornament && note.ornament->type == OrnamentType::Arpeggio) {
            if (!arpeggio) arpeggio = &*note.ornament;
            ++arpeggio_notes;
        }
    }
    if (arpeggio && is_chord) {
        if (arpeggio->arpeggio_direction == ArpeggioDirection::Up)
            event_out << "\\arpeggioArrowUp ";
        else if (arpeggio->arpeggio_direction == ArpeggioDirection::Down)
            event_out << "\\arpeggioArrowDown ";
        if (arpeggio_notes != ng.notes.size()) {
            add_loss_diagnostic(
                report,
                "LilyPond arpeggio is chord-scoped; a per-note Sunny arpeggio was promoted to "
                "the containing chord",
                position,
                part_id);
        }
    }

    if (is_chord) {
        event_out << "<";
        for (std::size_t i = 0; i < ng.notes.size(); ++i) {
            if (i > 0) event_out << " ";
            emit_notehead_prefix(event_out, ng.notes[i], report, position, part_id);
            event_out << compilable_ly_pitch(ng.notes[i].pitch);
            emit_note_post_events(event_out, ng.notes[i], report, position, part_id);
        }
        event_out << ">" << dur_str;
    } else {
        const Note& note = ng.notes[0];
        emit_notehead_prefix(event_out, note, report, position, part_id);
        event_out << compilable_ly_pitch(note.pitch) << dur_str;
        emit_note_post_events(event_out, note, report, position, part_id);
    }

    if (arpeggio) {
        if (is_chord) {
            event_out << "\\arpeggio";
            if (arpeggio->arpeggio_direction != ArpeggioDirection::None)
                event_out << " \\arpeggioNormal";
        } else {
            emit_text_post_event(event_out, "arpeggio");
            add_loss_diagnostic(report,
                                "LilyPond arpeggio is chord-scoped; preserved a single-note "
                                "Arpeggio ornament as text",
                                position,
                                part_id);
        }
    }

    std::vector<std::string_view> chord_dynamics;
    for (const auto& note : ng.notes) {
        if (!role.attack) break;
        if (const auto dynamic = semantic_dynamic(note))
            chord_dynamics.emplace_back(ly_dynamic(*dynamic));
        if (note.articulation) {
            const auto& mapping = ly_articulation(*note.articulation);
            if (mapping.disposition == ArticulationDisposition::ChordDynamic)
                chord_dynamics.push_back(mapping.lilypond);
        }
    }
    std::sort(chord_dynamics.begin(), chord_dynamics.end());
    chord_dynamics.erase(std::unique(chord_dynamics.begin(), chord_dynamics.end()),
                         chord_dynamics.end());
    for (const auto dynamic : chord_dynamics)
        event_out << dynamic;
    if (is_chord && chord_dynamics.size() > 1) {
        add_loss_diagnostic(
            report,
            "LilyPond dynamics are chord-scoped; distinct per-note dynamic events were retained "
            "at one chord onset but cannot preserve per-pitch ownership",
            position,
            part_id);
    }

    if (role.attack && ng.slur_end) event_out << ")";
    if (role.attack && ng.slur_start) event_out << "(";
    emit_beam_boundary(
        event_out,
        BeamBoundary{beam_boundary.start && role.attack, beam_boundary.end && role.release});

    bool breath = false;
    bool caesura = false;
    for (const auto& note : ng.notes) {
        for (const auto& technical : note.technical) {
            breath = breath || technical.type == TechnicalDirection::Type::BreathMark;
            caesura = caesura || technical.type == TechnicalDirection::Type::Caesura;
        }
    }
    if (role.release && breath) event_out << " \\breathe";
    if (role.release && caesura) event_out << " \\caesura";
    return event_out.str();
}

/// Emit a single NoteGroup (note or chord) with all annotations.
///
/// Every written note carries a real value (Gould): the sounding duration
/// times the enclosing tuplet ratio is written as tied values, under an
/// inferred \tuplet when the duration itself implies one. A duration with no
/// written form keeps an exact multiplier and is reported as a residual.
void emit_note_group(std::ostringstream& out,
                     const NoteGroup& ng,
                     Beat context_ratio,
                     CompilationReport& report,
                     ScoreTime position,
                     std::optional<PartId> part_id,
                     BeamBoundary beam_boundary) {
    if (!ng.notes.empty() && ng.notes.front().grace) {
        const std::string dur_str = ng.tuplet_context ? tuplet_note_duration(*ng.tuplet_context)
                                                      : exact_duration(ng.duration);
        out << (*ng.notes.front().grace == GraceType::Acciaccatura ? "\\acciaccatura { "
                                                                   : "\\appoggiatura { ")
            << note_group_piece_text(ng, dur_str, {}, report, position, part_id, beam_boundary)
            << " } ";
        // Sunny's grace duration is an explicit structural allocation. LilyPond grace
        // expressions consume no main-voice time, so an invisible allocation follows.
        emit_spacer(out, ng.duration);
        return;
    }

    const auto written =
        project_written_duration(ng.duration, context_ratio, LILYPOND_MIN_EXPONENT);
    if (!written) {
        add_loss_diagnostic(report,
                            "LilyPond has no written note value for this duration; emitted an "
                            "exact duration multiplier",
                            position,
                            part_id);
        const std::string dur_str = ng.tuplet_context ? tuplet_note_duration(*ng.tuplet_context)
                                                      : exact_duration(ng.duration);
        out << note_group_piece_text(ng, dur_str, {}, report, position, part_id, beam_boundary);
        return;
    }

    out << inferred_tuplet_open(*written);
    for (std::size_t piece = 0; piece < written->pieces.size(); ++piece) {
        const PieceRole role{piece == 0, piece + 1 == written->pieces.size()};
        NoteGroup piece_group = ng;
        for (auto& note : piece_group.notes) {
            // Marks belong to the attack; later pieces carry only the tie.
            note.tie_forward = note.tie_forward || !role.release;
            if (!role.attack) {
                note.articulation.reset();
                note.ornament.reset();
                note.technical.clear();
                note.dynamic.reset();
                note.velocity.written.reset();
                note.notation_head.reset();
            }
        }
        if (piece > 0) out << " ";
        out << note_group_piece_text(piece_group,
                                     ly_written_value(written->pieces[piece].value),
                                     role,
                                     report,
                                     position,
                                     part_id,
                                     beam_boundary);
    }
    out << inferred_tuplet_close(*written);
}

/// Emit a rest event. Uses "R" for full-bar rests to enable \compressMMRests.
void emit_rest(std::ostringstream& out,
               const RestEvent& rest,
               const Beat& measure_duration,
               BeamBoundary beam_boundary,
               const TupletContext* tuplet_context,
               Beat context_ratio,
               CompilationReport& report,
               ScoreTime position,
               std::optional<PartId> part_id) {
    const Beat written_duration = tuplet_context ? tuplet_context->normal_type : rest.duration;
    if (!rest.visible) {
        // A spacer prints nothing, so an exact multiplier is its written form.
        out << "s" << exact_duration(written_duration);
        emit_beam_boundary(out, beam_boundary);
        return;
    }

    bool is_full_bar = !tuplet_context && rest.duration == measure_duration;

    if (is_full_bar) {
        // R1*5/8 is LilyPond's whole-measure rest for any metre.
        out << "R" << exact_duration(rest.duration);
        emit_beam_boundary(out, beam_boundary);
        return;
    }

    const auto written =
        project_written_duration(rest.duration, context_ratio, LILYPOND_MIN_EXPONENT);
    if (!written) {
        add_loss_diagnostic(report,
                            "LilyPond has no written rest value for this duration; emitted an "
                            "exact duration multiplier",
                            position,
                            part_id);
        out << "r" << exact_duration(written_duration);
        emit_beam_boundary(out, beam_boundary);
        return;
    }
    out << inferred_tuplet_open(*written);
    for (std::size_t piece = 0; piece < written->pieces.size(); ++piece) {
        if (piece > 0) out << " ";
        out << "r" << ly_written_value(written->pieces[piece].value);
        emit_beam_boundary(out,
                           BeamBoundary{beam_boundary.start && piece == 0,
                                        beam_boundary.end && piece + 1 == written->pieces.size()});
    }
    out << inferred_tuplet_close(*written);
}

/// Emit a ScoreDirection as a LilyPond directive.
void emit_direction(std::ostringstream& out, const ScoreDirection& dir) {
    switch (dir.type) {
    case DirectionType::Text:
        if (dir.text) out << "<>^\"" << ly_escape(*dir.text) << "\" ";
        break;
    case DirectionType::TempoText:
        if (dir.text) out << "\\tempo \"" << ly_escape(*dir.text) << "\" ";
        break;
    case DirectionType::ClefChange:
        if (dir.new_clef) out << "\\clef " << ly_clef_name(*dir.new_clef) << " ";
        break;
    case DirectionType::Coda:
        out << "<>\\coda ";
        break;
    case DirectionType::Segno:
        out << "<>\\segno ";
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
        out << "<>\\sustainOn ";
        break;
    case DirectionType::PedalUp:
        out << "<>\\sustainOff ";
        break;
    case DirectionType::BreathMark:
        out << "\\breathe ";
        break;
    case DirectionType::Caesura:
        out << "\\caesura ";
        break;
    }
}

std::string display_pitch_name(SpelledPitch pitch) {
    static constexpr std::array letters{'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    std::string result(1, letters[pitch.letter < letters.size() ? pitch.letter : 0]);
    if (pitch.accidental > 0)
        result.append(static_cast<std::size_t>(pitch.accidental), '#');
    else if (pitch.accidental < 0)
        result.append(static_cast<std::size_t>(-pitch.accidental), 'b');
    return result;
}

std::string chord_symbol_text(const ChordSymbolEvent& symbol) {
    std::string text = display_pitch_name(symbol.root) + symbol.quality;
    if (symbol.bass) text += "/" + display_pitch_name(*symbol.bass);
    if (symbol.roman) text += " [" + *symbol.roman + "]";
    for (const auto& extension : symbol.extensions)
        text += " " + extension;
    return text;
}

const char* tempo_beat_unit(BeatUnit unit) {
    switch (unit) {
    case BeatUnit::Whole:
        return "1";
    case BeatUnit::Half:
        return "2";
    case BeatUnit::DottedHalf:
        return "2.";
    case BeatUnit::Quarter:
        return "4";
    case BeatUnit::DottedQuarter:
        return "4.";
    case BeatUnit::Eighth:
        return "8";
    case BeatUnit::DottedEighth:
        return "8.";
    case BeatUnit::Sixteenth:
        return "16";
    }
    return "4";
}

std::string tempo_rate(const PositiveRational& bpm) {
    if (bpm.denominator() == 1) return std::to_string(bpm.numerator());
    return "#(/ " + std::to_string(bpm.numerator()) + " " + std::to_string(bpm.denominator()) + ")";
}

void emit_tempo(std::ostringstream& out,
                const TempoEvent& tempo,
                CompilationReport& report,
                std::optional<PartId> part_id) {
    out << "\\tempo ";
    if (tempo.transition_type == TempoTransitionType::MetricModulation) {
        out << "\\markup { \\italic \"metric modulation " << tempo_beat_unit(tempo.old_unit)
            << " = " << tempo_beat_unit(tempo.new_unit) << "\" } ";
        add_loss_diagnostic(
            report,
            "LilyPond retained Sunny's metric-modulation units as text and emitted the exact "
            "resulting metronome rate",
            tempo.position,
            part_id);
    }
    out << tempo_beat_unit(tempo.beat_unit) << " = " << tempo_rate(tempo.bpm) << " ";
}

void emit_linear_tempo_start(std::ostringstream& out,
                             ScoreTime start,
                             CompilationReport& report,
                             std::optional<PartId> part_id) {
    out << "<>^\\markup { \\italic \"linear tempo transition\" } ";
    add_loss_diagnostic(
        report,
        "LilyPond metronome marks do not encode Sunny's Linear tempo curve/duration; emitted an "
        "explicit label at the transition start and the exact metronome mark at its endpoint",
        start,
        part_id);
}

void collect_annotation_offset(std::set<Beat>& offsets, Beat offset, const Beat& measure_duration) {
    if (offset >= Beat::zero() && offset <= measure_duration) offsets.insert(offset);
}

std::string directive_label(const PartDirective& directive) {
    std::string label{directive_name(directive.directive)};
    if (directive.directive == DirectiveType::Divisi)
        label += " a " + std::to_string(directive.divisi_count);
    return label;
}

void emit_directive_text(std::ostringstream& out, const std::string& text) {
    out << "<>^\\markup { \\italic \"" << ly_escape(text) << "\" } ";
}

/// Emit a parallel spacer voice for point metadata. This avoids moving a direction,
/// chord symbol, tempo, key change, rehearsal mark, or hairpin endpoint to the end of
/// a sustained measured event merely because it appears later in the event vector.
bool emit_annotation_layer(std::ostringstream& out,
                           const Score& score,
                           const Part& part,
                           const Measure& measure,
                           Beat measure_duration,
                           std::size_t part_index,
                           std::uint8_t staff_index,
                           CompilationReport& report) {
    const auto bar = measure.bar_number;
    const bool part_scope_owner = staff_index == 0;
    const bool global_scope_owner = part_index == 0 && part_scope_owner;
    std::set<Beat> offsets;

    for (const auto& voice : measure.voices) {
        if (voice.staff_index != staff_index) continue;
        for (const auto& event : voice.events) {
            if (event.is_direction() || event.is_chord_symbol())
                collect_annotation_offset(offsets, event.offset, measure_duration);
        }
    }
    if (part_scope_owner) {
        for (const auto& hairpin : part.hairpins) {
            if (hairpin.start.bar == bar)
                collect_annotation_offset(offsets, hairpin.start.beat, measure_duration);
            if (hairpin.end.bar == bar)
                collect_annotation_offset(offsets, hairpin.end.beat, measure_duration);
            if (bar == score.metadata.total_bars && hairpin.end.bar == bar + 1 &&
                hairpin.end.beat == Beat::zero())
                collect_annotation_offset(offsets, measure_duration, measure_duration);
        }
        for (const auto& directive : part.part_directives) {
            if (directive.start.bar == bar)
                collect_annotation_offset(offsets, directive.start.beat, measure_duration);
            if (directive.end.bar == bar)
                collect_annotation_offset(offsets, directive.end.beat, measure_duration);
            if (bar == score.metadata.total_bars && directive.end.bar == bar + 1 &&
                directive.end.beat == Beat::zero())
                collect_annotation_offset(offsets, measure_duration, measure_duration);
        }
    }
    for (const auto& key : score.key_map) {
        if (key.position.bar == bar && key.position.beat > Beat::zero())
            collect_annotation_offset(offsets, key.position.beat, measure_duration);
    }
    if (global_scope_owner) {
        for (const auto& tempo : score.tempo_map) {
            if (tempo.position.bar == bar)
                collect_annotation_offset(offsets, tempo.position.beat, measure_duration);
        }
        for (const auto& mark : score.rehearsal_marks) {
            if (mark.position.bar == bar)
                collect_annotation_offset(offsets, mark.position.beat, measure_duration);
        }
    }

    if (offsets.empty()) return false;

    Beat cursor = Beat::zero();
    for (const auto& offset : offsets) {
        emit_spacer(out, offset - cursor);

        for (const auto& key : score.key_map) {
            if (key.position.bar == bar && key.position.beat == offset && offset > Beat::zero())
                emit_key_signature(out, key.key, report, key.position, part.id);
        }
        if (global_scope_owner) {
            for (std::size_t tempo_index = 0; tempo_index < score.tempo_map.size(); ++tempo_index) {
                const auto& tempo = score.tempo_map[tempo_index];
                if (tempo.position.bar == bar && tempo.position.beat == offset)
                    emit_tempo(out, tempo, report, part.id);
                if (tempo_index + 1 < score.tempo_map.size() && tempo.position.bar == bar &&
                    tempo.position.beat == offset &&
                    score.tempo_map[tempo_index + 1].transition_type == TempoTransitionType::Linear)
                    emit_linear_tempo_start(out, tempo.position, report, part.id);
            }
            for (const auto& mark : score.rehearsal_marks) {
                if (mark.position.bar == bar && mark.position.beat == offset)
                    out << "\\mark \\markup { \\box \"" << ly_escape(mark.label) << "\" } ";
            }
        }

        if (part_scope_owner) {
            for (const auto& hairpin : part.hairpins) {
                const bool terminal_score_endpoint =
                    bar == score.metadata.total_bars && hairpin.end.bar == bar + 1 &&
                    hairpin.end.beat == Beat::zero() && offset == measure_duration;
                if ((hairpin.end.bar == bar && hairpin.end.beat == offset) ||
                    terminal_score_endpoint)
                    out << "<>\\! ";
                if (((hairpin.end.bar == bar && hairpin.end.beat == offset) ||
                     terminal_score_endpoint) &&
                    hairpin.target)
                    out << "<>" << ly_dynamic(*hairpin.target) << " ";
            }
            for (const auto& directive : part.part_directives) {
                const bool terminal_score_endpoint =
                    bar == score.metadata.total_bars && directive.end.bar == bar + 1 &&
                    directive.end.beat == Beat::zero() && offset == measure_duration;
                if ((directive.end.bar == bar && directive.end.beat == offset) ||
                    terminal_score_endpoint)
                    emit_directive_text(out, "end " + directive_label(directive));
            }
            for (const auto& hairpin : part.hairpins) {
                if (hairpin.start.bar == bar && hairpin.start.beat == offset)
                    out << (hairpin.type == HairpinType::Crescendo ? "<>\\< " : "<>\\> ");
            }
            for (const auto& directive : part.part_directives) {
                if (directive.start.bar == bar && directive.start.beat == offset)
                    emit_directive_text(out, directive_label(directive));
            }
        }

        for (const auto& voice : measure.voices) {
            if (voice.staff_index != staff_index) continue;
            for (const auto& event : voice.events) {
                if (event.offset != offset) continue;
                if (const auto* direction = std::get_if<ScoreDirection>(&event.payload)) {
                    emit_direction(out, *direction);
                } else if (const auto* symbol = std::get_if<ChordSymbolEvent>(&event.payload)) {
                    out << "<>^\\markup { \\bold \"" << ly_escape(chord_symbol_text(*symbol))
                        << "\" } ";
                    add_loss_diagnostic(
                        report,
                        "ChordSymbol was preserved at its exact LilyPond time as display text; "
                        "Sunny's structured numeral/degree semantics and free-form presentation "
                        "cannot be mapped losslessly to the admitted LilyPond chordmode profile",
                        ScoreTime{bar, offset},
                        part.id);
                }
            }
        }
        cursor = offset;
    }
    emit_spacer(out, measure_duration - cursor);
    return true;
}

// =============================================================================
// Voice emission
// =============================================================================

/// Emit all events in a single voice for one measure.
void emit_voice_events(std::ostringstream& out,
                       const Voice& voice,
                       const Beat& measure_duration,
                       CompilationReport& report,
                       std::uint32_t bar_number,
                       std::optional<PartId> part_id) {
    const auto contexts = collect_tuplet_contexts(voice);
    std::vector<TupletId> active_tuplets;

    // Written-to-sounding ratio of every enclosing tuplet of one event.
    const auto context_ratio = [&](const Event& event) {
        const auto chain = tuplet_context_chain(event_tuplet_context(event), contexts);
        if (!chain) return Beat::one();
        return cumulative_tuplet_written_ratio(*chain).value_or(Beat::one());
    };

    const auto transition_tuplets = [&](const TupletContext* leaf) {
        const auto chain_result = tuplet_context_chain(leaf, contexts);
        std::vector<const TupletContext*> chain;
        if (chain_result) chain = *chain_result;

        std::size_t common = 0;
        while (common < active_tuplets.size() && common < chain.size() &&
               active_tuplets[common] == chain[common]->id)
            ++common;
        for (std::size_t i = active_tuplets.size(); i > common; --i)
            out << "} ";
        active_tuplets.resize(common);
        for (std::size_t i = common; i < chain.size(); ++i) {
            const auto* context = chain[i];
            out << "\\tuplet " << static_cast<int>(context->actual) << "/"
                << static_cast<int>(context->normal) << " { ";
            active_tuplets.push_back(context->id);
        }
    };

    for (std::size_t ei = 0; ei < voice.events.size(); ++ei) {
        const Event& event = voice.events[ei];
        ScoreTime position{bar_number, event.offset};

        if (event.is_note_group()) {
            const NoteGroup& ng = *event.as_note_group();
            transition_tuplets(ng.tuplet_context ? &*ng.tuplet_context : nullptr);

            const auto beam = beam_boundary_for(voice, event.id);
            emit_note_group(out, ng, context_ratio(event), report, position, part_id, beam);
            out << " ";

        } else if (event.is_rest()) {
            const RestEvent& rest = *event.as_rest();
            transition_tuplets(rest.tuplet_context ? &*rest.tuplet_context : nullptr);

            const auto beam = beam_boundary_for(voice, event.id);
            emit_rest(out,
                      rest,
                      measure_duration,
                      beam,
                      rest.tuplet_context ? &*rest.tuplet_context : nullptr,
                      context_ratio(event),
                      report,
                      position,
                      part_id);
            out << " ";
        }
    }

    transition_tuplets(nullptr);
}

const Voice*
find_staff_voice(const Measure& measure, std::uint8_t staff_index, std::uint8_t voice_index) {
    const auto found = std::ranges::find_if(measure.voices, [&](const Voice& voice) {
        return voice.staff_index == staff_index && voice.voice_index == voice_index;
    });
    return found == measure.voices.end() ? nullptr : &*found;
}

bool ordinary_lyric_note_group(const Event& event) {
    const auto* group = event.as_note_group();
    return group && !std::ranges::any_of(group->notes,
                                         [](const Note& note) { return note.grace.has_value(); });
}

std::map<std::uint8_t, std::set<std::uint16_t>> lyric_lanes_for_staff(const Part& part,
                                                                      std::uint8_t staff_index) {
    std::map<std::uint8_t, std::set<std::uint16_t>> result;
    for (const auto& measure : part.measures) {
        for (const auto& voice : measure.voices) {
            if (voice.staff_index != staff_index) continue;
            for (const auto& event : voice.events) {
                if (!ordinary_lyric_note_group(event)) continue;
                const auto* group = event.as_note_group();
                if (!group || group->notes.empty()) continue;
                for (const auto& lyric : group->notes.front().lyrics)
                    result[voice.voice_index].insert(lyric.verse);
            }
        }
    }
    return result;
}

std::string
lyric_anchor_name(std::string_view part_name, std::uint8_t staff_index, std::uint8_t voice_index) {
    return std::string(part_name) + "Staff" + std::to_string(staff_index) + "Voice" +
           std::to_string(voice_index) + "LyricsAnchor";
}

void emit_lyric_anchor(std::ostringstream& out,
                       const Score& score,
                       const Part& part,
                       std::uint8_t staff_index,
                       std::uint8_t voice_index,
                       std::string_view anchor_name) {
    out << "  \\new NullVoice = \"" << anchor_name << "\" { ";
    for (const auto& measure : part.measures) {
        const auto signature =
            measure.local_time.value_or(time_at_bar(score.time_map, measure.bar_number));
        const auto* voice = find_staff_voice(measure, staff_index, voice_index);
        if (!voice) {
            emit_spacer(out, signature.measure_duration());
            out << "| ";
            continue;
        }
        for (const auto& event : voice->events) {
            if (ordinary_lyric_note_group(event)) {
                out << "c" << exact_duration(event.duration()) << " ";
            } else if (event.is_note_group() || event.is_rest()) {
                emit_spacer(out, event.duration());
            }
        }
        out << "| ";
    }
    out << "}\n";
}

void emit_lyrics_context(std::ostringstream& out,
                         const Part& part,
                         std::uint8_t staff_index,
                         std::uint8_t voice_index,
                         std::uint16_t verse,
                         std::string_view anchor_name) {
    out << "  \\new Lyrics \\lyricsto \"" << anchor_name << "\" { \\set stanza = \"" << verse
        << ".\" \\set extendersOverRests = ##t ";
    for (const auto& measure : part.measures) {
        const auto* voice = find_staff_voice(measure, staff_index, voice_index);
        if (!voice) continue;
        for (const auto& event : voice->events) {
            if (!ordinary_lyric_note_group(event)) continue;
            const auto* group = event.as_note_group();
            const LyricSyllable* syllable = nullptr;
            if (group && !group->notes.empty()) {
                const auto found = std::ranges::find_if(
                    group->notes.front().lyrics,
                    [verse](const LyricSyllable& lyric) { return lyric.verse == verse; });
                if (found != group->notes.front().lyrics.end()) syllable = &*found;
            }
            if (!syllable) {
                out << "_ ";
                continue;
            }
            out << "\"" << ly_escape(syllable->text) << "\" ";
            if (syllable->syllabic == LyricSyllabic::Begin ||
                syllable->syllabic == LyricSyllabic::Middle)
                out << "-- ";
            if (syllable->extend) out << "__ ";
        }
    }
    out << "}\n";
}

} // anonymous namespace

// =============================================================================
// compile_score_to_lilypond
// =============================================================================

Result<LilyPondCompilationResult> compile_score_to_lilypond(const Score& score) {
    if (!is_compilable(score)) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }

    CompilationReport report;
    report.tuning_definitions_requested = 1;
    if (is_standard_midi_tuning(score.tuning)) {
        report.tuning_definitions_written = 1;
    } else {
        report.diagnostics.push_back(
            {"Score tuning was not written: Sunny's admitted LilyPond 2.24 profile preserves "
             "notation pitch but has no complete 128-note sounding-pitch carrier",
             std::nullopt,
             std::nullopt});
    }
    if (!score.section_map.empty()) {
        add_loss_diagnostic(report,
                            "The hierarchical SectionMap was not represented in the LilyPond "
                            "notation profile",
                            score.section_map.front().start);
    }
    if (!score.harmonic_annotations.empty()) {
        add_loss_diagnostic(
            report,
            "The persisted harmonic-analysis layer was not represented in the LilyPond notation "
            "profile; Voice ChordSymbol events remain a distinct exported construct",
            score.harmonic_annotations.front().position);
    }
    if (!score.orchestration_annotations.empty()) {
        add_loss_diagnostic(
            report,
            "The orchestration-analysis layer, including any rendering-only dynamic balance, was "
            "not represented in the LilyPond notation profile",
            score.orchestration_annotations.front().start,
            score.orchestration_annotations.front().part_id);
    }
    if (score.tone_row) {
        add_loss_diagnostic(report,
                            "The governing twelve-tone row was not represented in the LilyPond "
                            "notation profile");
    }
    if (!score.stale_harmonic_regions.empty()) {
        add_loss_diagnostic(report,
                            "Stale harmonic-analysis region state was not represented in the "
                            "LilyPond notation profile",
                            score.stale_harmonic_regions.front().start);
    }
    if (!score.stale_orchestration_regions.empty()) {
        add_loss_diagnostic(report,
                            "Stale orchestration-analysis region state was not represented in the "
                            "LilyPond notation profile",
                            score.stale_orchestration_regions.front().start);
    }
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group) continue;
                    for (const auto& note : group->notes) {
                        const ScoreTime position{measure.bar_number, event.offset};
                        if (!semantic_dynamic(note) && is_valid_velocity(note.velocity.value)) {
                            add_loss_diagnostic(
                                report,
                                "Explicit numeric note attack velocity was not represented in "
                                "the LilyPond notation profile; the admitted profile carries "
                                "semantic dynamic marks rather than an exact per-note MIDI byte",
                                position,
                                part.id);
                        }
                        if (note.release_velocity != 64) {
                            add_loss_diagnostic(
                                report,
                                "Non-default note release velocity was not represented in the "
                                "LilyPond notation profile",
                                position,
                                part.id);
                        }
                    }
                }
            }
        }
    }
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
            // \transpose <from> <to> moves concert music by from -> to. Written C4
            // sounds at C4 plus the written-to-sounding interval (B-flat 3 for a
            // B-flat instrument), so \transpose bes c' writes concert pitch a
            // major second higher. The diatonic interval spells the pitch.
            SpelledPitch concert_c{0, 0, 4}; // C4
            const SpelledPitch sounding_pitch =
                apply_interval(concert_c, written_to_sounding_interval(def));

            out << "\\transpose " << compilable_ly_pitch(sounding_pitch) << " "
                << compilable_ly_pitch(concert_c) << " { ";
        }

        const bool multi_staff = def.staff_count > 1;
        out << (multi_staff ? "\\new PianoStaff \\with {\n" : "<<\n  \\new Staff \\with {\n");
        out << "  instrumentName = \"" << ly_escape(def.name) << "\"\n";
        if (!def.abbreviation.empty()) {
            out << "  shortInstrumentName = \"" << ly_escape(def.abbreviation) << "\"\n";
        }
        out << "} <<\n";

        for (std::uint16_t staff = 0; staff < def.staff_count; ++staff) {
            const auto staff_index = static_cast<std::uint8_t>(staff);
            const auto lyric_lanes = lyric_lanes_for_staff(part, staff_index);
            if (multi_staff) out << "  \\new Staff <<\n";
            out << "  {\n";

            const auto initial_clef =
                def.staff_clefs.size() == def.staff_count ? def.staff_clefs[staff_index] : def.clef;
            out << "  \\clef " << ly_clef_name(initial_clef) << "\n";

            // LilyPond key state belongs to each Staff, so every configured Staff
            // receives the complete initial and changing key sequence.
            if (!score.key_map.empty()) {
                const KeySignature& initial_key = score.key_map.front().key;
                out << "  ";
                emit_key_signature(out, initial_key, report, SCORE_START, part.id);
                out << "\n";
            }
            if (!score.time_map.empty()) {
                const TimeSignature& initial_ts = score.time_map.front().time_signature;
                out << "  " << score_time_signature_to_lilypond(initial_ts) << "\n";
            }

            out << "  ";
            std::optional<KeySignature> prev_key =
                score.key_map.empty() ? std::nullopt
                                      : std::optional<KeySignature>{score.key_map.front().key};
            std::optional<TimeSignature> prev_ts =
                score.time_map.empty()
                    ? std::nullopt
                    : std::optional<TimeSignature>{score.time_map.front().time_signature};

            for (const auto& measure : part.measures) {
                const std::uint32_t bar = measure.bar_number;
                const KeySignature current_key =
                    measure.local_key.value_or(key_at_bar(score.key_map, bar));
                if (prev_key && !same_key_signature_state(current_key, *prev_key)) {
                    emit_key_signature(
                        out, current_key, report, ScoreTime{bar, Beat::zero()}, part.id);
                }
                prev_key = current_key;

                const TimeSignature current_ts =
                    measure.local_time.value_or(time_at_bar(score.time_map, bar));
                if (prev_ts && current_ts != *prev_ts) {
                    out << score_time_signature_to_lilypond(current_ts) << " ";
                }
                prev_ts = current_ts;

                const Beat measure_dur = current_ts.measure_duration();
                std::ostringstream annotation_out;
                const bool has_annotations = emit_annotation_layer(
                    annotation_out, score, part, measure, measure_dur, pi, staff_index, report);

                std::vector<const Voice*> staff_voices;
                for (const auto& voice : measure.voices) {
                    if (voice.staff_index == staff_index) staff_voices.push_back(&voice);
                }

                // Point annotations run as a parallel spacer joined to the
                // first voice without `\\`: `<< {...} \\ {...} >>` would create
                // new voices with forced stems and break ties at the barline
                // (LilyPond Notation Reference 1.5.2). Only genuine polyphony
                // uses the separator.
                if (staff_voices.size() <= 1) {
                    if (has_annotations) out << "<< { ";
                    if (staff_voices.empty())
                        emit_spacer(out, measure_dur);
                    else
                        emit_voice_events(
                            out, *staff_voices.front(), measure_dur, report, bar, part.id);
                    if (has_annotations) out << "} { " << annotation_out.str() << "} >> ";
                } else {
                    out << "<< ";
                    for (std::size_t vi = 0; vi < staff_voices.size(); ++vi) {
                        if (vi > 0) out << "\\\\ ";
                        out << "{ ";
                        emit_voice_events(
                            out, *staff_voices[vi], measure_dur, report, bar, part.id);
                        out << "} ";
                        if (vi == 0 && has_annotations) out << "{ " << annotation_out.str() << "} ";
                    }
                    out << ">> ";
                }
                out << "| ";
            }

            out << "\n  }\n";
            for (const auto& [voice_index, verses] : lyric_lanes) {
                (void)verses;
                const auto anchor = lyric_anchor_name(var_name, staff_index, voice_index);
                emit_lyric_anchor(out, score, part, staff_index, voice_index, anchor);
            }
            out << "  >>\n";
            for (const auto& [voice_index, verses] : lyric_lanes) {
                const auto anchor = lyric_anchor_name(var_name, staff_index, voice_index);
                for (const auto verse : verses)
                    emit_lyrics_context(out, part, staff_index, voice_index, verse, anchor);
            }
        }

        out << ">>\n";
        if (transposing) out << "}\n";
        out << "\n";
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

} // namespace sunny::infrastructure::formats
