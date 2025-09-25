#include "board_xcore407i.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_bus.h"

/* Centralized initialization for motion related GPIO (STEP/DIR/ENA) and safety inputs.
 * Keeps legacy scattered init code from diverging when pin map changes.
 */
void Board_MotionGPIO_Init(void)
{
    /* Enable required GPIO port clocks */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);

    LL_GPIO_InitTypeDef init = {0};

    /* STEP pins: Alternate Function push-pull (timers), pull-down to avoid spurious pulses */
    init.Mode = LL_GPIO_MODE_ALTERNATE;
    init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    init.Pull = LL_GPIO_PULL_DOWN;
    init.Speed = LL_GPIO_SPEED_FREQ_HIGH;

    /* X STEP PB6 TIM4_CH1 AF2 */
    init.Pin = X_STEP_PIN; init.Alternate = LL_GPIO_AF_2; LL_GPIO_Init(X_STEP_PORT, &init);
    /* Y STEP PB8 TIM4_CH3 AF2 */
    init.Pin = Y_STEP_PIN; init.Alternate = LL_GPIO_AF_2; LL_GPIO_Init(Y_STEP_PORT, &init);
    /* Z STEP PA6 TIM3_CH1 AF2 */
    init.Pin = Z_STEP_PIN; init.Alternate = LL_GPIO_AF_2; LL_GPIO_Init(Z_STEP_PORT, &init);
    /* A STEP PC6 TIM8_CH1 AF3 */
    init.Pin = A_STEP_PIN; init.Alternate = LL_GPIO_AF_3; LL_GPIO_Init(A_STEP_PORT, &init);

    /* DIR pins: plain outputs, default low */
    init.Mode = LL_GPIO_MODE_OUTPUT;
    init.Pull = LL_GPIO_PULL_DOWN;
    init.Alternate = 0;

    init.Pin = X_DIR_PIN; LL_GPIO_Init(X_DIR_PORT, &init); LL_GPIO_ResetOutputPin(X_DIR_PORT, X_DIR_PIN);
    init.Pin = Y_DIR_PIN; LL_GPIO_Init(Y_DIR_PORT, &init); LL_GPIO_ResetOutputPin(Y_DIR_PORT, Y_DIR_PIN);
    init.Pin = Z_DIR_PIN; LL_GPIO_Init(Z_DIR_PORT, &init); LL_GPIO_ResetOutputPin(Z_DIR_PORT, Z_DIR_PIN);
    init.Pin = A_DIR_PIN; LL_GPIO_Init(A_DIR_PORT, &init); LL_GPIO_ResetOutputPin(A_DIR_PORT, A_DIR_PIN);

    /* ENA pins: outputs, default enabled (high) or adjust if active low in future */
    init.Mode = LL_GPIO_MODE_OUTPUT;
    init.Pull = LL_GPIO_PULL_DOWN;

    init.Pin = X_ENA_PIN; LL_GPIO_Init(X_ENA_PORT, &init); LL_GPIO_SetOutputPin(X_ENA_PORT, X_ENA_PIN);
    init.Pin = Y_ENA_PIN; LL_GPIO_Init(Y_ENA_PORT, &init); LL_GPIO_SetOutputPin(Y_ENA_PORT, Y_ENA_PIN);
    init.Pin = Z_ENA_PIN; LL_GPIO_Init(Z_ENA_PORT, &init); LL_GPIO_SetOutputPin(Z_ENA_PORT, Z_ENA_PIN);
    init.Pin = A_ENA_PIN; LL_GPIO_Init(A_ENA_PORT, &init); LL_GPIO_SetOutputPin(A_ENA_PORT, A_ENA_PIN);

    /* ALM (fault) pins: inputs with pull-up (active low from drivers) */
    init.Mode = LL_GPIO_MODE_INPUT;
    init.Pull = LL_GPIO_PULL_UP;

    init.Pin = X_ALM_PIN; LL_GPIO_Init(X_ALM_PORT, &init);
    init.Pin = Y_ALM_PIN; LL_GPIO_Init(Y_ALM_PORT, &init);
    init.Pin = Z_ALM_PIN; LL_GPIO_Init(Z_ALM_PORT, &init);
    init.Pin = A_ALM_PIN; LL_GPIO_Init(A_ALM_PORT, &init);

    /* E-Stop input (active low) */
    init.Pin = ESTOP_PIN; LL_GPIO_Init(ESTOP_PORT, &init);

    /* Probe input (active low) */
    init.Pin = PROBE_PIN; LL_GPIO_Init(PROBE_PORT, &init);

    /* Jog inputs (active low) */
    init.Pin = JOG_ALL_PINS; LL_GPIO_Init(JOG_PORT, &init);
}
