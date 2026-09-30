"""Integration tests for Sunny's native render functions.

Tests the pybind11 bindings for the Sunny render layer.
"""

from __future__ import annotations

import pytest


class TestLfo:
    """Test LFO modulation source."""

    def test_lfo_creation(self, sunny_native_module):
        """Verify LFO can be created and configured."""
        sn = sunny_native_module

        lfo = sn.Lfo()
        lfo.set_frequency(1.0)
        lfo.set_waveform(sn.LfoWaveform.Sine)

        # Initial value should be 0
        assert lfo.value() == 0.0

    def test_lfo_process(self, sunny_native_module):
        """Verify LFO produces output."""
        sn = sunny_native_module

        lfo = sn.Lfo()
        lfo.set_frequency(1.0)
        lfo.set_waveform(sn.LfoWaveform.Sine)

        sample_rate = 44100.0
        value = lfo.process(sample_rate)

        # Should produce a value in [-1, 1]
        assert -1.0 <= value <= 1.0

    def test_lfo_waveforms(self, sunny_native_module):
        """Verify all LFO waveforms work."""
        sn = sunny_native_module

        waveforms = [
            sn.LfoWaveform.Sine,
            sn.LfoWaveform.Triangle,
            sn.LfoWaveform.Saw,
            sn.LfoWaveform.Square,
            sn.LfoWaveform.Random,
        ]

        for wf in waveforms:
            lfo = sn.Lfo()
            lfo.set_waveform(wf)
            lfo.set_frequency(1.0)
            value = lfo.process(44100.0)
            assert -1.0 <= value <= 1.0

    def test_lfo_reset(self, sunny_native_module):
        """Verify LFO reset works."""
        sn = sunny_native_module

        lfo = sn.Lfo()
        lfo.set_frequency(1.0)
        lfo.set_waveform(sn.LfoWaveform.Sine)

        # Process some samples
        for _ in range(100):
            lfo.process(44100.0)

        # Reset
        lfo.reset()
        assert lfo.value() == 0.0

    def test_lfo_block_matches_scalar_processing(self, sunny_native_module):
        """Vector processing retains the exact scalar state transition sequence."""
        sn = sunny_native_module
        block_lfo = sn.Lfo()
        scalar_lfo = sn.Lfo()
        for candidate in (block_lfo, scalar_lfo):
            candidate.set_waveform(sn.LfoWaveform.Random)
            candidate.set_seed(42)
            candidate.set_frequency(4.0)

        assert block_lfo.process_block(16.0, 8) == [scalar_lfo.process(16.0) for _ in range(8)]
        with pytest.raises(ValueError):
            block_lfo.process_block(16.0, 0)
        with pytest.raises(ValueError):
            block_lfo.process_block(16.0, -1)

    def test_lfo_validation_and_seed_reproducibility(self, sunny_native_module):
        """Invalid domains raise, while seeded random streams replay exactly."""
        sn = sunny_native_module
        lfo = sn.Lfo()
        with pytest.raises(ValueError):
            lfo.set_frequency(-1.0)
        with pytest.raises(ValueError):
            lfo.set_phase(1.0)
        with pytest.raises(ValueError):
            lfo.process(0.0)

        first = sn.Lfo()
        second = sn.Lfo()
        for candidate in (first, second):
            candidate.set_waveform(sn.LfoWaveform.Random)
            candidate.set_seed(42)
            candidate.set_frequency(1.0)
        assert [first.process(1.0) for _ in range(4)] == [second.process(1.0) for _ in range(4)]


