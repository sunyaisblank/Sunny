/**
 * @file python_module.cpp
 * @brief pybind11 Python bindings for Sunny
 *
 *
 * Exposes the core, render, and infrastructure APIs
 * functionality to Python via pybind11.
 * This is the minimal binding layer - all logic is in C++.
 */

#include <cmath>
#include <cstddef>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <utility>
#include <vector>

// Core includes
#include <sunny/core/harmony/harmonic_function.hpp>
#include <sunny/core/harmony/negative_harmony.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/midi_note.hpp>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/pitch/pitch_class_set.hpp>
#include <sunny/core/rhythm/euclidean.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/scale/generation.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>

// Render includes
#include <sunny/render/arpeggiator.hpp>
#include <sunny/render/modulation.hpp>
#include <sunny/render/transport.hpp>

// Infrastructure includes
#include <sunny/infrastructure/ableton/target_snapshot.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <sunny/version.hpp>

namespace py = pybind11;
using namespace sunny::core;

// Helper to unwrap Result<T>
template <typename T> T unwrap(const Result<T>& result, const char* msg) {
    if (!result) {
        throw std::runtime_error(msg);
    }
    return *result;
}

void require_render(const VoidResult& result, const char* message) {
    if (!result) throw py::value_error(message);
}

template <typename T> T require_render(Result<T> result, const char* message) {
    if (!result) throw py::value_error(message);
    return std::move(*result);
}

// Validated conversions at the Python boundary: out-of-range input raises
// instead of truncating into a value that violates the type invariant.
PitchClass to_pitch_class(int pc) {
    return unwrap(PitchClass::from_int(pc), "Pitch class must be in [0, 11]");
}

MidiNote to_midi_note(int note) {
    return unwrap(MidiNote::from_int(note), "MIDI note must be in [0, 127]");
}

/// Python has no Ableton transport, so its orchestrator delivers into a
/// recording that acknowledges every message; drain_messages() hands the
/// record to the caller.
struct OfflineOrchestrator {
    sunny::infrastructure::RecordingDelivery delivery;
    sunny::infrastructure::Orchestrator orchestrator;
};

MidiNote to_render_midi_note(int note) {
    auto result = MidiNote::from_int(note);
    if (!result) throw py::value_error("MIDI note must be in [0, 127]");
    return *result;
}

