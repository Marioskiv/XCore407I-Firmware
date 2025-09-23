#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

err_t ethernetif_init(struct netif *netif);
void  ethernetif_poll(struct netif *netif);
uint16_t ethernetif_phy_read(uint16_t reg);
uint8_t ethernetif_link_up(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */
