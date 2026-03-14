/**
 * @file FMMX002A.cpp
 * @brief MusicXML compilation from Score IR — implementation
 *
 * Component: FMMX002A
 * Domain: FM (Format) | Category: MX (MusicXML)
 */

#include "FMMX002A.h"
#include "FMMX_Internal.h"
#include "FMMX001A.h"

#include "../../Sunny.Core/Score/SIDC001A.h"
#include "../../Sunny.Core/Score/SITP001A.h"
#include "../../Sunny.Core/Score/SICM001A.h"
#include "../../Sunny.Core/Score/SIVD001A.h"
#include "../../Sunny.Core/Score/SITM001A.h"

#include <pugixml.hpp>
#include <sstream>

namespace Sunny::Infrastructure::Format {

using namespace MxmlInternal;

namespace {

// -----------------------------------------------------------------------------
// Key/mode helpers
// -----------------------------------------------------------------------------

bool is_minor_key(const Sunny::Core::KeySignature& key) {
    auto ints = key.mode.get_intervals();
    return ints.size() >= 3 && ints[2] == 3;
}

// -----------------------------------------------------------------------------
// Clef mapping
// -----------------------------------------------------------------------------

struct ClefMapping {
    const char* sign;
    int line;
};

ClefMapping clef_to_mxml(Sunny::Core::Clef clef) {
    switch (clef) {
        case Sunny::Core::Clef::Treble:     return {"G", 2};
        case Sunny::Core::Clef::Bass:       return {"F", 4};
        case Sunny::Core::Clef::Alto:       return {"C", 3};
        case Sunny::Core::Clef::Tenor:      return {"C", 4};
        case Sunny::Core::Clef::Percussion: return {"percussion", 0};
        case Sunny::Core::Clef::Tab:        return {"TAB", 5};
    }
    return {"G", 2};
}

// -----------------------------------------------------------------------------
// Dynamic level to MusicXML element name
// -----------------------------------------------------------------------------

const char* dynamic_to_mxml(Sunny::Core::DynamicLevel level) {
    switch (level) {
        case Sunny::Core::DynamicLevel::pppp: return "pppp";
        case Sunny::Core::DynamicLevel::ppp:  return "ppp";
        case Sunny::Core::DynamicLevel::pp:   return "pp";
        case Sunny::Core::DynamicLevel::p:    return "p";
        case Sunny::Core::DynamicLevel::mp:   return "mp";
        case Sunny::Core::DynamicLevel::mf:   return "mf";
        case Sunny::Core::DynamicLevel::f:    return "f";
        case Sunny::Core::DynamicLevel::ff:   return "ff";
        case Sunny::Core::DynamicLevel::fff:  return "fff";
        case Sunny::Core::DynamicLevel::ffff: return "ffff";
        case Sunny::Core::DynamicLevel::sfz:  return "sfz";
        case Sunny::Core::DynamicLevel::fp:   return "fp";
        case Sunny::Core::DynamicLevel::sfp:  return "sfp";
        case Sunny::Core::DynamicLevel::rfz:  return "rfz";
    }
    return "mf";
}

// -----------------------------------------------------------------------------
// Articulation classification
// -----------------------------------------------------------------------------

enum class ArticulationCategory { Articulations, Ornaments, Technical, Notations };

struct ArticulationMxmlEntry {
    Sunny::Core::ArticulationType type;
    const char* element_name;
    ArticulationCategory category;
};

constexpr ArticulationMxmlEntry ARTICULATION_MAP[] = {
    {Sunny::Core::ArticulationType::Staccato,       "staccato",         ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Staccatissimo,  "staccatissimo",    ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Accent,         "accent",           ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Marcato,        "strong-accent",    ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Tenuto,         "tenuto",           ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Portato,        "detached-legato",  ArticulationCategory::Articulations},
    {Sunny::Core::ArticulationType::Fermata,        "fermata",          ArticulationCategory::Notations},
    {Sunny::Core::ArticulationType::Trill,          "trill-mark",       ArticulationCategory::Ornaments},
    {Sunny::Core::ArticulationType::Mordent,        "mordent",          ArticulationCategory::Ornaments},
    {Sunny::Core::ArticulationType::Turn,           "turn",             ArticulationCategory::Ornaments},
    {Sunny::Core::ArticulationType::Tremolo,        "tremolo",          ArticulationCategory::Ornaments},
    {Sunny::Core::ArticulationType::SnapPizzicato,  "snap-pizzicato",   ArticulationCategory::Technical},
    {Sunny::Core::ArticulationType::Harmonic,       "harmonic",         ArticulationCategory::Technical},
    {Sunny::Core::ArticulationType::UpBow,          "up-bow",           ArticulationCategory::Technical},
    {Sunny::Core::ArticulationType::DownBow,        "down-bow",         ArticulationCategory::Technical},
};

const ArticulationMxmlEntry* find_articulation_entry(Sunny::Core::ArticulationType art) {
    for (const auto& entry : ARTICULATION_MAP) {
        if (entry.type == art) return &entry;
    }
    return nullptr;
}

bool is_unsupported_articulation(Sunny::Core::ArticulationType art) {
    switch (art) {
        case Sunny::Core::ArticulationType::InvertedMordent:
        case Sunny::Core::ArticulationType::InvertedTurn:
        case Sunny::Core::ArticulationType::BendUp:
        case Sunny::Core::ArticulationType::BendDown:
        case Sunny::Core::ArticulationType::GlissandoStart:
        case Sunny::Core::ArticulationType::GlissandoEnd:
        case Sunny::Core::ArticulationType::OpenString:
        case Sunny::Core::ArticulationType::Stopped:
        case Sunny::Core::ArticulationType::Muted:
            return true;
        default:
            return false;
    }
}

// -----------------------------------------------------------------------------
// Chord quality to MusicXML <kind>
// -----------------------------------------------------------------------------

const char* quality_to_mxml_kind(const std::string& quality) {
    if (quality == "major" || quality == "maj" || quality == "M") return "major";
    if (quality == "minor" || quality == "min" || quality == "m") return "minor";
    if (quality == "augmented" || quality == "aug" || quality == "+") return "augmented";
    if (quality == "diminished" || quality == "dim" || quality == "o") return "diminished";
    if (quality == "dominant" || quality == "dom" || quality == "7") return "dominant";
    if (quality == "major-seventh" || quality == "maj7" || quality == "M7") return "major-seventh";
    if (quality == "minor-seventh" || quality == "m7" || quality == "min7") return "minor-seventh";
    if (quality == "diminished-seventh" || quality == "dim7") return "diminished-seventh";
    if (quality == "half-diminished" || quality == "m7b5") return "half-diminished";
    if (quality == "augmented-seventh" || quality == "aug7") return "augmented-seventh";
    if (quality == "suspended-second" || quality == "sus2") return "suspended-second";
    if (quality == "suspended-fourth" || quality == "sus4") return "suspended-fourth";
    return "other";
}

// -----------------------------------------------------------------------------
// Collect all NoteGroup durations across entire score for divisions computation
// -----------------------------------------------------------------------------

std::vector<Sunny::Core::Beat> collect_all_durations(const Sunny::Core::Score& score) {
    std::vector<Sunny::Core::Beat> durations;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    auto dur = event.duration();
                    if (dur != Sunny::Core::Beat::zero()) {
                        durations.push_back(dur);
                    }
                }
            }
        }
    }
    return durations;
}

