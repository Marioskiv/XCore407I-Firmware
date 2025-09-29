#include "stm32f4xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <string.h>

extern void SystemClock_Config(void);


static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    MX_USB_DEVICE_Init();

    const char *msg = "Hello from XCore407I (CDC)\r\n";

    while (1) {
        CDC_Transmit_FS((uint8_t*)msg, (uint16_t)strlen(msg));
        HAL_Delay(1000);
    }
}
