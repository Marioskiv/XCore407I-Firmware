#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_desc.h"

#define USBD_VID     0x0483
#define USBD_PID     0x5740
#define USBD_LANGID_STRING 1033
#define USBD_MANUFACTURER_STRING "XCore407I"
#define USBD_PRODUCT_STRING      "XCore407I CDC"
#define USBD_CONFIGURATION_STRING "CDC Config"
#define USBD_INTERFACE_STRING     "CDC Interface"

/* Forward declarations (string/device descriptor providers) */
uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef FS_Desc =
{
  USBD_FS_DeviceDescriptor,
  USBD_FS_LangIDStrDescriptor,
  USBD_FS_ManufacturerStrDescriptor,
  USBD_FS_ProductStrDescriptor,
  USBD_FS_SerialStrDescriptor,
  USBD_FS_ConfigStrDescriptor,
  USBD_FS_InterfaceStrDescriptor
};

static uint8_t  USBD_DeviceDesc[USB_LEN_DEV_DESC] =
{
  0x12,                       /* bLength */
  USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
  0x00, 0x02,                 /* bcdUSB */
  0x02,                       /* bDeviceClass (CDC) */
  0x00,                       /* bDeviceSubClass */
  0x00,                       /* bDeviceProtocol */
  USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
  LOBYTE(USBD_VID), HIBYTE(USBD_VID),
  LOBYTE(USBD_PID), HIBYTE(USBD_PID),
  0x00, 0x02,                 /* bcdDevice rel. 2.00 */
  USBD_IDX_MFC_STR,           /* Index of manufacturer  string */
  USBD_IDX_PRODUCT_STR,       /* Index of product string */
  USBD_IDX_SERIAL_STR,        /* Index of serial number string */
  USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; *length = (uint16_t)sizeof(USBD_DeviceDesc); return USBD_DeviceDesc; }

uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; static uint8_t str[] = {0x04, USB_DESC_TYPE_STRING, LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING)}; *length=(uint16_t)sizeof(str); return str; }

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{ for (uint8_t idx = 0 ; idx < len ; idx++) { if ((value >> 28) < 0xA) pbuf[2*idx] = (value >> 28) + '0'; else pbuf[2*idx] = (value >> 28) + 'A' - 10; pbuf[2*idx + 1] = 0; value = value << 4; } }

uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  static uint8_t serial[26]; serial[0] = 26; serial[1] = USB_DESC_TYPE_STRING; 
  uint32_t uid0 = HAL_GetUIDw0(); uint32_t uid1 = HAL_GetUIDw1(); uint32_t uid2 = HAL_GetUIDw2();
  IntToUnicode(uid0, &serial[2], 8); IntToUnicode(uid1, &serial[18], 4); (void)uid2; /* keep length 26 */
  *length = 26; return serial;
}

uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t*)USBD_PRODUCT_STRING, USBD_StrDesc, length); return USBD_StrDesc; }
uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc, length); return USBD_StrDesc; }
uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t*)USBD_CONFIGURATION_STRING, USBD_StrDesc, length); return USBD_StrDesc; }
uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t*)USBD_INTERFACE_STRING, USBD_StrDesc, length); return USBD_StrDesc; }
