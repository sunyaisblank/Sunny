/**
 * @file sunny.events.cpp
 * @brief Max ITM permanent-event wrapper for Sunny's bounded event adapter
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>
#include <sunny/max/itm_event_adapter.hpp>

namespace {

struct SunnyEvents;

struct SunnyEventSlot {
    t_object object;
    SunnyEvents* owner;
    std::size_t index;
    t_timeobject* time;
};

struct SunnyEvents {
    t_object object;
    sunny::max::ItmEventAdapter* adapter;
    void* command_clock;
    void* event_outlet;
    void* status_outlet;
    t_itm* itm;
    std::array<SunnyEventSlot*, sunny::max::MAX_ITM_EVENT_CAPACITY> slots;
};

t_class* sunny_events_class = nullptr;
t_class* sunny_event_slot_class = nullptr;

void report_error(SunnyEvents* instance, const char* operation, sunny::core::ErrorCode error) {
    object_error(reinterpret_cast<t_object*>(instance),
                 "%s declined (Sunny error %d)",
                 operation,
                 static_cast<int>(error));
}

void wake_consumer(void* context) noexcept {
    auto* instance = static_cast<SunnyEvents*>(context);
    clock_fdelay(instance->command_clock, 0.0);
}

void report_publication(SunnyEvents* instance,
                        const char* operation,
                        const sunny::core::VoidResult& result) {
    if (!result) report_error(instance, operation, result.error());
}

void schedule_slot(void* context, std::size_t slot_index, double host_tick) noexcept {
    auto* instance = static_cast<SunnyEvents*>(context);
    auto* slot = instance->slots[slot_index];
    t_atom target;
    atom_setfloat(&target, host_tick);
    time_setvalue(slot->time, nullptr, 1, &target);
}

void cancel_slot(void* context, std::size_t slot_index) noexcept {
    auto* instance = static_cast<SunnyEvents*>(context);
    time_stop(instance->slots[slot_index]->time);
}

[[nodiscard]] sunny::max::ItmHostSnapshot host_snapshot(const SunnyEvents* instance) noexcept {
    return {
        .current_tick = itm_getticks(instance->itm),
        .ticks_per_quarter = itm_getresolution(),
    };
}

void sunny_events_apply(SunnyEvents* instance) {
    (void)instance->adapter->apply_pending(
        host_snapshot(instance), instance, schedule_slot, cancel_slot);
}

void sunny_event_slot_tick(SunnyEventSlot* slot) {
    if (slot->owner == nullptr || slot->owner->adapter == nullptr) return;
    std::array<sunny::render::ScheduledEvent, sunny::max::MAX_ITM_EVENT_CAPACITY> events{};
    const auto fired = slot->owner->adapter->fire(slot->index, events, slot->owner, cancel_slot);
    if (!fired) return;

    for (std::size_t index = 0; index < *fired; ++index) {
        t_atom message[3];
        atom_setlong(&message[0], static_cast<t_atom_long>(events[index].event.pitch.value()));
        atom_setlong(&message[1], static_cast<t_atom_long>(events[index].event.velocity));
        atom_setlong(&message[2], static_cast<t_atom_long>(events[index].event.release_velocity));
        outlet_list(slot->owner->event_outlet, nullptr, 3, message);
    }
}

void sunny_events_event(SunnyEvents* instance, t_symbol*, long argument_count, t_atom* arguments) {
    if ((argument_count != 3 && argument_count != 4) ||
        std::any_of(arguments, arguments + argument_count, [](const t_atom& argument) {
            return atom_gettype(&argument) != A_LONG;
        })) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "event expects tick pitch velocity [release_velocity]");
        return;
    }
    const auto release_velocity = argument_count == 4 ? atom_getlong(arguments + 3) : 64;
    report_publication(instance,
                       "event",
                       instance->adapter->publish_event(atom_getlong(arguments),
                                                        atom_getlong(arguments + 1),
                                                        atom_getlong(arguments + 2),
                                                        release_velocity,
                                                        instance,
                                                        wake_consumer));
}

void sunny_events_note(SunnyEvents* instance, t_symbol*, long argument_count, t_atom* arguments) {
    if ((argument_count != 4 && argument_count != 5) ||
        std::any_of(arguments, arguments + argument_count, [](const t_atom& argument) {
            return atom_gettype(&argument) != A_LONG;
        })) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "note expects tick pitch duration_ticks velocity [release_velocity]");
        return;
    }
    const auto release_velocity = argument_count == 5 ? atom_getlong(arguments + 4) : 64;
    report_publication(instance,
                       "note",
                       instance->adapter->publish_note(atom_getlong(arguments),
                                                       atom_getlong(arguments + 1),
                                                       atom_getlong(arguments + 2),
                                                       atom_getlong(arguments + 3),
                                                       release_velocity,
                                                       instance,
                                                       wake_consumer));
}

void sunny_events_event_after(SunnyEvents* instance,
                              t_symbol*,
                              long argument_count,
                              t_atom* arguments) {
    if ((argument_count != 3 && argument_count != 4) ||
        std::any_of(arguments, arguments + argument_count, [](const t_atom& argument) {
            return atom_gettype(&argument) != A_LONG;
        })) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "event_after expects delay_ticks pitch velocity [release_velocity]");
        return;
    }
    const auto tick = sunny::max::tick_after_itm(
        host_snapshot(instance), instance->adapter->ppq(), atom_getlong(arguments));
    if (!tick) {
        report_error(instance, "event_after", tick.error());
        return;
    }
    const auto release_velocity = argument_count == 4 ? atom_getlong(arguments + 3) : 64;
    report_publication(instance,
                       "event_after",
                       instance->adapter->publish_event(*tick,
                                                        atom_getlong(arguments + 1),
                                                        atom_getlong(arguments + 2),
                                                        release_velocity,
                                                        instance,
                                                        wake_consumer));
}

void sunny_events_note_after(SunnyEvents* instance,
                             t_symbol*,
                             long argument_count,
                             t_atom* arguments) {
    if ((argument_count != 4 && argument_count != 5) ||
        std::any_of(arguments, arguments + argument_count, [](const t_atom& argument) {
            return atom_gettype(&argument) != A_LONG;
        })) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "note_after expects delay_ticks pitch duration_ticks velocity "
                     "[release_velocity]");
        return;
    }
    const auto tick = sunny::max::tick_after_itm(
        host_snapshot(instance), instance->adapter->ppq(), atom_getlong(arguments));
    if (!tick) {
        report_error(instance, "note_after", tick.error());
        return;
    }
    const auto release_velocity = argument_count == 5 ? atom_getlong(arguments + 4) : 64;
    report_publication(instance,
                       "note_after",
                       instance->adapter->publish_note(*tick,
                                                       atom_getlong(arguments + 1),
                                                       atom_getlong(arguments + 2),
                                                       atom_getlong(arguments + 3),
                                                       release_velocity,
                                                       instance,
                                                       wake_consumer));
}

void sunny_events_clear(SunnyEvents* instance) {
    report_publication(
        instance, "clear", instance->adapter->publish_clear(instance, wake_consumer));
}

void sunny_events_clear_error(SunnyEvents* instance) {
    instance->adapter->clear_error();
}

void sunny_events_status(SunnyEvents* instance) {
    const auto status = instance->adapter->status();
    object_post(reinterpret_cast<t_object*>(instance),
                "enqueued=%llu applied=%llu rejected=%llu pending_commands=%llu scheduled=%llu "
                "fired=%llu cancelled=%llu callback_failures=%llu reserved_events=%llu "
                "retained_events=%llu last_error=%d",
                static_cast<unsigned long long>(status.commands_enqueued),
                static_cast<unsigned long long>(status.commands_applied),
                static_cast<unsigned long long>(status.commands_rejected),
                static_cast<unsigned long long>(status.commands_pending),
                static_cast<unsigned long long>(status.events_scheduled),
                static_cast<unsigned long long>(status.events_fired),
                static_cast<unsigned long long>(status.events_cancelled),
                static_cast<unsigned long long>(status.callback_failures),
                static_cast<unsigned long long>(status.events_reserved),
                static_cast<unsigned long long>(status.events_retained),
                static_cast<int>(status.last_error));
    t_atom evidence[8];
    atom_setlong(&evidence[0], status.commands_enqueued != 0 ? 1 : 0);
    atom_setlong(&evidence[1], status.events_scheduled != 0 ? 1 : 0);
    atom_setlong(&evidence[2], status.events_fired != 0 ? 1 : 0);
    atom_setlong(&evidence[3], status.events_cancelled != 0 ? 1 : 0);
    atom_setlong(&evidence[4], status.callback_failures == 0 ? 1 : 0);
    atom_setlong(&evidence[5], static_cast<t_atom_long>(status.events_reserved));
    atom_setlong(&evidence[6], static_cast<t_atom_long>(status.events_retained));
    atom_setlong(&evidence[7], static_cast<t_atom_long>(status.last_error));
    outlet_anything(instance->status_outlet, gensym("event_status"), 8, evidence);
}

void sunny_events_transport_status(SunnyEvents* instance) {
    const auto host = host_snapshot(instance);
    auto* const name = itm_getname(instance->itm);
    if (!std::isfinite(host.current_tick) || host.current_tick < 0.0 ||
        !std::isfinite(host.ticks_per_quarter) || host.ticks_per_quarter <= 0.0 ||
        name == nullptr) {
        report_error(
            instance, "transport_status", sunny::core::ErrorCode::RenderUnrepresentableTiming);
        return;
    }

    t_atom snapshot[4];
    atom_setfloat(&snapshot[0], host.current_tick);
    atom_setfloat(&snapshot[1], host.ticks_per_quarter);
    atom_setlong(&snapshot[2], itm_getstate(instance->itm) != 0 ? 1 : 0);
    atom_setsym(&snapshot[3], name);
    outlet_anything(instance->status_outlet, gensym("transport_status"), 4, snapshot);
}

void sunny_events_assist(SunnyEvents*, void*, long message, long argument, char* text) {
    if (message == ASSIST_INLET)
        std::snprintf(text,
                      512,
                      "Messages: event/event_after position pitch velocity [release_velocity]; "
                      "note/note_after position pitch duration_ticks velocity [release_velocity]; "
                      "clear; status; transport_status; clear_error");
    else if (argument == 1)
        std::snprintf(text,
                      512,
                      "event_status diagnostics or transport_status snapshot from the matching "
                      "message");
    else
        std::snprintf(text, 512, "(list) pitch, attack-or-zero velocity, release velocity");
}

void sunny_event_slot_free(SunnyEventSlot* slot) {
    if (slot->time != nullptr) {
        time_stop(slot->time);
        freeobject(slot->time);
        slot->time = nullptr;
    }
}

void sunny_events_free(SunnyEvents* instance) {
    if (instance->command_clock != nullptr) {
        clock_unset(instance->command_clock);
        freeobject(instance->command_clock);
        instance->command_clock = nullptr;
    }
    for (auto*& slot : instance->slots) {
        if (slot == nullptr) continue;
        slot->owner = nullptr;
        object_free(slot);
        slot = nullptr;
    }
    delete instance->adapter;
    instance->adapter = nullptr;
    if (instance->itm != nullptr) {
        itm_dereference(instance->itm);
        instance->itm = nullptr;
    }
}

void* sunny_events_new(t_symbol*, long argument_count, t_atom* arguments) {
    auto* instance = static_cast<SunnyEvents*>(object_alloc(sunny_events_class));
    if (instance == nullptr) return nullptr;
    instance->adapter = nullptr;
    instance->command_clock = nullptr;
    instance->event_outlet = nullptr;
    instance->status_outlet = nullptr;
    instance->itm = nullptr;
    instance->slots.fill(nullptr);

    if (argument_count > 1 || (argument_count == 1 && atom_gettype(arguments) != A_LONG)) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "expected at most one integer PPQ argument");
        object_free(instance);
        return nullptr;
    }
    const auto ppq = argument_count == 1 ? atom_getlong(arguments) : sunny::render::DEFAULT_PPQ;
    if (ppq < 1 || ppq > 65'535) {
        report_error(instance, "initial PPQ", sunny::core::ErrorCode::RenderInvalidPPQ);
        object_free(instance);
        return nullptr;
    }

    instance->itm = static_cast<t_itm*>(itm_getglobal());
    if (instance->itm == nullptr) {
        object_error(reinterpret_cast<t_object*>(instance),
                     "unable to reference the global Max transport");
        object_free(instance);
        return nullptr;
    }
    itm_reference(instance->itm);

    instance->adapter = new (std::nothrow) sunny::max::ItmEventAdapter{ppq};
    instance->command_clock = clock_new(instance, reinterpret_cast<method>(sunny_events_apply));
    instance->status_outlet = outlet_new(reinterpret_cast<t_object*>(instance), nullptr);
    instance->event_outlet = outlet_new(reinterpret_cast<t_object*>(instance), nullptr);
    if (instance->adapter == nullptr || instance->command_clock == nullptr ||
        instance->status_outlet == nullptr || instance->event_outlet == nullptr) {
        object_error(reinterpret_cast<t_object*>(instance), "unable to allocate Sunny event state");
        object_free(instance);
        return nullptr;
    }

    constexpr long time_flags = TIME_FLAGS_TICKSONLY | TIME_FLAGS_USECLOCK | TIME_FLAGS_PERMANENT |
                                TIME_FLAGS_LOCATION | TIME_FLAGS_POSITIVE;
    for (std::size_t index = 0; index < instance->slots.size(); ++index) {
        auto* slot = static_cast<SunnyEventSlot*>(object_alloc(sunny_event_slot_class));
        if (slot == nullptr) {
            object_error(reinterpret_cast<t_object*>(instance),
                         "unable to allocate fixed ITM event slot %llu",
                         static_cast<unsigned long long>(index));
            object_free(instance);
            return nullptr;
        }
        slot->owner = instance;
        slot->index = index;
        slot->time =
            static_cast<t_timeobject*>(time_new(reinterpret_cast<t_object*>(slot),
                                                gensym("targettime"),
                                                reinterpret_cast<method>(sunny_event_slot_tick),
                                                time_flags));
        instance->slots[index] = slot;
        if (slot->time == nullptr) {
            object_error(reinterpret_cast<t_object*>(instance),
                         "unable to allocate ITM time object %llu",
                         static_cast<unsigned long long>(index));
            object_free(instance);
            return nullptr;
        }
        time_stop(slot->time);
    }
    return instance;
}

} // namespace

extern "C" C74_EXPORT void ext_main(void*) {
    auto* slot_class = class_new("sunny.events.slot",
                                 nullptr,
                                 reinterpret_cast<method>(sunny_event_slot_free),
                                 sizeof(SunnyEventSlot),
                                 nullptr,
                                 0);
    if (slot_class == nullptr) return;
    constexpr long time_flags = TIME_FLAGS_TICKSONLY | TIME_FLAGS_PERMANENT | TIME_FLAGS_LOCATION |
                                TIME_FLAGS_TRANSPORT | TIME_FLAGS_POSITIVE;
    class_time_addattr(slot_class, "targettime", "Target Time", time_flags);

    auto* klass = class_new("sunny.events",
                            reinterpret_cast<method>(sunny_events_new),
                            reinterpret_cast<method>(sunny_events_free),
                            sizeof(SunnyEvents),
                            nullptr,
                            A_GIMME,
                            0);
    if (klass == nullptr) {
        (void)class_free(slot_class);
        return;
    }
    t_max_err method_error = MAX_ERR_NONE;
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_events_event), "event", A_GIMME, 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_events_note), "note", A_GIMME, 0);
    method_error |= class_addmethod(
        klass, reinterpret_cast<method>(sunny_events_event_after), "event_after", A_GIMME, 0);
    method_error |= class_addmethod(
        klass, reinterpret_cast<method>(sunny_events_note_after), "note_after", A_GIMME, 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_events_clear), "clear", 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_events_status), "status", 0);
    method_error |= class_addmethod(
        klass, reinterpret_cast<method>(sunny_events_transport_status), "transport_status", 0);
    method_error |= class_addmethod(
        klass, reinterpret_cast<method>(sunny_events_clear_error), "clear_error", 0);
    method_error |=
        class_addmethod(klass, reinterpret_cast<method>(sunny_events_assist), "assist", A_CANT, 0);
    if (method_error != MAX_ERR_NONE || class_register(CLASS_NOBOX, slot_class) != MAX_ERR_NONE) {
        (void)class_free(klass);
        (void)class_free(slot_class);
        return;
    }
    sunny_event_slot_class = slot_class;
    if (class_register(CLASS_BOX, klass) != MAX_ERR_NONE) {
        (void)class_free(klass);
        return;
    }
    sunny_events_class = klass;
}
