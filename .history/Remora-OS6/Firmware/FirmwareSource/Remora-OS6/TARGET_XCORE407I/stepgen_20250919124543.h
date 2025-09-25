#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void stepgen_init(void);
void stepgen_apply_command(int axis, int32_t freq, uint8_t enable);
void stepgen_poll(void); /* placeholder for future acceleration profiles */

#ifdef __cplusplus
}
#endif