class TestEnvelope:
    """Test ADSR envelope."""

    def test_envelope_creation(self, sunny_native_module):
        """Verify envelope can be created and configured."""
        sn = sunny_native_module

        env = sn.Envelope()
        env.set_attack(0.01)
        env.set_decay(0.1)
        env.set_sustain(0.7)
        env.set_release(0.3)

        # Should start idle
        assert env.state() == sn.EnvelopeState.Idle
        assert not env.is_active()

    def test_envelope_trigger(self, sunny_native_module):
        """Verify envelope trigger works."""
        sn = sunny_native_module

        env = sn.Envelope()
        env.set_attack(0.01)

        # Trigger should start attack phase
        env.trigger()
        assert env.state() == sn.EnvelopeState.Attack
        assert env.is_active()

    def test_envelope_release(self, sunny_native_module):
        """Verify envelope release works."""
        sn = sunny_native_module

        env = sn.Envelope()
        env.set_attack(0.001)
        env.set_decay(0.001)
        env.set_sustain(0.7)
        env.set_release(0.1)

        env.trigger()

        # Process through attack and decay to sustain
        for _ in range(500):
            env.process(44100.0)

        # Now release
        env.release()
        assert env.state() == sn.EnvelopeState.Release

    def test_envelope_reset(self, sunny_native_module):
        """Verify envelope reset works."""
        sn = sunny_native_module

        env = sn.Envelope()
        env.trigger()

        for _ in range(100):
            env.process(44100.0)

        env.reset()
        assert env.state() == sn.EnvelopeState.Idle
        assert env.value() == 0.0

    def test_envelope_validation(self, sunny_native_module):
        """ADSR parameters and sample rates are validated at the binding."""
        env = sunny_native_module.Envelope()
        with pytest.raises(ValueError):
            env.set_attack(-0.1)
        with pytest.raises(ValueError):
            env.set_sustain(1.1)
        with pytest.raises(ValueError):
            env.process(float("nan"))

    def test_envelope_block_matches_scalar_processing(self, sunny_native_module):
        """ADSR block output is the scalar recurrence evaluated over one vector."""
        sn = sunny_native_module
        block_env = sn.Envelope()
        scalar_env = sn.Envelope()
        for candidate in (block_env, scalar_env):
            candidate.set_attack(0.25)
            candidate.set_decay(0.25)
            candidate.set_sustain(0.5)
            candidate.trigger()

        assert block_env.process_block(16.0, 8) == [scalar_env.process(16.0) for _ in range(8)]
        assert block_env.state() == scalar_env.state()


class TestSampleAndHold:
    """Test sample and hold."""

    def test_sample_and_hold(self, sunny_native_module):
        """Verify sample and hold works."""
        sn = sunny_native_module

        sh = sn.SampleAndHold()
        assert sh.value() == 0.0

        sh.trigger(0.5)
        assert sh.value() == 0.5

        sh.trigger(0.8)
        assert sh.value() == 0.8
        assert sh.process_block(48_000.0, 3) == [0.8, 0.8, 0.8]
        assert sh.value() == 0.8

        sh.reset()
        assert sh.value() == 0.0

        with pytest.raises(ValueError):
            sh.trigger(1.1)
        with pytest.raises(ValueError):
            sh.process_block(48_000.0, 0)


class TestArpeggiator:
    """Test arpeggiator."""

    def test_arpeggiator_creation(self, sunny_native_module):
        """Verify arpeggiator can be created."""
        sn = sunny_native_module

        arp = sn.Arpeggiator()
        arp.set_direction(sn.ArpDirection.Up)
        arp.set_octave_range(1)
        arp.set_gate(0.5)

        assert arp.direction() == sn.ArpDirection.Up
        assert arp.octave_range() == 1
        assert arp.gate() == 0.5

    def test_arpeggiator_up_pattern(self, sunny_native_module):
        """Verify up pattern generation."""
        sn = sunny_native_module

        arp = sn.Arpeggiator()
        arp.set_direction(sn.ArpDirection.Up)
        arp.set_notes([60, 64, 67])  # C E G

        pattern = arp.generate_pattern()

        # Should be sorted ascending
        assert pattern == [60, 64, 67]

    def test_arpeggiator_down_pattern(self, sunny_native_module):
        """Verify down pattern generation."""
        sn = sunny_native_module

        arp = sn.Arpeggiator()
        arp.set_direction(sn.ArpDirection.Down)
        arp.set_notes([60, 64, 67])  # C E G

        pattern = arp.generate_pattern()

        # Should be sorted descending
        assert pattern == [67, 64, 60]

    def test_arpeggiator_octave_range(self, sunny_native_module):
        """Verify octave range expansion."""
        sn = sunny_native_module

        arp = sn.Arpeggiator()
        arp.set_direction(sn.ArpDirection.Up)
        arp.set_octave_range(2)
        arp.set_notes([60, 64, 67])

        pattern = arp.generate_pattern()

        # Should span 2 octaves (6 notes)
        assert len(pattern) == 6
        # Should include original + octave up
        assert 72 in pattern  # C5
        assert 76 in pattern  # E5
        assert 79 in pattern  # G5

    def test_arpeggiator_step_access(self, sunny_native_module):
        """Verify step-by-step access."""
        sn = sunny_native_module

        arp = sn.Arpeggiator()
        arp.set_direction(sn.ArpDirection.Up)
        arp.set_notes([60, 64, 67])

        # next returns the current note, then advances (pinned by )
        arp.reset()
        assert arp.current() == 60

        assert arp.next() == 60
        assert arp.next() == 64
        assert arp.next() == 67

    def test_arpeggiator_declines_empty_and_stale_patterns(self, sunny_native_module):
        """No note is fabricated, and direction changes rebuild cached output."""
        sn = sunny_native_module
        arp = sn.Arpeggiator()
        with pytest.raises(ValueError):
            arp.next()
        with pytest.raises(ValueError):
            arp.set_notes([128])

        arp.set_notes([60, 64, 67])
        assert arp.generate_pattern() == [60, 64, 67]
        arp.set_direction(sn.ArpDirection.Down)
        assert arp.generate_pattern() == [67, 64, 60]

        arp.set_notes([120])
        arp.set_octave_range(2)
        with pytest.raises(ValueError):
            arp.generate_pattern()

    def test_arpeggio_input_validation(self, sunny_native_module):
        """Event generation rejects invalid gate and unrepresentable timing."""
        sn = sunny_native_module
        voicing = sn.ChordVoicing()
        voicing.notes = [60, 64]
        with pytest.raises(ValueError):
            sn.generate_arpeggio(voicing, sn.ArpDirection.Up, 0.25, 0.0, 1)


