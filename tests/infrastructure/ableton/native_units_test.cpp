#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/ableton/native_units.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
using sunny::core::ErrorCode;

namespace {
const LomPath device{{"song", "tracks", "2", "devices", "1"}};

json utility_candidate() {
    // Literal native evidence: the displays intentionally have no linear gain
    // curve. These expected readings are not produced by either unit parser.
    auto result = json::parse(R"JSON({
      "schema_version":1,"device_class_name":"StereoGain",
      "parameter_original_name":"Gain","parameter_index":2,"unit":"Decibels",
      "target":-6.0,"display_tolerance":0.0,"internal_value":0.5,
      "display":"-6.00 dB","display_value":-6.0,"display_increment":0.01,
      "absolute_display_error":0.0,"balance_full_scale":null,
      "descriptor":{"minimum":0.0,"maximum":1.0,"value":0.375,
        "default_value":0.625,"is_quantized":false,"is_enabled":true,
        "state":0,"automation_state":0},
      "modes":{
        "Channel Mode":{"minimum":0.0,"maximum":2.0,"value":1.0,
          "is_quantized":true,"is_enabled":true,"state":0,"automation_state":0,
          "value_items":["Left","Stereo","Right"],"label":"Stereo"},
        "Mono":{"minimum":0.0,"maximum":1.0,"value":1.0,
          "is_quantized":true,"is_enabled":true,"state":0,"automation_state":0,
          "value_items":["On","Off"],"label":"Off"},
        "Mute":{"minimum":0.0,"maximum":1.0,"value":0.0,
          "is_quantized":true,"is_enabled":true,"state":0,"automation_state":0,
          "value_items":["Off","On"],"label":"Off"}},
      "eq8_scale_display":null,
      "population":[{"name":"Device On","original_name":"Device On"},
        {"name":"Channel Mode","original_name":"Channel Mode"},
        {"name":"Output level","original_name":"Gain"},
        {"name":"Mono","original_name":"Mono"},
        {"name":"Mute","original_name":"Mute"}],
      "formatter_calls":20,"samples":[],
      "qualification":"ObservedNativeDisplayCandidate",
      "source_commit":"e83d5192f321b24eb9daab843ac49a2d95d862b1",
      "host_qualified":false,"native_knob_only":true,
      "coverage_limits":["Displayed coordinate only; effective DSP and persistence are unqualified."]
    })JSON");
    const std::array<const char*, 17> displays{"-inf dB",
                                               "-60 dB",
                                               "-42 dB",
                                               "-30 dB",
                                               "-24 dB",
                                               "-18 dB",
                                               "-12 dB",
                                               "-9 dB",
                                               "-6.00 dB",
                                               "-3 dB",
                                               "0 dB",
                                               "3 dB",
                                               "6 dB",
                                               "9 dB",
                                               "12 dB",
                                               "15 dB",
                                               "18 dB"};
    const std::array<double, 17> physical{
        0, -60, -42, -30, -24, -18, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18};
    for (std::size_t i = 0; i < displays.size(); ++i)
        result["samples"].push_back({{"internal_value", static_cast<double>(i) / 16.0},
                                     {"display", displays[i]},
                                     {"phase", "grid"},
                                     {"display_value", i == 0 ? json(nullptr) : json(physical[i])},
                                     {"negative_infinity", i == 0}});
    for (const auto i : {0, 16, 8}) {
        auto repeated = result["samples"][i];
        repeated["phase"] = "repeat";
        result["samples"].push_back(repeated);
    }
    return result;
}

json envelope(const std::string& id, json candidate) {
    return {{"schema_version", 1},
            {"capability_id", id},
            {"target", candidate.at("target")},
            {"tolerance", candidate.at("display_tolerance")},
            {"outcome", "resolved"},
            {"candidate", std::move(candidate)}};
}

