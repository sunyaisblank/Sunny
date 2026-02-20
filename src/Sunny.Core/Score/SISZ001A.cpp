/**
 * @file SISZ001A.cpp
 * @brief Score IR serialisation — implementation
 *
 * Component: SISZ001A
 * Domain: SI (Score IR) | Category: SZ (Serialisation)
 */

#include "SISZ001A.h"
#include "SIVD001A.h"

namespace Sunny::Core {

using json = nlohmann::json;

// =============================================================================
// SpelledPitch serialisation helpers
// =============================================================================

namespace {

json spelled_pitch_to_json(const SpelledPitch& sp) {
    return json{
        {"letter", static_cast<int>(sp.letter)},
        {"accidental", sp.accidental},
        {"octave", sp.octave}
    };
}

SpelledPitch spelled_pitch_from_json(const json& j) {
    return SpelledPitch{
        static_cast<std::uint8_t>(j.at("letter").get<int>()),
        j.at("accidental").get<std::int8_t>(),
        static_cast<std::int8_t>(j.at("octave").get<int>())
    };
}

json beat_to_json(const Beat& b) {
    return json{{"num", b.numerator}, {"den", b.denominator}};
}

Beat beat_from_json(const json& j) {
    return Beat{j.at("num").get<std::int64_t>(),
                j.at("den").get<std::int64_t>()};
}

json score_time_to_json(const ScoreTime& st) {
    return json{{"bar", st.bar}, {"beat", beat_to_json(st.beat)}};
}

ScoreTime score_time_from_json(const json& j) {
    return ScoreTime{
        j.at("bar").get<std::uint32_t>(),
        beat_from_json(j.at("beat"))
    };
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
        v.written = static_cast<DynamicLevel>(j.at("written").get<int>());
    }
    v.value = j.at("value").get<std::uint8_t>();
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
    o.type = static_cast<OrnamentType>(j.at("type").get<int>());
    o.trill_interval = j.value("trill_interval", static_cast<Interval>(0));
    if (j.contains("accidental"))
        o.accidental = j.at("accidental").get<std::int8_t>();
    o.arpeggio_direction = static_cast<ArpeggioDirection>(
        j.value("arpeggio_direction", 2));  // None
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
    td.type = static_cast<TechnicalDirection::Type>(j.at("type").get<int>());
    if (j.contains("fingering"))
        td.fingering = j.at("fingering").get<std::vector<std::uint8_t>>();
    td.number = j.value("number", static_cast<std::uint8_t>(0));
    if (j.contains("pattern"))
        td.pattern = j.at("pattern").get<std::string>();
    td.slide_direction = static_cast<SlideDirection>(
        j.value("slide_direction", 0));
    td.bend_cents = j.value("bend_cents", static_cast<std::int16_t>(0));
    td.vibrato_speed = static_cast<VibratoSpeed>(
        j.value("vibrato_speed", 1));  // Normal
    return td;
}

// =============================================================================
// Note serialisation
// =============================================================================

json note_to_json(const Note& note) {
    json j;
    j["pitch"] = spelled_pitch_to_json(note.pitch);
    j["velocity"] = velocity_value_to_json(note.velocity);
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
    if (note.lyric) j["lyric"] = *note.lyric;
    if (note.notation_head) j["notation_head"] = static_cast<int>(*note.notation_head);
    return j;
}

Note note_from_json(const json& j) {
    Note note;
    note.pitch = spelled_pitch_from_json(j.at("pitch"));
    note.velocity = velocity_value_from_json(j.at("velocity"));
    if (j.contains("articulation"))
        note.articulation = static_cast<ArticulationType>(j.at("articulation").get<int>());
    if (j.contains("dynamic"))
        note.dynamic = static_cast<DynamicLevel>(j.at("dynamic").get<int>());
    if (j.contains("ornament"))
        note.ornament = ornament_from_json(j.at("ornament"));
    note.tie_forward = j.value("tie_forward", false);
    if (j.contains("grace"))
        note.grace = static_cast<GraceType>(j.at("grace").get<int>());
    if (j.contains("technical")) {
        for (const auto& tj : j.at("technical")) {
            note.technical.push_back(technical_from_json(tj));
        }
    }
    if (j.contains("lyric"))
        note.lyric = j.at("lyric").get<std::string>();
    if (j.contains("notation_head"))
        note.notation_head = static_cast<NoteHeadType>(j.at("notation_head").get<int>());
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
    tc.id = TupletId{j.at("id").get<std::uint64_t>()};
    tc.actual = j.at("actual").get<std::uint8_t>();
    tc.normal = j.at("normal").get<std::uint8_t>();
    tc.normal_type = beat_from_json(j.at("normal_type"));
    if (j.contains("nested_in"))
        tc.nested_in = TupletId{j.at("nested_in").get<std::uint64_t>()};
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
        if (ng->tuplet_context)
            j["tuplet_context"] = tuplet_context_to_json(*ng->tuplet_context);
        if (ng->beam_group) j["beam_group"] = ng->beam_group->value;
        if (ng->slur_start) j["slur_start"] = true;
        if (ng->slur_end) j["slur_end"] = true;
    } else if (auto* r = std::get_if<RestEvent>(&event.payload)) {
        j["type"] = "rest";
        j["duration"] = beat_to_json(r->duration);
        j["visible"] = r->visible;
    } else if (auto* cs = std::get_if<ChordSymbolEvent>(&event.payload)) {
        j["type"] = "chord_symbol";
        j["root"] = spelled_pitch_to_json(cs->root);
        j["quality"] = cs->quality;
        if (cs->bass) j["bass"] = spelled_pitch_to_json(*cs->bass);
        if (cs->roman) j["roman"] = *cs->roman;
        if (!cs->extensions.empty()) j["extensions"] = cs->extensions;
    } else if (auto* d = std::get_if<ScoreDirection>(&event.payload)) {
        j["type"] = "direction";
        j["direction_type"] = static_cast<int>(d->type);
        if (d->text) j["text"] = *d->text;
        if (d->new_clef) j["new_clef"] = static_cast<int>(*d->new_clef);
        if (d->ottava_shift != 0) j["ottava_shift"] = d->ottava_shift;
    }

