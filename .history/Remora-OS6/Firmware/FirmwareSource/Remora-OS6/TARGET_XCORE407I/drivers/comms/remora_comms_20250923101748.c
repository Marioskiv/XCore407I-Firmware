#include "remora_comms.h"
#include <string.h>

static remora_comms_stats_t g_stats;

void remora_comms_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    /* Future: open UDP, configure source, etc. */
}

int remora_comms_poll(void)
{
    /* Future: receive packets, increment stats */
    return 0;
}

int remora_comms_send_feedback(const void *payload, uint16_t len)
{
    (void)payload; (void)len;
    /* Future: send via UDP or other transport */
    g_stats.tx_packets++;
    return 1; /* pretend success */
}

const remora_comms_stats_t *remora_comms_get_stats(void)
{
    return &g_stats;
}
