/**
 * @file sunny.euclidean_tilde.cpp
 * @brief Max/MSP external for Euclidean rhythm generation
 *
 * Component: MXEU001A
 * Domain: MX (Max) | Category: EU (Euclidean)
 *
 * sunny.euclidean~ generates Euclidean rhythm patterns as triggers,
 * synchronized to an external clock or transport.
 *
 * Inlets:
 *   1. Clock input (signal or bang)
 *   2. Pulses (int/signal)
 *   3. Steps (int/signal)
 *   4. Rotation (int/signal)
 *
 * Outlets:
 *   1. Trigger output (signal, 1.0 on hit, 0.0 otherwise)
 *   2. Step index (int, 0 to steps-1)
 *   3. Pattern as list
 *
 * Messages:
 *   - pulses <int>: Set number of pulses
 *   - steps <int>: Set number of steps
 *   - rotation <int>: Set pattern rotation
 *   - reset: Reset to step 0
 *
 * Attributes:
 *   - @pulses: Number of pulses (default: 3)
 *   - @steps: Number of steps (default: 8)
 *   - @rotation: Pattern rotation (default: 0)
 *   - @retrigger: Retrigger duration in samples (default: 1)
 */

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <vector>
#include <cmath>

// =============================================================================
// Object Structure
// =============================================================================

typedef struct _sunny_euclidean {
    t_pxobject ob;              // MSP object header

    // Pattern parameters
    long pulses;                 // Number of pulses
    long steps;                  // Number of steps
    long rotation;               // Pattern rotation

    // Pattern state
    std::vector<bool>* pattern;  // Current pattern
    long current_step;           // Current position in pattern
    bool pattern_dirty;          // Needs regeneration

    // Clock state
    double prev_clock;           // Previous clock sample
    long retrigger_samples;      // Retrigger duration
    long trigger_countdown;      // Samples until trigger ends

    // Outlets
    void* step_outlet;
    void* pattern_outlet;

} t_sunny_euclidean;

// =============================================================================
// Function Prototypes
// =============================================================================

void* sunny_euclidean_new(t_symbol* s, long argc, t_atom* argv);
void sunny_euclidean_free(t_sunny_euclidean* x);
void sunny_euclidean_dsp64(t_sunny_euclidean* x, t_object* dsp64, short* count,
                           double samplerate, long maxvectorsize, long flags);
void sunny_euclidean_perform64(t_sunny_euclidean* x, t_object* dsp64,
                               double** ins, long numins,
                               double** outs, long numouts,
                               long sampleframes, long flags, void* userparam);

void sunny_euclidean_pulses(t_sunny_euclidean* x, long n);
void sunny_euclidean_steps(t_sunny_euclidean* x, long n);
void sunny_euclidean_rotation(t_sunny_euclidean* x, long n);
void sunny_euclidean_reset(t_sunny_euclidean* x);
void sunny_euclidean_bang(t_sunny_euclidean* x);
void sunny_euclidean_regenerate(t_sunny_euclidean* x);
void sunny_euclidean_output_pattern(t_sunny_euclidean* x);

void sunny_euclidean_assist(t_sunny_euclidean* x, void* b, long m, long a, char* s);

// =============================================================================
// Euclidean Algorithm (Bresenham)
// =============================================================================

/**
 * Generate Euclidean rhythm using Bresenham's algorithm.
 * Integer arithmetic for exact results.
 */
static void generate_euclidean_pattern(std::vector<bool>& pattern, long pulses, long steps, long rotation) {
    pattern.resize(steps);

    if (steps <= 0) return;

    // Clamp pulses
    if (pulses < 0) pulses = 0;
    if (pulses > steps) pulses = steps;

    // Edge cases
    if (pulses == 0) {
        for (long i = 0; i < steps; i++) pattern[i] = false;
        return;
    }
    if (pulses == steps) {
        for (long i = 0; i < steps; i++) pattern[i] = true;
        return;
    }

    // Bresenham's algorithm
    for (long i = 0; i < steps; i++) {
        long threshold = (i + 1) * pulses;
        long prev_threshold = i * pulses;
        pattern[i] = (threshold / steps) > (prev_threshold / steps);
    }

    // Apply rotation
    if (rotation != 0) {
        rotation = ((rotation % steps) + steps) % steps;
        std::vector<bool> rotated(steps);
        for (long i = 0; i < steps; i++) {
            rotated[i] = pattern[(i + rotation) % steps];
        }
        pattern = rotated;
    }
}

