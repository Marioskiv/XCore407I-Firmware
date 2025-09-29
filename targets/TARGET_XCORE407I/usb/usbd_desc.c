#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID     0x0483
#define USBD_LANGID_STRING 1033
#define USBD_MANUFACTURER_STRING (uint8_t*)"XCore"
#define USBD_PID_FS  0x5740
#define USBD_PRODUCT_STRING_FS (uint8_t*)"XCore407I CDC"
#define USBD_CONFIGURATION_STRING_FS (uint8_t*)"CDC Config"
#define USBD_INTERFACE_STRING_FS (uint8_t*)"CDC Interface"

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t hUSBDDeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
  0x12,                       /* bLength */
  USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
  0x00, 0x02,                 /* bcdUSB 2.00 */
  0x02,                       /* bDeviceClass (CDC) */
  0x02,                       /* bDeviceSubClass */
  0x00,                       /* bDeviceProtocol */
  USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
  LOBYTE(USBD_VID), HIBYTE(USBD_VID), /* idVendor */
  LOBYTE(USBD_PID_FS), HIBYTE(USBD_PID_FS), /* idProduct */
  0x00, 0x01,                 /* bcdDevice rel. 1.00 */
  USBD_IDX_MFC_STR,           /* Index of manufacturer */
  USBD_IDX_PRODUCT_STR,       /* Index of product */
  USBD_IDX_SERIAL_STR,        /* Index of serial number */
  USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  *length = sizeof(hUSBDDeviceDesc);
  return hUSBDDeviceDesc;
}

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  for (uint8_t idx = 0; idx < len; idx++) {
    uint8_t digit = (value >> 28) & 0xF;
    if (digit < 0xA) pbuf[2*idx] = digit + '0'; else pbuf[2*idx] = digit + 'A' - 10;
    pbuf[2*idx + 1] = 0;
    value <<= 4;
  }
}

static uint8_t *USBD_StrTemplate(uint8_t *ascii, uint16_t *length)
{
  uint8_t idx = 0;
  while (ascii[idx] != 0 && idx < 63) idx++;
  USBD_StrDesc[0] = (idx * 2) + 2;
  USBD_StrDesc[1] = USB_DESC_TYPE_STRING;
  for (uint8_t i = 0; i < idx; i++) {
    USBD_StrDesc[2 + (i * 2)] = ascii[i];
    USBD_StrDesc[3 + (i * 2)] = 0;
  }
  *length = USBD_StrDesc[0];
  return USBD_StrDesc;
}

uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  static uint8_t langIDDesc[4] = {4, USB_DESC_TYPE_STRING, LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING)};
  *length = 4;
  return langIDDesc;
}

uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed; return USBD_StrTemplate(USBD_MANUFACTURER_STRING, length);
}

uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed; return USBD_StrTemplate(USBD_PRODUCT_STRING_FS, length);
}

uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed;
  static uint8_t serialStr[26];
  serialStr[0] = 26; serialStr[1] = USB_DESC_TYPE_STRING;
  uint32_t id0 = *(uint32_t*)0x1FFF7A10;
  uint32_t id1 = *(uint32_t*)0x1FFF7A14;
  uint32_t id2 = *(uint32_t*)0x1FFF7A18;
  IntToUnicode(id0 + id2, &serialStr[2], 8);
  IntToUnicode(id1, &serialStr[18], 4);
  *length = serialStr[0];
  return serialStr;
}

uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed; return USBD_StrTemplate(USBD_CONFIGURATION_STRING_FS, length);
}

uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
  (void)speed; return USBD_StrTemplate(USBD_INTERFACE_STRING_FS, length);
}

USBD_DescriptorsTypeDef FS_Desc = {
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor
};
