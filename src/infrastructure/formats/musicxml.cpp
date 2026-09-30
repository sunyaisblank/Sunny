/**
 * @file musicxml.cpp
 * @brief MusicXML reader/writer implementation
 *
 */

#include <cctype>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <pugixml.hpp>
#include <set>
#include <sstream>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>
#include <sunny/infrastructure/formats/musicxml_internal.hpp>

namespace sunny::infrastructure::formats {

using namespace MxmlInternal;

namespace {

/// Compute divisions for a MusicXmlScore by collecting all note durations.
/// The compact writer deliberately uses an int-valued timing profile.
std::optional<int> compute_divisions(const MusicXmlScore& score) {
    std::vector<sunny::core::Beat> durations;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& note : measure.notes)
                durations.push_back(note.duration);
            for (const auto& harmony : measure.harmonies)
                durations.push_back(harmony.offset);
        }
    }
    return compute_divisions_from(durations.begin(), durations.end());
}

bool valid_musicxml_mode(std::string_view mode) {
    return mode == "major" || mode == "minor" || mode == "dorian" || mode == "phrygian" ||
           mode == "lydian" || mode == "mixolydian" || mode == "aeolian" || mode == "ionian" ||
           mode == "locrian" || mode == "none";
}

std::optional<int> mode_fifths_adjustment(std::string_view mode) {
    if (mode.empty() || mode == "major" || mode == "ionian") return 0;
    if (mode == "dorian") return -2;
    if (mode == "minor" || mode == "aeolian") return -3;
    if (mode == "phrygian") return -4;
    if (mode == "lydian") return 1;
    if (mode == "mixolydian") return -1;
    if (mode == "locrian") return -5;
    return std::nullopt;
}

std::optional<bool> major_minor_family(std::string_view mode) {
    if (mode.empty() || mode == "major" || mode == "ionian") return true;
    if (mode == "minor" || mode == "aeolian") return false;
    return std::nullopt;
}

std::string_view trim_xml_token(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    return text;
}

std::optional<int> parse_xml_int(std::string_view text) {
    text = trim_xml_token(text);
    if (text.empty()) return std::nullopt;
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

std::optional<int> parse_xml_int(pugi::xml_node node) {
    return parse_xml_int(node.text().get());
}

std::size_t child_count(pugi::xml_node parent, const char* name) {
    std::size_t count = 0;
    for ([[maybe_unused]] auto child : parent.children(name))
        ++count;
    return count;
}

bool has_only_attributes(pugi::xml_node node, std::initializer_list<std::string_view> allowed) {
    for (const auto attribute : node.attributes()) {
        const std::string_view name{attribute.name()};
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) return false;
    }
    return true;
}

sunny::core::Result<sunny::core::SpelledPitch> parse_harmony_pitch(pugi::xml_node parent,
                                                                   const char* step_name,
                                                                   const char* alter_name,
                                                                   std::int8_t octave) {
    if (!has_only_attributes(parent, {}) || child_count(parent, step_name) != 1 ||
        child_count(parent, alter_name) > 1)
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    for (auto child : parent.children()) {
        const std::string_view name{child.name()};
        if (name != step_name && name != alter_name)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }
    if (!has_only_attributes(parent.child(step_name), {}) ||
        (parent.child(alter_name) && !has_only_attributes(parent.child(alter_name), {})))
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    const auto step = trim_xml_token(parent.child(step_name).text().get());
    const int letter = step.size() == 1 ? step_to_letter(step.front()) : -1;
    if (letter < 0) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    int alteration = 0;
    if (const auto alter = parent.child(alter_name)) {
        const auto parsed = parse_xml_int(alter);
        if (!parsed || *parsed < std::numeric_limits<std::int8_t>::min() ||
            *parsed > std::numeric_limits<std::int8_t>::max())
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        alteration = *parsed;
    }
    return sunny::core::SpelledPitch{
        static_cast<std::uint8_t>(letter), static_cast<std::int8_t>(alteration), octave};
}

