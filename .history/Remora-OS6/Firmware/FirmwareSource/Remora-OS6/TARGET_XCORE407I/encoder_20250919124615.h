#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void encoder_init(void);
int32_t encoder_get_count(int axis);
void encoder_latch_and_reset(void); /* optional reset each cycle */

#ifdef __cplusplus
}
#endif
