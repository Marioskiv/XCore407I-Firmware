#include "stm32f4xx_hal.h"

/* Configure NVIC priority grouping and core peripheral priorities.
 * Strategy:
 *  - Grouping: 4 bits pre-emption, 0 bits subpriority (NVIC_PRIORITYGROUP_4)
 *  - SysTick: medium-low to allow high-priority external interrupts (Ethernet) to pre-empt
 *  - Ethernet IRQ (if used) elevated for low-latency frame handling
 *  - External motion related EXTI lines (0..4) given high, but below Ethernet
 *  - DMA streams (if later enabled for peripherals) keep default for now
 */
void Board_NVIC_Config(void)
{
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* Core system interrupts */
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
    HAL_NVIC_SetPriority(BusFault_IRQn,        0, 0);
    HAL_NVIC_SetPriority(UsageFault_IRQn,      0, 0);
    HAL_NVIC_SetPriority(SVCall_IRQn,          3, 0);
    HAL_NVIC_SetPriority(DebugMonitor_IRQn,    3, 0);
    HAL_NVIC_SetPriority(PendSV_IRQn,          15,0); /* lowest (if later using RTOS context switch) */
    HAL_NVIC_SetPriority(SysTick_IRQn,         10,0);

    /* Ethernet global interrupt (if driver switches to interrupt mode later) */
#ifdef ETH_IRQn
    HAL_NVIC_SetPriority(ETH_IRQn, 5, 0); /* below time-critical motion EXTI (if needed adjust) */
    HAL_NVIC_EnableIRQ(ETH_IRQn);
#endif

    /* EXTI lines for motion alarm / estop (example lines 0..4) */
    HAL_NVIC_SetPriority(EXTI0_IRQn,  5,0);
    HAL_NVIC_SetPriority(EXTI1_IRQn,  5,0);
    HAL_NVIC_SetPriority(EXTI2_IRQn,  5,0);
    HAL_NVIC_SetPriority(EXTI3_IRQn,  5,0);
    HAL_NVIC_SetPriority(EXTI4_IRQn,  5,0);
}

/* Try HSE (8 MHz) PLL -> 168 MHz (Q=7 for USB). Return 0 on success, -1 on failure. */
static int Clock_Try_HSE_168(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Explicitly manage HSE/LSE/LSI states; LSE OFF for boards without crystal */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;   /* 8 MHz /8 = 1 MHz */
    RCC_OscInitStruct.PLL.PLLN = 336; /* 1 MHz *336 = 336 MHz */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; /* 336/2 = 168 MHz */
    RCC_OscInitStruct.PLL.PLLQ = 7;   /* 48 MHz for USB */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        return -1;
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;   /* 168 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;    /* 42 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;    /* 84 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* Fallback: HSI (16 MHz) PLL -> 168 MHz (Q=7 for USB). Return 0 on success, -1 on failure. */
static int Clock_Try_HSI_168(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Ensure LSE is OFF explicitly; use HSI as PLL source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;  /* 16 MHz /16 = 1 MHz */
    RCC_OscInitStruct.PLL.PLLN = 336; /* 1 MHz *336 = 336 MHz */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; /* 336/2 = 168 MHz */
    RCC_OscInitStruct.PLL.PLLQ = 7;   /* 48 MHz for USB */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        return -1;
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;   /* 168 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;    /* 42 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;    /* 84 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* Configure system clock to 168MHz with robust fallback. */
void SystemClock_Config(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Primary: HSE. If it fails (no crystal, unstable), fall back to HSI. */
    if (Clock_Try_HSE_168() != 0) {
        /* Ensure PLL and HSE are reset before fallback attempt */
        HAL_RCC_DeInit();
        if (Clock_Try_HSI_168() != 0) {
            /* If both fail, trap */
            while (1) { __NOP(); }
        }
    }
    SystemCoreClockUpdate();
}
