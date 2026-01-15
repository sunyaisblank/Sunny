"""Sunny - Voice Leading Engine.

Component: THVL001A
Domain: TH (Theory) | Category: VL (Voice Leading)

Professional voice leading algorithms for smooth chord progressions:
- Nearest-tone algorithm for minimal voice movement
- Part writing rules (parallel 5ths/octaves detection)
- Leading tone and seventh resolution
- Optimal voicing generation

Bounds:
    - voicing: list[int] where ∀ n: n ∈ [0, 127]
    - num_voices: int ∈ [2, 8]
    - inversion: int ∈ [0, len(chord) - 1]

Error Codes:
    - 1330: Invalid voicing
    - 1331: Voice crossing detected
    - 1332: Parallel fifths/octaves detected
"""

from __future__ import annotations

import logging
from typing import NamedTuple

logger = logging.getLogger("sunny.theory.voiceleading")


class VoiceMotion(NamedTuple):
    """Description of motion between two voices.
    
    Component: THVL001A.VoiceMotion
    """
    
    voice_index: int
    start_pitch: int
    end_pitch: int
    interval: int  # Positive = up, negative = down


class VoiceLeadingViolation(NamedTuple):
    """Voice leading rule violation.
    
    Component: THVL001A.VoiceLeadingViolation
    """
    
    rule: str
    voice1: int
    voice2: int
    description: str