// =============================================================================
// Class Definition
// =============================================================================

static t_class* sunny_euclidean_class = nullptr;

void ext_main(void* r) {
    t_class* c;

    c = class_new("sunny.euclidean~",
                  (method)sunny_euclidean_new,
                  (method)sunny_euclidean_free,
                  sizeof(t_sunny_euclidean),
                  (method)nullptr,
                  A_GIMME,
                  0);

    class_dspinit(c);

    // Methods
    class_addmethod(c, (method)sunny_euclidean_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)sunny_euclidean_pulses, "pulses", A_LONG, 0);
    class_addmethod(c, (method)sunny_euclidean_steps, "steps", A_LONG, 0);
    class_addmethod(c, (method)sunny_euclidean_rotation, "rotation", A_LONG, 0);
    class_addmethod(c, (method)sunny_euclidean_reset, "reset", 0);
    class_addmethod(c, (method)sunny_euclidean_bang, "bang", 0);
    class_addmethod(c, (method)sunny_euclidean_assist, "assist", A_CANT, 0);

    // Attributes
    CLASS_ATTR_LONG(c, "pulses", 0, t_sunny_euclidean, pulses);
    CLASS_ATTR_FILTER_CLIP(c, "pulses", 0, 64);
    CLASS_ATTR_LABEL(c, "pulses", 0, "Number of Pulses");

    CLASS_ATTR_LONG(c, "steps", 0, t_sunny_euclidean, steps);
    CLASS_ATTR_FILTER_CLIP(c, "steps", 1, 64);
    CLASS_ATTR_LABEL(c, "steps", 0, "Number of Steps");

    CLASS_ATTR_LONG(c, "rotation", 0, t_sunny_euclidean, rotation);
    CLASS_ATTR_LABEL(c, "rotation", 0, "Pattern Rotation");

    CLASS_ATTR_LONG(c, "retrigger", 0, t_sunny_euclidean, retrigger_samples);
    CLASS_ATTR_FILTER_CLIP(c, "retrigger", 1, 4410);
    CLASS_ATTR_LABEL(c, "retrigger", 0, "Retrigger Duration (samples)");

    class_register(CLASS_BOX, c);
    sunny_euclidean_class = c;
}

// =============================================================================
// Object Lifecycle
// =============================================================================

void* sunny_euclidean_new(t_symbol* s, long argc, t_atom* argv) {
    t_sunny_euclidean* x = (t_sunny_euclidean*)object_alloc(sunny_euclidean_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 4);  // 4 signal inlets (clock, pulses, steps, rotation)

        // Create outlets (right to left)
        x->pattern_outlet = outlet_new(x, nullptr);  // Pattern list
        x->step_outlet = intout(x);                   // Step index
        outlet_new(x, "signal");                       // Trigger signal

        // Initialize pattern
        x->pattern = new std::vector<bool>();
        x->pulses = 3;
        x->steps = 8;
        x->rotation = 0;
        x->current_step = 0;
        x->pattern_dirty = true;

        // Clock state
        x->prev_clock = 0.0;
        x->retrigger_samples = 1;
        x->trigger_countdown = 0;

        // Process arguments/attributes
        attr_args_process(x, argc, argv);

        // Generate initial pattern
        sunny_euclidean_regenerate(x);
    }

    return x;
}

void sunny_euclidean_free(t_sunny_euclidean* x) {
    dsp_free((t_pxobject*)x);
    delete x->pattern;
}

// =============================================================================
// DSP
// =============================================================================

void sunny_euclidean_dsp64(t_sunny_euclidean* x, t_object* dsp64, short* count,
                           double samplerate, long maxvectorsize, long flags) {
    object_method(dsp64, gensym("dsp_add64"), x, sunny_euclidean_perform64, 0, nullptr);
}