sunny::core::Result<MusicXmlHarmony>
parse_harmony(pugi::xml_node node, sunny::core::Beat cursor, int divisions) {
    if (!has_only_attributes(node, {}))
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    const auto root_count = child_count(node, "root");
    const auto numeral_count = child_count(node, "numeral");
    if (root_count + numeral_count != 1 || child_count(node, "kind") != 1 ||
        child_count(node, "inversion") > 1 || child_count(node, "bass") > 1 ||
        child_count(node, "offset") > 1 || child_count(node, "staff") > 1)
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    for (auto child : node.children()) {
        const std::string_view name{child.name()};
        if (name != "root" && name != "numeral" && name != "kind" && name != "inversion" &&
            name != "bass" && name != "degree" && name != "offset" && name != "staff")
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }
    if (const auto staff = node.child("staff")) {
        if (!has_only_attributes(staff, {}))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto value = parse_xml_int(staff);
        if (!value || *value != 1) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }

    MusicXmlHarmony result;
    result.offset = cursor;
    if (const auto offset = node.child("offset")) {
        if (!has_only_attributes(offset, {}))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto units = parse_xml_int(offset);
        if (!units) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto absolute =
            sunny::core::checked_add(cursor, mxml_duration_to_beat(*units, divisions));
        if (!absolute || *absolute < sunny::core::Beat::zero())
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        result.offset = *absolute;
    }

    auto& symbol = result.symbol;
    if (const auto root = node.child("root")) {
        auto parsed = parse_harmony_pitch(root, "root-step", "root-alter", 4);
        if (!parsed) return std::unexpected(parsed.error());
        symbol.root = *parsed;
    } else {
        const auto numeral = node.child("numeral");
        if (!has_only_attributes(numeral, {}) || child_count(numeral, "numeral-root") != 1 ||
            child_count(numeral, "numeral-alter") > 1 || child_count(numeral, "numeral-key") != 1)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        for (auto child : numeral.children()) {
            const std::string_view name{child.name()};
            if (name != "numeral-root" && name != "numeral-alter" && name != "numeral-key")
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        }

        if (!has_only_attributes(numeral.child("numeral-root"), {"text"}) ||
            (numeral.child("numeral-alter") &&
             !has_only_attributes(numeral.child("numeral-alter"), {})))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

        const auto numeral_root = parse_xml_int(numeral.child("numeral-root"));
        if (!numeral_root || *numeral_root < 1 || *numeral_root > 7)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        int numeral_alteration = 0;
        if (const auto alter = numeral.child("numeral-alter")) {
            const auto parsed = parse_xml_int(alter);
            if (!parsed || *parsed < std::numeric_limits<std::int8_t>::min() ||
                *parsed > std::numeric_limits<std::int8_t>::max())
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
            numeral_alteration = *parsed;
        }

        const auto key = numeral.child("numeral-key");
        if (!has_only_attributes(key, {}) || child_count(key, "numeral-fifths") != 1 ||
            child_count(key, "numeral-mode") != 1)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        for (auto child : key.children()) {
            const std::string_view name{child.name()};
            if (name != "numeral-fifths" && name != "numeral-mode")
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        }
        if (!has_only_attributes(key.child("numeral-fifths"), {}) ||
            !has_only_attributes(key.child("numeral-mode"), {}))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto fifths = parse_xml_int(key.child("numeral-fifths"));
        const auto mode =
            musicxml_to_chord_numeral_mode(trim_xml_token(key.child("numeral-mode").text().get()));
        if (!fifths || *fifths < -7 || *fifths > 7 || !mode)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

        symbol.numeral = sunny::core::ChordNumeral{
            static_cast<std::uint8_t>(*numeral_root),
            static_cast<std::int8_t>(numeral_alteration),
            sunny::core::ChordNumeralKey{static_cast<std::int8_t>(*fifths), *mode}};
        const auto derived = sunny::core::derive_chord_numeral_root(*symbol.numeral);
        if (!derived) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        symbol.root = *derived;
        const std::string_view display = numeral.child("numeral-root").attribute("text").value();
        if (!display.empty()) symbol.roman = std::string{display};
    }

    const auto kind = node.child("kind");
    if (!has_only_attributes(kind, {"text"}))
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    const std::string_view kind_value = trim_xml_token(kind.text().get());
    if (!valid_musicxml_harmony_kind(kind_value))
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    const std::string_view display = kind.attribute("text").value();
    if (kind_value == "other") {
        if (display.empty()) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        symbol.quality = std::string{display};
    } else {
        symbol.quality = std::string{kind_value};
        if (!display.empty()) symbol.extensions.emplace_back(display);
    }

    if (const auto inversion = node.child("inversion")) {
        if (!has_only_attributes(inversion, {}))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto value = parse_xml_int(inversion);
        if (!value || *value < 0 ||
            static_cast<unsigned>(*value) > std::numeric_limits<std::uint16_t>::max())
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        symbol.inversion = static_cast<std::uint16_t>(*value);
    }
    if (const auto bass = node.child("bass")) {
        auto parsed = parse_harmony_pitch(bass, "bass-step", "bass-alter", 3);
        if (!parsed) return std::unexpected(parsed.error());
        symbol.bass = *parsed;
    }
    for (const auto degree : node.children("degree")) {
        if (!has_only_attributes(degree, {}) || child_count(degree, "degree-value") != 1 ||
            child_count(degree, "degree-alter") != 1 || child_count(degree, "degree-type") != 1)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        for (auto child : degree.children()) {
            const std::string_view name{child.name()};
            if (name != "degree-value" && name != "degree-alter" && name != "degree-type")
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        }
        if (!has_only_attributes(degree.child("degree-value"), {}) ||
            !has_only_attributes(degree.child("degree-alter"), {}) ||
            !has_only_attributes(degree.child("degree-type"), {}))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        const auto value = parse_xml_int(degree.child("degree-value"));
        const auto alteration = parse_xml_int(degree.child("degree-alter"));
        const auto type =
            musicxml_to_chord_degree_type(trim_xml_token(degree.child("degree-type").text().get()));
        if (!value || *value <= 0 ||
            static_cast<unsigned>(*value) > std::numeric_limits<std::uint16_t>::max() ||
            !alteration || *alteration < std::numeric_limits<std::int8_t>::min() ||
            *alteration > std::numeric_limits<std::int8_t>::max() || !type)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        symbol.degrees.push_back(
            {static_cast<std::uint16_t>(*value), static_cast<std::int8_t>(*alteration), *type});
    }
    return result;
}

