#ifdef USE_ABSTRACT_DRIVERS
#include "drivers/qei/qeiDriver.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* Simple diagnostic helper (optional): call qei_diag_poll() periodically
 * after encoder_init() to log rollover behavior. Not compiled when
 * USE_ABSTRACT_DRIVERS=OFF. You can wire this into an existing debug loop.
 */
extern TIM_HandleTypeDef htim3; // if needed expose or adapt

static uint32_t lastPrintMs = 0;
static int calls = 0;

void qei_diag_poll(uint32_t nowMs, qei_t *q)
{
    if(!q) return;
    calls++;
    /* Update velocity every poll; qei_update_velocity handles zero dt */
    int32_t vel = qei_update_velocity(q, (int)nowMs);
    if(nowMs - lastPrintMs >= 1000){
        lastPrintMs = nowMs;
        printf("[QEI][diag] pos=%ld vel=%ldc/s raw=%u wraps=%d calls=%d\n",
               (long)qei_get_position(q), (long)vel, (unsigned)q->last_raw, q->wraps, calls);
        calls = 0;
    }
}
#endif
