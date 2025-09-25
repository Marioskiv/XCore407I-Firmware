// Motion control auxiliary IO: Enables, Alarms, E-Stop, Jog inputs
#include "stm32f4xx_hal.h"
#include "board_xcore407i.h"
#include "stepgen.h"
#include "encoder.h"
#include "homing.h"
#include "remora_udp.h" /* for remora_count_estop_edge */

/* Global fault bitmask: bit0=X, bit1=Y, bit2=Z, bit3=A */
static volatile uint32_t motion_fault_mask = 0;
static volatile uint32_t estop_active = 0; /* 1 when E-Stop asserted */
static volatile uint32_t limit_mask = 0; /* bit per axis when at/over travel limit */

/* Soft limits (units: encoder counts after homing offset applied) */
static int32_t soft_limit_min[4] = { -100000, -100000, -50000, -50000 };
static int32_t soft_limit_max[4] = {  100000,  100000,  50000,  50000 };

/* Forward references to step timers so we can force stop */
extern TIM_HandleTypeDef htim4; /* axis 0 & 1 */
extern TIM_HandleTypeDef htim3; /* axis 2 */
extern TIM_HandleTypeDef htim8; /* axis 3 */

static void stop_all_step_timers(void)
{
    __HAL_TIM_DISABLE(&htim4);
    __HAL_TIM_DISABLE(&htim3);
    __HAL_TIM_DISABLE(&htim8);
}

static void disable_all_enables(void)
{
    HAL_GPIO_WritePin(GPIOD, X_ENA_PIN | Y_ENA_PIN | Z_ENA_PIN | A_ENA_PIN, GPIO_PIN_RESET);
}

static void motion_alarm_fault_handler(int axis)
{
    if (axis < 0 || axis > 3) return;
    motion_fault_mask |= (1u << axis);
    stop_all_step_timers();
    disable_all_enables();
}

uint32_t motion_get_fault_mask(void) { return motion_fault_mask; }
uint32_t motion_get_estop(void) { return estop_active; }
uint32_t motion_get_limit_mask(void) { return limit_mask; }

uint8_t motion_try_clear_faults(void)
{
    if (estop_active) return 0; /* cannot clear if E-Stop latched */
    motion_fault_mask = 0;
    /* Re-enable outputs (remain disabled until host commands motion) */
    HAL_GPIO_WritePin(GPIOD, X_ENA_PIN | Y_ENA_PIN | Z_ENA_PIN | A_ENA_PIN, GPIO_PIN_SET);
    return 1;
}


void motion_control_io_init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    // Enable pins (outputs)
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = X_ENA_PIN | Y_ENA_PIN | Z_ENA_PIN | A_ENA_PIN;
    HAL_GPIO_Init(GPIOD, &g);
    HAL_GPIO_WritePin(GPIOD, X_ENA_PIN | Y_ENA_PIN | Z_ENA_PIN | A_ENA_PIN, GPIO_PIN_SET);

    // Alarm inputs with EXTI (falling edge by default)
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP; // assume open-drain from drivers
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = X_ALM_PIN | Y_ALM_PIN | Z_ALM_PIN | A_ALM_PIN;
    HAL_GPIO_Init(GPIOE, &g);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0); HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0); HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0); HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0); HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    // E-Stop input PE4 (EXTI4) active low
    GPIO_InitTypeDef e = {0};
    e.Mode = GPIO_MODE_IT_FALLING;
    e.Pull = GPIO_PULLUP;
    e.Speed = GPIO_SPEED_FREQ_LOW;
    e.Pin = ESTOP_PIN;
    HAL_GPIO_Init(ESTOP_PORT, &e);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);

    // Jog inputs initialized here (module uses them); PF0..PF7 active low
    GPIO_InitTypeDef j = {0};
    j.Mode = GPIO_MODE_INPUT;
    j.Pull = GPIO_PULLUP;
    j.Speed = GPIO_SPEED_FREQ_LOW;
    j.Pin = JOG_ALL_PINS;
    HAL_GPIO_Init(JOG_PORT, &j);

    // Probe input PG0 active low with pull-up
    GPIO_InitTypeDef p = {0};
    p.Mode = GPIO_MODE_INPUT;
    p.Pull = GPIO_PULLUP;
    p.Speed = GPIO_SPEED_FREQ_LOW;
    p.Pin = PROBE_PIN;
    HAL_GPIO_Init(PROBE_PORT, &p);
}

// Jog poll: call periodically (e.g., each main loop iteration)

// HAL EXTI callback
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == X_ALM_PIN) {
        motion_alarm_fault_handler(AXIS_X);
    } else if (GPIO_Pin == Y_ALM_PIN) {
        motion_alarm_fault_handler(AXIS_Y);
    } else if (GPIO_Pin == Z_ALM_PIN) {
        motion_alarm_fault_handler(AXIS_Z);
    } else if (GPIO_Pin == A_ALM_PIN) {
        motion_alarm_fault_handler(AXIS_A);
    } else if (GPIO_Pin == ESTOP_PIN) {
        estop_active = 1;
        stop_all_step_timers();
        disable_all_enables();
        remora_count_estop_edge();
    }
}

uint32_t motion_get_probe(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(PROBE_PORT, PROBE_PIN);
    return (s == GPIO_PIN_RESET) ? 0u : 1u; /* 0 = triggered */
}

/* Enforce soft limits on commanded motion: if position beyond limit or moving further out, zero frequency and set limit flag */
void motion_apply_limited_step(int axis, int32_t freq, int enable)
{
    if (axis < 0 || axis > 3) return;
    int32_t pos = encoder_get_count(axis) - homing_get_offset(axis);
    int limited = 0;
    if (pos <= soft_limit_min[axis] && freq < 0) { limited = 1; }
    if (pos >= soft_limit_max[axis] && freq > 0) { limited = 1; }
    if (limited) {
        limit_mask |= (1u << axis);
        freq = 0; enable = 0;
    } else {
        /* Clear bit if inside relaxed window */
        if (pos > soft_limit_min[axis]+10 && pos < soft_limit_max[axis]-10) {
            limit_mask &= ~(1u<<axis);
        }
    }
    stepgen_apply_command(axis, freq, (uint8_t)(enable ? 1 : 0));
}

