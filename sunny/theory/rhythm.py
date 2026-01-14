"""Sunny - Rhythm Engine.

Component: THRH001A
Domain: TH (Theory) | Category: RH (Rhythm)

Advanced rhythmic pattern generation including polyrhythms,
tuplets, and swing quantization.

Bounds:
    - pulses: int ∈ [0, steps]
    - steps: int ∈ [1, 64]
    - swing_amount: float ∈ [0.0, 1.0]
    - grid: str ∈ {"8th", "16th", "32nd", "quarter"}
    - length_beats: float ∈ [0.25, 128.0]

Error Codes:
    - 1350: Invalid tuplet ratio
    - 1351: Pulses exceed steps
    - 1352: Invalid grid value
"""

from __future__ import annotations

import math
from typing import NamedTuple


class NoteEvent(NamedTuple):
    """MIDI note event with timing.
    
    Component: THRH001A.NoteEvent
    
    Fields:
        pitch: int ∈ [0, 127]
        start_time: float ∈ [0, ∞) in beats
        duration: float ∈ (0, ∞) in beats
        velocity: int ∈ [1, 127]
    """
    
    pitch: int
    start_time: float  # In beats
    duration: float    # In beats
    velocity: int = 100


class RhythmEngine:
    """Rhythm engine for advanced rhythmic patterns.
    
    Component: THRH001A
    
    Provides:
    - Polyrhythm generation (3:2, 5:4, etc.)
    - Tuplet creation (triplets, quintuplets, etc.)
    - Swing quantization with adjustable amount
    - Pattern-based rhythm generation
    
    Bounds:
        - pulses: int ∈ [0, steps]
        - steps: int ∈ [1, 64]
        - n (tuplet): int ∈ [2, 15]
        - swing_amount: float ∈ [0.0, 1.0]
        - grid: str ∈ {"8th", "16th", "32nd", "quarter"}
    
    Error Codes:
        - 1350: Invalid tuplet ratio
        - 1351: Pulses exceed steps (Euclidean)
        - 1352: Invalid grid value
    """
    
    def create_polyrhythm(
        self,
        pattern_a: tuple[int, int],
        pattern_b: tuple[int, int],
        length_beats: float
    ) -> dict[str, list[float]]:
        """Generate polyrhythmic pattern.
        
        Creates two interlocking patterns that align at the start
        and end of the specified duration.
        
        Args:
            pattern_a: (beats, divisions) - e.g., (3, 1) for 3 quarter notes
            pattern_b: (beats, divisions) - e.g., (2, 1) for 2 quarter notes
            length_beats: Total pattern length in beats
        
        Returns:
            Dictionary with 'a' and 'b' lists of beat positions
        
        Example:
            >>> engine.create_polyrhythm((3, 1), (2, 1), 4.0)
            {
                'a': [0.0, 1.333, 2.666],
                'b': [0.0, 2.0]
            }
        """
        beats_a, divisions_a = pattern_a
        beats_b, divisions_b = pattern_b
        
        # Calculate timing for each pattern
        total_pulses_a = beats_a * divisions_a
        total_pulses_b = beats_b * divisions_b
        
        interval_a = length_beats / total_pulses_a
        interval_b = length_beats / total_pulses_b
        
        positions_a = [i * interval_a for i in range(total_pulses_a)]
        positions_b = [i * interval_b for i in range(total_pulses_b)]
        
        return {
            "a": positions_a,
            "b": positions_b,
            "combined": sorted(set(positions_a + positions_b)),
        }
    
    def create_tuplet(
        self,
        n: int,
        in_the_space_of: int,
        note_value: float
    ) -> list[float]:
        """Generate tuplet timing.
        
        Creates n notes in the space of 'in_the_space_of' notes
        of the given value.
        
        Args:
            n: Number of notes in tuplet
            in_the_space_of: Number of regular notes in same space
            note_value: Duration of regular note in beats
        
        Returns:
            List of beat durations for each tuplet note
        
        Example:
            >>> engine.create_tuplet(3, 2, 0.5)  # Triplet eighth notes
            [0.333, 0.333, 0.333]
            
            >>> engine.create_tuplet(5, 4, 0.25)  # Quintuplet 16ths
            [0.2, 0.2, 0.2, 0.2, 0.2]
        """
        total_duration = in_the_space_of * note_value
        tuplet_duration = total_duration / n
        
        return [tuplet_duration] * n
    
    def create_tuplet_positions(
        self,
        n: int,
        in_the_space_of: int,
        note_value: float,
        start_beat: float = 0.0
    ) -> list[float]:
        """Generate tuplet start positions.
        
        Args:
            n: Number of notes in tuplet
            in_the_space_of: Number of regular notes in same space
            note_value: Duration of regular note in beats
            start_beat: Starting beat position
        
        Returns:
            List of beat positions for tuplet notes
        """
        total_duration = in_the_space_of * note_value
        tuplet_duration = total_duration / n
        
        return [start_beat + i * tuplet_duration for i in range(n)]
    
    def apply_swing(
        self,
        notes: list[dict],
        swing_amount: float,
        grid: str = "16th"
    ) -> list[dict]:
        """Apply swing feel to note timings.
        
        Delays every other note on the specified grid by the
        swing amount.
        
        Args:
            notes: List of note dictionaries with 'start_time'
            swing_amount: Amount of swing (0.0 = straight, 1.0 = max)
            grid: Grid resolution ("8th", "16th", "32nd")
        
        Returns:
            List of notes with adjusted start times
        
        Example:
            >>> notes = [{"start_time": 0}, {"start_time": 0.25}]
            >>> engine.apply_swing(notes, 0.5, "16th")
            [{"start_time": 0}, {"start_time": 0.333}]
        """
        # Grid values in beats
        grid_values = {
            "8th": 0.5,
            "16th": 0.25,
            "32nd": 0.125,
            "quarter": 1.0,
        }
        
        grid_size = grid_values.get(grid, 0.25)
        
        # Maximum swing delay (as fraction of grid)
        max_delay = grid_size * 0.33  # Standard swing is ~66:33 ratio
        delay = max_delay * min(1.0, max(0.0, swing_amount))
        
        swung_notes = []
        
        for note in notes:
            new_note = note.copy()
            start = note.get("start_time", 0)
            
            # Check if on offbeat (odd grid position)
            grid_position = round(start / grid_size)
            
            if grid_position % 2 == 1:  # Offbeat
                new_note["start_time"] = start + delay
            
            swung_notes.append(new_note)
        
        return swung_notes
    
    def create_euclidean_rhythm(
        self,
        pulses: int,
        steps: int,
        rotation: int = 0
    ) -> list[bool]:
        """Generate Euclidean rhythm pattern.
        
        Distributes k pulses as evenly as possible across n steps.
        Used in many world music traditions.
        
        Args:
            pulses: Number of hits (k)
            steps: Total steps in pattern (n)
            rotation: Pattern rotation (default: 0)
        
        Returns:
            List of booleans, True = hit, False = rest
        
        Example:
            >>> engine.create_euclidean_rhythm(3, 8)  # Tresillo
            [True, False, False, True, False, False, True, False]
            
            >>> engine.create_euclidean_rhythm(5, 8)  # Cinquillo
            [True, False, True, True, False, True, True, False]
        """
        if pulses > steps:
            raise ValueError(f"Pulses ({pulses}) cannot exceed steps ({steps})")
        
        if pulses == 0:
            return [False] * steps
        
        if pulses == steps:
            return [True] * steps
        
        # Bresenham-style algorithm
        pattern = []
        for i in range(steps):
            threshold = (i + 1) * pulses / steps
            prev_threshold = i * pulses / steps
            pattern.append(int(threshold) > int(prev_threshold))
        
        # Apply rotation
        if rotation != 0:
            rotation = rotation % steps
            pattern = pattern[rotation:] + pattern[:rotation]
        
        return pattern
    
    def euclidean_to_positions(
        self,
        pattern: list[bool],
        step_duration: float,
        note_duration: float | None = None
    ) -> list[NoteEvent]:
        """Convert Euclidean pattern to note events.
        
        Args:
            pattern: Boolean rhythm pattern
            step_duration: Duration of each step in beats
            note_duration: Duration of each note (default: step_duration)
        
        Returns:
            List of NoteEvent tuples
        """
        if note_duration is None:
            note_duration = step_duration
        
        events = []
        for i, hit in enumerate(pattern):
            if hit:
                events.append(NoteEvent(
                    pitch=0,  # Placeholder - caller should set
                    start_time=i * step_duration,
                    duration=note_duration,
                ))
        
        return events
    
    def quantize_to_grid(
        self,
        notes: list[dict],
        grid: str = "16th",
        strength: float = 1.0
    ) -> list[dict]:
        """Quantize notes to rhythmic grid.
        
        Args:
            notes: List of note dictionaries with 'start_time'
            grid: Grid resolution ("8th", "16th", "32nd")
            strength: Quantize strength (0.0 = none, 1.0 = full)
        
        Returns:
            List of notes with quantized start times
        """
        grid_values = {
            "8th": 0.5,
            "16th": 0.25,
            "32nd": 0.125,
            "quarter": 1.0,
        }
        
        grid_size = grid_values.get(grid, 0.25)
        strength = min(1.0, max(0.0, strength))
        
        quantized = []
        for note in notes:
            new_note = note.copy()
            start = note.get("start_time", 0)
            
            # Find nearest grid position
            grid_pos = round(start / grid_size) * grid_size
            
            # Apply strength (interpolate between original and quantized)
            new_start = start + (grid_pos - start) * strength
            new_note["start_time"] = new_start
            
            quantized.append(new_note)
        
        return quantized
    
    # ==========================================================================
    # Nested Tuplets
    # ==========================================================================
    
    def create_nested_tuplet(
        self,
        outer: tuple[int, int],
        inner: tuple[int, int],
        note_value: float,
        start_beat: float = 0.0
    ) -> list[float]:
        """Create nested tuplets (e.g., triplet of quintuplets).
        
        This creates complex rhythmic structures where tuplets exist
        within tuplets, common in contemporary orchestral music.
        
        Args:
            outer: (n, in_space_of) for outer tuplet (e.g., (3, 2) = triplet)
            inner: (n, in_space_of) for inner tuplet within each outer note
            note_value: Duration of the base note in beats
            start_beat: Starting position
        
        Returns:
            List of beat positions for all nested tuplet notes
        
        Example:
            >>> engine.create_nested_tuplet((3, 2), (5, 4), 1.0)
            # Creates 15 notes: 5 quintuplets inside each triplet eighth
        """
        outer_n, outer_space = outer
        inner_n, inner_space = inner
        
        # Total duration of the outer tuplet
        outer_duration = outer_space * note_value
        
        # Duration of each outer tuplet note
        outer_note_duration = outer_duration / outer_n
        
        # Duration of each inner tuplet note
        inner_note_duration = (inner_space * outer_note_duration / inner_n) / inner_space * inner_space / inner_n
        # Simplified: The inner tuplet plays 'inner_n' notes in the space of 'inner_space'
        # notes of the outer tuplet's subdivision
        inner_total = outer_note_duration  # Space allocated for inner
        inner_single = inner_total / inner_n
        
        positions = []
        
        for i in range(outer_n):
            outer_start = start_beat + i * outer_note_duration
            for j in range(inner_n):
                positions.append(outer_start + j * inner_single)
        
        return positions
    
    def create_nested_tuplet_events(
        self,
        outer: tuple[int, int],
        inner: tuple[int, int],
        note_value: float,
        pitch: int = 60,
        velocity: int = 100,
        start_beat: float = 0.0
    ) -> list[NoteEvent]:
        """Create nested tuplet note events.
        
        Args:
            outer: Outer tuplet (n, in_space_of)
            inner: Inner tuplet (n, in_space_of)
            note_value: Base note duration
            pitch: MIDI pitch
            velocity: Note velocity
            start_beat: Starting position
        
        Returns:
            List of NoteEvent tuples
        """
        positions = self.create_nested_tuplet(outer, inner, note_value, start_beat)
        
        # Calculate note duration (duration of single inner tuplet note)
        outer_n, outer_space = outer
        inner_n, inner_space = inner
        
        outer_duration = outer_space * note_value
        outer_note_duration = outer_duration / outer_n
        inner_duration = outer_note_duration / inner_n
        
        return [
            NoteEvent(
                pitch=pitch,
                start_time=pos,
                duration=inner_duration * 0.9,  # Slight separation
                velocity=velocity,
            )
            for pos in positions
        ]
    
    # ==========================================================================
    # Irrational Time Signatures
    # ==========================================================================
    
    def create_irrational_meter(
        self,
        numerator: float,
        denominator: int
    ) -> dict:
        """Create an irrational time signature.
        
        Irrational meters like 4.5/8 or 2.5/4 divide the beat
        into non-integer portions, creating unique metric feels.
        
        Args:
            numerator: Can be fractional (e.g., 4.5, 3.5)
            denominator: Note value (4 = quarter, 8 = eighth)
        
        Returns:
            Dict with meter info and beat positions
        
        Example:
            >>> engine.create_irrational_meter(4.5, 8)
            {"beats": [0, 0.5, 1.0, 1.5, 2.0], "bar_length": 2.25}
        """
        # Bar length in quarter notes
        bar_length = numerator * (4 / denominator)
        
        # Beat positions
        beat_value = 4 / denominator  # Each beat in quarter notes
        beats = []
        current = 0.0
        
        # For fractional numerators, the last beat is shorter
        full_beats = int(numerator)
        fractional = numerator - full_beats
        
        for i in range(full_beats):
            beats.append(current)
            current += beat_value
        
        # Add fractional beat if present
        if fractional > 0:
            beats.append(current)
        
        return {
            "numerator": numerator,
            "denominator": denominator,
            "bar_length": bar_length,
            "beats": beats,
            "beat_value": beat_value,
            "is_irrational": numerator != int(numerator),
        }
    
    # ==========================================================================
    # Mixed Meter
    # ==========================================================================
    
    def create_mixed_meter_pattern(
        self,
        signatures: list[tuple[int, int]],
        repeats: int = 1
    ) -> dict:
        """Generate beat patterns for mixed meter.
        
        Mixed meter alternates between time signatures (e.g., 5/8 + 7/8).
        Common in progressive rock, film scoring, and contemporary classical.
        
        Args:
            signatures: List of (numerator, denominator) tuples
            repeats: Number of times to repeat the full cycle
        
        Returns:
            Dict with bar positions, downbeats, and total length
        
        Example:
            >>> engine.create_mixed_meter_pattern([(5, 8), (7, 8), (3, 4)])
        """
        bars = []
        current_pos = 0.0
        
        for _ in range(repeats):
            for numerator, denominator in signatures:
                bar_length = numerator * (4 / denominator)
                beat_value = 4 / denominator
                
                bar_info = {
                    "position": current_pos,
                    "numerator": numerator,
                    "denominator": denominator,
                    "length": bar_length,
                    "beats": [current_pos + i * beat_value for i in range(numerator)],
                }
                bars.append(bar_info)
                current_pos += bar_length
        
        return {
            "bars": bars,
            "total_length": current_pos,
            "downbeats": [bar["position"] for bar in bars],
            "pattern": signatures,
        }
    
    # ==========================================================================
    # Metric Modulation
    # ==========================================================================
    
    def calculate_metric_modulation(
        self,
        old_tempo: float,
        old_value: str,
        new_value: str
    ) -> float:
        """Calculate new tempo after metric modulation.
        
        Metric modulation redefines the tempo by equating a note value
        from the old tempo to a different note value in the new tempo.
        
        Args:
            old_tempo: Current tempo in BPM
            old_value: Old note value ("quarter", "8th", "triplet_8th", etc.)
            new_value: New note value to equal the old value
        
        Returns:
            New tempo in BPM
        
        Example:
            >>> engine.calculate_metric_modulation(120, "16th", "triplet_8th")
            160.0  # 16th at 120 BPM becomes triplet 8th at 160 BPM
        """
        # Note value durations relative to quarter note at tempo 1
        note_values = {
            "whole": 4.0,
            "half": 2.0,
            "dotted_half": 3.0,
            "quarter": 1.0,
            "dotted_quarter": 1.5,
            "8th": 0.5,
            "dotted_8th": 0.75,
            "triplet_quarter": 2/3,
            "triplet_8th": 1/3,
            "16th": 0.25,
            "triplet_16th": 1/6,
            "32nd": 0.125,
            "quintuplet_8th": 0.4,
            "quintuplet_16th": 0.2,
            "septuplet_8th": 2/7,
        }
        
        old_duration = note_values.get(old_value, 0.25)
        new_duration = note_values.get(new_value, 0.25)
        
        # If old_value = new_value in duration, tempo scales inversely
        # old_duration at old_tempo = new_duration at new_tempo
        # new_tempo = old_tempo * (new_duration / old_duration)
        
        return old_tempo * (new_duration / old_duration)
    
    def metric_modulation_chain(
        self,
        start_tempo: float,
        modulations: list[tuple[str, str]]
    ) -> list[float]:
        """Calculate tempo through a chain of metric modulations.
        
        Args:
            start_tempo: Initial tempo
            modulations: List of (old_value, new_value) tuples
        
        Returns:
            List of tempos through the chain
        
        Example:
            >>> engine.metric_modulation_chain(120, [
            ...     ("quarter", "triplet_half"),  # Quarter = triplet half
            ...     ("8th", "quarter"),           # Then 8th = quarter
            ... ])
            [120, 80, 160]
        """
        tempos = [start_tempo]
        current = start_tempo
        
        for old_val, new_val in modulations:
            current = self.calculate_metric_modulation(current, old_val, new_val)
            tempos.append(round(current, 2))
        
        return tempos
    
    # ==========================================================================
    # Polymeter
    # ==========================================================================
    
    def create_polymeter(
        self,
        meter_a: tuple[int, int],
        meter_b: tuple[int, int],
        length_bars_a: int
    ) -> dict:
        """Create polymeter pattern (two simultaneous time signatures).
        
        Unlike polyrhythm where patterns align at start/end, polymeter
        maintains different bar lengths, creating shifting accents.
        
        Args:
            meter_a: First time signature (num, denom)
            meter_b: Second time signature (num, denom)
            length_bars_a: Number of bars in meter_a's terms
        
        Returns:
            Dict with beat positions for both meters
        
        Example:
            >>> engine.create_polymeter((4, 4), (3, 4), 3)
            # 3 bars of 4/4 = 4 bars of 3/4
        """
        num_a, denom_a = meter_a
        num_b, denom_b = meter_b
        
        # Calculate bar lengths in quarter notes
        bar_length_a = num_a * (4 / denom_a)
        bar_length_b = num_b * (4 / denom_b)
        
        total_length = length_bars_a * bar_length_a
        
        # Generate beats for meter A
        beats_a = []
        beat_val_a = 4 / denom_a
        for bar in range(length_bars_a):
            bar_start = bar * bar_length_a
            for beat in range(num_a):
                beats_a.append(bar_start + beat * beat_val_a)
        
        # Generate beats for meter B
        beats_b = []
        beat_val_b = 4 / denom_b
        bars_b = int(total_length / bar_length_b) + 1
        current = 0.0
        while current < total_length:
            for beat in range(num_b):
                pos = current + beat * beat_val_b
                if pos < total_length:
                    beats_b.append(pos)
            current += bar_length_b
        
        # Find alignment points (LCM of bar lengths)
        import math
        lcm_bars = (bar_length_a * bar_length_b) / math.gcd(int(bar_length_a * 1000), int(bar_length_b * 1000)) * 1000
        
        return {
            "meter_a": meter_a,
            "meter_b": meter_b,
            "beats_a": beats_a,
            "beats_b": beats_b,
            "total_length": total_length,
            "alignment_interval": lcm_bars / 1000,
            "downbeats_a": [i * bar_length_a for i in range(length_bars_a)],
            "downbeats_b": [i * bar_length_b for i in range(bars_b) if i * bar_length_b < total_length],
        }
    
    # ==========================================================================
    # Groove Templates
    # ==========================================================================
    
    def apply_groove(
        self,
        notes: list[dict],
        groove_name: str
    ) -> list[dict]:
        """Apply a named groove template to notes.
        
        Grooves combine timing shifts, velocity accents, and
        subtle duration changes for authentic feel.
        
        Args:
            notes: List of note dicts
            groove_name: Name of groove template
        
        Returns:
            Notes with groove applied
        """
        grooves = {
            "shuffle": {"swing": 0.6, "accent_beats": [0, 2]},
            "funk_16th": {"swing": 0.3, "accent_beats": [0, 1.5, 2.5, 3.5]},
            "reggae": {"swing": 0.0, "accent_beats": [2, 4], "ghost_beats": [0.5, 1.5]},
            "jazz_swing": {"swing": 0.67, "accent_beats": [1, 3]},
            "hip_hop": {"swing": 0.5, "accent_beats": [1, 3], "lazy": 0.02},
        }
        
        groove = grooves.get(groove_name, {"swing": 0.0, "accent_beats": []})
        
        # Apply swing
        swung = self.apply_swing(notes, groove.get("swing", 0.0))
        
        # Apply accent pattern
        accent_beats = set(groove.get("accent_beats", []))
        
        result = []
        for note in swung:
            new_note = note.copy()
            start = note.get("start_time", 0) % 4  # Position within bar
            
            # Accent on specified beats
            if any(abs(start - b) < 0.1 for b in accent_beats):
                new_note["velocity"] = min(127, int(note.get("velocity", 100) * 1.2))
            
            # Lazy feel (slight delay)
            if "lazy" in groove:
                new_note["start_time"] = note.get("start_time", 0) + groove["lazy"]
            
            result.append(new_note)
        
        return result