void sunny_euclidean_perform64(t_sunny_euclidean* x, t_object* dsp64,
                               double** ins, long numins,
                               double** outs, long numouts,
                               long sampleframes, long flags, void* userparam) {
    double* clock_in = ins[0];
    double* out = outs[0];

    // Regenerate pattern if dirty
    if (x->pattern_dirty) {
        generate_euclidean_pattern(*x->pattern, x->pulses, x->steps, x->rotation);
        x->pattern_dirty = false;
    }

    std::vector<bool>& pattern = *x->pattern;
    long steps = (long)pattern.size();
    if (steps == 0) steps = 1;

    for (long i = 0; i < sampleframes; i++) {
        double clock = clock_in[i];

        // Detect rising edge (clock transition from <= 0 to > 0)
        bool trigger_now = false;
        if (clock > 0.0 && x->prev_clock <= 0.0) {
            // Advance step
            x->current_step = (x->current_step + 1) % steps;

            // Check if this step is a hit
            if (pattern[x->current_step]) {
                trigger_now = true;
                x->trigger_countdown = x->retrigger_samples;

                // Output step index (deferred)
                // Note: outlet_int should not be called from audio thread
                // In production, use a queue/clock for thread safety
            }
        }
        x->prev_clock = clock;

        // Output trigger signal
        if (x->trigger_countdown > 0) {
            out[i] = 1.0;
            x->trigger_countdown--;
        } else {
            out[i] = 0.0;
        }
    }
}

// =============================================================================
// Message Handlers
// =============================================================================

void sunny_euclidean_pulses(t_sunny_euclidean* x, long n) {
    if (n < 0) n = 0;
    if (n > x->steps) n = x->steps;
    x->pulses = n;
    x->pattern_dirty = true;
    sunny_euclidean_output_pattern(x);
}

void sunny_euclidean_steps(t_sunny_euclidean* x, long n) {
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    x->steps = n;
    if (x->pulses > n) x->pulses = n;
    x->current_step = x->current_step % n;
    x->pattern_dirty = true;
    sunny_euclidean_output_pattern(x);
}

void sunny_euclidean_rotation(t_sunny_euclidean* x, long n) {
    x->rotation = n;
    x->pattern_dirty = true;
    sunny_euclidean_output_pattern(x);
}

void sunny_euclidean_reset(t_sunny_euclidean* x) {
    x->current_step = x->steps - 1;  // Will become 0 on next clock
    x->trigger_countdown = 0;
}

void sunny_euclidean_bang(t_sunny_euclidean* x) {
    // Manually trigger next step
    long steps = (long)x->pattern->size();
    if (steps == 0) return;

    x->current_step = (x->current_step + 1) % steps;

    if ((*x->pattern)[x->current_step]) {
        outlet_int(x->step_outlet, x->current_step);
    }
}

void sunny_euclidean_regenerate(t_sunny_euclidean* x) {
    generate_euclidean_pattern(*x->pattern, x->pulses, x->steps, x->rotation);
    x->pattern_dirty = false;
    sunny_euclidean_output_pattern(x);
}

void sunny_euclidean_output_pattern(t_sunny_euclidean* x) {
    std::vector<bool>& pattern = *x->pattern;
    long n = (long)pattern.size();

    t_atom* atoms = new t_atom[n];
    for (long i = 0; i < n; i++) {
        atom_setlong(atoms + i, pattern[i] ? 1 : 0);
    }

    outlet_list(x->pattern_outlet, nullptr, n, atoms);
    delete[] atoms;
}

// =============================================================================
// Assistance
// =============================================================================

void sunny_euclidean_assist(t_sunny_euclidean* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0: sprintf(s, "(signal) Clock Input"); break;
            case 1: sprintf(s, "(signal/int) Pulses"); break;
            case 2: sprintf(s, "(signal/int) Steps"); break;
            case 3: sprintf(s, "(signal/int) Rotation"); break;
        }
    } else {
        switch (a) {
            case 0: sprintf(s, "(signal) Trigger Output"); break;
            case 1: sprintf(s, "(int) Current Step"); break;
            case 2: sprintf(s, "(list) Pattern"); break;
        }
    }
}
