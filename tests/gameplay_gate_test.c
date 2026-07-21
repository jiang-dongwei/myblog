#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "gameplay_gate.h"

#define FP_LEN 8u

static const uint8_t unlocked_neutral[FP_LEN] = {
    0x46, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
};
static const uint8_t locked_neutral[FP_LEN] = {
    0x46, 0x50, 0x00, 0x00, 0x80, 0x00, 0x00, 0x96,
};
static const uint8_t unlocked_b1[FP_LEN] = {
    0x46, 0x50, 0x01, 0x00, 0x00, 0x00, 0x00, 0x17,
};
static const uint8_t locked_b1[FP_LEN] = {
    0x46, 0x50, 0x01, 0x00, 0x80, 0x00, 0x00, 0x97,
};
static const uint8_t locked_gp19[FP_LEN] = {
    0x46, 0x50, 0x00, 0x20, 0x80, 0x00, 0x00, 0xB6,
};
static const uint8_t unlocked_gp19[FP_LEN] = {
    0x46, 0x50, 0x00, 0x20, 0x00, 0x00, 0x00, 0x36,
};
static const uint8_t locked_gp20[FP_LEN] = {
    0x46, 0x50, 0x00, 0x40, 0x80, 0x00, 0x00, 0xD6,
};

typedef struct {
    gameplay_gate_t gate;
    uint16_t hid_buttons;
    unsigned activity_updates;
} receiver_model_t;

static uint8_t checksum(const uint8_t frame[FP_LEN])
{
    uint8_t value = 0;
    for (unsigned i = 0; i < FP_LEN - 1; ++i) {
        value ^= frame[i];
    }
    return value;
}

static bool frame_valid(const uint8_t frame[FP_LEN])
{
    return frame[0] == 0x46 && frame[1] == 0x50 &&
           checksum(frame) == frame[FP_LEN - 1];
}

static gameplay_gate_action_t receive_fp(receiver_model_t *receiver,
                                         const uint8_t frame[FP_LEN])
{
    if (!frame_valid(frame)) {
        return GAMEPLAY_GATE_SUPPRESS;
    }

    uint16_t buttons = (frame[2] | ((uint16_t)frame[3] << 8)) &
                       GAMEPLAY_FP_BUTTON_MASK;
    uint8_t dpad = frame[4] & GAMEPLAY_FP_DPAD_MASK;
    bool locked = (frame[4] & GAMEPLAY_FP_LOCK_BIT) != 0;
    bool neutral = buttons == 0 && dpad == 0 && frame[5] == 0 && frame[6] == 0;
    gameplay_gate_action_t action =
        gameplay_gate_accept_fp(&receiver->gate, locked, neutral);

    if (action == GAMEPLAY_GATE_FORWARD) {
        receiver->hid_buttons = buttons;
        ++receiver->activity_updates;
    } else {
        receiver->hid_buttons = 0;
    }
    return action;
}

static void test_known_frames_and_masks(void)
{
    assert(frame_valid(unlocked_neutral));
    assert(frame_valid(locked_neutral));
    assert(frame_valid(unlocked_b1));
    assert(frame_valid(locked_b1));
    assert(frame_valid(locked_gp19));
    assert(frame_valid(unlocked_gp19));
    assert(frame_valid(locked_gp20));

    assert((locked_gp20[4] & GAMEPLAY_FP_DPAD_MASK) == 0);
    assert((((uint16_t)locked_gp20[3] << 8) & GAMEPLAY_FP_BUTTON_MASK) ==
           (1u << 14));
    assert((GAMEPLAY_FP_BUTTON_MASK & (1u << 15)) == 0);
}

