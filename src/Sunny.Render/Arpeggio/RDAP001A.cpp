/**
 * @file RDAP001A.cpp
 * @brief Arpeggiator implementation
 *
 * Component: RDAP001A
 */

#include "RDAP001A.h"

#include <algorithm>

namespace Sunny::Render {

void Arpeggiator::set_notes(const std::vector<Core::MidiNote>& notes) {
    input_notes_ = notes;
    pattern_dirty_ = true;
}

void Arpeggiator::set_notes(const Core::ChordVoicing& voicing) {
    input_notes_ = voicing.notes;
    pattern_dirty_ = true;
}

void Arpeggiator::clear() {
    input_notes_.clear();
    pattern_cache_.clear();
    pattern_dirty_ = true;
    current_step_ = 0;
}

std::vector<Core::MidiNote> Arpeggiator::generate_pattern() const {
    if (pattern_dirty_) {
        rebuild_pattern();
    }
    return pattern_cache_;
}

void Arpeggiator::reset() {
    current_step_ = 0;
    going_up_ = true;
}

Core::MidiNote Arpeggiator::next() {
    if (pattern_dirty_) {
        rebuild_pattern();
    }

    if (pattern_cache_.empty()) {
        return 60;  // Middle C default
    }

    Core::MidiNote note = pattern_cache_[current_step_];

    // Advance step
    current_step_++;
    if (current_step_ >= pattern_cache_.size()) {
        current_step_ = 0;
    }

    return note;
}

Core::MidiNote Arpeggiator::current() const {
    if (pattern_dirty_) {
        rebuild_pattern();
    }

    if (pattern_cache_.empty()) {
        return 60;
    }

    return pattern_cache_[current_step_];
}

std::size_t Arpeggiator::pattern_length() const {
    if (pattern_dirty_) {
        rebuild_pattern();
    }
    return pattern_cache_.size();
}

void Arpeggiator::rebuild_pattern() const {
    pattern_cache_.clear();
    pattern_dirty_ = false;

    if (input_notes_.empty()) {
        return;
    }

    // Sort input notes
    std::vector<Core::MidiNote> sorted = input_notes_;
    std::sort(sorted.begin(), sorted.end());

    // Expand with octaves
    std::vector<Core::MidiNote> expanded;
    for (int oct = 0; oct < octave_range_; ++oct) {
        for (auto note : sorted) {
            int transposed = note + oct * 12;
            if (transposed <= 127) {
                expanded.push_back(static_cast<Core::MidiNote>(transposed));
            }
        }
    }

    // Apply direction
    switch (direction_) {
        case ArpDirection::Up:
            pattern_cache_ = expanded;
            break;

        case ArpDirection::Down:
            pattern_cache_ = expanded;
            std::reverse(pattern_cache_.begin(), pattern_cache_.end());
            break;

        case ArpDirection::UpDown:
            pattern_cache_ = expanded;
            if (expanded.size() > 1) {
                for (auto it = expanded.rbegin() + 1; it != expanded.rend() - 1; ++it) {
                    pattern_cache_.push_back(*it);
                }
            }
            break;

        case ArpDirection::DownUp:
            pattern_cache_ = expanded;
            std::reverse(pattern_cache_.begin(), pattern_cache_.end());
            if (expanded.size() > 1) {
                for (auto it = expanded.begin() + 1; it != expanded.end() - 1; ++it) {
                    pattern_cache_.push_back(*it);
                }
            }
            break;

        case ArpDirection::Random:
            pattern_cache_ = expanded;
            std::shuffle(pattern_cache_.begin(), pattern_cache_.end(), rng_);
            break;

        case ArpDirection::Order:
            // Expand in original order
            pattern_cache_.clear();
            for (int oct = 0; oct < octave_range_; ++oct) {
                for (auto note : input_notes_) {
                    int transposed = note + oct * 12;
                    if (transposed <= 127) {
                        pattern_cache_.push_back(static_cast<Core::MidiNote>(transposed));
                    }
                }
            }
            break;
    }
}

std::vector<Core::NoteEvent> generate_arpeggio(
    const Core::ChordVoicing& voicing,
    ArpDirection direction,
    Core::Beat step_duration,
    double gate,
    int octaves
) {
    Arpeggiator arp;
    arp.set_direction(direction);
    arp.set_octave_range(octaves);
    arp.set_gate(gate);
    arp.set_notes(voicing);

    auto pattern = arp.generate_pattern();
    std::vector<Core::NoteEvent> events;
    events.reserve(pattern.size());

    // Calculate gate duration using rational scaling to avoid truncation
    // Multiply by gate as a fraction: (numerator * gate_scaled) / (denominator * scale)
    constexpr std::int64_t GATE_SCALE = 1000;
    std::int64_t gate_scaled = static_cast<std::int64_t>(gate * GATE_SCALE);
    if (gate_scaled < 1) gate_scaled = 1;  // Minimum gate

    Core::Beat gate_duration{
        step_duration.numerator * gate_scaled,
        step_duration.denominator * GATE_SCALE
    };

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        Core::NoteEvent event;
        event.pitch = pattern[i];
        event.start_time = Core::Beat{
            static_cast<std::int64_t>(i) * step_duration.numerator,
            step_duration.denominator
        };
        event.duration = gate_duration;
        event.velocity = 100;

        events.push_back(event);
    }

    return events;
}

}  // namespace Sunny::Render