    return j;
}

Event event_from_json(const json& j) {
    Event event;
    event.id = EventId{j.at("id").get<std::uint64_t>()};
    event.offset = beat_from_json(j.at("offset"));

    std::string type = j.at("type").get<std::string>();

    if (type == "note_group") {
        NoteGroup ng;
        for (const auto& nj : j.at("notes")) {
            ng.notes.push_back(note_from_json(nj));
        }
        ng.duration = beat_from_json(j.at("duration"));
        if (j.contains("tuplet_context"))
            ng.tuplet_context = tuplet_context_from_json(j.at("tuplet_context"));
        if (j.contains("beam_group"))
            ng.beam_group = BeamGroupId{j.at("beam_group").get<std::uint64_t>()};
        ng.slur_start = j.value("slur_start", false);
        ng.slur_end = j.value("slur_end", false);
        event.payload = std::move(ng);
    } else if (type == "rest") {
        RestEvent r;
        r.duration = beat_from_json(j.at("duration"));
        r.visible = j.value("visible", true);
        event.payload = std::move(r);
    } else if (type == "chord_symbol") {
        ChordSymbolEvent cs;
        cs.root = spelled_pitch_from_json(j.at("root"));
        cs.quality = j.at("quality").get<std::string>();
        if (j.contains("bass"))
            cs.bass = spelled_pitch_from_json(j.at("bass"));
        if (j.contains("roman"))
            cs.roman = j.at("roman").get<std::string>();
        if (j.contains("extensions"))
            cs.extensions = j.at("extensions").get<std::vector<std::string>>();
        event.payload = std::move(cs);
    } else if (type == "direction") {
        ScoreDirection d;
        d.type = static_cast<DirectionType>(j.at("direction_type").get<int>());
        if (j.contains("text"))
            d.text = j.at("text").get<std::string>();
        if (j.contains("new_clef"))
            d.new_clef = static_cast<Clef>(j.at("new_clef").get<int>());
        d.ottava_shift = j.value("ottava_shift", static_cast<std::int8_t>(0));
        event.payload = std::move(d);
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
    for (const auto& eid : bg.event_ids) ids.push_back(eid.value);
    j["event_ids"] = ids;
    if (!bg.beam_breaks.empty()) j["beam_breaks"] = bg.beam_breaks;
    return j;
}

BeamGroup beam_group_from_json(const json& j) {
    BeamGroup bg;
    bg.id = BeamGroupId{j.at("id").get<std::uint64_t>()};
    for (const auto& ej : j.at("event_ids")) {
        bg.event_ids.push_back(EventId{ej.get<std::uint64_t>()});
    }
    if (j.contains("beam_breaks"))
        bg.beam_breaks = j.at("beam_breaks").get<std::vector<std::uint8_t>>();
    return bg;
}

json voice_to_json(const Voice& voice) {
    json j;
    j["voice_index"] = voice.voice_index;
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

Voice voice_from_json(const json& j) {
    Voice voice;
    voice.voice_index = j.at("voice_index").get<std::uint8_t>();
    for (const auto& ej : j.at("events")) {
        voice.events.push_back(event_from_json(ej));
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
    return j;
}

KeySignature key_signature_from_json(const json& j) {
    KeySignature ks;
    ks.root = spelled_pitch_from_json(j.at("root"));
    ks.accidentals = j.at("accidentals").get<std::int8_t>();
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
    return j;
}

Measure measure_from_json(const json& j) {
    Measure measure;
    measure.bar_number = j.at("bar_number").get<std::uint32_t>();
    for (const auto& vj : j.at("voices")) {
        measure.voices.push_back(voice_from_json(vj));
    }
    if (j.contains("local_key"))
        measure.local_key = key_signature_from_json(j.at("local_key"));
    return measure;
}

json part_definition_to_json(const PartDefinition& def) {
    json j;
    j["name"] = def.name;
    j["abbreviation"] = def.abbreviation;
    j["instrument_type"] = static_cast<int>(def.instrument_type);
    j["transposition"] = def.transposition;
    j["clef"] = static_cast<int>(def.clef);
    j["staff_count"] = def.staff_count;
    if (def.custom_descriptor) j["custom_descriptor"] = *def.custom_descriptor;
    j["midi_channel"] = def.rendering.midi_channel;
    if (def.rendering.instrument_preset)
        j["instrument_preset"] = *def.rendering.instrument_preset;
    return j;
}

PartDefinition part_definition_from_json(const json& j) {
    PartDefinition def;
    def.name = j.at("name").get<std::string>();
    def.abbreviation = j.at("abbreviation").get<std::string>();
    def.instrument_type = static_cast<InstrumentType>(j.at("instrument_type").get<int>());
    def.transposition = j.value("transposition", static_cast<Interval>(0));
    def.clef = static_cast<Clef>(j.value("clef", 0));
    def.staff_count = j.value("staff_count", static_cast<std::uint8_t>(1));
    if (j.contains("custom_descriptor"))
        def.custom_descriptor = j.at("custom_descriptor").get<std::string>();
    def.rendering.midi_channel = j.value("midi_channel", static_cast<std::uint8_t>(1));
    if (j.contains("instrument_preset"))
        def.rendering.instrument_preset = j.at("instrument_preset").get<std::string>();
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
    hp.type = static_cast<HairpinType>(j.at("type").get<int>());
    if (j.contains("target"))
        hp.target = static_cast<DynamicLevel>(j.at("target").get<int>());
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
    pd.directive = static_cast<DirectiveType>(j.at("directive").get<int>());
    pd.divisi_count = j.value("divisi_count", static_cast<std::uint8_t>(0));
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

Part part_from_json(const json& j) {
    Part part;
    part.id = PartId{j.at("id").get<std::uint64_t>()};
    part.definition = part_definition_from_json(j.at("definition"));
    for (const auto& mj : j.at("measures")) {
        part.measures.push_back(measure_from_json(mj));
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

json harmonic_annotation_to_json(const HarmonicAnnotation& ha) {
    json j;
    j["position"] = score_time_to_json(ha.position);
    j["duration"] = beat_to_json(ha.duration);
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

HarmonicAnnotation harmonic_annotation_from_json(const json& j) {
    HarmonicAnnotation ha;
    ha.position = score_time_from_json(j.at("position"));
    ha.duration = beat_from_json(j.at("duration"));
    ha.roman_numeral = j.at("roman_numeral").get<std::string>();
    ha.function = static_cast<ScoreHarmonicFunction>(j.at("function").get<int>());
    if (j.contains("secondary_function"))
        ha.secondary_function = j.at("secondary_function").get<std::string>();
    ha.key_context = key_signature_from_json(j.at("key_context"));
    if (j.contains("cadence"))
        ha.cadence = static_cast<CadenceType>(j.at("cadence").get<int>());
    ha.confidence = j.value("confidence", 1.0f);
    if (j.contains("non_chord_tones")) {
        for (const auto& nj : j.at("non_chord_tones")) {
            NonChordToneAnnotation nct;
            nct.event_id = EventId{nj.at("event_id").get<std::uint64_t>()};
            nct.note_index = nj.at("note_index").get<std::uint8_t>();
            nct.type = static_cast<NonChordToneType>(nj.at("type").get<int>());
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
    oa.part_id = PartId{j.at("part_id").get<std::uint64_t>()};
    oa.start = score_time_from_json(j.at("start"));
    oa.end = score_time_from_json(j.at("end"));
    oa.role = static_cast<TexturalRole>(j.at("role").get<int>());
    if (j.contains("texture"))
        oa.texture = static_cast<TextureType>(j.at("texture").get<int>());
    if (j.contains("dynamic_balance"))
        oa.dynamic_balance = static_cast<DynamicBalance>(j.at("dynamic_balance").get<int>());
    if (j.contains("doubled_part"))
        oa.doubled_part = PartId{j.at("doubled_part").get<std::uint64_t>()};
    if (j.contains("pedal_pitch"))
        oa.pedal_pitch = spelled_pitch_from_json(j.at("pedal_pitch"));
    if (j.contains("dialogue_partner"))
        oa.dialogue_partner = PartId{j.at("dialogue_partner").get<std::uint64_t>()};
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
        for (const auto& pid : sr.parts) pids.push_back(pid.value);
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
            sr.parts.push_back(PartId{pj.get<std::uint64_t>()});
        }
    }
    return sr;
}

// =============================================================================
// ToneRow serialisation
// =============================================================================

json tone_row_to_json(const ToneRow& tr) {
    json arr = json::array();
    for (const auto& pc : tr.elements) arr.push_back(pc);
    return arr;
}

ToneRow tone_row_from_json(const json& arr) {
    ToneRow tr;
    for (std::size_t i = 0; i < 12 && i < arr.size(); ++i) {
        tr.elements[i] = arr[i].get<PitchClass>();
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
        j["bpm_num"] = event.bpm.numerator;
        j["bpm_den"] = event.bpm.denominator;
        j["beat_unit"] = static_cast<int>(event.beat_unit);
        j["transition"] = static_cast<int>(event.transition_type);
        arr.push_back(j);
    }
    return arr;
}

TempoMap tempo_map_from_json(const json& arr) {
    TempoMap tm;
    for (const auto& j : arr) {
        TempoEvent event;
        event.position = score_time_from_json(j.at("position"));
        event.bpm = PositiveRational{
            j.at("bpm_num").get<std::int64_t>(),
            j.at("bpm_den").get<std::int64_t>()
        };
        event.beat_unit = static_cast<BeatUnit>(j.value("beat_unit", 3));  // Quarter
        event.transition_type = static_cast<TempoTransitionType>(j.value("transition", 0));
        event.linear_duration = Beat::zero();
        event.old_unit = BeatUnit::Quarter;
        event.new_unit = BeatUnit::Quarter;
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

KeySignatureMap key_map_from_json(const json& arr) {
    KeySignatureMap km;
    for (const auto& j : arr) {
        KeySignatureEntry entry;
        entry.position = score_time_from_json(j.at("position"));
        entry.key = key_signature_from_json(j.at("key"));
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
        j["denominator"] = entry.time_signature.denominator;
        j["groups"] = entry.time_signature.groups;
        arr.push_back(j);
    }
    return arr;
}

TimeSignatureMap time_map_from_json(const json& arr) {
    TimeSignatureMap tm;
    for (const auto& j : arr) {
        TimeSignatureEntry entry;
        entry.bar = j.at("bar").get<std::uint32_t>();
        entry.time_signature.groups = j.at("groups").get<std::vector<int>>();
        entry.time_signature.denominator = j.at("denominator").get<int>();
        tm.push_back(entry);
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
    section.id = SectionId{j.at("id").get<std::uint64_t>()};
    section.label = j.at("label").get<std::string>();
    section.start = score_time_from_json(j.at("start"));
    section.end = score_time_from_json(j.at("end"));
    if (j.contains("form_function")) {
        section.form_function = static_cast<FormFunction>(
            j.at("form_function").get<int>()
        );
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

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

nlohmann::json score_to_json(const Score& score) {
    json j;
    j["schema_version"] = SCORE_IR_SCHEMA_VERSION;
    j["id"] = score.id.value;
    j["version"] = score.version;
    j["state"] = static_cast<int>(score.state);

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
    // Schema version check
    int version = j.value("schema_version", 0);
    if (version < 1 || version > SCORE_IR_SCHEMA_VERSION) {
        return std::unexpected(ErrorCode::FormatError);
    }

    Score score;
    score.id = ScoreId{j.at("id").get<std::uint64_t>()};
    score.version = j.value("version", static_cast<std::uint64_t>(1));
    score.state = static_cast<DocumentState>(j.value("state", 0));

    // Metadata
    const auto& meta = j.at("metadata");
    score.metadata.title = meta.at("title").get<std::string>();
    if (meta.contains("subtitle"))
        score.metadata.subtitle = meta.at("subtitle").get<std::string>();
    if (meta.contains("composer"))
        score.metadata.composer = meta.at("composer").get<std::string>();
    if (meta.contains("arranger"))
        score.metadata.arranger = meta.at("arranger").get<std::string>();
    if (meta.contains("opus"))
        score.metadata.opus = meta.at("opus").get<std::string>();
    score.metadata.created_at = meta.value("created_at", static_cast<std::uint64_t>(0));
    score.metadata.modified_at = meta.value("modified_at", static_cast<std::uint64_t>(0));
    score.metadata.total_bars = meta.at("total_bars").get<std::uint32_t>();
    if (meta.contains("tags"))
        score.metadata.tags = meta.at("tags").get<std::vector<std::string>>();

    // Global maps
    score.tempo_map = tempo_map_from_json(j.at("tempo_map"));
    score.key_map = key_map_from_json(j.at("key_map"));
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
        score.parts.push_back(part_from_json(pj));
    }

    // Harmonic annotations
    if (j.contains("harmonic_annotations")) {
        for (const auto& hj : j.at("harmonic_annotations")) {
            score.harmonic_annotations.push_back(harmonic_annotation_from_json(hj));
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

    // Re-validation on load (§13.3)
    auto structural_diags = validate_structural(score);
    for (const auto& diag : structural_diags) {
        if (diag.severity == ValidationSeverity::Error) {
            return std::unexpected(ErrorCode::FormatError);
        }
    }

    return score;
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

}  // namespace Sunny::Core
