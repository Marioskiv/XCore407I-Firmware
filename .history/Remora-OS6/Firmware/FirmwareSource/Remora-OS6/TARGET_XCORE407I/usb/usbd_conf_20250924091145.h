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

/* Provide memory helpers expected by STM32 USB Device library */
#include <stdlib.h>
#include <string.h>
#ifndef USBD_malloc
#define USBD_malloc        malloc
#endif
#ifndef USBD_free
#define USBD_free          free
#endif
#ifndef USBD_memset
#define USBD_memset        memset
#endif
#ifndef USBD_memcpy
#define USBD_memcpy        memcpy
#endif

/* FS device ID for USBD_Init */
#ifndef DEVICE_FS
#define DEVICE_FS 0
#endif

void Error_Handler(void);
