#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Skeleton for future Remora communications abstraction.
 * Intent: decouple packet framing / transport specifics from core logic.
 */

typedef struct {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t rx_errors;
} remora_comms_stats_t;

void remora_comms_init(void); /* set up sockets or transport underlying */
int  remora_comms_poll(void); /* process inbound, return # processed */
int  remora_comms_send_feedback(const void *payload, uint16_t len); /* wrap send */
const remora_comms_stats_t *remora_comms_get_stats(void);

#ifdef __cplusplus
}
#endif
