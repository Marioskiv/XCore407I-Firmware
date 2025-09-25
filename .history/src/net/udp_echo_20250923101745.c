#include "lwip/udp.h"
#include <string.h>

static struct udp_pcb *echo_pcb = 0;

static void echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                      const ip_addr_t *addr, u16_t port)
{
    (void)arg; (void)pcb;
    if (p) {
        /* Echo back */
        udp_sendto(pcb, p, addr, port);
        pbuf_free(p);
    }
}

void udp_echo_init(void)
{
    echo_pcb = udp_new();
    if (!echo_pcb) return;
    if (udp_bind(echo_pcb, IP_ANY_TYPE, 5005) != ERR_OK) {
        udp_remove(echo_pcb);
        echo_pcb = 0;
        return;
    }
    udp_recv(echo_pcb, echo_recv, NULL);
}
