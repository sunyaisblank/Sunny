/**
 * @file serialization_primitives.cpp
 * @brief Shared IR serialisation primitives implementation
 *
 */

#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/serialization_primitives.hpp>

namespace sunny::core {

using json = nlohmann::json;

json beat_to_json(const Beat& b) {
    return json{{"num", b.numerator()}, {"den", b.denominator()}};
}

Beat beat_from_json(const json& j) {
    auto num = detail::checked_integer<std::int64_t>(j.at("num"), "Beat numerator");
    auto den = detail::checked_integer<std::int64_t>(j.at("den"), "Beat denominator");
    if (den <= 0) {
        throw json::other_error::create(601, "Beat denominator must be > 0", &j);
    }
    return Beat::normalise(num, den);
}

json score_time_to_json(const ScoreTime& st) {
    return json{{"bar", st.bar}, {"beat", beat_to_json(st.beat)}};
}

ScoreTime score_time_from_json(const json& j) {
    return ScoreTime{detail::checked_integer<std::uint32_t>(j.at("bar"), "ScoreTime bar"),
                     beat_from_json(j.at("beat"))};
}

} // namespace sunny::core
