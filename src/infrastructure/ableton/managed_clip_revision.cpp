#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_geometry.hpp>
#include <sunny/infrastructure/ableton/managed_clip_revision.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using KeyedNotes = std::map<std::string, json>;

std::optional<KeyedNotes> keyed_notes(const ManagedClipProjection& projection,
                                      const std::vector<std::string>& keys) {
    const json geometry{{"end_marker", projection.clip_end},
                        {"signature_numerator", projection.signature_numerator},
                        {"signature_denominator", projection.signature_denominator}};
    if (!managed_detail::clip_geometry_valid(geometry) || !projection.notes.is_array() ||
        projection.notes.size() > 65536 || keys.size() != projection.notes.size())
        return std::nullopt;
    KeyedNotes result;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const auto& note = projection.notes.at(index);
        if (!managed_detail::population_note_key(keys.at(index)) ||
            !managed_detail::note_values(note))
            return std::nullopt;
        const auto start = note.at("start_time").get<double>();
        const auto duration = note.at("duration").get<double>();
        const auto end = start + duration;
        if (start < 0.0 || start >= projection.clip_end || !std::isfinite(end) ||
            end > projection.clip_end ||
            !result.emplace(keys.at(index), managed_detail::semantic_note(note)).second)
            return std::nullopt;
    }
    return result;
}

std::map<std::string, std::set<std::string>> event_keys(const KeyedNotes& notes) {
    std::map<std::string, std::set<std::string>> result;
    for (const auto& [key, note] : notes) {
        static_cast<void>(note);
        result[key.substr(0, key.find("_n"))].insert(key);
    }
    return result;
}

bool supported_retained_notes(const KeyedNotes& previous, const KeyedNotes& desired) {
    const auto previous_events = event_keys(previous), desired_events = event_keys(desired);
    for (const auto& [event, keys] : previous_events) {
        const auto current = desired_events.find(event);
        if (current != desired_events.end() && current->second != keys) return false;
    }
    for (const auto& [key, note] : previous) {
        const auto current = desired.find(key);
        if (current == desired.end()) continue;
        for (const auto* field : {"probability", "velocity_deviation"})
            if (note.at(field) != current->second.at(field)) return false;
    }
    return true;
}
} // namespace

sunny::core::Result<std::vector<ManagedClipRevisionStage>>
plan_managed_clip_revision(const ManagedClipProjection& previous,
                           const std::vector<std::string>& previous_note_keys,
                           const ManagedClipProjection& desired,
                           const std::vector<std::string>& desired_note_keys) {
    const auto previous_notes = keyed_notes(previous, previous_note_keys);
    const auto desired_notes = keyed_notes(desired, desired_note_keys);
    if (!previous_notes || !desired_notes)
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    if (!supported_retained_notes(*previous_notes, *desired_notes))
        return std::unexpected(sunny::core::ErrorCode::TargetValueUnrepresentable);

    std::vector<ManagedClipRevisionStage> result;
    const bool notes_changed = *previous_notes != *desired_notes;
    if (notes_changed) {
        const auto admitted_end = std::max(previous.clip_end, desired.clip_end);
        if (admitted_end > previous.clip_end) {
            auto expanded = previous;
            expanded.clip_end = admitted_end;
            expanded.outside_clip_requests = desired.outside_clip_requests;
            result.push_back({ManagedClipRevisionPhase::ExpandGeometry,
                              std::move(expanded),
                              previous_note_keys});
        }
        auto population = desired;
        population.clip_end = admitted_end;
        population.signature_numerator = previous.signature_numerator;
        population.signature_denominator = previous.signature_denominator;
        result.push_back({ManagedClipRevisionPhase::UpdateNotePopulation,
                          std::move(population),
                          desired_note_keys});
        if (desired.clip_end == admitted_end &&
            desired.signature_numerator == previous.signature_numerator &&
            desired.signature_denominator == previous.signature_denominator)
            return result;
    } else if (desired.clip_end == previous.clip_end &&
               desired.signature_numerator == previous.signature_numerator &&
               desired.signature_denominator == previous.signature_denominator) {
        return result;
    }
    result.push_back({ManagedClipRevisionPhase::FinalizeGeometry, desired, desired_note_keys});
    return result;
}

} // namespace sunny::infrastructure
