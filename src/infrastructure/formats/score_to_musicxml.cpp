/**
 * @file score_to_musicxml.cpp
 * @brief MusicXML compilation from Score IR — implementation
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <pugixml.hpp>
#include <sstream>
#include <string_view>
#include <sunny/core/score/document.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/tuplets.hpp>
#include <sunny/core/score/types.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/written_duration.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>
#include <sunny/infrastructure/formats/musicxml_internal.hpp>
#include <sunny/infrastructure/formats/score_to_musicxml.hpp>

namespace sunny::infrastructure::formats {

using namespace MxmlInternal;

namespace {

// -----------------------------------------------------------------------------
// Clef mapping
// -----------------------------------------------------------------------------

struct ClefMapping {
    const char* sign;
    int line;
};

ClefMapping clef_to_mxml(sunny::core::Clef clef) {
    switch (clef) {
    case sunny::core::Clef::Treble:
        return {"G", 2};
    case sunny::core::Clef::Bass:
        return {"F", 4};
    case sunny::core::Clef::Alto:
        return {"C", 3};
    case sunny::core::Clef::Tenor:
        return {"C", 4};
    case sunny::core::Clef::Percussion:
        return {"percussion", 0};
    case sunny::core::Clef::Tab:
        return {"TAB", 5};
    }
    return {"G", 2};
}

// -----------------------------------------------------------------------------
// Dynamic level to MusicXML element name
// -----------------------------------------------------------------------------

const char* dynamic_to_mxml(sunny::core::DynamicLevel level) {
    switch (level) {
    case sunny::core::DynamicLevel::pppp:
        return "pppp";
    case sunny::core::DynamicLevel::ppp:
        return "ppp";
    case sunny::core::DynamicLevel::pp:
        return "pp";
    case sunny::core::DynamicLevel::p:
        return "p";
    case sunny::core::DynamicLevel::mp:
        return "mp";
    case sunny::core::DynamicLevel::mf:
        return "mf";
    case sunny::core::DynamicLevel::f:
        return "f";
    case sunny::core::DynamicLevel::ff:
        return "ff";
    case sunny::core::DynamicLevel::fff:
        return "fff";
    case sunny::core::DynamicLevel::ffff:
        return "ffff";
    case sunny::core::DynamicLevel::sfz:
        return "sfz";
    case sunny::core::DynamicLevel::fp:
        return "fp";
    case sunny::core::DynamicLevel::sfp:
        return "sfp";
    case sunny::core::DynamicLevel::rfz:
        return "rfz";
    }
    return "mf";
}

std::optional<sunny::core::DynamicLevel> semantic_dynamic(const sunny::core::Note& note) {
    if (note.dynamic) return note.dynamic;
    return note.velocity.written;
}

// -----------------------------------------------------------------------------
// Articulation classification
// -----------------------------------------------------------------------------

enum class ArticulationCategory {
    Articulations,
    Ornaments,
    Technical,
    Dynamics,
    Notations,
    GlissandoStart,
    GlissandoStop,
    OtherTechnical
};

struct ArticulationMxmlEntry {
    const char* element_name;
    sunny::core::ArticulationType type;
    ArticulationCategory category;
};

constexpr ArticulationMxmlEntry ARTICULATION_MAP[] = {
    {"staccato", sunny::core::ArticulationType::Staccato, ArticulationCategory::Articulations},
    {"staccatissimo",
     sunny::core::ArticulationType::Staccatissimo,
     ArticulationCategory::Articulations},
    {"accent", sunny::core::ArticulationType::Accent, ArticulationCategory::Articulations},
    {"strong-accent", sunny::core::ArticulationType::Marcato, ArticulationCategory::Articulations},
    {"tenuto", sunny::core::ArticulationType::Tenuto, ArticulationCategory::Articulations},
    {"detached-legato",
     sunny::core::ArticulationType::Portato,
     ArticulationCategory::Articulations},
    {"sfz", sunny::core::ArticulationType::Sforzando, ArticulationCategory::Dynamics},
    {"fp", sunny::core::ArticulationType::ForzandoPiano, ArticulationCategory::Dynamics},
    {"fermata", sunny::core::ArticulationType::Fermata, ArticulationCategory::Notations},
    {"trill-mark", sunny::core::ArticulationType::Trill, ArticulationCategory::Ornaments},
    {"mordent", sunny::core::ArticulationType::Mordent, ArticulationCategory::Ornaments},
    {"inverted-mordent",
     sunny::core::ArticulationType::InvertedMordent,
     ArticulationCategory::Ornaments},
    {"turn", sunny::core::ArticulationType::Turn, ArticulationCategory::Ornaments},
    {"inverted-turn", sunny::core::ArticulationType::InvertedTurn, ArticulationCategory::Ornaments},
    {"tremolo", sunny::core::ArticulationType::Tremolo, ArticulationCategory::Ornaments},
    {"snap-pizzicato",
     sunny::core::ArticulationType::SnapPizzicato,
     ArticulationCategory::Technical},
    {"harmonic", sunny::core::ArticulationType::Harmonic, ArticulationCategory::Technical},
    {"up-bow", sunny::core::ArticulationType::UpBow, ArticulationCategory::Technical},
    {"down-bow", sunny::core::ArticulationType::DownBow, ArticulationCategory::Technical},
    {"open-string", sunny::core::ArticulationType::OpenString, ArticulationCategory::Technical},
    {"stopped", sunny::core::ArticulationType::Stopped, ArticulationCategory::Technical},
    {"glissando",
     sunny::core::ArticulationType::GlissandoStart,
     ArticulationCategory::GlissandoStart},
    {"glissando", sunny::core::ArticulationType::GlissandoEnd, ArticulationCategory::GlissandoStop},
    {"muted", sunny::core::ArticulationType::Muted, ArticulationCategory::OtherTechnical},
    {"bend-up", sunny::core::ArticulationType::BendUp, ArticulationCategory::OtherTechnical},
    {"bend-down", sunny::core::ArticulationType::BendDown, ArticulationCategory::OtherTechnical},
};

consteval bool articulation_map_is_total_and_unique() {
    constexpr auto count = static_cast<std::size_t>(sunny::core::ArticulationType::BendDown) + 1;
    bool seen[count]{};
    for (const auto& entry : ARTICULATION_MAP) {
        const auto index = static_cast<std::size_t>(entry.type);
        if (index >= count || seen[index]) return false;
        seen[index] = true;
    }
    for (bool present : seen) {
        if (!present) return false;
    }
    return true;
}

static_assert(articulation_map_is_total_and_unique(),
              "Every ArticulationType must have exactly one MusicXML disposition");

const ArticulationMxmlEntry* find_articulation_entry(sunny::core::ArticulationType art) {
    for (const auto& entry : ARTICULATION_MAP) {
        if (entry.type == art) return &entry;
    }
    return nullptr;
}

bool ornament_supersedes_articulation(const sunny::core::Ornament& ornament,
                                      sunny::core::ArticulationType articulation) {
    using sunny::core::ArticulationType;
    using sunny::core::OrnamentType;
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
// Collect every Beat value projected into MusicXML integer timing. Divisions
// must cover point offsets and measure endpoints as well as note durations;
// otherwise a valid rational point can be silently truncated even when all
// measured events happen to use simpler denominators.
// -----------------------------------------------------------------------------

std::vector<sunny::core::Beat> collect_all_musicxml_timing_values(const sunny::core::Score& score) {
    std::vector<sunny::core::Beat> values;
    for (const auto& entry : score.time_map) {
        values.push_back(entry.time_signature.measure_duration());
    }
    for (const auto& entry : score.tempo_map) {
        values.push_back(entry.position.beat);
    }
    for (const auto& entry : score.key_map) {
        values.push_back(entry.position.beat);
    }
    for (const auto& mark : score.rehearsal_marks) {
        values.push_back(mark.position.beat);
    }
    for (const auto& part : score.parts) {
        for (const auto& hairpin : part.hairpins) {
            values.push_back(hairpin.start.beat);
            values.push_back(hairpin.end.beat);
        }
        for (const auto& directive : part.part_directives) {
            values.push_back(directive.start.beat);
            values.push_back(directive.end.beat);
        }
        for (const auto& measure : part.measures) {
            if (measure.local_time) values.push_back(measure.local_time->measure_duration());
            for (const auto& voice : measure.voices) {
                const auto contexts = sunny::core::collect_tuplet_contexts(voice);
                for (const auto& event : voice.events) {
                    values.push_back(event.offset);
                    auto dur = event.duration();
                    if (dur != sunny::core::Beat::zero()) {
                        values.push_back(dur);
                    }
                    if (!event.is_note_group() && !event.is_rest()) continue;
                    const auto chain = sunny::core::tuplet_context_chain(
                        sunny::core::event_tuplet_context(event), contexts);
                    if (!chain) continue;
                    const auto ratio = sunny::core::cumulative_tuplet_written_ratio(*chain);
                    if (!ratio) continue;
                    if (const auto written = sunny::core::project_written_duration(dur, *ratio))
                        for (const auto& piece : written->pieces)
                            values.push_back(piece.sounding);
                }
            }
        }
    }
    return values;
}

// -----------------------------------------------------------------------------
// Lookup helpers for global maps
// -----------------------------------------------------------------------------

/// Find the key signature active at the downbeat of a measure. A later key
/// entry in the same measure must not be pulled backwards to the downbeat.
const sunny::core::KeySignatureEntry*
key_at_measure_start(const sunny::core::KeySignatureMap& key_map, std::uint32_t bar) {
    const sunny::core::KeySignatureEntry* active = nullptr;
    for (const auto& entry : key_map) {
        if (entry.position.bar < bar ||
            (entry.position.bar == bar && entry.position.beat == sunny::core::Beat::zero())) {
            active = &entry;
        } else {
            break;
        }
    }
    return active;
}

/// Find the time signature active at a given bar number.
const sunny::core::TimeSignatureEntry* time_at_bar(const sunny::core::TimeSignatureMap& time_map,
                                                   std::uint32_t bar) {
    const sunny::core::TimeSignatureEntry* active = nullptr;
    for (const auto& entry : time_map) {
        if (entry.bar <= bar) {
            active = &entry;
        } else {
            break;
        }
    }
    return active;
}

std::string_view musicxml_mode(const sunny::core::KeySignature& key) {
    const std::string_view mode = key.mode.name;
    if (mode == "major" || mode == "minor" || mode == "dorian" || mode == "phrygian" ||
        mode == "lydian" || mode == "mixolydian" || mode == "aeolian" || mode == "ionian" ||
        mode == "locrian")
        return mode;
    return "none";
}

bool same_key_signature_state(const sunny::core::KeySignature& lhs,
                              const sunny::core::KeySignature& rhs) {
    return lhs == rhs;
}

bool same_time_signature_state(const sunny::core::TimeSignature& lhs,
                               const sunny::core::TimeSignature& rhs) {
    return lhs == rhs;
}

// -----------------------------------------------------------------------------
// Emit <attributes> element
// -----------------------------------------------------------------------------

void emit_attributes(pugi::xml_node measure_node,
                     int divisions,
                     const sunny::core::KeySignature* key,
                     const sunny::core::TimeSignature* time_sig,
                     const sunny::core::Clef* clef,
                     std::uint8_t staff_count,
                     const std::vector<sunny::core::Clef>* staff_clefs,
                     std::optional<sunny::core::DiatonicInterval> transposition,
                     sunny::core::CompilationReport& report,
                     std::optional<sunny::core::ScoreTime> location,
                     std::optional<sunny::core::PartId> part_id) {
    auto attrs = measure_node.append_child("attributes");
    attrs.append_child("divisions").text().set(divisions);

    if (key) {
        auto key_node = attrs.append_child("key");
        key_node.append_child("fifths").text().set(key->accidentals);
        const auto mode = musicxml_mode(*key);
        key_node.append_child("mode").text().set(mode.data());
        if (mode == "none") {
            const auto scale_label = key->mode.name.empty()
                                         ? std::string{"an anonymous Sunny scale"}
                                         : "Sunny scale '" + key->mode.name + "'";
            report.diagnostics.push_back(
                {"MusicXML 4 traditional key mode cannot encode " + scale_label +
                     "; retained the exact fifths count and emitted mode none; the "
                     "non-traditional key form requires altered-step data absent from Score IR",
                 location,
                 part_id});
        }
    }

    if (time_sig) {
        auto time_node = attrs.append_child("time");
        std::string beats;
        const auto& groups = time_sig->groups();
        const bool additive =
            groups.size() > 1 && !std::all_of(groups.begin() + 1, groups.end(), [&](int group) {
                return group == groups.front();
            });
        if (additive) {
            for (std::size_t i = 0; i < groups.size(); ++i) {
                if (i > 0) beats += "+";
                beats += std::to_string(groups[i]);
            }
        }
        if (beats.empty()) beats = std::to_string(time_sig->numerator());
        time_node.append_child("beats").text().set(beats.c_str());
        time_node.append_child("beat-type").text().set(time_sig->denominator());
    }

    if (staff_count > 1) attrs.append_child("staves").text().set(staff_count);

    if (clef) {
        const auto emit_clef = [&](sunny::core::Clef value, std::uint8_t staff_index) {
            auto clef_node = attrs.append_child("clef");
            if (staff_count > 1)
                clef_node.append_attribute("number") = static_cast<int>(staff_index) + 1;
            auto mapping = clef_to_mxml(value);
            clef_node.append_child("sign").text().set(mapping.sign);
            if (mapping.line > 0) clef_node.append_child("line").text().set(mapping.line);
        };
        for (std::uint16_t staff = 0; staff < std::max<std::uint8_t>(staff_count, 1); ++staff) {
            const auto staff_index = static_cast<std::uint8_t>(staff);
            const auto staff_clef = staff_clefs && staff_clefs->size() == staff_count
                                        ? (*staff_clefs)[staff_index]
                                        : *clef;
            emit_clef(staff_clef, staff_index);
        }
    }

    // MusicXML 4.0 adds <transpose> to the written <pitch> to obtain sounding
    // pitch; <diatonic> keeps the letter-name step so readers respell exactly.
    if (transposition && transposition->chromatic != 0) {
        auto trans_node = attrs.append_child("transpose");
        trans_node.append_child("diatonic").text().set(transposition->diatonic);
        trans_node.append_child("chromatic").text().set(transposition->chromatic);
    }
}

/// Written form of a concert key for a part whose written pitch is the
/// concert pitch moved by `concert_to_written`: the tonic moves by the same
/// interval and the signature by the interval's line-of-fifths displacement
/// 7c - 12d (a major second up adds two sharps).
sunny::core::KeySignature written_key(const sunny::core::KeySignature& concert,
                                      sunny::core::DiatonicInterval concert_to_written) {
    if (concert_to_written.chromatic == 0 && concert_to_written.diatonic == 0) return concert;
    sunny::core::KeySignature written = concert;
    written.root = sunny::core::apply_interval(concert.root, concert_to_written);
    written.accidentals = static_cast<std::int8_t>(
        concert.accidentals + 7 * concert_to_written.chromatic - 12 * concert_to_written.diatonic);
    return written;
}

// -----------------------------------------------------------------------------
// Emit <direction> for dynamics
// -----------------------------------------------------------------------------

void emit_dynamic_direction(pugi::xml_node parent,
                            sunny::core::DynamicLevel level,
                            int voice_number,
                            int offset_units = 0,
                            int staff_number = 0) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto dynamics_node = dir_type.append_child("dynamics");
    dynamics_node.append_child(dynamic_to_mxml(level));
    if (offset_units != 0) {
        auto offset = dir.append_child("offset");
        offset.text().set(offset_units);
        offset.append_attribute("sound") = "yes";
    }
    dir.append_child("voice").text().set(voice_number);
    if (staff_number > 0) dir.append_child("staff").text().set(staff_number);
}

void emit_direction_offset(pugi::xml_node direction, int offset_units, bool affects_sound = true) {
    if (offset_units == 0) return;
    auto offset = direction.append_child("offset");
    offset.text().set(offset_units);
    if (affects_sound) offset.append_attribute("sound") = "yes";
}

// -----------------------------------------------------------------------------
// Emit <direction> for tempo
// -----------------------------------------------------------------------------

struct MxmlBeatUnit {
    const char* name;
    bool dotted;
};

MxmlBeatUnit tempo_beat_unit(sunny::core::BeatUnit unit) {
    using sunny::core::BeatUnit;
    switch (unit) {
    case BeatUnit::Whole:
        return {"whole", false};
    case BeatUnit::Half:
        return {"half", false};
    case BeatUnit::DottedHalf:
        return {"half", true};
    case BeatUnit::Quarter:
        return {"quarter", false};
    case BeatUnit::DottedQuarter:
        return {"quarter", true};
    case BeatUnit::Eighth:
        return {"eighth", false};
    case BeatUnit::DottedEighth:
        return {"eighth", true};
    case BeatUnit::Sixteenth:
        return {"16th", false};
    }
    return {"quarter", false};
}

void append_metronome_unit(pugi::xml_node metronome, sunny::core::BeatUnit unit) {
    const auto mapped = tempo_beat_unit(unit);
    metronome.append_child("beat-unit").text().set(mapped.name);
    if (mapped.dotted) metronome.append_child("beat-unit-dot");
}

std::string exact_tempo_rate(const sunny::core::PositiveRational& rate) {
    if (rate.denominator() == 1) return std::to_string(rate.numerator());
    return std::to_string(rate.numerator()) + "/" + std::to_string(rate.denominator());
}

void emit_tempo_direction(pugi::xml_node parent,
                          const sunny::core::TempoEvent& event,
                          double quarter_bpm,
                          int offset_units) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";

    if (event.transition_type == sunny::core::TempoTransitionType::MetricModulation) {
        auto relation_type = dir.append_child("direction-type");
        auto relation = relation_type.append_child("metronome");
        append_metronome_unit(relation, event.old_unit);
        relation.append_child("metronome-relation").text().set("equals");
        append_metronome_unit(relation, event.new_unit);
    }

    auto dir_type = dir.append_child("direction-type");
    auto metronome = dir_type.append_child("metronome");
    append_metronome_unit(metronome, event.beat_unit);
    metronome.append_child("per-minute").text().set(exact_tempo_rate(event.bpm).c_str());
    emit_direction_offset(dir, offset_units);
    auto sound = dir.append_child("sound");
    sound.append_attribute("tempo") = quarter_bpm;
}

void emit_linear_tempo_start(pugi::xml_node parent,
                             int offset_units,
                             sunny::core::CompilationReport& report,
                             sunny::core::ScoreTime start,
                             sunny::core::PartId part_id,
                             bool record_loss) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";
    auto dir_type = dir.append_child("direction-type");
    dir_type.append_child("words").text().set("linear tempo transition");
    emit_direction_offset(dir, offset_units, false);
    if (record_loss)
        report.diagnostics.push_back(
            {"MusicXML preserves a Sunny Linear tempo transition as a start label and structured "
             "endpoint metronome mark; standard MusicXML sound tempo is point-valued and does "
             "not encode the continuous curve",
             start,
             part_id});
}

// -----------------------------------------------------------------------------
// Emit <direction> for rehearsal marks
// -----------------------------------------------------------------------------

void emit_rehearsal_direction(pugi::xml_node parent, const std::string& label, int offset_units) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";
    auto dir_type = dir.append_child("direction-type");
    dir_type.append_child("rehearsal").text().set(label.c_str());
    emit_direction_offset(dir, offset_units, false);
}

// -----------------------------------------------------------------------------
// Emit <direction> for hairpin wedge start/stop
// -----------------------------------------------------------------------------

void emit_hairpin_start(pugi::xml_node parent, sunny::core::HairpinType type, int offset_units) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto wedge = dir_type.append_child("wedge");
    wedge.append_attribute("type") =
        (type == sunny::core::HairpinType::Crescendo) ? "crescendo" : "diminuendo";
    emit_direction_offset(dir, offset_units);
}

void emit_hairpin_stop(pugi::xml_node parent, int offset_units) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "below";
    auto dir_type = dir.append_child("direction-type");
    auto wedge = dir_type.append_child("wedge");
    wedge.append_attribute("type") = "stop";
    emit_direction_offset(dir, offset_units);
}

std::string directive_label(const sunny::core::PartDirective& directive) {
    std::string label{sunny::core::directive_name(directive.directive)};
    if (directive.directive == sunny::core::DirectiveType::Divisi)
        label += " a " + std::to_string(directive.divisi_count);
    return label;
}

void emit_part_directive(pugi::xml_node parent, const std::string& text, int offset_units) {
    auto dir = parent.append_child("direction");
    dir.append_attribute("placement") = "above";
    auto dir_type = dir.append_child("direction-type");
    dir_type.append_child("words").text().set(text.c_str());
    emit_direction_offset(dir, offset_units, false);
}

// -----------------------------------------------------------------------------
// Emit a ScoreDirection event as <direction> element
// -----------------------------------------------------------------------------

void emit_score_direction(pugi::xml_node parent,
                          const sunny::core::ScoreDirection& sd,
                          int offset_units,
                          int voice_number,
                          int staff_number,
                          sunny::core::CompilationReport& report,
                          std::optional<sunny::core::ScoreTime> location,
                          std::optional<sunny::core::PartId> part_id) {
    using DT = sunny::core::DirectionType;
    pugi::xml_node dir;

    switch (sd.type) {
    case DT::TempoText: {
        dir = parent.append_child("direction");
        dir.append_attribute("placement") = "above";
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("words").text().set(sd.text.value_or("").c_str());
        break;
    }
    case DT::Text: {
        dir = parent.append_child("direction");
        dir.append_attribute("placement") = "above";
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("words").text().set(sd.text.value_or("").c_str());
        break;
    }
    case DT::PedalDown: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        auto pedal = dir_type.append_child("pedal");
        pedal.append_attribute("type") = "start";
        break;
    }
    case DT::PedalUp: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        auto pedal = dir_type.append_child("pedal");
        pedal.append_attribute("type") = "stop";
        break;
    }
    case DT::OttavaStart: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        const int semitones = std::abs(static_cast<int>(sd.ottava_shift));
        if ((semitones == 12 || semitones == 24) && sd.ottava_shift != 0) {
            auto shift = dir_type.append_child("octave-shift");
            shift.append_attribute("type") = sd.ottava_shift > 0 ? "down" : "up";
            shift.append_attribute("size") = semitones == 12 ? 8 : 15;
        } else {
            dir_type.append_child("other-direction")
                .text()
                .set(("ottava " + std::to_string(static_cast<int>(sd.ottava_shift)) + " semitones")
                         .c_str());
            report.diagnostics.push_back(
                {"MusicXML octave-shift supports Sunny's documented ±12/±24-semitone "
                 "OttavaStart values; preserved nonstandard shift as other-direction",
                 location,
                 part_id});
        }
        break;
    }
    case DT::OttavaEnd: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        auto shift = dir_type.append_child("octave-shift");
        shift.append_attribute("type") = "stop";
        break;
    }
    case DT::BreathMark: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("other-direction").text().set("breath-mark");
        break;
    }
    case DT::Caesura: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("other-direction").text().set("caesura");
        break;
    }
    case DT::Coda: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("coda");
        break;
    }
    case DT::Segno: {
        dir = parent.append_child("direction");
        auto dir_type = dir.append_child("direction-type");
        dir_type.append_child("segno");
        break;
    }
    case DT::ClefChange:
    case DT::DoubleBarline:
    case DT::FinalBarline:
    case DT::RepeatStart:
    case DT::RepeatEnd:
        report.diagnostics.push_back(
            {"Internal MusicXML point-event routing failure", location, part_id});
        return;
    }

    emit_direction_offset(dir, offset_units);
    dir.append_child("voice").text().set(voice_number);
    if (staff_number > 0) dir.append_child("staff").text().set(staff_number);
}

// -----------------------------------------------------------------------------
// Emit a <harmony> element for ChordSymbolEvent
// -----------------------------------------------------------------------------

void emit_chord_symbol(pugi::xml_node parent,
                       const sunny::core::ChordSymbolEvent& cs,
                       int offset_units,
                       int staff_number,
                       sunny::core::CompilationReport& report,
                       std::optional<sunny::core::ScoreTime> location,
                       std::optional<sunny::core::PartId> part_id) {
    auto harmony = parent.append_child("harmony");

    if (cs.numeral) {
        auto numeral = harmony.append_child("numeral");
        auto numeral_root = numeral.append_child("numeral-root");
        numeral_root.text().set(static_cast<int>(cs.numeral->root));
        if (cs.roman) numeral_root.append_attribute("text") = cs.roman->c_str();
        if (cs.numeral->alteration != 0) {
            numeral.append_child("numeral-alter")
                .text()
                .set(static_cast<int>(cs.numeral->alteration));
        }
        auto numeral_key = numeral.append_child("numeral-key");
        numeral_key.append_child("numeral-fifths")
            .text()
            .set(static_cast<int>(cs.numeral->key.fifths));
        numeral_key.append_child("numeral-mode")
            .text()
            .set(chord_numeral_mode_to_musicxml(cs.numeral->key.mode));
    } else {
        auto root_node = harmony.append_child("root");
        std::string step_str(1, STEP_CHARS[cs.root.letter < 7 ? cs.root.letter : 0]);
        root_node.append_child("root-step").text().set(step_str.c_str());
        if (cs.root.accidental != 0) {
            root_node.append_child("root-alter").text().set(static_cast<int>(cs.root.accidental));
        }
    }

    auto kind = harmony.append_child("kind");
    const std::string_view musicxml_kind{quality_to_mxml_kind(cs.quality)};
    kind.text().set(musicxml_kind.data());
    if (!cs.numeral && (cs.roman || !cs.extensions.empty())) {
        std::string display = cs.roman.value_or(cs.quality);
        for (const auto& extension : cs.extensions) {
            if (!display.empty()) display += " ";
            display += extension;
        }
        kind.append_attribute("text") = display.c_str();
        report.diagnostics.push_back(
            {"Legacy ChordSymbol roman/extensions preserved as MusicXML kind display text; "
             "author numeral/degrees for structured MusicXML harmony semantics",
             location,
             part_id});
    } else if (cs.numeral) {
        std::string display = musicxml_kind == "other" ? cs.quality : std::string{};
        for (const auto& extension : cs.extensions) {
            if (!display.empty()) display += " ";
            display += extension;
        }
        if (!display.empty()) kind.append_attribute("text") = display.c_str();
        if (!cs.extensions.empty()) {
            report.diagnostics.push_back(
                {"ChordSymbol free-form extensions preserved as MusicXML kind display text; "
                 "author degrees for structured MusicXML harmony semantics",
                 location,
                 part_id});
        } else if (musicxml_kind != "other" && cs.roman) {
            // The display spelling already owns quality/inversion text.
            kind.append_attribute("text") = "";
        }
    } else if (musicxml_kind == "other") {
        kind.append_attribute("text") = cs.quality.c_str();
    }

    if (cs.inversion) {
        harmony.append_child("inversion").text().set(static_cast<unsigned>(*cs.inversion));
    }

    if (cs.bass) {
        auto bass_node = harmony.append_child("bass");
        std::string bass_step(1, STEP_CHARS[cs.bass->letter < 7 ? cs.bass->letter : 0]);
        bass_node.append_child("bass-step").text().set(bass_step.c_str());
        if (cs.bass->accidental != 0) {
            bass_node.append_child("bass-alter").text().set(static_cast<int>(cs.bass->accidental));
        }
    }
    for (const auto& source_degree : cs.degrees) {
        auto degree = harmony.append_child("degree");
        degree.append_child("degree-value").text().set(static_cast<unsigned>(source_degree.value));
        degree.append_child("degree-alter").text().set(static_cast<int>(source_degree.alteration));
        degree.append_child("degree-type")
            .text()
            .set(chord_degree_type_to_musicxml(source_degree.type));
    }
    if (offset_units != 0) harmony.append_child("offset").text().set(offset_units);
    if (staff_number > 0) harmony.append_child("staff").text().set(staff_number);
}

void emit_backup(pugi::xml_node parent, int duration_units) {
    if (duration_units <= 0) return;
    auto backup = parent.append_child("backup");
    backup.append_child("duration").text().set(duration_units);
}

void emit_forward(pugi::xml_node parent,
                  int duration_units,
                  int voice_number,
                  int staff_number = 0) {
    if (duration_units <= 0) return;
    auto forward = parent.append_child("forward");
    forward.append_child("duration").text().set(duration_units);
    forward.append_child("voice").text().set(voice_number);
    if (staff_number > 0) forward.append_child("staff").text().set(staff_number);
}

void emit_clef_change(pugi::xml_node parent, sunny::core::Clef clef, int staff_number) {
    auto attributes = parent.append_child("attributes");
    auto clef_node = attributes.append_child("clef");
    if (staff_number > 0) clef_node.append_attribute("number") = staff_number;
    const auto mapping = clef_to_mxml(clef);
    clef_node.append_child("sign").text().set(mapping.sign);
    if (mapping.line > 0) clef_node.append_child("line").text().set(mapping.line);
}

void emit_barline_direction(pugi::xml_node parent,
                            sunny::core::DirectionType type,
                            bool at_measure_start) {
    auto barline = parent.append_child("barline");
    barline.append_attribute("location") = at_measure_start ? "left" : "middle";
    switch (type) {
    case sunny::core::DirectionType::DoubleBarline:
        barline.append_child("bar-style").text().set("light-light");
        break;
    case sunny::core::DirectionType::FinalBarline:
        barline.append_child("bar-style").text().set("light-heavy");
        break;
    case sunny::core::DirectionType::RepeatStart: {
        barline.append_child("bar-style").text().set("heavy-light");
        auto repeat = barline.append_child("repeat");
        repeat.append_attribute("direction") = "forward";
        break;
    }
    case sunny::core::DirectionType::RepeatEnd: {
        barline.append_child("bar-style").text().set("light-heavy");
        auto repeat = barline.append_child("repeat");
        repeat.append_attribute("direction") = "backward";
        break;
    }
    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// Emit pitch, duration, voice, type, dot, and notehead metadata for a note/rest
// -----------------------------------------------------------------------------

void emit_note_pitch(pugi::xml_node note_node, const sunny::core::SpelledPitch& pitch) {
    auto pitch_node = note_node.append_child("pitch");
    std::string step_str(1, STEP_CHARS[pitch.letter < 7 ? pitch.letter : 0]);
    pitch_node.append_child("step").text().set(step_str.c_str());
    if (pitch.accidental != 0) {
        pitch_node.append_child("alter").text().set(static_cast<int>(pitch.accidental));
    }
    pitch_node.append_child("octave").text().set(static_cast<int>(pitch.octave));
}

void emit_note_duration(pugi::xml_node note_node, sunny::core::Beat duration, int divisions) {
    int dur_units = beat_to_mxml_duration(duration, divisions);
    note_node.append_child("duration").text().set(dur_units);
}

/// MusicXML <type> names indexed by 1 - exponent (breve .. 1024th).
constexpr std::array<std::string_view, 12> WRITTEN_TYPE_NAMES{"breve",
                                                              "whole",
                                                              "half",
                                                              "quarter",
                                                              "eighth",
                                                              "16th",
                                                              "32nd",
                                                              "64th",
                                                              "128th",
                                                              "256th",
                                                              "512th",
                                                              "1024th"};

void emit_written_value(pugi::xml_node note_node,
                        sunny::core::WrittenNoteValue value,
                        int voice_number,
                        bool cue_size) {
    note_node.append_child("voice").text().set(voice_number);
    auto type = note_node.append_child("type");
    type.text().set(WRITTEN_TYPE_NAMES[static_cast<std::size_t>(1 - value.exponent)].data());
    if (cue_size) type.append_attribute("size") = "cue";
    for (int dot = 0; dot < value.dots; ++dot)
        note_node.append_child("dot");
}

bool emit_note_voice_type_and_dot(pugi::xml_node note_node,
                                  sunny::core::Beat duration,
                                  int voice_number,
                                  bool cue_size) {
    note_node.append_child("voice").text().set(voice_number);

    if (const auto graphic = musicxml_graphic_duration(duration)) {
        auto type = note_node.append_child("type");
        type.text().set(graphic->type.data());
        if (cue_size) type.append_attribute("size") = "cue";
        for (int dot = 0; dot < graphic->dots; ++dot)
            note_node.append_child("dot");
        return true;
    }
    return false;
}

const char* notehead_to_mxml(sunny::core::NoteHeadType notehead) {
    switch (notehead) {
    case sunny::core::NoteHeadType::Normal:
        return "normal";
    case sunny::core::NoteHeadType::Diamond:
        return "diamond";
    case sunny::core::NoteHeadType::Cross:
        return "x";
    case sunny::core::NoteHeadType::Slash:
        return "slash";
    case sunny::core::NoteHeadType::Triangle:
        return "triangle";
    case sunny::core::NoteHeadType::CircleX:
        return "circle-x";
    case sunny::core::NoteHeadType::Square:
        return "square";
    case sunny::core::NoteHeadType::Cue:
        return nullptr;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Emit notations (articulations, ornaments, technical, ties, slurs, tuplets)
// -----------------------------------------------------------------------------

struct TupletBoundary {
    const sunny::core::TupletContext* context;
    int number;
};

void emit_tuplet_boundaries(pugi::xml_node notations,
                            const std::vector<TupletBoundary>& starts,
                            const std::vector<TupletBoundary>& stops) {
    for (const auto& boundary : starts) {
        auto tuplet = notations.append_child("tuplet");
        tuplet.append_attribute("type") = "start";
        tuplet.append_attribute("number") = boundary.number;

        const auto graphic = musicxml_graphic_duration(boundary.context->normal_type);
        auto actual = tuplet.append_child("tuplet-actual");
        actual.append_child("tuplet-number").text().set(static_cast<int>(boundary.context->actual));
        if (graphic) {
            actual.append_child("tuplet-type").text().set(graphic->type.data());
            for (int dot = 0; dot < graphic->dots; ++dot)
                actual.append_child("tuplet-dot");
        }
        auto normal = tuplet.append_child("tuplet-normal");
        normal.append_child("tuplet-number").text().set(static_cast<int>(boundary.context->normal));
        if (graphic) {
            normal.append_child("tuplet-type").text().set(graphic->type.data());
            for (int dot = 0; dot < graphic->dots; ++dot)
                normal.append_child("tuplet-dot");
        }
    }
    for (const auto& boundary : stops) {
        auto tuplet = notations.append_child("tuplet");
        tuplet.append_attribute("type") = "stop";
        tuplet.append_attribute("number") = boundary.number;
    }
}

void emit_note_notations(pugi::xml_node note_node,
                         const sunny::core::Note& note,
                         const sunny::core::NoteGroup& ng,
                         bool is_first_note,
                         bool tie_stop,
                         const std::vector<TupletBoundary>& tuplet_starts,
                         const std::vector<TupletBoundary>& tuplet_stops,
                         sunny::core::CompilationReport& report,
                         std::optional<sunny::core::ScoreTime> location,
                         std::optional<sunny::core::PartId> part_id) {
    // Determine whether we need a <notations> element at all
    bool need_notations = false;

    if (tie_stop || note.tie_forward) need_notations = true;
    if (is_first_note && (ng.slur_start || ng.slur_end)) need_notations = true;
    if (is_first_note && (!tuplet_starts.empty() || !tuplet_stops.empty())) need_notations = true;
    if (note.ornament || !note.technical.empty()) need_notations = true;
    if (note.articulation) {
        auto* entry = find_articulation_entry(*note.articulation);
        if (entry) need_notations = true;
    }

    if (!need_notations) return;

    auto notations = note_node.append_child("notations");

    // Tied notation
    if (tie_stop) {
        auto tied = notations.append_child("tied");
        tied.append_attribute("type") = "stop";
    }
    if (note.tie_forward) {
        auto tied = notations.append_child("tied");
        tied.append_attribute("type") = "start";
    }

    // Slur
    if (is_first_note && ng.slur_end) {
        auto slur = notations.append_child("slur");
        slur.append_attribute("type") = "stop";
    }
    if (is_first_note && ng.slur_start) {
        auto slur = notations.append_child("slur");
        slur.append_attribute("type") = "start";
    }

    if (is_first_note) emit_tuplet_boundaries(notations, tuplet_starts, tuplet_stops);

    // Articulations by category
    const bool richer_ornament_precedes_articulation =
        note.ornament && note.articulation &&
        ornament_supersedes_articulation(*note.ornament, *note.articulation);
    if (note.articulation && !richer_ornament_precedes_articulation) {
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
                auto ornament = orns.append_child(entry->element_name);
                if (*note.articulation == sunny::core::ArticulationType::Tremolo) {
                    ornament.append_attribute("type") = "unmeasured";
                    ornament.text().set(0);
                }
                break;
            }
            case ArticulationCategory::Technical: {
                auto tech = notations.append_child("technical");
                tech.append_child(entry->element_name);
                break;
            }
            case ArticulationCategory::Dynamics: {
                auto dynamics = notations.append_child("dynamics");
                dynamics.append_child(entry->element_name);
                break;
            }
            case ArticulationCategory::Notations: {
                // Fermata goes directly under <notations>
                notations.append_child(entry->element_name);
                break;
            }
            case ArticulationCategory::GlissandoStart:
            case ArticulationCategory::GlissandoStop: {
                auto glissando = notations.append_child("glissando");
                glissando.append_attribute("type") =
                    entry->category == ArticulationCategory::GlissandoStart ? "start" : "stop";
                break;
            }
            case ArticulationCategory::OtherTechnical: {
                auto tech = notations.append_child("technical");
                tech.append_child("other-technical").text().set(entry->element_name);
                report.diagnostics.push_back(
                    {"Articulation has no lossless standard MusicXML encoding; emitted "
                     "<other-technical>: " +
                         std::string{entry->element_name},
                     location,
                     part_id});
                break;
            }
            }
        }
    }

    // First-class Ornament metadata is richer than the legacy articulation variants.
    if (note.ornament) {
        const auto& ornament = *note.ornament;
        if (ornament.type == sunny::core::OrnamentType::Arpeggio) {
            auto arpeggiate = notations.append_child("arpeggiate");
            if (ornament.arpeggio_direction == sunny::core::ArpeggioDirection::Up)
                arpeggiate.append_attribute("direction") = "up";
            else if (ornament.arpeggio_direction == sunny::core::ArpeggioDirection::Down)
                arpeggiate.append_attribute("direction") = "down";
        } else {
            auto ornaments = notations.append_child("ornaments");
            pugi::xml_node element;
            switch (ornament.type) {
            case sunny::core::OrnamentType::Trill:
                element = ornaments.append_child("trill-mark");
                if (ornament.trill_interval == 0)
                    element.append_attribute("trill-step") = "unison";
                else if (ornament.trill_interval == 1)
                    element.append_attribute("trill-step") = "half";
                else if (ornament.trill_interval == 2)
                    element.append_attribute("trill-step") = "whole";
                else
                    report.diagnostics.push_back(
                        {"MusicXML trill-step cannot represent a " +
                             std::to_string(ornament.trill_interval) +
                             "-semitone Ornament interval; emitted the trill symbol without "
                             "playback-step metadata",
                         location,
                         part_id});
                break;
            case sunny::core::OrnamentType::Mordent:
                element = ornaments.append_child("mordent");
                break;
            case sunny::core::OrnamentType::InvertedMordent:
                element = ornaments.append_child("inverted-mordent");
                break;
            case sunny::core::OrnamentType::Turn:
                element = ornaments.append_child("turn");
                break;
            case sunny::core::OrnamentType::InvertedTurn:
                element = ornaments.append_child("inverted-turn");
                break;
            case sunny::core::OrnamentType::Shake:
                element = ornaments.append_child("shake");
                break;
            case sunny::core::OrnamentType::Arpeggio:
                break;
            }

            if (ornament.type == sunny::core::OrnamentType::Trill && ornament.accidental) {
                const char* accidental = nullptr;
                switch (*ornament.accidental) {
                case -2:
                    accidental = "flat-flat";
                    break;
                case -1:
                    accidental = "flat";
                    break;
                case 0:
                    accidental = "natural";
                    break;
                case 1:
                    accidental = "sharp";
                    break;
                case 2:
                    accidental = "double-sharp";
                    break;
                default:
                    report.diagnostics.push_back(
                        {"MusicXML accidental-mark cannot represent Ornament accidental " +
                             std::to_string(*ornament.accidental),
                         location,
                         part_id});
                    break;
                }
                if (accidental) ornaments.append_child("accidental-mark").text().set(accidental);
            }
        }
    }

    // Preserve all technical directions. MusicXML-native variants use their standard
    // elements; variants whose Sunny model lacks a required start/stop phase use
    // <other-technical> plus a diagnostic instead of silently inventing linkage.
    pugi::xml_node technical;
    pugi::xml_node technical_articulations;
    auto technical_parent = [&]() {
        if (!technical) technical = notations.append_child("technical");
        return technical;
    };
    auto articulation_parent = [&]() {
        if (!technical_articulations)
            technical_articulations = notations.append_child("articulations");
        return technical_articulations;
    };
    auto emit_extension = [&](const std::string& text) {
        technical_parent().append_child("other-technical").text().set(text.c_str());
        report.diagnostics.push_back(
            {"Sunny TechnicalDirection has no lossless standard MusicXML encoding; emitted "
             "<other-technical>: " +
                 text,
             location,
             part_id});
    };

    for (const auto& direction : note.technical) {
        switch (direction.type) {
        case sunny::core::TechnicalDirection::Type::Fingering:
            if (direction.fingering.empty()) {
                emit_extension("fingering (unspecified)");
            } else {
                for (auto finger : direction.fingering)
                    technical_parent()
                        .append_child("fingering")
                        .text()
                        .set(std::to_string(finger).c_str());
            }
            break;
        case sunny::core::TechnicalDirection::Type::StringNumber:
            if (direction.number == 0)
                emit_extension("string 0");
            else
                technical_parent().append_child("string").text().set(direction.number);
            break;
        case sunny::core::TechnicalDirection::Type::Position:
            emit_extension("position " + std::to_string(direction.number));
            break;
        case sunny::core::TechnicalDirection::Type::BowingPattern:
            emit_extension(direction.pattern.empty() ? "bowing pattern (unspecified)"
                                                     : "bowing pattern: " + direction.pattern);
            break;
        case sunny::core::TechnicalDirection::Type::BreathMark:
            articulation_parent().append_child("breath-mark");
            break;
        case sunny::core::TechnicalDirection::Type::Slide: {
            const char* slide = "into";
            switch (direction.slide_direction) {
            case sunny::core::SlideDirection::Into:
                break;
            case sunny::core::SlideDirection::OutOf:
                slide = "out-of";
                break;
            case sunny::core::SlideDirection::Ascending:
                slide = "ascending";
                break;
            case sunny::core::SlideDirection::Descending:
                slide = "descending";
                break;
            }
            emit_extension(std::string{"slide "} + slide);
            break;
        }
        case sunny::core::TechnicalDirection::Type::HammerOn:
            emit_extension("hammer-on");
            break;
        case sunny::core::TechnicalDirection::Type::PullOff:
            emit_extension("pull-off");
            break;
        case sunny::core::TechnicalDirection::Type::Bend: {
            auto bend = technical_parent().append_child("bend");
            bend.append_child("bend-alter")
                .text()
                .set(static_cast<double>(direction.bend_cents) / 100.0);
            break;
        }
        case sunny::core::TechnicalDirection::Type::Vibrato: {
            const char* speed = "normal";
            switch (direction.vibrato_speed) {
            case sunny::core::VibratoSpeed::Slow:
                speed = "slow";
                break;
            case sunny::core::VibratoSpeed::Normal:
                break;
            case sunny::core::VibratoSpeed::Fast:
                speed = "fast";
                break;
            case sunny::core::VibratoSpeed::None:
                speed = "none";
                break;
            }
            emit_extension(std::string{"vibrato "} + speed);
            break;
        }
        case sunny::core::TechnicalDirection::Type::Caesura:
            articulation_parent().append_child("caesura");
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// Compute total MusicXML duration of all events in a voice for <backup>
// -----------------------------------------------------------------------------

int compute_voice_forward_duration(const sunny::core::Voice& voice, int divisions) {
    int total = 0;
    for (const auto& event : voice.events) {
        auto dur = event.duration();
        if (dur != sunny::core::Beat::zero()) {
            total += beat_to_mxml_duration(dur, divisions);
        }
    }
    return total;
}

const sunny::core::Event*
adjacent_measured_event(const sunny::core::Voice& voice, std::size_t event_index, bool forward) {
    std::size_t cursor = event_index;
    while (forward ? cursor + 1 < voice.events.size() : cursor > 0) {
        cursor = forward ? cursor + 1 : cursor - 1;
        const auto& candidate = voice.events[cursor];
        if (candidate.is_note_group() || candidate.is_rest()) return &candidate;
    }
    return nullptr;
}

std::vector<const sunny::core::TupletContext*>
event_tuplet_chain(const sunny::core::Event* event, const sunny::core::TupletContextMap& contexts) {
    if (!event) return {};
    auto chain =
        sunny::core::tuplet_context_chain(sunny::core::event_tuplet_context(*event), contexts);
    return chain ? *chain : std::vector<const sunny::core::TupletContext*>{};
}

struct EventTupletProjection {
    std::vector<const sunny::core::TupletContext*> chain;
    std::vector<TupletBoundary> starts;
    std::vector<TupletBoundary> stops;
    sunny::core::Beat cumulative_ratio = sunny::core::Beat::one();
};

EventTupletProjection project_event_tuplets(const sunny::core::Voice& voice,
                                            std::size_t event_index,
                                            const sunny::core::TupletContextMap& contexts) {
    EventTupletProjection projection;
    projection.chain = event_tuplet_chain(&voice.events[event_index], contexts);
    const auto previous =
        event_tuplet_chain(adjacent_measured_event(voice, event_index, false), contexts);
    const auto next =
        event_tuplet_chain(adjacent_measured_event(voice, event_index, true), contexts);

    std::size_t previous_common = 0;
    while (previous_common < previous.size() && previous_common < projection.chain.size() &&
           previous[previous_common]->id == projection.chain[previous_common]->id)
        ++previous_common;
    for (std::size_t i = previous_common; i < projection.chain.size(); ++i)
        projection.starts.push_back(TupletBoundary{projection.chain[i], static_cast<int>(i + 1)});

    std::size_t next_common = 0;
    while (next_common < next.size() && next_common < projection.chain.size() &&
           next[next_common]->id == projection.chain[next_common]->id)
        ++next_common;
    for (std::size_t i = next_common; i < projection.chain.size(); ++i)
        projection.stops.push_back(TupletBoundary{projection.chain[i], static_cast<int>(i + 1)});

    if (const auto ratio = sunny::core::cumulative_tuplet_written_ratio(projection.chain))
        projection.cumulative_ratio = *ratio;
    return projection;
}

void emit_tuplet_time_modification(pugi::xml_node note,
                                   const EventTupletProjection& projection,
                                   const sunny::core::WrittenDuration* written = nullptr) {
    // A ratio implied by the duration itself (a 1/12 note outside any
    // TupletContext is an eighth under 3:2) multiplies the context ratio.
    const std::int64_t inferred_actual = written ? written->inferred_actual : 1;
    const std::int64_t inferred_normal = written ? written->inferred_normal : 1;
    if (projection.chain.empty() && inferred_actual == inferred_normal) return;
    auto tm = note.append_child("time-modification");
    tm.append_child("actual-notes")
        .text()
        .set(static_cast<long long>(projection.cumulative_ratio.numerator() * inferred_actual));
    tm.append_child("normal-notes")
        .text()
        .set(static_cast<long long>(projection.cumulative_ratio.denominator() * inferred_normal));
    if (projection.chain.size() > 1) {
        const auto outer_type = projection.chain.front()->normal_type;
        if (const auto graphic = musicxml_graphic_duration(outer_type)) {
            tm.append_child("normal-type").text().set(graphic->type.data());
            for (int dot = 0; dot < graphic->dots; ++dot)
                tm.append_child("normal-dot");
        }
    }
}

struct BeamProjection {
    const char* value = nullptr;
};

/// Beam value of one tied piece of a split event: a group boundary stays on
/// the outer piece and the inner pieces continue the beam.
BeamProjection beam_for_piece(BeamProjection event_beam, bool first_piece, bool last_piece) {
    if (!event_beam.value || (first_piece && last_piece)) return event_beam;
    const std::string_view value = event_beam.value;
    if (value == "begin" && first_piece) return event_beam;
    if (value == "end" && last_piece) return event_beam;
    return {"continue"};
}

BeamProjection primary_beam_for(const sunny::core::Voice& voice, sunny::core::EventId event_id) {
    for (const auto& group : voice.beam_groups) {
        const auto found = std::find(group.event_ids.begin(), group.event_ids.end(), event_id);
        if (found == group.event_ids.end() || group.event_ids.size() < 2) continue;
        if (found == group.event_ids.begin()) return {"begin"};
        if (std::next(found) == group.event_ids.end()) return {"end"};
        return {"continue"};
    }
    return {};
}

void emit_primary_beam(pugi::xml_node note, const BeamProjection& projection) {
    if (!projection.value) return;
    auto beam = note.append_child("beam");
    beam.append_attribute("number") = 1;
    beam.text().set(projection.value);
}

enum class LyricExtendProjection : std::uint8_t { None, Start, Continue, Stop };

struct ProjectedLyric {
    std::uint16_t verse = 1;
    const sunny::core::LyricSyllable* syllable = nullptr;
    LyricExtendProjection extend{LyricExtendProjection::None};
};

const char* musicxml_syllabic(sunny::core::LyricSyllabic syllabic) {
    using sunny::core::LyricSyllabic;
    switch (syllabic) {
    case LyricSyllabic::Single:
        return "single";
    case LyricSyllabic::Begin:
        return "begin";
    case LyricSyllabic::Middle:
        return "middle";
    case LyricSyllabic::End:
        return "end";
    }
    return "single";
}

const char* musicxml_extend_type(LyricExtendProjection extend) {
    switch (extend) {
    case LyricExtendProjection::Start:
        return "start";
    case LyricExtendProjection::Continue:
        return "continue";
    case LyricExtendProjection::Stop:
        return "stop";
    case LyricExtendProjection::None:
        break;
    }
    return nullptr;
}

std::map<std::uint64_t, std::vector<ProjectedLyric>>
project_part_lyrics(const sunny::core::Part& part) {
    using Lane = std::pair<std::uint8_t, std::uint8_t>; // staff, voice
    std::map<Lane, std::vector<const sunny::core::Event*>> lanes;
    for (const auto& measure : part.measures)
        for (const auto& voice : measure.voices)
            for (const auto& event : voice.events) {
                const auto* group = event.as_note_group();
                if (!group || std::ranges::any_of(group->notes, [](const auto& note) {
                        return note.grace.has_value();
                    }))
                    continue;
                lanes[{voice.staff_index, voice.voice_index}].push_back(&event);
            }

    std::map<std::uint64_t, std::vector<ProjectedLyric>> result;
    for (const auto& [lane, events] : lanes) {
        (void)lane;
        std::set<std::uint16_t> verses;
        for (const auto* event : events) {
            const auto* group = event->as_note_group();
            if (!group || group->notes.empty()) continue;
            for (const auto& lyric : group->notes.front().lyrics)
                verses.insert(lyric.verse);
        }

        for (const auto verse : verses) {
            std::optional<std::size_t> active_extend;
            const auto finish_extend = [&](std::size_t endpoint) {
                if (!active_extend || endpoint <= *active_extend) return;
                for (std::size_t i = *active_extend + 1; i <= endpoint; ++i) {
                    result[events[i]->id.value].push_back(
                        ProjectedLyric{verse,
                                       nullptr,
                                       i == endpoint ? LyricExtendProjection::Stop
                                                     : LyricExtendProjection::Continue});
                }
                active_extend.reset();
            };

            for (std::size_t i = 0; i < events.size(); ++i) {
                const auto* group = events[i]->as_note_group();
                const sunny::core::LyricSyllable* syllable = nullptr;
                if (group && !group->notes.empty()) {
                    const auto found = std::ranges::find_if(
                        group->notes.front().lyrics,
                        [verse](const auto& lyric) { return lyric.verse == verse; });
                    if (found != group->notes.front().lyrics.end()) syllable = &*found;
                }
                if (!syllable) continue;

                if (active_extend) finish_extend(i - 1);
                result[events[i]->id.value].push_back(ProjectedLyric{
                    verse,
                    syllable,
                    syllable->extend ? LyricExtendProjection::Start : LyricExtendProjection::None});
                if (syllable->extend) active_extend = i;
            }
            if (active_extend) finish_extend(events.size() - 1);
        }
    }

    for (auto& [event_id, lyrics] : result) {
        (void)event_id;
        std::sort(lyrics.begin(), lyrics.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.verse < rhs.verse;
        });
    }
    return result;
}

void emit_projected_lyrics(pugi::xml_node note, const std::vector<ProjectedLyric>& lyrics) {
    for (const auto& projection : lyrics) {
        auto lyric = note.append_child("lyric");
        lyric.append_attribute("number") = static_cast<unsigned int>(projection.verse);
        if (projection.syllable) {
            lyric.append_child("syllabic")
                .text()
                .set(musicxml_syllabic(projection.syllable->syllabic));
            lyric.append_child("text").text().set(projection.syllable->text.c_str());
        }
        if (const auto* type = musicxml_extend_type(projection.extend)) {
            auto extend = lyric.append_child("extend");
            extend.append_attribute("type") = type;
        }
    }
}

} // anonymous namespace

// =============================================================================
// compile_score_to_musicxml
// =============================================================================

sunny::core::Result<MusicXmlCompilationResult>
compile_score_to_musicxml(const sunny::core::Score& score) {
    if (!sunny::core::is_compilable(score)) {
        return std::unexpected(sunny::core::ErrorCode::InvariantViolation);
    }

    sunny::core::CompilationReport report;
    report.tuning_definitions_requested = 1;
    if (sunny::core::is_standard_midi_tuning(score.tuning)) {
        report.tuning_definitions_written = 1;
    } else {
        report.diagnostics.push_back(
            {"Score tuning was not written: Sunny's admitted MusicXML 4 profile preserves notation "
             "pitch but has no complete 128-note sounding-pitch carrier",
             std::nullopt,
             std::nullopt});
    }
    if (!score.section_map.empty()) {
        report.diagnostics.push_back(
            {"The hierarchical SectionMap was not represented in the MusicXML notation profile",
             score.section_map.front().start,
             std::nullopt});
    }
    if (!score.harmonic_annotations.empty()) {
        report.diagnostics.push_back(
            {"The persisted harmonic-analysis layer was not represented in the MusicXML notation "
             "profile; Voice ChordSymbol events remain a distinct exported construct",
             score.harmonic_annotations.front().position,
             std::nullopt});
    }
    if (!score.orchestration_annotations.empty()) {
        report.diagnostics.push_back(
            {"The orchestration-analysis layer, including any rendering-only dynamic balance, was "
             "not represented in the MusicXML notation profile",
             score.orchestration_annotations.front().start,
             score.orchestration_annotations.front().part_id});
    }
    if (score.tone_row) {
        report.diagnostics.push_back(
            {"The governing twelve-tone row was not represented in the MusicXML notation profile",
             std::nullopt,
             std::nullopt});
    }
    if (!score.stale_harmonic_regions.empty()) {
        report.diagnostics.push_back(
            {"Stale harmonic-analysis region state was not represented in the MusicXML notation "
             "profile",
             score.stale_harmonic_regions.front().start,
             std::nullopt});
    }
    if (!score.stale_orchestration_regions.empty()) {
        report.diagnostics.push_back(
            {"Stale orchestration-analysis region state was not represented in the MusicXML "
             "notation profile",
             score.stale_orchestration_regions.front().start,
             std::nullopt});
    }
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group) continue;
                    for (const auto& note : group->notes) {
                        const auto position =
                            sunny::core::ScoreTime{measure.bar_number, event.offset};
                        if (!semantic_dynamic(note) &&
                            sunny::core::is_valid_velocity(note.velocity.value)) {
                            report.diagnostics.push_back(
                                {"Explicit numeric note attack velocity was not represented in "
                                 "the MusicXML notation profile; MusicXML dynamics are relative "
                                 "playback suggestions rather than an exact MIDI-velocity byte",
                                 position,
                                 part.id});
                        }
                        if (note.release_velocity != 64) {
                            report.diagnostics.push_back(
                                {"Non-default note release velocity was not represented in the "
                                 "MusicXML notation profile",
                                 position,
                                 part.id});
                        }
                    }
                }
            }
        }
    }

    // Preflight the complete target timing algebra. This compiler's tractable
    // MusicXML profile uses positive int-valued divisions and duration units.
    const auto timing_values = collect_all_musicxml_timing_values(score);
    const auto computed_divisions =
        compute_divisions_from(timing_values.begin(), timing_values.end());
    if (!computed_divisions) return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    const int divisions = *computed_divisions;
    if (std::any_of(timing_values.begin(), timing_values.end(), [&](sunny::core::Beat value) {
            return !checked_beat_to_mxml_duration(value, divisions).has_value();
        }))
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

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
        std::map<std::uint8_t, std::vector<sunny::core::SpelledPitch>> incoming_ties;
        const auto lyric_projection = project_part_lyrics(part);
        // Score notes are concert pitch; MusicXML writes transposing parts at
        // written pitch with <transpose> giving written-to-sounding.
        const auto written_to_sounding = sunny::core::written_to_sounding_interval(part.definition);
        const auto concert_to_written = sunny::core::interval_negate(written_to_sounding);

        auto part_node = score_node.append_child("part");
        part_node.append_attribute("id") = part_id_str.c_str();

        std::optional<sunny::core::KeySignature> previous_key;
        std::optional<sunny::core::TimeSignature> previous_time;

        for (const auto& measure : part.measures) {
            auto m = part_node.append_child("measure");
            m.append_attribute("number") = static_cast<int>(measure.bar_number);

            std::uint32_t bar = measure.bar_number;

            // Determine active key and time for this bar
            const sunny::core::KeySignature* active_key = nullptr;
            const sunny::core::TimeSignature* active_time = nullptr;
            const sunny::core::Clef* active_clef = nullptr;

            // Check for local overrides first, then global maps
            if (measure.local_key) {
                active_key = &*measure.local_key;
            } else {
                auto* ke = key_at_measure_start(score.key_map, bar);
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
            const bool key_changed =
                active_key && (is_first_bar || !previous_key ||
                               !same_key_signature_state(*active_key, *previous_key));
            const bool time_changed =
                active_time && (is_first_bar || !previous_time ||
                                !same_time_signature_state(*active_time, *previous_time));

            if (is_first_bar) active_clef = &part.definition.clef;

            if (is_first_bar || key_changed || time_changed) {
                const auto written_active_key = active_key
                                                    ? written_key(*active_key, concert_to_written)
                                                    : sunny::core::KeySignature{};
                emit_attributes(m,
                                divisions,
                                key_changed ? &written_active_key : nullptr,
                                time_changed ? active_time : nullptr,
                                is_first_bar ? active_clef : nullptr,
                                is_first_bar ? part.definition.staff_count : 1,
                                is_first_bar ? &part.definition.staff_clefs : nullptr,
                                is_first_bar ? std::optional{written_to_sounding} : std::nullopt,
                                report,
                                sunny::core::ScoreTime{bar, sunny::core::Beat::zero()},
                                part.id);
            }
            if (active_key) previous_key = *active_key;
            if (active_time) previous_time = *active_time;

            // Global point events are emitted while the measure cursor is at zero.
            // Their offsets therefore equal their absolute within-measure positions.
            for (std::size_t tempo_index = 0; tempo_index < score.tempo_map.size(); ++tempo_index) {
                const auto& te = score.tempo_map[tempo_index];
                if (te.position.bar == bar) {
                    double bpm = sunny::core::effective_quarter_bpm(te);
                    emit_tempo_direction(
                        m, te, bpm, beat_to_mxml_duration(te.position.beat, divisions));
                }
                if (tempo_index + 1 < score.tempo_map.size() && te.position.bar == bar &&
                    score.tempo_map[tempo_index + 1].transition_type ==
                        sunny::core::TempoTransitionType::Linear) {
                    emit_linear_tempo_start(m,
                                            beat_to_mxml_duration(te.position.beat, divisions),
                                            report,
                                            te.position,
                                            part.id,
                                            pi == 0);
                }
            }

            // Emit rehearsal marks at this bar
            for (const auto& rm : score.rehearsal_marks) {
                if (rm.position.bar == bar) {
                    emit_rehearsal_direction(
                        m, rm.label, beat_to_mxml_duration(rm.position.beat, divisions));
                }
            }

            // Hairpins are Part-level point boundaries, so emit each exactly once
            // rather than once per Voice/event sharing the boundary.
            for (const auto& hp : part.hairpins) {
                if (hp.start.bar == bar) {
                    emit_hairpin_start(m, hp.type, beat_to_mxml_duration(hp.start.beat, divisions));
                }
                const bool terminal_score_endpoint = bar == score.metadata.total_bars &&
                                                     hp.end.bar == bar + 1 &&
                                                     hp.end.beat == sunny::core::Beat::zero();
                if (hp.end.bar == bar || terminal_score_endpoint) {
                    const auto endpoint = terminal_score_endpoint && active_time
                                              ? active_time->measure_duration()
                                              : hp.end.beat;
                    const int endpoint_units = beat_to_mxml_duration(endpoint, divisions);
                    emit_hairpin_stop(m, endpoint_units);
                    if (hp.target) emit_dynamic_direction(m, *hp.target, 1, endpoint_units);
                }
            }

            // PartDirective is scoped semantic/notation state. Preserve both
            // half-open boundaries as exact visible words; host playback effects
            // remain a separate target capability decision.
            for (const auto& directive : part.part_directives) {
                const bool terminal_score_endpoint =
                    bar == score.metadata.total_bars && directive.end.bar == bar + 1 &&
                    directive.end.beat == sunny::core::Beat::zero();
                if (directive.end.bar == bar || terminal_score_endpoint) {
                    const auto endpoint = terminal_score_endpoint && active_time
                                              ? active_time->measure_duration()
                                              : directive.end.beat;
                    emit_part_directive(m,
                                        "end " + directive_label(directive),
                                        beat_to_mxml_duration(endpoint, divisions));
                }
                if (directive.start.bar == bar) {
                    emit_part_directive(m,
                                        directive_label(directive),
                                        beat_to_mxml_duration(directive.start.beat, divisions));
                }
            }

            // A traditional key change may occur inside a measure. Position the
            // attributes node with an exact temporary cursor move, then restore the
            // cursor before emitting measured voices.
            for (const auto& key_entry : score.key_map) {
                if (key_entry.position.bar != bar ||
                    key_entry.position.beat == sunny::core::Beat::zero())
                    continue;
                const int offset_units = beat_to_mxml_duration(key_entry.position.beat, divisions);
                emit_forward(m, offset_units, 1);
                const auto written_change = written_key(key_entry.key, concert_to_written);
                emit_attributes(m,
                                divisions,
                                &written_change,
                                nullptr,
                                nullptr,
                                1,
                                nullptr,
                                std::nullopt,
                                report,
                                key_entry.position,
                                part.id);
                emit_backup(m, offset_units);
                previous_key = key_entry.key;
            }

            // Emit events voice by voice
            for (std::size_t vi = 0; vi < measure.voices.size(); ++vi) {
                const auto& voice = measure.voices[vi];
                const auto tuplet_contexts = sunny::core::collect_tuplet_contexts(voice);
                int voice_number = static_cast<int>(voice.voice_index) + 1;
                const int staff_number =
                    part.definition.staff_count > 1 ? static_cast<int>(voice.staff_index) + 1 : 0;

                // If not the first voice, emit <backup> to rewind
                if (vi > 0) {
                    int backup_dur =
                        compute_voice_forward_duration(measure.voices[vi - 1], divisions);
                    if (backup_dur > 0) {
                        auto backup = m.append_child("backup");
                        backup.append_child("duration").text().set(backup_dur);
                    }
                }
                int cursor_units = 0;

                auto& voice_incoming_ties = incoming_ties[voice.voice_index];
                for (std::size_t event_index = 0; event_index < voice.events.size();
                     ++event_index) {
                    const auto& event = voice.events[event_index];
                    sunny::core::ScoreTime event_location{bar, event.offset};

                    // Handle each payload variant
                    if (auto* ng = std::get_if<sunny::core::NoteGroup>(&event.payload)) {
                        const auto tuplets =
                            project_event_tuplets(voice, event_index, tuplet_contexts);
                        const auto beam_projection = primary_beam_for(voice, event.id);
                        // Emit one onset-scoped direction for the first semantic
                        // dynamic, regardless of which coherent Score carrier owns it.
                        for (const auto& note : ng->notes) {
                            if (const auto dynamic = semantic_dynamic(note)) {
                                emit_dynamic_direction(m, *dynamic, voice_number, 0, staff_number);
                                break; // one dynamic per NoteGroup
                            }
                        }

                        // Every written note needs a value and dots (Gould). A
                        // duration with no single glyph is written as tied
                        // pieces; one with no written form keeps its exact
                        // <duration> and is reported rather than mislabelled.
                        const bool grace_group = !ng->notes.empty() && ng->notes.front().grace;
                        const auto written = grace_group
                                                 ? std::nullopt
                                                 : sunny::core::project_written_duration(
                                                       ng->duration, tuplets.cumulative_ratio);
                        if (!grace_group && !written) {
                            report.diagnostics.push_back(
                                {"MusicXML has no written note value for this duration; emitted "
                                 "the exact <duration> without a <type>",
                                 event_location,
                                 part.id});
                        }
                        const std::size_t piece_count = written ? written->pieces.size() : 1;
                        const std::vector<TupletBoundary> no_boundaries;

                        for (std::size_t piece = 0; piece < piece_count; ++piece) {
                            const bool first_piece = piece == 0;
                            const bool last_piece = piece + 1 == piece_count;
                            const auto piece_duration =
                                written ? written->pieces[piece].sounding : ng->duration;
                            sunny::core::NoteGroup piece_group = *ng;
                            piece_group.slur_start = ng->slur_start && first_piece;
                            piece_group.slur_end = ng->slur_end && last_piece;

                            for (std::size_t ni = 0; ni < ng->notes.size(); ++ni) {
                                const auto& source_note = ng->notes[ni];
                                bool is_first_in_chord = (ni == 0);
                                const bool tie_stop =
                                    !first_piece ||
                                    std::find(voice_incoming_ties.begin(),
                                              voice_incoming_ties.end(),
                                              source_note.pitch) != voice_incoming_ties.end();
                                // Marks belong to the attack; later pieces carry
                                // only the tie that continues the sound.
                                sunny::core::Note note = source_note;
                                note.tie_forward = !last_piece || source_note.tie_forward;
                                if (!first_piece) {
                                    note.articulation.reset();
                                    note.ornament.reset();
                                    note.technical.clear();
                                }

                                auto n = m.append_child("note");

                                // Grace note
                                if (note.grace) {
                                    auto grace = n.append_child("grace");
                                    grace.append_attribute("slash") =
                                        *note.grace == sunny::core::GraceType::Acciaccatura ? "yes"
                                                                                            : "no";
                                }

                                // Chord flag for subsequent notes in the group
                                if (!is_first_in_chord) {
                                    n.append_child("chord");
                                }

                                // Pitch
                                emit_note_pitch(
                                    n, sunny::core::apply_interval(note.pitch, concert_to_written));

                                // Duration (omitted for grace notes)
                                if (!note.grace) {
                                    emit_note_duration(n, piece_duration, divisions);
                                }

                                // Sound ties precede voice/type in the MusicXML note sequence.
                                if (tie_stop) {
                                    auto tie = n.append_child("tie");
                                    tie.append_attribute("type") = "stop";
                                }
                                if (note.tie_forward) {
                                    auto tie = n.append_child("tie");
                                    tie.append_attribute("type") = "start";
                                }

                                const bool cue_size =
                                    note.notation_head == sunny::core::NoteHeadType::Cue;
                                bool graphic_duration_emitted = true;
                                if (written) {
                                    emit_written_value(
                                        n, written->pieces[piece].value, voice_number, cue_size);
                                } else {
                                    const auto written_type_duration =
                                        tuplets.chain.empty() ? ng->duration
                                                              : tuplets.chain.back()->normal_type;
                                    graphic_duration_emitted = emit_note_voice_type_and_dot(
                                        n, written_type_duration, voice_number, cue_size);
                                }
                                if (cue_size && !graphic_duration_emitted) {
                                    report.diagnostics.push_back(
                                        {"MusicXML cannot attach cue size without a representable "
                                         "graphic note type; sounding duration retained",
                                         event_location,
                                         part.id});
                                }

                                emit_tuplet_time_modification(
                                    n, tuplets, written ? &*written : nullptr);

                                if (note.notation_head && !cue_size) {
                                    if (const char* notehead =
                                            notehead_to_mxml(*note.notation_head))
                                        n.append_child("notehead").text().set(notehead);
                                }
                                if (staff_number > 0)
                                    n.append_child("staff").text().set(staff_number);

                                // Beam is chord/stem scoped, so only the first note in a
                                // simultaneous NoteGroup owns the MusicXML beam element.
                                if (is_first_in_chord)
                                    emit_primary_beam(
                                        n,
                                        beam_for_piece(beam_projection, first_piece, last_piece));

                                // Notations (articulations, ornaments, technical, ties,
                                // slurs, tuplets)
                                emit_note_notations(n,
                                                    note,
                                                    piece_group,
                                                    is_first_in_chord,
                                                    tie_stop,
                                                    first_piece ? tuplets.starts : no_boundaries,
                                                    last_piece ? tuplets.stops : no_boundaries,
                                                    report,
                                                    event_location,
                                                    part.id);

                                if (is_first_in_chord && first_piece) {
                                    const auto lyrics = lyric_projection.find(event.id.value);
                                    if (lyrics != lyric_projection.end())
                                        emit_projected_lyrics(n, lyrics->second);
                                }
                            }
                        }

                        const int allocation_units = beat_to_mxml_duration(ng->duration, divisions);
                        if (!ng->notes.empty() && ng->notes.front().grace)
                            emit_forward(m, allocation_units, voice_number, staff_number);
                        cursor_units += allocation_units;

                        voice_incoming_ties.clear();
                        for (const auto& note : ng->notes) {
                            if (note.tie_forward) voice_incoming_ties.push_back(note.pitch);
                        }

                    } else if (auto* rest = std::get_if<sunny::core::RestEvent>(&event.payload)) {
                        const auto tuplets =
                            project_event_tuplets(voice, event_index, tuplet_contexts);
                        const auto beam_projection = primary_beam_for(voice, event.id);
                        // A whole-measure rest is one symbol whatever the metre
                        // (MusicXML <rest measure="yes"/>); other rests are
                        // written as consecutive rest values.
                        const bool whole_measure =
                            tuplets.chain.empty() && active_time &&
                            rest->duration == active_time->measure_duration();
                        auto written = sunny::core::project_written_duration(
                            rest->duration, tuplets.cumulative_ratio);
                        if (whole_measure && written && written->pieces.size() > 1) written.reset();
                        if (!whole_measure && !written) {
                            report.diagnostics.push_back(
                                {"MusicXML has no written rest value for this duration; emitted "
                                 "the exact <duration> without a <type>",
                                 event_location,
                                 part.id});
                        }
                        const std::size_t piece_count = written ? written->pieces.size() : 1;
                        for (std::size_t piece = 0; piece < piece_count; ++piece) {
                            const bool first_piece = piece == 0;
                            const bool last_piece = piece + 1 == piece_count;
                            auto n = m.append_child("note");
                            if (!rest->visible) n.append_attribute("print-object") = "no";
                            auto rest_node = n.append_child("rest");
                            emit_note_duration(n,
                                               written ? written->pieces[piece].sounding
                                                       : rest->duration,
                                               divisions);
                            if (written) {
                                emit_written_value(
                                    n, written->pieces[piece].value, voice_number, false);
                            } else {
                                if (whole_measure) rest_node.append_attribute("measure") = "yes";
                                n.append_child("voice").text().set(voice_number);
                            }
                            emit_tuplet_time_modification(
                                n, tuplets, written ? &*written : nullptr);
                            if (staff_number > 0) n.append_child("staff").text().set(staff_number);
                            emit_primary_beam(
                                n, beam_for_piece(beam_projection, first_piece, last_piece));
                            const auto& starts =
                                first_piece ? tuplets.starts : std::vector<TupletBoundary>{};
                            const auto& stops =
                                last_piece ? tuplets.stops : std::vector<TupletBoundary>{};
                            if (!starts.empty() || !stops.empty()) {
                                auto notations = n.append_child("notations");
                                emit_tuplet_boundaries(notations, starts, stops);
                            }
                        }
                        cursor_units += beat_to_mxml_duration(rest->duration, divisions);
                        voice_incoming_ties.clear();

                    } else if (auto* cs =
                                   std::get_if<sunny::core::ChordSymbolEvent>(&event.payload)) {
                        const int event_units = beat_to_mxml_duration(event.offset, divisions);
                        emit_chord_symbol(m,
                                          *cs,
                                          event_units - cursor_units,
                                          staff_number,
                                          report,
                                          event_location,
                                          part.id);

                    } else if (auto* sd =
                                   std::get_if<sunny::core::ScoreDirection>(&event.payload)) {
                        const int event_units = beat_to_mxml_duration(event.offset, divisions);
                        const int relative_units = event_units - cursor_units;
                        const bool cursor_bound =
                            sd->type == sunny::core::DirectionType::ClefChange ||
                            sd->type == sunny::core::DirectionType::DoubleBarline ||
                            sd->type == sunny::core::DirectionType::FinalBarline ||
                            sd->type == sunny::core::DirectionType::RepeatStart ||
                            sd->type == sunny::core::DirectionType::RepeatEnd;
                        if (!cursor_bound) {
                            emit_score_direction(m,
                                                 *sd,
                                                 relative_units,
                                                 voice_number,
                                                 staff_number,
                                                 report,
                                                 event_location,
                                                 part.id);
                        } else {
                            if (relative_units < 0)
                                emit_backup(m, -relative_units);
                            else
                                emit_forward(m, relative_units, voice_number, staff_number);

                            if (sd->type == sunny::core::DirectionType::ClefChange) {
                                if (sd->new_clef)
                                    emit_clef_change(m, *sd->new_clef, staff_number);
                                else
                                    report.diagnostics.push_back(
                                        {"ClefChange has no target clef", event_location, part.id});
                            } else {
                                emit_barline_direction(
                                    m, sd->type, event.offset == sunny::core::Beat::zero());
                            }

                            if (relative_units < 0)
                                emit_forward(m, -relative_units, voice_number, staff_number);
                            else
                                emit_backup(m, relative_units);
                        }
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

} // namespace sunny::infrastructure::formats
