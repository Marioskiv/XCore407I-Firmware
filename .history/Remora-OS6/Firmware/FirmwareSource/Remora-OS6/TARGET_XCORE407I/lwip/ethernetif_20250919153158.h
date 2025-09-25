#pragma once
#include "lwip/netif.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t ethernetif_init(struct netif *netif);
void ethernetif_poll(struct netif *netif);

/* Accessor for internal ethernet stats structure */
const void *ethernetif_get_stats(void);

#ifdef __cplusplus
}
#endif
