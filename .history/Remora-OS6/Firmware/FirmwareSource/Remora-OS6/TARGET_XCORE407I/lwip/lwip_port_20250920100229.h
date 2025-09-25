#pragma once
#include "lwip/netif.h"
#ifdef __cplusplus
extern "C" {
#endif

void lwip_port_init(void);
void lwip_port_poll(void);
struct netif* lwip_port_netif(void);
/* CubeMX-style wrappers */
void MX_LWIP_Init(void);
void MX_LWIP_Process(void);
void lwip_port_set_link_state(int up);

#ifdef __cplusplus
}
#endif
