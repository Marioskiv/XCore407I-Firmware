#include "usbd_cdc_if.h"
#include "usbd_core.h"

USBD_HandleTypeDef hUsbDeviceFS;
static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

extern USBD_DescriptorsTypeDef FS_Desc; /* from usbd_desc.c */

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{ return (USBD_OK); }

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{ (void)cmd; (void)pbuf; (void)length; return (USBD_OK); }

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* Echo back */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, *Len);
  USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* Prepare for next reception */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  /* Transmit complete callback (optional) */
  NULL
};

void MX_USB_DEVICE_Init(void)
{
  /* Init Device Library, add supported class and start the library. */
  USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
  USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
  USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
  /* Give host time to stabilize VBUS before asserting connect */
  HAL_Delay(100);
  USBD_Start(&hUsbDeviceFS);
}

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
