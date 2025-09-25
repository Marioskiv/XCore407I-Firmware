/* lwIP requires opt.h included first for proper feature selection */
#include "lwip/opt.h"
#include "board_xcore407i.h" /* ensure ETH_DEBUG_* macros visible (e.g., ETH_DEBUG_TX_SPAM) */
#include <string.h> /* memset, memcpy */
#ifndef LWIP_DHCP
#error "LWIP_DHCP must be defined in lwipopts.h before including dhcp.h"
#endif
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h" /* for ethernet_input */
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#ifndef DHCP_FORWARD_DECLS
#define DHCP_FORWARD_DECLS 1
err_t dhcp_start(struct netif *netif);
u8_t dhcp_supplied_address(const struct netif *netif);
void dhcp_stop(struct netif *netif);
#endif
#include "lwip/ip4_addr.h"
#include "ethernetif.h"
#include "config_store.h"

static uint8_t g_dhcp_active = 0; /* 1 if DHCP lease acquired */
static uint8_t g_dhcp_attempt = 0; /* 1 while attempting */
static uint32_t g_dhcp_start_tick = 0;
static const uint32_t DHCP_TIMEOUT_MS = 5000; /* 5s fallback requirement */

static struct netif g_netif;

/* Static IPv4 configuration */
#define LWIP_IP_ADDR0 192
#define LWIP_IP_ADDR1 168
#define LWIP_IP_ADDR2 2
#define LWIP_IP_ADDR3 50
 
#define LWIP_NETMASK_ADDR0 255
#define LWIP_NETMASK_ADDR1 255
#define LWIP_NETMASK_ADDR2 255
#define LWIP_NETMASK_ADDR3 0
 
#define LWIP_GW_ADDR0 192
#define LWIP_GW_ADDR1 168
#define LWIP_GW_ADDR2 2
#define LWIP_GW_ADDR3 1

void lwip_port_init(void)
{
    lwip_init();
    ip4_addr_t ipaddr, netmask, gw;
    config_store_t *cfg = config_store_get();
    /* Start with static config; may be overridden by DHCP */
    if (cfg) {
        ipaddr.addr = htonl(cfg->static_ip);
        netmask.addr = htonl(cfg->static_netmask);
        gw.addr = htonl(cfg->static_gateway);
    } else {
        IP4_ADDR(&ipaddr, LWIP_IP_ADDR0, LWIP_IP_ADDR1, LWIP_IP_ADDR2, LWIP_IP_ADDR3);
        IP4_ADDR(&netmask, LWIP_NETMASK_ADDR0, LWIP_NETMASK_ADDR1, LWIP_NETMASK_ADDR2, LWIP_NETMASK_ADDR3);
        IP4_ADDR(&gw, LWIP_GW_ADDR0, LWIP_GW_ADDR1, LWIP_GW_ADDR2, LWIP_GW_ADDR3);
    }
    netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, ethernet_input);
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    /* Always try DHCP first (even if config store absent); fallback after timeout */
    if (dhcp_start(&g_netif) == ERR_OK) {
        g_dhcp_attempt = 1;
        g_dhcp_start_tick = HAL_GetTick();
#if ETH_DEBUG_UART
        printf("[NET] DHCP started (timeout %lums)\n", (unsigned long)DHCP_TIMEOUT_MS);
#endif
    }
}

