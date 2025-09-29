#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
  return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  (void)cmd; (void)pbuf; (void)length; return (USBD_OK);
}

/* Receive callback: echo back immediately (simple test) */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  uint32_t rxLen = (Len != NULL) ? *Len : 0;
  if (rxLen > 0) {
    /* Bound length to TX buffer size */
    if (rxLen > APP_TX_DATA_SIZE) rxLen = APP_TX_DATA_SIZE;
    memcpy(UserTxBufferFS, Buf, rxLen);
    /* Initiate echo transmit if not busy */
    (void)CDC_Transmit_FS(UserTxBufferFS, (uint16_t)rxLen);
  }
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, Buf);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  (void)Buf; (void)Len; (void)epnum; /* Nothing to do for now */
  return (USBD_OK);
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc == NULL) return USBD_FAIL;
  if (hcdc->TxState != 0) return USBD_BUSY;
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
