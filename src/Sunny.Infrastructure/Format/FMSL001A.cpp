/**
 * @file FMSL001A.cpp
 * @brief Scala tuning file (.scl) reader/writer implementation
 *
 * Component: FMSL001A
 */

#include "FMSL001A.h"

#include <cmath>
#include <charconv>
#include <sstream>
#include <iomanip>

namespace Sunny::Infrastructure::Format {

double ratio_to_cents(int num, int den) {
    if (num <= 0 || den <= 0) return 0.0;
    return 1200.0 * std::log2(static_cast<double>(num) / static_cast<double>(den));
}

namespace {

/// Skip comment lines (starting with '!') and return non-comment lines
std::vector<std::string> extract_data_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(pos, end - pos);
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }
        if (!line.empty() && line[0] != '!') {
            lines.emplace_back(line);
        }
        pos = end + 1;
    }
    return lines;
}

/// Trim leading/trailing whitespace
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

/// Parse a single interval line
Sunny::Core::Result<ScalaInterval> parse_interval(std::string_view line) {
    line = trim(line);
    if (line.empty()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }

    // Check for ratio (contains '/')
    auto slash = line.find('/');
    if (slash != std::string_view::npos) {
        // Ratio: num/den
        auto num_sv = trim(line.substr(0, slash));
        auto den_sv = trim(line.substr(slash + 1));
        // Denominator ends at first space (inline comment)
        auto sp = den_sv.find(' ');
        if (sp != std::string_view::npos) den_sv = den_sv.substr(0, sp);

        int num = 0, den = 0;
        auto [p1, ec1] = std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), num);
        auto [p2, ec2] = std::from_chars(den_sv.data(), den_sv.data() + den_sv.size(), den);
        if (ec1 != std::errc{} || ec2 != std::errc{} || den == 0) {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
        }
        return ScalaInterval{ratio_to_cents(num, den), true, num, den};
    }

    // Check for integer ratio (a whole number with no decimal point)
    // In Scala format, a bare integer like "2" means ratio 2/1
    bool has_dot = (line.find('.') != std::string_view::npos);

    // Isolate the numeric portion (before any inline comment)
    auto sp = line.find(' ');
    std::string_view num_part = (sp != std::string_view::npos) ? line.substr(0, sp) : line;

    if (!has_dot) {
        // Integer ratio: n/1
        int num = 0;
        auto [p, ec] = std::from_chars(num_part.data(), num_part.data() + num_part.size(), num);
        if (ec != std::errc{} || num <= 0) {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
        }
        return ScalaInterval{ratio_to_cents(num, 1), true, num, 1};
    }

    // Cents value (has decimal point)
    double cents = 0.0;
    // std::from_chars for double may not be available on all compilers,
    // fall back to strtod
    char* end_ptr = nullptr;
    std::string num_str(num_part);
    cents = std::strtod(num_str.c_str(), &end_ptr);
    if (end_ptr == num_str.c_str()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }
    return ScalaInterval{cents, false, 0, 0};
}

}  // anonymous namespace

Sunny::Core::Result<ScalaTuning> parse_scala(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }

    auto lines = extract_data_lines(text);
    if (lines.size() < 2) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }

    ScalaTuning result;
    result.description = lines[0];

    // Second data line: note count
    int note_count = 0;
    auto count_sv = trim(lines[1]);
    auto [p, ec] = std::from_chars(count_sv.data(), count_sv.data() + count_sv.size(), note_count);
    if (ec != std::errc{} || note_count < 0) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }

    // Parse intervals
    if (static_cast<int>(lines.size()) - 2 < note_count) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidScalaFile);
    }

    result.intervals.reserve(static_cast<std::size_t>(note_count));
    for (int i = 0; i < note_count; ++i) {
        auto interval = parse_interval(lines[static_cast<std::size_t>(i) + 2]);
        if (!interval) {
            return std::unexpected(interval.error());
        }
        result.intervals.push_back(*interval);
    }

    return result;
}

std::string write_scala(const ScalaTuning& tuning) {
    std::ostringstream out;
    out << "! " << tuning.description << '\n';
    out << "!\n";
    out << tuning.description << '\n';
    out << tuning.intervals.size() << '\n';
    out << "!\n";

    for (const auto& iv : tuning.intervals) {
        if (iv.is_ratio) {
            out << iv.ratio_num << '/' << iv.ratio_den << '\n';
        } else {
            out << std::fixed << std::setprecision(6) << iv.cents << '\n';
        }
    }

    return out.str();
}

Sunny::Core::Result<std::array<double, 12>> scala_to_cent_table(const ScalaTuning& tuning) {
    if (tuning.intervals.size() != 12) {
        return std::unexpected(Sunny::Core::ErrorCode::FormatError);
    }

    std::array<double, 12> table{};
    // table[0] = unison deviation (always 0)
    table[0] = 0.0;
    for (int i = 0; i < 11; ++i) {
        double equal_cents = 100.0 * (i + 1);
        table[static_cast<std::size_t>(i) + 1] = tuning.intervals[static_cast<std::size_t>(i)].cents - equal_cents;
    }
    // index 0 is unison; intervals[11] should be ~1200 (octave)
    return table;
}

}  // namespace Sunny::Infrastructure::Format
