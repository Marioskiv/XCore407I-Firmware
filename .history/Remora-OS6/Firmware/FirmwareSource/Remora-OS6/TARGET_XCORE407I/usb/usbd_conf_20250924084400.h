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

/* For CDC class */
#define CDC_IN_EP                   0x81
#define CDC_OUT_EP                  0x01
#define CDC_CMD_EP                  0x82

#define CDC_DATA_FS_MAX_PACKET_SIZE 64
#define CDC_CMD_PACKET_SIZE         8

void Error_Handler(void);
