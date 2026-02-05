/**
 * @file BDPY001A.cpp
 * @brief pybind11 Python bindings for Sunny
 *
 * Component: BDPY001A
 * Domain: BD (Binding) | Category: PY (Python)
 *
 * Exposes Sunny.Core, Sunny.Render, and Sunny.Infrastructure
 * functionality to Python via pybind11.
 * This is the minimal binding layer - all logic is in C++.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

// Core includes
#include "Tensor/TNTP001A.h"
#include "Tensor/TNBT001A.h"
#include "Tensor/TNEV001A.h"
#include "Pitch/PTPC001A.h"
#include "Pitch/PTMN001A.h"
#include "Pitch/PTPS001A.h"
#include "Scale/SCDF001A.h"
#include "Scale/SCGN001A.h"
#include "Harmony/HRFN001A.h"
#include "Harmony/HRNG001A.h"
#include "Harmony/HRRN001A.h"
#include "VoiceLeading/VLNT001A.h"
#include "Rhythm/RHEU001A.h"

// Render includes
#include "Transport/RDTR001A.h"
#include "Modulation/RDMD001A.h"
#include "Arpeggio/RDAP001A.h"

// Infrastructure includes
#include "Session/INSM001A.h"
#include "Application/INOR001A.h"

namespace py = pybind11;
using namespace Sunny::Core;

// Helper to unwrap Result<T>
template<typename T>
T unwrap(const Result<T>& result, const char* msg) {
    if (!result) {
        throw std::runtime_error(msg);
    }
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

    // =========================================================================
    // Error Codes
    // =========================================================================
    py::enum_<ErrorCode>(m, "ErrorCode")
        .value("Ok", ErrorCode::Ok)
        .value("InvalidMidiNote", ErrorCode::InvalidMidiNote)
        .value("InvalidPitchClass", ErrorCode::InvalidPitchClass)
        .value("InvalidScaleName", ErrorCode::InvalidScaleName)
        .value("InvalidRomanNumeral", ErrorCode::InvalidRomanNumeral)
        .value("ScaleGenerationFailed", ErrorCode::ScaleGenerationFailed)
        .value("ChordGenerationFailed", ErrorCode::ChordGenerationFailed)
        .value("VoiceLeadingFailed", ErrorCode::VoiceLeadingFailed)
        .value("EuclideanInvalidParams", ErrorCode::EuclideanInvalidParams)
        .export_values();

    // =========================================================================
    // Beat Type
    // =========================================================================
    py::class_<Beat>(m, "Beat")
        .def(py::init<std::int64_t, std::int64_t>())
        .def_readwrite("numerator", &Beat::numerator)
        .def_readwrite("denominator", &Beat::denominator)
        .def("to_float", &Beat::to_float)
        .def_static("from_float", &Beat::from_float,
                    py::arg("beats"), py::arg("max_denom") = 10000)
        .def_static("zero", &Beat::zero)
        .def_static("one", &Beat::one)
        .def("__repr__", [](const Beat& b) {
            return "Beat(" + std::to_string(b.numerator) + "/" +
                   std::to_string(b.denominator) + ")";
        });

    // =========================================================================
    // ChordVoicing
    // =========================================================================
    py::class_<ChordVoicing>(m, "ChordVoicing")
        .def(py::init<>())
        .def_readwrite("notes", &ChordVoicing::notes)
        .def_readwrite("root", &ChordVoicing::root)
        .def_readwrite("quality", &ChordVoicing::quality)
        .def_readwrite("inversion", &ChordVoicing::inversion)
        .def("empty", &ChordVoicing::empty)
        .def("__len__", &ChordVoicing::size);

    // =========================================================================
    // VoiceLeadingResult
    // =========================================================================
    py::class_<VoiceLeadingResult>(m, "VoiceLeadingResult")
        .def_readonly("voiced_notes", &VoiceLeadingResult::voiced_notes)
        .def_readonly("total_motion", &VoiceLeadingResult::total_motion)
        .def_readonly("has_parallel_fifths", &VoiceLeadingResult::has_parallel_fifths)
        .def_readonly("has_parallel_octaves", &VoiceLeadingResult::has_parallel_octaves);

    // =========================================================================
    // Pitch Operations
    // =========================================================================
    m.def("pitch_class", [](int midi) {
        return pitch_class(static_cast<MidiNote>(midi));
    }, py::arg("midi"), "Get pitch class from MIDI note");

    m.def("transpose", [](int pc, int interval) {
        return transpose(static_cast<PitchClass>(pc), interval);
    }, py::arg("pc"), py::arg("interval"), "Transpose pitch class");

    m.def("invert", [](int pc, int axis) {
        return invert(static_cast<PitchClass>(pc), axis);
    }, py::arg("pc"), py::arg("axis") = 0, "Invert pitch class");

    m.def("interval_class", &interval_class, py::arg("semitones"),
          "Get interval class [0-6]");

    m.def("note_name", [](int pc, bool flats) {
        return std::string(note_name(static_cast<PitchClass>(pc), flats));
    }, py::arg("pc"), py::arg("prefer_flats") = false, "Get note name");

    m.def("note_to_pitch_class", [](const std::string& name) {
        return unwrap(note_to_pitch_class(name), "Invalid note name");
    }, py::arg("name"), "Parse note name to pitch class");

    m.def("closest_pitch_class_midi", [](int ref, int target_pc) {
        return closest_pitch_class_midi(
            static_cast<MidiNote>(ref),
            static_cast<PitchClass>(target_pc)
        );
    }, py::arg("reference"), py::arg("target_pc"),
       "Find closest MIDI note with target pitch class");

    // =========================================================================
    // Pitch Class Set Operations
    // =========================================================================
    m.def("pcs_transpose", [](const std::set<int>& pcs, int n) {
        PitchClassSet cpp_pcs;
        for (int pc : pcs) cpp_pcs.insert(static_cast<PitchClass>(pc));
        auto result = pcs_transpose(cpp_pcs, n);
        std::set<int> py_result;
        for (auto pc : result) py_result.insert(pc);
        return py_result;
    }, py::arg("pcs"), py::arg("n"), "Transpose pitch class set");

    m.def("pcs_invert", [](const std::set<int>& pcs, int axis) {
        PitchClassSet cpp_pcs;
        for (int pc : pcs) cpp_pcs.insert(static_cast<PitchClass>(pc));
        auto result = pcs_invert(cpp_pcs, axis);
        std::set<int> py_result;
        for (auto pc : result) py_result.insert(pc);
        return py_result;
    }, py::arg("pcs"), py::arg("axis") = 0, "Invert pitch class set");

    m.def("pcs_interval_vector", [](const std::set<int>& pcs) {
        PitchClassSet cpp_pcs;
        for (int pc : pcs) cpp_pcs.insert(static_cast<PitchClass>(pc));
        auto iv = pcs_interval_vector(cpp_pcs);
        return std::vector<int>(iv.begin(), iv.end());
    }, py::arg("pcs"), "Get interval vector");

    // =========================================================================
    // Scale Operations
    // =========================================================================
    m.def("generate_scale_notes", [](int root_pc,
                                      const std::vector<int>& intervals,
                                      int octave) {
        std::vector<Interval> cpp_intervals;
        for (int i : intervals) cpp_intervals.push_back(static_cast<Interval>(i));
        return unwrap(generate_scale_notes(
            static_cast<PitchClass>(root_pc), cpp_intervals, octave
        ), "Scale generation failed");
    }, py::arg("root_pc"), py::arg("intervals"), py::arg("octave"),
       "Generate scale MIDI notes");

    m.def("is_note_in_scale", [](int note, int root_pc,
                                  const std::vector<int>& intervals) {
        std::vector<Interval> cpp_intervals;
        for (int i : intervals) cpp_intervals.push_back(static_cast<Interval>(i));
        return is_note_in_scale(
            static_cast<MidiNote>(note),
            static_cast<PitchClass>(root_pc),
            cpp_intervals
        );
    }, py::arg("note"), py::arg("root_pc"), py::arg("intervals"),
       "Check if note is in scale");

    m.def("quantize_to_scale", [](int note, int root_pc,
                                   const std::vector<int>& intervals) {
        std::vector<Interval> cpp_intervals;
        for (int i : intervals) cpp_intervals.push_back(static_cast<Interval>(i));
        return quantize_to_scale(
            static_cast<MidiNote>(note),
            static_cast<PitchClass>(root_pc),
            cpp_intervals
        );
    }, py::arg("note"), py::arg("root_pc"), py::arg("intervals"),
       "Quantize note to scale");

    // =========================================================================
    // Rhythm Operations
    // =========================================================================
    m.def("euclidean_rhythm", [](int pulses, int steps, int rotation) {
        return unwrap(euclidean_rhythm(pulses, steps, rotation),
                      "Invalid Euclidean parameters");
    }, py::arg("pulses"), py::arg("steps"), py::arg("rotation") = 0,
       "Generate Euclidean rhythm pattern");

    m.def("euclidean_preset", [](const std::string& name) {
        return unwrap(euclidean_preset(name), "Unknown preset");
    }, py::arg("name"), "Get named Euclidean preset");

    // =========================================================================
    // Harmony Operations
    // =========================================================================
    m.def("negative_harmony", [](const std::set<int>& pcs, int key_root) {
        PitchClassSet cpp_pcs;
        for (int pc : pcs) cpp_pcs.insert(static_cast<PitchClass>(pc));
        auto result = negative_harmony(cpp_pcs, static_cast<PitchClass>(key_root));
        std::set<int> py_result;
        for (auto pc : result) py_result.insert(pc);
        return py_result;
    }, py::arg("chord_pcs"), py::arg("key_root"),
       "Apply negative harmony transformation");

    m.def("generate_chord_from_numeral", [](const std::string& numeral,
                                             int key_root,
                                             const std::vector<int>& scale_intervals,
                                             int octave) {
        std::vector<Interval> cpp_intervals;
        for (int i : scale_intervals) cpp_intervals.push_back(static_cast<Interval>(i));
        return unwrap(generate_chord_from_numeral(
            numeral, static_cast<PitchClass>(key_root), cpp_intervals, octave
        ), "Chord generation failed");
    }, py::arg("numeral"), py::arg("key_root"),
       py::arg("scale_intervals"), py::arg("octave") = 4,
       "Generate chord from Roman numeral");

    m.def("generate_chord", [](int root, const std::string& quality, int octave) {
        return unwrap(generate_chord(
            static_cast<PitchClass>(root), quality, octave
        ), "Chord generation failed");
    }, py::arg("root"), py::arg("quality"), py::arg("octave") = 4,
       "Generate chord from root and quality");

    // =========================================================================
    // Voice Leading
    // =========================================================================
    m.def("voice_lead_nearest_tone", [](const std::vector<int>& source,
                                         const std::vector<int>& target_pcs,
                                         bool lock_bass,
                                         bool allow_p5,
                                         bool allow_p8) {
        std::vector<MidiNote> cpp_source;
        for (int n : source) cpp_source.push_back(static_cast<MidiNote>(n));
        std::vector<PitchClass> cpp_target;
        for (int pc : target_pcs) cpp_target.push_back(static_cast<PitchClass>(pc));
        return unwrap(voice_lead_nearest_tone(
            cpp_source, cpp_target, lock_bass, allow_p5, allow_p8
        ), "Voice leading failed");
    }, py::arg("source_pitches"), py::arg("target_pitch_classes"),
       py::arg("lock_bass") = false,
       py::arg("allow_parallel_fifths") = false,
       py::arg("allow_parallel_octaves") = false,
       "Compute optimal voice leading");

    m.def("generate_close_voicing", [](const std::vector<int>& pcs, int octave) {
        std::vector<PitchClass> cpp_pcs;
        for (int pc : pcs) cpp_pcs.push_back(static_cast<PitchClass>(pc));
        auto result = generate_close_voicing(cpp_pcs, octave);
        return std::vector<int>(result.begin(), result.end());
    }, py::arg("pitch_classes"), py::arg("root_octave") = 4,
       "Generate close voicing");

    m.def("generate_drop2_voicing", [](const std::vector<int>& close) {
        std::vector<MidiNote> cpp_close;
        for (int n : close) cpp_close.push_back(static_cast<MidiNote>(n));
        auto result = generate_drop2_voicing(cpp_close);
        return std::vector<int>(result.begin(), result.end());
    }, py::arg("close_voicing"), "Generate drop-2 voicing");

    // =========================================================================
    // Render: Modulation
    // =========================================================================
    using namespace Sunny::Render;

    py::enum_<LfoWaveform>(m, "LfoWaveform")
        .value("Sine", LfoWaveform::Sine)
        .value("Triangle", LfoWaveform::Triangle)
        .value("Saw", LfoWaveform::Saw)
        .value("Square", LfoWaveform::Square)
        .value("Random", LfoWaveform::Random)
        .export_values();

    py::class_<Lfo>(m, "Lfo")
        .def(py::init<>())
        .def("set_frequency", &Lfo::set_frequency, py::arg("hz"))
        .def("set_waveform", &Lfo::set_waveform, py::arg("waveform"))
        .def("set_phase", &Lfo::set_phase, py::arg("phase"))
        .def("reset", &Lfo::reset)
        .def("process", &Lfo::process, py::arg("sample_rate"))
        .def("value", &Lfo::value);

    py::enum_<EnvelopeState>(m, "EnvelopeState")
        .value("Idle", EnvelopeState::Idle)
        .value("Attack", EnvelopeState::Attack)
        .value("Decay", EnvelopeState::Decay)
        .value("Sustain", EnvelopeState::Sustain)
        .value("Release", EnvelopeState::Release)
        .export_values();

    py::class_<Envelope>(m, "Envelope")
        .def(py::init<>())
        .def("set_attack", &Envelope::set_attack, py::arg("seconds"))
        .def("set_decay", &Envelope::set_decay, py::arg("seconds"))
        .def("set_sustain", &Envelope::set_sustain, py::arg("level"))
        .def("set_release", &Envelope::set_release, py::arg("seconds"))
        .def("trigger", &Envelope::trigger)
        .def("release", &Envelope::release)
        .def("reset", &Envelope::reset)
        .def("process", &Envelope::process, py::arg("sample_rate"))
        .def("value", &Envelope::value)
        .def("state", &Envelope::state)
        .def("is_active", &Envelope::is_active);

    py::class_<SampleAndHold>(m, "SampleAndHold")
        .def(py::init<>())
        .def("trigger", &SampleAndHold::trigger, py::arg("input"))
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
        .value("Order", ArpDirection::Order)
        .export_values();

    py::class_<Arpeggiator>(m, "Arpeggiator")
        .def(py::init<>())
        .def("set_direction", &Arpeggiator::set_direction, py::arg("direction"))
        .def("set_octave_range", &Arpeggiator::set_octave_range, py::arg("octaves"))
        .def("set_gate", &Arpeggiator::set_gate, py::arg("gate"))
        .def("set_notes", [](Arpeggiator& self, const std::vector<int>& notes) {
            std::vector<MidiNote> cpp_notes;
            for (int n : notes) cpp_notes.push_back(static_cast<MidiNote>(n));
            self.set_notes(cpp_notes);
        }, py::arg("notes"))
        .def("clear", &Arpeggiator::clear)
        .def("generate_pattern", [](const Arpeggiator& self) {
            auto pattern = self.generate_pattern();
            return std::vector<int>(pattern.begin(), pattern.end());
        })
        .def("reset", &Arpeggiator::reset)
        .def("next", [](Arpeggiator& self) { return static_cast<int>(self.next()); })
        .def("current", [](const Arpeggiator& self) { return static_cast<int>(self.current()); })
        .def("step", &Arpeggiator::step)
        .def("pattern_length", &Arpeggiator::pattern_length)
        .def("direction", &Arpeggiator::direction)
        .def("octave_range", &Arpeggiator::octave_range)
        .def("gate", &Arpeggiator::gate);

    m.def("generate_arpeggio", [](const ChordVoicing& voicing,
                                   ArpDirection direction,
                                   double step_duration,
                                   double gate,
                                   int octaves) {
        auto beat = Beat::from_float(step_duration);
        auto events = generate_arpeggio(voicing, direction, beat, gate, octaves);
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
    }, py::arg("voicing"), py::arg("direction"),
       py::arg("step_duration") = 0.25, py::arg("gate") = 0.5, py::arg("octaves") = 1,
       "Generate arpeggio note events");

    // =========================================================================
    // Render: Transport
    // =========================================================================
    py::enum_<TransportState>(m, "TransportState")
        .value("Stopped", TransportState::Stopped)
        .value("Playing", TransportState::Playing)
        .value("Paused", TransportState::Paused)
        .value("Recording", TransportState::Recording)
        .export_values();

    py::class_<TransportPosition>(m, "TransportPosition")
        .def_readonly("ticks", &TransportPosition::ticks)
        .def_readonly("ppq", &TransportPosition::ppq)
        .def_readonly("tempo_bpm", &TransportPosition::tempo_bpm)
        .def("to_beats", [](const TransportPosition& p) {
            return p.to_beats().to_float();
        })
        .def("to_seconds", &TransportPosition::to_seconds);

    py::class_<Transport>(m, "Transport")
        .def(py::init<std::int64_t>(), py::arg("ppq") = 480)
        .def("play", &Transport::play)
        .def("stop", &Transport::stop)
        .def("pause", &Transport::pause)
        .def("set_tempo", &Transport::set_tempo, py::arg("bpm"))
        .def("set_position", &Transport::set_position, py::arg("ticks"))
        .def("state", &Transport::state)
        .def("position", &Transport::position)
        .def("tempo", &Transport::tempo)
        .def("is_playing", &Transport::is_playing)
        .def("schedule_note", &Transport::schedule_note,
             py::arg("tick"), py::arg("pitch"), py::arg("duration"), py::arg("velocity") = 100)
        .def("clear_scheduled", &Transport::clear_scheduled)
        .def("advance", &Transport::advance, py::arg("ticks"));

    // =========================================================================
    // Infrastructure: Session
    // =========================================================================
    using namespace Sunny::Infrastructure;

    py::enum_<ConnectionState>(m, "ConnectionState")
        .value("Disconnected", ConnectionState::Disconnected)
        .value("Connecting", ConnectionState::Connecting)
        .value("Connected", ConnectionState::Connected)
        .value("Reconnecting", ConnectionState::Reconnecting)
        .value("Error", ConnectionState::Error)
        .export_values();

    py::enum_<SessionMode>(m, "SessionMode")
        .value("Idle", SessionMode::Idle)
        .value("Playing", SessionMode::Playing)
        .value("Recording", SessionMode::Recording)
        .value("Overdubbing", SessionMode::Overdubbing)
        .export_values();

    py::class_<SessionStateMachine>(m, "SessionStateMachine")
        .def(py::init<>())
        .def("set_connected", &SessionStateMachine::set_connected)
        .def("set_disconnected", &SessionStateMachine::set_disconnected,
             py::arg("reason") = "")
        .def("set_connecting", &SessionStateMachine::set_connecting)
        .def("set_error", &SessionStateMachine::set_error, py::arg("error"))
        .def("connection_state", &SessionStateMachine::connection_state)
        .def("is_connected", &SessionStateMachine::is_connected)
        .def("set_mode", &SessionStateMachine::set_mode, py::arg("mode"))
        .def("mode", &SessionStateMachine::mode)
        .def("start_playing", &SessionStateMachine::start_playing)
        .def("stop_playing", &SessionStateMachine::stop_playing)
        .def("start_recording", &SessionStateMachine::start_recording)
        .def("stop_recording", &SessionStateMachine::stop_recording)
        .def("connection_state_string", &SessionStateMachine::connection_state_string)
        .def("mode_string", &SessionStateMachine::mode_string);

    // =========================================================================
    // Infrastructure: Orchestrator
    // =========================================================================
    py::enum_<BridgeMessageType>(m, "BridgeMessageType")
        .value("GetProperty", BridgeMessageType::GetProperty)
        .value("SetProperty", BridgeMessageType::SetProperty)
        .value("CallMethod", BridgeMessageType::CallMethod)
        .value("CreateClip", BridgeMessageType::CreateClip)
        .value("AddNotes", BridgeMessageType::AddNotes)
        .value("Batch", BridgeMessageType::Batch)
        .export_values();

    py::class_<BridgeMessage>(m, "BridgeMessage")
        .def_readonly("type", &BridgeMessage::type)
        .def_readonly("path", &BridgeMessage::path)
        .def_readonly("args", &BridgeMessage::args);

    py::class_<OrchestratorResult>(m, "OrchestratorResult")
        .def_readonly("success", &OrchestratorResult::success)
        .def_readonly("operation_id", &OrchestratorResult::operation_id)
        .def_readonly("message", &OrchestratorResult::message);

    py::class_<Orchestrator>(m, "Orchestrator")
        .def(py::init<>())
        .def("create_progression_clip", &Orchestrator::create_progression_clip,
             py::arg("track_index"), py::arg("slot_index"),
             py::arg("root"), py::arg("scale"), py::arg("numerals"),
             py::arg("octave") = 4, py::arg("duration_beats") = 4.0)
        .def("apply_euclidean_rhythm", &Orchestrator::apply_euclidean_rhythm,
             py::arg("track_index"), py::arg("slot_index"),
             py::arg("pulses"), py::arg("steps"), py::arg("pitch"),
             py::arg("step_duration") = 0.25)
        .def("apply_arpeggio", &Orchestrator::apply_arpeggio,
             py::arg("track_index"), py::arg("slot_index"),
             py::arg("numerals"), py::arg("direction"),
             py::arg("step_duration") = 0.25)
        .def("undo", &Orchestrator::undo)
        .def("redo", &Orchestrator::redo)
        .def("can_undo", &Orchestrator::can_undo)
        .def("can_redo", &Orchestrator::can_redo)
        .def("clear_history", &Orchestrator::clear_history)
        .def("drain_messages", &Orchestrator::drain_messages)
        .def("pending_message_count", &Orchestrator::pending_message_count)
        .def("set_max_undo_levels", &Orchestrator::set_max_undo_levels, py::arg("levels"));

    // =========================================================================
    // Version
    // =========================================================================
    m.attr("__version__") = "0.3.0";
}
