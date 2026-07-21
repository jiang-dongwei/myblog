#include <stddef.h>

#include "gameplay_gate.h"

gameplay_gate_action_t gameplay_gate_accept_fp(gameplay_gate_t *gate,
                                                bool gameplay_locked,
                                                bool payload_neutral)
{
    if (gate == NULL) {
        return GAMEPLAY_GATE_SUPPRESS;
    }

    if (gameplay_locked) {
        *gate = GAMEPLAY_GATE_LOCKED;
        return GAMEPLAY_GATE_SUPPRESS;
    }

    if (*gate == GAMEPLAY_GATE_LOCKED) {
        *gate = GAMEPLAY_GATE_DRAIN;
    }

    if (*gate == GAMEPLAY_GATE_DRAIN) {
        if (payload_neutral) {
            *gate = GAMEPLAY_GATE_UNLOCKED;
        }
        return GAMEPLAY_GATE_SUPPRESS;
    }

    if (*gate != GAMEPLAY_GATE_UNLOCKED) {
        /* Unknown/corrupt local state must fail neutral. */
        *gate = GAMEPLAY_GATE_DRAIN;
        return GAMEPLAY_GATE_SUPPRESS;
    }

    return GAMEPLAY_GATE_FORWARD;
}

void gameplay_gate_force_drain(gameplay_gate_t *gate)
{
    if (gate != NULL) {
        *gate = GAMEPLAY_GATE_DRAIN;
    }
}
