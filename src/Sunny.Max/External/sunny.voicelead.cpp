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

#include "Algorithm/VoiceLeadStandalone.h"

#ifdef SUNNY_CORE_AVAILABLE
#include "Tensor/TNTP001A.h"
#include "VoiceLeading/VLNT001A.h"
#endif

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

// Delegate to extracted algorithm header
static long nearest_pitch(long source, long target_pc) {
    return Sunny::Max::Algorithm::nearest_pitch(source, target_pc);
}

static long compute_voice_leading(
    const std::vector<long>& source,
    const std::vector<long>& target_pcs,
    std::vector<long>& result,
    bool lock_bass,
    long max_jump
) {
    return Sunny::Max::Algorithm::compute_voice_leading(
        source, target_pcs, result, lock_bass, max_jump);
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

    long motion = 0;

#ifdef SUNNY_CORE_AVAILABLE
    // Use Sunny::Core voice leading (production path)
    {
        std::vector<Sunny::Core::MidiNote> source;
        source.reserve(x->source_chord->size());
        for (auto n : *x->source_chord) {
            source.push_back(static_cast<Sunny::Core::MidiNote>(n));
        }

        std::vector<Sunny::Core::PitchClass> target;
        target.reserve(x->target_pcs->size());
        for (auto pc : *x->target_pcs) {
            target.push_back(static_cast<Sunny::Core::PitchClass>(pc));
        }

        auto result = Sunny::Core::voice_lead_nearest_tone(
            source, target, x->lock_bass != 0);

        if (result) {
            x->result_chord->clear();
            for (auto n : result->voiced_notes) {
                x->result_chord->push_back(static_cast<long>(n));
            }
            motion = result->total_motion;
        } else {
            // Fallback to standalone algorithm on error
            motion = compute_voice_leading(
                *x->source_chord, *x->target_pcs, *x->result_chord,
                x->lock_bass != 0, x->max_jump);
        }
    }
#else
    // Standalone voice leading (no Sunny::Core)
    motion = compute_voice_leading(
        *x->source_chord,
        *x->target_pcs,
        *x->result_chord,
        x->lock_bass != 0,
        x->max_jump
    );
#endif

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