bool valid_compact_harmony(const MusicXmlHarmony& harmony) {
    const auto& symbol = harmony.symbol;
    if (harmony.offset < sunny::core::Beat::zero() || symbol.root.letter > 6 ||
        symbol.root.octave != 4 || symbol.quality.empty() ||
        (symbol.bass && (symbol.bass->letter > 6 || symbol.bass->octave != 3)) ||
        (symbol.roman && symbol.roman->empty()) || symbol.extensions.size() > 1)
        return false;
    if (!symbol.extensions.empty() && symbol.extensions.front().empty()) return false;
    for (const auto& degree : symbol.degrees) {
        if (degree.value == 0 ||
            static_cast<std::uint8_t>(degree.type) >
                static_cast<std::uint8_t>(sunny::core::ChordDegreeType::Subtract))
            return false;
    }
    if (symbol.numeral) {
        const auto derived = sunny::core::derive_chord_numeral_root(*symbol.numeral);
        if (!derived || derived->letter != symbol.root.letter ||
            derived->accidental != symbol.root.accidental)
            return false;
    } else if (symbol.roman) {
        return false;
    }
    if (!valid_musicxml_harmony_kind(symbol.quality) && !symbol.extensions.empty()) return false;
    return true;
}

void append_compact_harmony(pugi::xml_node measure, const MusicXmlHarmony& source, int divisions) {
    const auto& symbol = source.symbol;
    auto harmony = measure.append_child("harmony");
    if (symbol.numeral) {
        auto numeral = harmony.append_child("numeral");
        auto root = numeral.append_child("numeral-root");
        root.text().set(static_cast<unsigned>(symbol.numeral->root));
        if (symbol.roman) root.append_attribute("text") = symbol.roman->c_str();
        if (symbol.numeral->alteration != 0)
            numeral.append_child("numeral-alter")
                .text()
                .set(static_cast<int>(symbol.numeral->alteration));
        auto key = numeral.append_child("numeral-key");
        key.append_child("numeral-fifths").text().set(static_cast<int>(symbol.numeral->key.fifths));
        key.append_child("numeral-mode")
            .text()
            .set(chord_numeral_mode_to_musicxml(symbol.numeral->key.mode));
    } else {
        auto root = harmony.append_child("root");
        root.append_child("root-step")
            .text()
            .set(std::string(1, STEP_CHARS[symbol.root.letter]).c_str());
        if (symbol.root.accidental != 0)
            root.append_child("root-alter").text().set(static_cast<int>(symbol.root.accidental));
    }
    auto kind = harmony.append_child("kind");
    if (valid_musicxml_harmony_kind(symbol.quality) && symbol.quality != "other") {
        kind.text().set(symbol.quality.c_str());
        if (!symbol.extensions.empty())
            kind.append_attribute("text") = symbol.extensions.front().c_str();
    } else {
        kind.text().set("other");
        kind.append_attribute("text") = symbol.quality.c_str();
    }
    if (symbol.inversion)
        harmony.append_child("inversion").text().set(static_cast<unsigned>(*symbol.inversion));
    if (symbol.bass) {
        auto bass = harmony.append_child("bass");
        bass.append_child("bass-step")
            .text()
            .set(std::string(1, STEP_CHARS[symbol.bass->letter]).c_str());
        if (symbol.bass->accidental != 0)
            bass.append_child("bass-alter").text().set(static_cast<int>(symbol.bass->accidental));
    }
    for (const auto& source_degree : symbol.degrees) {
        auto degree = harmony.append_child("degree");
        degree.append_child("degree-value").text().set(static_cast<unsigned>(source_degree.value));
        degree.append_child("degree-alter").text().set(static_cast<int>(source_degree.alteration));
        degree.append_child("degree-type")
            .text()
            .set(chord_degree_type_to_musicxml(source_degree.type));
    }
    if (source.offset != sunny::core::Beat::zero())
        harmony.append_child("offset").text().set(
            *checked_beat_to_mxml_duration(source.offset, divisions));
}

} // anonymous namespace

