/**
 * @file voice_leading.cpp
 * @brief Nearest-tone voice leading implementation
 *
 */

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <optional>
#include <sunny/core/pitch/midi_note.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>

namespace sunny::core {

namespace {

// Check interval between two notes
int get_interval(MidiNote a, MidiNote b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) % 12;
}

// Check for parallel motion at specific interval
bool check_parallel(
    MidiNote prev_a, MidiNote prev_b, MidiNote next_a, MidiNote next_b, int target_ic) {
    int prev_interval = get_interval(prev_a, prev_b);
    int next_interval = get_interval(next_a, next_b);

    if (prev_interval != target_ic || next_interval != target_ic) {
        return false;
    }

    // Check for parallel (not contrary) motion
    int motion_a = static_cast<int>(next_a) - static_cast<int>(prev_a);
    int motion_b = static_cast<int>(next_b) - static_cast<int>(prev_b);

    // Parallel motion: both move in the same direction
    return (motion_a > 0 && motion_b > 0) || (motion_a < 0 && motion_b < 0);
}

bool introduces_disallowed_parallel(std::span<const MidiNote> source,
                                    std::span<const MidiNote> voiced,
                                    std::size_t voice,
                                    MidiNote candidate,
                                    bool allow_parallel_fifths,
                                    bool allow_parallel_octaves) {
    for (std::size_t other = 0; other < voice; ++other) {
        if (!allow_parallel_fifths &&
            check_parallel(source[other], source[voice], voiced[other], candidate, 7)) {
            return true;
        }
        if (!allow_parallel_octaves &&
            check_parallel(source[other], source[voice], voiced[other], candidate, 0)) {
            return true;
        }
    }
    return false;
}

// Hungarian algorithm (Kuhn-Munkres) for minimum-weight assignment.
// Input: n×n cost matrix. Output: assignment[i] = j.
std::vector<int> hungarian_assign(const std::vector<std::vector<int>>& cost) {
    int n = static_cast<int>(cost.size());
    if (n == 0) return {};

    std::vector<int> u(n + 1, 0), v(n + 1, 0);
    std::vector<int> p(n + 1, 0);
    std::vector<int> way(n + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<int> minv(n + 1, INT_MAX);
        std::vector<bool> used(n + 1, false);

        do {
            used[j0] = true;
            int i0 = p[j0];
            int delta = INT_MAX;
            int j1 = -1;

            for (int j = 1; j <= n; ++j) {
                if (!used[j]) {
                    int cur = cost[i0 - 1][j - 1] - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }

            for (int j = 0; j <= n; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }

    std::vector<int> assignment(n);
    for (int j = 1; j <= n; ++j) {
        if (p[j] > 0) {
            assignment[p[j] - 1] = j - 1;
        }
    }
    return assignment;
}

// Exact branch-and-bound search for voice_lead_nearest_tone.
//
// Voices are placed from the bass upwards. A partial voicing is extended
// only by pitches above the voice below (strict order), of a target pitch
// class not yet assigned, that form no disallowed parallel with any voice
// already placed. The bound is the cost so far plus, for every unplaced
// voice, its distance to the nearest pitch of any target class; that sum
// never exceeds the true completion cost, so no pruned branch can hold a
// better voicing. Candidates are tried in order of (motion, pitch, target
// index), and only a strictly cheaper voicing replaces the incumbent, so the
// result is deterministic. The worst case is exponential in the voice count.
class NearestToneSearch {
  public:
    NearestToneSearch(std::span<const MidiNote> source,
                      std::span<const PitchClass> targets,
                      bool lock_bass,
                      bool allow_parallel_fifths,
                      bool allow_parallel_octaves)
        : source_(source), targets_(targets), lock_bass_(lock_bass),
          allow_parallel_fifths_(allow_parallel_fifths),
          allow_parallel_octaves_(allow_parallel_octaves), placed_(source.size()),
          used_(targets.size(), false), remaining_bound_(source.size() + 1, 0) {
        for (std::size_t voice = source.size(); voice-- > 0;) {
            int nearest = std::numeric_limits<int>::max();
            for (auto pc : targets) {
                const int up =
                    ((static_cast<int>(pc) - static_cast<int>(source[voice])) % 12 + 12) % 12;
                nearest = std::min(nearest, std::min(up, 12 - up));
            }
            remaining_bound_[voice] = remaining_bound_[voice + 1] + nearest;
        }
    }

    [[nodiscard]] std::optional<std::vector<MidiNote>> run() {
        place(0, 0);
        if (best_cost_ == std::numeric_limits<int>::max()) return std::nullopt;
        return best_;
    }

  private:
    struct Candidate {
        int motion;
        int pitch;
        std::size_t target;
        auto operator<=>(const Candidate&) const = default;
    };

    std::span<const MidiNote> source_;
    std::span<const PitchClass> targets_;
    bool lock_bass_;
    bool allow_parallel_fifths_;
    bool allow_parallel_octaves_;
    std::vector<MidiNote> placed_;
    std::vector<bool> used_;
    std::vector<int> remaining_bound_;
    std::vector<MidiNote> best_;
    int best_cost_ = std::numeric_limits<int>::max();

    // Exceeds any total motion (at most 128 voices times 127 semitones) and
    // leaves headroom for the Hungarian potentials.
    static constexpr int UNREACHABLE = 1 << 20;

    [[nodiscard]] bool forms_disallowed_parallel(std::size_t voice, MidiNote candidate) const {
        std::span<const MidiNote> placed(placed_.data(), voice);
        return introduces_disallowed_parallel(
            source_, placed, voice, candidate, allow_parallel_fifths_, allow_parallel_octaves_);
    }

    // Lower bound on the motion of voices voice..n-1: a minimum-cost
    // assignment of those voices to the unused targets, where a voice may
    // take any pitch of the target class that leaves room below it for the
    // voices already placed and above it for the voices still to come. Only
    // the mutual order of the unplaced voices and the parallel rule are
    // relaxed, so the bound never exceeds the true completion cost.
    [[nodiscard]] int assignment_bound(std::size_t voice, int floor) const {
        const std::size_t n = source_.size();
        const std::size_t remaining = n - voice;
        std::vector<std::size_t> free_targets;
        for (std::size_t target = 0; target < targets_.size(); ++target)
            if (!used_[target] && !(lock_bass_ && target == 0 && voice > 0))
                free_targets.push_back(target);
        if (free_targets.size() != remaining) return UNREACHABLE;

        std::vector<std::vector<int>> cost(remaining, std::vector<int>(remaining, UNREACHABLE));
        for (std::size_t row = 0; row < remaining; ++row) {
            const std::size_t v = voice + row;
            const int low = floor + static_cast<int>(row);
            const int high = 127 - static_cast<int>(n - 1 - v);
            const int origin = static_cast<int>(source_[v]);
            for (std::size_t col = 0; col < remaining; ++col) {
                const std::size_t target = free_targets[col];
                if (lock_bass_ && (v == 0) != (target == 0)) continue;
                for (int pitch = static_cast<int>(targets_[target]); pitch <= high; pitch += 12) {
                    if (pitch >= low)
                        cost[row][col] = std::min(cost[row][col], std::abs(pitch - origin));
                }
            }
        }
        const auto assignment = hungarian_assign(cost);
        int bound = 0;
        for (std::size_t row = 0; row < remaining; ++row) {
            const int entry = cost[row][static_cast<std::size_t>(assignment[row])];
            if (entry >= UNREACHABLE) return UNREACHABLE;
            bound += entry;
        }
        return bound;
    }

    void place(std::size_t voice, int cost) {
        if (voice == source_.size()) {
            if (cost < best_cost_) {
                best_cost_ = cost;
                best_ = placed_;
            }
            return;
        }

        const int floor = voice == 0 ? 0 : static_cast<int>(placed_[voice - 1]) + 1;
        const int bound = assignment_bound(voice, floor);
        if (bound >= UNREACHABLE || cost + bound >= best_cost_) return;
        const int origin = static_cast<int>(source_[voice]);
        std::vector<Candidate> candidates;
        for (std::size_t target = 0; target < targets_.size(); ++target) {
            if (used_[target]) continue;
            if (lock_bass_ && (voice == 0) != (target == 0)) continue;
            // Equal pitch classes are interchangeable; try only the first unused one.
            bool duplicate = false;
            for (std::size_t earlier = 0; earlier < target && !duplicate; ++earlier)
                duplicate = !used_[earlier] && targets_[earlier] == targets_[target] &&
                            !(lock_bass_ && earlier == 0);
            if (duplicate) continue;
            for (int pitch = static_cast<int>(targets_[target]); pitch <= 127; pitch += 12) {
                if (pitch < floor) continue;
                candidates.push_back({std::abs(pitch - origin), pitch, target});
            }
        }
        std::sort(candidates.begin(), candidates.end());

        for (const auto& candidate : candidates) {
            // Candidates are sorted by motion, so once the bound fails it
            // fails for every later candidate.
            if (cost + candidate.motion + remaining_bound_[voice + 1] >= best_cost_) break;
            const auto pitch = MidiNote::from_int(candidate.pitch);
            if (!pitch || forms_disallowed_parallel(voice, *pitch)) continue;
            placed_[voice] = *pitch;
            used_[candidate.target] = true;
            place(voice + 1, cost + candidate.motion);
            used_[candidate.target] = false;
        }
    }
};

} // namespace

Result<VoiceLeadingResult> voice_lead_nearest_tone(std::span<const MidiNote> source_pitches,
                                                   std::span<const PitchClass> target_pitch_classes,
                                                   bool lock_bass,
                                                   bool allow_parallel_fifths,
                                                   bool allow_parallel_octaves) {
    if (source_pitches.empty()) {
        return VoiceLeadingResult{{}, 0, false, false};
    }

    if (target_pitch_classes.empty()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    if (source_pitches.size() != target_pitch_classes.size()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    NearestToneSearch search(source_pitches,
                             target_pitch_classes,
                             lock_bass,
                             allow_parallel_fifths,
                             allow_parallel_octaves);
    auto voiced = search.run();
    if (!voiced) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    VoiceLeadingResult result;
    result.voiced_notes = std::move(*voiced);
    result.total_motion = 0;
    result.has_parallel_fifths = false;
    result.has_parallel_octaves = false;
    for (std::size_t i = 0; i < source_pitches.size(); ++i) {
        result.total_motion += std::abs(static_cast<int>(result.voiced_notes[i]) -
                                        static_cast<int>(source_pitches[i]));
    }

    // Report parallels that the flags permitted.
    for (std::size_t i = 0; i < source_pitches.size(); ++i) {
        for (std::size_t j = i + 1; j < source_pitches.size(); ++j) {
            if (check_parallel(source_pitches[i],
                               source_pitches[j],
                               result.voiced_notes[i],
                               result.voiced_notes[j],
                               7)) {
                result.has_parallel_fifths = true;
            }
            if (check_parallel(source_pitches[i],
                               source_pitches[j],
                               result.voiced_notes[i],
                               result.voiced_notes[j],
                               0)) {
                result.has_parallel_octaves = true;
            }
        }
    }

    return result;
}

std::vector<MidiNote> generate_close_voicing(std::span<const PitchClass> pitch_classes,
                                             int root_octave) {
    if (pitch_classes.empty()) {
        return {};
    }

    std::vector<MidiNote> voicing;
    voicing.reserve(pitch_classes.size());

    auto base = pitch_octave_to_midi(pitch_classes[0], root_octave);
    if (!base) {
        return {};
    }

    voicing.push_back(*base);

    // Add remaining notes in ascending order within one octave
    for (std::size_t i = 1; i < pitch_classes.size(); ++i) {
        MidiNote last = voicing.back();
        PitchClass target_pc = pitch_classes[i];

        // Find next occurrence of this PC above the last note; adjust in
        // int space, then store through the validated factory. The +12/-12
        // steps keep the value within [0, 139] and the final correction
        // returns it to [0, 127], so the fallback to the uncorrected
        // candidate is unreachable.
        MidiNote candidate = closest_pitch_class_midi(last, target_pc);
        int candidate_val = candidate;
        if (candidate_val <= last) {
            candidate_val += 12;
        }
        if (candidate_val > Constants::MIDI_NOTE_MAX) {
            candidate_val -= 12; // Wrap if needed
        }

        voicing.push_back(MidiNote::from_int(candidate_val).value_or(candidate));
    }

    return voicing;
}

Result<std::vector<MidiNote>> generate_drop2_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 4) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop the second-from-top note down an octave; the factory refuses
    // the drop below MIDI 0, matching the old >= 12 guard.
    std::size_t drop_idx = result.size() - 2;
    if (auto dropped = MidiNote::from_int(result[drop_idx] - 12)) {
        result[drop_idx] = *dropped;
    }

    // Re-sort to maintain ascending order
    std::sort(result.begin(), result.end());

    return result;
}

Result<std::vector<MidiNote>> generate_drop3_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 4) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop the third-from-top note down an octave; the factory refuses
    // the drop below MIDI 0, matching the old >= 12 guard.
    std::size_t drop_idx = result.size() - 3;
    if (auto dropped = MidiNote::from_int(result[drop_idx] - 12)) {
        result[drop_idx] = *dropped;
    }

    std::sort(result.begin(), result.end());

    return result;
}

std::vector<MidiNote> generate_open_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 2) {
        return std::vector<MidiNote>(close_voicing.begin(), close_voicing.end());
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop every other voice (index 1, 3, 5, ...) down an octave; the
    // factory refuses drops below MIDI 0, matching the old >= 12 guard.
    for (std::size_t i = 1; i < result.size(); i += 2) {
        if (auto dropped = MidiNote::from_int(result[i] - 12)) {
            result[i] = *dropped;
        }
    }

    std::sort(result.begin(), result.end());

    return result;
}

