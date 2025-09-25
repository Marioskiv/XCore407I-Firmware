#ifndef XCORE407I_PIN_H
#define XCORE407I_PIN_H

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

static inline pin_t pin_make(GPIO_TypeDef *port, uint16_t pin) { pin_t p = {port, pin}; return p; }

static inline void pin_mode_output(pin_t p) {
    GPIO_InitTypeDef g; memset(&g, 0, sizeof(g));
    g.Pin = p.pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(p.port, &g);
}
static inline void pin_mode_input_pullup(pin_t p) {
    GPIO_InitTypeDef g; memset(&g, 0, sizeof(g));
    g.Pin = p.pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(p.port, &g);
}

static inline void pin_write(pin_t p, uint8_t level) { HAL_GPIO_WritePin(p.port, p.pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static inline void pin_toggle(pin_t p) { HAL_GPIO_TogglePin(p.port, p.pin); }
static inline uint8_t pin_read(pin_t p) { return (uint8_t)(HAL_GPIO_ReadPin(p.port, p.pin) == GPIO_PIN_SET); }

#ifdef __cplusplus
}
#endif

#endif /* XCORE407I_PIN_H */