LomRequest
request(const std::string& id = "utility.gain", double target = -6.0, double tolerance = 0.0) {
    auto value = make_native_display_resolution_request(device, id, target, tolerance);
    REQUIRE(value);
    return *value;
}

void replace_readings(json& candidate,
                      const std::array<const char*, 17>& displays,
                      const std::array<double, 17>& physical,
                      const std::array<double, 17>& internal,
                      std::size_t selected) {
    candidate["samples"] = json::array();
    for (std::size_t i = 0; i < displays.size(); ++i)
        candidate["samples"].push_back({{"internal_value", internal[i]},
                                        {"display", displays[i]},
                                        {"phase", "grid"},
                                        {"display_value", physical[i]},
                                        {"negative_infinity", false}});
    for (const auto i : {std::size_t{0}, std::size_t{16}, selected}) {
        auto repeated = candidate["samples"][i];
        repeated["phase"] = "repeat";
        candidate["samples"].push_back(repeated);
    }
    candidate["internal_value"] = internal[selected];
    candidate["display"] = displays[selected];
    candidate["display_value"] = physical[selected];
    candidate["target"] = physical[selected];
    candidate["descriptor"]["minimum"] = internal.front();
    candidate["descriptor"]["maximum"] = internal.back();
    candidate["descriptor"]["value"] = internal[4];
    candidate["descriptor"]["default_value"] = internal[8];
}

json eq_candidate(bool q = false) {
    auto candidate = utility_candidate();
    candidate["device_class_name"] = "Eq8";
    candidate["parameter_original_name"] = q ? "7 Resonance A" : "7 Frequency A";
    candidate["unit"] = q ? "QualityFactor" : "Hertz";
    candidate["display_increment"] = q ? 0.01 : 10.0;
    candidate["formatter_calls"] = 21;
    candidate["population"] = json::parse(R"JSON([
      {"name":"Device On","original_name":"Device On"},
      {"name":"Scale","original_name":"Scale"},
      {"name":"Band 7 control","original_name":"7 Frequency A"},
      {"name":"Adaptive Q","original_name":"Adaptive Q"},
      {"name":"7 Filter On A","original_name":"7 Filter On A"}])JSON");
    candidate["population"][2]["original_name"] = candidate["parameter_original_name"];
    candidate["modes"] = json::parse(R"JSON({
      "global_mode":0,
      "Scale":{"minimum":-1.0,"maximum":1.0,"value":0.25,"default_value":0.0,
        "is_quantized":false,"is_enabled":true,"state":0,"automation_state":0},
      "Adaptive Q":{"minimum":0.0,"maximum":1.0,"value":1.0,
        "is_quantized":true,"is_enabled":true,"state":0,"automation_state":0,
        "value_items":["Off","On"],"label":"On"},
      "7 Filter On A":{"minimum":0.0,"maximum":1.0,"value":0.0,
        "is_quantized":true,"is_enabled":true,"state":0,"automation_state":0,
        "value_items":["On","Off"],"label":"On"}})JSON");
    candidate["eq8_scale_display"] = {
        {"display", "50%"}, {"display_value", 50.0}, {"display_increment", 1.0}};
    const std::array<double, 17> internal{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    if (q) {
        const std::array<const char*, 17> displays{"0.10",
                                                   "0.12",
                                                   "0.15",
                                                   "0.20",
                                                   "0.25",
                                                   "0.32",
                                                   "0.40",
                                                   "0.50",
                                                   "1.00",
                                                   "1.25",
                                                   "1.50",
                                                   "2.00",
                                                   "2.50",
                                                   "3.00",
                                                   "4.00",
                                                   "6.00",
                                                   "10.00"};
        const std::array<double, 17> physical{
            .1, .12, .15, .2, .25, .32, .4, .5, 1, 1.25, 1.5, 2, 2.5, 3, 4, 6, 10};
        replace_readings(candidate, displays, physical, internal, 8);
    } else {
        const std::array<const char*, 17> displays{"20 Hz",
                                                   "25 Hz",
                                                   "31.5 Hz",
                                                   "40 Hz",
                                                   "50 Hz",
                                                   "63 Hz",
                                                   "80 Hz",
                                                   "100 Hz",
                                                   "125 Hz",
                                                   "160 Hz",
                                                   "200 Hz",
                                                   "250 Hz",
                                                   "315 Hz",
                                                   "500 Hz",
                                                   "1.00 kHz",
                                                   "2 kHz",
                                                   "20 kHz"};
        const std::array<double, 17> physical{
            20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 500, 1000, 2000, 20000};
        replace_readings(candidate, displays, physical, internal, 14);
    }
    return candidate;
}