Result<std::vector<MidiNote>> generate_drop24_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 4) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop 2nd-from-top and 4th-from-top voices down an octave
    std::size_t drop2_idx = result.size() - 2;
    std::size_t drop4_idx = result.size() - 4;

    if (auto dropped = MidiNote::from_int(result[drop2_idx] - 12)) {
        result[drop2_idx] = *dropped;
    }
    if (auto dropped = MidiNote::from_int(result[drop4_idx] - 12)) {
        result[drop4_idx] = *dropped;
    }

    std::sort(result.begin(), result.end());

    return result;
}

Result<std::vector<MidiNote>> generate_spread_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 3) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Bass note must be >= octave below the next voice.
    // Drop bass until the gap is at least 12 semitones; the factory
    // refuses drops below MIDI 0, matching the old >= 12 guard.
    while (result.size() >= 2 && (result[1] - result[0]) < 12) {
        if (auto dropped = MidiNote::from_int(result[0] - 12)) {
            result[0] = *dropped;
        } else {
            break; // Cannot drop further
        }
    }

    // Already sorted since only the lowest note moved lower
    return result;
}

VoiceMotionType classify_voice_motion(MidiNote a1, MidiNote a2, MidiNote b1, MidiNote b2) {
    int motion_a = static_cast<int>(a2) - static_cast<int>(a1);
    int motion_b = static_cast<int>(b2) - static_cast<int>(b1);

    if (motion_a == 0 && motion_b == 0) {
        return VoiceMotionType::Static;
    }
    if (motion_a == 0 || motion_b == 0) {
        return VoiceMotionType::Oblique;
    }
    // Both voices move — check direction
    bool same_dir = (motion_a > 0) == (motion_b > 0);
    if (!same_dir) {
        return VoiceMotionType::Contrary;
    }
    // Same direction — parallel if same displacement, similar otherwise
    if (motion_a == motion_b) {
        return VoiceMotionType::Parallel;
    }
    return VoiceMotionType::Similar;
}

