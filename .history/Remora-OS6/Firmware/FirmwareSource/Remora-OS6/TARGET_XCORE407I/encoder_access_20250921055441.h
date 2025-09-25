#pragma once
#ifdef USE_ABSTRACT_DRIVERS
#include "drivers/qei/qeiDriver.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Accessors for qei instances (defined in encoder.c) */
qei_t *encoder_get_qei(int axis);
#ifdef __cplusplus
}
#endif
#endif
