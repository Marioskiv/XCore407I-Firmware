#include "homing.h"
#include "encoder.h"
#include "stepgen.h"
#include "board_xcore407i.h"
#include "config_store.h"

#ifndef JOINTS
#define JOINTS 4
#endif

static const int8_t homing_dir[JOINTS] = { -1, -1, -1, -1 };
static const int32_t homing_seek_speed[JOINTS] = { 2000, 2000, 2000, 1000 };
static const int32_t homing_backoff_steps[JOINTS] = { 400, 400, 400, 200 };
static const uint32_t homing_trigger_debounce = 3;

typedef struct {
    homing_state_t state;
    int8_t dir;
    uint32_t trigger_count;
    int32_t trigger_position;
    int32_t backoff_start;
    int32_t offset;
} homing_axis_t;

static homing_axis_t axes[JOINTS];
static uint32_t homed_mask = 0;

extern uint32_t motion_get_probe(void);
extern uint32_t motion_get_estop(void);
extern uint32_t motion_get_fault_mask(void);

static void apply_motion(int axis, int32_t speed) {
    stepgen_apply_command(axis, speed, speed!=0);
}

int homing_start(int axis) {
    if (axis < 0 || axis >= JOINTS) return -1;
    if (motion_get_estop() || motion_get_fault_mask()) return -1;
    if (homing_is_active()) return -1;
    homing_axis_t *a = &axes[axis];
    a->state = HOMING_SEEK;
    a->dir = homing_dir[axis];
    a->trigger_count = 0;
    return 0;
}

void homing_abort(void) {
    for (int i=0;i<JOINTS;i++) {
        if (axes[i].state != HOMING_IDLE && axes[i].state != HOMING_DONE && axes[i].state != HOMING_FAIL) {
            axes[i].state = HOMING_IDLE;
            stepgen_apply_command(i, 0, 0);
        }
    }
}

int homing_is_active(void) {
    for (int i=0;i<JOINTS;i++) {
        switch(axes[i].state) {
            case HOMING_SEEK:
            case HOMING_TRIGGERED:
            case HOMING_BACKOFF:
            case HOMING_SET_ZERO:
                return 1;
            default: break;
        }
    }
    return 0;
}

homing_state_t homing_get_state(int axis) {
    if (axis < 0 || axis >= JOINTS) return HOMING_FAIL;
    return axes[axis].state;
}

uint32_t homing_get_homed_mask(void) { return homed_mask; }
int32_t homing_get_offset(int axis) { if(axis<0||axis>=JOINTS) return 0; return axes[axis].offset; }
void homing_set_offset(int axis, int32_t off){ if(axis<0||axis>=JOINTS) return; axes[axis].offset = off; homed_mask |= (1u<<axis); axes[axis].state = HOMING_DONE; }

void homing_poll(void) {
    if (motion_get_estop() || motion_get_fault_mask()) {
        homing_abort();
        return;
    }
    uint32_t probe = motion_get_probe();
    for (int axis=0; axis<JOINTS; ++axis) {
        homing_axis_t *a = &axes[axis];
        switch(a->state) {
            case HOMING_IDLE:
            case HOMING_DONE:
            case HOMING_FAIL:
                break;
            case HOMING_SEEK:
                apply_motion(axis, a->dir * homing_seek_speed[axis]);
                if (probe == 0) {
                    if (++a->trigger_count >= homing_trigger_debounce) {
                        a->trigger_position = encoder_get_count(axis);
                        stepgen_apply_command(axis, 0, 0);
                        a->state = HOMING_TRIGGERED;
                    }
                } else {
                    a->trigger_count = 0;
                }
                break;
            case HOMING_TRIGGERED:
                a->backoff_start = encoder_get_count(axis);
                a->state = HOMING_BACKOFF;
                break;
            case HOMING_BACKOFF: {
                int32_t pos = encoder_get_count(axis);
                int32_t delta = pos - a->backoff_start;
                apply_motion(axis, -a->dir * (homing_seek_speed[axis]/3));
                int32_t traveled = (a->dir < 0) ? -delta : delta;
                if (traveled >= homing_backoff_steps[axis]) {
                    stepgen_apply_command(axis, 0, 0);
                    a->state = HOMING_SET_ZERO;
                }
            } break;
            case HOMING_SET_ZERO: {
                int32_t now = encoder_get_count(axis);
                a->offset = now;
                homed_mask |= (1u << axis);
                a->state = HOMING_DONE;
                /* Persist offset */
                config_store_t *c = config_store_get();
                if (axis < CONFIG_MAX_AXES) {
                    c->homing_offsets[axis] = a->offset;
                    config_store_mark_dirty();
                }
            } break;
        }
    }
}
