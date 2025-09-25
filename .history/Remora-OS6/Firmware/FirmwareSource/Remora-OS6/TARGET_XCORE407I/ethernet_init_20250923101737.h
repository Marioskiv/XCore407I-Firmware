#pragma once
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ETH_HandleTypeDef heth;
void MX_ETH_Init(void);
int ETH_LinkUp(void);

#ifdef __cplusplus
}
#endif
