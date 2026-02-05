"""Integration tests for sunny_native render functions.

Tests the pybind11 bindings for Sunny.Render.
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

        sh.reset()
        assert sh.value() == 0.0


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

        # Step through pattern
        arp.reset()
        assert arp.current() == 60

        note1 = arp.next()
        assert note1 == 64

        note2 = arp.next()
        assert note2 == 67


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
