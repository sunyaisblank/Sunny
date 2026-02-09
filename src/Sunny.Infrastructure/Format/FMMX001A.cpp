/**
 * @file FMMX001A.cpp
 * @brief MusicXML reader/writer implementation
 *
 * Component: FMMX001A
 */

#include "FMMX001A.h"

#include "../../Sunny.Core/Pitch/PTPC001A.h"

#include <pugixml.hpp>
#include <sstream>
#include <cmath>

namespace Sunny::Infrastructure::Format {

namespace {

/// Letter index to MusicXML step character
constexpr std::array<char, 7> STEP_CHARS = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};

/// MusicXML step character to letter index
int step_to_letter(char step) {
    switch (step) {
        case 'C': return 0; case 'D': return 1; case 'E': return 2;
        case 'F': return 3; case 'G': return 4; case 'A': return 5;
        case 'B': return 6;
        default: return -1;
    }
}

/// Convert line-of-fifths key centre to MusicXML <fifths> value
/// The fifths value is the number of sharps (positive) or flats (negative).
/// This maps key centre LoF position to the conventional circle of fifths count.
int lof_to_fifths(Sunny::Core::SpelledPitch tonic, bool is_major) {
    // For major keys, the LoF position of the tonic directly gives the fifths count
    // C=0, G=1, D=2, A=3, E=4, B=5, F#=6, F=-1, Bb=-2, Eb=-3, Ab=-4, Db=-5, Gb=-6
    int lof = Sunny::Core::line_of_fifths_position(tonic);
    if (!is_major) {
        // Minor key: relative major is 3 LoF positions up (A minor -> C major)
        lof += 3;
    }
    return lof;
}

/// Convert MusicXML <fifths> to a SpelledPitch tonic
Sunny::Core::SpelledPitch fifths_to_tonic(int fifths, bool is_major) {
    int lof = fifths;
    if (!is_major) {
        lof -= 3;  // Relative minor is 3 LoF positions down
    }
    return Sunny::Core::from_line_of_fifths(lof, 4);
}

/// Compute divisions needed for a set of Beat durations
/// Returns the LCM of all denominators (relative to quarter note)
int compute_divisions(const MusicXmlScore& score) {
    int64_t lcm_val = 1;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& note : measure.notes) {
                auto r = note.duration.reduce();
                // Duration is in beats (quarter notes).
                // divisions * duration = MusicXML duration units
                // We need divisions such that divisions * (num/den) is always integral.
                // That means divisions must be a multiple of den.
                // But duration is in quarter notes (1/4 = quarter note = 1 beat).
                // Actually, Beat{1,4} = quarter note. Beat{1,2} = half note.
                // MusicXML duration = divisions * (beat_value_in_quarters)
                // beat_value_in_quarters = num/den / (1/4) = 4*num/den
                // So we need divisions * 4 * num / den to be integral.
                // => divisions must be a multiple of den / gcd(den, 4*num)
                int64_t beat_num = 4 * r.numerator;
                int64_t beat_den = r.denominator;
                int64_t g = std::gcd(beat_num, beat_den);
                int64_t needed_den = beat_den / g;
                lcm_val = std::lcm(lcm_val, needed_den);
            }
        }
    }
    return static_cast<int>(lcm_val);
}

/// Convert Beat duration to MusicXML duration units
int beat_to_mxml_duration(Sunny::Core::Beat dur, int divisions) {
    auto r = dur.reduce();
    // duration_units = divisions * 4 * num / den
    return static_cast<int>(static_cast<int64_t>(divisions) * 4 * r.numerator / r.denominator);
}

/// Convert MusicXML duration units to Beat
Sunny::Core::Beat mxml_duration_to_beat(int units, int divisions) {
    // units = divisions * 4 * num / den
    // In quarter notes: units / divisions beats
    // In absolute: units / (divisions * 4) whole notes = units / divisions quarter notes
    // Beat is in whole notes by convention? No, Beat is abstract rational.
    // Looking at existing usage: Beat{1,4} = quarter note, Beat{1,1} = whole note.
    // So Beat = units / (4 * divisions)
    return Sunny::Core::Beat{units, 4 * static_cast<int64_t>(divisions)};
}

/// Get MusicXML <type> string for a duration
std::string duration_type_name(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    // Map common durations
    if (r == Sunny::Core::Beat{1, 1}) return "whole";
    if (r == Sunny::Core::Beat{1, 2}) return "half";
    if (r == Sunny::Core::Beat{1, 4}) return "quarter";
    if (r == Sunny::Core::Beat{1, 8}) return "eighth";
    if (r == Sunny::Core::Beat{1, 16}) return "16th";
    if (r == Sunny::Core::Beat{1, 32}) return "32nd";
    if (r == Sunny::Core::Beat{3, 8}) return "quarter";  // dotted quarter
    if (r == Sunny::Core::Beat{3, 4}) return "half";      // dotted half
    if (r == Sunny::Core::Beat{3, 16}) return "eighth";   // dotted eighth
    return "quarter";  // fallback
}

