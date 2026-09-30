/**
 * @file meter.cpp
 * @brief Time signature and metre analysis implementation
 *
 */

#include <algorithm>
#include <sunny/core/rhythm/meter.hpp>

namespace sunny::core {

namespace {

bool is_power_of_two(int n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

} // namespace

Result<Beat> checked_measure_duration(const TimeSignature& ts) noexcept {
    return ts.measure_duration();
}

Result<TimeSignature> make_time_signature(int num, int denom) {
    if (num < 1 || denom < 1 || !is_power_of_two(denom)) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    std::vector<int> groups;

    // Compound metre: numerator divisible by 3 and >= 6
    if (num >= 6 && num % 3 == 0) {
        int beat_count = num / 3;
        groups.assign(beat_count, 3);
    } else {
        // Simple metre: each pulse is its own group
        groups.assign(num, 1);
    }

    return TimeSignature{std::move(groups), denom};
}

Result<TimeSignature> make_additive_time_signature(std::vector<int> groups, int denom) {
    return TimeSignature::from_groups(std::move(groups), denom);
}

bool has_distinct_meter_grouping(const TimeSignature& ts) noexcept {
    const auto& groups = ts.groups();
    const int numerator = ts.numerator();
    if (numerator >= 6 && numerator % 3 == 0)
        return groups.size() != static_cast<std::size_t>(numerator / 3) ||
               !std::all_of(groups.begin(), groups.end(), [](int group) { return group == 3; });
    return groups.size() != static_cast<std::size_t>(numerator) ||
           !std::all_of(groups.begin(), groups.end(), [](int group) { return group == 1; });
}

MetreType classify_metre(const TimeSignature& ts) noexcept {
    const auto& groups = ts.groups();
    bool all_equal =
        std::all_of(groups.begin(), groups.end(), [&](int group) { return group == groups[0]; });

    if (all_equal) {
        if (groups[0] == 1 || groups[0] == 2) return MetreType::Simple;
        if (groups[0] == 3) return MetreType::Compound;
        return MetreType::Complex;
    }

    // Unequal groups: check if all are 2 or 3 (aksak/asymmetric)
    bool all_2_or_3 = std::all_of(
        groups.begin(), groups.end(), [](int group) { return group == 2 || group == 3; });

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
    for (int g : ts.groups()) {
        if (pos == boundary && boundary > 0) return 3;
        boundary += g;
    }

    // Check if position is at a group boundary
    boundary = 0;
    for (int g : ts.groups()) {
        boundary += g;
        if (pos == boundary) return 3;
    }

    // For compound groups of 3, the middle subdivision is weaker than the start
    // Check if position falls on a subdivision boundary within a group
    boundary = 0;
    for (int g : ts.groups()) {
        int offset = pos - boundary;
        if (offset >= 0 && offset < g) {
            if (offset == 0) return 3; // Group start (already caught above for non-downbeat)
            // Mid-group subdivisions
            if (g >= 2 && offset == g / 2) return 1;
            return 0;
        }
        boundary += g;
    }

    return 0;
}

bool is_syncopated(const TimeSignature& ts, int onset_pos, int duration_pulses) noexcept {
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

} // namespace sunny::core