static void dhcp_check_timeout(void)
{
    if (!(g_dhcp_attempt && !g_dhcp_active)) return;
    if (dhcp_supplied_address(&g_netif)) {
        g_dhcp_active = 1; g_dhcp_attempt = 0;
#if ETH_DEBUG_UART
        printf("[NET] DHCP success: %s\n", ip4addr_ntoa(netif_ip4_addr(&g_netif)));
#endif
        return;
    }
    uint32_t now = HAL_GetTick();
    if ((now - g_dhcp_start_tick) > DHCP_TIMEOUT_MS) {
        /* Timeout: apply baked-in static fallback if store absent */
        ip4_addr_t ipaddr, netmask, gw;
        config_store_t *cfg = config_store_get();
        if (cfg) {
            ipaddr.addr = htonl(cfg->static_ip);
            netmask.addr = htonl(cfg->static_netmask);
            gw.addr = htonl(cfg->static_gateway);
        } else {
            IP4_ADDR(&ipaddr, LWIP_IP_ADDR0, LWIP_IP_ADDR1, LWIP_IP_ADDR2, LWIP_IP_ADDR3);
            IP4_ADDR(&netmask, LWIP_NETMASK_ADDR0, LWIP_NETMASK_ADDR1, LWIP_NETMASK_ADDR2, LWIP_NETMASK_ADDR3);
            IP4_ADDR(&gw, LWIP_GW_ADDR0, LWIP_GW_ADDR1, LWIP_GW_ADDR2, LWIP_GW_ADDR3);
        }
        netif_set_addr(&g_netif, &ipaddr, &netmask, &gw);
        dhcp_stop(&g_netif);
        g_dhcp_attempt = 0; g_dhcp_active = 0;
#if ETH_DEBUG_UART
        printf("[NET] DHCP timeout, using static %s\n", ip4addr_ntoa(netif_ip4_addr(&g_netif)));
#endif
    }
}

void lwip_port_poll(void)
{
    ethernetif_poll(&g_netif);
    sys_check_timeouts();
    dhcp_check_timeout();
}

struct netif* lwip_port_netif(void)
{
    return &g_netif;
}

#if ETH_DEBUG_TX_SPAM
/* Send a simple raw broadcast Ethernet frame with custom ethertype 0x88B5 for sniffing */
void lwip_port_debug_broadcast(void)
{
    extern ETH_HandleTypeDef heth; /* from ethernet_init.c */
    uint8_t frame[64];
    /* Destination: broadcast */
    memset(frame, 0xFF, 6);
    /* Source: our MAC */
    memcpy(frame + 6, heth.Init.MACAddr, 6);
    /* Ethertype (custom) */
    frame[12] = 0x88; frame[13] = 0xB5;
    /* Payload pattern */
    const char *msg = "PHYDBG"; /* 6 bytes */
    memcpy(frame + 14, msg, 6);
    uint32_t tick = HAL_GetTick();
    frame[20] = (uint8_t)(tick & 0xFF);
    frame[21] = (uint8_t)((tick >> 8) & 0xFF);
    frame[22] = (uint8_t)((tick >> 16) & 0xFF);
    frame[23] = (uint8_t)((tick >> 24) & 0xFF);
    for (int i = 24; i < 60; ++i) frame[i] = 0xA5; /* pad */
    uint32_t length = 60; /* minimum Ethernet frame w/o CRC (handled by MAC) */
    ETH_TxPacketConfigTypeDef txConf; memset(&txConf,0,sizeof(txConf));
    ETH_BufferTypeDef txBuf; memset(&txBuf,0,sizeof(txBuf));
    txBuf.buffer = frame; txBuf.len = length; txBuf.next = NULL;
    txConf.Length = length; txConf.TxBuffer = &txBuf; txConf.Attributes = 0;
    HAL_ETH_Transmit(&heth, &txConf, 10);
}
#endif

uint8_t lwip_port_dhcp_active(void){ return g_dhcp_active; }
uint8_t lwip_port_dhcp_attempting(void){ return g_dhcp_attempt; }

/* CubeMX-style wrapper functions (requested integration) */
void MX_LWIP_Init(void)
{
    lwip_port_init();
}

void MX_LWIP_Process(void)
{
    lwip_port_poll();
}

/* Unified poll used by board main loop (alias for clarity) */
void Net_Poll(uint32_t now_ms)
{
    (void)now_ms; /* currently unused; kept for future timers */
    lwip_port_poll();
}

/* Runtime link state control (called from main after PHY poll) */
void lwip_port_set_link_state(int up)
{
    if (up) {
        if (!netif_is_link_up(&g_netif)) {
            netif_set_link_up(&g_netif);
        }
    } else {
        if (netif_is_link_up(&g_netif)) {
            netif_set_link_down(&g_netif);
        }
    }
}
