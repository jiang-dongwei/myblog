#ifndef GAMEPLAY_GATE_H
#define GAMEPLAY_GATE_H

#include <stdbool.h>
#include <stdint.h>

#define GAMEPLAY_FP_LOCK_BIT 0x80u
#define GAMEPLAY_FP_DPAD_MASK 0x0Fu
#define GAMEPLAY_FP_BUTTON_MASK 0x7FFFu

typedef enum {
    GAMEPLAY_GATE_UNLOCKED = 0,
    GAMEPLAY_GATE_LOCKED,
    GAMEPLAY_GATE_DRAIN,
} gameplay_gate_t;

typedef enum {
    GAMEPLAY_GATE_SUPPRESS = 0,
    GAMEPLAY_GATE_FORWARD,
} gameplay_gate_action_t;

/* Apply one already-validated FP report to the receiver gate.  A neutral FP
 * that completes DRAIN is consumed; only a later report can be forwarded. */
gameplay_gate_action_t gameplay_gate_accept_fp(gameplay_gate_t *gate,
                                                bool gameplay_locked,
                                                bool payload_neutral);

/* Transport loss/change and UART staleness all fail neutral through DRAIN. */
void gameplay_gate_force_drain(gameplay_gate_t *gate);

#endif
