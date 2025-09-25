#pragma once
/* Minimal lwIP configuration for baremetal NO_SYS raw API UDP echo */

#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0
#define LWIP_RAW                        1

#define LWIP_IPV4                       1
#define LWIP_IPV6                       0

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (8*1024)

#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_SYS_TIMEOUT            5

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1524

#define LWIP_ARP                        1
#define ARP_TABLE_SIZE                  10
#define ARP_QUEUEING                    0

#define ETHARP_SUPPORT_STATIC_ENTRIES   1

#define LWIP_DHCP                       0
#define LWIP_UDP                        1
#define LWIP_TCP                        0

#define IP_REASSEMBLY                   0
#define IP_FRAG                         0

#define LWIP_CHECKSUM_CTRL_PER_NETIF    0

/* Hardware checksum offload coordination.
 * When ETH_HW_CHECKSUM==1 (compile-time flag propagated via CMake) we disable
 * software generation and checking; the STM32 MAC + DMA own checksum duties.
 */
#ifndef ETH_HW_CHECKSUM
#define ETH_HW_CHECKSUM 0
#endif

#if ETH_HW_CHECKSUM
#define CHECKSUM_GEN_IP                 0
#define CHECKSUM_GEN_UDP                0
#define CHECKSUM_GEN_ICMP               0
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_ICMP             0
#else
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_ICMP             1
#endif

#define LWIP_STATS                      0
#define LWIP_PROVIDE_ERRNO              0

#define LWIP_TIMERS                     1
#define LWIP_MPU_COMPATIBLE             1

/* Timing */
#define TCP_TMR_INTERVAL                250

/* Debug options (set to LWIP_DBG_ON to enable) */
#define LWIP_DEBUG                      0

/* Use hardware checksum if HAL ETH configured for it */
#define CHECKSUM_GEN_ETH                0
#define CHECKSUM_CHECK_ETH              0