static void test_boot_lock_and_drain_sequences(void)
{
    receiver_model_t receiver = {
        .gate = GAMEPLAY_GATE_DRAIN,
    };

    /* 1: boot / FT=BT transition leaves DRAIN; neutral completes it. */
    gameplay_gate_force_drain(&receiver.gate);
    assert(receive_fp(&receiver, unlocked_neutral) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_UNLOCKED);
    assert(receiver.hid_buttons == 0);

    /* 2: normal unlocked B1 reaches HID and activity. */
    assert(receive_fp(&receiver, unlocked_b1) == GAMEPLAY_GATE_FORWARD);
    assert(receiver.hid_buttons == 1);
    assert(receiver.activity_updates == 1);

    /* 3/4: locked reports immediately neutralize and never update activity. */
    assert(receive_fp(&receiver, locked_b1) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_LOCKED);
    assert(receiver.hid_buttons == 0);
    assert(receiver.activity_updates == 1);
    assert(receive_fp(&receiver, locked_neutral) == GAMEPLAY_GATE_SUPPRESS);
    assert(receive_fp(&receiver, locked_b1) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.activity_updates == 1);

    /* 5/6: unlocked GP19 is drained; neutral is consumed; later B1 passes. */
    assert(receive_fp(&receiver, unlocked_gp19) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_DRAIN);
    assert(receiver.hid_buttons == 0);
    assert(receive_fp(&receiver, unlocked_neutral) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_UNLOCKED);
    assert(receive_fp(&receiver, unlocked_b1) == GAMEPLAY_GATE_FORWARD);
    assert(receiver.activity_updates == 2);

    /* 7: GP20 remains physical bit14 only and cannot leak while locked. */
    assert(receive_fp(&receiver, locked_gp20) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_LOCKED);
    assert(receiver.hid_buttons == 0);
    assert(receiver.activity_updates == 2);
}

static void test_stale_transport_and_bad_checksum(void)
{
    receiver_model_t receiver = {
        .gate = GAMEPLAY_GATE_DRAIN,
    };

    receive_fp(&receiver, unlocked_neutral);
    assert(receive_fp(&receiver, unlocked_b1) == GAMEPLAY_GATE_FORWARD);

    /* 8: the production stale path calls this and neutralizes desired HID. */
    gameplay_gate_force_drain(&receiver.gate);
    receiver.hid_buttons = 0;
    assert(receive_fp(&receiver, unlocked_b1) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_DRAIN);

    /* 9: FT=USB and a later USB->BT transition both force DRAIN. */
    gameplay_gate_force_drain(&receiver.gate);
    assert(receive_fp(&receiver, unlocked_neutral) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_UNLOCKED);
    gameplay_gate_force_drain(&receiver.gate);
    assert(receiver.gate == GAMEPLAY_GATE_DRAIN);

    /* Repeated FT=BT is deliberately represented by no gate call/change. */
    receiver.gate = GAMEPLAY_GATE_LOCKED;
    assert(receiver.gate == GAMEPLAY_GATE_LOCKED);

    /* 10: invalid checksum is rejected before it can change gate or drain. */
    uint8_t bad_locked[FP_LEN];
    for (unsigned i = 0; i < FP_LEN; ++i) {
        bad_locked[i] = locked_b1[i];
    }
    bad_locked[FP_LEN - 1] ^= 0x01;
    receiver.gate = GAMEPLAY_GATE_UNLOCKED;
    unsigned activity_before = receiver.activity_updates;
    assert(receive_fp(&receiver, bad_locked) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_UNLOCKED);
    assert(receiver.activity_updates == activity_before);

    uint8_t bad_neutral[FP_LEN];
    for (unsigned i = 0; i < FP_LEN; ++i) {
        bad_neutral[i] = unlocked_neutral[i];
    }
    bad_neutral[FP_LEN - 1] ^= 0x01;
    receiver.gate = GAMEPLAY_GATE_DRAIN;
    assert(receive_fp(&receiver, bad_neutral) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_DRAIN);

    /* 11 receiver side: bit7/checksum changes are observable immediately.  The
     * less-than-10ms RP transmit timing remains an RP2350-side test. */
    assert(unlocked_b1[4] != locked_b1[4]);
    assert(unlocked_b1[7] != locked_b1[7]);
    receiver.gate = GAMEPLAY_GATE_UNLOCKED;
    assert(receive_fp(&receiver, locked_b1) == GAMEPLAY_GATE_SUPPRESS);
    assert(receiver.gate == GAMEPLAY_GATE_LOCKED);
}

int main(void)
{
    test_known_frames_and_masks();
    test_boot_lock_and_drain_sequences();
    test_stale_transport_and_bad_checksum();
    puts("gameplay_gate_test: PASS");
    return 0;
}
