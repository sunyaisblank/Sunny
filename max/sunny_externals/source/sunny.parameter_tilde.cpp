/**
 * @file sunny.parameter_tilde.cpp
 * @brief Max/MSP external for real-time parameter control
 *
 * Component: MXPR001A
 * Domain: MX (Max) | Category: PR (Parameter)
 *
 * sunny.parameter~ receives OSC messages and outputs sample-accurate
 * parameter changes for use with live.remote~ or direct signal processing.
 *
 * Inlets:
 *   1. Signal inlet (pass-through or trigger)
 *   2. Parameter target messages
 *
 * Outlets:
 *   1. Signal outlet (parameter value as signal)
 *   2. Bang on parameter change
 *
 * Messages:
 *   - target <path>: Set LOM path for parameter
 *   - value <float>: Set parameter value (with interpolation)
 *   - ramp <float> <ms>: Ramp to value over time
 *   - curve <type>: Set interpolation curve (linear, exp, log)
 *
 * Attributes:
 *   - @smooth: Smoothing time in ms (default: 10)
 *   - @min: Minimum value (default: 0)
 *   - @max: Maximum value (default: 1)
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#ifdef SUNNY_CORE_AVAILABLE
#include "RealTime/RTLF001A.h"
#endif

#include <atomic>
#include <cmath>
#include <string>

// =============================================================================
// Object Structure
// =============================================================================

/// Parameter update message for lock-free queue
struct ParamUpdate {
    float value{0.0f};
    float ramp_ms{0.0f};
};

typedef struct _sunny_parameter {
    t_pxobject ob;              // MSP object header

    // Parameter state
    double current_value;        // Current output value
    double target_value;         // Target value for ramping
    double ramp_increment;       // Per-sample increment
    long ramp_samples_remaining; // Samples until ramp complete

    // Interpolation
    double smooth_ms;            // Smoothing time
    double smooth_coeff;         // One-pole coefficient
    t_symbol* curve_type;        // Interpolation curve

    // Range
    double min_value;
    double max_value;

    // Target path
    t_symbol* target_path;

    // Outlets
    void* bang_outlet;

#ifdef SUNNY_CORE_AVAILABLE
    // Lock-free SPSC queue for parameter updates from control thread
    Sunny::Infrastructure::SpscRingBuffer<ParamUpdate, 64>* param_queue;
#endif

} t_sunny_parameter;

// =============================================================================
// Function Prototypes
// =============================================================================

void* sunny_parameter_new(t_symbol* s, long argc, t_atom* argv);
void sunny_parameter_free(t_sunny_parameter* x);
void sunny_parameter_dsp64(t_sunny_parameter* x, t_object* dsp64, short* count,
                           double samplerate, long maxvectorsize, long flags);
void sunny_parameter_perform64(t_sunny_parameter* x, t_object* dsp64,
                               double** ins, long numins,
                               double** outs, long numouts,
                               long sampleframes, long flags, void* userparam);

void sunny_parameter_float(t_sunny_parameter* x, double f);
void sunny_parameter_int(t_sunny_parameter* x, long n);
void sunny_parameter_value(t_sunny_parameter* x, t_symbol* s, long argc, t_atom* argv);
void sunny_parameter_ramp(t_sunny_parameter* x, t_symbol* s, long argc, t_atom* argv);
void sunny_parameter_target(t_sunny_parameter* x, t_symbol* s);
void sunny_parameter_curve(t_sunny_parameter* x, t_symbol* s);
void sunny_parameter_bang(t_sunny_parameter* x);

void sunny_parameter_assist(t_sunny_parameter* x, void* b, long m, long a, char* s);

// =============================================================================
// Class Definition
// =============================================================================

static t_class* sunny_parameter_class = nullptr;

void ext_main(void* r) {
    t_class* c;

    c = class_new("sunny.parameter~",
                  (method)sunny_parameter_new,
                  (method)sunny_parameter_free,
                  sizeof(t_sunny_parameter),
                  (method)nullptr,
                  A_GIMME,
                  0);

    class_dspinit(c);

    // Methods
    class_addmethod(c, (method)sunny_parameter_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)sunny_parameter_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)sunny_parameter_int, "int", A_LONG, 0);
    class_addmethod(c, (method)sunny_parameter_value, "value", A_GIMME, 0);
    class_addmethod(c, (method)sunny_parameter_ramp, "ramp", A_GIMME, 0);
    class_addmethod(c, (method)sunny_parameter_target, "target", A_SYM, 0);
    class_addmethod(c, (method)sunny_parameter_curve, "curve", A_SYM, 0);
    class_addmethod(c, (method)sunny_parameter_bang, "bang", 0);
    class_addmethod(c, (method)sunny_parameter_assist, "assist", A_CANT, 0);

    // Attributes
    CLASS_ATTR_DOUBLE(c, "smooth", 0, t_sunny_parameter, smooth_ms);
    CLASS_ATTR_FILTER_CLIP(c, "smooth", 0.0, 10000.0);
    CLASS_ATTR_LABEL(c, "smooth", 0, "Smoothing Time (ms)");

    CLASS_ATTR_DOUBLE(c, "min", 0, t_sunny_parameter, min_value);
    CLASS_ATTR_LABEL(c, "min", 0, "Minimum Value");

    CLASS_ATTR_DOUBLE(c, "max", 0, t_sunny_parameter, max_value);
    CLASS_ATTR_LABEL(c, "max", 0, "Maximum Value");

    class_register(CLASS_BOX, c);
    sunny_parameter_class = c;
}

// =============================================================================
// Object Lifecycle
// =============================================================================

void* sunny_parameter_new(t_symbol* s, long argc, t_atom* argv) {
    t_sunny_parameter* x = (t_sunny_parameter*)object_alloc(sunny_parameter_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 1);  // 1 signal inlet

        // Create outlets (right to left)
        x->bang_outlet = outlet_new(x, nullptr);  // Bang outlet
        outlet_new(x, "signal");                   // Signal outlet

        // Initialize state
        x->current_value = 0.0;
        x->target_value = 0.0;
        x->ramp_increment = 0.0;
        x->ramp_samples_remaining = 0;

        // Default attributes
        x->smooth_ms = 10.0;
        x->smooth_coeff = 0.0;
        x->curve_type = gensym("linear");
        x->min_value = 0.0;
        x->max_value = 1.0;
        x->target_path = gensym("");

        // Process arguments/attributes
        attr_args_process(x, argc, argv);

#ifdef SUNNY_CORE_AVAILABLE
        x->param_queue = new Sunny::Infrastructure::SpscRingBuffer<ParamUpdate, 64>();
#endif
    }

    return x;
}

void sunny_parameter_free(t_sunny_parameter* x) {
    dsp_free((t_pxobject*)x);
#ifdef SUNNY_CORE_AVAILABLE
    delete x->param_queue;
#endif
}

// =============================================================================
// DSP
// =============================================================================

void sunny_parameter_dsp64(t_sunny_parameter* x, t_object* dsp64, short* count,
                           double samplerate, long maxvectorsize, long flags) {
    // Calculate smoothing coefficient
    if (x->smooth_ms > 0.0 && samplerate > 0.0) {
        double smooth_samples = x->smooth_ms * samplerate / 1000.0;
        x->smooth_coeff = exp(-1.0 / smooth_samples);
    } else {
        x->smooth_coeff = 0.0;
    }

    object_method(dsp64, gensym("dsp_add64"), x, sunny_parameter_perform64, 0, nullptr);
}

void sunny_parameter_perform64(t_sunny_parameter* x, t_object* dsp64,
                               double** ins, long numins,
                               double** outs, long numouts,
                               long sampleframes, long flags, void* userparam) {
    double* out = outs[0];
    double current = x->current_value;
    double target = x->target_value;
    double coeff = x->smooth_coeff;

#ifdef SUNNY_CORE_AVAILABLE
    // Drain lock-free queue (no allocation, no locks — audio-safe)
    ParamUpdate update;
    while (x->param_queue->try_pop(update)) {
        if (update.ramp_ms > 0.0f) {
            double samplerate = sys_getsr();
            if (samplerate <= 0.0) samplerate = 44100.0;
            long ramp_samples = static_cast<long>(update.ramp_ms * samplerate / 1000.0);
            if (ramp_samples < 1) ramp_samples = 1;
            x->target_value = static_cast<double>(update.value);
            x->ramp_increment = (x->target_value - current) / static_cast<double>(ramp_samples);
            x->ramp_samples_remaining = ramp_samples;
        } else {
            x->target_value = static_cast<double>(update.value);
            x->ramp_samples_remaining = 0;
        }
        target = x->target_value;
    }
#endif

    for (long i = 0; i < sampleframes; i++) {
        // Handle ramping
        if (x->ramp_samples_remaining > 0) {
            target += x->ramp_increment;
            x->ramp_samples_remaining--;

            if (x->ramp_samples_remaining == 0) {
                target = x->target_value;  // Ensure exact final value
            }
        }

        // Apply smoothing (one-pole lowpass)
        if (coeff > 0.0) {
            current = current * coeff + target * (1.0 - coeff);
        } else {
            current = target;
        }

        // Clamp to range
        if (current < x->min_value) current = x->min_value;
        if (current > x->max_value) current = x->max_value;

        out[i] = current;
    }

    x->current_value = current;
}

// =============================================================================
// Message Handlers
// =============================================================================

void sunny_parameter_float(t_sunny_parameter* x, double f) {
    x->target_value = f;
    x->ramp_samples_remaining = 0;
    outlet_bang(x->bang_outlet);
}

void sunny_parameter_int(t_sunny_parameter* x, long n) {
    sunny_parameter_float(x, (double)n);
}

void sunny_parameter_value(t_sunny_parameter* x, t_symbol* s, long argc, t_atom* argv) {
    if (argc >= 1 && (atom_gettype(argv) == A_FLOAT || atom_gettype(argv) == A_LONG)) {
        sunny_parameter_float(x, atom_getfloat(argv));
    }
}

void sunny_parameter_ramp(t_sunny_parameter* x, t_symbol* s, long argc, t_atom* argv) {
    if (argc >= 2) {
        double target = atom_getfloat(argv);
        double time_ms = atom_getfloat(argv + 1);

        if (time_ms <= 0.0) {
            sunny_parameter_float(x, target);
            return;
        }

        // Calculate ramp parameters
        double samplerate = sys_getsr();
        if (samplerate <= 0.0) samplerate = 44100.0;

        long ramp_samples = (long)(time_ms * samplerate / 1000.0);
        if (ramp_samples < 1) ramp_samples = 1;

        x->target_value = target;
        x->ramp_increment = (target - x->current_value) / (double)ramp_samples;
        x->ramp_samples_remaining = ramp_samples;
    }
}

void sunny_parameter_target(t_sunny_parameter* x, t_symbol* s) {
    x->target_path = s;
}

void sunny_parameter_curve(t_sunny_parameter* x, t_symbol* s) {
    x->curve_type = s;
}

void sunny_parameter_bang(t_sunny_parameter* x) {
    // Output current value immediately
    outlet_bang(x->bang_outlet);
}

// =============================================================================
// Assistance
// =============================================================================

void sunny_parameter_assist(t_sunny_parameter* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(signal) Pass-through / Trigger");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "(signal) Parameter Value");
                break;
            case 1:
                sprintf(s, "(bang) Parameter Changed");
                break;
        }
    }
}
