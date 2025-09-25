#include "ethernetif.h"
#include "lwip/opt.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "lwip/sys.h"
#include "board_xcore407i.h"
#include <string.h>

extern ETH_HandleTypeDef heth; /* from ethernet_init.c */

static struct {
    uint32_t rx_ok;
    uint32_t rx_dropped;
    uint32_t rx_alloc_fail;
    uint32_t rx_errors;
    uint32_t link_up_events;
    uint32_t link_down_events;
    uint32_t tx_ok;
    uint32_t tx_err;
} eth_stats;

const void *ethernetif_get_stats(void) { return &eth_stats; }

int ETH_GetMACStats(eth_mac_stats_t *out)
{
    if (!out) return 0;
    out->rx_ok = eth_stats.rx_ok;
    out->rx_dropped = eth_stats.rx_dropped;
    out->rx_alloc_fail = eth_stats.rx_alloc_fail;
    out->rx_errors = eth_stats.rx_errors;
    out->link_up_events = eth_stats.link_up_events;
    out->link_down_events = eth_stats.link_down_events;
    out->tx_ok = eth_stats.tx_ok;
    out->tx_err = eth_stats.tx_err;
    return 1;
}

static int phy_link_state(void)
{
    uint32_t bsr = 0;
    uint16_t addr = ETH_GetPhyAddress();
    if (HAL_ETH_ReadPHYRegister(&heth, addr, PHY_BSR, &bsr) != HAL_OK) return 0;
    return (bsr & PHY_LINKED_STATUS) ? 1 : 0;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    struct pbuf *q;
    uint32_t framelen = 0;
    uint8_t frame[1536];
    for (q = p; q != NULL; q = q->next) {
        if (framelen + q->len > sizeof(frame)) return ERR_IF;
        memcpy(&frame[framelen], q->payload, q->len);
        framelen += q->len;
    }
    ETH_TxPacketConfigTypeDef txConf;
    memset(&txConf, 0, sizeof(txConf));
    ETH_BufferTypeDef txBuf;
    txBuf.buffer = frame;
    txBuf.len = framelen;
    txBuf.next = NULL;
    txConf.Length = framelen;
    txConf.TxBuffer = &txBuf;
    txConf.Attributes = 0; /* default: software checksums */
#if ETH_HW_CHECKSUM
#ifdef ETH_TX_PACKETS_FEATURES_CSUM
    txConf.Attributes |= ETH_TX_PACKETS_FEATURES_CSUM; /* request HW checksum */
#endif
#endif
    if (HAL_ETH_Transmit(&heth, &txConf, 100) != HAL_OK) {
        eth_stats.tx_err++;
        return ERR_IF;
    }
    eth_stats.tx_ok++;
    return ERR_OK;
}

static struct pbuf * low_level_input(struct netif *netif)
{
    (void)netif;
    void *rxBuffer = NULL;
    if (HAL_ETH_ReadData(&heth, &rxBuffer) != HAL_OK) {
        return NULL; /* no frame ready */
    }

    uint32_t len = heth.RxDescList.RxDataLength;
    if (len == 0 || len > 1536) {
        eth_stats.rx_errors++;
        return NULL;
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
    if (!p) {
        eth_stats.rx_alloc_fail++;
        return NULL;
    }

    uint32_t copied = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        memcpy(q->payload, (uint8_t*)rxBuffer + copied, q->len);
        copied += q->len;
    }
    eth_stats.rx_ok++;
#if ETH_HW_CHECKSUM
    /* Future enhancement: inspect descriptor status bits for checksum errors
     * (if HAL exposes them) and increment rx_errors if hardware flagged issues.
     */
#endif
    return p;
}

static void low_level_init(struct netif *netif)
{
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, heth.Init.MACAddr, 6);
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

err_t ethernetif_init(struct netif *netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    low_level_init(netif);
    return ERR_OK;
}

void ethernetif_poll(struct netif *netif)
{
    static uint32_t last_link_check = 0;
    uint32_t now = sys_now();
    if ((now - last_link_check) >= 500) { // check twice per second
        last_link_check = now;
        int link = phy_link_state();
        if (link && !(netif->flags & NETIF_FLAG_LINK_UP)) {
            netif->flags |= NETIF_FLAG_LINK_UP;
            eth_stats.link_up_events++;
#if ETH_DEBUG_UART
            printf("[ETH] Link UP\n");
#endif
        } else if (!link && (netif->flags & NETIF_FLAG_LINK_UP)) {
            netif->flags &= ~NETIF_FLAG_LINK_UP;
            eth_stats.link_down_events++;
#if ETH_DEBUG_UART
            printf("[ETH] Link DOWN\n");
#endif
        }
    }

    while (1) {
        struct pbuf *p = low_level_input(netif);
        if (!p) break; /* no more frames */
        if (netif->input(p, netif) != ERR_OK) {
            eth_stats.rx_dropped++;
            pbuf_free(p);
        }
    }
    sys_check_timeouts();
}
