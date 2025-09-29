#pragma once
#include "stm32f4xx_hal.h"
#include "usbd_def.h"

#ifdef __cplusplus
extern "C" {
#endif

extern USBD_HandleTypeDef hUsbDeviceFS;
void MX_USB_DEVICE_Init(void);

#ifdef __cplusplus
}
#endif