bool is_dotted(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    return r.numerator == 3 && (r.denominator == 8 || r.denominator == 4 ||
                                 r.denominator == 16 || r.denominator == 2);
}

}  // anonymous namespace

// =============================================================================
// parse_musicxml
// =============================================================================

Sunny::Core::Result<MusicXmlScore> parse_musicxml(std::string_view xml) {
    pugi::xml_document doc;
    std::string xml_str(xml);
    auto parse_result = doc.load_string(xml_str.c_str());
    if (!parse_result) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidMusicXml);
    }

    auto score_node = doc.child("score-partwise");
    if (!score_node) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidMusicXml);
    }

    MusicXmlScore score;

    // Parse title from <work><work-title> or <movement-title>
    auto work_title = score_node.child("work").child("work-title");
    if (work_title) {
        score.title = work_title.text().get();
    }
    auto movement = score_node.child("movement-title");
    if (movement && score.title.empty()) {
        score.title = movement.text().get();
    }

    // Parse part-list
    auto part_list = score_node.child("part-list");

    // Parse parts
    for (auto part_node : score_node.children("part")) {
        MusicXmlPart part;
        part.id = part_node.attribute("id").as_string();

        // Look up part name from part-list
        if (part_list) {
            for (auto sp : part_list.children("score-part")) {
                if (std::string(sp.attribute("id").as_string()) == part.id) {
                    auto pn = sp.child("part-name");
                    if (pn) part.name = pn.text().get();
                    break;
                }
            }
        }

        int divisions = 1;

        for (auto measure_node : part_node.children("measure")) {
            MusicXmlMeasure measure;
            measure.number = measure_node.attribute("number").as_int();

            // Attributes
            auto attrs = measure_node.child("attributes");
            if (attrs) {
                auto div_node = attrs.child("divisions");
                if (div_node) {
                    divisions = div_node.text().as_int();
                    score.divisions = divisions;
                }

                auto key_node = attrs.child("key");
                if (key_node) {
                    int fifths = key_node.child("fifths").text().as_int();
                    auto mode_node = key_node.child("mode");
                    bool is_major = true;
                    if (mode_node) {
                        std::string mode = mode_node.text().get();
                        is_major = (mode != "minor");
                    }
                    measure.key_tonic = fifths_to_tonic(fifths, is_major);
                    measure.key_is_major = is_major;
                }

                auto time_node = attrs.child("time");
                if (time_node) {
                    int beats = time_node.child("beats").text().as_int();
                    int beat_type = time_node.child("beat-type").text().as_int();
                    if (beats > 0 && beat_type > 0) {
                        measure.time_signature = {beats, beat_type};
                    }
                }
            }

            // Notes
            for (auto note_node : measure_node.children("note")) {
                MusicXmlNote note;

                // Rest
                if (note_node.child("rest")) {
                    note.is_rest = true;
                } else {
                    // Pitch
                    auto pitch_node = note_node.child("pitch");
                    if (pitch_node) {
                        char step = pitch_node.child("step").text().get()[0];
                        int letter = step_to_letter(step);
                        if (letter < 0) {
                            return std::unexpected(Sunny::Core::ErrorCode::InvalidMusicXml);
                        }

                        int alter = 0;
                        auto alter_node = pitch_node.child("alter");
                        if (alter_node) {
                            alter = alter_node.text().as_int();
                        }

                        int octave = pitch_node.child("octave").text().as_int();

                        note.pitch = Sunny::Core::SpelledPitch{
                            static_cast<uint8_t>(letter),
                            static_cast<int8_t>(alter),
                            static_cast<int8_t>(octave)
                        };
                    }
                }

                // Chord flag
                if (note_node.child("chord")) {
                    note.is_chord = true;
                }

                // Voice
                auto voice_node = note_node.child("voice");
                if (voice_node) {
                    note.voice = voice_node.text().as_int();
                }

                // Duration
                auto dur_node = note_node.child("duration");
                if (dur_node) {
                    int units = dur_node.text().as_int();
                    note.duration = mxml_duration_to_beat(units, divisions);
                }

                measure.notes.push_back(note);
            }

            part.measures.push_back(std::move(measure));
        }

        score.parts.push_back(std::move(part));
    }

    return score;
}

// =============================================================================
// write_musicxml
// =============================================================================

