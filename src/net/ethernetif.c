#include "stm32f4xx_hal.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "board_xcore407i.h" /* for ETH_GetPhyAddress */
#include <string.h>

/* Reuse existing HAL ETH handle and descriptor initialization from ethernet_init.c */
#if defined(__GNUC__)
__attribute__((weak)) ETH_HandleTypeDef heth; /* expected from ethernet_init.c */
#else
extern ETH_HandleTypeDef heth; /* defined in ethernet_init.c */
#endif

/* We only supply minimal wrappers to feed frames to lwIP using existing low-level driver. */

uint16_t ethernetif_phy_read(uint16_t reg)
{
    uint32_t val=0xFFFF;
    uint16_t phyAddr = ETH_GetPhyAddress();
    HAL_ETH_ReadPHYRegister(&heth, phyAddr, reg, &val);
    return (uint16_t)val;
}

uint8_t ethernetif_link_up(void)
{
    uint16_t bsr = ethernetif_phy_read(0x01); /* Basic Status */
    return (uint8_t)((bsr & (1<<2)) ? 1 : 0);
}

static err_t ethernetif_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    /* Simple copy into a temporary stack frame buffer then transmit via HAL
       (existing driver already created descriptor rings). */
    uint8_t frame[1524];
    if (p->tot_len > sizeof(frame)) return ERR_IF;
    uint32_t offset=0;
    for (struct pbuf *q=p; q; q=q->next) { memcpy(frame+offset, q->payload, q->len); offset+=q->len; }
    ETH_TxPacketConfigTypeDef txConf; memset(&txConf,0,sizeof(txConf));
    ETH_BufferTypeDef txBuf; memset(&txBuf,0,sizeof(txBuf));
    txBuf.buffer = frame; txBuf.len = offset; txBuf.next = NULL;
    txConf.Length = offset; txConf.TxBuffer=&txBuf; txConf.Attributes=0; /* CRC added by MAC */
#if ETH_HW_CHECKSUM
    /* Enable hardware checksum insertion for IPv4/TCP/UDP/ICMP frames.
       The STM32 HAL expects ChecksumCtrl flag bits in Attributes; using
       ETH_TX_PACKETS_FEATURES_CSUM (if defined) via HAL macro names. Some
       HAL versions use 'ETH_TX_DESC_CHECKSUM_INSERT' style macros; guard
       with #ifdefs to stay portable. */
#ifdef ETH_TX_PACKETS_FEATURES_CSUM
    txConf.Attributes |= ETH_TX_PACKETS_FEATURES_CSUM;
#endif
#ifdef ETH_CHECKSUM_BY_HARDWARE
    /* heth.Init.ChecksumMode should already be set in ethernet_init.c when ETH_HW_CHECKSUM=1 */
#endif
#endif /* ETH_HW_CHECKSUM */
    if (HAL_ETH_Transmit(&heth,&txConf,10)!=HAL_OK) return ERR_IF;
    return ERR_OK;
}

void ethernetif_poll(struct netif *netif)
{
    (void)netif;
    /* Use HAL_ETH_ReadData in a loop (as already used in loopback test). */
    void *rxBuff=NULL;
    while (HAL_ETH_ReadData(&heth,&rxBuff)==HAL_OK) {
        uint32_t len = heth.RxDescList.RxDataLength;
        if (len==0 || len>1524) continue;
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (!p) continue;
        uint8_t *src = (uint8_t*)rxBuff;
        uint32_t copied=0; for (struct pbuf *q=p; q; q=q->next){ memcpy(q->payload, src+copied, q->len); copied+=q->len; }
        if (netif->input(p, netif)!=ERR_OK) { pbuf_free(p); }
    }
}

err_t ethernetif_init(struct netif *netif)
{
    netif->name[0]='e'; netif->name[1]='n';
    netif->output = etharp_output;
    netif->linkoutput = ethernetif_output;
    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, heth.Init.MACAddr, 6);
    netif->mtu = 1500;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
    if (ethernetif_link_up()) netif_set_link_up(netif); else netif_set_link_down(netif);
    return ERR_OK;
}
