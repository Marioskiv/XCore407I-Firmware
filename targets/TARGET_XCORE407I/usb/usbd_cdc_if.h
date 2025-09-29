#pragma once
/* Minimal interface header for CDC implementation */
#include "usbd_cdc.h"
#include <stdint.h>

#ifndef APP_RX_DATA_SIZE
#define APP_RX_DATA_SIZE 256
#endif
#ifndef APP_TX_DATA_SIZE
#define APP_TX_DATA_SIZE 256
#endif

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;
