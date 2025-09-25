#ifndef HOMING_H
#define HOMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HOMING_IDLE = 0,
    HOMING_SEEK,
    HOMING_TRIGGERED,
    HOMING_BACKOFF,
    HOMING_SET_ZERO,
    HOMING_DONE,
    HOMING_FAIL
} homing_state_t;

int homing_start(int axis);
void homing_abort(void);
void homing_poll(void);
homing_state_t homing_get_state(int axis);
int homing_is_active(void);
uint32_t homing_get_homed_mask(void);
int32_t homing_get_offset(int axis);
/* Allow external persistence layer to seed offsets (used at boot). */
void homing_set_offset(int axis, int32_t offset);

#ifdef __cplusplus
}
#endif

#endif