// -----------------------------------------------------------------------------
// Lookup helpers for global maps
// -----------------------------------------------------------------------------

/// Find the key signature active at a given bar number.
const Sunny::Core::KeySignatureEntry* key_at_bar(
    const Sunny::Core::KeySignatureMap& key_map, std::uint32_t bar
) {
    const Sunny::Core::KeySignatureEntry* active = nullptr;
    for (const auto& entry : key_map) {
        if (entry.position.bar <= bar) {
            active = &entry;
        } else {
            break;
        }
    }
    return active;
}

/// Find the time signature active at a given bar number.
const Sunny::Core::TimeSignatureEntry* time_at_bar(
    const Sunny::Core::TimeSignatureMap& time_map, std::uint32_t bar
) {
    const Sunny::Core::TimeSignatureEntry* active = nullptr;
    for (const auto& entry : time_map) {
        if (entry.bar <= bar) {
            active = &entry;
        } else {
            break;
        }
    }
    return active;
}

/// Check whether the key signature changes at this bar relative to the previous bar.
bool key_changes_at_bar(
    const Sunny::Core::KeySignatureMap& key_map, std::uint32_t bar
) {
    for (const auto& entry : key_map) {
        if (entry.position.bar == bar && bar > 1) return true;
    }
    return false;
}

