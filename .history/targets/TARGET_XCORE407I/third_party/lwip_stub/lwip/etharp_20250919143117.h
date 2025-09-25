#pragma once
#include "pbuf.h"
#include "netif.h"
static inline int etharp_output(struct netif* netif, struct pbuf* p, const void* ipaddr){(void)netif;(void)p;(void)ipaddr; return 0;}
