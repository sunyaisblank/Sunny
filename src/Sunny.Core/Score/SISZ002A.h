/**
 * @file SISZ002A.h
 * @brief Shared IR serialisation primitives
 *
 * Component: SISZ002A
 * Domain: SI (Score IR) | Category: SZ (Serialisation)
 *
 * One JSON scheme for the temporal types every IR family serialises:
 *   Beat      -> {"num": int64, "den": int64}
 *   ScoreTime -> {"bar": uint32, "beat": {"num": ..., "den": ...}}
 *
 * Readers refuse malformed input (missing fields, non-positive
 * denominators) by throwing nlohmann::json exceptions; each family's
 * from_json entry point converts these to Result errors. Extracted from
 * the four per-family copies so the on-disk scheme and its guards
 * cannot drift again.
 */

#pragma once

#include "../Tensor/TNBT001A.h"
#include "SITP001A.h"

#include <nlohmann/json.hpp>

namespace Sunny::Core {

/// Serialise a Beat as {"num", "den"}
[[nodiscard]] nlohmann::json beat_to_json(const Beat& b);

/// Read a Beat; throws json::other_error if "den" <= 0, json::out_of_range
/// if a field is missing. The value is normalised to lowest terms.
[[nodiscard]] Beat beat_from_json(const nlohmann::json& j);

/// Serialise a ScoreTime as {"bar", "beat"}
[[nodiscard]] nlohmann::json score_time_to_json(const ScoreTime& st);

/// Read a ScoreTime; throws on missing fields or malformed Beat
[[nodiscard]] ScoreTime score_time_from_json(const nlohmann::json& j);

}  // namespace Sunny::Core
