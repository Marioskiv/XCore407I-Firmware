#ifndef XCORE407I_QEI_DRIVER_H
#define XCORE407I_QEI_DRIVER_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight QEI abstraction (optional). Wraps a TIM in encoder mode. */

typedef struct {
    TIM_TypeDef *instance;
    uint32_t last_raw;
    int32_t accumulated; /* extended position (counts) */
    int32_t velocity_cps; /* counts per second (simple discrete derivative) */
    uint32_t last_ts_ms;  /* last velocity sample timestamp (ms) */
    int wraps;            /* number of raw counter wrap events (debug) */
} qei_t;

int qei_init(qei_t *qei, TIM_TypeDef *tim, uint32_t period, int32_t start_position, int sample_now_ms);
int32_t qei_get_position(qei_t *qei);
void qei_reset(qei_t *qei, int32_t to_zero);
/* Update velocity using current time (ms); returns counts/sec */
int32_t qei_update_velocity(qei_t *qei, int now_ms);
int32_t qei_get_velocity(const qei_t *qei);

#ifdef __cplusplus
}
#endif

#endif /* XCORE407I_QEI_DRIVER_H */