json balance_candidate() {
    auto candidate = utility_candidate();
    candidate["parameter_original_name"] = "Balance";
    candidate["population"][2]["original_name"] = "Balance";
    candidate["unit"] = "StereoBalance";
    candidate["balance_full_scale"] = 100.0;
    candidate["display_increment"] = 0.01;
    const std::array<const char*, 17> displays{"100 L",
                                               "87.5 L",
                                               "75 L",
                                               "62.5 L",
                                               "50 L",
                                               "37.5 L",
                                               "25 L",
                                               "12.5 L",
                                               "C",
                                               "12.5 R",
                                               "25 R",
                                               "37.5 R",
                                               "50 R",
                                               "62.5 R",
                                               "75 R",
                                               "87.5 R",
                                               "100 R"};
    const std::array<double, 17> physical{-1,
                                          -.875,
                                          -.75,
                                          -.625,
                                          -.5,
                                          -.375,
                                          -.25,
                                          -.125,
                                          0,
                                          .125,
                                          .25,
                                          .375,
                                          .5,
                                          .625,
                                          .75,
                                          .875,
                                          1};
    const std::array<double, 17> internal{
        -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    replace_readings(candidate, displays, physical, internal, 6);
    return candidate;
}

class ObservationTransport final : public LomTransport {
  public:
    LomResponse response{true, envelope("utility.gain", utility_candidate()), std::nullopt};
    bool recording = false;
    std::vector<LomRequest> requests;
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        return response;
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        FAIL("A native display query cannot send notes");
        return {};
    }
    bool is_connected() const override { return true; }
    bool records_without_execution() const override { return recording; }
};
} // namespace

TEST_CASE("Native unit requests admit the finite continuous catalogue and exact device paths",
          "[ableton][native-units]") {
    std::size_t admitted = 0, quantized = 0;
    for (const auto& entry : sunny::core::live_native_parameter_registry()) {
        const auto made = make_native_display_resolution_request(device, entry.id, 1.0, 0.0);
        if (entry.kind == sunny::core::LiveNativeParameterKind::Continuous) {
            REQUIRE(made);
            REQUIRE(LomProtocol::validate_request(*made));
            CHECK(json::parse(LomProtocol::serialize_request(*made))["args"][0] ==
                  json{{"capability_id", entry.id}, {"target", 1.0}, {"tolerance", 0.0}});
            CHECK(made->type == LomRequestType::CallMethod);
            CHECK(made->property_or_method == "sunny_resolve_native_display_value");
            CHECK(std::get<json>(made->args.front()) ==
                  json{{"capability_id", entry.id}, {"target", 1.0}, {"tolerance", 0.0}});
            ++admitted;
        } else {
            CHECK_FALSE(made);
            ++quantized;
        }
    }
    CHECK(admitted == 28);
    CHECK(quantized == 16);
    for (const auto* path : {"song/return_tracks/0/devices/0", "song/master_track/devices/0"})
        CHECK(make_native_display_resolution_request(LomPath::parse(path), "utility.gain", -6, 0));
    for (const auto* path : {"song/tracks/0",
                             "song/tracks/00/devices/0",
                             "song/tracks/0/devices/x",
                             "song/tracks/0/devices/0/parameters/0",
                             "song/tracks/0/devices/2147483648",
                             "song/tracks/0/devices/0/chains/0/devices/0",
                             "song/master_track/devices/0/"})
        CHECK_FALSE(
            make_native_display_resolution_request(LomPath::parse(path), "utility.gain", -6, 0));
    CHECK_FALSE(make_native_display_resolution_request(device, "utility.not_a_control", 0, 0));
    CHECK_FALSE(make_native_display_resolution_request(device, "utility.gain", 0, -1));
    CHECK_FALSE(make_native_display_resolution_request(
        device, "utility.gain", std::numeric_limits<double>::infinity(), 0));
}

