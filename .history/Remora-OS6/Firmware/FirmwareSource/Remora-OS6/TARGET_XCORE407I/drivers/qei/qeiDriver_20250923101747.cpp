#include "qeiDriver.h"

/* NOTE: This is a minimal adaptation. Real deployment should map GPIO AF and enable clocks externally. */

static void enable_tim_clock(TIM_TypeDef *tim)
{
    if (tim == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
    else if (tim == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
    else if (tim == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
    else if (tim == TIM4) __HAL_RCC_TIM4_CLK_ENABLE();
    else if (tim == TIM5) __HAL_RCC_TIM5_CLK_ENABLE();
    else if (tim == TIM8) __HAL_RCC_TIM8_CLK_ENABLE();
}

int qei_init(qei_t *qei, TIM_TypeDef *tim, uint32_t period, int32_t start_position, int sample_now_ms)
{
    if (!qei || !tim) return 0;
    enable_tim_clock(tim);
    qei->instance = tim;
    qei->last_raw = 0;
    qei->accumulated = start_position;
    qei->velocity_cps = 0;
    qei->last_ts_ms = (uint32_t)sample_now_ms;
    qei->wraps = 0;

    /* Basic encoder mode configuration (assuming CH1/CH2 already in AF inputs) */
    tim->SMCR = (tim->SMCR & ~TIM_SMCR_SMS) | (0x3 << TIM_SMCR_SMS_Pos); /* Encoder mode 3 */
    tim->CCMR1 = (tim->CCMR1 & ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S)) | (1 << TIM_CCMR1_CC1S_Pos) | (1 << TIM_CCMR1_CC2S_Pos);
    tim->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P); /* rising edges by default */
    tim->ARR = period - 1;  /* auto-reload */
    tim->CNT = 0;
    tim->CR1 |= TIM_CR1_CEN;
    return 1;
}

int32_t qei_get_position(qei_t *qei)
{
    if (!qei || !qei->instance) return 0;
    uint32_t raw = qei->instance->CNT;
    /* Handle rollover (assuming 16-bit timers for TIM3/TIM4 etc.; if 32-bit just return CNT) */
    uint32_t mask = qei->instance == TIM2 || qei->instance == TIM5 ? 0xFFFFFFFFu : 0xFFFFu;
    uint32_t half = mask / 2u + 1u;
    int32_t delta = (int32_t)((raw - qei->last_raw + half) & mask) - (int32_t)half;
    if (delta > (int32_t)half/2) qei->wraps++; else if (delta < -(int32_t)half/2) qei->wraps++;
    qei->last_raw = raw;
    qei->accumulated += delta;
    return qei->accumulated;
}

void qei_reset(qei_t *qei, int32_t to_zero)
{
    if (!qei || !qei->instance) return;
    qei->instance->CNT = 0;
    qei->last_raw = 0;
    if (to_zero) qei->accumulated = 0;
    qei->velocity_cps = 0;
    qei->wraps = 0;
}

int32_t qei_update_velocity(qei_t *qei, int now_ms)
{
    if (!qei || !qei->instance) return 0;
    if (now_ms == (int)qei->last_ts_ms) return qei->velocity_cps; // no time delta
    int32_t pos_before = qei->accumulated;
    (void)qei_get_position(qei); // updates accumulated
    int32_t dp = qei->accumulated - pos_before;
    uint32_t dt_ms = (uint32_t)(now_ms - (int)qei->last_ts_ms);
    if (dt_ms == 0) return qei->velocity_cps;
    // counts per second = dp * 1000 / dt_ms (basic integer scaling)
    qei->velocity_cps = (int32_t)((int64_t)dp * 1000 / (int32_t)dt_ms);
    qei->last_ts_ms = (uint32_t)now_ms;
    return qei->velocity_cps;
}

int32_t qei_get_velocity(const qei_t *qei)
{
    return qei ? qei->velocity_cps : 0;
}
