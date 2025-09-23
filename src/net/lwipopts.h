#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define NO_SYS                          1
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0

#define LWIP_IPV4                       1
#define LWIP_IPV6                       0

#define LWIP_UDP                        1
#define LWIP_TCP                        0
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_DHCP                       0
#define LWIP_ARP                        1

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16*1024)

#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                8
#define MEMP_NUM_SYS_TIMEOUT            10

#define PBUF_POOL_SIZE                  12
#define PBUF_POOL_BUFSIZE               1524

#define ARP_TABLE_SIZE                  8
#define ARP_QUEUEING                    0

#define LWIP_NETIF_LINK_CALLBACK        0
#define LWIP_NETIF_STATUS_CALLBACK      0

#define SYS_LIGHTWEIGHT_PROT            0

#define LWIP_STATS                      0

/* Checksum strategy:
 * Hardware checksum offload on STM32F4 MAC is available only when descriptors
 * are prepared with proper control bits (and HAL integration may differ).
 * Current ethernetif path copies payloads and does not set offload flags, so we
 * retain full software checksum generation & checking for robustness. If a later
 * optimization enables zero-copy with offload, these can be selectively disabled.
 */
#if ETH_HW_CHECKSUM
/* Hardware checksum offload active: disable software generation & checking */
#define CHECKSUM_GEN_IP                 0
#define CHECKSUM_GEN_UDP                0
#define CHECKSUM_GEN_ICMP               0
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_ICMP             0
#define LWIP_CHECKSUM_ON_COPY           0
#else
/* Pure software checksum path */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_ICMP             1
#define LWIP_CHECKSUM_ON_COPY           0
#endif

#endif /* __LWIPOPTS_H__ */