TEST_CASE("Native display protocol rejects descriptor and policy injection",
          "[ableton][native-units]") {
    const auto original = request();
    for (const auto& bad :
         {json{{"capability_id", "utility.gain"},
               {"target", -6.0},
               {"tolerance", 0.0},
               {"modes", json::object()}},
          json{{"capability_id", "eq8.band.1.type"}, {"target", 1.0}, {"tolerance", 0.0}},
          json{{"capability_id", "utility.gain"}, {"target", true}, {"tolerance", 0.0}},
          json{{"capability_id", "utility.gain"}, {"target", -6.0}, {"tolerance", -0.01}}}) {
        auto malformed = original;
        malformed.args = {bad};
        CHECK_FALSE(LomProtocol::validate_request(malformed));
    }
    auto malformed = original;
    malformed.args.push_back(json::object());
    CHECK_FALSE(LomProtocol::validate_request(malformed));
    malformed = original;
    malformed.path = LomPath::parse("song/tracks/2");
    CHECK_FALSE(LomProtocol::validate_request(malformed));
}

TEST_CASE("Native gain observation retains nonlinear infinity and descriptor provenance",
          "[ableton][native-units]") {
    const auto wire = envelope("utility.gain", utility_candidate());
    const auto parsed = parse_native_display_resolution(request(), wire);
    REQUIRE(parsed);
    REQUIRE(parsed->candidate);
    const auto& candidate = *parsed->candidate;
    CHECK(parsed->status == NativeDisplayResolutionStatus::Candidate);
    CHECK(candidate.device_path == "song/tracks/2/devices/1");
    CHECK(candidate.internal_value == 0.5);
    CHECK(candidate.display_value == -6.0);
    CHECK(candidate.display_increment == 0.01);
    CHECK(candidate.descriptor.value == 0.375); // Query did not write its candidate.
    CHECK(candidate.parameter_index == 2);
    CHECK(candidate.samples.front().negative_infinity);
    CHECK_FALSE(candidate.samples.front().display_value);
    CHECK(candidate.formatter_calls == 20);
    CHECK_FALSE(candidate.host_qualified);
    CHECK(candidate.native_knob_only);
    CHECK(candidate.evidence == wire.at("candidate"));
    CHECK(parsed->evidence == wire);
}

TEST_CASE("Native EQ frequency and Q retain nonneutral Scale and Adaptive Q as knob evidence",
          "[ableton][native-units]") {
    for (const bool q : {false, true}) {
        const auto id = q ? "eq8.band.7.q" : "eq8.band.7.frequency";
        const auto target = q ? 1.0 : 1000.0;
        const auto parsed =
            parse_native_display_resolution(request(id, target), envelope(id, eq_candidate(q)));
        REQUIRE(parsed);
        REQUIRE(parsed->candidate);
        CHECK(parsed->candidate->display_value == target);
        CHECK(parsed->candidate->display_increment == (q ? 0.01 : 10.0));
        CHECK(parsed->candidate->internal_value == (q ? 8.0 : 14.0));
        CHECK(parsed->candidate->formatter_calls == 21);
        CHECK(parsed->candidate->modes.at("Adaptive Q").at("label") == "On");
        CHECK(parsed->candidate->eq8_scale_display.at("display_value") == 50.0);
        CHECK(parsed->candidate->native_knob_only);
    }
}

