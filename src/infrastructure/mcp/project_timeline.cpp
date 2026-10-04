/** Bar-anchored automation points and morph spans follow whole-measure edits. */
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/infrastructure/mcp/project_timeline.hpp>

namespace sunny::infrastructure {

using namespace sunny::core;
using json = nlohmann::json;

json relocate_project_controls(const McpSession& session,
                               const ProjectRecord& project,
                               const std::string& tool_name,
                               const json& arguments) {
    const bool insertion = tool_name == "score_insert_measures";
    if (!insertion && tool_name != "score_delete_measures") return json::object();
    const auto bar = detail::checked_integer<std::uint32_t>(
        arguments.at(insertion ? "after_bar" : "bar"), "splice bar");
    const auto count =
        detail::checked_integer<std::uint32_t>(arguments.at("count"), "splice count");
    std::uint64_t moved = 0, removed = 0, removed_lanes = 0, clipped = 0, removed_morphs = 0;
    const auto deleted = [&](ScoreTime time) {
        return !insertion && time.bar >= bar &&
               static_cast<std::uint64_t>(time.bar) < static_cast<std::uint64_t>(bar) + count;
    };
    const auto relocate = [&](ScoreTime& time) {
        if (insertion && time.bar > bar) {
            const auto shifted = static_cast<std::uint64_t>(time.bar) + count;
            if (shifted > std::numeric_limits<std::uint32_t>::max())
                throw std::overflow_error("Project control bar domain exhausted");
            time.bar = static_cast<std::uint32_t>(shifted);
            ++moved;
        } else if (!insertion && static_cast<std::uint64_t>(time.bar) >=
                                     static_cast<std::uint64_t>(bar) + count) {
            time.bar -= count;
            ++moved;
        }
    };
    const auto splice_lanes = [&](auto& lanes) {
        for (auto& lane : lanes) {
            std::erase_if(lane.breakpoints, [&](const auto& point) {
                if (!deleted(point.time)) return false;
                ++removed;
                return true;
            });
            for (auto& point : lane.breakpoints)
                relocate(point.time);
        }
        std::erase_if(lanes, [&](const auto& lane) {
            if (!lane.breakpoints.empty()) return false;
            ++removed_lanes;
            return true;
        });
    };
    for (const auto id : project.profile_ids) {
        auto& profile = *session.timbre->find(id);
        splice_lanes(profile.parameter_automation);
        std::erase_if(profile.preset_morphs, [&](auto& morph) {
            for (auto* endpoint : {&morph.start, &morph.end}) {
                if (deleted(*endpoint)) {
                    *endpoint = ScoreTime{bar, Beat::zero()};
                    ++clipped;
                } else
                    relocate(*endpoint);
            }
            if (morph.start < morph.end) return false;
            ++removed_morphs;
            return true;
        });
    }
    splice_lanes(session.mix->find(project.mix_graph_id)->automation);
    return {{"points_or_endpoints_relocated", moved},
            {"points_removed", removed},
            {"empty_lanes_removed", removed_lanes},
            {"morph_endpoints_clipped", clipped},
            {"collapsed_morphs_removed", removed_morphs},
            {"interpretation",
             "bar-anchored controls; existing interpolation follows relocated anchors"}};
}

} // namespace sunny::infrastructure
