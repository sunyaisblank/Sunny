#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>

namespace sunny::infrastructure::managed_detail {

inline bool note_integer(const nlohmann::json& value, std::int64_t low, std::int64_t high) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() &&
         value.get<std::uint64_t>() > static_cast<std::uint64_t>(high)))
        return false;
    const auto number = value.get<std::int64_t>();
    return low <= number && number <= high;
}

inline bool note_field(std::string_view name, const nlohmann::json& value) {
    if (name == "pitch") return note_integer(value, 0, 127);
    if (name == "mute") return value.is_boolean();
    if (!value.is_number() || !std::isfinite(value.get<double>())) return false;
    const auto number = value.get<double>();
    if (name == "duration") return number > 0.0;
    if (name == "velocity" || name == "release_velocity") return 0.0 <= number && number <= 127.0;
    if (name == "probability") return 0.0 <= number && number <= 1.0;
    if (name == "velocity_deviation") return -127.0 <= number && number <= 127.0;
    return name == "start_time";
}

inline bool note_values(const nlohmann::json& note, bool actual = false) {
    if (!note.is_object() || note.size() != 8) return false;
    for (const auto* name : {"pitch",
                             "start_time",
                             "duration",
                             "velocity",
                             "mute",
                             "probability",
                             "velocity_deviation",
                             "release_velocity"}) {
        if (!note.contains(name) || !note_field(name, note.at(name))) return false;
        if (actual && name != std::string_view{"pitch"} && name != std::string_view{"mute"} &&
            !note.at(name).is_number_float())
            return false;
    }
    return true;
}

inline nlohmann::json semantic_note(nlohmann::json note) {
    note.erase("note_id");
    for (auto& [name, value] : note.items())
        if (name != "pitch" && name != "mute") value = value.get<double>();
    return note;
}

inline bool note_changes_valid(const nlohmann::json& changes) {
    if (!changes.is_array() || changes.empty() || changes.size() > 65536) return false;
    std::set<std::int32_t> ids;
    for (const auto& change : changes) {
        if (!change.is_object() || change.size() != 3 || !change.contains("note_id") ||
            !change.contains("expected") || !change.contains("updates") ||
            !note_integer(change.at("note_id"), INT32_MIN, INT32_MAX) ||
            !ids.insert(change.at("note_id").get<std::int32_t>()).second ||
            !note_values(change.at("expected")))
            return false;
        const auto& updates = change.at("updates");
        if (!updates.is_object() || updates.empty()) return false;
        for (const auto& [name, value] : updates.items())
            if (name == "probability" || name == "velocity_deviation" || !note_field(name, value) ||
                (name == "start_time" && value.get<double>() < 0.0))
                return false;
    }
    return true;
}

inline bool note_identity_valid(const nlohmann::json& identity) {
    if (!identity.is_object() || identity.size() != 2 ||
        !identity.contains("entire_clip_population_observed") ||
        !identity.at("entire_clip_population_observed").is_boolean() ||
        !identity.contains("notes") || !identity.at("notes").is_array())
        return false;
    std::set<std::int32_t> ids;
    for (const auto& note : identity.at("notes")) {
        if (!note.is_object() || note.size() != 9 || !note.contains("note_id") ||
            !note_integer(note.at("note_id"), INT32_MIN, INT32_MAX) ||
            !ids.insert(note.at("note_id").get<std::int32_t>()).second)
            return false;
        auto values = note;
        values.erase("note_id");
        if (!note_values(values, true)) return false;
        // Native adapters normalize to doubles; do not silently normalize a
        // malformed receipt with integer-valued native numeric fields.
        for (const auto& [name, value] : note.items())
            if (name != "pitch" && name != "mute" && name != "note_id" && !value.is_number_float())
                return false;
    }
    return true;
}

using NativeNoteMap = std::map<std::int32_t, nlohmann::json>;

inline NativeNoteMap note_map(const nlohmann::json& identity) {
    NativeNoteMap result;
    for (const auto& note : identity.at("notes"))
        result.emplace(note.at("note_id").get<std::int32_t>(), note);
    return result;
}