TEST_CASE("Native balance normalization uses observed endpoint magnitude and nonstandard bounds",
          "[ableton][native-units]") {
    auto candidate = balance_candidate();
    auto parsed = parse_native_display_resolution(request("utility.balance", -.25),
                                                  envelope("utility.balance", candidate));
    REQUIRE(parsed);
    REQUIRE(parsed->candidate);
    CHECK(parsed->candidate->internal_value == -2.0);
    CHECK(parsed->candidate->balance_full_scale == 100.0);
    CHECK(parsed->candidate->display_value == -.25);
    CHECK(parsed->candidate->display_increment == .01);
    candidate["target"] = 0.0;
    candidate["internal_value"] = 0.0;
    candidate["display"] = "C";
    candidate["display_value"] = 0.0;
    candidate["display_increment"] = nullptr;
    candidate["samples"].back() = candidate["samples"][8];
    candidate["samples"].back()["phase"] = "repeat";
    parsed = parse_native_display_resolution(request("utility.balance", 0),
                                             envelope("utility.balance", candidate));
    REQUIRE(parsed);
    CHECK_FALSE(parsed->candidate->display_increment);
}

TEST_CASE("Native width admits display plateaus and refuses an active Mid Side substitute",
          "[ableton][native-units]") {
    auto candidate = utility_candidate();
    candidate["parameter_original_name"] = "Stereo Width";
    candidate["population"][2]["original_name"] = "Stereo Width";
    candidate["unit"] = "Percent";
    candidate["display_increment"] = 1.0;
    const std::array<const char*, 17> displays{"0%",
                                               "0%",
                                               "25%",
                                               "25%",
                                               "50%",
                                               "50%",
                                               "75%",
                                               "75%",
                                               "100%",
                                               "100%",
                                               "125%",
                                               "125%",
                                               "150%",
                                               "150%",
                                               "175%",
                                               "175%",
                                               "200%"};
    const std::array<double, 17> physical{
        0, 0, 25, 25, 50, 50, 75, 75, 100, 100, 125, 125, 150, 150, 175, 175, 200};
    const std::array<double, 17> internal{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    replace_readings(candidate, displays, physical, internal, 8);
    candidate["population"].push_back(
        {{"name", "Mid/Side Balance"}, {"original_name", "Mid/Side Balance"}});
    candidate["modes"]["Mid/Side Balance"] = {{"is_enabled", false}, {"state", 1}};
    const auto parsed = parse_native_display_resolution(request("utility.width", 100),
                                                        envelope("utility.width", candidate));
    REQUIRE(parsed);
    CHECK(parsed->candidate->display_value == 100.0);
    CHECK(parsed->candidate->internal_value == 8.0);
    CHECK(parsed->candidate->samples[0].display_value ==
          parsed->candidate->samples[1].display_value);
    candidate["modes"]["Mid/Side Balance"] = {{"is_enabled", true}, {"state", 0}};
    CHECK_FALSE(parse_native_display_resolution(request("utility.width", 100),
                                                envelope("utility.width", candidate)));
}

TEST_CASE("Native EQ band and output gain use finite dB evidence without gain infinity",
          "[ableton][native-units]") {
    auto candidate = eq_candidate();
    const auto gain = utility_candidate();
    for (const auto* name : {"target",
                             "internal_value",
                             "display",
                             "display_value",
                             "display_increment",
                             "descriptor",
                             "samples"})
        candidate[name] = gain.at(name);
    candidate["unit"] = "Decibels";
    candidate["parameter_original_name"] = "7 Gain A";
    candidate["population"][2]["original_name"] = "7 Gain A";
    CHECK_FALSE(parse_native_display_resolution(request("eq8.band.7.gain", -6),
                                                envelope("eq8.band.7.gain", candidate)));
    for (const auto i : {0, 17}) {
        candidate["samples"][i]["display"] = "-72 dB";
        candidate["samples"][i]["display_value"] = -72.0;
        candidate["samples"][i]["negative_infinity"] = false;
    }
    auto parsed = parse_native_display_resolution(request("eq8.band.7.gain", -6),
                                                  envelope("eq8.band.7.gain", candidate));
    REQUIRE(parsed);
    CHECK(parsed->candidate->display_value == -6.0);
    candidate["parameter_original_name"] = "Output Gain";
    candidate["population"][2]["original_name"] = "Output Gain";
    candidate["population"].erase(candidate["population"].begin() + 4);
    candidate["modes"].erase("7 Filter On A");
    parsed = parse_native_display_resolution(request("eq8.output_gain", -6),
                                             envelope("eq8.output_gain", candidate));
    REQUIRE(parsed);
    CHECK(parsed->candidate->parameter_original_name == "Output Gain");
}

TEST_CASE("Native unit domains retain doubles beyond float bounds", "[ableton][native-units]") {
    auto candidate = utility_candidate();
    const std::array<double, 17> internal{-8e100,
                                          -7e100,
                                          -6e100,
                                          -5e100,
                                          -4e100,
                                          -3e100,
                                          -2e100,
                                          -1e100,
                                          0,
                                          1e100,
                                          2e100,
                                          3e100,
                                          4e100,
                                          5e100,
                                          6e100,
                                          7e100,
                                          8e100};
    for (std::size_t i = 0; i < internal.size(); ++i)
        candidate["samples"][i]["internal_value"] = internal[i];
    candidate["samples"][17]["internal_value"] = -8e100;
    candidate["samples"][18]["internal_value"] = 8e100;
    candidate["samples"][19]["internal_value"] = 0.0;
    candidate["internal_value"] = 0.0;
    candidate["descriptor"]["minimum"] = -8e100;
    candidate["descriptor"]["maximum"] = 8e100;
    candidate["descriptor"]["value"] = 0.0;
    candidate["descriptor"]["default_value"] = 0.0;
    const auto parsed =
        parse_native_display_resolution(request(), envelope("utility.gain", candidate));
    REQUIRE(parsed);
    CHECK(parsed->candidate->descriptor.minimum == -8e100);
    CHECK(parsed->candidate->descriptor.maximum == 8e100);
}

TEST_CASE("Native unit parser rejects malformed closed requests before reading candidate data",
          "[ableton][native-units]") {
    const auto wire = envelope("utility.gain", utility_candidate());
    auto changed = request();
    changed.type = LomRequestType::SetProperty;
    CHECK_FALSE(parse_native_display_resolution(changed, wire));
    changed = request();
    std::get<json>(changed.args[0])["modes"] = json::object();
    CHECK_FALSE(parse_native_display_resolution(changed, wire));
    changed = request();
    std::get<json>(changed.args[0])["target"] = true;
    CHECK_FALSE(parse_native_display_resolution(changed, wire));
    changed = request();
    changed.args.push_back(1);
    CHECK_FALSE(parse_native_display_resolution(changed, wire));
}

TEST_CASE(
    "Native unit parser declines corrupted identity request domain and qualification evidence",
    "[ableton][native-units]") {
    const auto original = envelope("utility.gain", utility_candidate());
    const std::array<std::pair<const char*, json>, 23> corruptions{
        {{"/capability_id", "utility.width"},
         {"/target", -5.0},
         {"/tolerance", .1},
         {"/schema_version", 2},
         {"/candidate/schema_version", 0},
         {"/candidate/device_class_name", "Eq8"},
         {"/candidate/parameter_original_name", "Balance"},
         {"/candidate/parameter_index", 1},
         {"/candidate/unit", "Hertz"},
         {"/candidate/target", -5.0},
         {"/candidate/display_tolerance", .1},
         {"/candidate/internal_value", 1.01},
         {"/candidate/host_qualified", true},
         {"/candidate/native_knob_only", false},
         {"/candidate/source_commit", "new_source"},
         {"/candidate/qualification", "HostQualified"},
         {"/candidate/formatter_calls", 65},
         {"/candidate/descriptor/is_enabled", false},
         {"/candidate/descriptor/state", 1},
         {"/candidate/descriptor/automation_state", 2},
         {"/candidate/descriptor/default_value", 2.0},
         {"/candidate/descriptor/is_quantized", true},
         {"/candidate/balance_full_scale", 50.0}}};
    for (const auto& [path, value] : corruptions) {
        INFO(path);
        auto wire = original;
        wire[json::json_pointer(path)] = value;
        const auto parsed = parse_native_display_resolution(request(), wire);
        CHECK_FALSE(parsed);
        if (!parsed) CHECK(parsed.error() == ErrorCode::ProtocolError);
    }
    auto wire = original;
    wire["caller_modes"] = json::object();
    CHECK_FALSE(parse_native_display_resolution(request(), wire));
    wire = original;
    wire["candidate"]["descriptor"]["minimum"] = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(parse_native_display_resolution(request(), wire));
}

TEST_CASE("Native unit parser rejects changed populations ambiguous aliases and mode gates",
          "[ableton][native-units]") {
    auto candidate = utility_candidate();
    candidate["population"].push_back({{"name", "Gain"}, {"original_name", "Other"}});
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = utility_candidate();
    candidate["population"].erase(candidate["population"].begin() + 3);
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = utility_candidate();
    candidate["modes"]["Channel Mode"]["value"] = 0.0;
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = utility_candidate();
    candidate["modes"]["Mono"]["value_items"] = json::array({"Off", "On"});
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = eq_candidate();
    candidate["modes"]["global_mode"] = 1;
    CHECK_FALSE(parse_native_display_resolution(request("eq8.band.7.frequency", 1000),
                                                envelope("eq8.band.7.frequency", candidate)));
    candidate = eq_candidate();
    candidate["modes"].erase("Adaptive Q");
    CHECK_FALSE(parse_native_display_resolution(request("eq8.band.7.frequency", 1000),
                                                envelope("eq8.band.7.frequency", candidate)));
}

TEST_CASE("Native unit parser rejects unsupported unit text locale and invented decimal precision",
          "[ableton][native-units]") {
    for (const auto* display :
         {"-6,00 dB", "-6.00 Hz", "-6e0 dB", "−6.00 dB", "-6.00\u00a0dB", "-6. dB"}) {
        auto candidate = utility_candidate();
        candidate["display"] = display;
        CHECK_FALSE(
            parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    }
    auto candidate = utility_candidate();
    candidate["display_increment"] = .001;
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = eq_candidate();
    candidate["display_value"] = 1.0;
    CHECK_FALSE(parse_native_display_resolution(request("eq8.band.7.frequency", 1000),
                                                envelope("eq8.band.7.frequency", candidate)));
    candidate = balance_candidate();
    candidate["balance_full_scale"] = 50.0;
    CHECK_FALSE(parse_native_display_resolution(request("utility.balance", -.25),
                                                envelope("utility.balance", candidate)));
}

TEST_CASE("Native unit parser enforces collected monotonicity and three repeated anchors",
          "[ableton][native-units]") {
    auto candidate = utility_candidate();
    std::swap(candidate["samples"][3]["display"], candidate["samples"][4]["display"]);
    std::swap(candidate["samples"][3]["display_value"], candidate["samples"][4]["display_value"]);
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    for (const auto i : {17, 18, 19}) {
        candidate = utility_candidate();
        candidate["samples"][i]["display"] = "changed";
        CHECK_FALSE(
            parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    }
    candidate = utility_candidate();
    candidate["samples"].erase(candidate["samples"].begin() + 17);
    candidate["formatter_calls"] = 19;
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = utility_candidate();
    candidate["samples"][3]["phase"] = "search";
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
}

TEST_CASE("Native unit parser retains explicit tolerance and rejects fabricated zero error",
          "[ableton][native-units]") {
    auto candidate = utility_candidate();
    candidate["target"] = -5.5;
    candidate["display_tolerance"] = .5;
    candidate["absolute_display_error"] = .5;
    REQUIRE(parse_native_display_resolution(request("utility.gain", -5.5, .5),
                                            envelope("utility.gain", candidate)));
    candidate["absolute_display_error"] = 0.0;
    CHECK_FALSE(parse_native_display_resolution(request("utility.gain", -5.5, .5),
                                                envelope("utility.gain", candidate)));
    candidate["display_tolerance"] = .49;
    candidate["absolute_display_error"] = .5;
    CHECK_FALSE(parse_native_display_resolution(request("utility.gain", -5.5, .49),
                                                envelope("utility.gain", candidate)));
}

TEST_CASE("Native unit formatter accounting includes EQ Scale and the finite budget",
          "[ableton][native-units]") {
    auto candidate = utility_candidate();
    for (int i = 0; i < 44; ++i) {
        auto sample = candidate["samples"][8];
        sample["phase"] = "search";
        candidate["samples"].insert(candidate["samples"].end() - 3, sample);
    }
    candidate["formatter_calls"] = 64;
    REQUIRE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate["formatter_calls"] = 63;
    CHECK_FALSE(parse_native_display_resolution(request(), envelope("utility.gain", candidate)));
    candidate = eq_candidate();
    candidate["formatter_calls"] = 20;
    CHECK_FALSE(parse_native_display_resolution(request("eq8.band.7.frequency", 1000),
                                                envelope("eq8.band.7.frequency", candidate)));
}

TEST_CASE("Native unit decline evidence remains a decline with its original reason and budget",
          "[ableton][native-units]") {
    auto wire = json{{"schema_version", 1},
                     {"capability_id", "utility.gain"},
                     {"target", -6.0},
                     {"tolerance", 0.0},
                     {"outcome", "declined"},
                     {"reason", "ToleranceNotMet"},
                     {"diagnostic", "No observed candidate met the requested tolerance"},
                     {"formatter_calls", 61}};
    const auto parsed = parse_native_display_resolution(request(), wire);
    REQUIRE(parsed);
    CHECK(parsed->status == NativeDisplayResolutionStatus::Declined);
    CHECK_FALSE(parsed->candidate);
    CHECK(parsed->reason == "ToleranceNotMet");
    CHECK(parsed->formatter_calls == 61);
    CHECK(parsed->evidence == wire);
    wire["formatter_calls"] = 65;
    CHECK_FALSE(parse_native_display_resolution(request(), wire));
}

TEST_CASE("Native unit transport observes once while recording-only transports stay unavailable",
          "[ableton][native-units]") {
    ObservationTransport transport;
    auto parsed = resolve_native_display_value(device, "utility.gain", -6, 0, transport);
    REQUIRE(parsed);
    REQUIRE(parsed->candidate);
    REQUIRE(transport.requests.size() == 1);
    CHECK(std::get<json>(transport.requests[0].args[0]) ==
          json{{"capability_id", "utility.gain"}, {"target", -6.0}, {"tolerance", 0.0}});
    transport.recording = true;
    parsed = resolve_native_display_value(device, "utility.gain", -6, 0, transport);
    REQUIRE(parsed);
    CHECK(parsed->status == NativeDisplayResolutionStatus::ObservationUnavailable);
    CHECK_FALSE(parsed->candidate);
    CHECK_FALSE(parsed->evidence);
    CHECK(parsed->formatter_calls == 0);
    CHECK(transport.requests.size() == 1);
    transport.recording = false;
    transport.response.value.reset();
    CHECK_FALSE(resolve_native_display_value(device, "utility.gain", -6, 0, transport));
    transport.response.success = false;
    auto failed = resolve_native_display_value(device, "utility.gain", -6, 0, transport);
    REQUIRE_FALSE(failed);
    CHECK(failed.error() == ErrorCode::SendFailed);
}
