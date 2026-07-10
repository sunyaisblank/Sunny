/**
 * @file SISZ002A.cpp
 * @brief Shared IR serialisation primitives implementation
 *
 * Component: SISZ002A
 */

#include "SISZ002A.h"

namespace Sunny::Core {

using json = nlohmann::json;

json beat_to_json(const Beat& b) {
    return json{{"num", b.numerator}, {"den", b.denominator}};
}

Beat beat_from_json(const json& j) {
    auto num = j.at("num").get<std::int64_t>();
    auto den = j.at("den").get<std::int64_t>();
    if (den <= 0) {
        throw json::other_error::create(601, "Beat denominator must be > 0", &j);
    }
    return Beat::normalise(num, den);
}

json score_time_to_json(const ScoreTime& st) {
    return json{{"bar", st.bar}, {"beat", beat_to_json(st.beat)}};
}

ScoreTime score_time_from_json(const json& j) {
    return ScoreTime{
        j.at("bar").get<std::uint32_t>(),
        beat_from_json(j.at("beat"))
    };
}

}  // namespace Sunny::Core
