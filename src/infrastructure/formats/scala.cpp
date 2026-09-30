/**
 * @file scala.cpp
 * @brief Scala tuning file (.scl) reader/writer implementation
 *
 */

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <sunny/infrastructure/formats/scala.hpp>

namespace sunny::infrastructure::formats {

sunny::core::Result<double> ratio_to_cents(int num, int den) {
    if (num <= 0 || den <= 0) return std::unexpected(sunny::core::ErrorCode::InvalidJIRatio);
    const double cents = 1200.0 * std::log2(static_cast<double>(num) / static_cast<double>(den));
    if (!std::isfinite(cents)) return std::unexpected(sunny::core::ErrorCode::InvalidJIRatio);
    return cents;
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
        if (line.empty() || line[0] != '!') lines.emplace_back(line);
        pos = end + 1;
    }
    return lines;
}

/// Trim leading/trailing whitespace
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

/// Parse a single interval line
sunny::core::Result<ScalaInterval> parse_interval(std::string_view line) {
    line = trim(line);
    if (line.empty()) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    }

    const auto is_horizontal_space = [](char c) { return c == ' ' || c == '\t'; };

    // A period in the parsed numeric token selects cents. Text after a valid value is allowed by
    // the Scala format, but it must be separated from that value by horizontal whitespace.
    char* end_ptr = nullptr;
    std::string line_string(line);
    errno = 0;
    const double cents = std::strtod(line_string.c_str(), &end_ptr);
    const std::string_view parsed_prefix{line_string.data(),
                                         static_cast<std::size_t>(end_ptr - line_string.data())};
    if (end_ptr != line_string.data() && parsed_prefix.find('.') != std::string_view::npos) {
        const auto consumed = static_cast<std::size_t>(end_ptr - line_string.data());
        if ((consumed != line_string.size() && !is_horizontal_space(*end_ptr)) || errno == ERANGE ||
            !std::isfinite(cents))
            return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
        return ScalaInterval{
            cents, false, 0, 0, std::string{trim(std::string_view{line_string}.substr(consumed))}};
    }

    // Otherwise the value is a positive integer ratio, with an optional slash and denominator.
    const char* begin = line.data();
    const char* end = line.data() + line.size();
    int numerator = 0;
    const auto [after_numerator, numerator_error] = std::from_chars(begin, end, numerator);
    if (numerator_error != std::errc{} || numerator <= 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);

    const char* cursor = after_numerator;
    while (cursor != end && is_horizontal_space(*cursor))
        ++cursor;
    int denominator = 1;
    std::string_view trailing;
    if (cursor != end && *cursor == '/') {
        ++cursor;
        while (cursor != end && is_horizontal_space(*cursor))
            ++cursor;
        const auto [after_denominator, denominator_error] =
            std::from_chars(cursor, end, denominator);
        if (denominator_error != std::errc{} || denominator <= 0)
            return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
        cursor = after_denominator;
        if (cursor != end && !is_horizontal_space(*cursor))
            return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
        trailing = trim(std::string_view{cursor, static_cast<std::size_t>(end - cursor)});
    } else if (after_numerator != end && !is_horizontal_space(*after_numerator)) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    } else {
        trailing = trim(
            std::string_view{after_numerator, static_cast<std::size_t>(end - after_numerator)});
    }

    const auto cents_from_ratio = ratio_to_cents(numerator, denominator);
    if (!cents_from_ratio) return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    return ScalaInterval{*cents_from_ratio, true, numerator, denominator, std::string{trailing}};
}

} // anonymous namespace

