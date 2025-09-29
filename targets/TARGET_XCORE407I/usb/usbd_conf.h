// Minimal usbd_conf.h for CDC device build
#pragma once
#include "stm32f4xx_hal.h"
#include <string.h>

// Endpoint addresses (0x80 bit = IN direction)
#define CDC_IN_EP      0x81U
#define CDC_OUT_EP     0x01U
#define CDC_CMD_EP     0x82U

// Packet sizes (Full Speed)
#define CDC_DATA_FS_MAX_PACKET_SIZE  64U
#define CDC_CMD_PACKET_SIZE          8U
#define USB_MAX_EP0_SIZE             64U

// Core library configuration overrides
#define USBD_MAX_NUM_INTERFACES       1U
#define USBD_MAX_NUM_CONFIGURATION    1U
#define USBD_MAX_STR_DESC_SIZ         128U
#define USBD_SUPPORT_USER_STRING_DESC 0U
#define USBD_SELF_POWERED             0U
#define USBD_MAX_POWER                0x32U  /* 100 mA */

// Full speed device index passed to USBD_Init
#define DEVICE_FS 0

/* NOTE:
 * Do NOT declare extern FS_Desc here: usbd_def.h includes this file BEFORE
 * defining USBD_DescriptorsTypeDef. We keep the extern in usbd_desc.h only to
 * avoid an incomplete type usage and the previous compile error.
 */

/* Minimal memory management layer expected by ST USB Device Core.
 * We provide a single static buffer sufficient for CDC class state.
 * This avoids pulling in a full heap implementation for this minimal build.
 */
void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc   USBD_static_malloc
#define USBD_free     USBD_static_free
#define USBD_memset   memset
#define USBD_memcpy   memcpy
#define USBD_Delay    HAL_Delay