/// Check whether the time signature changes at this bar relative to the previous bar.
bool time_changes_at_bar(
    const Sunny::Core::TimeSignatureMap& time_map, std::uint32_t bar
) {
    for (const auto& entry : time_map) {
        if (entry.bar == bar && bar > 1) return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Emit <attributes> element
// -----------------------------------------------------------------------------

void emit_attributes(
    pugi::xml_node measure_node,
    int divisions,
    const Sunny::Core::KeySignature* key,
    const Sunny::Core::TimeSignature* time_sig,
    const Sunny::Core::Clef* clef,
    int transposition
) {
    auto attrs = measure_node.append_child("attributes");
    attrs.append_child("divisions").text().set(divisions);

    if (key) {
        auto key_node = attrs.append_child("key");
        bool minor = is_minor_key(*key);
        int fifths = lof_to_fifths(key->root, !minor);
        key_node.append_child("fifths").text().set(fifths);
        key_node.append_child("mode").text().set(minor ? "minor" : "major");
    }

    if (time_sig) {
        auto time_node = attrs.append_child("time");
        time_node.append_child("beats").text().set(time_sig->numerator());
        time_node.append_child("beat-type").text().set(time_sig->denominator);
    }

    if (clef) {
        auto clef_node = attrs.append_child("clef");
        auto mapping = clef_to_mxml(*clef);
        clef_node.append_child("sign").text().set(mapping.sign);
        if (mapping.line > 0) {
            clef_node.append_child("line").text().set(mapping.line);
        }
    }

    if (transposition != 0) {
        auto trans_node = attrs.append_child("transpose");
        trans_node.append_child("chromatic").text().set(transposition);
    }
}

// -----------------------------------------------------------------------------
// Emit <direction> for dynamics
// -----------------------------------------------------------------------------

void emit_dynamic_direction(
    pugi::xml_node parent,
    Sunny::Core::DynamicLevel level,
    int voice_number
) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto dynamics_node = dir_type.append_child("dynamics");
    dynamics_node.append_child(dynamic_to_mxml(level));
    dir.append_child("voice").text().set(voice_number);
}

// -----------------------------------------------------------------------------
// Emit <direction> for tempo
// -----------------------------------------------------------------------------

void emit_tempo_direction(pugi::xml_node parent, double bpm) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";
    auto dir_type = dir.append_child("direction-type");
    dir_type.append_child("words").text().set(
        (std::string("q = ") + std::to_string(static_cast<int>(bpm + 0.5))).c_str());
    auto sound = dir.append_child("sound");
    sound.append_attribute("tempo") = bpm;
}

// -----------------------------------------------------------------------------
// Emit <direction> for rehearsal marks
// -----------------------------------------------------------------------------

void emit_rehearsal_direction(pugi::xml_node parent, const std::string& label) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";
    auto dir_type = dir.append_child("direction-type");
    dir_type.append_child("rehearsal").text().set(label.c_str());
}

// -----------------------------------------------------------------------------
// Emit <direction> for hairpin wedge start/stop
// -----------------------------------------------------------------------------

void emit_hairpin_start(
    pugi::xml_node parent,
    Sunny::Core::HairpinType type
) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto wedge = dir_type.append_child("wedge");
    wedge.append_attribute("type") =
        (type == Sunny::Core::HairpinType::Crescendo) ? "crescendo" : "diminuendo";
}

void emit_hairpin_stop(pugi::xml_node parent) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto wedge = dir_type.append_child("wedge");
    wedge.append_attribute("type") = "stop";
}

// -----------------------------------------------------------------------------
// Emit a ScoreDirection event as <direction> element
// -----------------------------------------------------------------------------

