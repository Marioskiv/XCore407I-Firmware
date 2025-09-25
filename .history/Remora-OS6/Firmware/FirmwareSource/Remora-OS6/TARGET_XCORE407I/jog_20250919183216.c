/* Jog control implementation */
#include "stm32f4xx_hal.h"
#include "board_xcore407i.h"
#include "jog.h"
#include "stepgen.h"

#define AXES 4

typedef struct {
    uint32_t target_speed;   /* steps/s configured */
    uint32_t accel;          /* steps/s^2 */
    uint32_t current_speed;  /* steps/s ramped */
    int direction;           /* -1,0,+1 internal */
    uint8_t debounce_pos;    /* counters */
    uint8_t debounce_neg;
} jog_axis_t;

static jog_axis_t jog_axes[AXES];

static uint32_t last_tick_ms = 0;

static uint32_t millis(void){ return HAL_GetTick(); }

void jog_init(void)
{
    for (int i=0;i<AXES;++i){
        jog_axes[i].target_speed = JOG_FIXED_FREQ;
        jog_axes[i].accel = JOG_ACCEL_DEFAULT;
        jog_axes[i].current_speed = 0;
        jog_axes[i].direction = 0;
        jog_axes[i].debounce_pos = 0;
        jog_axes[i].debounce_neg = 0;
    }
    last_tick_ms = millis();
}

void jog_set_speed_axis(int axis, uint32_t steps_per_sec){ if(axis>=0 && axis<AXES) jog_axes[axis].target_speed = steps_per_sec; }
void jog_set_accel_axis(int axis, uint32_t steps_per_sec2){ if(axis>=0 && axis<AXES) jog_axes[axis].accel = steps_per_sec2; }

void jog_set_speed(uint32_t steps_per_sec){ for(int i=0;i<AXES;++i) jog_set_speed_axis(i, steps_per_sec); }
void jog_set_accel(uint32_t steps_per_sec2){ for(int i=0;i<AXES;++i) jog_set_accel_axis(i, steps_per_sec2); }

uint32_t jog_get_speed_axis(int axis){ return (axis>=0 && axis<AXES)? jog_axes[axis].target_speed:0; }
uint32_t jog_get_accel_axis(int axis){ return (axis>=0 && axis<AXES)? jog_axes[axis].accel:0; }
uint32_t jog_get_current_speed_axis(int axis){ return (axis>=0 && axis<AXES)? jog_axes[axis].current_speed:0; }
uint32_t jog_get_target_speed_axis(int axis){ return jog_get_speed_axis(axis); }
uint32_t jog_get_direction_axis(int axis){ if(axis<0||axis>=AXES) return 0; int d=jog_axes[axis].direction; return d>0?1:(d<0?2:0); }

/* Map axis index to GPIO pins */
static void get_axis_pins(int axis, uint16_t *pos_pin, uint16_t *neg_pin){
    switch(axis){
        case 0: *pos_pin = JOG_X_POS_PIN; *neg_pin = JOG_X_NEG_PIN; break;
        case 1: *pos_pin = JOG_Y_POS_PIN; *neg_pin = JOG_Y_NEG_PIN; break;
        case 2: *pos_pin = JOG_Z_POS_PIN; *neg_pin = JOG_Z_NEG_PIN; break;
        case 3: *pos_pin = JOG_A_POS_PIN; *neg_pin = JOG_A_NEG_PIN; break;
        default: *pos_pin = *neg_pin = 0; break;
    }
}

/* Legacy helper retained for potential future direct direction reads; mark unused */
static int __attribute__((unused)) read_jog_direction(int axis){
    uint16_t p,n; get_axis_pins(axis,&p,&n);
    GPIO_PinState pos = HAL_GPIO_ReadPin(JOG_PORT,p);
    GPIO_PinState neg = HAL_GPIO_ReadPin(JOG_PORT,n);
    if (pos == GPIO_PIN_RESET && neg == GPIO_PIN_RESET) return 0; /* both pressed -> stop */
    if (pos == GPIO_PIN_RESET) return +1;
    if (neg == GPIO_PIN_RESET) return -1;
    return 0;
}

void jog_poll(void)
{
    uint32_t now = millis();
    uint32_t dt_ms = now - last_tick_ms;
    if (dt_ms == 0) dt_ms = 1; /* avoid div zero */
    last_tick_ms = now;

    for (int axis=0; axis<AXES; ++axis){
        uint16_t p_pin,n_pin; get_axis_pins(axis,&p_pin,&n_pin);
        GPIO_PinState pos_raw = HAL_GPIO_ReadPin(JOG_PORT,p_pin);
        GPIO_PinState neg_raw = HAL_GPIO_ReadPin(JOG_PORT,n_pin);
        /* Debounce */
        if (pos_raw == GPIO_PIN_RESET){ if (jog_axes[axis].debounce_pos < JOG_DEBOUNCE_COUNT) jog_axes[axis].debounce_pos++; } else jog_axes[axis].debounce_pos = 0;
        if (neg_raw == GPIO_PIN_RESET){ if (jog_axes[axis].debounce_neg < JOG_DEBOUNCE_COUNT) jog_axes[axis].debounce_neg++; } else jog_axes[axis].debounce_neg = 0;

        int dir = 0;
        if (jog_axes[axis].debounce_pos >= JOG_DEBOUNCE_COUNT && jog_axes[axis].debounce_neg < JOG_DEBOUNCE_COUNT) dir = +1;
        else if (jog_axes[axis].debounce_neg >= JOG_DEBOUNCE_COUNT && jog_axes[axis].debounce_pos < JOG_DEBOUNCE_COUNT) dir = -1;
        else dir = 0;
        jog_axes[axis].direction = dir;

        /* Ramp current_speed toward target */
        uint32_t accel = jog_axes[axis].accel;
        uint32_t target = (dir==0)?0:jog_axes[axis].target_speed;
        uint32_t cur = jog_axes[axis].current_speed;
        if (cur < target){
            /* dv = a * dt; dt in ms -> steps/s^2 * ms/1000 */
            uint32_t dv = (uint32_t)((uint64_t)accel * (uint64_t)dt_ms / 1000u);
            if (dv == 0) dv = 1;
            cur += dv;
            if (cur > target) cur = target;
        } else if (cur > target){
            uint32_t dv = (uint32_t)((uint64_t)accel * (uint64_t)dt_ms / 1000u);
            if (dv == 0) dv = 1;
            if (dv > cur) dv = cur;
            cur -= dv;
        }
        jog_axes[axis].current_speed = cur;

        /* Apply to step generator */
        int32_t freq = (dir==0)?0:(int32_t)cur * (dir>0?1:-1);
        stepgen_apply_command(axis, freq, (cur>0)?1:0);
    }
}
