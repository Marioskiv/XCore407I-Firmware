#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

/* ########################## Module Selection ############################ */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

/* ########################## HSE/HSI Values ############################## */
#define HSE_VALUE    ((uint32_t)25000000U) /* Updated to match XCore407I 25 MHz external crystal */
#define HSI_VALUE    ((uint32_t)16000000U)
#define LSI_VALUE    ((uint32_t)32000U)
#define LSE_VALUE    ((uint32_t)32768U)
/* Add missing standard HAL timing/clock macros used by RCC */
#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT    100U
#endif
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT    5000U
#endif
#ifndef EXTERNAL_CLOCK_VALUE
#define EXTERNAL_CLOCK_VALUE   12288000U
#endif

#define VDD_VALUE                    ((uint32_t)3300U)
#define TICK_INT_PRIORITY            ((uint32_t)0x0FU)
#define USE_RTOS                     0
#define PREFETCH_ENABLE              1
#define INSTRUCTION_CACHE_ENABLE     1
#define DATA_CACHE_ENABLE            1

/* Ethernet configuration parameters */
#define ETH_RXBUFNB        ((uint32_t)4U)
#define ETH_TXBUFNB        ((uint32_t)4U)

/* Ethernet PHY access timeouts and reset delay
 * These are required by the HAL ETH driver (stm32f4xx_hal_eth.c).
 * Values are aligned with STM32CubeF4 project templates.
 */
#ifndef PHY_RESET_DELAY
#define PHY_RESET_DELAY                 (0x000000FFU)
#endif
#ifndef PHY_READ_TO
#define PHY_READ_TO                     (0x0000FFFFU)
#endif
#ifndef PHY_WRITE_TO
#define PHY_WRITE_TO                    (0x0000FFFFU)
#endif

/* Debug assert disabled for size */
#ifdef  USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_eth.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_tim_ex.h"
#include "stm32f4xx_hal_pcd.h"
#include "stm32f4xx_ll_usb.h"

#endif /* STM32F4XX_HAL_CONF_H */
