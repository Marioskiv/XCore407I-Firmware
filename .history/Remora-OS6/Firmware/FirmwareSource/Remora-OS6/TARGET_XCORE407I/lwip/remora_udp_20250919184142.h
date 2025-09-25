#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void remora_udp_init(void);
void remora_udp_poll_tx(void); /* package and send feedback */
void remora_udp_poll(void);
void remora_udp_send_feedback(void);

/* Telemetry / diagnostics accessors */
uint32_t remora_get_auth_failures(void);
void remora_count_auth_failure(void);
void remora_count_crc_error(void);
void remora_count_estop_edge(void);
void remora_disable_ext_telemetry(void);
void remora_enable_ext_telemetry(void);

#ifdef __cplusplus
}
#endif
