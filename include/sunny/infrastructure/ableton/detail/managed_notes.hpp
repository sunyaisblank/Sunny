#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
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

inline bool geometry_admissible(const nlohmann::json& identity,
                                const NativeNoteMap& result,
                                const std::set<std::int32_t>& geometry) {
    if (geometry.empty()) return true;
    for (const auto& [id, note] : result) {
        (void)id;
        if (!std::isfinite(note.at("start_time").get<double>() + note.at("duration").get<double>()))
            return false;
    }
    for (const auto& note : identity.at("notes"))
        if (!std::isfinite(note.at("start_time").get<double>() + note.at("duration").get<double>()))
            return false;
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
        double furthest_end = -std::numeric_limits<double>::infinity();
        double changed_end = furthest_end;
        for (const auto* note : notes) {
            const auto start = note->at("start_time").get<double>();
            const bool changed = geometry.contains(note->at("note_id").get<std::int32_t>());
            if ((changed && furthest_end > start) || changed_end > start) return false;
            const auto end = start + note->at("duration").get<double>();
            furthest_end = std::max(furthest_end, end);
            if (changed) changed_end = std::max(changed_end, end);
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
                    return false;
        }
    }
    return true;
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
    if (!geometry_admissible(identity, result, geometry)) return std::nullopt;
    return result;
}

inline bool population_note_key(std::string_view key) {
    if (key.empty() || key.front() != 'e') return false;
    const auto separator = key.find("_n");
    if (separator == std::string_view::npos || separator < 2) return false;
    const auto event = key.substr(1, separator - 1), ordinal = key.substr(separator + 2);
    if (event.front() == '0' || ordinal.empty() || (ordinal.size() > 1 && ordinal.front() == '0'))
        return false;
    std::uint64_t event_id = 0, index = 0;
    const auto parse = [](std::string_view text, std::uint64_t& value) {
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        return result.ec == std::errc{} && result.ptr == text.data() + text.size();
    };
    return parse(event, event_id) && event_id > 0 && parse(ordinal, index) && index <= 65535;
}

inline bool population_request_valid(const nlohmann::json& request) {
    for (const auto* key : {"changes", "deletions", "additions"})
        if (!request.contains(key) || !request.at(key).is_array() || request.at(key).size() > 65536)
            return false;
    const auto& changes = request.at("changes");
    if (changes.empty() && request.at("deletions").empty() && request.at("additions").empty())
        return false;
    if (!changes.empty() && !note_changes_valid(changes)) return false;
    std::set<std::int32_t> ids;
    for (const auto& change : changes)
        ids.insert(change.at("note_id").get<std::int32_t>());
    for (const auto& deletion : request.at("deletions")) {
        if (!deletion.is_object() || deletion.size() != 2 || !deletion.contains("note_id") ||
            !deletion.contains("expected") ||
            !note_integer(deletion.at("note_id"), INT32_MIN, INT32_MAX) ||
            !ids.insert(deletion.at("note_id").get<std::int32_t>()).second ||
            !note_values(deletion.at("expected")))
            return false;
    }
    std::set<std::string> keys;
    for (const auto& addition : request.at("additions")) {
        if (!addition.is_object() || addition.size() != 2 || !addition.contains("note_key") ||
            !addition.contains("note") || !addition.at("note_key").is_string() ||
            !population_note_key(addition.at("note_key").get_ref<const std::string&>()) ||
            !keys.insert(addition.at("note_key").get<std::string>()).second ||
            !note_values(addition.at("note")) ||
            addition.at("note").at("start_time").get<double>() < 0.0)
            return false;
    }
    return true;
}

/// Deletion removes only explicit retained IDs, then existing-ID updates use the
/// same conservative intermediate guard. Additions use private arithmetic IDs
/// solely for geometry preflight; no synthetic ID becomes native authority.
inline std::optional<NativeNoteMap> proposed_population(const nlohmann::json& identity,
                                                        const nlohmann::json& request,
                                                        double clip_end) {
    using json = nlohmann::json;
    if (!note_identity_valid(identity) || identity.at("entire_clip_population_observed") != true ||
        !population_request_valid(request) || !std::isfinite(clip_end) || clip_end <= 0.0)
        return std::nullopt;
    const auto before = note_map(identity);
    auto retained = before;
    for (const auto& deletion : request.at("deletions")) {
        const auto id = deletion.at("note_id").get<std::int32_t>();
        const auto found = retained.find(id);
        if (found == retained.end() || managed_digest(semantic_note(found->second)) !=
                                           managed_digest(semantic_note(deletion.at("expected"))))
            return std::nullopt;
        retained.erase(found);
    }
    if (retained.size() + request.at("additions").size() > 65536) return std::nullopt;
    json retained_identity{{"entire_clip_population_observed", true}, {"notes", json::array()}};
    for (const auto& [id, note] : retained) {
        (void)id;
        retained_identity["notes"].push_back(note);
    }
    std::optional<NativeNoteMap> proposed = retained;
    if (!request.at("changes").empty())
        proposed = proposed_notes(retained_identity, request.at("changes"), clip_end);
    if (!proposed) return std::nullopt;
    auto final = *proposed;
    json current{{"entire_clip_population_observed", true}, {"notes", json::array()}};
    for (const auto& [id, note] : *proposed) {
        (void)id;
        current["notes"].push_back(note);
    }
    std::set<std::int32_t> geometry;
    std::int64_t synthetic = INT32_MIN;
    for (const auto& addition : request.at("additions")) {
        while (before.contains(static_cast<std::int32_t>(synthetic)) ||
               final.contains(static_cast<std::int32_t>(synthetic)))
            ++synthetic;
        auto note = semantic_note(addition.at("note"));
        const auto start = note.at("start_time").get<double>();
        if (!(0.0 <= start && start < clip_end) ||
            !std::isfinite(start + note.at("duration").get<double>()))
            return std::nullopt;
        const auto id = static_cast<std::int32_t>(synthetic++);
        note["note_id"] = id;
        final.emplace(id, std::move(note));
        geometry.insert(id);
    }
    if (!geometry_admissible(current, final, geometry)) return std::nullopt;
    return proposed;
}

} // namespace sunny::infrastructure::managed_detail
