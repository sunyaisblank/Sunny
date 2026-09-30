/**
 * @file serialization.cpp
 * @brief Score IR serialisation — implementation
 *
 */

#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/serialization_primitives.hpp>
#include <sunny/core/score/validation.hpp>

namespace sunny::core {

using json = nlohmann::json;

// =============================================================================
// SpelledPitch serialisation helpers
// =============================================================================

namespace {

template <typename EnumT>
EnumT check_enum_range(const json& value, EnumT max_val, const char* name, const json& j) {
    const auto val = detail::checked_integer<int>(value, name);
    if (val < 0 || val > static_cast<int>(max_val))
        throw json::other_error::create(604, std::string(name) + " out of range", &j);
    return static_cast<EnumT>(val);
}

json spelled_pitch_to_json(const SpelledPitch& sp) {
    return json{{"letter", static_cast<int>(sp.letter)},
                {"accidental", sp.accidental},
                {"octave", sp.octave}};
}

SpelledPitch spelled_pitch_from_json(const json& j) {
    const auto letter_val = detail::checked_integer<std::uint8_t>(j.at("letter"), "pitch letter");
    if (letter_val > 6) {
        throw json::other_error::create(602, "SpelledPitch letter must be in [0, 6]", &j);
    }
    return SpelledPitch{
        letter_val,
        detail::checked_integer<std::int8_t>(j.at("accidental"), "pitch accidental"),
        detail::checked_integer<std::int8_t>(j.at("octave"), "pitch octave")};
}

json velocity_value_to_json(const VelocityValue& v) {
    json j;
    if (v.written) j["written"] = static_cast<int>(*v.written);
    j["value"] = v.value;
    return j;
}

VelocityValue velocity_value_from_json(const json& j) {
    VelocityValue v;
    if (j.contains("written")) {
        v.written = check_enum_range(j.at("written"), DynamicLevel::rfz, "DynamicLevel", j);
    }
    v.value = detail::checked_integer<std::uint8_t>(j.at("value"), "velocity value");
    return v;
}

// =============================================================================
// Ornament serialisation
// =============================================================================

json ornament_to_json(const Ornament& o) {
    json j;
    j["type"] = static_cast<int>(o.type);
    j["trill_interval"] = o.trill_interval;
    if (o.accidental) j["accidental"] = *o.accidental;
    j["arpeggio_direction"] = static_cast<int>(o.arpeggio_direction);
    return j;
}

Ornament ornament_from_json(const json& j) {
    Ornament o;
    o.type = check_enum_range(j.at("type"), OrnamentType::Arpeggio, "OrnamentType", j);
    o.trill_interval = j.value("trill_interval", static_cast<Interval>(0));
    if (j.contains("accidental"))
        o.accidental =
            detail::checked_integer<std::int8_t>(j.at("accidental"), "ornament accidental");
    o.arpeggio_direction =
        j.contains("arpeggio_direction")
            ? check_enum_range(
                  j.at("arpeggio_direction"), ArpeggioDirection::None, "ArpeggioDirection", j)
            : ArpeggioDirection::None;
    return o;
}

// =============================================================================
// TechnicalDirection serialisation
// =============================================================================

json technical_to_json(const TechnicalDirection& td) {
    json j;
    j["type"] = static_cast<int>(td.type);
    if (!td.fingering.empty()) j["fingering"] = td.fingering;
    if (td.number != 0) j["number"] = td.number;
    if (!td.pattern.empty()) j["pattern"] = td.pattern;
    j["slide_direction"] = static_cast<int>(td.slide_direction);
    if (td.bend_cents != 0) j["bend_cents"] = td.bend_cents;
    j["vibrato_speed"] = static_cast<int>(td.vibrato_speed);
    return j;
}

TechnicalDirection technical_from_json(const json& j) {
    TechnicalDirection td;
    td.type = check_enum_range(
        j.at("type"), TechnicalDirection::Type::Caesura, "TechnicalDirection::Type", j);
    if (j.contains("fingering")) {
        for (const auto& value : j.at("fingering"))
            td.fingering.push_back(detail::checked_integer<std::uint8_t>(value, "fingering value"));
    }
    td.number = detail::checked_integer_or<std::uint8_t>(j, "number", 0, "technical number");
    if (j.contains("pattern")) td.pattern = j.at("pattern").get<std::string>();
    td.slide_direction =
        j.contains("slide_direction")
            ? check_enum_range(
                  j.at("slide_direction"), SlideDirection::Descending, "SlideDirection", j)
            : SlideDirection::Into;
    td.bend_cents = detail::checked_integer_or<std::int16_t>(j, "bend_cents", 0, "bend cents");
    td.vibrato_speed =
        j.contains("vibrato_speed")
            ? check_enum_range(j.at("vibrato_speed"), VibratoSpeed::None, "VibratoSpeed", j)
            : VibratoSpeed::Normal;
    return td;
}

// =============================================================================
// Note serialisation
// =============================================================================

json lyric_syllable_to_json(const LyricSyllable& lyric) {
    return json{{"text", lyric.text},
                {"verse", lyric.verse},
                {"syllabic", static_cast<int>(lyric.syllabic)},
                {"extend", lyric.extend}};
}

LyricSyllable lyric_syllable_from_json(const json& j) {
    LyricSyllable lyric;
    lyric.text = j.at("text").get<std::string>();
    lyric.verse = detail::checked_integer_or<std::uint16_t>(j, "verse", 1, "lyric verse");
    lyric.syllabic =
        j.contains("syllabic")
            ? check_enum_range(j.at("syllabic"), LyricSyllabic::End, "LyricSyllabic", j)
            : LyricSyllabic::Single;
    lyric.extend = j.value("extend", false);
    return lyric;
}

json note_to_json(const Note& note) {
    json j;
    j["pitch"] = spelled_pitch_to_json(note.pitch);
    j["velocity"] = velocity_value_to_json(note.velocity);
    j["release_velocity"] = note.release_velocity;
    if (note.articulation) j["articulation"] = static_cast<int>(*note.articulation);
    if (note.dynamic) j["dynamic"] = static_cast<int>(*note.dynamic);
    if (note.ornament) j["ornament"] = ornament_to_json(*note.ornament);
    if (note.tie_forward) j["tie_forward"] = true;
    if (note.grace) j["grace"] = static_cast<int>(*note.grace);
    if (!note.technical.empty()) {
        json techs = json::array();
        for (const auto& td : note.technical) {
            techs.push_back(technical_to_json(td));
        }
        j["technical"] = techs;
    }
    if (!note.lyrics.empty()) {
        j["lyrics"] = json::array();
        for (const auto& lyric : note.lyrics)
            j["lyrics"].push_back(lyric_syllable_to_json(lyric));
    }
    if (note.notation_head) j["notation_head"] = static_cast<int>(*note.notation_head);
    return j;
}

Note note_from_json(const json& j, int schema_version) {
    Note note;
    note.pitch = spelled_pitch_from_json(j.at("pitch"));
    note.velocity = velocity_value_from_json(j.at("velocity"));
    // Schema 7 makes the field mandatory.  Preserve it when encountered in a
    // legacy-labelled document as well, matching the forward-field migration
    // policy used by other formerly optional Score fields.
    if (schema_version >= 7 || j.contains("release_velocity")) {
        note.release_velocity =
            detail::checked_integer<std::uint8_t>(j.at("release_velocity"), "release velocity");
        if (note.release_velocity > 127)
            throw json::other_error::create(604, "release velocity exceeds 127", &j);
    }
    if (j.contains("articulation")) {
        note.articulation = check_enum_range(
            j.at("articulation"), ArticulationType::BendDown, "ArticulationType", j);
    }
    if (j.contains("dynamic")) {
        note.dynamic = check_enum_range(j.at("dynamic"), DynamicLevel::rfz, "DynamicLevel", j);
    }
    if (j.contains("ornament")) note.ornament = ornament_from_json(j.at("ornament"));
    note.tie_forward = j.value("tie_forward", false);
    if (j.contains("grace")) {
        note.grace = check_enum_range(j.at("grace"), GraceType::Appoggiatura, "GraceType", j);
    }
    if (j.contains("technical")) {
        for (const auto& tj : j.at("technical")) {
            note.technical.push_back(technical_from_json(tj));
        }
    }
    if (j.contains("lyrics")) {
        for (const auto& lyric : j.at("lyrics"))
            note.lyrics.push_back(lyric_syllable_from_json(lyric));
    } else if (j.contains("lyric")) {
        // Schema versions 1-3 stored one unstructured syllable string.
        note.lyrics.push_back(
            LyricSyllable{j.at("lyric").get<std::string>(), 1, LyricSyllabic::Single, false});
    }
    if (j.contains("notation_head")) {
        note.notation_head =
            check_enum_range(j.at("notation_head"), NoteHeadType::Cue, "NoteHeadType", j);
    }
    return note;
}

// =============================================================================
// TupletContext serialisation
// =============================================================================

json tuplet_context_to_json(const TupletContext& tc) {
    json j;
    j["id"] = tc.id.value;
    j["actual"] = tc.actual;
    j["normal"] = tc.normal;
    j["normal_type"] = beat_to_json(tc.normal_type);
    if (tc.nested_in) j["nested_in"] = tc.nested_in->value;
    return j;
}

TupletContext tuplet_context_from_json(const json& j) {
    TupletContext tc;
    tc.id = TupletId{detail::checked_integer<std::uint64_t>(j.at("id"), "tuplet id")};
    tc.actual = detail::checked_integer<std::uint8_t>(j.at("actual"), "tuplet actual");
    tc.normal = detail::checked_integer<std::uint8_t>(j.at("normal"), "tuplet normal");
    tc.normal_type = beat_from_json(j.at("normal_type"));
    if (j.contains("nested_in"))
        tc.nested_in =
            TupletId{detail::checked_integer<std::uint64_t>(j.at("nested_in"), "parent tuplet id")};
    return tc;
}

// =============================================================================
// Event serialisation
// =============================================================================

json event_to_json(const Event& event) {
    json j;
    j["id"] = event.id.value;
    j["offset"] = beat_to_json(event.offset);

    if (auto* ng = std::get_if<NoteGroup>(&event.payload)) {
        j["type"] = "note_group";
        json notes = json::array();
        for (const auto& note : ng->notes) {
            notes.push_back(note_to_json(note));
        }
        j["notes"] = notes;
        j["duration"] = beat_to_json(ng->duration);
        if (ng->tuplet_context) j["tuplet_context"] = tuplet_context_to_json(*ng->tuplet_context);
        if (ng->beam_group) j["beam_group"] = ng->beam_group->value;
        if (ng->slur_start) j["slur_start"] = true;
        if (ng->slur_end) j["slur_end"] = true;
    } else if (auto* r = std::get_if<RestEvent>(&event.payload)) {
        j["type"] = "rest";
        j["duration"] = beat_to_json(r->duration);
        j["visible"] = r->visible;
        if (r->tuplet_context) j["tuplet_context"] = tuplet_context_to_json(*r->tuplet_context);
    } else if (auto* cs = std::get_if<ChordSymbolEvent>(&event.payload)) {
        j["type"] = "chord_symbol";
        j["root"] = spelled_pitch_to_json(cs->root);
        j["quality"] = cs->quality;
        if (cs->bass) j["bass"] = spelled_pitch_to_json(*cs->bass);
        if (cs->roman) j["roman"] = *cs->roman;
        if (!cs->extensions.empty()) j["extensions"] = cs->extensions;
        if (cs->numeral) {
            j["numeral"] = {{"root", cs->numeral->root},
                            {"alteration", cs->numeral->alteration},
                            {"key",
                             {{"fifths", cs->numeral->key.fifths},
                              {"mode", static_cast<int>(cs->numeral->key.mode)}}}};
        }
        if (cs->inversion) j["inversion"] = *cs->inversion;
        if (!cs->degrees.empty()) {
            j["degrees"] = json::array();
            for (const auto& degree : cs->degrees) {
                j["degrees"].push_back({{"value", degree.value},
                                        {"alteration", degree.alteration},
                                        {"type", static_cast<int>(degree.type)}});
            }
        }
    } else if (auto* d = std::get_if<ScoreDirection>(&event.payload)) {
        j["type"] = "direction";
        j["direction_type"] = static_cast<int>(d->type);
        if (d->text) j["text"] = *d->text;
        if (d->new_clef) j["new_clef"] = static_cast<int>(*d->new_clef);
        if (d->ottava_shift != 0) j["ottava_shift"] = d->ottava_shift;
    }

    return j;
}

Event event_from_json(const json& j, int schema_version) {
    Event event;
    event.id = EventId{detail::checked_integer<std::uint64_t>(j.at("id"), "event id")};
    event.offset = beat_from_json(j.at("offset"));

    std::string type = j.at("type").get<std::string>();

    if (type == "note_group") {
        NoteGroup ng;
        for (const auto& nj : j.at("notes")) {
            ng.notes.push_back(note_from_json(nj, schema_version));
        }
        ng.duration = beat_from_json(j.at("duration"));
        if (j.contains("tuplet_context"))
            ng.tuplet_context = tuplet_context_from_json(j.at("tuplet_context"));
        if (j.contains("beam_group"))
            ng.beam_group = BeamGroupId{
                detail::checked_integer<std::uint64_t>(j.at("beam_group"), "beam group id")};
        ng.slur_start = j.value("slur_start", false);
        ng.slur_end = j.value("slur_end", false);
        event.payload = std::move(ng);
    } else if (type == "rest") {
        RestEvent r;
        r.duration = beat_from_json(j.at("duration"));
        r.visible = j.value("visible", true);
        if (j.contains("tuplet_context"))
            r.tuplet_context = tuplet_context_from_json(j.at("tuplet_context"));
        event.payload = r;
    } else if (type == "chord_symbol") {
        ChordSymbolEvent cs;
        cs.root = spelled_pitch_from_json(j.at("root"));
        cs.quality = j.at("quality").get<std::string>();
        if (j.contains("bass")) cs.bass = spelled_pitch_from_json(j.at("bass"));
        if (j.contains("roman")) cs.roman = j.at("roman").get<std::string>();
        if (j.contains("extensions"))
            cs.extensions = j.at("extensions").get<std::vector<std::string>>();
        if (j.contains("numeral")) {
            const auto& numeral = j.at("numeral");
            const auto& key = numeral.at("key");
            cs.numeral = ChordNumeral{
                detail::checked_integer<std::uint8_t>(numeral.at("root"), "chord numeral root"),
                detail::checked_integer<std::int8_t>(numeral.at("alteration"),
                                                     "chord numeral alteration"),
                ChordNumeralKey{
                    detail::checked_integer<std::int8_t>(key.at("fifths"),
                                                         "chord numeral key fifths"),
                    check_enum_range(
                        key.at("mode"), ChordNumeralMode::HarmonicMinor, "ChordNumeralMode", key)}};
        }
        if (j.contains("inversion"))
            cs.inversion =
                detail::checked_integer<std::uint16_t>(j.at("inversion"), "chord inversion");
        if (j.contains("degrees")) {
            for (const auto& degree : j.at("degrees")) {
                cs.degrees.push_back(ChordDegree{
                    detail::checked_integer<std::uint16_t>(degree.at("value"),
                                                           "chord degree value"),
                    detail::checked_integer<std::int8_t>(degree.at("alteration"),
                                                         "chord degree alteration"),
                    check_enum_range(
                        degree.at("type"), ChordDegreeType::Subtract, "ChordDegreeType", degree)});
            }
        }
        event.payload = std::move(cs);
    } else if (type == "direction") {
        ScoreDirection d;
        d.type =
            check_enum_range(j.at("direction_type"), DirectionType::Caesura, "DirectionType", j);
        if (j.contains("text")) d.text = j.at("text").get<std::string>();
        if (j.contains("new_clef")) {
            d.new_clef = check_enum_range(j.at("new_clef"), Clef::Tab, "Clef", j);
        }
        d.ottava_shift =
            detail::checked_integer_or<std::int8_t>(j, "ottava_shift", 0, "ottava shift");
        event.payload = std::move(d);
    } else {
        throw json::other_error::create(603, "Unrecognised event type: " + type, &j);
    }

    return event;
}

// =============================================================================
// Voice / Measure / Part serialisation
// =============================================================================

json beam_group_to_json(const BeamGroup& bg) {
    json j;
    j["id"] = bg.id.value;
    json ids = json::array();
    for (const auto& eid : bg.event_ids)
        ids.push_back(eid.value);
    j["event_ids"] = ids;
    if (!bg.beam_breaks.empty()) j["beam_breaks"] = bg.beam_breaks;
    return j;
}

BeamGroup beam_group_from_json(const json& j) {
    BeamGroup bg;
    bg.id = BeamGroupId{detail::checked_integer<std::uint64_t>(j.at("id"), "beam group id")};
    for (const auto& ej : j.at("event_ids")) {
        bg.event_ids.push_back(
            EventId{detail::checked_integer<std::uint64_t>(ej, "beam event id")});
    }
    if (j.contains("beam_breaks")) {
        for (const auto& value : j.at("beam_breaks"))
            bg.beam_breaks.push_back(
                detail::checked_integer<std::uint8_t>(value, "beam break index"));
    }
    return bg;
}

json voice_to_json(const Voice& voice) {
    json j;
    j["voice_index"] = voice.voice_index;
    j["staff_index"] = voice.staff_index;
    json events = json::array();
    for (const auto& event : voice.events) {
        events.push_back(event_to_json(event));
    }
    j["events"] = events;
    if (!voice.beam_groups.empty()) {
        json bgs = json::array();
        for (const auto& bg : voice.beam_groups) {
            bgs.push_back(beam_group_to_json(bg));
        }
        j["beam_groups"] = bgs;
    }
    return j;
}

Voice voice_from_json(const json& j, int schema_version) {
    Voice voice;
    voice.voice_index = detail::checked_integer<std::uint8_t>(j.at("voice_index"), "voice index");
    voice.staff_index =
        detail::checked_integer_or<std::uint8_t>(j, "staff_index", 0, "staff index");
    for (const auto& ej : j.at("events")) {
        voice.events.push_back(event_from_json(ej, schema_version));
    }
    if (j.contains("beam_groups")) {
        for (const auto& bj : j.at("beam_groups")) {
            voice.beam_groups.push_back(beam_group_from_json(bj));
        }
    }
    return voice;
}

json key_signature_to_json(const KeySignature& ks) {
    json j;
    j["root"] = spelled_pitch_to_json(ks.root);
    j["accidentals"] = ks.accidentals;
    // Serialise mode intervals
    json mode_arr = json::array();
    for (auto iv : ks.mode.get_intervals()) {
        mode_arr.push_back(static_cast<int>(iv));
    }
    j["mode_intervals"] = mode_arr;
    j["mode_note_count"] = ks.mode.note_count;
    if (!ks.mode.name.empty()) j["mode_name"] = ks.mode.name;
    if (!ks.mode.description.empty()) j["mode_description"] = ks.mode.description;
    return j;
}

KeySignature key_signature_from_json(const json& j, int schema_version) {
    KeySignature ks;
    ks.root = spelled_pitch_from_json(j.at("root"));
    ks.accidentals =
        detail::checked_integer<std::int8_t>(j.at("accidentals"), "key signature accidentals");
    if (j.contains("mode_intervals")) {
        if (!j.at("mode_intervals").is_array() || j.at("mode_intervals").size() > 12) {
            throw json::other_error::create(
                604, "mode_intervals must be an array of at most 12 values", &j);
        }
        ks.mode = {};
        ks.mode.intervals = {};
        std::uint8_t idx = 0;
        for (const auto& iv : j.at("mode_intervals")) {
            ks.mode.intervals[idx++] = detail::checked_integer<int>(iv, "mode interval");
        }
        ks.mode.note_count =
            detail::checked_integer_or<std::uint8_t>(j, "mode_note_count", idx, "mode note count");
        if (j.contains("mode_name")) ks.mode.name = j.at("mode_name").get<std::string>();
        if (j.contains("mode_description"))
            ks.mode.description = j.at("mode_description").get<std::string>();
    } else if (schema_version >= 4) {
        throw json::other_error::create(604, "schema-v4+ key requires mode_intervals", &j);
    }
    return ks;
}

json measure_to_json(const Measure& measure) {
    json j;
    j["bar_number"] = measure.bar_number;
    json voices = json::array();
    for (const auto& voice : measure.voices) {
        voices.push_back(voice_to_json(voice));
    }
    j["voices"] = voices;
    if (measure.local_key) j["local_key"] = key_signature_to_json(*measure.local_key);
    if (measure.local_time) {
        json lt;
        lt["groups"] = measure.local_time->groups();
        lt["denominator"] = measure.local_time->denominator();
        j["local_time"] = lt;
    }
    return j;
}

Measure measure_from_json(const json& j, int schema_version) {
    Measure measure;
    measure.bar_number =
        detail::checked_integer<std::uint32_t>(j.at("bar_number"), "measure bar number");
    for (const auto& vj : j.at("voices")) {
        measure.voices.push_back(voice_from_json(vj, schema_version));
    }
    if (j.contains("local_key"))
        measure.local_key = key_signature_from_json(j.at("local_key"), schema_version);
    if (j.contains("local_time")) {
        const auto& encoded = j.at("local_time");
        auto time_signature = TimeSignature::from_groups(
            encoded.at("groups").get<std::vector<int>>(),
            detail::checked_integer<int>(encoded.at("denominator"), "time-signature denominator"));
        if (!time_signature)
            throw json::other_error::create(604, "Invalid local time signature", &encoded);
        measure.local_time = std::move(*time_signature);
    }
    return measure;
}

// =============================================================================
// ArticulationMapping serialisation (recursive for Combined type)
// =============================================================================

json articulation_mapping_to_json_impl(const ArticulationMapping& am) {
    json j{{"type", static_cast<int>(am.type)}};
    switch (am.type) {
    case ArticulationMapping::Type::Keyswitch:
        j["keyswitch_pitch"] = spelled_pitch_to_json(am.keyswitch_pitch);
        break;
    case ArticulationMapping::Type::CC:
        j["cc_number"] = am.cc_number;
        j["cc_value"] = am.cc_value;
        break;
    case ArticulationMapping::Type::VelocityLayer:
        j["velocity_min"] = am.velocity_min;
        j["velocity_max"] = am.velocity_max;
        break;
    case ArticulationMapping::Type::NoteDurationScale:
        j["duration_scale"] = am.duration_scale;
        break;
    case ArticulationMapping::Type::ProgramChange:
        j["program"] = am.program;
        break;
    case ArticulationMapping::Type::Combined: {
        json arr = json::array();
        for (const auto& sub : am.combined) {
            arr.push_back(articulation_mapping_to_json_impl(sub));
        }
        j["combined"] = arr;
        break;
    }
    }
    return j;
}

ArticulationMapping
articulation_mapping_from_json_impl(const json& j, std::size_t depth, std::size_t& nodes) {
    if (!j.is_object() || depth > 16 || ++nodes > 1024)
        throw json::other_error::create(605, "ArticulationMapping nesting limit exceeded", &j);
    ArticulationMapping am;
    am.type = check_enum_range(
        j.at("type"), ArticulationMapping::Type::Combined, "ArticulationMapping::Type", j);
    if (am.type == ArticulationMapping::Type::Keyswitch) {
        if (!j.contains("keyswitch_pitch"))
            throw json::other_error::create(605, "Keyswitch mapping requires a pitch", &j);
        am.keyswitch_pitch = spelled_pitch_from_json(j.at("keyswitch_pitch"));
    } else if (am.type == ArticulationMapping::Type::CC) {
        am.cc_number =
            detail::checked_integer<std::uint8_t>(j.at("cc_number"), "articulation CC number");
        am.cc_value =
            detail::checked_integer<std::uint8_t>(j.at("cc_value"), "articulation CC value");
    } else if (am.type == ArticulationMapping::Type::VelocityLayer) {
        am.velocity_min = detail::checked_integer<std::uint8_t>(j.at("velocity_min"),
                                                                "articulation minimum velocity");
        am.velocity_max = detail::checked_integer<std::uint8_t>(j.at("velocity_max"),
                                                                "articulation maximum velocity");
    } else if (am.type == ArticulationMapping::Type::NoteDurationScale) {
        am.duration_scale = j.at("duration_scale").get<float>();
    } else if (am.type == ArticulationMapping::Type::ProgramChange) {
        am.program = detail::checked_integer<std::uint8_t>(j.at("program"), "articulation program");
    } else {
        if (!j.contains("combined") || !j.at("combined").is_array() || j.at("combined").empty())
            throw json::other_error::create(605, "Combined mapping requires children", &j);
        for (const auto& sub : j.at("combined")) {
            am.combined.push_back(articulation_mapping_from_json_impl(sub, depth + 1, nodes));
        }
    }
    if (!validate_articulation_mapping(am))
        throw json::other_error::create(605, "Invalid ArticulationMapping value", &j);
    return am;
}

json part_definition_to_json(const PartDefinition& def) {
    json j;
    j["name"] = def.name;
    j["abbreviation"] = def.abbreviation;
    j["instrument_type"] = static_cast<int>(def.instrument_type);
    j["transposition"] = def.transposition;
    j["clef"] = static_cast<int>(def.clef);
    j["staff_count"] = def.staff_count;
    if (!def.staff_clefs.empty()) {
        j["staff_clefs"] = json::array();
        for (const auto clef : def.staff_clefs)
            j["staff_clefs"].push_back(static_cast<int>(clef));
    }
    if (def.custom_descriptor) j["custom_descriptor"] = *def.custom_descriptor;

    // Range
    json range;
    range["absolute_low"] = spelled_pitch_to_json(def.range.absolute_low);
    range["absolute_high"] = spelled_pitch_to_json(def.range.absolute_high);
    range["comfortable_low"] = spelled_pitch_to_json(def.range.comfortable_low);
    range["comfortable_high"] = spelled_pitch_to_json(def.range.comfortable_high);
    j["range"] = range;

    // Articulation vocabulary
    if (!def.articulation_vocabulary.empty()) {
        json vocab = json::array();
        for (auto art : def.articulation_vocabulary) {
            vocab.push_back(static_cast<int>(art));
        }
        j["articulation_vocabulary"] = vocab;
    }

    // Rendering config
    json rc;
    rc["midi_channel"] = def.rendering.midi_channel;
    if (def.rendering.instrument_preset) rc["instrument_preset"] = *def.rendering.instrument_preset;
    rc["expression_cc"] = def.rendering.expression_cc;
    if (def.rendering.pan) rc["pan"] = *def.rendering.pan;
    if (def.rendering.group) rc["group"] = *def.rendering.group;
    if (!def.rendering.articulation_map.empty()) {
        json am_obj = json::object();
        for (const auto& [art, mapping] : def.rendering.articulation_map) {
            am_obj[std::to_string(static_cast<int>(art))] =
                articulation_mapping_to_json_impl(mapping);
        }
        rc["articulation_map"] = am_obj;
    }
    j["rendering"] = rc;

    // Backward compat: keep flat midi_channel/instrument_preset
    j["midi_channel"] = def.rendering.midi_channel;
    if (def.rendering.instrument_preset) j["instrument_preset"] = *def.rendering.instrument_preset;
    return j;
}

PartDefinition part_definition_from_json(const json& j) {
    PartDefinition def;
    def.name = j.at("name").get<std::string>();
    def.abbreviation = j.at("abbreviation").get<std::string>();
    def.instrument_type =
        check_enum_range(j.at("instrument_type"), InstrumentType::Custom, "InstrumentType", j);
    def.transposition = j.value("transposition", static_cast<Interval>(0));
    def.clef =
        j.contains("clef") ? check_enum_range(j.at("clef"), Clef::Tab, "Clef", j) : Clef::Treble;
    def.staff_count = detail::checked_integer_or<std::uint8_t>(j, "staff_count", 1, "staff count");
    if (j.contains("staff_clefs")) {
        if (!j.at("staff_clefs").is_array())
            throw json::other_error::create(605, "staff_clefs must be an array", &j);
        for (const auto& clef : j.at("staff_clefs"))
            def.staff_clefs.push_back(check_enum_range(clef, Clef::Tab, "Clef", j));
    }
    if (j.contains("custom_descriptor"))
        def.custom_descriptor = j.at("custom_descriptor").get<std::string>();

    // Range
    if (j.contains("range")) {
        const auto& rj = j.at("range");
        def.range.absolute_low = spelled_pitch_from_json(rj.at("absolute_low"));
        def.range.absolute_high = spelled_pitch_from_json(rj.at("absolute_high"));
        def.range.comfortable_low = spelled_pitch_from_json(rj.at("comfortable_low"));
        def.range.comfortable_high = spelled_pitch_from_json(rj.at("comfortable_high"));
    }

    // Articulation vocabulary
    if (j.contains("articulation_vocabulary")) {
        for (const auto& av : j.at("articulation_vocabulary")) {
            def.articulation_vocabulary.push_back(
                check_enum_range(av, ArticulationType::BendDown, "ArticulationType", av));
        }
    }

    // Rendering config — prefer nested "rendering" object, fall back to flat keys
    if (j.contains("rendering")) {
        const auto& rc = j.at("rendering");
        def.rendering.midi_channel = detail::checked_integer_or<std::uint8_t>(
            rc, "midi_channel", 1, "rendering MIDI channel");
        if (rc.contains("instrument_preset"))
            def.rendering.instrument_preset = rc.at("instrument_preset").get<std::string>();
        def.rendering.expression_cc = detail::checked_integer_or<std::uint8_t>(
            rc, "expression_cc", 11, "rendering expression CC");
        if (rc.contains("pan")) def.rendering.pan = rc.at("pan").get<float>();
        if (rc.contains("group")) def.rendering.group = rc.at("group").get<std::string>();
        if (rc.contains("articulation_map")) {
            for (const auto& [key, val] : rc.at("articulation_map").items()) {
                const auto encoded =
                    detail::checked_decimal_integer<int>(key, "articulation-map key", val);
                auto art = check_enum_range(
                    json(encoded), ArticulationType::BendDown, "ArticulationType", val);
                std::size_t nodes = 0;
                def.rendering.articulation_map[art] =
                    articulation_mapping_from_json_impl(val, 1, nodes);
            }
        }
    } else {
        // Backward compat: flat keys from schema v1/v2
        def.rendering.midi_channel = detail::checked_integer_or<std::uint8_t>(
            j, "midi_channel", 1, "rendering MIDI channel");
        if (j.contains("instrument_preset"))
            def.rendering.instrument_preset = j.at("instrument_preset").get<std::string>();
    }
    return def;
}

// =============================================================================
// Hairpin / PartDirective serialisation
// =============================================================================

json hairpin_to_json(const Hairpin& hp) {
    json j;
    j["start"] = score_time_to_json(hp.start);
    j["end"] = score_time_to_json(hp.end);
    j["type"] = static_cast<int>(hp.type);
    if (hp.target) j["target"] = static_cast<int>(*hp.target);
    return j;
}

Hairpin hairpin_from_json(const json& j) {
    Hairpin hp;
    hp.start = score_time_from_json(j.at("start"));
    hp.end = score_time_from_json(j.at("end"));
    hp.type = check_enum_range(j.at("type"), HairpinType::Diminuendo, "HairpinType", j);
    if (j.contains("target")) {
        hp.target = check_enum_range(j.at("target"), DynamicLevel::rfz, "DynamicLevel", j);
    }
    return hp;
}

json part_directive_to_json(const PartDirective& pd) {
    json j;
    j["start"] = score_time_to_json(pd.start);
    j["end"] = score_time_to_json(pd.end);
    j["directive"] = static_cast<int>(pd.directive);
    if (pd.divisi_count > 0) j["divisi_count"] = pd.divisi_count;
    return j;
}

PartDirective part_directive_from_json(const json& j) {
    PartDirective pd;
    pd.start = score_time_from_json(j.at("start"));
    pd.end = score_time_from_json(j.at("end"));
    pd.directive = check_enum_range(j.at("directive"), DirectiveType::TreCorde, "DirectiveType", j);
    pd.divisi_count =
        detail::checked_integer_or<std::uint8_t>(j, "divisi_count", 0, "divisi count");
    return pd;
}

// =============================================================================
// Part serialisation
// =============================================================================

json part_to_json(const Part& part) {
    json j;
    j["id"] = part.id.value;
    j["definition"] = part_definition_to_json(part.definition);
    json measures = json::array();
    for (const auto& m : part.measures) {
        measures.push_back(measure_to_json(m));
    }
    j["measures"] = measures;
    if (!part.part_directives.empty()) {
        json pds = json::array();
        for (const auto& pd : part.part_directives) {
            pds.push_back(part_directive_to_json(pd));
        }
        j["part_directives"] = pds;
    }
    if (!part.hairpins.empty()) {
        json hps = json::array();
        for (const auto& hp : part.hairpins) {
            hps.push_back(hairpin_to_json(hp));
        }
        j["hairpins"] = hps;
    }
    return j;
}

Part part_from_json(const json& j, int schema_version) {
    Part part;
    part.id = PartId{detail::checked_integer<std::uint64_t>(j.at("id"), "part id")};
    part.definition = part_definition_from_json(j.at("definition"));
    for (const auto& mj : j.at("measures")) {
        part.measures.push_back(measure_from_json(mj, schema_version));
    }
    if (j.contains("part_directives")) {
        for (const auto& pdj : j.at("part_directives")) {
            part.part_directives.push_back(part_directive_from_json(pdj));
        }
    }
    if (j.contains("hairpins")) {
        for (const auto& hj : j.at("hairpins")) {
            part.hairpins.push_back(hairpin_from_json(hj));
        }
    }
    return part;
}

// =============================================================================
// Annotation layer serialisation
// =============================================================================

json chord_voicing_to_json(const ChordVoicing& cv) {
    json j;
    json notes_arr = json::array();
    for (auto n : cv.notes)
        notes_arr.push_back(static_cast<int>(n));
    j["notes"] = notes_arr;
    j["root"] = static_cast<int>(cv.root);
    j["quality"] = cv.quality;
    j["inversion"] = cv.inversion;
    return j;
}

ChordVoicing chord_voicing_from_json(const json& j) {
    ChordVoicing cv;
    if (j.contains("notes")) {
        for (const auto& n : j.at("notes")) {
            auto note = MidiNote::from_int(detail::checked_integer<int>(n, "MIDI note"));
            if (!note) throw json::other_error::create(604, "MidiNote out of range [0,127]", &j);
            cv.notes.push_back(*note);
        }
    }
    auto root = PitchClass::from_int(
        detail::checked_integer_or<int>(j, "root", 0, "chord root pitch class"));
    if (!root) throw json::other_error::create(604, "PitchClass root out of range [0,11]", &j);
    cv.root = *root;
    cv.quality = j.value("quality", std::string{});
    cv.inversion = detail::checked_integer_or<int>(j, "inversion", 0, "chord inversion");
    return cv;
}

json harmonic_annotation_to_json(const HarmonicAnnotation& ha) {
    json j;
    j["position"] = score_time_to_json(ha.position);
    j["duration"] = beat_to_json(ha.duration);
    j["chord"] = chord_voicing_to_json(ha.chord);
    j["roman_numeral"] = ha.roman_numeral;
    j["function"] = static_cast<int>(ha.function);
    if (ha.secondary_function) j["secondary_function"] = *ha.secondary_function;
    j["key_context"] = key_signature_to_json(ha.key_context);
    if (ha.cadence) j["cadence"] = static_cast<int>(*ha.cadence);
    j["confidence"] = ha.confidence;
    if (!ha.non_chord_tones.empty()) {
        json ncts = json::array();
        for (const auto& nct : ha.non_chord_tones) {
            json nj;
            nj["event_id"] = nct.event_id.value;
            nj["note_index"] = nct.note_index;
            nj["type"] = static_cast<int>(nct.type);
            ncts.push_back(nj);
        }
        j["non_chord_tones"] = ncts;
    }
    return j;
}

HarmonicAnnotation harmonic_annotation_from_json(const json& j, int schema_version) {
    HarmonicAnnotation ha;
    ha.position = score_time_from_json(j.at("position"));
    ha.duration = beat_from_json(j.at("duration"));
    if (j.contains("chord")) ha.chord = chord_voicing_from_json(j.at("chord"));
    ha.roman_numeral = j.at("roman_numeral").get<std::string>();
    ha.function = check_enum_range(
        j.at("function"), ScoreHarmonicFunction::Ambiguous, "ScoreHarmonicFunction", j);
    if (j.contains("secondary_function"))
        ha.secondary_function = j.at("secondary_function").get<std::string>();
    ha.key_context = key_signature_from_json(j.at("key_context"), schema_version);
    if (j.contains("cadence"))
        ha.cadence = check_enum_range(j.at("cadence"), CadenceType::None, "CadenceType", j);
    ha.confidence = j.value("confidence", 1.0f);
    if (j.contains("non_chord_tones")) {
        for (const auto& nj : j.at("non_chord_tones")) {
            NonChordToneAnnotation nct;
            nct.event_id =
                EventId{detail::checked_integer<std::uint64_t>(nj.at("event_id"), "event id")};
            nct.note_index =
                detail::checked_integer<std::uint8_t>(nj.at("note_index"), "note index");
            nct.type =
                check_enum_range(nj.at("type"), NonChordToneType::Pedal, "NonChordToneType", nj);
            ha.non_chord_tones.push_back(nct);
        }
    }
    return ha;
}

json orchestration_to_json(const OrchestrationAnnotation& oa) {
    json j;
    j["part_id"] = oa.part_id.value;
    j["start"] = score_time_to_json(oa.start);
    j["end"] = score_time_to_json(oa.end);
    j["role"] = static_cast<int>(oa.role);
    if (oa.texture) j["texture"] = static_cast<int>(*oa.texture);
    if (oa.dynamic_balance) j["dynamic_balance"] = static_cast<int>(*oa.dynamic_balance);
    if (oa.doubled_part) j["doubled_part"] = oa.doubled_part->value;
    if (oa.pedal_pitch) j["pedal_pitch"] = spelled_pitch_to_json(*oa.pedal_pitch);
    if (oa.dialogue_partner) j["dialogue_partner"] = oa.dialogue_partner->value;
    return j;
}

OrchestrationAnnotation orchestration_from_json(const json& j) {
    OrchestrationAnnotation oa;
    oa.part_id = PartId{detail::checked_integer<std::uint64_t>(j.at("part_id"), "part id")};
    oa.start = score_time_from_json(j.at("start"));
    oa.end = score_time_from_json(j.at("end"));
    oa.role = check_enum_range(j.at("role"), TexturalRole::Accompagnato, "TexturalRole", j);
    if (j.contains("texture"))
        oa.texture =
            check_enum_range(j.at("texture"), TextureType::MelodyAccompaniment, "TextureType", j);
    if (j.contains("dynamic_balance"))
        oa.dynamic_balance = check_enum_range(
            j.at("dynamic_balance"), DynamicBalance::Background, "DynamicBalance", j);
    if (j.contains("doubled_part"))
        oa.doubled_part =
            PartId{detail::checked_integer<std::uint64_t>(j.at("doubled_part"), "doubled part id")};
    if (j.contains("pedal_pitch")) oa.pedal_pitch = spelled_pitch_from_json(j.at("pedal_pitch"));
    if (j.contains("dialogue_partner"))
        oa.dialogue_partner = PartId{detail::checked_integer<std::uint64_t>(
            j.at("dialogue_partner"), "dialogue partner id")};
    return oa;
}

// =============================================================================
// ScoreRegion serialisation
// =============================================================================

json score_region_to_json(const ScoreRegion& sr) {
    json j;
    j["start"] = score_time_to_json(sr.start);
    j["end"] = score_time_to_json(sr.end);
    if (!sr.parts.empty()) {
        json pids = json::array();
        for (const auto& pid : sr.parts)
            pids.push_back(pid.value);
        j["parts"] = pids;
    }
    return j;
}

ScoreRegion score_region_from_json(const json& j) {
    ScoreRegion sr;
    sr.start = score_time_from_json(j.at("start"));
    sr.end = score_time_from_json(j.at("end"));
    if (j.contains("parts")) {
        for (const auto& pj : j.at("parts")) {
            sr.parts.push_back(
                PartId{detail::checked_integer<std::uint64_t>(pj, "region part id")});
        }
    }
    return sr;
}

// =============================================================================
// ToneRow serialisation
// =============================================================================

json tone_row_to_json(const ToneRow& tr) {
    json arr = json::array();
    for (const auto& pc : tr.elements)
        arr.push_back(pc.value());
    return arr;
}

ToneRow tone_row_from_json(const json& arr) {
    ToneRow tr;
    for (std::size_t i = 0; i < 12 && i < arr.size(); ++i) {
        auto pc =
            PitchClass::from_int(detail::checked_integer<int>(arr[i], "tone-row pitch class"));
        if (!pc) throw json::other_error::create(604, "PitchClass out of range [0,11]", &arr);
        tr.elements[i] = *pc;
    }
    return tr;
}

// =============================================================================
// Global maps serialisation
// =============================================================================

json tempo_map_to_json(const TempoMap& tm) {
    json arr = json::array();
    for (const auto& event : tm) {
        json j;
        j["position"] = score_time_to_json(event.position);
        j["bpm_num"] = event.bpm.numerator();
        j["bpm_den"] = event.bpm.denominator();
        j["beat_unit"] = static_cast<int>(event.beat_unit);
        j["transition"] = static_cast<int>(event.transition_type);
        j["linear_duration"] = beat_to_json(event.linear_duration);
        j["old_unit"] = static_cast<int>(event.old_unit);
        j["new_unit"] = static_cast<int>(event.new_unit);
        arr.push_back(j);
    }
    return arr;
}

TempoMap tempo_map_from_json(const json& arr) {
    TempoMap tm;
    for (const auto& j : arr) {
        TempoEvent event;
        event.position = score_time_from_json(j.at("position"));
        const auto bpm = PositiveRational::from_ratio(
            detail::checked_integer<std::int64_t>(j.at("bpm_num"), "tempo numerator"),
            detail::checked_integer<std::int64_t>(j.at("bpm_den"), "tempo denominator"));
        if (!bpm)
            throw json::other_error::create(
                604, "Tempo numerator and denominator must be positive", &j);
        event.bpm = *bpm;
        event.beat_unit =
            j.contains("beat_unit")
                ? check_enum_range(j.at("beat_unit"), BeatUnit::Sixteenth, "BeatUnit", j)
                : BeatUnit::Quarter;
        event.transition_type = j.contains("transition")
                                    ? check_enum_range(j.at("transition"),
                                                       TempoTransitionType::MetricModulation,
                                                       "TempoTransitionType",
                                                       j)
                                    : TempoTransitionType::Immediate;
        event.linear_duration =
            j.contains("linear_duration") ? beat_from_json(j.at("linear_duration")) : Beat::zero();
        event.old_unit =
            j.contains("old_unit")
                ? check_enum_range(j.at("old_unit"), BeatUnit::Sixteenth, "BeatUnit", j)
                : BeatUnit::Quarter;
        event.new_unit =
            j.contains("new_unit")
                ? check_enum_range(j.at("new_unit"), BeatUnit::Sixteenth, "BeatUnit", j)
                : BeatUnit::Quarter;
        tm.push_back(event);
    }
    return tm;
}

json key_map_to_json(const KeySignatureMap& km) {
    json arr = json::array();
    for (const auto& entry : km) {
        json j;
        j["position"] = score_time_to_json(entry.position);
        j["key"] = key_signature_to_json(entry.key);
        arr.push_back(j);
    }
    return arr;
}

KeySignatureMap key_map_from_json(const json& arr, int schema_version) {
    KeySignatureMap km;
    for (const auto& j : arr) {
        KeySignatureEntry entry;
        entry.position = score_time_from_json(j.at("position"));
        entry.key = key_signature_from_json(j.at("key"), schema_version);
        km.push_back(entry);
    }
    return km;
}

json time_map_to_json(const TimeSignatureMap& tm) {
    json arr = json::array();
    for (const auto& entry : tm) {
        json j;
        j["bar"] = entry.bar;
        j["numerator"] = entry.time_signature.numerator();
        j["denominator"] = entry.time_signature.denominator();
        j["groups"] = entry.time_signature.groups();
        arr.push_back(j);
    }
    return arr;
}

TimeSignatureMap time_map_from_json(const json& arr) {
    TimeSignatureMap tm;
    for (const auto& j : arr) {
        auto time_signature = TimeSignature::from_groups(
            j.at("groups").get<std::vector<int>>(),
            detail::checked_integer<int>(j.at("denominator"), "time-signature denominator"));
        if (!time_signature)
            throw json::other_error::create(604, "Invalid global time signature", &j);
        tm.push_back(
            TimeSignatureEntry{detail::checked_integer<std::uint32_t>(j.at("bar"), "time-map bar"),
                               std::move(*time_signature)});
    }
    return tm;
}

json section_to_json(const ScoreSection& section) {
    json j;
    j["id"] = section.id.value;
    j["label"] = section.label;
    j["start"] = score_time_to_json(section.start);
    j["end"] = score_time_to_json(section.end);
    if (section.form_function) {
        j["form_function"] = static_cast<int>(*section.form_function);
    }
    if (!section.children.empty()) {
        json children = json::array();
        for (const auto& child : section.children) {
            children.push_back(section_to_json(child));
        }
        j["children"] = children;
    }
    return j;
}

ScoreSection section_from_json(const json& j) {
    ScoreSection section;
    section.id = SectionId{detail::checked_integer<std::uint64_t>(j.at("id"), "section id")};
    section.label = j.at("label").get<std::string>();
    section.start = score_time_from_json(j.at("start"));
    section.end = score_time_from_json(j.at("end"));
    if (j.contains("form_function")) {
        section.form_function =
            check_enum_range(j.at("form_function"), FormFunction::Parenthetical, "FormFunction", j);
    }
    if (j.contains("children")) {
        for (const auto& cj : j.at("children")) {
            section.children.push_back(section_from_json(cj));
        }
    }
    return section;
}

json rehearsal_mark_to_json(const RehearsalMark& rm) {
    json j;
    j["position"] = score_time_to_json(rm.position);
    j["label"] = rm.label;
    return j;
}

RehearsalMark rehearsal_mark_from_json(const json& j) {
    RehearsalMark rm;
    rm.position = score_time_from_json(j.at("position"));
    rm.label = j.at("label").get<std::string>();
    return rm;
}

} // anonymous namespace

nlohmann::json articulation_mapping_to_json(const ArticulationMapping& mapping) {
    return articulation_mapping_to_json_impl(mapping);
}

Result<ArticulationMapping> articulation_mapping_from_json(const nlohmann::json& value) {
    try {
        std::size_t nodes = 0;
        return articulation_mapping_from_json_impl(value, 1, nodes);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

// =============================================================================
// Public API
// =============================================================================

nlohmann::json score_to_json(const Score& score) {
    json j;
    j["schema_version"] = SCORE_IR_SCHEMA_VERSION;
    j["id"] = score.id.value;
    j["version"] = score.version;

    json tuning;
    tuning["name"] = score.tuning.name;
    tuning["reference_midi_note"] = score.tuning.reference_midi_note;
    tuning["reference_frequency_hz"] = score.tuning.reference_frequency_hz;
    tuning["cents_from_reference"] = score.tuning.cents_from_reference;
    j["tuning"] = std::move(tuning);

    // Metadata
    json meta;
    meta["title"] = score.metadata.title;
    if (score.metadata.subtitle) meta["subtitle"] = *score.metadata.subtitle;
    if (score.metadata.composer) meta["composer"] = *score.metadata.composer;
    if (score.metadata.arranger) meta["arranger"] = *score.metadata.arranger;
    if (score.metadata.opus) meta["opus"] = *score.metadata.opus;
    meta["created_at"] = score.metadata.created_at;
    meta["modified_at"] = score.metadata.modified_at;
    meta["total_bars"] = score.metadata.total_bars;
    if (!score.metadata.tags.empty()) meta["tags"] = score.metadata.tags;
    j["metadata"] = meta;

    // Global maps
    j["tempo_map"] = tempo_map_to_json(score.tempo_map);
    j["key_map"] = key_map_to_json(score.key_map);
    j["time_map"] = time_map_to_json(score.time_map);

    // Section map
    json sections = json::array();
    for (const auto& section : score.section_map) {
        sections.push_back(section_to_json(section));
    }
    j["section_map"] = sections;

    // Rehearsal marks
    if (!score.rehearsal_marks.empty()) {
        json rms = json::array();
        for (const auto& rm : score.rehearsal_marks) {
            rms.push_back(rehearsal_mark_to_json(rm));
        }
        j["rehearsal_marks"] = rms;
    }

    // Parts
    json parts = json::array();
    for (const auto& part : score.parts) {
        parts.push_back(part_to_json(part));
    }
    j["parts"] = parts;

    // Harmonic annotations
    if (!score.harmonic_annotations.empty()) {
        json has = json::array();
        for (const auto& ha : score.harmonic_annotations) {
            has.push_back(harmonic_annotation_to_json(ha));
        }
        j["harmonic_annotations"] = has;
    }

    // Orchestration annotations
    if (!score.orchestration_annotations.empty()) {
        json oas = json::array();
        for (const auto& oa : score.orchestration_annotations) {
            oas.push_back(orchestration_to_json(oa));
        }
        j["orchestration_annotations"] = oas;
    }

    // Tone row
    if (score.tone_row) {
        j["tone_row"] = tone_row_to_json(*score.tone_row);
    }

    // Stale regions
    if (!score.stale_harmonic_regions.empty()) {
        json srs = json::array();
        for (const auto& sr : score.stale_harmonic_regions) {
            srs.push_back(score_region_to_json(sr));
        }
        j["stale_harmonic_regions"] = srs;
    }
    if (!score.stale_orchestration_regions.empty()) {
        json srs = json::array();
        for (const auto& sr : score.stale_orchestration_regions) {
            srs.push_back(score_region_to_json(sr));
        }
        j["stale_orchestration_regions"] = srs;
    }

    return j;
}

Result<Score> score_from_json(const nlohmann::json& j) {
    try {
        // Schema version check
        const auto version =
            detail::checked_integer_or<int>(j, "schema_version", 0, "Score schema version");
        if (version < 1 || version > SCORE_IR_SCHEMA_VERSION) {
            return std::unexpected(ErrorCode::FormatError);
        }

        Score score;
        score.id = ScoreId{detail::checked_integer<std::uint64_t>(j.at("id"), "score id")};
        score.version = detail::checked_integer_or<std::uint64_t>(j, "version", 1, "score version");
        if (j.contains("state")) {
            if (version >= 5) {
                throw json::other_error::create(
                    604, "schema-v5 removed the non-authoritative document state", &j);
            }
            const int legacy_state =
                detail::checked_integer<int>(j.at("state"), "legacy document state");
            if (legacy_state < 0 || legacy_state > 3) {
                throw json::other_error::create(604, "invalid legacy document state", &j);
            }
        }

        if (version >= 6 || j.contains("tuning")) {
            const auto& tuning = j.at("tuning");
            score.tuning.name = tuning.at("name").get<std::string>();
            score.tuning.reference_midi_note = detail::checked_integer<std::uint8_t>(
                tuning.at("reference_midi_note"), "tuning reference MIDI note");
            score.tuning.reference_frequency_hz = tuning.at("reference_frequency_hz").get<double>();
            const auto& cents = tuning.at("cents_from_reference");
            if (!cents.is_array() || cents.size() != SCORE_TUNING_NOTE_COUNT) {
                throw json::other_error::create(604, "tuning requires exactly 128 entries", &j);
            }
            for (std::size_t note = 0; note < SCORE_TUNING_NOTE_COUNT; ++note)
                score.tuning.cents_from_reference[note] = cents.at(note).get<double>();
        }

        // Metadata
        const auto& meta = j.at("metadata");
        score.metadata.title = meta.at("title").get<std::string>();
        if (meta.contains("subtitle"))
            score.metadata.subtitle = meta.at("subtitle").get<std::string>();
        if (meta.contains("composer"))
            score.metadata.composer = meta.at("composer").get<std::string>();
        if (meta.contains("arranger"))
            score.metadata.arranger = meta.at("arranger").get<std::string>();
        if (meta.contains("opus")) score.metadata.opus = meta.at("opus").get<std::string>();
        score.metadata.created_at = detail::checked_integer_or<std::uint64_t>(
            meta, "created_at", 0, "metadata creation time");
        score.metadata.modified_at = detail::checked_integer_or<std::uint64_t>(
            meta, "modified_at", 0, "metadata modification time");
        score.metadata.total_bars =
            detail::checked_integer<std::uint32_t>(meta.at("total_bars"), "metadata total bars");
        if (meta.contains("tags"))
            score.metadata.tags = meta.at("tags").get<std::vector<std::string>>();

        // Global maps
        score.tempo_map = tempo_map_from_json(j.at("tempo_map"));
        score.key_map = key_map_from_json(j.at("key_map"), version);
        score.time_map = time_map_from_json(j.at("time_map"));

        // Section map
        if (j.contains("section_map")) {
            for (const auto& sj : j.at("section_map")) {
                score.section_map.push_back(section_from_json(sj));
            }
        }

        // Rehearsal marks
        if (j.contains("rehearsal_marks")) {
            for (const auto& rj : j.at("rehearsal_marks")) {
                score.rehearsal_marks.push_back(rehearsal_mark_from_json(rj));
            }
        }

        // Parts
        for (const auto& pj : j.at("parts")) {
            score.parts.push_back(part_from_json(pj, version));
        }

        // Harmonic annotations
        if (j.contains("harmonic_annotations")) {
            for (const auto& hj : j.at("harmonic_annotations")) {
                score.harmonic_annotations.push_back(harmonic_annotation_from_json(hj, version));
            }
        }

        // Orchestration annotations
        if (j.contains("orchestration_annotations")) {
            for (const auto& oj : j.at("orchestration_annotations")) {
                score.orchestration_annotations.push_back(orchestration_from_json(oj));
            }
        }

        // Tone row
        if (j.contains("tone_row")) {
            score.tone_row = tone_row_from_json(j.at("tone_row"));
        }

        // Stale regions
        if (j.contains("stale_harmonic_regions")) {
            for (const auto& sj : j.at("stale_harmonic_regions")) {
                score.stale_harmonic_regions.push_back(score_region_from_json(sj));
            }
        }
        if (j.contains("stale_orchestration_regions")) {
            for (const auto& sj : j.at("stale_orchestration_regions")) {
                score.stale_orchestration_regions.push_back(score_region_from_json(sj));
            }
        }

        // Full validation on load (§13.3). Structural and rendering-domain
        // errors cannot be repaired by a downstream compiler. Musical
        // diagnostics remain advisory because target compilers may degrade
        // individual events with explicit report evidence.
        auto all_diags = validate_score(score);
        for (const auto& diag : all_diags) {
            if (diag.severity == ValidationSeverity::Error && !diag.rule.empty() &&
                (diag.rule[0] == 'S' || diag.rule[0] == 'R')) {
                return std::unexpected(ErrorCode::ValidationOnLoadFailed);
            }
        }

        return score;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

std::string score_to_json_string(const Score& score, int indent) {
    return score_to_json(score).dump(indent);
}

Result<Score> score_from_json_string(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        return score_from_json(j);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

} // namespace sunny::core
