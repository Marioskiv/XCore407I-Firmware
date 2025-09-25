#pragma once

#include "stm32f4xx_hal.h"
#include "usbd_def.h"

/* Low Level Driver Configuration */
#define USBD_MAX_NUM_INTERFACES     1
#define USBD_MAX_NUM_CONFIGURATION  1
#define USBD_MAX_STR_DESC_SIZ       128
#define USBD_SUPPORT_USER_STRING    1
#define USBD_DEBUG_LEVEL            0
#define USBD_LPM_ENABLED            0
#define USBD_SELF_POWERED           1

/* For CDC class: use defaults from usbd_cdc.h (do not redefine sizes) */

/* FS device ID for USBD_Init */
#ifndef DEVICE_FS
#define DEVICE_FS 0
#endif

void Error_Handler(void);
