// Lightweight debug UART init and printf retarget for core board without LEDs.
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "board_xcore407i.h" /* for eth_* structs and prototypes */
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip4.h"

#ifndef DEBUG_UART_ENABLE
#define DEBUG_UART_ENABLE 1
#endif

#if DEBUG_UART_ENABLE
static UART_HandleTypeDef g_dbgUart;
static char cmd_buf[32];
static uint8_t cmd_len = 0;

/* Most prototypes provided by board_xcore407i.h now */

void Board_DebugUART_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    GPIO_InitTypeDef gpio;
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9; /* PD8=TX, PD9=RX */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    g_dbgUart.Instance = USART3;
    g_dbgUart.Init.BaudRate   = 115200;
    g_dbgUart.Init.WordLength = UART_WORDLENGTH_8B;
    g_dbgUart.Init.StopBits   = UART_STOPBITS_1;
    g_dbgUart.Init.Parity     = UART_PARITY_NONE;
    g_dbgUart.Init.Mode       = UART_MODE_TX_RX;
    g_dbgUart.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    g_dbgUart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&g_dbgUart);
}

int Board_DebugUART_PutChar(int ch)
{
    if ((ch == '\n')) {
        uint8_t cr = '\r';
        HAL_UART_Transmit(&g_dbgUart, &cr, 1, 10);
    }
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&g_dbgUart, &c, 1, 10);
    return ch;
}

// Minimal _write syscall hook for printf redirection (newlib nano compatible)
/* If system provides weak _write elsewhere, you can enable this redirect by
 * defining DEBUG_UART_OVERRIDE_WRITE=1 at compile time. Default leaves existing
 * syscalls.c implementation intact. */
#if DEBUG_UART_ENABLE && defined(DEBUG_UART_OVERRIDE_WRITE) && (DEBUG_UART_OVERRIDE_WRITE==1)
int _write(int fd, const char *buf, int len) { (void)fd; if(!buf||len<=0) return 0; for(int i=0;i<len;i++){Board_DebugUART_PutChar(buf[i]);} return len; }
#endif

/* Poll UART RX and process simple diagnostic commands.
 * Commands (send followed by \n):
 *  phy?    - dump basic PHY registers and PHYSTS
 *  mdio?   - show MDIO stats
 *  ethrst  - restart autoneg
 *  ethloop - attempt internal loopback test
 *  ethtest - send one debug broadcast frame (if implemented)
 */
