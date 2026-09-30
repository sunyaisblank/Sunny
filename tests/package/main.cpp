#include <span>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/rhythm/meter.hpp>
#include <sunny/core/score/score_document.hpp>
#include <sunny/core/score/tuning.hpp>
#include <sunny/core/score/types.hpp>
#include <sunny/infrastructure/ableton/target_snapshot.hpp>
#include <sunny/infrastructure/ableton/validation_record.hpp>
#include <sunny/infrastructure/formats/scala.hpp>
#include <sunny/infrastructure/max/max_test_result.hpp>
#include <sunny/infrastructure/max/release_matrix.hpp>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <sunny/max/clock_adapter.hpp>
#include <sunny/max/itm_event_adapter.hpp>
#include <sunny/max/modulation_adapter.hpp>
#include <sunny/render/block_clock.hpp>
#include <sunny/render/block_event_scheduler.hpp>
#include <sunny/render/modulation.hpp>
#include <sunny/render/transport.hpp>
#include <sunny/version.hpp>
#include <type_traits>
#include <vector>

namespace {

struct MaxEventActions {
    std::size_t scheduled = 0;
    std::size_t cancelled = 0;
};

void schedule_max_event(void* context, std::size_t, double) noexcept {
    ++static_cast<MaxEventActions*>(context)->scheduled;
}

void cancel_max_event(void* context, std::size_t) noexcept {
    ++static_cast<MaxEventActions*>(context)->cancelled;
}

} // namespace