void emit_score_direction(
    pugi::xml_node parent,
    const Sunny::Core::ScoreDirection& sd,
    Sunny::Core::CompilationReport& /*report*/,
    std::optional<Sunny::Core::ScoreTime> /*location*/,
    std::optional<Sunny::Core::PartId> /*part_id*/
) {
    using DT = Sunny::Core::DirectionType;

    switch (sd.type) {
        case DT::TempoText: {
            auto dir = parent.append_child("direction");
            dir.append_attribute("placement") = "above";
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("words").text().set(
                sd.text.value_or("").c_str());
            break;
        }
        case DT::Text: {
            auto dir = parent.append_child("direction");
            dir.append_attribute("placement") = "above";
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("words").text().set(
                sd.text.value_or("").c_str());
            break;
        }
        case DT::PedalDown: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            auto pedal = dir_type.append_child("pedal");
            pedal.append_attribute("type") = "start";
            break;
        }
        case DT::PedalUp: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            auto pedal = dir_type.append_child("pedal");
            pedal.append_attribute("type") = "stop";
            break;
        }
        case DT::OttavaStart: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            auto shift = dir_type.append_child("octave-shift");
            int size = (sd.ottava_shift > 0) ? sd.ottava_shift : -sd.ottava_shift;
            shift.append_attribute("type") = "down";
            shift.append_attribute("size") = size;
            break;
        }
        case DT::OttavaEnd: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            auto shift = dir_type.append_child("octave-shift");
            shift.append_attribute("type") = "stop";
            break;
        }
        case DT::BreathMark: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("other-direction").text().set("breath-mark");
            break;
        }
        case DT::Caesura: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("other-direction").text().set("caesura");
            break;
        }
        case DT::Coda: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("coda");
            break;
        }
        case DT::Segno: {
            auto dir = parent.append_child("direction");
            auto dir_type = dir.append_child("direction-type");
            dir_type.append_child("segno");
            break;
        }
        case DT::ClefChange:
        case DT::DoubleBarline:
        case DT::FinalBarline:
        case DT::RepeatStart:
        case DT::RepeatEnd:
            // Handled elsewhere or not emittable as <direction>
            break;
    }
}

// -----------------------------------------------------------------------------
// Emit a <harmony> element for ChordSymbolEvent
// -----------------------------------------------------------------------------

void emit_chord_symbol(
    pugi::xml_node parent,
    const Sunny::Core::ChordSymbolEvent& cs
) {
    auto harmony = parent.append_child("harmony");

    auto root_node = harmony.append_child("root");
    std::string step_str(1, STEP_CHARS[cs.root.letter < 7 ? cs.root.letter : 0]);
    root_node.append_child("root-step").text().set(step_str.c_str());
    if (cs.root.accidental != 0) {
        root_node.append_child("root-alter").text().set(
            static_cast<int>(cs.root.accidental));
    }

    harmony.append_child("kind").text().set(
        quality_to_mxml_kind(cs.quality));

    if (cs.bass) {
        auto bass_node = harmony.append_child("bass");
        std::string bass_step(1, STEP_CHARS[cs.bass->letter < 7 ? cs.bass->letter : 0]);
        bass_node.append_child("bass-step").text().set(bass_step.c_str());
        if (cs.bass->accidental != 0) {
            bass_node.append_child("bass-alter").text().set(
                static_cast<int>(cs.bass->accidental));
        }
    }
}

// -----------------------------------------------------------------------------
// Emit pitch, duration, voice, type, dot for a note/rest
// -----------------------------------------------------------------------------

void emit_note_pitch(
    pugi::xml_node note_node,
    const Sunny::Core::SpelledPitch& pitch
) {
    auto pitch_node = note_node.append_child("pitch");
    std::string step_str(1, STEP_CHARS[pitch.letter < 7 ? pitch.letter : 0]);
    pitch_node.append_child("step").text().set(step_str.c_str());
    if (pitch.accidental != 0) {
        pitch_node.append_child("alter").text().set(
            static_cast<int>(pitch.accidental));
    }
    pitch_node.append_child("octave").text().set(
        static_cast<int>(pitch.octave));
}

void emit_note_duration_and_type(
    pugi::xml_node note_node,
    Sunny::Core::Beat duration,
    int voice_number,
    int divisions
) {
    int dur_units = beat_to_mxml_duration(duration, divisions);
    note_node.append_child("duration").text().set(dur_units);
    note_node.append_child("voice").text().set(voice_number);

    std::string type_name = duration_type_name(duration);
    note_node.append_child("type").text().set(type_name.c_str());

    if (is_dotted(duration)) {
        note_node.append_child("dot");
    }
}

// -----------------------------------------------------------------------------
// Emit notations (articulations, ornaments, technical, ties, slurs, tuplets)
// -----------------------------------------------------------------------------