class TestTransport:
    """Test MIDI transport."""

    def test_transport_creation(self, sunny_native_module):
        """Verify transport can be created."""
        sn = sunny_native_module

        transport = sn.Transport(480)  # 480 PPQ

        assert transport.state() == sn.TransportState.Stopped
        assert transport.tempo() == 120.0
        assert not transport.is_playing()

    def test_transport_play_stop(self, sunny_native_module):
        """Verify play/stop works."""
        sn = sunny_native_module

        transport = sn.Transport()

        transport.play()
        assert transport.state() == sn.TransportState.Playing
        assert transport.is_playing()

        transport.stop()
        assert transport.state() == sn.TransportState.Stopped
        assert not transport.is_playing()

    def test_transport_pause(self, sunny_native_module):
        """Verify pause works."""
        sn = sunny_native_module

        transport = sn.Transport()

        transport.play()
        transport.pause()
        assert transport.state() == sn.TransportState.Paused

    def test_transport_tempo(self, sunny_native_module):
        """Verify tempo setting."""
        sn = sunny_native_module

        transport = sn.Transport()

        transport.set_tempo(140.0)
        assert transport.tempo() == 140.0

    def test_transport_position(self, sunny_native_module):
        """Verify position setting."""
        sn = sunny_native_module

        transport = sn.Transport()

        transport.set_position(960)  # 2 beats at 480 PPQ
        pos = transport.position()
        assert pos.ticks == 960
        assert pos.to_beats() == 0.5
        assert pos.to_quarter_notes() == 2.0

    def test_transport_validation_and_fractional_ticks(self, sunny_native_module):
        """Construction and timing errors are explicit; small blocks accumulate."""
        sn = sunny_native_module
        with pytest.raises(ValueError):
            sn.Transport(0)

        transport = sn.Transport()
        with pytest.raises(ValueError):
            transport.set_tempo(float("nan"))
        with pytest.raises(ValueError):
            transport.set_position(-1)
        with pytest.raises(ValueError):
            transport.schedule_note(0, 60, 1 / 7)
        with pytest.raises(ValueError):
            transport.schedule_note(0, 128, 1.0)
        with pytest.raises(ValueError):
            transport.schedule_note(0, 60, 1.0, release_velocity=128)

        transport.play()
        transport.process_block(1, 1920.0)
        assert transport.position().ticks == 0
        transport.process_block(1, 1920.0)
        assert transport.position().ticks == 1

    def test_recording_is_running(self, sunny_native_module):
        """Recording follows the same clock as playback without claiming capture."""
        sn = sunny_native_module
        transport = sn.Transport()
        transport.record()
        assert transport.state() == sn.TransportState.Recording
        assert transport.is_running()
        assert not transport.is_playing()
        transport.advance(10)
        assert transport.position().ticks == 10
