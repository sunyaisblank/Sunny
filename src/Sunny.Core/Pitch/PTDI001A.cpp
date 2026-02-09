/**
 * @file PTDI001A.cpp
 * @brief Diatonic interval non-constexpr implementations
 *
 * Component: PTDI001A
 */

#include "PTDI001A.h"

namespace Sunny::Core {

std::string quality(DiatonicInterval di) {
    int d = di.diatonic;
    int dev = deviation(di);

    // For descending intervals, work with the absolute diatonic value
    // but preserve sign of deviation
    int d_abs = d < 0 ? -d : d;
    int d_mod7 = d_abs % 7;

    // Descending intervals: deviation sign flips relative to the negated interval
    if (d < 0) {
        dev = -dev;
    }

    bool perfect = is_perfect_family(d_mod7);

    if (perfect) {
        if (dev == 0) return "P";
        if (dev > 0) return std::string(dev, 'A');
        return std::string(-dev, 'd');
    } else {
        if (dev == 0) return "M";
        if (dev == -1) return "m";
        if (dev > 0) return std::string(dev, 'A');
        // dev <= -2: diminished (offset from minor, so -2 = d, -3 = dd, etc.)
        return std::string(-dev - 1, 'd');
    }
}

std::string quality_abbreviation(DiatonicInterval di) {
    std::string q = quality(di);
    int num = interval_number(di);
    return q + std::to_string(num);
}

Result<DiatonicInterval> from_quality_and_number(std::string_view s) {
    if (s.empty()) {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);
    }

    // Parse quality prefix
    std::size_t pos = 0;
    int dev = 0;

    if (s[pos] == 'P') {
        dev = 0;
        ++pos;
    } else if (s[pos] == 'M') {
        dev = 0;
        ++pos;
    } else if (s[pos] == 'm') {
        dev = -1;
        ++pos;
    } else if (s[pos] == 'A') {
        int count = 0;
        while (pos < s.size() && s[pos] == 'A') {
            ++count;
            ++pos;
        }
        dev = count;
    } else if (s[pos] == 'd') {
        int count = 0;
        while (pos < s.size() && s[pos] == 'd') {
            ++count;
            ++pos;
        }
        // For perfect family: dev = -count
        // For imperfect family: dev = -(count + 1)
        // Resolved after parsing the number
        dev = -count;
    } else {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);
    }

    // Parse interval number
    if (pos >= s.size()) {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);
    }

    int number = 0;
    bool has_digit = false;
    while (pos < s.size()) {
        if (s[pos] >= '0' && s[pos] <= '9') {
            number = number * 10 + (s[pos] - '0');
            has_digit = true;
            ++pos;
        } else {
            return std::unexpected(ErrorCode::InvalidIntervalQuality);
        }
    }

    if (!has_digit || number < 1) {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);
    }

    // Diatonic displacement = number - 1
    int diatonic = number - 1;
    int d_mod7 = diatonic % 7;
    bool is_pf = is_perfect_family(d_mod7);

    // Validate quality-family compatibility
    char first_char = s[0];
    if (first_char == 'P' && !is_pf) {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);  // P3 is invalid
    }
    if ((first_char == 'M' || first_char == 'm') && is_pf) {
        return std::unexpected(ErrorCode::InvalidIntervalQuality);  // m5 is invalid
    }

    // Adjust deviation for A/d with imperfect family
    if (first_char == 'A') {
        // dev is already correct for both families (augmented = +count)
    } else if (first_char == 'd') {
        if (!is_pf) {
            // Imperfect family: d = minor - 1, so dev needs one extra
            dev = dev - 1;  // dev was -count, now -(count+1)
        }
    }

    int chromatic = default_chromatic_size(diatonic) + dev;
    return DiatonicInterval{chromatic, diatonic};
}

}  // namespace Sunny::Core
