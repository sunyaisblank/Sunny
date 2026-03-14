/**
 * @file TSMI001A.cpp
 * @brief Unit tests for Mix IR foundation types (MITP001A)
 *
 * Component: TSMI001A
 * Domain: TS (Test) | Category: MI (Mix IR)
 *
 * Tests: MITP001A
 * Coverage: Id types, enumerations, SpatialPosition, Fader, MixEQ,
 *           MixCompressor, MixGate, MixLimiter, MixSaturation,
 *           MixStereoProcessor, LoudnessTarget, MeteringConfig
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Mix/MITP001A.h"

using namespace Sunny::Core;

// =============================================================================
// Identifier types
// =============================================================================

TEST_CASE("MITP001A: MixGraphId equality and ordering", "[mix-ir][types]") {
    MixGraphId a{1};
    MixGraphId b{1};
    MixGraphId c{2};

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
}

TEST_CASE("MITP001A: ChannelStripId and GroupBusId are distinct", "[mix-ir][types]") {
    ChannelStripId ch{42};
    GroupBusId gb{42};
    CHECK(ch.value == gb.value);
    // No cross-type comparison possible at compile time
}

TEST_CASE("MITP001A: AuxBusId and MixEffectId are distinct", "[mix-ir][types]") {
    AuxBusId aux{10};
    MixEffectId fx{10};
    CHECK(aux.value == fx.value);
}

// =============================================================================
// SpatialPosition
// =============================================================================

TEST_CASE("MITP001A: SpatialPosition default is centre", "[mix-ir][types]") {
    SpatialPosition sp;
    CHECK(sp.pan == 0.0f);
    CHECK(sp.depth == 0.0f);
    CHECK(sp.elevation == 0.0f);
    CHECK(sp.width == 0.0f);
    CHECK(sp.pan_law == PanLaw::ConstantPower);
    CHECK(sp.spatial_mode == SpatialMode::SimplePan);
}

TEST_CASE("MITP001A: SpatialPosition hard left/right", "[mix-ir][types]") {
    SpatialPosition left;
    left.pan = -1.0f;
    CHECK(left.pan == -1.0f);

    SpatialPosition right;
    right.pan = 1.0f;
    CHECK(right.pan == 1.0f);
}

TEST_CASE("MITP001A: SpatialPosition with depth", "[mix-ir][types]") {
    SpatialPosition sp;
    sp.depth = 0.7f;
    sp.elevation = 0.3f;
    CHECK(sp.depth == Catch::Approx(0.7f));
    CHECK(sp.elevation == Catch::Approx(0.3f));
}

// =============================================================================
// Fader
// =============================================================================

TEST_CASE("MITP001A: Fader default is 0 dB", "[mix-ir][types]") {
    Fader f;
    CHECK(f.level_db == 0.0f);
    CHECK_FALSE(f.relative_level.has_value());
}

TEST_CASE("MITP001A: Fader with relative level", "[mix-ir][types]") {
    Fader f;
    f.level_db = -3.0f;
    RelativeLevel rel;
    rel.reference.type = LevelReferenceType::Channel;
    rel.reference.channel_id = ChannelStripId{5};
    rel.reference.relationship = "3 dB below Violin I";
    rel.offset_db = -3.0f;
    f.relative_level = rel;

    REQUIRE(f.relative_level.has_value());
    CHECK(f.relative_level->reference.type == LevelReferenceType::Channel);
    CHECK(f.relative_level->offset_db == -3.0f);
}

// =============================================================================
// AuxSendLevel
// =============================================================================

TEST_CASE("MITP001A: AuxSendLevel default values", "[mix-ir][types]") {
    AuxSendLevel send;
    CHECK(send.level_db == -12.0f);
    CHECK_FALSE(send.pre_fader);
    CHECK(send.enabled);
}

// =============================================================================
// MixEQ
// =============================================================================

TEST_CASE("MITP001A: MixEQBand default values", "[mix-ir][types]") {
    MixEQBand band;
    CHECK(band.frequency == 1000.0f);
    CHECK(band.gain == 0.0f);
    CHECK(band.q == 1.0f);
    CHECK(band.band_type == MixEQBandType::Peak);
    CHECK_FALSE(band.dynamic.has_value());
}

TEST_CASE("MITP001A: MixEQBand with dynamic config", "[mix-ir][types]") {
    MixEQBand band;
    band.band_type = MixEQBandType::HighShelf;
    band.dynamic = DynamicEQConfig{-15.0f, 3.0f, 5.0f, 50.0f};

    REQUIRE(band.dynamic.has_value());
    CHECK(band.dynamic->threshold == -15.0f);
    CHECK(band.dynamic->ratio == 3.0f);
}

TEST_CASE("MITP001A: MixEQ with multiple bands", "[mix-ir][types]") {
    MixEQ eq;
    eq.bands.push_back({80.0f, 0.0f, 0.7f, MixEQBandType::LowCut, std::nullopt});
    eq.bands.push_back({3000.0f, -2.5f, 2.0f, MixEQBandType::Peak, std::nullopt});
    eq.linear_phase = true;
    eq.auto_gain = true;

    CHECK(eq.bands.size() == 2);
    CHECK(eq.linear_phase);
    CHECK(eq.auto_gain);
}

// =============================================================================
// MixCompressor
// =============================================================================

TEST_CASE("MITP001A: MixCompressor default values", "[mix-ir][types]") {
    MixCompressor comp;
    CHECK(comp.threshold == -20.0f);
    CHECK(comp.ratio == 4.0f);
    CHECK(comp.attack == 10.0f);
    CHECK(comp.release == 100.0f);
    CHECK(comp.detection == DetectionMode::RMS);
    CHECK(comp.topology == CompressorTopology::FeedForward);
    CHECK(comp.sidechain.source == SidechainSourceType::Internal);
}

TEST_CASE("MITP001A: MixCompressor with external sidechain", "[mix-ir][types]") {
    MixCompressor comp;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = ChannelStripId{3};
    comp.sidechain.filter = SidechainFilter{MixEQBandType::LowCut, 200.0f, 1.0f};

    CHECK(comp.sidechain.source == SidechainSourceType::ExternalChannel);
    REQUIRE(comp.sidechain.filter.has_value());
    CHECK(comp.sidechain.filter->frequency == 200.0f);
}

// =============================================================================
// MixGate, MixLimiter
// =============================================================================

TEST_CASE("MITP001A: MixGate default values", "[mix-ir][types]") {
    MixGate gate;
    CHECK(gate.threshold == -40.0f);
    CHECK(gate.hold == 50.0f);
    CHECK(gate.range == -80.0f);
}

TEST_CASE("MITP001A: MixLimiter default values", "[mix-ir][types]") {
    MixLimiter lim;
    CHECK(lim.ceiling == -1.0f);
    CHECK(lim.algorithm == LimiterAlgorithm::TruePeak);
}

// =============================================================================
// MixSaturation
// =============================================================================

TEST_CASE("MITP001A: MixSaturation tape mode", "[mix-ir][types]") {
    MixSaturation sat;
    sat.algorithm.type = SaturationTypeTag::Tape;
    sat.algorithm.tape_speed = TapeSpeed::Ips15;
    sat.drive = 0.4f;

    CHECK(sat.algorithm.type == SaturationTypeTag::Tape);
    CHECK(sat.algorithm.tape_speed == TapeSpeed::Ips15);
}

// =============================================================================
// MixStereoProcessor
// =============================================================================

TEST_CASE("MITP001A: MixStereoProcessor default is unity width", "[mix-ir][types]") {
    MixStereoProcessor sp;
    CHECK(sp.width == 1.0f);
    CHECK(sp.mid_side_balance == 0.5f);
    CHECK_FALSE(sp.mono_below.has_value());
}

// =============================================================================
// MixEffectParameters variant
// =============================================================================

TEST_CASE("MITP001A: MixEffect holds EQ variant", "[mix-ir][types]") {
    MixEffect fx;
    fx.id = MixEffectId{1};
    fx.parameters = MixEQ{{}, false, false};
    fx.enabled = true;

    CHECK(std::holds_alternative<MixEQ>(fx.parameters));
}

TEST_CASE("MITP001A: MixEffect holds Compressor variant", "[mix-ir][types]") {
    MixEffect fx;
    fx.parameters = MixCompressor{};
    CHECK(std::holds_alternative<MixCompressor>(fx.parameters));
}

// =============================================================================
// LoudnessTarget
// =============================================================================

TEST_CASE("MITP001A: LoudnessTarget streaming defaults", "[mix-ir][types]") {
    LoudnessTarget lt;
    CHECK(lt.integrated_lufs == -14.0f);
    CHECK(lt.true_peak_dbfs == -1.0f);
    CHECK(lt.standard == LoudnessStandard::StreamingLoud);
}

// =============================================================================
// MeteringConfig
// =============================================================================

TEST_CASE("MITP001A: MeteringConfig defaults", "[mix-ir][types]") {
    MeteringConfig mc;
    CHECK(mc.lufs);
    CHECK(mc.true_peak);
    CHECK_FALSE(mc.rms);
    CHECK(mc.correlation);
    CHECK_FALSE(mc.spectrum);
}

// =============================================================================
// ChannelIntent
// =============================================================================

TEST_CASE("MITP001A: ChannelIntent roles", "[mix-ir][types]") {
    ChannelIntent intent;
    intent.role_in_mix = MixRole::Lead;
    intent.depth_position = DepthPosition::FrontClose;
    intent.frequency_space.fundamental_low = 82.0f;
    intent.frequency_space.fundamental_high = 988.0f;
    intent.frequency_space.presence_low = 2000.0f;
    intent.frequency_space.presence_high = 5000.0f;

    CHECK(intent.role_in_mix == MixRole::Lead);
    CHECK(intent.depth_position == DepthPosition::FrontClose);
    CHECK(intent.frequency_space.fundamental_low == 82.0f);
}

// =============================================================================
// OutputFormat
// =============================================================================

TEST_CASE("MITP001A: OutputFormat enum values", "[mix-ir][types]") {
    CHECK(static_cast<int>(OutputFormat::Stereo) == 0);
    CHECK(static_cast<int>(OutputFormat::Binaural) == 6);
}

TEST_CASE("MITP001A: AtmosConfig defaults", "[mix-ir][types]") {
    AtmosConfig ac;
    CHECK(ac.bed_channels == 10);
    CHECK(ac.object_count == 16);
}

// =============================================================================
// InterpolationMode
// =============================================================================

TEST_CASE("MITP001A: InterpolationMode enum values", "[mix-ir][types]") {
    CHECK(static_cast<int>(InterpolationMode::Step) == 0);
    CHECK(static_cast<int>(InterpolationMode::Linear) == 1);
    CHECK(static_cast<int>(InterpolationMode::Smooth) == 2);
    CHECK(static_cast<int>(InterpolationMode::Exponential) == 3);
}

// =============================================================================
// MixAutomation
// =============================================================================

TEST_CASE("MITP001A: MixAutomation with breakpoints", "[mix-ir][types]") {
    MixAutomation ma;
    ma.target = "channels[0].fader.level_db";
    ma.breakpoints.push_back({ScoreTime{1, Beat{0, 1}}, -6.0f});
    ma.breakpoints.push_back({ScoreTime{5, Beat{0, 1}}, 0.0f});
    ma.interpolation = InterpolationMode::Linear;
    ma.intent = "Gradual fade in over introduction";

    CHECK(ma.breakpoints.size() == 2);
    CHECK(ma.target == "channels[0].fader.level_db");
    REQUIRE(ma.intent.has_value());
}
