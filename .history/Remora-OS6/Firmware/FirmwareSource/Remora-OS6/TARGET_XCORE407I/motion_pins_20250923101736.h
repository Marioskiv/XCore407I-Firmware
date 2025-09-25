#ifndef MOTION_PINS_H
#define MOTION_PINS_H

/* Placeholder motion control pin assignments - update per schematic */

#define AXIS_COUNT 4

/* Step/Dir for X,Y,Z,A */
#define STEP_X_PORT GPIOA
#define STEP_X_PIN  GPIO_PIN_8
#define DIR_X_PORT  GPIOA
#define DIR_X_PIN   GPIO_PIN_9

#define STEP_Y_PORT GPIOA
#define STEP_Y_PIN  GPIO_PIN_10
#define DIR_Y_PORT  GPIOA
#define DIR_Y_PIN   GPIO_PIN_11

#define STEP_Z_PORT GPIOA
#define STEP_Z_PIN  GPIO_PIN_12
#define DIR_Z_PORT  GPIOA
#define DIR_Z_PIN   GPIO_PIN_15

#define STEP_A_PORT GPIOB
#define STEP_A_PIN  GPIO_PIN_4
#define DIR_A_PORT  GPIOB
#define DIR_A_PIN   GPIO_PIN_5

/* Limit switches (placeholders) */
#define LIMIT_X_PORT GPIOC
#define LIMIT_X_PIN  GPIO_PIN_0
#define LIMIT_Y_PORT GPIOC
#define LIMIT_Y_PIN  GPIO_PIN_1
#define LIMIT_Z_PORT GPIOC
#define LIMIT_Z_PIN  GPIO_PIN_2
#define LIMIT_A_PORT GPIOC
#define LIMIT_A_PIN  GPIO_PIN_3

/* Encoder (shared or rotary) placeholder pins */
#define ENC_A_PORT GPIOD
#define ENC_A_PIN  GPIO_PIN_12
#define ENC_B_PORT GPIOD
#define ENC_B_PIN  GPIO_PIN_13

#endif /* MOTION_PINS_H */