Sunny::Core::Result<std::string> write_musicxml(const MusicXmlScore& score) {
    pugi::xml_document doc;

    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto score_node = doc.append_child("score-partwise");
    score_node.append_attribute("version") = "4.0";

    // Title
    if (!score.title.empty()) {
        auto work = score_node.append_child("work");
        work.append_child("work-title").text().set(score.title.c_str());
    }

    // Compute divisions
    int divisions = compute_divisions(score);
    if (divisions < 1) divisions = 1;

    // Part list
    auto part_list = score_node.append_child("part-list");
    for (const auto& part : score.parts) {
        auto sp = part_list.append_child("score-part");
        sp.append_attribute("id") = part.id.c_str();
        sp.append_child("part-name").text().set(part.name.c_str());
    }

    // Parts
    for (const auto& part : score.parts) {
        auto part_node = score_node.append_child("part");
        part_node.append_attribute("id") = part.id.c_str();

        for (const auto& measure : part.measures) {
            auto m = part_node.append_child("measure");
            m.append_attribute("number") = measure.number;

            // Attributes (if this measure has key, time, or is first)
            bool need_attrs = measure.time_signature.has_value() ||
                              measure.key_tonic.has_value() ||
                              measure.number == 1;
            if (need_attrs) {
                auto attrs = m.append_child("attributes");
                attrs.append_child("divisions").text().set(divisions);

                if (measure.key_tonic.has_value()) {
                    auto key = attrs.append_child("key");
                    bool is_major = measure.key_is_major.value_or(true);
                    int fifths = lof_to_fifths(*measure.key_tonic, is_major);
                    key.append_child("fifths").text().set(fifths);
                    key.append_child("mode").text().set(is_major ? "major" : "minor");
                }

                if (measure.time_signature.has_value()) {
                    auto time = attrs.append_child("time");
                    time.append_child("beats").text().set(measure.time_signature->first);
                    time.append_child("beat-type").text().set(measure.time_signature->second);
                }
            }

            // Notes
            for (const auto& note : measure.notes) {
                auto n = m.append_child("note");

                if (note.is_chord) {
                    n.append_child("chord");
                }

                if (note.is_rest) {
                    n.append_child("rest");
                } else {
                    auto pitch = n.append_child("pitch");
                    std::string step_str(1, STEP_CHARS[note.pitch.letter]);
                    pitch.append_child("step").text().set(step_str.c_str());
                    if (note.pitch.accidental != 0) {
                        pitch.append_child("alter").text().set(
                            static_cast<int>(note.pitch.accidental));
                    }
                    pitch.append_child("octave").text().set(
                        static_cast<int>(note.pitch.octave));
                }

                // Duration
                int dur_units = beat_to_mxml_duration(note.duration, divisions);
                n.append_child("duration").text().set(dur_units);

                // Voice
                n.append_child("voice").text().set(note.voice);

                // Type
                std::string type_name = duration_type_name(note.duration);
                n.append_child("type").text().set(type_name.c_str());

                // Dot
                if (is_dotted(note.duration)) {
                    n.append_child("dot");
                }
            }
        }
    }

    // Serialize to string
    std::ostringstream oss;
    doc.save(oss, "  ");
    return oss.str();
}

// =============================================================================
// Conversion functions
// =============================================================================

std::vector<Sunny::Core::NoteEvent> musicxml_to_note_events(const MusicXmlScore& score) {
    std::vector<Sunny::Core::NoteEvent> result;
    if (score.parts.empty()) return result;

    const auto& part = score.parts[0];
    Sunny::Core::Beat current_time = Sunny::Core::Beat::zero();

    for (const auto& measure : part.measures) {
        for (const auto& note : measure.notes) {
            if (note.is_chord) {
                // Chord note starts at same time as previous
                if (!result.empty()) {
                    auto& prev = result.back();
                    if (!note.is_rest) {
                        auto midi_val = Sunny::Core::midi(note.pitch);
                        if (midi_val) {
                            result.push_back({
                                *midi_val,
                                prev.start_time,
                                note.duration,
                                80,
                                false
                            });
                        }
                    }
                }
            } else if (note.is_rest) {
                current_time = current_time + note.duration;
            } else {
                auto midi_val = Sunny::Core::midi(note.pitch);
                if (midi_val) {
                    result.push_back({
                        *midi_val,
                        current_time,
                        note.duration,
                        80,
                        false
                    });
                }
                current_time = current_time + note.duration;
            }
        }
    }

    return result;
}

MusicXmlScore note_events_to_musicxml(
    std::span<const Sunny::Core::NoteEvent> events,
    int key_lof)
{
    MusicXmlScore score;
    score.title = "";

    MusicXmlPart part;
    part.id = "P1";
    part.name = "Part 1";

    // Simple: put all events in a single measure
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};

    auto tonic = Sunny::Core::from_line_of_fifths(key_lof, 4);
    measure.key_tonic = tonic;
    measure.key_is_major = true;

    for (const auto& ev : events) {
        if (ev.muted) continue;

        int8_t octave = static_cast<int8_t>(ev.pitch / 12 - 1);
        auto pc = Sunny::Core::pitch_class(ev.pitch);
        auto sp = Sunny::Core::default_spelling(pc, key_lof, octave);

        MusicXmlNote note;
        note.pitch = sp;
        note.duration = ev.duration;
        note.is_rest = false;
        note.is_chord = false;
        note.voice = 1;

        measure.notes.push_back(note);
    }

    part.measures.push_back(std::move(measure));
    score.parts.push_back(std::move(part));
    return score;
}

}  // namespace Sunny::Infrastructure::Format
