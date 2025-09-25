#include "lwip/sys.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

u32_t sys_now(void)
{
    return (u32_t)HAL_GetTick();
}
