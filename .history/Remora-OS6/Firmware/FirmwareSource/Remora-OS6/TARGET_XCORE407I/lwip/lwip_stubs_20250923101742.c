#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4.h"
#include "lwip/inet_chksum.h"

/* Provide minimal weak stubs if checksum/frag/reass not linked due to trimmed sources */

/* ip_data global normally in ip.c when IPv4 enabled */
struct ip_globals ip_data; /* zero init */

/* If fragmentation/reassembly disabled in opts, lwIP shouldn't reference these.
 * Because selected subset references symbols, provide no-op implementations. */
u16_t ip4_frag(struct pbuf *p, struct netif *netif, const ip4_addr_t *dest) { (void)p;(void)netif;(void)dest; return 0; }
void ip_reass_tmr(void) { }
struct pbuf *ip4_reass(struct pbuf *p) { return p; }

/* Checksums: provide wrappers if not compiled */
u16_t inet_chksum(const void *dataptr, u16_t len)
{
  const u8_t *data = (const u8_t*)dataptr; u32_t acc = 0; for (u16_t i=0;i+1<len;i+=2){ acc += (u16_t)((data[i]<<8)|data[i+1]); }
  if (len & 1){ acc += (u16_t)(data[len-1]<<8); }
  while (acc >> 16) acc = (acc & 0xFFFF) + (acc>>16); return (u16_t)~acc; }

u16_t inet_chksum_pbuf(struct pbuf *p)
{ u32_t acc=0; for(struct pbuf *q=p;q;q=q->next){ const u8_t*data=(const u8_t*)q->payload; for(u16_t i=0;i+1<q->len;i+=2){ acc += (u16_t)((data[i]<<8)|data[i+1]); } if(q->len&1){ acc += (u16_t)(data[q->len-1]<<8); } } while(acc>>16) acc=(acc&0xFFFF)+(acc>>16); return (u16_t)~acc; }

u16_t ip_chksum_pseudo(struct pbuf *p, u8_t proto, u16_t proto_len,
                        const ip4_addr_t *src, const ip4_addr_t *dest)
{ (void)proto;(void)proto_len;(void)src;(void)dest; return inet_chksum_pbuf(p); }

/* Provide _kill and _getpid weak stubs to satisfy newlib if not using semihosting */
int _kill(int pid, int sig){ (void)pid; (void)sig; return -1; }
int _getpid(void){ return 1; }