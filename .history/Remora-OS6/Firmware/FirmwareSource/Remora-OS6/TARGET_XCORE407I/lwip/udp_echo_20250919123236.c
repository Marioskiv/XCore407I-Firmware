#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "udp_echo.h"

#ifndef UDP_ECHO_PORT
#define UDP_ECHO_PORT 5005
#endif

static struct udp_pcb *echo_pcb;

static void udp_echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port)
{
    (void)arg; (void)pcb;
    if (p == NULL) {
        return;
    }
    /* Echo back */
    udp_sendto(echo_pcb, p, addr, port);
    pbuf_free(p);
}

void udp_echo_init(void)
{
    echo_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (!echo_pcb) return;
    if (udp_bind(echo_pcb, IP_ANY_TYPE, UDP_ECHO_PORT) != ERR_OK) {
        udp_remove(echo_pcb);
        echo_pcb = NULL;
        return;
    }
    udp_recv(echo_pcb, udp_echo_recv, NULL);
}
