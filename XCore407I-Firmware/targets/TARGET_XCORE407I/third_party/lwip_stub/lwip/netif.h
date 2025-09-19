#pragma once
#include <stdint.h>
struct netif {
  uint8_t hwaddr[6];
  uint8_t hwaddr_len;
  uint16_t mtu;
  uint8_t flags;
  char name[2];
  err_t (*input)(struct pbuf *p, struct netif *netif);
  err_t (*output)(struct netif *netif, struct pbuf *p, const void *ipaddr);
  err_t (*linkoutput)(struct netif *netif, struct pbuf *p);
};
#define NETIF_FLAG_BROADCAST 0x01
#define NETIF_FLAG_ETHARP    0x02
#define NETIF_FLAG_LINK_UP   0x04