sunny::core::Result<ScalaTuning> parse_scala(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    }

    auto lines = extract_data_lines(text);
    if (lines.size() < 2) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    }

    ScalaTuning result;
    result.description = lines[0];

    // Second data line: note count
    int note_count = 0;
    auto count_sv = trim(lines[1]);
    auto [p, ec] = std::from_chars(count_sv.data(), count_sv.data() + count_sv.size(), note_count);
    if (ec != std::errc{} || p != count_sv.data() + count_sv.size() || note_count < 0) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    }

    // Parse intervals
    if (static_cast<std::uint64_t>(note_count) + 2 != lines.size()) {
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
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

sunny::core::Result<std::string> write_scala(const ScalaTuning& tuning) {
    if ((!tuning.description.empty() && tuning.description.front() == '!') ||
        tuning.description.find_first_of("\r\n") != std::string::npos)
        return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    for (const auto& interval : tuning.intervals) {
        const auto ratio_cents = interval.is_ratio
                                     ? ratio_to_cents(interval.ratio_num, interval.ratio_den)
                                     : sunny::core::Result<double>{0.0};
        if (!std::isfinite(interval.cents) ||
            (interval.is_ratio &&
             (!ratio_cents || std::abs(interval.cents - *ratio_cents) > 1.0e-9)) ||
            (!interval.is_ratio && (interval.ratio_num != 0 || interval.ratio_den != 0)) ||
            interval.trailing_text.find_first_of("\r\n") != std::string::npos ||
            trim(interval.trailing_text) != interval.trailing_text)
            return std::unexpected(sunny::core::ErrorCode::InvalidScalaFile);
    }

    std::ostringstream out;
    out << "! " << tuning.description << '\n';
    out << "!\n";
    out << tuning.description << '\n';
    out << tuning.intervals.size() << '\n';
    out << "!\n";

    for (const auto& iv : tuning.intervals) {
        if (iv.is_ratio) {
            out << iv.ratio_num << '/' << iv.ratio_den;
        } else {
            // max_digits10 preserves the exact double through decimal parse;
            // showpoint keeps an integral cent value in Scala's cents branch
            // instead of changing it into an integer ratio.
            out << std::defaultfloat << std::showpoint
                << std::setprecision(std::numeric_limits<double>::max_digits10) << iv.cents;
        }
        if (!iv.trailing_text.empty()) out << ' ' << iv.trailing_text;
        out << '\n';
    }

    return out.str();
}

sunny::core::Result<sunny::core::TuningTable> scala_to_cent_table(const ScalaTuning& tuning) {
    if (tuning.intervals.size() != 12) {
        return std::unexpected(sunny::core::ErrorCode::FormatError);
    }
    if (std::any_of(tuning.intervals.begin(), tuning.intervals.end(), [](const auto& interval) {
            const auto ratio_cents = interval.is_ratio
                                         ? ratio_to_cents(interval.ratio_num, interval.ratio_den)
                                         : sunny::core::Result<double>{0.0};
            return !std::isfinite(interval.cents) ||
                   (interval.is_ratio &&
                    (!ratio_cents || std::abs(interval.cents - *ratio_cents) > 1.0e-9)) ||
                   (!interval.is_ratio && (interval.ratio_num != 0 || interval.ratio_den != 0));
        }))
        return std::unexpected(sunny::core::ErrorCode::FormatError);

    // Scala defines the final degree as the formal period and permits periods other than an
    // octave. TuningTable is specifically a deviation function on Z/12Z whose octave recurrence
    // is fixed at 1200 cents, so a non-octave Scala period has no lossless representation here.
    if (tuning.intervals.back().cents != 1200.0)
        return std::unexpected(sunny::core::ErrorCode::FormatError);

    sunny::core::TuningTable table{};
    // table[0] = unison deviation (always 0)
    table[0] = 0.0;
    for (int i = 0; i < 11; ++i) {
        double equal_cents = 100.0 * (i + 1);
        table[static_cast<std::size_t>(i) + 1] =
            tuning.intervals[static_cast<std::size_t>(i)].cents - equal_cents;
    }
    // index 0 is unison; intervals[11] is the validated octave period
    return table;
}

sunny::core::Result<sunny::core::ScoreTuning> scala_to_score_tuning(const ScalaTuning& tuning,
                                                                    std::uint8_t reference_note,
                                                                    double reference_frequency_hz) {
    if (tuning.intervals.empty() || reference_note >= sunny::core::SCORE_TUNING_NOTE_COUNT ||
        !std::isfinite(reference_frequency_hz) || reference_frequency_hz <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::FormatError);
    for (const auto& interval : tuning.intervals) {
        const auto ratio_cents = interval.is_ratio
                                     ? ratio_to_cents(interval.ratio_num, interval.ratio_den)
                                     : sunny::core::Result<double>{0.0};
        if (!std::isfinite(interval.cents) ||
            (interval.is_ratio &&
             (!ratio_cents || std::abs(interval.cents - *ratio_cents) > 1.0e-9)) ||
            (!interval.is_ratio && (interval.ratio_num != 0 || interval.ratio_den != 0)))
            return std::unexpected(sunny::core::ErrorCode::FormatError);
    }

    const double period = tuning.intervals.back().cents;
    if (period <= 0.0) return std::unexpected(sunny::core::ErrorCode::FormatError);
    const auto degrees_per_period = static_cast<std::int64_t>(tuning.intervals.size());

    sunny::core::ScoreTuning result;
    result.name = tuning.description;
    result.reference_midi_note = reference_note;
    result.reference_frequency_hz = reference_frequency_hz;
    for (std::size_t note = 0; note < sunny::core::SCORE_TUNING_NOTE_COUNT; ++note) {
        const auto offset = static_cast<std::int64_t>(note) - reference_note;
        std::int64_t periods = offset / degrees_per_period;
        std::int64_t degree = offset % degrees_per_period;
        if (degree < 0) {
            degree += degrees_per_period;
            --periods;
        }
        const double degree_cents =
            degree == 0 ? 0.0 : tuning.intervals[static_cast<std::size_t>(degree - 1)].cents;
        result.cents_from_reference[note] = static_cast<double>(periods) * period + degree_cents;
    }
    if (auto valid = sunny::core::validate_score_tuning(result); !valid)
        return std::unexpected(sunny::core::ErrorCode::FormatError);
    return result;
}

} // namespace sunny::infrastructure::formats