void DebugUART_PollCommands(void)
{
    uint8_t ch;
    while (HAL_UART_Receive(&g_dbgUart, &ch, 1, 0) == HAL_OK) {
        if (ch == '\r') continue; /* ignore CR */
        if (ch == '\n') {
            cmd_buf[cmd_len] = 0;
            if (cmd_len > 0) {
                if (strcmp(cmd_buf, "help") == 0) {
                    printf("[CMD] Commands: help, phy?, mdio?, mdioclr, ethrst, ethloop, ethtest, phyid, board?, ethforce, status, version?\n");
                } else if (strcmp(cmd_buf, "phy?") == 0) {
                    eth_phy_debug_t dbg; if (ETH_GetPHY_Debug(&dbg)) {
                        printf("[CMD][PHY] addr=%u ID=%04x:%04x BCR=%04x BSR=%04x PHYSTS=%04x\n", dbg.phy_addr, dbg.id1, dbg.id2, dbg.bcr_last, dbg.bsr_last, dbg.physts_last);
                    }
                } else if (strcmp(cmd_buf, "mdio?") == 0) {
                    eth_mdio_stats_t st; if (ETH_GetMDIOStats(&st)) {
                        printf("[CMD][MDIO] rd_ok=%lu rd_fail=%lu rd_retry=%lu wr_ok=%lu wr_fail=%lu wr_retry=%lu\n", (unsigned long)st.rd_ok, (unsigned long)st.rd_fail, (unsigned long)st.rd_retry, (unsigned long)st.wr_ok, (unsigned long)st.wr_fail, (unsigned long)st.wr_retry);
                    }
                } else if (strcmp(cmd_buf, "ethrst") == 0) {
                    ETH_RestartAutoNeg();
                    printf("[CMD] Auto-negotiation restarted\n");
                } else if (strcmp(cmd_buf, "ethloop") == 0) {
                    int ok = ETH_RunInternalLoopbackTest();
                    printf("[CMD] internal loopback %s\n", ok?"PASS":"FAIL");
                } else if (strcmp(cmd_buf, "ethtest") == 0) {
                    extern void lwip_port_debug_broadcast(void); lwip_port_debug_broadcast();
                    printf("[CMD] broadcast test frame queued\n");
                } else if (strcmp(cmd_buf, "phyid") == 0) {
                    /* Read and print current PHY ID + validation status */
                    eth_phy_debug_t dbg; if (ETH_GetPHY_Debug(&dbg)) {
                        int ok = (dbg.id1 == 0x2000) && ((dbg.id2 & 0xFFF0U) == 0x5C90U);
                        printf("[CMD][PHYID] ID1=%04x ID2=%04x %s\n", dbg.id1, dbg.id2, ok?"(DP83848I)":"(unexpected)");
                    }
                } else if (strcmp(cmd_buf, "mdioclr") == 0) {
                    ETH_ClearMDIOStats();
                    printf("[CMD][MDIO] counters cleared\n");
                } else if (strcmp(cmd_buf, "board?") == 0) {
                    /* Report compile-time board feature toggles and TX_EN selection */
                    int eth = 0; int ulpi = 0; int nand = 0; const char *txen = "?";
#ifdef BOARD_ENABLE_ETH
                    eth = BOARD_ENABLE_ETH;
#endif
#ifdef BOARD_ENABLE_ULPI
                    ulpi = BOARD_ENABLE_ULPI;
#endif
#ifdef BOARD_ENABLE_NAND
                    nand = BOARD_ENABLE_NAND;
#endif
#ifdef USE_PG11_TXEN
                    txen = (USE_PG11_TXEN?"PG11":"PB11");
#else
                    txen = "PB11"; /* fallback if macro undefined */
#endif
                    printf("[BOARD] ETH=%d ULPI=%d NAND=%d TX_EN=%s\n", eth, ulpi, nand, txen);
                } else if (strncmp(cmd_buf, "ethforce", 8) == 0) {
                    const char *arg = cmd_buf + 8;
                    while (*arg == ' ') arg++;
                    if (*arg == 0) {
                        printf("[CMD][ETH] usage: ethforce auto|100f|100h|10f|10h\n");
                    } else if (strcmp(arg, "auto") == 0) {
                        if (ETH_SetAuto()) printf("[CMD][ETH] autoneg enabled\n"); else printf("[CMD][ETH] autoneg enable failed\n");
                    } else {
                        int spd100 = 1; int full = 1; int valid = 1;
                        if (strcmp(arg, "100f") == 0) { spd100=1; full=1; }
                        else if (strcmp(arg, "100h") == 0) { spd100=1; full=0; }
                        else if (strcmp(arg, "10f") == 0) { spd100=0; full=1; }
                        else if (strcmp(arg, "10h") == 0) { spd100=0; full=0; }
                        else valid = 0;
                        if (!valid) {
                            printf("[CMD][ETH] invalid arg; usage: ethforce auto|100f|100h|10f|10h\n");
                        } else {
                            if (ETH_ForceMode(spd100, full)) {
                                printf("[CMD][ETH] forced %s %s\n", spd100?"100M":"10M", full?"FullDuplex":"HalfDuplex");
                            } else {
                                printf("[CMD][ETH] force failed\n");
                            }
                        }
                    }
                } else if (strcmp(cmd_buf, "status") == 0) {
                    /* Summarize link, mode, PHY/MAC counters, IP configuration */
                    uint8_t forced = ETH_IsForcedMode();
                    uint8_t fs=0, fd=0; if (forced) ETH_GetForcedParams(&fs,&fd);
                    eth_mdio_stats_t mst; eth_mac_stats_t macst; eth_phy_debug_t dbg;
                    ETH_GetMDIOStats(&mst);
                    ETH_GetMACStats(&macst);
                    ETH_GetPHY_Debug(&dbg);
                    uint8_t link = (dbg.physts_last & DP83848_PHYSTS_LINK)?1:0;
                    uint8_t ph_speed10 = (dbg.physts_last & DP83848_PHYSTS_SPEED10)?1:0;
                    uint8_t ph_full = (dbg.physts_last & DP83848_PHYSTS_FULLDUP)?1:0;
                    struct netif *n = lwip_port_netif();
                    const ip4_addr_t *ipa = netif_ip4_addr(n);
                    const ip4_addr_t *nma = netif_ip4_netmask(n);
                    const ip4_addr_t *gwa = netif_ip4_gw(n);
                    printf("[STATUS] link=%u phy=%s %s mode=%s", link, ph_speed10?"10M":"100M", ph_full?"Full":"Half", forced?"FORCED":"AUTO");
                    if (forced) printf("(%s %s)", fs?"100M":"10M", fd?"Full":"Half");
                    printf(" ip=%s nm=%s gw=%s", ip4addr_ntoa(ipa), ip4addr_ntoa(nma), ip4addr_ntoa(gwa));
                    printf(" mdio rd_ok=%lu rd_fail=%lu wr_ok=%lu wr_fail=%lu rx_ok=%lu drop=%lu alloc_fail=%lu err=%lu tx_ok=%lu tx_err=%lu up=%lu down=%lu\n",
                           (unsigned long)mst.rd_ok, (unsigned long)mst.rd_fail,
                           (unsigned long)mst.wr_ok, (unsigned long)mst.wr_fail,
                           (unsigned long)macst.rx_ok, (unsigned long)macst.rx_dropped,
                           (unsigned long)macst.rx_alloc_fail, (unsigned long)macst.rx_errors,
                           (unsigned long)macst.tx_ok, (unsigned long)macst.tx_err,
                           (unsigned long)macst.link_up_events, (unsigned long)macst.link_down_events);
                    if (link && macst.rx_ok==0 && macst.tx_ok==0) {
                        printf("[STATUS] Hint: Link is up but no traffic seen. Check host NIC subnet or cabling.\n");
                    }
                } else if (strcmp(cmd_buf, "version?") == 0) {
                    printf("[VER] %s build %s\n", FW_VersionString(), FW_BuildInfoString());
                } else if (strcmp(cmd_buf, "netdump") == 0) {
/* Support both Cube HAL ETH handle (ETH_HandleTypeDef) and legacy descriptor structs */
#ifdef HAL_ETH_MODULE_ENABLED
                    extern ETH_HandleTypeDef heth;
                    printf("[NETDUMP] MAC/DMA Registers\n");
                    printf(" MACCR   = 0x%08lX\n", (unsigned long)ETH->MACCR);
                    printf(" MACFFR  = 0x%08lX\n", (unsigned long)ETH->MACFFR);
                    printf(" MACMIIAR= 0x%08lX\n", (unsigned long)ETH->MACMIIAR);
                    printf(" MACMIIDR= 0x%08lX\n", (unsigned long)ETH->MACMIIDR);
                    printf(" MACSR   = 0x%08lX\n", (unsigned long)ETH->MACSR);
                    printf(" MACPMTCSR=0x%08lX\n", (unsigned long)ETH->MACPMTCSR);
                    printf(" DMAOMR  = 0x%08lX\n", (unsigned long)ETH->DMAOMR);
                    printf(" DMABMR  = 0x%08lX\n", (unsigned long)ETH->DMABMR);
                    printf(" DMASR   = 0x%08lX\n", (unsigned long)ETH->DMASR);
                    printf(" DMAIER  = 0x%08lX\n", (unsigned long)ETH->DMAIER);
                    printf(" DMARDLAR= 0x%08lX\n", (unsigned long)ETH->DMARDLAR);
                    printf(" DMATDLAR= 0x%08lX\n", (unsigned long)ETH->DMATDLAR);
                    ETH_DMADescTypeDef *rx = (ETH_DMADescTypeDef*)heth.Init.RxDesc;
                    ETH_DMADescTypeDef *tx = (ETH_DMADescTypeDef*)heth.Init.TxDesc;
                    if (rx && tx) {
                        printf(" Descriptors (first 4)\n");
                        for (int i=0;i<4;i++) {
                            printf("  RX[%d] D0=0x%08lX D1=0x%08lX D2=0x%08lX D3=0x%08lX\n", i,
                                   (unsigned long)rx[i].DESC0,
                                   (unsigned long)rx[i].DESC1,
                                   (unsigned long)rx[i].DESC2,
                                   (unsigned long)rx[i].DESC3);
                        }
                        for (int i=0;i<4;i++) {
                            printf("  TX[%d] D0=0x%08lX D1=0x%08lX D2=0x%08lX D3=0x%08lX\n", i,
                                   (unsigned long)tx[i].DESC0,
                                   (unsigned long)tx[i].DESC1,
                                   (unsigned long)tx[i].DESC2,
                                   (unsigned long)tx[i].DESC3);
                        }
                    } else {
                        printf(" Descriptor bases NULL (init issue)\n");
                    }
                    if(!(ETH->DMAOMR & ETH_DMAOMR_SR)) printf(" WARN: DMA RX stopped\n");
                    if(!(ETH->DMAOMR & ETH_DMAOMR_ST)) printf(" WARN: DMA TX stopped\n");
                    if(ETH->DMASR & ETH_DMASR_RBUS) printf(" RBUS: RX bus error\n");
                    if(ETH->DMASR & ETH_DMASR_TBUS) printf(" TBUS: TX bus error\n");
                    if(ETH->DMASR & ETH_DMASR_AIS)  printf(" AIS: abnormal interrupt summary\n");
                    if(ETH->DMASR & ETH_DMASR_RPS)  printf(" RPS: RX process stopped\n");
                    if(ETH->DMASR & ETH_DMASR_TPS)  printf(" TPS: TX process stopped\n");
                    printf("[NETDUMP] Done\n");
#else
                    printf("Ethernet disabled in build\n");
#endif
                } else {
                    printf("[CMD] unknown: %s\n", cmd_buf);
                }
            }
            cmd_len = 0;
        } else if (ch == 0x08 || ch == 0x7F) {
            if (cmd_len) cmd_len--; /* backspace */
        } else if (cmd_len < sizeof(cmd_buf)-1) {
            cmd_buf[cmd_len++] = (char)ch;
        }
    }
}

#else
void Board_DebugUART_Init(void) { }
#endif
