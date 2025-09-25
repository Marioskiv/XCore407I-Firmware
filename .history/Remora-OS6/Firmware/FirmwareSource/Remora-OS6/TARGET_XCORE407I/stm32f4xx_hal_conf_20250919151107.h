#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

/* HAL module selection */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_RNG_MODULE_ENABLED
#define HAL_FSMC_MODULE_ENABLED

/* Oscillator values (override defaults from HAL if not provided) */
#ifndef HSE_VALUE
#define HSE_VALUE              ((uint32_t)8000000U)   /* 8 MHz external crystal */
#endif
#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT    ((uint32_t)100U)       /* 100 ms */
#endif
#ifndef LSE_VALUE
#define LSE_VALUE              ((uint32_t)32768U)     /* 32.768 kHz crystal */
#endif
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000U)      /* 5 s */
#endif
#ifndef HSI_VALUE
#define HSI_VALUE              ((uint32_t)16000000U)
#endif
#ifndef LSI_VALUE
#define LSI_VALUE              ((uint32_t)32000U)
#endif
/* External clock value used for some peripherals (I2S, etc.). Provide a safe default */
#ifndef EXTERNAL_CLOCK_VALUE
#define EXTERNAL_CLOCK_VALUE   ((uint32_t)8000000U)
#endif

#define VDD_VALUE                    ((uint32_t)3300U)
#define TICK_INT_PRIORITY            ((uint32_t)0x0FU)
#define USE_RTOS                     0
#define PREFETCH_ENABLE              1
#define INSTRUCTION_CACHE_ENABLE     1
#define DATA_CACHE_ENABLE            1

/* Ethernet configuration */
#define MAC_ADDR0   2
#define MAC_ADDR1   0
#define MAC_ADDR2   0
#define MAC_ADDR3   0
#define MAC_ADDR4   0
#define MAC_ADDR5   0

#define ETH_RX_BUF_SIZE  ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE  ETH_MAX_PACKET_SIZE
#define ETH_RXBUFNB      ((uint32_t)4)
#define ETH_TXBUFNB      ((uint32_t)4)

#define PHY_RESET_DELAY                 ((uint32_t)0x000000FF)
#define PHY_CONFIG_DELAY                ((uint32_t)0x00000FFF)
#define PHY_READ_TO                     ((uint32_t)0x0000FFFF)
#define PHY_WRITE_TO                    ((uint32_t)0x0000FFFF)

#define PHY_BSR                         ((uint16_t)0x01U)
#define PHY_LINKED_STATUS               ((uint16_t)0x0004U)
#define PHY_AUTONEGO_COMPLETE           ((uint16_t)0x0020U)

#define PHY_ADDRESS                     ((uint16_t)0x01U)

/* Disable HAL callback registration features to silence Wundef warnings */
#define USE_HAL_ETH_REGISTER_CALLBACKS     0U
#define USE_HAL_UART_REGISTER_CALLBACKS    0U
#define USE_HAL_USART_REGISTER_CALLBACKS   0U
#define USE_HAL_TIM_REGISTER_CALLBACKS     0U
#define USE_HAL_RNG_REGISTER_CALLBACKS     0U
#define USE_HAL_RTC_REGISTER_CALLBACKS     0U
#define USE_HAL_PCD_REGISTER_CALLBACKS     0U
#define USE_HAL_HCD_REGISTER_CALLBACKS     0U

/* Static inline assert macro */
#define assert_param(expr) ((void)0U)

/* Includes */
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_exti.h"
#include "stm32f4xx_hal_rtc.h"
#include "stm32f4xx_hal_eth.h"
#include "stm32f4xx_hal_pcd.h"
#include "stm32f4xx_hal_hcd.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_usart.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_rng.h"
#include "stm32f4xx_ll_fsmc.h"

#endif /* STM32F4XX_HAL_CONF_H */
