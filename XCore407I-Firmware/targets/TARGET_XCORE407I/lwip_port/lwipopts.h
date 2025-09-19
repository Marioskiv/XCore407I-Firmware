#pragma once
/* Minimal lwIP configuration for NO_SYS=1, IPv4, UDP only */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_RAW                        0
#define LWIP_TCP                        0
#define LWIP_UDP                        1
#define LWIP_ICMP                       1
#define LWIP_DHCP                       1
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_NETIF_STATUS_CALLBACK      0
#define LWIP_NETIF_LINK_CALLBACK        0
#define LWIP_NETIF_EXT_STATUS_CALLBACK  0
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

#define PBUF_POOL_SIZE                  8
#define PBUF_POOL_BUFSIZE               1536
#define MEM_SIZE                        (16*1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_SYS_TIMEOUT            4

#define LWIP_NOASSERT                   1

/* Disable stats to reduce footprint */
#define LWIP_STATS                      0
#define LWIP_PROVIDE_ERRNO              1

/* Checksums */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_ICMP             1

/* ARP */
#define ARP_TABLE_SIZE                  4
#define ARP_QUEUEING                    0

/* Timeouts: rely on sys_check_timeouts() call in poll */
#define LWIP_TIMERS                     1
#define LWIP_MPU_COMPATIBLE             0

/* Provide lightweight random (if any) */
#define LWIP_RAND() ((u32_t)rand())
