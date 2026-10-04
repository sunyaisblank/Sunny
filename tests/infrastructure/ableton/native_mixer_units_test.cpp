#include "native_mixer_units_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/native_mixer_units.hpp>
using namespace sunny::infrastructure;
using nlohmann::json;

TEST_CASE("Native Mixer volume validates genuine context and literal nonlinear displayed dB",
          "[ableton][native-mixer-units]") {
    const auto fixtures = native_mixer_units_fixture();
    for (const auto* name : {"nonlinear", "nonstandard", "infinity"}) {
        const auto parsed =
            parse_native_mixer_display_candidate("volume", -6.0, 0.0, fixtures.at(name));
        REQUIRE(parsed);
        CHECK(parsed->display == "-6.00 dB");
        CHECK(parsed->display_value == -6.0);
        CHECK(parsed->formatter_calls == 21);
        CHECK(parsed->internal_value == (std::string_view{name} == "nonstandard" ? 2.0 : 0.5));
        CHECK_FALSE(parsed->host_qualified);
        CHECK(parsed->evidence == fixtures.at(name));
        CHECK(parsed->descriptor.value == (std::string_view{name} == "nonstandard" ? 1.0 : .375));
    }
    CHECK_FALSE(
        parse_native_mixer_display_candidate("panning", -6.0, 0.0, fixtures.at("nonlinear")));
    CHECK_FALSE(
        parse_native_mixer_display_candidate("volume", -5.0, 0.0, fixtures.at("nonlinear")));
}

TEST_CASE("Native Mixer source context cannot be retyped or rewritten as a Device proof",
          "[ableton][native-mixer-units][closure]") {
    const auto good = native_mixer_units_fixture().at("nonlinear");
    const std::pair<const char*, json> changes[]{
        {"/schema_version", 1.0},
        {"/unit", "Hertz"},
        {"/host_qualified", true},
        {"/parameter_kind", "panning"},
        {"/parameter_original_name", "Different"},
        {"/device_cohort_count", 17},
        {"/mixer_capture/panning_mode", 0.0},
        {"/mixer_capture/parameters/0/kind", "send"},
        {"/mixer_capture/parameters/3/send_index", 1},
        {"/descriptor/minimum", 0},
        {"/track_context/mute", 0},
        {"/formatter_calls", 19},
        {"/formatter_calls", 65},
        {"/samples/0/internal_value", 0.01},
        {"/samples/19/display", "-5.00 dB"},
        {"/absolute_display_error", 1.0},
        {"/display_increment", 0.1},
        {"/current_display/internal_value", 0.5},
        {"/current_display/display", "-6.00 dB"},
        {"/current_display/display_increment", 0.1},
        {"/current_display/negative_infinity", true},
    };
    for (const auto& [pointer, value] : changes) {
        CAPTURE(pointer);
        auto bad = good;
        bad[json::json_pointer{pointer}] = value;
        CHECK_FALSE(parse_native_mixer_display_candidate("volume", -6.0, 0.0, bad));
    }
    auto bad = good;
    bad["device_class_name"] = "MixerDevice";
    CHECK_FALSE(parse_native_mixer_display_candidate("volume", -6.0, 0.0, bad));
    bad = good;
    bad["mixer_capture"]["parameters"][0]["descriptor"]["value"] = 0.5;
    CHECK_FALSE(parse_native_mixer_display_candidate("volume", -6.0, 0.0, bad));
}
