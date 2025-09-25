#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"
#include "usbd_def.h"

#ifndef APP_RX_DATA_SIZE
#define APP_RX_DATA_SIZE 512
#endif
#ifndef APP_TX_DATA_SIZE
#define APP_TX_DATA_SIZE 512
#endif

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void);
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

#ifdef __cplusplus
}
#endif