/// Closed full-population geometry admission. Live beats are absolute Clip
/// quarter notes. Half-open same-pitch collisions involving a changed geometry
/// are rejected at actual double width, with no epsilon or hidden note removal.
inline std::optional<NativeNoteMap>
proposed_notes(const nlohmann::json& identity, const nlohmann::json& changes, double clip_end) {
    if (!note_identity_valid(identity) || !note_changes_valid(changes) ||
        identity.at("entire_clip_population_observed") != true || !std::isfinite(clip_end) ||
        clip_end <= 0.0)
        return std::nullopt;
    auto result = note_map(identity);
    std::set<std::int32_t> geometry;
    for (const auto& change : changes) {
        const auto id = change.at("note_id").get<std::int32_t>();
        auto found = result.find(id);
        if (found == result.end() || managed_digest(semantic_note(found->second)) !=
                                         managed_digest(semantic_note(change.at("expected"))))
            return std::nullopt;
        const auto before = found->second;
        for (const auto& [name, value] : change.at("updates").items())
            found->second[name] =
                name == "pitch" || name == "mute" ? value : nlohmann::json(value.get<double>());
        const auto& note = found->second;
        const double start = note.at("start_time").get<double>();
        const double end = start + note.at("duration").get<double>();
        if (!(0.0 <= start && start < clip_end) || !std::isfinite(end)) return std::nullopt;
        for (const auto* name : {"pitch", "start_time", "duration"})
            if (note.at(name) != before.at(name)) geometry.insert(id);
    }
    if (geometry.empty()) return result;
    for (const auto& [id, note] : result) {
        (void)id;
        if (!std::isfinite(note.at("start_time").get<double>() + note.at("duration").get<double>()))
            return std::nullopt;
    }
    for (const auto& note : identity.at("notes"))
        if (!std::isfinite(note.at("start_time").get<double>() + note.at("duration").get<double>()))
            return std::nullopt;
    std::map<int, std::vector<const nlohmann::json*>> pitches;
    for (const auto& [id, note] : result) {
        (void)id;
        pitches[note.at("pitch").get<int>()].push_back(&note);
    }
    for (auto& [pitch, notes] : pitches) {
        (void)pitch;
        if (std::ranges::none_of(notes, [&geometry](const auto* note) {
                return geometry.contains(note->at("note_id").template get<std::int32_t>());
            }))
            continue;
        std::ranges::sort(notes, {}, [](const auto* note) {
            return note->at("start_time").template get<double>();
        });
        for (std::size_t i = 0; i < notes.size(); ++i)
            for (std::size_t j = i + 1; j < notes.size(); ++j) {
                if (notes[j]->at("start_time").get<double>() >=
                    notes[i]->at("start_time").get<double>() +
                        notes[i]->at("duration").get<double>())
                    break;
                if (geometry.contains(notes[i]->at("note_id").get<std::int32_t>()) ||
                    geometry.contains(notes[j]->at("note_id").get<std::int32_t>()))
                    return std::nullopt;
            }
    }
    // Batch collision atomicity is not established by the public/pinned APIs.
    // Check each destination against other retained baseline intervals too.
    // A sweep keeps this bounded by sorting rather than all-pairs scanning.
    const auto end = [](const nlohmann::json* note) {
        return note->at("start_time").get<double>() + note->at("duration").get<double>();
    };
    std::map<int, std::vector<const nlohmann::json*>> baseline, destinations;
    for (const auto& note : identity.at("notes"))
        baseline[note.at("pitch").get<int>()].push_back(&note);
    for (const auto id : geometry) {
        const auto& note = result.at(id);
        destinations[note.at("pitch").get<int>()].push_back(&note);
    }
    for (auto& [pitch, targets] : destinations) {
        auto& sources = baseline[pitch];
        std::ranges::sort(sources, {}, [](const auto* note) {
            return note->at("start_time").template get<double>();
        });
        std::ranges::sort(targets, {}, end);
        std::size_t cursor = 0;
        const nlohmann::json *furthest = nullptr, *second = nullptr;
        for (const auto* target : targets) {
            while (cursor < sources.size() &&
                   sources[cursor]->at("start_time").get<double>() < end(target)) {
                const auto* source = sources[cursor++];
                if (!furthest || end(source) > end(furthest)) {
                    second = furthest;
                    furthest = source;
                } else if (!second || end(source) > end(second))
                    second = source;
            }
            for (const auto* source : {furthest, second})
                if (source && source->at("note_id") != target->at("note_id") &&
                    end(source) > target->at("start_time").get<double>())
                    return std::nullopt;
        }
    }
    return result;
}

} // namespace sunny::infrastructure::managed_detail
