#include "stepgen.h"
#include "stm32f4xx_hal.h"
#include "board_xcore407i.h"
#include <string.h>

/* Simple timer-based step pulse generation: configure timers in toggle mode.
 * Frequency command (Hz) => timer ARR based on 168MHz / prescaler.
 * For simplicity use a fixed prescaler to keep ARR in range.
 */

#define MAX_AXES 4

TIM_HandleTypeDef htim4; /* shared for axis 0 and 1 (different channels) */
TIM_HandleTypeDef htim3; /* axis 2 */
TIM_HandleTypeDef htim8; /* axis 3 */

static void step_timer_base_init(TIM_HandleTypeDef *htim, TIM_TypeDef *instance)
{
    htim->Instance = instance;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    /* Prescaler: divide 168MHz to 1MHz -> prescaler = 168-1 */
    htim->Init.Prescaler = 168 - 1;
    htim->Init.Period = 1000; /* default 500Hz step (toggle produces ~500Hz) */
    if (HAL_TIM_Base_Init(htim) != HAL_OK) return;
}

static void step_channel_pwm_init(TIM_HandleTypeDef *htim, uint32_t channel)
{
    TIM_OC_InitTypeDef sOC = {0};
    HAL_TIM_PWM_Init(htim);
    sOC.OCMode = TIM_OCMODE_TOGGLE;
    sOC.Pulse = htim->Init.Period / 2; /* toggle midpoint */
    sOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_OC_ConfigChannel(htim, &sOC, channel);
    HAL_TIM_OC_Start(htim, channel);
}

void stepgen_init(void)
{
    /* Centralized motion GPIO initialization (STEP/DIR/ENA/inputs). */
    Board_MotionGPIO_Init();

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_TIM8_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Legacy direct GPIO init below retained temporarily until fully migrated to LL init in Board_MotionGPIO_Init().
     * TODO: Remove duplicate configuration once validated on hardware (ensures no regression).
     */
    GPIO_InitTypeDef gi = {0};

    gi.Mode = GPIO_MODE_AF_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;

    gi.Pin = GPIO_PIN_6; gi.Alternate = GPIO_AF2_TIM4; HAL_GPIO_Init(GPIOB, &gi); /* PB6 TIM4_CH1 */
    gi.Pin = GPIO_PIN_8; gi.Alternate = GPIO_AF2_TIM4; HAL_GPIO_Init(GPIOB, &gi); /* PB8 TIM4_CH3 */
    gi.Pin = GPIO_PIN_6; gi.Alternate = GPIO_AF2_TIM3; HAL_GPIO_Init(GPIOA, &gi); /* PA6 TIM3_CH1 */
    gi.Pin = GPIO_PIN_6; gi.Alternate = GPIO_AF3_TIM8; HAL_GPIO_Init(GPIOC, &gi); /* PC6 TIM8_CH1 */

    /* DIR pins as output */
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pin = GPIO_PIN_7; HAL_GPIO_Init(GPIOB, &gi); /* DIR0 PB7 */
    gi.Pin = GPIO_PIN_9; HAL_GPIO_Init(GPIOB, &gi); /* DIR1 PB9 */
    gi.Pin = GPIO_PIN_14; HAL_GPIO_Init(GPIOB, &gi); /* DIR2 PB14 (reassigned) */
    gi.Pin = GPIO_PIN_7; HAL_GPIO_Init(GPIOC, &gi); /* DIR3 PC7 */

    step_timer_base_init(&htim4, TIM4);
    step_timer_base_init(&htim3, TIM3);
    step_timer_base_init(&htim8, TIM8);

    step_channel_pwm_init(&htim4, TIM_CHANNEL_1); /* axis 0 */
    step_channel_pwm_init(&htim4, TIM_CHANNEL_3); /* axis 1 */
    step_channel_pwm_init(&htim3, TIM_CHANNEL_1); /* axis 2 */
    step_channel_pwm_init(&htim8, TIM_CHANNEL_1); /* axis 3 */
}

static void apply_freq(TIM_HandleTypeDef *htim, uint32_t channel, int32_t freq)
{
    if (freq <= 0) {
        /* stop channel */
        __HAL_TIM_DISABLE(htim);
        return;
    }
    uint32_t base_clk = 1000000; /* 1 MHz after prescaler */
    uint32_t period = (base_clk / (uint32_t)(freq * 2)); /* toggle gives half-period pulses */
    if (period == 0) period = 1;
    __HAL_TIM_DISABLE(htim);
    __HAL_TIM_SET_AUTORELOAD(htim, period);
    __HAL_TIM_SET_COUNTER(htim, 0);
    switch(channel){
        case TIM_CHANNEL_1: htim->Instance->CCR1 = period/2; break;
        case TIM_CHANNEL_2: htim->Instance->CCR2 = period/2; break;
        case TIM_CHANNEL_3: htim->Instance->CCR3 = period/2; break;
        case TIM_CHANNEL_4: htim->Instance->CCR4 = period/2; break;
    }
    __HAL_TIM_ENABLE(htim);
}

void stepgen_apply_command(int axis, int32_t freq, uint8_t enable)
{
    if (axis < 0 || axis >= MAX_AXES) return;
    if (!enable || freq == 0) {
        switch(axis){
            case 0: __HAL_TIM_DISABLE(&htim4); break;
            case 1: /* both on same timer: if one disabled we still might need other */ break;
            case 2: __HAL_TIM_DISABLE(&htim3); break;
            case 3: __HAL_TIM_DISABLE(&htim8); break;
        }
        return;
    }
    switch(axis){
        case 0: apply_freq(&htim4, TIM_CHANNEL_1, freq); break;
        case 1: apply_freq(&htim4, TIM_CHANNEL_3, freq); break;
        case 2: apply_freq(&htim3, TIM_CHANNEL_1, freq); break;
        case 3: apply_freq(&htim8, TIM_CHANNEL_1, freq); break;
    }
}

void stepgen_poll(void)
{
    /* Placeholder for acceleration / ramping logic */
}
