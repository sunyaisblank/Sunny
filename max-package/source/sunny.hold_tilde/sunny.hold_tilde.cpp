/**
 * @file sunny.hold_tilde.cpp
 * @brief Traditional Max/MSP wrapper for Sunny's bounded held-value adapter
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <cstdint>
#include <cstdio>
#include <new>
#include <sunny/max/modulation_adapter.hpp>

namespace {

struct SunnyHold {
    t_pxobject object;
    sunny::max::SampleAndHoldAdapter* adapter;
    void* status_outlet;
    std::int64_t callback_maximum_frames;
    bool dsp_setup_accepted;
};

t_class* sunny_hold_class = nullptr;

void report_error(SunnyHold* instance, const char* operation, sunny::core::ErrorCode error) {
    object_error(reinterpret_cast<t_object*>(instance),
                 "%s declined (Sunny error %d)",
                 operation,
                 static_cast<int>(error));
}

void report_result(SunnyHold* instance,
                   const char* operation,
                   const sunny::core::VoidResult& result) {
    if (!result) report_error(instance, operation, result.error());
}

void sunny_hold_perform64(SunnyHold* instance,
                          t_object*,
                          double** inputs,
                          long input_count,
                          double** outputs,
                          long output_count,
                          long frame_count,
                          long,
                          void*) {
    if (instance->dsp_setup_accepted) {
        const auto rendered =
            instance->adapter->process(frame_count, input_count, inputs, output_count, outputs);
        if (rendered) return;
    }
    (void)sunny::max::detail::silence_generator_output(
        instance->callback_maximum_frames, frame_count, input_count, output_count, outputs);
}

void sunny_hold_dsp64(
    SunnyHold* instance, t_object* dsp64, short*, double sample_rate, long maximum_frames, long) {
    instance->callback_maximum_frames = static_cast<std::int64_t>(maximum_frames);
    const auto configured = instance->adapter->configure_dsp(sample_rate, maximum_frames);
    instance->dsp_setup_accepted = configured.has_value();
    if (!configured) report_error(instance, "dsp64", configured.error());
    dsp_add64(dsp64,
              reinterpret_cast<t_object*>(instance),
              reinterpret_cast<t_perfroutine64>(sunny_hold_perform64),
              0,
              nullptr);
}

void sunny_hold_value(SunnyHold* instance, double value) {
    report_result(instance, "value", instance->adapter->publish_value(value));
}

void sunny_hold_reset(SunnyHold* instance) {
    report_result(instance, "reset", instance->adapter->publish_reset());
}

void sunny_hold_clear_error(SunnyHold* instance) {
    instance->adapter->clear_error();
}

void sunny_hold_status(SunnyHold* instance) {
    const auto status = instance->adapter->status();
    object_post(reinterpret_cast<t_object*>(instance),
                "configured=%d sample_rate=%.17g maximum_frames=%lld setup_failures=%llu "
                "enqueued=%llu applied=%llu rejected=%llu pending=%llu process_calls=%lld "
                "last_frame_count=%lld process_failures=%llu last_error=%d",
                status.configured ? 1 : 0,
                status.sample_rate,
                static_cast<long long>(status.maximum_frames),
                static_cast<unsigned long long>(status.setup_failures),
                static_cast<unsigned long long>(status.controls_enqueued),
                static_cast<unsigned long long>(status.controls_applied),
                static_cast<unsigned long long>(status.controls_rejected),
                static_cast<unsigned long long>(status.controls_pending),
                static_cast<long long>(status.process_calls),
                static_cast<long long>(status.last_frame_count),
                static_cast<unsigned long long>(status.process_failures),
                static_cast<int>(status.last_error));
    t_atom evidence[8];
    atom_setlong(&evidence[0], status.configured ? 1 : 0);
    atom_setfloat(&evidence[1], status.sample_rate);
    atom_setlong(&evidence[2], static_cast<t_atom_long>(status.maximum_frames));
    atom_setlong(&evidence[3], status.process_calls != 0 ? 1 : 0);
    atom_setlong(&evidence[4], static_cast<t_atom_long>(status.process_calls));
    atom_setlong(&evidence[5], static_cast<t_atom_long>(status.last_frame_count));
    atom_setlong(&evidence[6], status.process_failures == 0 ? 1 : 0);
    atom_setlong(&evidence[7], static_cast<t_atom_long>(status.last_error));
    outlet_anything(instance->status_outlet, gensym("host_status"), 8, evidence);
}

void sunny_hold_assist(SunnyHold*, void*, long message, long argument, char* text) {
    if (message == ASSIST_INLET)
        std::snprintf(text, 512, "Messages: float/value [-1, 1], reset, status, clear_error");
    else if (argument == 1)
        std::snprintf(text, 512, "host_status diagnostics from the status message");
    else
        std::snprintf(text, 512, "(signal) held message value in [-1, 1]");
}

void sunny_hold_free(SunnyHold* instance) {
    dsp_free(reinterpret_cast<t_pxobject*>(instance));
    delete instance->adapter;
}

void* sunny_hold_new() {
    auto* instance = static_cast<SunnyHold*>(object_alloc(sunny_hold_class));
    if (instance == nullptr) return nullptr;
    instance->adapter = nullptr;
    instance->status_outlet = nullptr;
    instance->callback_maximum_frames = 0;
    instance->dsp_setup_accepted = false;
    dsp_setup(reinterpret_cast<t_pxobject*>(instance), 0);
    instance->object.z_misc |= Z_NO_INPLACE;
    instance->status_outlet = outlet_new(reinterpret_cast<t_object*>(instance), nullptr);
    auto* signal_outlet = outlet_new(reinterpret_cast<t_object*>(instance), "signal");
    if (instance->status_outlet == nullptr || signal_outlet == nullptr) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "unable to allocate Sunny signal/status outlets");
        object_free(instance);
        return nullptr;
    }
    instance->adapter = new (std::nothrow) sunny::max::SampleAndHoldAdapter;
    if (instance->adapter == nullptr) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "unable to allocate Sunny held-value adapter");
        object_free(instance);
        return nullptr;
    }
    return instance;
}

} // namespace

extern "C" C74_EXPORT void ext_main(void*) {
    auto* klass = class_new("sunny.hold~",
                            reinterpret_cast<method>(sunny_hold_new),
                            reinterpret_cast<method>(sunny_hold_free),
                            sizeof(SunnyHold),
                            nullptr,
                            0);
    if (klass == nullptr) return;
    class_dspinit(klass);
    t_max_err method_error = MAX_ERR_NONE;
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_dsp64), "dsp64", A_CANT, 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_value), "float", A_FLOAT, 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_value), "value", A_FLOAT, 0);
    method_error |= class_addmethod(klass, reinterpret_cast<method>(sunny_hold_reset), "reset", 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_status), "status", 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_clear_error), "clear_error", 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_hold_assist), "assist", A_CANT, 0);
    if (method_error != MAX_ERR_NONE || class_register(CLASS_BOX, klass) != MAX_ERR_NONE) {
        (void)class_free(klass);
        return;
    }
    sunny_hold_class = klass;
}