class VoiceLeadingEngine:
    """Voice leading engine for smooth progressions.
    
    Component: THVL001A
    
    Implements professional part-writing techniques:
    - Nearest-tone voicing for smooth transitions
    - Detection of parallel 5ths and octaves
    - Proper resolution of tendency tones
    - Voice crossing and spacing checks
    
    Bounds:
        - voicing: list[int] where ∀ n: n ∈ [0, 127]
        - num_voices: int ∈ [2, 8]
        - max_interval (spacing): int ∈ [6, 24] semitones
    
    Error Codes:
        - 1330: Invalid voicing (empty or malformed)
        - 1331: Voice crossing detected
        - 1332: Parallel fifths detected
        - 1333: Parallel octaves detected
    """
    
    # Common intervals
    PERFECT_UNISON = 0
    MINOR_SECOND = 1
    MAJOR_SECOND = 2
    MINOR_THIRD = 3
    MAJOR_THIRD = 4
    PERFECT_FOURTH = 5
    TRITONE = 6
    PERFECT_FIFTH = 7
    MINOR_SIXTH = 8
    MAJOR_SIXTH = 9
    MINOR_SEVENTH = 10
    MAJOR_SEVENTH = 11
    OCTAVE = 12
    
    # ==========================================================================
    # Core Voice Leading Algorithm
    # ==========================================================================
    
    def nearest_tone_voicing(
        self,
        current_voicing: list[int],
        target_chord: list[int],
        lock_bass: bool = True
    ) -> list[int]:
        """Generate optimal voicing using nearest-tone algorithm.
        
        For each voice in the current chord, find the closest pitch
        in the target chord, minimizing total voice movement.
        
        Args:
            current_voicing: Current MIDI pitches (bass to soprano)
            target_chord: Target chord pitch classes or MIDI notes
            lock_bass: If True, bass always takes chord root
        
        Returns:
            New voicing with minimal voice movement
        
        Example:
            >>> vl.nearest_tone_voicing([60, 64, 67], [62, 65, 69])
            [62, 65, 69]  # D minor from C major with smooth motion
        """
        if not current_voicing or not target_chord:
            return list(target_chord) if target_chord else []
        
        num_voices = len(current_voicing)
        
        # Get target pitch classes
        target_pcs = [(p % 12) for p in target_chord]
        
        # Ensure we have at least as many targets as voices
        if len(target_pcs) < num_voices:
            # Duplicate pitch classes in different octaves
            while len(target_pcs) < num_voices:
                target_pcs.append(target_pcs[len(target_pcs) % len(target_chord)])
        
        new_voicing = []
        used_targets = set()
        
        # Process voices from bass to soprano
        for i, current_pitch in enumerate(current_voicing):
            if lock_bass and i == 0:
                # Bass takes the root (first target)
                target_pc = target_pcs[0]
                # Find closest octave to current pitch
                new_pitch = self._closest_pitch_class(current_pitch, target_pc)
                new_voicing.append(new_pitch)
                used_targets.add(0)
                continue
            
            # Find closest available target
            best_pitch = current_pitch
            best_distance = float('inf')
            best_target_idx = 0
            
            for j, target_pc in enumerate(target_pcs):
                if j in used_targets and len(used_targets) < len(target_pcs):
                    continue
                
                candidate = self._closest_pitch_class(current_pitch, target_pc)
                distance = abs(candidate - current_pitch)
                
                if distance < best_distance:
                    best_distance = distance
                    best_pitch = candidate
                    best_target_idx = j
            
            new_voicing.append(best_pitch)
            used_targets.add(best_target_idx)
        
        # Check and fix voice crossings
        new_voicing = self._fix_voice_crossings(new_voicing)
        
        return new_voicing
    
    def _closest_pitch_class(self, reference: int, target_pc: int) -> int:
        """Find the MIDI pitch closest to reference with given pitch class."""
        reference_octave = reference // 12
        
        candidates = [
            reference_octave * 12 + target_pc,
            (reference_octave - 1) * 12 + target_pc,
            (reference_octave + 1) * 12 + target_pc,
        ]
        
        # Filter valid MIDI range
        candidates = [c for c in candidates if 0 <= c <= 127]
        
        return min(candidates, key=lambda x: abs(x - reference))
    
    def _fix_voice_crossings(self, voicing: list[int]) -> list[int]:
        """Fix any voice crossings in a voicing.
        
        Ensures that each voice is higher than the previous one
        (bass to soprano order).
        """
        fixed = list(voicing)
        
        for i in range(1, len(fixed)):
            if fixed[i] <= fixed[i - 1]:
                # Move up by octave until no crossing
                while fixed[i] <= fixed[i - 1]:
                    fixed[i] += 12
                    if fixed[i] > 127:
                        # Can't fix, try moving lower voice down
                        fixed[i] -= 12
                        fixed[i - 1] -= 12
                        break
        
        return fixed
    
    # ==========================================================================
    # Part Writing Rules
    # ==========================================================================
    
    def check_parallel_fifths(
        self,
        voicing1: list[int],
        voicing2: list[int]
    ) -> list[VoiceLeadingViolation]:
        """Check for parallel perfect fifths between two voicings.
        
        Parallel P5s occur when two voices move in the same direction
        while maintaining a perfect fifth interval. This is generally
        avoided in classical voice leading.
        
        Args:
            voicing1: First voicing (MIDI pitches)
            voicing2: Second voicing (MIDI pitches)
        
        Returns:
            List of violations found
        """
        violations = []
        
        for i in range(len(voicing1)):
            for j in range(i + 1, len(voicing1)):
                if j >= len(voicing2) or i >= len(voicing2):
                    continue
                
                # Check interval in first chord
                interval1 = abs(voicing1[j] - voicing1[i]) % 12
                
                # Check interval in second chord
                interval2 = abs(voicing2[j] - voicing2[i]) % 12
                
                # Both perfect fifths?
                if interval1 == self.PERFECT_FIFTH and interval2 == self.PERFECT_FIFTH:
                    # Check for parallel motion (both move same direction)
                    motion_i = voicing2[i] - voicing1[i]
                    motion_j = voicing2[j] - voicing1[j]
                    
                    if motion_i != 0 and motion_j != 0:
                        if (motion_i > 0 and motion_j > 0) or (motion_i < 0 and motion_j < 0):
                            violations.append(VoiceLeadingViolation(
                                rule="parallel_fifths",
                                voice1=i,
                                voice2=j,
                                description=f"Parallel P5 between voices {i} and {j}",
                            ))
        
        return violations
    
    def check_parallel_octaves(
        self,
        voicing1: list[int],
        voicing2: list[int]
    ) -> list[VoiceLeadingViolation]:
        """Check for parallel octaves/unisons between two voicings.
        
        Similar to parallel fifths, parallel octaves reduce voice
        independence and are avoided in classical writing.
        
        Args:
            voicing1: First voicing
            voicing2: Second voicing
        
        Returns:
            List of violations found
        """
        violations = []
        
        for i in range(len(voicing1)):
            for j in range(i + 1, len(voicing1)):
                if j >= len(voicing2) or i >= len(voicing2):
                    continue
                
                interval1 = abs(voicing1[j] - voicing1[i]) % 12
                interval2 = abs(voicing2[j] - voicing2[i]) % 12
                
                # Both unisons/octaves?
                if interval1 == self.PERFECT_UNISON and interval2 == self.PERFECT_UNISON:
                    motion_i = voicing2[i] - voicing1[i]
                    motion_j = voicing2[j] - voicing1[j]
                    
                    if motion_i != 0 and motion_j != 0:
                        if (motion_i > 0 and motion_j > 0) or (motion_i < 0 and motion_j < 0):
                            violations.append(VoiceLeadingViolation(
                                rule="parallel_octaves",
                                voice1=i,
                                voice2=j,
                                description=f"Parallel octaves between voices {i} and {j}",
                            ))
        
        return violations
    
    def check_voice_crossing(self, voicing: list[int]) -> list[VoiceLeadingViolation]:
        """Check for voice crossings in a voicing.
        
        Voice crossing occurs when a lower voice is higher in pitch
        than an upper voice. Generally avoided for clarity.
        
        Args:
            voicing: Voicing to check (should be bass to soprano)
        
        Returns:
            List of violations
        """
        violations = []
        
        for i in range(len(voicing) - 1):
            if voicing[i] > voicing[i + 1]:
                violations.append(VoiceLeadingViolation(
                    rule="voice_crossing",
                    voice1=i,
                    voice2=i + 1,
                    description=f"Voice {i} crosses above voice {i + 1}",
                ))
        
        return violations
    
    def check_spacing(
        self,
        voicing: list[int],
        max_interval: int = 12
    ) -> list[VoiceLeadingViolation]:
        """Check for excessive spacing between adjacent voices.
        
        In choral writing, upper voices (S-A-T) should be within
        an octave of each other. Bass can be further.
        
        Args:
            voicing: Voicing to check
            max_interval: Maximum interval between adjacent upper voices
        
        Returns:
            List of violations
        """
        violations = []
        
        # Check upper voices (skip bass-tenor, start from index 1)
        for i in range(1, len(voicing) - 1):
            spacing = voicing[i + 1] - voicing[i]
            if spacing > max_interval:
                violations.append(VoiceLeadingViolation(
                    rule="excessive_spacing",
                    voice1=i,
                    voice2=i + 1,
                    description=f"Spacing of {spacing} semitones between voices {i} and {i + 1}",
                ))
        
        return violations
    
    def check_all_rules(
        self,
        voicing1: list[int],
        voicing2: list[int]
    ) -> list[VoiceLeadingViolation]:
        """Run all voice leading checks between two voicings.
        
        Args:
            voicing1: First voicing
            voicing2: Second voicing
        
        Returns:
            Combined list of all violations
        """
        violations = []
        violations.extend(self.check_parallel_fifths(voicing1, voicing2))
        violations.extend(self.check_parallel_octaves(voicing1, voicing2))
        violations.extend(self.check_voice_crossing(voicing2))
        violations.extend(self.check_spacing(voicing2))
        return violations
    
    # ==========================================================================
    # Smooth Progression Generation
    # ==========================================================================
    
    def generate_smooth_progression(
        self,
        chords: list[list[int]],
        starting_voicing: list[int] | None = None,
        num_voices: int = 4
    ) -> list[list[int]]:
        """Generate a complete progression with smooth voice leading.
        
        Applies nearest-tone algorithm to each chord transition,
        ensuring smooth, musical voice movement throughout.
        
        Args:
            chords: List of chords (each as list of pitch classes or MIDI)
            starting_voicing: Initial voicing (or None to create one)
            num_voices: Number of voices to use
        
        Returns:
            List of voicings for the entire progression
        """
        if not chords:
            return []
        
        voicings = []
        
        # Create starting voicing if not provided
        if starting_voicing is None:
            first_chord = chords[0]
            starting_voicing = self._create_initial_voicing(first_chord, num_voices)
        
        voicings.append(starting_voicing)
        
        # Generate each subsequent voicing
        current = starting_voicing
        for chord in chords[1:]:
            new_voicing = self.nearest_tone_voicing(current, chord)
            voicings.append(new_voicing)
            current = new_voicing
        
        return voicings
    
    def _create_initial_voicing(
        self,
        chord: list[int],
        num_voices: int,
        base_octave: int = 3
    ) -> list[int]:
        """Create an initial voicing for a chord.
        
        Spreads voices across a reasonable range starting from base_octave.
        """
        # Get pitch classes
        pcs = [(p % 12) for p in chord]
        
        # Ensure we have enough pitch classes
        while len(pcs) < num_voices:
            pcs.append(pcs[len(pcs) % len(chord)])
        
        voicing = []
        current_octave = base_octave
        
        for i, pc in enumerate(pcs[:num_voices]):
            pitch = current_octave * 12 + pc
            
            # Ensure ascending
            if voicing and pitch <= voicing[-1]:
                pitch = ((voicing[-1] // 12) + 1) * 12 + pc
            
            voicing.append(pitch)
        
        return voicing
    
    # ==========================================================================
    # Tendency Tone Resolution
    # ==========================================================================
    
    def resolve_leading_tone(
        self,
        leading_tone: int,
        key_root: int
    ) -> int:
        """Resolve a leading tone up to the tonic.
        
        The leading tone (7th scale degree) has a strong tendency
        to resolve upward by a half step to the tonic.
        
        Args:
            leading_tone: MIDI pitch of leading tone
            key_root: Pitch class of the key root (0-11)
        
        Returns:
            Resolution pitch (tonic in same or next octave)
        """
        return leading_tone + 1  # Up by half step
    
    def resolve_seventh(
        self,
        seventh: int,
        chord_root: int
    ) -> int:
        """Resolve a chord seventh downward.
        
        The 7th of a chord typically resolves down by step
        to the 3rd of the resolution chord.
        
        Args:
            seventh: MIDI pitch of the seventh
            chord_root: Root of the chord containing the seventh
        
        Returns:
            Resolution pitch (down by step)
        """
        # Dominant 7th (minor 7th) resolves down by half step to 3rd
        # Major 7th resolves down by half or whole step
        return seventh - 1  # Most common: down by half step
    
    def apply_tendency_resolution(
        self,
        voicing: list[int],
        target_chord: list[int],
        key_root: int
    ) -> list[int]:
        """Apply proper resolution to tendency tones.
        
        Identifies leading tones and sevenths in the voicing and
        ensures they resolve correctly.
        
        Args:
            voicing: Current voicing with tendency tones
            target_chord: Target chord to resolve to
            key_root: Key root pitch class
        
        Returns:
            Resolved voicing
        """
        resolved = list(voicing)
        
        # Identify leading tone (half step below tonic)
        leading_tone_pc = (key_root - 1) % 12
        
        for i, pitch in enumerate(voicing):
            pc = pitch % 12
            
            # Leading tone resolution
            if pc == leading_tone_pc:
                # Check if tonic is in target
                if key_root in [(p % 12) for p in target_chord]:
                    resolved[i] = pitch + 1  # Resolve up
            
            # Seventh resolution (more complex, requires chord analysis)
            # This is simplified - full implementation would analyze chord structure
        
        return resolved
    
    # ==========================================================================
    # Inversions
    # ==========================================================================
    
    def get_inversion(
        self,
        chord: list[int],
        inversion: int
    ) -> list[int]:
        """Get a specific inversion of a chord.
        
        Args:
            chord: Chord notes (root position)
            inversion: 0 = root position, 1 = first, 2 = second, etc.
        
        Returns:
            Chord in specified inversion
        """
        if not chord or inversion == 0:
            return list(chord)
        
        result = list(chord)
        
        for _ in range(inversion % len(chord)):
            # Move bottom note up an octave
            result[0] += 12
            result.sort()
        
        return result
    
    def optimal_inversion_for_bass(
        self,
        chord: list[int],
        target_bass_pc: int
    ) -> list[int]:
        """Find the inversion that puts a specific note in the bass.
        
        Args:
            chord: Chord notes
            target_bass_pc: Desired bass pitch class
        
        Returns:
            Chord in the inversion with target bass
        """
        for inversion in range(len(chord)):
            inverted = self.get_inversion(chord, inversion)
            if inverted[0] % 12 == target_bass_pc % 12:
                return inverted
        
        # Target not in chord, return root position
        return list(chord)