PYBIND11_MODULE(sunny_native, m) {
    m.doc() = R"doc(
        Sunny Native Backend
        ====================

        High-performance music theory computation library.

        Modules:
        - Pitch: pitch class operations (Z/12Z)
        - Scale: scale definitions and generation
        - Harmony: chord analysis and generation
        - VoiceLeading: optimal voice leading
        - Rhythm: Euclidean rhythm generation
    )doc";

    // Cross-peer diagnostics. The production Remote Script remains a thin
    // adapter, while the native contract parser is directly exercisable by
    // integration tests using the adapter's real snapshot output.
    m.attr("ABLETON_BRIDGE_PROTOCOL_VERSION") =
        py::int_(sunny::infrastructure::SUNNY_BRIDGE_PROTOCOL_VERSION);
    m.attr("ABLETON_TARGET_SNAPSHOT_SCHEMA_VERSION") =
        py::int_(sunny::infrastructure::SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION);
    m.def("validate_ableton_target_snapshot_json", [](const std::string& payload) {
        try {
            const auto encoded = nlohmann::json::parse(payload);
            const auto parsed = sunny::infrastructure::target_snapshot_from_json(encoded);
            if (!parsed) throw py::value_error("snapshot violates the native Ableton contract");
        } catch (const nlohmann::json::exception&) {
            throw py::value_error("snapshot is not valid JSON");
        }
        return true;
    });

    // =========================================================================
    // Error Codes
    // =========================================================================
    py::enum_<ErrorCode>(m, "ErrorCode")
        .value("Ok", ErrorCode::Ok)
        .value("InvalidMidiNote", ErrorCode::InvalidMidiNote)
        .value("InvalidPitchClass", ErrorCode::InvalidPitchClass)
        .value("InvalidBeat", ErrorCode::InvalidBeat)
        .value("InvalidScaleName", ErrorCode::InvalidScaleName)
        .value("InvalidRomanNumeral", ErrorCode::InvalidRomanNumeral)
        .value("ScaleGenerationFailed", ErrorCode::ScaleGenerationFailed)
        .value("ChordGenerationFailed", ErrorCode::ChordGenerationFailed)
        .value("VoiceLeadingFailed", ErrorCode::VoiceLeadingFailed)
        .value("EuclideanInvalidParams", ErrorCode::EuclideanInvalidParams)
        .value("RenderInvalidParameter", ErrorCode::RenderInvalidParameter)
        .value("RenderInvalidSampleRate", ErrorCode::RenderInvalidSampleRate)
        .value("RenderEmptyPattern", ErrorCode::RenderEmptyPattern)
        .value("RenderInvalidPPQ", ErrorCode::RenderInvalidPPQ)
        .value("RenderInvalidPosition", ErrorCode::RenderInvalidPosition)
        .value("RenderUnrepresentableTiming", ErrorCode::RenderUnrepresentableTiming)
        .value("RenderInvalidBlockSize", ErrorCode::RenderInvalidBlockSize)
        .value("RenderInvalidSignalBuffer", ErrorCode::RenderInvalidSignalBuffer)
        .value("RenderInvalidChannelCount", ErrorCode::RenderInvalidChannelCount)
        .value("RenderControlQueueFull", ErrorCode::RenderControlQueueFull)
        .value("RenderNotConfigured", ErrorCode::RenderNotConfigured)
        .value("RenderEventQueueFull", ErrorCode::RenderEventQueueFull)
        .value("RenderEventBufferFull", ErrorCode::RenderEventBufferFull);

    // =========================================================================
    // Beat Type
    // =========================================================================
    py::class_<Beat>(m, "Beat")
        .def(py::init([](std::int64_t numerator, std::int64_t denominator) {
            auto result = Beat::from_ratio(numerator, denominator);
            if (!result)
                throw py::value_error(
                    "Beat denominator must be non-zero and the ratio must fit int64");
            return *result;
        }))
        .def_property_readonly("numerator", &Beat::numerator)
        .def_property_readonly("denominator", &Beat::denominator)
        .def("to_float", &Beat::to_float)
        .def_static(
            "from_float",
            [](double beats, std::int64_t max_denom) {
                return require_render(Beat::from_float(beats, max_denom),
                                      "Beat must be finite with a positive denominator bound");
            },
            py::arg("beats"),
            py::arg("max_denom") = 10000)
        .def_static("zero", &Beat::zero)
        .def_static("one", &Beat::one)
        .def("__repr__", [](const Beat& b) {
            return "Beat(" + std::to_string(b.numerator()) + "/" + std::to_string(b.denominator()) +
                   ")";
        });

    // =========================================================================
    // Note Event (read-only bridge/render value)
    // =========================================================================
    py::class_<NoteEvent>(m, "NoteEvent")
        .def_property_readonly("pitch",
                               [](const NoteEvent& event) { return static_cast<int>(event.pitch); })
        .def_readonly("start_time", &NoteEvent::start_time)
        .def_readonly("duration", &NoteEvent::duration)
        .def_property_readonly(
            "velocity", [](const NoteEvent& event) { return static_cast<int>(event.velocity); })
        .def_readonly("muted", &NoteEvent::muted)
        .def_property_readonly(
            "release_velocity",
            [](const NoteEvent& event) { return static_cast<int>(event.release_velocity); })
        .def("end_time", &NoteEvent::end_time);

    // =========================================================================
    // ChordVoicing
    // =========================================================================
    py::class_<ChordVoicing>(m, "ChordVoicing")
        .def(py::init<>())
        .def_property(
            "notes",
            [](const ChordVoicing& cv) {
                return std::vector<int>(cv.notes.begin(), cv.notes.end());
            },
            [](ChordVoicing& cv, const std::vector<int>& notes) {
                std::vector<MidiNote> cpp_notes;
                cpp_notes.reserve(notes.size());
                for (int n : notes)
                    cpp_notes.push_back(to_midi_note(n));
                cv.notes = std::move(cpp_notes);
            })
        .def_property(
            "root",
            [](const ChordVoicing& cv) { return static_cast<int>(cv.root); },
            [](ChordVoicing& cv, int root) { cv.root = to_pitch_class(root); })
        .def_readwrite("quality", &ChordVoicing::quality)
        .def_readwrite("inversion", &ChordVoicing::inversion)
        .def("empty", &ChordVoicing::empty)
        .def("__len__", &ChordVoicing::size);

    // =========================================================================
    // VoiceLeadingResult
    // =========================================================================
    py::class_<VoiceLeadingResult>(m, "VoiceLeadingResult")
        .def_property_readonly("voiced_notes",
                               [](const VoiceLeadingResult& vlr) {
                                   return std::vector<int>(vlr.voiced_notes.begin(),
                                                           vlr.voiced_notes.end());
                               })
        .def_readonly("total_motion", &VoiceLeadingResult::total_motion)
        .def_readonly("has_parallel_fifths", &VoiceLeadingResult::has_parallel_fifths)
        .def_readonly("has_parallel_octaves", &VoiceLeadingResult::has_parallel_octaves);

    // =========================================================================
    // Pitch Operations
    // =========================================================================
    m.def(
        "pitch_class",
        [](int midi) { return static_cast<int>(pitch_class(to_midi_note(midi))); },
        py::arg("midi"),
        "Get pitch class from MIDI note");

    m.def(
        "transpose",
        [](int pc, int interval) {
            return static_cast<int>(transpose(to_pitch_class(pc), interval));
        },
        py::arg("pc"),
        py::arg("interval"),
        "Transpose pitch class");

    m.def(
        "invert",
        [](int pc, int axis) { return static_cast<int>(invert(to_pitch_class(pc), axis)); },
        py::arg("pc"),
        py::arg("axis") = 0,
        "Invert pitch class");

    m.def("interval_class", &interval_class, py::arg("semitones"), "Get interval class [0-6]");

    m.def(
        "note_name",
        [](int pc, bool flats) { return std::string(note_name(to_pitch_class(pc), flats)); },
        py::arg("pc"),
        py::arg("prefer_flats") = false,
        "Get note name");

    m.def(
        "note_to_pitch_class",
        [](const std::string& name) {
            return static_cast<int>(unwrap(note_to_pitch_class(name), "Invalid note name"));
        },
        py::arg("name"),
        "Parse note name to pitch class");

    m.def(
        "closest_pitch_class_midi",
        [](int ref, int target_pc) {
            return static_cast<int>(
                closest_pitch_class_midi(to_midi_note(ref), to_pitch_class(target_pc)));
        },
        py::arg("reference"),
        py::arg("target_pc"),
        "Find closest MIDI note with target pitch class");

    // =========================================================================
    // Pitch Class Set Operations
    // =========================================================================
    m.def(
        "pcs_transpose",
        [](const std::set<int>& pcs, int n) {
            PitchClassSet cpp_pcs;
            for (int pc : pcs)
                cpp_pcs.insert(to_pitch_class(pc));
            auto result = pcs_transpose(cpp_pcs, n);
            std::set<int> py_result;
            for (auto pc : result)
                py_result.insert(pc);
            return py_result;
        },
        py::arg("pcs"),
        py::arg("n"),
        "Transpose pitch class set");

    m.def(
        "pcs_invert",
        [](const std::set<int>& pcs, int axis) {
            PitchClassSet cpp_pcs;
            for (int pc : pcs)
                cpp_pcs.insert(to_pitch_class(pc));
            auto result = pcs_invert(cpp_pcs, axis);
            std::set<int> py_result;
            for (auto pc : result)
                py_result.insert(pc);
            return py_result;
        },
        py::arg("pcs"),
        py::arg("axis") = 0,
        "Invert pitch class set");

    m.def(
        "pcs_interval_vector",
        [](const std::set<int>& pcs) {
            PitchClassSet cpp_pcs;
            for (int pc : pcs)
                cpp_pcs.insert(to_pitch_class(pc));
            auto iv = pcs_interval_vector(cpp_pcs);
            return std::vector<int>(iv.begin(), iv.end());
        },
        py::arg("pcs"),
        "Get interval vector");

    // =========================================================================
    // Scale Operations
    // =========================================================================
    m.def(
        "generate_scale_notes",
        [](int root_pc, const std::vector<int>& intervals, int octave) {
            std::vector<Interval> cpp_intervals;
            for (int i : intervals)
                cpp_intervals.push_back(static_cast<Interval>(i));
            auto notes =
                unwrap(generate_scale_notes(to_pitch_class(root_pc), cpp_intervals, octave),
                       "Scale generation failed");
            return std::vector<int>(notes.begin(), notes.end());
        },
        py::arg("root_pc"),
        py::arg("intervals"),
        py::arg("octave"),
        "Generate scale MIDI notes");

    m.def(
        "is_note_in_scale",
        [](int note, int root_pc, const std::vector<int>& intervals) {
            std::vector<Interval> cpp_intervals;
            for (int i : intervals)
                cpp_intervals.push_back(static_cast<Interval>(i));
            return is_note_in_scale(to_midi_note(note), to_pitch_class(root_pc), cpp_intervals);
        },
        py::arg("note"),
        py::arg("root_pc"),
        py::arg("intervals"),
        "Check if note is in scale");

    m.def(
        "quantize_to_scale",
        [](int note, int root_pc, const std::vector<int>& intervals) {
            std::vector<Interval> cpp_intervals;
            for (int i : intervals)
                cpp_intervals.push_back(static_cast<Interval>(i));
            return static_cast<int>(
                quantize_to_scale(to_midi_note(note), to_pitch_class(root_pc), cpp_intervals));
        },
        py::arg("note"),
        py::arg("root_pc"),
        py::arg("intervals"),
        "Quantize note to scale");

    m.def(
        "list_scale_names",
        []() {
            auto names = list_scale_names();
            return std::vector<std::string>(names.begin(), names.end());
        },
        "List all built-in scale names");

    m.def(
        "scale_intervals",
        [](const std::string& name) {
            auto def = unwrap(find_scale(name), "Unknown scale name");
            auto iv = def.get_intervals();
            return std::vector<int>(iv.begin(), iv.end());
        },
        py::arg("name"),
        "Get semitone intervals of a built-in scale");

    // =========================================================================
    // Rhythm Operations
    // =========================================================================
    m.def(
        "euclidean_rhythm",
        [](int pulses, int steps, int rotation) {
            return unwrap(euclidean_rhythm(pulses, steps, rotation),
                          "Invalid Euclidean parameters");
        },
        py::arg("pulses"),
        py::arg("steps"),
        py::arg("rotation") = 0,
        "Generate Euclidean rhythm pattern");

    m.def(
        "euclidean_preset",
        [](const std::string& name) { return unwrap(euclidean_preset(name), "Unknown preset"); },
        py::arg("name"),
        "Get named Euclidean preset");

    // =========================================================================
    // Harmony Operations
    // =========================================================================
    m.def(
        "negative_harmony",
        [](const std::set<int>& pcs, int key_root) {
            PitchClassSet cpp_pcs;
            for (int pc : pcs)
                cpp_pcs.insert(to_pitch_class(pc));
            auto result = negative_harmony(cpp_pcs, to_pitch_class(key_root));
            std::set<int> py_result;
            for (auto pc : result)
                py_result.insert(pc);
            return py_result;
        },
        py::arg("chord_pcs"),
        py::arg("key_root"),
        "Apply negative harmony transformation");

    m.def(
        "generate_chord_from_numeral",
        [](const std::string& numeral,
           int key_root,
           const std::vector<int>& scale_intervals,
           int octave) {
            std::vector<Interval> cpp_intervals;
            for (int i : scale_intervals)
                cpp_intervals.push_back(static_cast<Interval>(i));
            return unwrap(generate_chord_from_numeral(
                              numeral, to_pitch_class(key_root), cpp_intervals, octave),
                          "Chord generation failed");
        },
        py::arg("numeral"),
        py::arg("key_root"),
        py::arg("scale_intervals"),
        py::arg("octave") = 4,
        "Generate chord from Roman numeral");

    m.def(
        "generate_chord",
        [](int root, const std::string& quality, int octave) {
            return unwrap(generate_chord(to_pitch_class(root), quality, octave),
                          "Chord generation failed");
        },
        py::arg("root"),
        py::arg("quality"),
        py::arg("octave") = 4,
        "Generate chord from root and quality");

    // =========================================================================
    // Voice Leading
    // =========================================================================
    m.def(
        "voice_lead_nearest_tone",
        [](const std::vector<int>& source,
           const std::vector<int>& target_pcs,
           bool lock_bass,
           bool allow_p5,
           bool allow_p8) {
            std::vector<MidiNote> cpp_source;
            for (int n : source)
                cpp_source.push_back(to_midi_note(n));
            std::vector<PitchClass> cpp_target;
            for (int pc : target_pcs)
                cpp_target.push_back(to_pitch_class(pc));
            return unwrap(
                voice_lead_nearest_tone(cpp_source, cpp_target, lock_bass, allow_p5, allow_p8),
                "Voice leading failed");
        },
        py::arg("source_pitches"),
        py::arg("target_pitch_classes"),
        py::arg("lock_bass") = false,
        py::arg("allow_parallel_fifths") = false,
        py::arg("allow_parallel_octaves") = false,
        "Compute optimal voice leading");

    m.def(
        "generate_close_voicing",
        [](const std::vector<int>& pcs, int octave) {
            std::vector<PitchClass> cpp_pcs;
            for (int pc : pcs)
                cpp_pcs.push_back(to_pitch_class(pc));
            auto result = generate_close_voicing(cpp_pcs, octave);
            return std::vector<int>(result.begin(), result.end());
        },
        py::arg("pitch_classes"),
        py::arg("root_octave") = 4,
        "Generate close voicing");

    m.def(
        "generate_drop2_voicing",
        [](const std::vector<int>& close) {
            std::vector<MidiNote> cpp_close;
            for (int n : close)
                cpp_close.push_back(to_midi_note(n));
            auto result = generate_drop2_voicing(cpp_close);
            if (!result) throw std::runtime_error("Drop-2 voicing requires >= 4 notes");
            return std::vector<int>(result->begin(), result->end());
        },
        py::arg("close_voicing"),
        "Generate drop-2 voicing");

    // =========================================================================
    // Render: Modulation
    // =========================================================================
    using namespace sunny::render;

    py::enum_<LfoWaveform>(m, "LfoWaveform")
        .value("Sine", LfoWaveform::Sine)
        .value("Triangle", LfoWaveform::Triangle)
        .value("Saw", LfoWaveform::Saw)
        .value("Square", LfoWaveform::Square)
        .value("Random", LfoWaveform::Random);

    py::class_<Lfo>(m, "Lfo")
        .def(py::init<>())
        .def(
            "set_frequency",
            [](Lfo& self, double hz) {
                require_render(self.set_frequency(hz),
                               "LFO frequency must be finite and non-negative");
            },
            py::arg("hz"))
        .def(
            "set_waveform",
            [](Lfo& self, LfoWaveform waveform) {
                require_render(self.set_waveform(waveform), "Unknown LFO waveform");
            },
            py::arg("waveform"))
        .def(
            "set_phase",
            [](Lfo& self, double phase) {
                require_render(self.set_phase(phase), "LFO phase must be finite and in [0, 1)");
            },
            py::arg("phase"))
        .def("set_seed", &Lfo::set_seed, py::arg("seed"))
        .def("reset", &Lfo::reset)
        .def(
            "process",
            [](Lfo& self, double sample_rate) {
                return require_render(
                    self.process(sample_rate),
                    "Sample rate must be finite, positive, and at least the LFO frequency");
            },
            py::arg("sample_rate"))
        .def(
            "process_block",
            [](Lfo& self, double sample_rate, std::int64_t frame_count) {
                auto context = require_render(
                    SignalBlockContext::create(sample_rate, frame_count),
                    "Sample rate must be finite and positive and frame count must be positive");
                std::vector<double> output(context.maximum_frames());
                require_render(
                    self.process_block(context, output),
                    "Block must fit the configured maximum and sample rate must cover frequency");
                return output;
            },
            py::arg("sample_rate"),
            py::arg("frame_count"))
        .def("value", &Lfo::value);

    py::enum_<EnvelopeState>(m, "EnvelopeState")
        .value("Idle", EnvelopeState::Idle)
        .value("Attack", EnvelopeState::Attack)
        .value("Decay", EnvelopeState::Decay)
        .value("Sustain", EnvelopeState::Sustain)
        .value("Release", EnvelopeState::Release);

    py::class_<sunny::render::Envelope>(m, "Envelope")
        .def(py::init<>())
        .def(
            "set_attack",
            [](sunny::render::Envelope& self, double value) {
                require_render(self.set_attack(value), "Attack must be finite and non-negative");
            },
            py::arg("seconds"))
        .def(
            "set_decay",
            [](sunny::render::Envelope& self, double value) {
                require_render(self.set_decay(value), "Decay must be finite and non-negative");
            },
            py::arg("seconds"))
        .def(
            "set_sustain",
            [](sunny::render::Envelope& self, double value) {
                require_render(self.set_sustain(value), "Sustain must be finite and in [0, 1]");
            },
            py::arg("level"))
        .def(
            "set_release",
            [](sunny::render::Envelope& self, double value) {
                require_render(self.set_release(value), "Release must be finite and non-negative");
            },
            py::arg("seconds"))
        .def("trigger", &sunny::render::Envelope::trigger)
        .def("release", &sunny::render::Envelope::release)
        .def("reset", &sunny::render::Envelope::reset)
        .def(
            "process",
            [](sunny::render::Envelope& self, double sample_rate) {
                return require_render(self.process(sample_rate),
                                      "Sample rate must be finite and positive");
            },
            py::arg("sample_rate"))
        .def(
            "process_block",
            [](sunny::render::Envelope& self, double sample_rate, std::int64_t frame_count) {
                auto context = require_render(
                    SignalBlockContext::create(sample_rate, frame_count),
                    "Sample rate must be finite and positive and frame count must be positive");
                std::vector<double> output(context.maximum_frames());
                require_render(self.process_block(context, output),
                               "Block must fit the configured maximum");
                return output;
            },
            py::arg("sample_rate"),
            py::arg("frame_count"))
        .def("value", &sunny::render::Envelope::value)
        .def("state", &sunny::render::Envelope::state)
        .def("is_active", &sunny::render::Envelope::is_active);

    py::class_<SampleAndHold>(m, "SampleAndHold")
        .def(py::init<>())
        .def(
            "trigger",
            [](SampleAndHold& self, double input) {
                require_render(self.trigger(input), "Sample-and-hold input must be in [-1, 1]");
            },
            py::arg("input"))
        .def(
            "process_block",
            [](const SampleAndHold& self, double sample_rate, std::int64_t frame_count) {
                auto context = require_render(
                    SignalBlockContext::create(sample_rate, frame_count),
                    "Sample rate must be finite and positive and frame count must be positive");
                std::vector<double> output(context.maximum_frames());
                require_render(self.process_block(context, output),
                               "Block must fit the configured maximum");
                return output;
            },
            py::arg("sample_rate"),
            py::arg("frame_count"))
        .def("value", &SampleAndHold::value)
        .def("reset", &SampleAndHold::reset);

    // =========================================================================
    // Render: Arpeggio
    // =========================================================================
    py::enum_<ArpDirection>(m, "ArpDirection")
        .value("Up", ArpDirection::Up)
        .value("Down", ArpDirection::Down)
        .value("UpDown", ArpDirection::UpDown)
        .value("DownUp", ArpDirection::DownUp)
        .value("Random", ArpDirection::Random)
        .value("Order", ArpDirection::Order);

    py::class_<Arpeggiator>(m, "Arpeggiator")
        .def(py::init<>())
        .def(
            "set_direction",
            [](Arpeggiator& self, ArpDirection value) {
                require_render(self.set_direction(value), "Unknown arpeggiator direction");
            },
            py::arg("direction"))
        .def(
            "set_octave_range",
            [](Arpeggiator& self, int value) {
                require_render(self.set_octave_range(value), "Octave range must be in [1, 11]");
            },
            py::arg("octaves"))
        .def(
            "set_gate",
            [](Arpeggiator& self, double value) {
                require_render(self.set_gate(value), "Gate must be finite and in (0, 1]");
            },
            py::arg("gate"))
        .def("set_seed", &Arpeggiator::set_seed, py::arg("seed"))
        .def(
            "set_notes",
            [](Arpeggiator& self, const std::vector<int>& notes) {
                std::vector<MidiNote> cpp_notes;
                for (int n : notes)
                    cpp_notes.push_back(to_render_midi_note(n));
                self.set_notes(cpp_notes);
            },
            py::arg("notes"))
        .def("clear", &Arpeggiator::clear)
        .def("generate_pattern",
             [](const Arpeggiator& self) {
                 auto pattern = require_render(self.generate_pattern(),
                                               "Cannot generate an empty or out-of-range pattern");
                 std::vector<int> result;
                 result.reserve(pattern.size());
                 for (auto note : pattern)
                     result.push_back(static_cast<int>(note));
                 return result;
             })
        .def("reset", &Arpeggiator::reset)
        .def("next",
             [](Arpeggiator& self) {
                 return static_cast<int>(
                     require_render(self.next(), "Arpeggiator pattern is empty"));
             })
        .def("current",
             [](const Arpeggiator& self) {
                 return static_cast<int>(
                     require_render(self.current(), "Arpeggiator pattern is empty"));
             })
        .def("step", &Arpeggiator::step)
        .def("pattern_length",
             [](const Arpeggiator& self) {
                 return require_render(self.pattern_length(), "Arpeggiator pattern is empty");
             })
        .def("direction", &Arpeggiator::direction)
        .def("octave_range", &Arpeggiator::octave_range)
        .def("gate", &Arpeggiator::gate);

    m.def(
        "generate_arpeggio",
        [](const ChordVoicing& voicing,
           ArpDirection direction,
           double step_duration,
           double gate,
           int octaves) {
            if (!std::isfinite(step_duration) || step_duration <= 0.0)
                throw py::value_error("Step duration must be positive and finite");
            auto beat = require_render(Beat::from_float(step_duration),
                                       "Step duration cannot be represented as a Beat");
            auto events = require_render(generate_arpeggio(voicing, direction, beat, gate, octaves),
                                         "Arpeggio input is empty, invalid, or out of MIDI range");
            // Convert to Python-friendly format
            py::list result;
            for (const auto& e : events) {
                py::dict event;
                event["pitch"] = static_cast<int>(e.pitch);
                event["start_time"] = e.start_time.to_float();
                event["duration"] = e.duration.to_float();
                event["velocity"] = static_cast<int>(e.velocity);
                result.append(event);
            }
            return result;
        },
        py::arg("voicing"),
        py::arg("direction"),
        py::arg("step_duration") = 0.25,
        py::arg("gate") = 0.5,
        py::arg("octaves") = 1,
        "Generate arpeggio note events; timing values are whole-note fractions");

    // =========================================================================
    // Render: Transport
    // =========================================================================
    py::enum_<TransportState>(m, "TransportState")
        .value("Stopped", TransportState::Stopped)
        .value("Playing", TransportState::Playing)
        .value("Paused", TransportState::Paused)
        .value("Recording", TransportState::Recording);

    py::class_<TransportPosition>(m, "TransportPosition")
        .def_readonly("ticks", &TransportPosition::ticks)
        .def_readonly("ppq", &TransportPosition::ppq)
        .def_readonly("tempo_bpm", &TransportPosition::tempo_bpm)
        .def(
            "to_beats",
            [](const TransportPosition& p) { return p.to_beats().to_float(); },
            "Return the Sunny whole-note coordinate")
        .def("to_quarter_notes",
             &TransportPosition::to_quarter_notes,
             "Return the MIDI/Live quarter-note coordinate")
        .def("to_seconds", &TransportPosition::to_seconds);

    py::class_<Transport>(m, "Transport")
        .def(py::init([](std::int64_t ppq) {
                 return require_render(Transport::create(ppq), "PPQ must be in [1, 65535]");
             }),
             py::arg("ppq") = 480)
        .def("play", &Transport::play)
        .def("record", &Transport::record)
        .def("stop", &Transport::stop)
        .def("pause", &Transport::pause)
        .def(
            "set_tempo",
            [](Transport& self, double value) {
                require_render(self.set_tempo(value), "Tempo must be finite and in [20, 999]");
            },
            py::arg("bpm"))
        .def(
            "set_position",
            [](Transport& self, std::int64_t value) {
                require_render(self.set_position(value), "Transport position must be non-negative");
            },
            py::arg("ticks"))
        .def("state", &Transport::state)
        .def("position", &Transport::position)
        .def("tempo", &Transport::tempo)
        .def("is_playing", &Transport::is_playing)
        .def("is_running", &Transport::is_running)
        .def(
            "schedule_note",
            [](Transport& self,
               std::int64_t tick,
               int pitch,
               double duration,
               int velocity,
               int release_velocity) {
                if (!std::isfinite(duration) || duration <= 0.0)
                    throw py::value_error("Duration must be positive and finite");
                if (!is_valid_velocity(velocity))
                    throw py::value_error("Velocity must be in [1, 127]");
                if (release_velocity < 0 || release_velocity > 127)
                    throw py::value_error("Release velocity must be in [0, 127]");
                auto beat = require_render(Beat::from_float(duration),
                                           "Duration cannot be represented as a Beat");
                require_render(self.schedule_note(tick,
                                                  to_render_midi_note(pitch),
                                                  beat,
                                                  static_cast<Velocity>(velocity),
                                                  static_cast<std::uint8_t>(release_velocity)),
                               "Note timing must be non-negative and exactly representable at PPQ");
            },
            py::arg("tick"),
            py::arg("pitch"),
            py::arg("duration"),
            py::arg("velocity") = 100,
            py::arg("release_velocity") = 64,
            "Schedule a note whose duration is a whole-note fraction")
        .def("clear_scheduled", &Transport::clear_scheduled)
        .def(
            "advance",
            [](Transport& self, std::int64_t ticks) {
                require_render(self.advance(ticks), "Advance must be non-negative and in range");
            },
            py::arg("ticks"))
        .def(
            "process_block",
            [](Transport& self, std::size_t samples, double sample_rate) {
                require_render(self.process_block(samples, sample_rate),
                               "Sample rate must be finite and positive");
            },
            py::arg("sample_count"),
            py::arg("sample_rate"));

    // =========================================================================
    // Infrastructure
    // =========================================================================
    using namespace sunny::infrastructure;
    py::enum_<BridgeMessageType>(m, "BridgeMessageType")
        .value("GetProperty", BridgeMessageType::GetProperty)
        .value("SetProperty", BridgeMessageType::SetProperty)
        .value("CallMethod", BridgeMessageType::CallMethod)
        .value("CreateClip", BridgeMessageType::CreateClip)
        .value("AddNotes", BridgeMessageType::AddNotes)
        .value("Batch", BridgeMessageType::Batch);

    py::class_<BridgeMessage>(m, "BridgeMessage")
        .def_readonly("type", &BridgeMessage::type)
        .def_readonly("path", &BridgeMessage::path)
        .def_readonly("args", &BridgeMessage::args)
        .def_readonly("notes", &BridgeMessage::notes);

    py::class_<OrchestratorResult>(m, "OrchestratorResult")
        .def_property_readonly("success", &OrchestratorResult::success)
        .def_readonly("operation_id", &OrchestratorResult::operation_id)
        .def_readonly("message", &OrchestratorResult::message);

    py::class_<OfflineOrchestrator>(m, "Orchestrator")
        .def(py::init<>())
        .def(
            "create_progression_clip",
            [](OfflineOrchestrator& self,
               int track_index,
               int slot_index,
               const std::string& root,
               const std::string& scale,
               const std::vector<std::string>& numerals,
               int octave,
               double duration_beats) {
                return self.orchestrator.create_progression_clip(self.delivery,
                                                                 track_index,
                                                                 slot_index,
                                                                 root,
                                                                 scale,
                                                                 numerals,
                                                                 octave,
                                                                 duration_beats);
            },
            py::arg("track_index"),
            py::arg("slot_index"),
            py::arg("root"),
            py::arg("scale"),
            py::arg("numerals"),
            py::arg("octave") = 4,
            py::arg("duration_beats") = 4.0)
        .def(
            "apply_euclidean_rhythm",
            [](OfflineOrchestrator& self,
               int track_index,
               int slot_index,
               int pulses,
               int steps,
               int pitch,
               double step_duration) {
                return self.orchestrator.apply_euclidean_rhythm(self.delivery,
                                                                track_index,
                                                                slot_index,
                                                                pulses,
                                                                steps,
                                                                to_midi_note(pitch),
                                                                step_duration);
            },
            py::arg("track_index"),
            py::arg("slot_index"),
            py::arg("pulses"),
            py::arg("steps"),
            py::arg("pitch"),
            py::arg("step_duration") = 0.25)
        .def(
            "apply_arpeggio",
            [](OfflineOrchestrator& self,
               int track_index,
               int slot_index,
               const std::string& root,
               const std::string& scale,
               const std::vector<std::string>& numerals,
               const std::string& direction,
               double step_duration) {
                return self.orchestrator.apply_arpeggio(self.delivery,
                                                        track_index,
                                                        slot_index,
                                                        root,
                                                        scale,
                                                        numerals,
                                                        direction,
                                                        step_duration);
            },
            py::arg("track_index"),
            py::arg("slot_index"),
            py::arg("root"),
            py::arg("scale"),
            py::arg("numerals"),
            py::arg("direction"),
            py::arg("step_duration") = 0.25)
        .def("undo",
             [](OfflineOrchestrator& self) {
                 return self.orchestrator.undo(self.delivery).success();
             })
        .def("redo",
             [](OfflineOrchestrator& self) {
                 return self.orchestrator.redo(self.delivery).success();
             })
        .def("can_undo",
             [](const OfflineOrchestrator& self) { return self.orchestrator.can_undo(); })
        .def("can_redo",
             [](const OfflineOrchestrator& self) { return self.orchestrator.can_redo(); })
        .def("clear_history", [](OfflineOrchestrator& self) { self.orchestrator.clear_history(); })
        .def("drain_messages",
             [](OfflineOrchestrator& self) { return self.delivery.drain_messages(); })
        .def("pending_message_count",
             [](const OfflineOrchestrator& self) { return self.delivery.pending_message_count(); })
        .def(
            "set_max_undo_levels",
            [](OfflineOrchestrator& self, std::size_t levels) {
                self.orchestrator.set_max_undo_levels(levels);
            },
            py::arg("levels"));

    // =========================================================================
    // Version
    // =========================================================================
    m.attr("__version__") = std::string(sunny::SUNNY_VERSION);
}