void emit_note_notations(
    pugi::xml_node note_node,
    const Sunny::Core::Note& note,
    const Sunny::Core::NoteGroup& ng,
    bool is_first_note,
    Sunny::Core::CompilationReport& report,
    std::optional<Sunny::Core::ScoreTime> location,
    std::optional<Sunny::Core::PartId> part_id
) {
    // Determine whether we need a <notations> element at all
    bool need_notations = false;

    if (note.tie_forward) need_notations = true;
    if (is_first_note && (ng.slur_start || ng.slur_end)) need_notations = true;
    if (is_first_note && ng.tuplet_context) need_notations = true;
    if (note.articulation) {
        auto* entry = find_articulation_entry(*note.articulation);
        if (entry) need_notations = true;
    }

    if (!need_notations) return;

    auto notations = note_node.append_child("notations");

    // Tied notation
    if (note.tie_forward) {
        auto tied = notations.append_child("tied");
        tied.append_attribute("type") = "start";
    }

    // Slur
    if (is_first_note && ng.slur_start) {
        auto slur = notations.append_child("slur");
        slur.append_attribute("type") = "start";
    }
    if (is_first_note && ng.slur_end) {
        auto slur = notations.append_child("slur");
        slur.append_attribute("type") = "stop";
    }

    // Tuplet
    if (is_first_note && ng.tuplet_context) {
        auto tuplet = notations.append_child("tuplet");
        tuplet.append_attribute("type") = "start";
    }

    // Articulations by category
    if (note.articulation) {
        auto* entry = find_articulation_entry(*note.articulation);
        if (entry) {
            switch (entry->category) {
                case ArticulationCategory::Articulations: {
                    auto arts = notations.append_child("articulations");
                    arts.append_child(entry->element_name);
                    break;
                }
                case ArticulationCategory::Ornaments: {
                    auto orns = notations.append_child("ornaments");
                    orns.append_child(entry->element_name);
                    break;
                }
                case ArticulationCategory::Technical: {
                    auto tech = notations.append_child("technical");
                    tech.append_child(entry->element_name);
                    break;
                }
                case ArticulationCategory::Notations: {
                    // Fermata goes directly under <notations>
                    notations.append_child(entry->element_name);
                    break;
                }
            }
        }

        if (is_unsupported_articulation(*note.articulation)) {
            std::string art_name;
            switch (*note.articulation) {
                case Sunny::Core::ArticulationType::InvertedMordent: art_name = "InvertedMordent"; break;
                case Sunny::Core::ArticulationType::InvertedTurn:    art_name = "InvertedTurn"; break;
                case Sunny::Core::ArticulationType::BendUp:          art_name = "BendUp"; break;
                case Sunny::Core::ArticulationType::BendDown:        art_name = "BendDown"; break;
                case Sunny::Core::ArticulationType::GlissandoStart:  art_name = "GlissandoStart"; break;
                case Sunny::Core::ArticulationType::GlissandoEnd:    art_name = "GlissandoEnd"; break;
                case Sunny::Core::ArticulationType::OpenString:      art_name = "OpenString"; break;
                case Sunny::Core::ArticulationType::Stopped:         art_name = "Stopped"; break;
                case Sunny::Core::ArticulationType::Muted:           art_name = "Muted"; break;
                default: art_name = "Unknown"; break;
            }
            report.diagnostics.push_back({
                "Unsupported articulation in MusicXML export: " + art_name,
                location,
                part_id
            });
        }
    }
}

// -----------------------------------------------------------------------------
// Compute total MusicXML duration of all events in a voice for <backup>
// -----------------------------------------------------------------------------

int compute_voice_forward_duration(
    const Sunny::Core::Voice& voice,
    int divisions
) {
    int total = 0;
    for (const auto& event : voice.events) {
        auto dur = event.duration();
        if (dur != Sunny::Core::Beat::zero()) {
            total += beat_to_mxml_duration(dur, divisions);
        }
    }
    return total;
}

}  // anonymous namespace

// =============================================================================
// compile_score_to_musicxml
// =============================================================================