int main() {
    const auto pitch = sunny::core::PitchClass::wrapped(0);
    const auto tempo = sunny::core::PositiveRational::from_ratio(240, 2);
    const auto grouped_meter = sunny::core::TimeSignature::from_groups({3, 2}, 8);
    const auto clock = sunny::render::BlockClock::create(480);
    const auto signal_context = sunny::render::SignalBlockContext::create(48'000.0, 1);
    if (!signal_context) return 1;
    sunny::render::Lfo lfo;
    double sample{};
    double* outputs[1]{&sample};
    const auto compatible = lfo.validate_context(*signal_context);
    const auto configured = lfo.set_frequency(1.0, *signal_context);
    const auto rendered =
        compatible ? lfo.process_block(*signal_context, 1, 0, nullptr, 1, outputs) : compatible;
    sunny::render::Transport transport;
    sunny::infrastructure::Orchestrator orchestrator;
    sunny::max::LfoAdapter max_lfo;
    sunny::max::SampleAndHoldAdapter max_hold;
    sunny::max::ClockAdapter max_clock;
    sunny::max::ItmEventAdapter max_events;
    sunny::render::BlockEventScheduler event_scheduler;
    const sunny::core::ScoreTuning default_tuning;
    const auto a4 = sunny::core::score_tuned_frequency(default_tuning, 69);
    sunny::infrastructure::formats::ScalaTuning scala{"one step per octave",
                                                      {{1200.0, false, 0, 0}}};
    const auto imported_tuning =
        sunny::infrastructure::formats::scala_to_score_tuning(scala, 69, 440.0);
    const auto max_configured = max_lfo.configure_dsp(48'000.0, 64);
    const auto max_hold_configured = max_hold.configure_dsp(48'000.0, 64);
    const auto max_hold_published = max_hold.publish_value(0.25);
    double held_sample{};
    double* held_outputs[1]{&held_sample};
    const auto max_hold_rendered = max_hold.process(1, 0, nullptr, 1, held_outputs);
    const auto max_clock_configured = max_clock.configure_dsp(1920.0, 2);
    const auto max_clock_playing = max_clock.publish_play();
    double clock_samples[2]{};
    double* clock_outputs[1]{clock_samples};
    const auto max_clock_rendered = max_clock.process(2, 0, nullptr, 1, clock_outputs);
    const auto event_scheduled = event_scheduler.schedule_note(0, 60, sunny::core::Beat{1, 4}, 100);
    event_scheduler.play();
    sunny::render::SampleOffsetEvent offset_event{};
    const auto event_rendered =
        event_scheduler.process_block(*signal_context, 1, std::span{&offset_event, 1});
    const auto max_event_published = max_events.publish_event(100, 60, 100);
    MaxEventActions max_event_actions;
    const auto max_event_applied = max_events.apply_pending(sunny::max::ItmHostSnapshot{0.0, 480.0},
                                                            &max_event_actions,
                                                            schedule_max_event,
                                                            cancel_max_event);

    static_assert(sunny::infrastructure::SUNNY_BRIDGE_PROTOCOL_VERSION > 0);
    static_assert(sunny::infrastructure::SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION > 0);
    static_assert(sunny::infrastructure::ABLETON_VALIDATION_RECORD_SCHEMA_VERSION == 1);
    static_assert(sunny::infrastructure::MAX_VALIDATION_RECORD_SCHEMA_VERSION == 1);
    static_assert(sunny::infrastructure::MAX_VALIDATION_OBSERVATION_SCHEMA_VERSION == 1);
    static_assert(sunny::infrastructure::MAX_TEST_RESULT_SCHEMA_VERSION == 1);
    static_assert(sunny::infrastructure::MAX_TEST_HARNESS_MANIFEST_SHA256.size() == 64);
    static_assert(sunny::infrastructure::MAX_RELEASE_MATRIX_SCHEMA_VERSION == 1);
    static_assert(sunny::infrastructure::MAX_RELEASE_MATRIX_MANIFEST_SHA256.size() == 64);
    static_assert(!sunny::SUNNY_VERSION.empty());
    static_assert(std::is_copy_constructible_v<sunny::core::ScoreDocument>);
    static_assert(!std::is_aggregate_v<sunny::core::PositiveRational>);
    static_assert(!std::is_aggregate_v<sunny::core::TimeSignature>);
    static_assert(sunny::render::Lfo::signal_input_count == 0);
    static_assert(sunny::render::Lfo::signal_output_count == 1);
    static_assert(sunny::render::SampleAndHold::signal_input_count == 0);
    static_assert(sunny::render::SampleAndHold::signal_output_count == 1);
    static_assert(sunny::render::BlockClock::signal_input_count == 0);
    static_assert(sunny::render::BlockClock::signal_output_count == 1);

    return pitch.value() +
           (tempo && tempo->numerator() == 120 && tempo->denominator() == 1 ? 0 : 1) +
           (grouped_meter && grouped_meter->groups() == std::vector<int>{3, 2} &&
                    sunny::core::has_distinct_meter_grouping(*grouped_meter)
                ? 0
                : 1) +
           (clock ? 0 : 1) + (transport.is_playing() ? 1 : 0) + (orchestrator.can_undo() ? 1 : 0) +
           (configured ? 0 : 1) + (rendered ? 0 : 1) + (a4 && *a4 == 440.0 ? 0 : 1) +
           (imported_tuning ? 0 : 1) + (max_configured ? 0 : 1) + (max_hold_configured ? 0 : 1) +
           (max_hold_published ? 0 : 1) + (max_hold_rendered && held_sample == 0.25 ? 0 : 1) +
           (max_clock_configured ? 0 : 1) + (max_clock_playing ? 0 : 1) +
           (max_clock_rendered && clock_samples[0] == 0.0 && clock_samples[1] > 0.0 ? 0 : 1) +
           (event_scheduled ? 0 : 1) +
           (event_rendered && event_rendered->events_written == 1 && offset_event.sample_offset == 0
                ? 0
                : 1) +
           (max_event_published && max_event_applied.commands_applied == 1 &&
                    max_events.status().events_reserved == 1 && max_event_actions.scheduled == 1 &&
                    max_event_actions.cancelled == 0
                ? 0
                : 1);
}
