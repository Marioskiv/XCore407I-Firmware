/* Jog control module - per-axis ramping and telemetry */
#ifndef JOG_H
#define JOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void jog_init(void);
void jog_poll(void);

/* Configuration setters per axis (axis index 0..3) */
void jog_set_speed_axis(int axis, uint32_t steps_per_sec);
void jog_set_accel_axis(int axis, uint32_t steps_per_sec2);

/* Global helpers (apply to all axes) for backward compatibility */
void jog_set_speed(uint32_t steps_per_sec);
void jog_set_accel(uint32_t steps_per_sec2);

/* Telemetry accessors */
uint32_t jog_get_speed_axis(int axis);        /* Configured target speed */
uint32_t jog_get_accel_axis(int axis);        /* Configured accel */
uint32_t jog_get_current_speed_axis(int axis);/* Current ramped speed */
uint32_t jog_get_target_speed_axis(int axis); /* Target speed (alias to configured) */
uint32_t jog_get_direction_axis(int axis);    /* 0 idle, 1 positive, 2 negative */

/* Called by motion layer opcode handlers (legacy names) */
static inline void motion_set_jog_speed(uint32_t s){ jog_set_speed(s); }
static inline void motion_set_jog_accel(uint32_t a){ jog_set_accel(a); }

#ifdef __cplusplus
}
#endif

#endif /* JOG_H */