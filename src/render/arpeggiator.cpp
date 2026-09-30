/**
 * @file arpeggiator.cpp
 * @brief Arpeggiator implementation
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <sunny/core/types/beat.hpp>
#include <sunny/render/arpeggiator.hpp>

namespace sunny::render {

namespace {

[[nodiscard]] bool valid_direction(ArpDirection direction) noexcept {
    switch (direction) {
    case ArpDirection::Up:
    case ArpDirection::Down:
    case ArpDirection::UpDown:
    case ArpDirection::DownUp:
    case ArpDirection::Random:
    case ArpDirection::Order:
        return true;
    }
    return false;
}

[[nodiscard]] sunny::core::Result<std::vector<sunny::core::MidiNote>>
expand_octaves(const std::vector<sunny::core::MidiNote>& notes, int octave_range) {
    std::vector<sunny::core::MidiNote> expanded;
    expanded.reserve(notes.size() * static_cast<std::size_t>(octave_range));
    for (int octave = 0; octave < octave_range; ++octave) {
        for (auto note : notes) {
            auto transposed =
                sunny::core::MidiNote::from_int(static_cast<int>(note.value()) + octave * 12);
            if (!transposed) return std::unexpected(transposed.error());
            expanded.push_back(*transposed);
        }
    }
    return expanded;
}

void deterministic_shuffle(std::vector<sunny::core::MidiNote>& notes, std::mt19937& rng) {
    for (std::size_t remaining = notes.size(); remaining > 1; --remaining) {
        const auto bound = static_cast<std::uint64_t>(remaining);
        const auto threshold = static_cast<std::uint64_t>(0ULL - bound) % bound;
        std::uint64_t sample = 0;
        do {
            sample = static_cast<std::uint64_t>(rng()) << 32;
            sample |= static_cast<std::uint64_t>(rng());
        } while (sample < threshold);
        const auto selected = static_cast<std::size_t>(sample % bound);
        std::swap(notes[remaining - 1], notes[selected]);
    }
}

} // namespace

void Arpeggiator::invalidate_pattern() {
    pattern_cache_.clear();
    pattern_dirty_ = true;
    current_step_ = 0;
}

sunny::core::VoidResult Arpeggiator::set_direction(ArpDirection direction) {
    if (!valid_direction(direction))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    direction_ = direction;
    invalidate_pattern();
    return {};
}

sunny::core::VoidResult Arpeggiator::set_octave_range(int octaves) {
    if (octaves < 1 || octaves > 11)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    octave_range_ = octaves;
    invalidate_pattern();
    return {};
}

sunny::core::VoidResult Arpeggiator::set_gate(double gate) {
    if (!std::isfinite(gate) || gate <= 0.0 || gate > 1.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    gate_ = gate;
    return {};
}

void Arpeggiator::set_seed(std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed_);
    invalidate_pattern();
}

void Arpeggiator::set_notes(const std::vector<sunny::core::MidiNote>& notes) {
    input_notes_ = notes;
    invalidate_pattern();
}

void Arpeggiator::set_notes(const sunny::core::ChordVoicing& voicing) {
    set_notes(voicing.notes);
}

void Arpeggiator::clear() {
    input_notes_.clear();
    pattern_cache_.clear();
    pattern_dirty_ = false;
    current_step_ = 0;
}

sunny::core::Result<std::vector<sunny::core::MidiNote>> Arpeggiator::generate_pattern() const {
    if (pattern_dirty_) {
        auto rebuilt = rebuild_pattern();
        if (!rebuilt) return std::unexpected(rebuilt.error());
    }
    if (pattern_cache_.empty()) return std::unexpected(sunny::core::ErrorCode::RenderEmptyPattern);
    return pattern_cache_;
}

void Arpeggiator::reset() {
    current_step_ = 0;
}

sunny::core::Result<sunny::core::MidiNote> Arpeggiator::next() {
    auto pattern = generate_pattern();
    if (!pattern) return std::unexpected(pattern.error());

    const auto note = (*pattern)[current_step_];
    current_step_ = (current_step_ + 1) % pattern->size();
    return note;
}

sunny::core::Result<sunny::core::MidiNote> Arpeggiator::current() const {
    auto pattern = generate_pattern();
    if (!pattern) return std::unexpected(pattern.error());
    return (*pattern)[current_step_];
}

sunny::core::Result<std::size_t> Arpeggiator::pattern_length() const {
    auto pattern = generate_pattern();
    if (!pattern) return std::unexpected(pattern.error());
    return pattern->size();
}

sunny::core::VoidResult Arpeggiator::rebuild_pattern() const {
    if (input_notes_.empty()) return std::unexpected(sunny::core::ErrorCode::RenderEmptyPattern);

    std::vector<sunny::core::MidiNote> source = input_notes_;
    if (direction_ != ArpDirection::Order) std::sort(source.begin(), source.end());

    auto expanded_result = expand_octaves(source, octave_range_);
    if (!expanded_result) return std::unexpected(expanded_result.error());
    auto expanded = std::move(*expanded_result);
    std::vector<sunny::core::MidiNote> rebuilt;

    switch (direction_) {
    case ArpDirection::Up:
    case ArpDirection::Order:
        rebuilt = expanded;
        break;

    case ArpDirection::Down:
        rebuilt = expanded;
        std::reverse(rebuilt.begin(), rebuilt.end());
        break;

    case ArpDirection::UpDown:
        rebuilt = expanded;
        if (expanded.size() > 1) {
            for (auto iterator = expanded.rbegin() + 1; iterator != expanded.rend() - 1; ++iterator)
                rebuilt.push_back(*iterator);
        }
        break;

    case ArpDirection::DownUp:
        rebuilt = expanded;
        std::reverse(rebuilt.begin(), rebuilt.end());
        if (expanded.size() > 1) {
            for (auto iterator = expanded.begin() + 1; iterator != expanded.end() - 1; ++iterator)
                rebuilt.push_back(*iterator);
        }
        break;

    case ArpDirection::Random:
        rebuilt = expanded;
        deterministic_shuffle(rebuilt, rng_);
        break;
    }

    pattern_cache_ = std::move(rebuilt);
    pattern_dirty_ = false;
    return {};
}

sunny::core::Result<std::vector<sunny::core::NoteEvent>>
generate_arpeggio(const sunny::core::ChordVoicing& voicing,
                  ArpDirection direction,
                  sunny::core::Beat step_duration,
                  double gate,
                  int octaves) {
    if (step_duration.numerator() <= 0)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);

    Arpeggiator arpeggiator;
    if (auto result = arpeggiator.set_direction(direction); !result)
        return std::unexpected(result.error());
    if (auto result = arpeggiator.set_octave_range(octaves); !result)
        return std::unexpected(result.error());
    if (auto result = arpeggiator.set_gate(gate); !result) return std::unexpected(result.error());
    arpeggiator.set_notes(voicing);

    auto pattern = arpeggiator.generate_pattern();
    if (!pattern) return std::unexpected(pattern.error());

    const auto gate_ratio = sunny::core::Beat::from_float(gate);
    if (!gate_ratio) return std::unexpected(gate_ratio.error());
    auto gate_duration = sunny::core::checked_mul(step_duration, *gate_ratio);
    if (!gate_duration) return std::unexpected(gate_duration.error());

    std::vector<sunny::core::NoteEvent> events;
    events.reserve(pattern->size());
    for (std::size_t index = 0; index < pattern->size(); ++index) {
        if (index > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        auto start = sunny::core::checked_mul(
            step_duration, sunny::core::Beat{static_cast<std::int64_t>(index), 1});
        if (!start) return std::unexpected(start.error());

        sunny::core::NoteEvent event;
        event.pitch = (*pattern)[index];
        event.start_time = *start;
        event.duration = *gate_duration;
        event.velocity = 100;
        events.push_back(event);
    }

    return events;
}

} // namespace sunny::render