// =============================================================================
// parse_musicxml
// =============================================================================

sunny::core::Result<MusicXmlScore> parse_musicxml(std::string_view xml) {
    pugi::xml_document doc;
    std::string xml_str(xml);
    auto parse_result = doc.load_string(xml_str.c_str());
    if (!parse_result) {
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }

    auto score_node = doc.child("score-partwise");
    if (!score_node) {
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
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
    if (!part_list) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    std::set<std::string> declared_part_ids;
    for (auto score_part : part_list.children("score-part")) {
        const std::string id = score_part.attribute("id").as_string();
        if (id.empty() || !score_part.child("part-name") || !declared_part_ids.insert(id).second)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }
    if (declared_part_ids.empty()) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    // Parse parts
    std::set<std::string> parsed_part_ids;
    for (auto part_node : score_node.children("part")) {
        MusicXmlPart part;
        part.id = part_node.attribute("id").as_string();
        if (!declared_part_ids.contains(part.id) || !parsed_part_ids.insert(part.id).second)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

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

        std::optional<int> divisions;

        for (auto measure_node : part_node.children("measure")) {
            MusicXmlMeasure measure;
            const auto measure_number = parse_xml_int(measure_node.attribute("number").value());
            if (!measure_number) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
            measure.number = *measure_number;

            // This compact representation is sequential. It cannot retain cursor rewinds or
            // advances, nor can its measure-level attributes retain mid-measure changes.
            if (measure_node.child("backup") || measure_node.child("forward") ||
                child_count(measure_node, "attributes") > 1)
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
            bool seen_note = false;
            for (auto child : measure_node.children()) {
                if (std::string_view{child.name()} == "note") seen_note = true;
                if (seen_note && std::string_view{child.name()} == "attributes")
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
            }

            // Attributes
            auto attrs = measure_node.child("attributes");
            if (attrs) {
                if (attrs.child("transpose") || attrs.child("for-part") || attrs.child("staves"))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                auto div_node = attrs.child("divisions");
                if (div_node) {
                    const auto d = parse_xml_int(div_node);
                    if (!d || *d <= 0)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    divisions = *d;
                }

                auto key_node = attrs.child("key");
                if (key_node) {
                    if (child_count(attrs, "key") > 1)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    const auto fifths_node = key_node.child("fifths");
                    // MusicXML's other legal branch is a sequence of
                    // key-step/key-alter pairs.  MusicXmlMeasure does not carry
                    // that spelling algebra, so reject it rather than treating
                    // a missing fifths node as zero.
                    if (!fifths_node)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    const auto fifths = parse_xml_int(fifths_node);
                    if (!fifths) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

                    auto mode_node = key_node.child("mode");
                    std::string mode;
                    if (mode_node) {
                        mode = trim_xml_token(mode_node.text().get());
                        if (!valid_musicxml_mode(mode))
                            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                        measure.key_mode = mode;
                    }
                    measure.key_fifths = *fifths;
                    if (const auto adjustment = mode_fifths_adjustment(mode)) {
                        measure.key_tonic =
                            sunny::core::from_line_of_fifths(*fifths - *adjustment, 4);
                    }
                    measure.key_is_major = major_minor_family(mode);
                }

                auto time_node = attrs.child("time");
                if (time_node) {
                    if (child_count(attrs, "time") > 1 || child_count(time_node, "beats") != 1 ||
                        child_count(time_node, "beat-type") != 1 ||
                        time_node.child("senza-misura") || time_node.child("interchangeable"))
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    const auto beats = parse_xml_int(time_node.child("beats"));
                    const auto beat_type = parse_xml_int(time_node.child("beat-type"));
                    if (!beats || !beat_type || *beats <= 0 || *beat_type <= 0)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    measure.time_signature = {*beats, *beat_type};
                }
            }

            // Harmony is a point event relative to the MusicXML cursor. The
            // compact profile has one sequential cursor and rejects explicit
            // backup/forward above, so this scan reconstructs exact local
            // offsets without inventing a voice or staff assignment.
            sunny::core::Beat harmony_cursor = sunny::core::Beat::zero();
            for (auto child : measure_node.children()) {
                const std::string_view name{child.name()};
                if (name == "harmony") {
                    if (!divisions) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    auto harmony = parse_harmony(child, harmony_cursor, *divisions);
                    if (!harmony) return std::unexpected(harmony.error());
                    measure.harmonies.push_back(std::move(*harmony));
                } else if (name == "note") {
                    if (!divisions) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    const auto units = parse_xml_int(child.child("duration"));
                    if (!units || *units <= 0)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    if (!child.child("chord")) {
                        const auto next = sunny::core::checked_add(
                            harmony_cursor, mxml_duration_to_beat(*units, *divisions));
                        if (!next) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                        harmony_cursor = *next;
                    }
                }
            }

            // Notes
            for (auto note_node : measure_node.children("note")) {
                MusicXmlNote note;

                if (note_node.child("grace") || note_node.child("cue") ||
                    note_node.child("unpitched") || note_node.child("staff"))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

                // Rest
                const bool has_rest = static_cast<bool>(note_node.child("rest"));
                const bool has_pitch = static_cast<bool>(note_node.child("pitch"));
                if (has_rest == has_pitch)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                if (has_rest) {
                    note.is_rest = true;
                } else {
                    // Pitch
                    auto pitch_node = note_node.child("pitch");
                    const auto step_text = trim_xml_token(pitch_node.child("step").text().get());
                    const int letter =
                        step_text.size() == 1 ? step_to_letter(step_text.front()) : -1;
                    const auto octave = parse_xml_int(pitch_node.child("octave"));
                    if (letter < 0 || !octave || *octave < 0 || *octave > 9)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

                    int alter = 0;
                    auto alter_node = pitch_node.child("alter");
                    if (alter_node) {
                        const auto parsed_alter = parse_xml_int(alter_node);
                        if (!parsed_alter ||
                            *parsed_alter < std::numeric_limits<std::int8_t>::min() ||
                            *parsed_alter > std::numeric_limits<std::int8_t>::max())
                            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                        alter = *parsed_alter;
                    }

                    note.pitch = sunny::core::SpelledPitch{static_cast<uint8_t>(letter),
                                                           static_cast<int8_t>(alter),
                                                           static_cast<int8_t>(*octave)};
                }

                // Chord flag
                if (note_node.child("chord")) {
                    note.is_chord = true;
                }

                // Voice
                auto voice_node = note_node.child("voice");
                if (voice_node) {
                    const auto voice = parse_xml_int(voice_node);
                    if (!voice || *voice < 1 || *voice > 256)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    note.voice = *voice;
                }

                // Duration
                auto dur_node = note_node.child("duration");
                const auto units = parse_xml_int(dur_node);
                if (!dur_node || !units || *units <= 0 || !divisions)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                note.duration = mxml_duration_to_beat(*units, *divisions);

                if (note.is_chord &&
                    (note.is_rest || measure.notes.empty() || measure.notes.back().is_rest ||
                     measure.notes.back().voice != note.voice ||
                     measure.notes.back().duration != note.duration))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

                measure.notes.push_back(note);
            }

            part.measures.push_back(std::move(measure));
        }

        if (part.measures.empty()) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        score.parts.push_back(std::move(part));
    }

    if (score.parts.size() != declared_part_ids.size())
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    return score;
}

// =============================================================================
// write_musicxml
// =============================================================================

sunny::core::Result<std::string> write_musicxml(const MusicXmlScore& score) {
    if (score.parts.empty()) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    std::set<std::string> part_ids;
    for (const auto& part : score.parts) {
        if (part.id.empty() || part.measures.empty() || !part_ids.insert(part.id).second)
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    }

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

    // Compute exact integer divisions only after the structural envelope has
    // been checked. The helper rejects malformed Beats and LCM overflow.
    const auto computed_divisions = compute_divisions(score);
    if (!computed_divisions) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    const int divisions = *computed_divisions;

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
            const bool has_key = measure.key_fifths.has_value() || measure.key_tonic.has_value();
            if (!has_key && (measure.key_mode.has_value() || measure.key_is_major.has_value()))
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

            bool need_attrs = measure.time_signature.has_value() || has_key || measure.number == 1;
            if (need_attrs) {
                auto attrs = m.append_child("attributes");
                attrs.append_child("divisions").text().set(divisions);

                if (has_key) {
                    auto key = attrs.append_child("key");
                    std::optional<std::string> mode = measure.key_mode;
                    if (mode && !valid_musicxml_mode(*mode))
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    if (!measure.key_fifths && !mode) {
                        mode = measure.key_is_major.value_or(true) ? "major" : "minor";
                    }
                    if (mode && measure.key_is_major) {
                        const auto family = major_minor_family(*mode);
                        if (!family || *family != *measure.key_is_major)
                            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    }

                    int fifths = 0;
                    if (measure.key_fifths) {
                        fifths = *measure.key_fifths;
                    } else {
                        const auto adjustment = mode_fifths_adjustment(mode.value_or("major"));
                        if (!adjustment || !measure.key_tonic)
                            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                        fifths =
                            sunny::core::line_of_fifths_position(*measure.key_tonic) + *adjustment;
                    }

                    if (measure.key_tonic) {
                        const auto adjustment = mode_fifths_adjustment(mode.value_or("major"));
                        if (adjustment && sunny::core::line_of_fifths_position(*measure.key_tonic) +
                                                  *adjustment !=
                                              fifths)
                            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    }
                    key.append_child("fifths").text().set(fifths);
                    if (mode) key.append_child("mode").text().set(mode->c_str());
                }

                if (measure.time_signature.has_value()) {
                    if (measure.time_signature->first <= 0 || measure.time_signature->second <= 0)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                    auto time = attrs.append_child("time");
                    time.append_child("beats").text().set(measure.time_signature->first);
                    time.append_child("beat-type").text().set(measure.time_signature->second);
                }
            }

            // Compact harmonies are emitted before sequential notes, with an
            // absolute non-negative offset from the measure origin.
            for (const auto& harmony : measure.harmonies) {
                if (!valid_compact_harmony(harmony))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                append_compact_harmony(m, harmony, divisions);
            }

            // Notes
            for (std::size_t note_index = 0; note_index < measure.notes.size(); ++note_index) {
                const auto& note = measure.notes[note_index];
                if (note.duration <= sunny::core::Beat::zero() || note.voice < 1 ||
                    note.voice > 256 ||
                    (!note.is_rest &&
                     (note.pitch.letter > 6 || note.pitch.octave < 0 || note.pitch.octave > 9)))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                if (note.is_chord &&
                    (note.is_rest || note_index == 0 || measure.notes[note_index - 1].is_rest ||
                     measure.notes[note_index - 1].voice != note.voice ||
                     measure.notes[note_index - 1].duration != note.duration))
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
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
                    pitch.append_child("octave").text().set(static_cast<int>(note.pitch.octave));
                }

                // Duration
                const auto duration_units = checked_beat_to_mxml_duration(note.duration, divisions);
                if (!duration_units)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                n.append_child("duration").text().set(*duration_units);

                // Voice
                n.append_child("voice").text().set(note.voice);

                // Graphic duration is optional in MusicXML and has a finite
                // vocabulary independent of sounding duration. Omit it for an
                // arbitrary rational rather than inventing a quarter-note glyph.
                if (const auto graphic = musicxml_graphic_duration(note.duration)) {
                    n.append_child("type").text().set(graphic->type.data());
                    for (int dot = 0; dot < graphic->dots; ++dot)
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

sunny::core::Result<std::vector<sunny::core::NoteEvent>>
musicxml_to_note_events(const MusicXmlScore& score) {
    std::vector<sunny::core::NoteEvent> result;
    if (score.parts.size() != 1 || !write_musicxml(score))
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    const auto& part = score.parts[0];
    sunny::core::Beat current_time = sunny::core::Beat::zero();
    std::optional<sunny::core::Beat> chord_anchor;

    for (const auto& measure : part.measures) {
        for (const auto& note : measure.notes) {
            if (note.is_chord) {
                if (!chord_anchor) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                const auto midi_value = sunny::core::midi(note.pitch);
                if (!midi_value) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                result.push_back({*midi_value, *chord_anchor, note.duration, 80, false});
            } else if (note.is_rest) {
                result.push_back({sunny::core::MidiNote{}, current_time, note.duration, 0, true});
                const auto end = sunny::core::checked_add(current_time, note.duration);
                if (!end) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                current_time = *end;
                chord_anchor.reset();
            } else {
                const auto midi_value = sunny::core::midi(note.pitch);
                if (!midi_value) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                chord_anchor = current_time;
                result.push_back({*midi_value, current_time, note.duration, 80, false});
                const auto end = sunny::core::checked_add(current_time, note.duration);
                if (!end) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
                current_time = *end;
            }
        }
    }

    return result;
}

sunny::core::Result<MusicXmlScore>
note_events_to_musicxml(std::span<const sunny::core::NoteEvent> events, int key_lof) {
    // from_line_of_fifths and default_spelling store accidentals in int8_t and
    // search six fifth-steps around the key centre.
    if (key_lof < -891 || key_lof > 888)
        return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

    MusicXmlScore score;
    score.title = "";

    MusicXmlPart part;
    part.id = "P1";
    part.name = "Part 1";

    // Simple: put all events in a single measure
    MusicXmlMeasure measure;
    measure.number = 1;

    auto tonic = sunny::core::from_line_of_fifths(key_lof, 4);
    measure.key_tonic = tonic;
    measure.key_is_major = true;
    measure.key_fifths = key_lof;
    measure.key_mode = "major";

    sunny::core::Beat cursor = sunny::core::Beat::zero();
    std::optional<sunny::core::Beat> group_start;
    std::optional<sunny::core::Beat> group_duration;
    bool group_is_pitched = false;

    for (const auto& ev : events) {
        if (ev.start_time < sunny::core::Beat::zero() || ev.duration <= sunny::core::Beat::zero() ||
            (ev.muted ? ev.velocity != 0 : ev.velocity != 80))
            return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        if (ev.release_velocity != 64)
            return std::unexpected(sunny::core::ErrorCode::TargetValueUnrepresentable);

        if (group_start && ev.start_time == *group_start) {
            if (ev.muted || !group_is_pitched || ev.duration != *group_duration)
                return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);

            const auto octave = static_cast<std::int8_t>(ev.pitch / 12 - 1);
            const auto pitch_class = sunny::core::pitch_class(ev.pitch);
            const auto spelling = sunny::core::default_spelling(pitch_class, key_lof, octave);
            measure.notes.push_back({spelling, ev.duration, false, true, 1});
            continue;
        }

        if (ev.start_time < cursor) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        if (ev.start_time > cursor) {
            const auto gap = sunny::core::checked_sub(ev.start_time, cursor);
            if (!gap) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
            measure.notes.push_back({sunny::core::SpelledPitch{}, *gap, true, false, 1});
        }

        MusicXmlNote note;
        note.duration = ev.duration;
        note.is_rest = ev.muted;
        note.is_chord = false;
        note.voice = 1;
        if (!ev.muted) {
            const auto octave = static_cast<std::int8_t>(ev.pitch / 12 - 1);
            const auto pitch_class = sunny::core::pitch_class(ev.pitch);
            note.pitch = sunny::core::default_spelling(pitch_class, key_lof, octave);
        }
        measure.notes.push_back(note);

        const auto end = sunny::core::checked_add(ev.start_time, ev.duration);
        if (!end) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
        cursor = *end;
        group_start = ev.start_time;
        group_duration = ev.duration;
        group_is_pitched = !ev.muted;
    }

    part.measures.push_back(std::move(measure));
    score.parts.push_back(std::move(part));
    if (!write_musicxml(score)) return std::unexpected(sunny::core::ErrorCode::InvalidMusicXml);
    return score;
}

} // namespace sunny::infrastructure::formats
