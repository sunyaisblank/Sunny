/**
 * @file sunny.voicelead.cpp
 * @brief Max object for voice leading computation
 *
 * Component: MXVL001A
 * Domain: MX (Max) | Category: VL (Voice Leading)
 *
 * sunny.voicelead computes optimal voice leading between chords
 * using the nearest-tone algorithm.
 *
 * Inlets:
 *   1. Source chord (list of MIDI notes)
 *   2. Target chord (list of pitch classes 0-11)
 *
 * Outlets:
 *   1. Result chord (list of MIDI notes)
 *   2. Total motion (int, sum of voice movements)
 *
 * Messages:
 *   - list: Set source chord and trigger computation
 *   - target <list>: Set target pitch classes
 *   - lock_bass <0|1>: Lock bass to chord root
 *
 * Attributes:
 *   - @lock_bass: Lock bass voice to root (default: 1)
 *   - @max_jump: Maximum voice jump in semitones (default: 12)
 */

#include "ext.h"
#include "ext_obex.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

// =============================================================================
// Object Structure
// =============================================================================

typedef struct _sunny_voicelead {
    t_object ob;                // Max object header

    // Chord state
    std::vector<long>* source_chord;
    std::vector<long>* target_pcs;
    std::vector<long>* result_chord;

    // Parameters
    long lock_bass;
    long max_jump;

    // Outlets
    void* motion_outlet;

} t_sunny_voicelead;

// =============================================================================
// Voice Leading Algorithm
// =============================================================================

/**
 * Find the nearest MIDI pitch to source with the given pitch class.
 */
static long nearest_pitch(long source, long target_pc) {
    long source_pc = source % 12;
    long diff = target_pc - source_pc;

    // Normalize to [-6, 5] for minimal motion
    if (diff > 6) diff -= 12;
    if (diff < -6) diff += 12;

    long result = source + diff;

    // Clamp to valid MIDI range
    if (result < 0) result = target_pc;
    if (result > 127) result = 127 - (12 - target_pc);

    return result;
}

/**
 * Compute optimal voice leading.
 * Returns total motion.
 */
static long compute_voice_leading(
    const std::vector<long>& source,
    const std::vector<long>& target_pcs,
    std::vector<long>& result,
    bool lock_bass,
    long max_jump
) {
    result.clear();
    if (source.empty() || target_pcs.empty()) return 0;

    // Extend target PCs if needed
    std::vector<long> targets = target_pcs;
    while (targets.size() < source.size()) {
        targets.push_back(target_pcs[targets.size() % target_pcs.size()]);
    }

    std::vector<bool> used(targets.size(), false);
    long total_motion = 0;

    for (size_t i = 0; i < source.size(); i++) {
        long src = source[i];
        long best_pitch = src;
        long best_distance = std::numeric_limits<long>::max();
        size_t best_idx = 0;

        if (lock_bass && i == 0) {
            // Bass takes root
            best_pitch = nearest_pitch(src, targets[0]);
            used[0] = true;
        } else {
            // Find closest available target
            for (size_t j = 0; j < targets.size(); j++) {
                if (used[j] && used.size() > source.size()) continue;

                long candidate = nearest_pitch(src, targets[j]);
                long distance = std::abs(candidate - src);

                // Prefer unused targets
                if (!used[j] && distance < best_distance) {
                    best_distance = distance;
                    best_pitch = candidate;
                    best_idx = j;
                } else if (used[j] && distance < best_distance - 2) {
                    // Reuse only if significantly closer
                    best_distance = distance;
                    best_pitch = candidate;
                    best_idx = j;
                }
            }
            used[best_idx] = true;
        }

        // Apply max jump constraint
        long motion = std::abs(best_pitch - src);
        if (max_jump > 0 && motion > max_jump) {
            if (best_pitch > src) {
                best_pitch = src + max_jump;
            } else {
                best_pitch = src - max_jump;
            }
            // Re-align to target pitch class
            long target_pc = targets[best_idx];
            long diff = target_pc - (best_pitch % 12);
            if (diff > 6) diff -= 12;
            if (diff < -6) diff += 12;
            best_pitch += diff;
            best_pitch = std::clamp(best_pitch, 0L, 127L);
        }

        result.push_back(best_pitch);
        total_motion += std::abs(best_pitch - src);
    }

    // Fix voice crossings
    for (size_t i = 1; i < result.size(); i++) {
        if (result[i] <= result[i - 1]) {
            while (result[i] <= result[i - 1] && result[i] + 12 <= 127) {
                result[i] += 12;
            }
        }
    }

    return total_motion;
}

// =============================================================================
// Function Prototypes
// =============================================================================

void* sunny_voicelead_new(t_symbol* s, long argc, t_atom* argv);
void sunny_voicelead_free(t_sunny_voicelead* x);
void sunny_voicelead_list(t_sunny_voicelead* x, t_symbol* s, long argc, t_atom* argv);
void sunny_voicelead_target(t_sunny_voicelead* x, t_symbol* s, long argc, t_atom* argv);
void sunny_voicelead_bang(t_sunny_voicelead* x);
void sunny_voicelead_assist(t_sunny_voicelead* x, void* b, long m, long a, char* s);