Result<VoiceLeadingResult> voice_lead_optimal(std::span<const MidiNote> source_pitches,
                                              std::span<const PitchClass> target_pitch_classes,
                                              bool lock_bass) {
    if (source_pitches.empty()) {
        return VoiceLeadingResult{{}, 0, false, false};
    }
    if (target_pitch_classes.empty()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    if (source_pitches.size() != target_pitch_classes.size()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::size_t num_voices = source_pitches.size();

    std::vector<PitchClass> targets(target_pitch_classes.begin(), target_pitch_classes.end());

    // For each (source_voice, target_slot), compute cost as the minimum
    // semitone distance to a MIDI note with the target pitch class.
    int n = static_cast<int>(num_voices);
    std::vector<std::vector<int>> cost(n, std::vector<int>(n));
    // Store the actual MIDI note each assignment would produce
    std::vector<std::vector<MidiNote>> candidates(n, std::vector<MidiNote>(n));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            MidiNote nearest = closest_pitch_class_midi(source_pitches[i], targets[j]);
            int dist = std::abs(static_cast<int>(nearest) - static_cast<int>(source_pitches[i]));
            cost[i][j] = dist;
            candidates[i][j] = nearest;
        }
    }

    // Lock bass: force voice 0 to target slot 0
    if (lock_bass && n > 1) {
        // Set cost[0][j] = very high for j != 0, and cost[i][0] = very high for i != 0
        for (int j = 1; j < n; ++j)
            cost[0][j] = 10000;
        for (int i = 1; i < n; ++i)
            cost[i][0] = 10000;
    }

    auto assignment = hungarian_assign(cost);

    VoiceLeadingResult result;
    result.voiced_notes.resize(num_voices);
    result.total_motion = 0;
    result.has_parallel_fifths = false;
    result.has_parallel_octaves = false;

    for (int i = 0; i < n; ++i) {
        result.voiced_notes[i] = candidates[i][assignment[i]];
        result.total_motion += cost[i][assignment[i]];
    }

    // Fix voice crossings (ensure ascending order). The factory calls
    // encode the old range guards, as in resolve_voices above.
    for (std::size_t i = 1; i < result.voiced_notes.size(); ++i) {
        while (result.voiced_notes[i] <= result.voiced_notes[i - 1]) {
            if (auto raised = MidiNote::from_int(result.voiced_notes[i] + 12)) {
                result.voiced_notes[i] = *raised;
            } else if (auto lowered = MidiNote::from_int(result.voiced_notes[i - 1] - 12)) {
                result.voiced_notes[i - 1] = *lowered;
            } else {
                break;
            }
        }
    }

    // Recalculate total motion after crossing fix
    result.total_motion = 0;
    for (std::size_t i = 0; i < num_voices; ++i) {
        result.total_motion += std::abs(static_cast<int>(result.voiced_notes[i]) -
                                        static_cast<int>(source_pitches[i]));
    }

    // Check for parallel fifths and octaves
    if (num_voices >= 2) {
        for (std::size_t i = 0; i < num_voices; ++i) {
            for (std::size_t j = i + 1; j < num_voices; ++j) {
                if (check_parallel(source_pitches[i],
                                   source_pitches[j],
                                   result.voiced_notes[i],
                                   result.voiced_notes[j],
                                   7)) {
                    result.has_parallel_fifths = true;
                }
                if (check_parallel(source_pitches[i],
                                   source_pitches[j],
                                   result.voiced_notes[i],
                                   result.voiced_notes[j],
                                   0)) {
                    result.has_parallel_octaves = true;
                }
            }
        }
    }

    return result;
}

bool has_parallel_motion(MidiNote prev_bass,
                         MidiNote prev_upper,
                         MidiNote next_bass,
                         MidiNote next_upper,
                         int interval_class) {
    return check_parallel(prev_bass, prev_upper, next_bass, next_upper, interval_class);
}

} // namespace sunny::core
