/* Minimal startup for STM32F407xx - vector table + reset handler */
.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global  g_pfnVectors
.global  Default_Handler

/* External symbols */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

/* Stack top defined in linker script as _estack */

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors
g_pfnVectors:
  .word _estack            /* Initial Stack Pointer */
  .word Reset_Handler      /* Reset Handler */
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* IRQ handlers (subset) */
  .word WWDG_Handler
  .word PVD_Handler
  .word TAMP_STAMP_Handler
  .word RTC_WKUP_Handler
  .word FLASH_Handler
  .word RCC_Handler
  .word EXTI0_Handler
  .word EXTI1_Handler
  .word EXTI2_Handler
  .word EXTI3_Handler
  .word EXTI4_Handler
  .word DMA1_Stream0_Handler
  .word DMA1_Stream1_Handler
  .word DMA1_Stream2_Handler
  .word DMA1_Stream3_Handler
  .word DMA1_Stream4_Handler
  .word DMA1_Stream5_Handler
  .word DMA1_Stream6_Handler
  .word ADC_Handler
  .word CAN1_TX_Handler
  .word CAN1_RX0_Handler
  .word CAN1_RX1_Handler
  .word CAN1_SCE_Handler
  .word EXTI9_5_Handler
  .word TIM1_BRK_TIM9_Handler
  .word TIM1_UP_TIM10_Handler
  .word TIM1_TRG_COM_TIM11_Handler
  .word TIM1_CC_Handler
  .word TIM2_Handler
  .word TIM3_Handler
  .word TIM4_Handler
  .word I2C1_EV_Handler
  .word I2C1_ER_Handler
  .word I2C2_EV_Handler
  .word I2C2_ER_Handler
  .word SPI1_Handler
  .word SPI2_Handler
  .word USART1_Handler
  .word USART2_Handler
  .word USART3_Handler
  .word EXTI15_10_Handler
  .word RTC_Alarm_Handler
  .word OTG_FS_WKUP_Handler
  .word TIM8_BRK_TIM12_Handler
  .word TIM8_UP_TIM13_Handler
  .word TIM8_TRG_COM_TIM14_Handler
  .word TIM8_CC_Handler
  .word DMA1_Stream7_Handler
  .word FSMC_Handler
  .word SDIO_Handler
  .word TIM5_Handler
  .word SPI3_Handler
  .word UART4_Handler
  .word UART5_Handler
  .word TIM6_DAC_Handler
  .word TIM7_Handler
  .word DMA2_Stream0_Handler
  .word DMA2_Stream1_Handler
  .word DMA2_Stream2_Handler
  .word DMA2_Stream3_Handler
  .word DMA2_Stream4_Handler
  .word ETH_Handler
  .word ETH_WKUP_Handler
  .word CAN2_TX_Handler
  .word CAN2_RX0_Handler
  .word CAN2_RX1_Handler
  .word CAN2_SCE_Handler
  .word OTG_FS_Handler
  .word DMA2_Stream5_Handler
  .word DMA2_Stream6_Handler
  .word DMA2_Stream7_Handler
  .word USART6_Handler
  .word I2C3_EV_Handler
  .word I2C3_ER_Handler
  .word OTG_HS_EP1_OUT_Handler
  .word OTG_HS_EP1_IN_Handler
  .word OTG_HS_WKUP_Handler
  .word OTG_HS_Handler
  .word DCMI_Handler
  .word CRYP_Handler
  .word HASH_RNG_Handler
  .word FPU_Handler

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  /* Copy data */
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  ittt lt
  ldrlt r3, [r0], #4
  strlt r3, [r1], #4
  blt 1b
  /* Zero bss */
  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
2:
  cmp r1, r2
  it lt
  strlt r3, [r1], #4
  blt 2b
  /* Call SystemInit if provided */
  bl SystemInit
  /* Call main */
  bl main
  b .

/* Weak default handlers */
.macro def_handler name
  .weak \name
  .thumb_set \name, Default_Handler
.endm

Default_Handler:
  b .

def_handler NMI_Handler

def_handler HardFault_Handler

def_handler MemManage_Handler

def_handler BusFault_Handler

def_handler UsageFault_Handler

def_handler SVC_Handler

def_handler DebugMon_Handler

def_handler PendSV_Handler

def_handler SysTick_Handler

def_handler WWDG_Handler

def_handler PVD_Handler

def_handler TAMP_STAMP_Handler

def_handler RTC_WKUP_Handler

def_handler FLASH_Handler

def_handler RCC_Handler

def_handler EXTI0_Handler

def_handler EXTI1_Handler

def_handler EXTI2_Handler

def_handler EXTI3_Handler

def_handler EXTI4_Handler

def_handler DMA1_Stream0_Handler

def_handler DMA1_Stream1_Handler

def_handler DMA1_Stream2_Handler

def_handler DMA1_Stream3_Handler

def_handler DMA1_Stream4_Handler

def_handler DMA1_Stream5_Handler

def_handler DMA1_Stream6_Handler

def_handler ADC_Handler

def_handler CAN1_TX_Handler

def_handler CAN1_RX0_Handler

def_handler CAN1_RX1_Handler

def_handler CAN1_SCE_Handler

def_handler EXTI9_5_Handler

def_handler TIM1_BRK_TIM9_Handler

def_handler TIM1_UP_TIM10_Handler

def_handler TIM1_TRG_COM_TIM11_Handler

def_handler TIM1_CC_Handler

def_handler TIM2_Handler

def_handler TIM3_Handler

def_handler TIM4_Handler

def_handler I2C1_EV_Handler

def_handler I2C1_ER_Handler

def_handler I2C2_EV_Handler

def_handler I2C2_ER_Handler

def_handler SPI1_Handler

def_handler SPI2_Handler

def_handler USART1_Handler

def_handler USART2_Handler

def_handler USART3_Handler

def_handler EXTI15_10_Handler

def_handler RTC_Alarm_Handler

def_handler OTG_FS_WKUP_Handler

def_handler TIM8_BRK_TIM12_Handler

def_handler TIM8_UP_TIM13_Handler

def_handler TIM8_TRG_COM_TIM14_Handler

def_handler TIM8_CC_Handler

def_handler DMA1_Stream7_Handler

def_handler FSMC_Handler

def_handler SDIO_Handler

def_handler TIM5_Handler

def_handler SPI3_Handler

def_handler UART4_Handler

def_handler UART5_Handler

def_handler TIM6_DAC_Handler

def_handler TIM7_Handler

def_handler DMA2_Stream0_Handler

def_handler DMA2_Stream1_Handler

def_handler DMA2_Stream2_Handler

def_handler DMA2_Stream3_Handler

def_handler DMA2_Stream4_Handler

def_handler ETH_Handler

def_handler ETH_WKUP_Handler

def_handler CAN2_TX_Handler

def_handler CAN2_RX0_Handler

def_handler CAN2_RX1_Handler

def_handler CAN2_SCE_Handler

def_handler OTG_FS_Handler

def_handler DMA2_Stream5_Handler

def_handler DMA2_Stream6_Handler

def_handler DMA2_Stream7_Handler

def_handler USART6_Handler

def_handler I2C3_EV_Handler

def_handler I2C3_ER_Handler

def_handler OTG_HS_EP1_OUT_Handler

def_handler OTG_HS_EP1_IN_Handler

def_handler OTG_HS_WKUP_Handler

def_handler OTG_HS_Handler

def_handler DCMI_Handler

def_handler CRYP_Handler

def_handler HASH_RNG_Handler

def_handler FPU_Handler

.size Reset_Handler, .-Reset_Handler