// =============================================================================
// Class Definition
// =============================================================================

static t_class* sunny_voicelead_class = nullptr;

void ext_main(void* r) {
    t_class* c;

    c = class_new("sunny.voicelead",
                  (method)sunny_voicelead_new,
                  (method)sunny_voicelead_free,
                  sizeof(t_sunny_voicelead),
                  (method)nullptr,
                  A_GIMME,
                  0);

    // Methods
    class_addmethod(c, (method)sunny_voicelead_list, "list", A_GIMME, 0);
    class_addmethod(c, (method)sunny_voicelead_target, "target", A_GIMME, 0);
    class_addmethod(c, (method)sunny_voicelead_bang, "bang", 0);
    class_addmethod(c, (method)sunny_voicelead_assist, "assist", A_CANT, 0);

    // Attributes
    CLASS_ATTR_LONG(c, "lock_bass", 0, t_sunny_voicelead, lock_bass);
    CLASS_ATTR_STYLE(c, "lock_bass", 0, "onoff");
    CLASS_ATTR_LABEL(c, "lock_bass", 0, "Lock Bass to Root");

    CLASS_ATTR_LONG(c, "max_jump", 0, t_sunny_voicelead, max_jump);
    CLASS_ATTR_FILTER_CLIP(c, "max_jump", 0, 24);
    CLASS_ATTR_LABEL(c, "max_jump", 0, "Maximum Voice Jump");

    class_register(CLASS_BOX, c);
    sunny_voicelead_class = c;
}

// =============================================================================
// Object Lifecycle
// =============================================================================

void* sunny_voicelead_new(t_symbol* s, long argc, t_atom* argv) {
    t_sunny_voicelead* x = (t_sunny_voicelead*)object_alloc(sunny_voicelead_class);

    if (x) {
        // Create outlets (right to left)
        x->motion_outlet = intout(x);
        outlet_new(x, nullptr);  // Result list

        // Create inlets (for target)
        inlet_new(x, nullptr);

        // Initialize state
        x->source_chord = new std::vector<long>();
        x->target_pcs = new std::vector<long>();
        x->result_chord = new std::vector<long>();

        // Default parameters
        x->lock_bass = 1;
        x->max_jump = 12;

        // Default target: C major triad
        x->target_pcs->push_back(0);  // C
        x->target_pcs->push_back(4);  // E
        x->target_pcs->push_back(7);  // G

        // Process attributes
        attr_args_process(x, argc, argv);
    }

    return x;
}

void sunny_voicelead_free(t_sunny_voicelead* x) {
    delete x->source_chord;
    delete x->target_pcs;
    delete x->result_chord;
}

// =============================================================================
// Message Handlers
// =============================================================================

void sunny_voicelead_list(t_sunny_voicelead* x, t_symbol* s, long argc, t_atom* argv) {
    // Set source chord
    x->source_chord->clear();
    for (long i = 0; i < argc; i++) {
        long note = atom_getlong(argv + i);
        if (note >= 0 && note <= 127) {
            x->source_chord->push_back(note);
        }
    }

    // Compute voice leading
    sunny_voicelead_bang(x);
}

void sunny_voicelead_target(t_sunny_voicelead* x, t_symbol* s, long argc, t_atom* argv) {
    // Set target pitch classes
    x->target_pcs->clear();
    for (long i = 0; i < argc; i++) {
        long pc = atom_getlong(argv + i) % 12;
        if (pc < 0) pc += 12;
        x->target_pcs->push_back(pc);
    }
}

void sunny_voicelead_bang(t_sunny_voicelead* x) {
    if (x->source_chord->empty() || x->target_pcs->empty()) {
        return;
    }

    // Compute voice leading
    long motion = compute_voice_leading(
        *x->source_chord,
        *x->target_pcs,
        *x->result_chord,
        x->lock_bass != 0,
        x->max_jump
    );

    // Output motion
    outlet_int(x->motion_outlet, motion);

    // Output result chord
    long n = (long)x->result_chord->size();
    t_atom* atoms = new t_atom[n];
    for (long i = 0; i < n; i++) {
        atom_setlong(atoms + i, (*x->result_chord)[i]);
    }
    outlet_list(outlet_nth((t_object*)x, 0), nullptr, n, atoms);
    delete[] atoms;
}

// =============================================================================
// Assistance
// =============================================================================

void sunny_voicelead_assist(t_sunny_voicelead* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0: sprintf(s, "(list) Source Chord (MIDI notes)"); break;
            case 1: sprintf(s, "target (list) Target Pitch Classes (0-11)"); break;
        }
    } else {
        switch (a) {
            case 0: sprintf(s, "(list) Result Chord (MIDI notes)"); break;
            case 1: sprintf(s, "(int) Total Voice Motion"); break;
        }
    }
}
