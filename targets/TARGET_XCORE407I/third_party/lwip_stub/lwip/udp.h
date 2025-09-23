#pragma once
#include "pbuf.h"
#include "ip_addr.h"
#include "err.h"
struct udp_pcb { int dummy; };
static inline struct udp_pcb* udp_new_ip_type(int type){(void)type; return (struct udp_pcb*)1;}
static inline int udp_bind(struct udp_pcb* pcb, const void* ip, uint16_t port){(void)pcb;(void)ip;(void)port; return ERR_OK;}
static inline void udp_recv(struct udp_pcb* pcb, void (*cb)(void*, struct udp_pcb*, struct pbuf*, const ip_addr_t*, uint16_t), void* arg){(void)pcb;(void)cb;(void)arg;}
static inline void udp_remove(struct udp_pcb* pcb){(void)pcb;}
static inline int udp_sendto(struct udp_pcb* pcb, struct pbuf* p, ip_addr_t* dst, uint16_t port){(void)pcb;(void)p;(void)dst;(void)port; return ERR_OK;}
