// Encoder implementation with optional abstraction layer.
// When USE_ABSTRACT_DRIVERS is enabled, this file routes through qeiDriver
// minimal wrapper to encourage cross-target reuse. Otherwise it retains the
// direct HAL implementation.

#include "encoder.h"
#include "stm32f4xx_hal.h"
#include "board_xcore407i.h"

#if USE_ABSTRACT_DRIVERS
#include "drivers/qei/qeiDriver.h"
#include "encoder_access.h"
#endif

/* Encoder mapping (conflict-free version):
 * Axis0: TIM3 CH1 PB4, CH2 PB5
 * Axis1: TIM1 CH1 PA8, CH2 PA9
 * Axis2: TIM2 CH1 PA15, CH2 PB3
 * Axis3: not wired (returns 0)
 */

static TIM_HandleTypeDef henc3; /* axis0 TIM3 */
static TIM_HandleTypeDef henc1; /* axis1 TIM1 */
static TIM_HandleTypeDef henc2; /* axis2 TIM2 */

#if USE_ABSTRACT_DRIVERS
static qei_t qei0; // TIM3
static qei_t qei1; // TIM1
static qei_t qei2; // TIM2
#endif

static void enc_timer_init(TIM_HandleTypeDef *htim, TIM_TypeDef *inst)
{
    htim->Instance = inst;
    htim->Init.Prescaler = 0;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = 0xFFFF;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(htim);
}

static void enc_mode_init(TIM_HandleTypeDef *htim)
{
    TIM_Encoder_InitTypeDef sEnc = {0};
    sEnc.EncoderMode = TIM_ENCODERMODE_TI12;
    sEnc.IC1Polarity = TIM_ICPOLARITY_RISING;
    sEnc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sEnc.IC1Prescaler = TIM_ICPSC_DIV1;
    sEnc.IC1Filter = 2;
    sEnc.IC2Polarity = TIM_ICPOLARITY_RISING;
    sEnc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sEnc.IC2Prescaler = TIM_ICPSC_DIV1;
    sEnc.IC2Filter = 2;
    HAL_TIM_Encoder_Init(htim, &sEnc);
    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
}

void encoder_init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gi = {0};
    gi.Mode = GPIO_MODE_AF_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;

    /* Axis0 TIM3 PB4 PB5 */
    gi.Alternate = GPIO_AF2_TIM3;
    gi.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOB, &gi);

    /* Axis1 TIM1 PA8 PA9 */
    gi.Alternate = GPIO_AF1_TIM1;
    gi.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &gi);

    /* Axis2 TIM2 PA15 PB3 */
    gi.Alternate = GPIO_AF1_TIM2;
    gi.Pin = GPIO_PIN_15; HAL_GPIO_Init(GPIOA, &gi);
    gi.Pin = GPIO_PIN_3; gi.Alternate = GPIO_AF1_TIM2; HAL_GPIO_Init(GPIOB, &gi);

    enc_timer_init(&henc3, TIM3);
    enc_timer_init(&henc1, TIM1);
    enc_timer_init(&henc2, TIM2);

    enc_mode_init(&henc3);
    enc_mode_init(&henc1);
    enc_mode_init(&henc2);

#if USE_ABSTRACT_DRIVERS
    // Initialize abstraction structs with same timers; period inferred from HAL setup (0xFFFF). Start at 0 position.
    int now_ms = (int)HAL_GetTick();
    qei_init(&qei0, henc3.Instance, 0x10000u, 0, now_ms);
    qei_init(&qei1, henc1.Instance, 0x10000u, 0, now_ms);
    qei_init(&qei2, henc2.Instance, 0x10000u, 0, now_ms);
#endif
}

int32_t encoder_get_count(int axis)
{
#if USE_ABSTRACT_DRIVERS
    switch(axis){
    case 0: return (int32_t)qei_get_position(&qei0);
    case 1: return (int32_t)qei_get_position(&qei1);
    case 2: return (int32_t)qei_get_position(&qei2);
    case 3: return 0;
    default: return 0;
    }
#else
    switch(axis){
    case 0: return (int32_t)__HAL_TIM_GET_COUNTER(&henc3);
    case 1: return (int32_t)__HAL_TIM_GET_COUNTER(&henc1);
    case 2: return (int32_t)__HAL_TIM_GET_COUNTER(&henc2);
    case 3: return 0; /* not wired */
    default: return 0;
    }
#endif
}

#if USE_ABSTRACT_DRIVERS
qei_t *encoder_get_qei(int axis)
{
    switch(axis){
    case 0: return &qei0;
    case 1: return &qei1;
    case 2: return &qei2;
    default: return 0;
    }
}
#endif

void encoder_latch_and_reset(void)
{
    /* Optionally reset counters each cycle (disabled for now) */
}