Sunny::Core::Result<MusicXmlCompilationResult>
compile_score_to_musicxml(const Sunny::Core::Score& score) {
    if (!Sunny::Core::is_compilable(score)) {
        return std::unexpected(Sunny::Core::ErrorCode::InvariantViolation);
    }

    Sunny::Core::CompilationReport report;

    // Compute divisions from all event durations across the entire score
    auto all_durations = collect_all_durations(score);
    int divisions = 1;
    if (!all_durations.empty()) {
        divisions = compute_divisions_from(all_durations.begin(), all_durations.end());
    }
    if (divisions < 1) divisions = 1;

    // Build the pugixml DOM
    pugi::xml_document doc;

    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto score_node = doc.append_child("score-partwise");
    score_node.append_attribute("version") = "4.0";

    // Work title
    if (!score.metadata.title.empty()) {
        auto work = score_node.append_child("work");
        work.append_child("work-title").text().set(score.metadata.title.c_str());
    }

    // Part list
    auto part_list = score_node.append_child("part-list");
    for (std::size_t pi = 0; pi < score.parts.size(); ++pi) {
        const auto& part = score.parts[pi];
        auto sp = part_list.append_child("score-part");
        std::string part_id_str = "P" + std::to_string(pi + 1);
        sp.append_attribute("id") = part_id_str.c_str();
        sp.append_child("part-name").text().set(part.definition.name.c_str());
    }

    // Track previous key/time per part to detect changes
    // (Global maps apply to all parts, but we track per-part for local overrides.)

    for (std::size_t pi = 0; pi < score.parts.size(); ++pi) {
        const auto& part = score.parts[pi];
        std::string part_id_str = "P" + std::to_string(pi + 1);

        auto part_node = score_node.append_child("part");
        part_node.append_attribute("id") = part_id_str.c_str();

        // Track what key/time was last emitted to detect changes
        // (Reserved for future incremental attribute emission.)

        for (const auto& measure : part.measures) {
            auto m = part_node.append_child("measure");
            m.append_attribute("number") = static_cast<int>(measure.bar_number);

            std::uint32_t bar = measure.bar_number;

            // Determine active key and time for this bar
            const Sunny::Core::KeySignature* active_key = nullptr;
            const Sunny::Core::TimeSignature* active_time = nullptr;
            const Sunny::Core::Clef* active_clef = nullptr;
            int transposition = 0;

            // Check for local overrides first, then global maps
            if (measure.local_key) {
                active_key = &*measure.local_key;
            } else {
                auto* ke = key_at_bar(score.key_map, bar);
                if (ke) active_key = &ke->key;
            }

            if (measure.local_time) {
                active_time = &*measure.local_time;
            } else {
                auto* te = time_at_bar(score.time_map, bar);
                if (te) active_time = &te->time_signature;
            }

            // Determine whether attributes are needed
            bool is_first_bar = (bar == 1);
            bool key_changed = false;
            bool time_changed = false;

            if (is_first_bar) {
                key_changed = true;
                time_changed = true;
                active_clef = &part.definition.clef;
                transposition = static_cast<int>(part.definition.transposition);
            } else {
                // Check if global key changes at this bar
                if (measure.local_key) {
                    key_changed = true;
                } else if (key_changes_at_bar(score.key_map, bar)) {
                    key_changed = true;
                }

                // Check if global time changes at this bar
                if (measure.local_time) {
                    time_changed = true;
                } else if (time_changes_at_bar(score.time_map, bar)) {
                    time_changed = true;
                }
            }

            if (is_first_bar || key_changed || time_changed) {
                emit_attributes(
                    m,
                    divisions,
                    key_changed ? active_key : nullptr,
                    time_changed ? active_time : nullptr,
                    is_first_bar ? active_clef : nullptr,
                    is_first_bar ? transposition : 0
                );
            }

            // Emit tempo events at this bar's downbeat
            for (const auto& te : score.tempo_map) {
                if (te.position.bar == bar &&
                    te.position.beat == Sunny::Core::Beat::zero()) {
                    double bpm = Sunny::Core::effective_quarter_bpm(te);
                    emit_tempo_direction(m, bpm);
                }
            }

            // Emit rehearsal marks at this bar
            for (const auto& rm : score.rehearsal_marks) {
                if (rm.position.bar == bar &&
                    rm.position.beat == Sunny::Core::Beat::zero()) {
                    emit_rehearsal_direction(m, rm.label);
                }
            }

            // Emit hairpin starts/stops at this bar's downbeat
            for (const auto& hp : part.hairpins) {
                if (hp.start.bar == bar &&
                    hp.start.beat == Sunny::Core::Beat::zero()) {
                    emit_hairpin_start(m, hp.type);
                }
                if (hp.end.bar == bar &&
                    hp.end.beat == Sunny::Core::Beat::zero()) {
                    emit_hairpin_stop(m);
                }
            }

            // Emit events voice by voice
            for (std::size_t vi = 0; vi < measure.voices.size(); ++vi) {
                const auto& voice = measure.voices[vi];
                int voice_number = static_cast<int>(voice.voice_index) + 1;

                // If not the first voice, emit <backup> to rewind
                if (vi > 0) {
                    int backup_dur = compute_voice_forward_duration(
                        measure.voices[vi - 1], divisions);
                    if (backup_dur > 0) {
                        auto backup = m.append_child("backup");
                        backup.append_child("duration").text().set(backup_dur);
                    }
                }

                for (const auto& event : voice.events) {
                    Sunny::Core::ScoreTime event_location{bar, event.offset};

                    // Handle each payload variant
                    if (auto* ng = std::get_if<Sunny::Core::NoteGroup>(&event.payload)) {
                        // Emit time-modification for tuplets (before notes)
                        bool has_tuplet = ng->tuplet_context.has_value();

                        // Emit dynamics direction before the note if any note has a dynamic
                        for (const auto& note : ng->notes) {
                            if (note.dynamic) {
                                emit_dynamic_direction(m, *note.dynamic, voice_number);
                                break;  // one dynamic per NoteGroup
                            }
                        }

                        // Emit hairpin starts/stops at non-downbeat positions within the measure
                        for (const auto& hp : part.hairpins) {
                            if (hp.start.bar == bar && hp.start.beat == event.offset &&
                                hp.start.beat != Sunny::Core::Beat::zero()) {
                                emit_hairpin_start(m, hp.type);
                            }
                            if (hp.end.bar == bar && hp.end.beat == event.offset &&
                                hp.end.beat != Sunny::Core::Beat::zero()) {
                                emit_hairpin_stop(m);
                            }
                        }

                        for (std::size_t ni = 0; ni < ng->notes.size(); ++ni) {
                            const auto& note = ng->notes[ni];
                            bool is_first_in_chord = (ni == 0);

                            auto n = m.append_child("note");

                            // Grace note
                            if (note.grace) {
                                n.append_child("grace");
                            }

                            // Chord flag for subsequent notes in the group
                            if (!is_first_in_chord) {
                                n.append_child("chord");
                            }

                            // Pitch
                            emit_note_pitch(n, note.pitch);

                            // Duration (omitted for grace notes)
                            if (!note.grace) {
                                emit_note_duration_and_type(
                                    n, ng->duration, voice_number, divisions);
                            } else {
                                // Grace notes still need voice and type
                                n.append_child("voice").text().set(voice_number);
                                std::string type_name = duration_type_name(ng->duration);
                                n.append_child("type").text().set(type_name.c_str());
                            }

                            // Tie element (separate from <tied> in notations)
                            if (note.tie_forward) {
                                auto tie = n.append_child("tie");
                                tie.append_attribute("type") = "start";
                            }

                            // Time modification for tuplets
                            if (has_tuplet) {
                                auto tm = n.append_child("time-modification");
                                tm.append_child("actual-notes").text().set(
                                    static_cast<int>(ng->tuplet_context->actual));
                                tm.append_child("normal-notes").text().set(
                                    static_cast<int>(ng->tuplet_context->normal));
                            }

                            // Notations (articulations, ornaments, technical, ties, slurs, tuplets)
                            emit_note_notations(
                                n, note, *ng, is_first_in_chord,
                                report, event_location, part.id);
                        }

                    } else if (auto* rest = std::get_if<Sunny::Core::RestEvent>(&event.payload)) {
                        auto n = m.append_child("note");
                        n.append_child("rest");
                        emit_note_duration_and_type(
                            n, rest->duration, voice_number, divisions);

                    } else if (auto* cs = std::get_if<Sunny::Core::ChordSymbolEvent>(&event.payload)) {
                        emit_chord_symbol(m, *cs);

                    } else if (auto* sd = std::get_if<Sunny::Core::ScoreDirection>(&event.payload)) {
                        emit_score_direction(m, *sd, report, event_location, part.id);
                    }
                }
            }
        }
    }

    // Serialise the DOM to string
    std::ostringstream oss;
    doc.save(oss, "  ");

    return MusicXmlCompilationResult{oss.str(), std::move(report)};
}

}  // namespace Sunny::Infrastructure::Format
