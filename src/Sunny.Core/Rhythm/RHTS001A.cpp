/**
 * @file RHTS001A.cpp
 * @brief Time signature and metre analysis implementation
 *
 * Component: RHTS001A
 */

#include "RHTS001A.h"

#include <algorithm>

namespace Sunny::Core {

namespace {

bool is_power_of_two(int n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

}  // namespace

Result<TimeSignature> make_time_signature(int num, int denom) {
    if (num < 1 || denom < 1 || !is_power_of_two(denom)) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    TimeSignature ts;
    ts.denominator = denom;

    // Compound metre: numerator divisible by 3 and >= 6
    if (num >= 6 && num % 3 == 0) {
        int beat_count = num / 3;
        ts.groups.assign(beat_count, 3);
    } else {
        // Simple metre: each pulse is its own group
        ts.groups.assign(num, 1);
    }

    return ts;
}

Result<TimeSignature> make_additive_time_signature(
    std::vector<int> groups,
    int denom
) {
    if (groups.empty() || denom < 1 || !is_power_of_two(denom)) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    for (int g : groups) {
        if (g < 1) {
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        }
    }

    TimeSignature ts;
    ts.groups = std::move(groups);
    ts.denominator = denom;
    return ts;
}

MetreType classify_metre(const TimeSignature& ts) noexcept {
    if (ts.groups.empty()) return MetreType::Simple;

    bool all_equal = std::all_of(ts.groups.begin(), ts.groups.end(),
        [&](int g) { return g == ts.groups[0]; });

    if (all_equal) {
        if (ts.groups[0] == 1 || ts.groups[0] == 2) return MetreType::Simple;
        if (ts.groups[0] == 3) return MetreType::Compound;
        return MetreType::Complex;
    }

    // Unequal groups: check if all are 2 or 3 (aksak/asymmetric)
    bool all_2_or_3 = std::all_of(ts.groups.begin(), ts.groups.end(),
        [](int g) { return g == 2 || g == 3; });

    if (all_2_or_3) return MetreType::Asymmetric;

    return MetreType::Complex;
}

int metrical_weight(const TimeSignature& ts, int pulse_position) noexcept {
    int num = ts.numerator();
    if (num <= 0) return 0;

    // Wrap position into measure
    int pos = ((pulse_position % num) + num) % num;

    // Downbeat is strongest
    if (pos == 0) return 4;

    // Check if position is at a group boundary
    int boundary = 0;
    for (int g : ts.groups) {
        if (pos == boundary && boundary > 0) return 3;
        boundary += g;
    }

    // Check if position is at a group boundary
    boundary = 0;
    for (int g : ts.groups) {
        boundary += g;
        if (pos == boundary) return 3;
    }

    // For compound groups of 3, the middle subdivision is weaker than the start
    // Check if position falls on a subdivision boundary within a group
    boundary = 0;
    for (int g : ts.groups) {
        int offset = pos - boundary;
        if (offset >= 0 && offset < g) {
            if (offset == 0) return 3;  // Group start (already caught above for non-downbeat)
            // Mid-group subdivisions
            if (g >= 2 && offset == g / 2) return 1;
            return 0;
        }
        boundary += g;
    }

    return 0;
}

bool is_syncopated(
    const TimeSignature& ts,
    int onset_pos,
    int duration_pulses
) noexcept {
    if (duration_pulses <= 0) return false;

    int num = ts.numerator();
    if (num <= 0) return false;

    int onset_weight = metrical_weight(ts, onset_pos);

    // Check all positions the note sustains through
    for (int i = 1; i < duration_pulses; ++i) {
        int sustained_pos = (onset_pos + i) % num;
        int sustained_weight = metrical_weight(ts, sustained_pos);
        if (sustained_weight > onset_weight) {
            return true;
        }
    }

    return false;
}

}  // namespace Sunny::Core
