#include "board_xcore407i.h"
#include "stm32f4xx_hal.h"

/* Centralized initialization for motion related GPIO (STEP/DIR/ENA) and safety inputs.
 * Keeps legacy scattered init code from diverging when pin map changes.
 * Remap variant (MOTION_PROFILE_FSMC_SAFE) handled via macros in board_xcore407i.h.
 */
void Board_MotionGPIO_Init(void)
{
    /* Enable required GPIO port clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef init = {0};

    /* STEP pins: Alternate Function push-pull (timers), pull-down to avoid spurious pulses */
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_PULLDOWN;
    init.Speed = GPIO_SPEED_FREQ_HIGH;

    /* X STEP PB6 TIM4_CH1 AF2 */
    init.Pin = X_STEP_PIN; init.Alternate = GPIO_AF2_TIM4; HAL_GPIO_Init(X_STEP_PORT, &init);
    /* Y STEP PB8 TIM4_CH3 AF2 */
    init.Pin = Y_STEP_PIN; init.Alternate = GPIO_AF2_TIM4; HAL_GPIO_Init(Y_STEP_PORT, &init);
    /* Z STEP PA6 TIM3_CH1 AF2 */
    init.Pin = Z_STEP_PIN; init.Alternate = GPIO_AF2_TIM3; HAL_GPIO_Init(Z_STEP_PORT, &init);
    /* A STEP PC6 TIM8_CH1 AF3 */
    init.Pin = A_STEP_PIN; init.Alternate = GPIO_AF3_TIM8; HAL_GPIO_Init(A_STEP_PORT, &init);

    /* DIR pins: plain outputs, default low */
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_PULLDOWN;
    init.Alternate = 0;

    init.Pin = X_DIR_PIN; HAL_GPIO_Init(X_DIR_PORT, &init); HAL_GPIO_WritePin(X_DIR_PORT, X_DIR_PIN, GPIO_PIN_RESET);
    init.Pin = Y_DIR_PIN; HAL_GPIO_Init(Y_DIR_PORT, &init); HAL_GPIO_WritePin(Y_DIR_PORT, Y_DIR_PIN, GPIO_PIN_RESET);
    init.Pin = Z_DIR_PIN; HAL_GPIO_Init(Z_DIR_PORT, &init); HAL_GPIO_WritePin(Z_DIR_PORT, Z_DIR_PIN, GPIO_PIN_RESET);
    init.Pin = A_DIR_PIN; HAL_GPIO_Init(A_DIR_PORT, &init); HAL_GPIO_WritePin(A_DIR_PORT, A_DIR_PIN, GPIO_PIN_RESET);

    /* ENA pins: outputs, default enabled (high) or adjust if active low in future */
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_PULLDOWN;

    init.Pin = X_ENA_PIN; HAL_GPIO_Init(X_ENA_PORT, &init); HAL_GPIO_WritePin(X_ENA_PORT, X_ENA_PIN, GPIO_PIN_SET);
    init.Pin = Y_ENA_PIN; HAL_GPIO_Init(Y_ENA_PORT, &init); HAL_GPIO_WritePin(Y_ENA_PORT, Y_ENA_PIN, GPIO_PIN_SET);
    init.Pin = Z_ENA_PIN; HAL_GPIO_Init(Z_ENA_PORT, &init); HAL_GPIO_WritePin(Z_ENA_PORT, Z_ENA_PIN, GPIO_PIN_SET);
    init.Pin = A_ENA_PIN; HAL_GPIO_Init(A_ENA_PORT, &init); HAL_GPIO_WritePin(A_ENA_PORT, A_ENA_PIN, GPIO_PIN_SET);

    /* ALM (fault) pins: inputs with pull-up (active low from drivers) */
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLUP;

    init.Pin = X_ALM_PIN; HAL_GPIO_Init(X_ALM_PORT, &init);
    init.Pin = Y_ALM_PIN; HAL_GPIO_Init(Y_ALM_PORT, &init);
    init.Pin = Z_ALM_PIN; HAL_GPIO_Init(Z_ALM_PORT, &init);
    init.Pin = A_ALM_PIN; HAL_GPIO_Init(A_ALM_PORT, &init);

    /* E-Stop input (active low) */
    init.Pin = ESTOP_PIN; HAL_GPIO_Init(ESTOP_PORT, &init);

    /* Probe input (active low) */
    init.Pin = PROBE_PIN; HAL_GPIO_Init(PROBE_PORT, &init);

    /* Jog inputs (active low) */
    init.Pin = JOG_ALL_PINS; HAL_GPIO_Init(JOG_PORT, &init);
}
