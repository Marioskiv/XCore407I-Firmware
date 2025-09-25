#include "stm32f4xx_hal.h"
#include "board_xcore407i.h"
#include <string.h>
#include <stdio.h>

/* Some older HAL versions may not expose PHY_BCR define; provide fallback */
#ifndef PHY_BCR
#define PHY_BCR 0x00U /* Basic Control Register */
#endif

#ifdef ETH_IRQn
void ETH_IRQHandler(void)
{
    uint32_t dmasr = ETH->DMASR;
    /* Check Receive status (RS bit 6 in DMASR indicates frame received) */
    if (dmasr & (1U<<6)) {
        eth_rx_flag = 1;
    }
    /* Clear handled interrupt sources by writing 1s */
    ETH->DMASR = dmasr & ((1U<<6) | (1U<<16)); /* clear RS and NIS if set */
}
#endif

/* Loopback test placed AFTER global variables so heth/g_phy_addr are declared */

ETH_HandleTypeDef heth; /* make handle visible externally */
static eth_mdio_stats_t g_mdio_stats = {0}; /* MDIO access statistics */
static uint16_t g_phy_addr = PHY_ADDRESS; /* runtime-detected PHY address */
static uint32_t g_phy_scan_bitmap = 0;    /* bit i set => address i responded with non-zero/non-FFFF ID1 */
static uint8_t g_forced_mode = 0;         /* bit0: forced mode active, bit1: speed100 (1=100,0=10), bit2: fullDuplex */
static uint16_t g_phy_physr = 0;          /* cached last-read PHYSTS (DP83848 reg 0x10) */
static uint32_t g_phy_id_combined = 0;    /* cached ID1:ID2 combined for debug */
static uint16_t g_phy_bcr_initial = 0;    /* initial Basic Control Register value */
static uint16_t g_phy_bsr_initial = 0;    /* initial Basic Status Register value */
static uint16_t g_phy_bcr_last = 0;       /* last refreshed BCR */
static uint16_t g_phy_bsr_last = 0;       /* last refreshed BSR */
static uint16_t g_phy_physts_initial = 0; /* PHYSTS captured early (may be 0 if read before link) */

#if ETH_DEBUG_LOOPBACK_AUTO
int ETH_RunInternalLoopbackTest(void)
{
    uint32_t orig_bcr=0;
    if (HAL_ETH_ReadPHYRegister(&heth, g_phy_addr, PHY_BCR, &orig_bcr) != HAL_OK) return 0;
        // Set checksum mode if needed
        // Checksum mode setting code here
    
    uint32_t bcr = orig_bcr;
    bcr &= ~(1U<<12); /* clear autoneg enable */
    bcr |= (1U<<13) | (1U<<8) | (1U<<14); /* 100M, full, loopback */
    if (HAL_ETH_WritePHYRegister(&heth, g_phy_addr, PHY_BCR, bcr) != HAL_OK) return 0;
        // Set checksum mode if needed
        // Checksum mode setting code here
    
    HAL_Delay(5);
    /* Build test frame */
    uint8_t frame[64];
    memset(frame, 0xFF, 6);
    memcpy(frame+6, heth.Init.MACAddr, 6);
    frame[12]=0x88; frame[13]=0x5A;
    const char *pat="LOOPTEST"; memcpy(frame+14, pat, 8);
    for (int i=22;i<60;i++) frame[i]=0x3C;
    ETH_TxPacketConfigTypeDef txConf; memset(&txConf,0,sizeof(txConf));
    ETH_BufferTypeDef txBuf; memset(&txBuf,0,sizeof(txBuf));
    txBuf.buffer=frame; txBuf.len=60; txBuf.next=NULL;
    txConf.Length=60; txConf.TxBuffer=&txBuf;
    if (HAL_ETH_Transmit(&heth,&txConf,50)!=HAL_OK) { HAL_ETH_WritePHYRegister(&heth, g_phy_addr, PHY_BCR, orig_bcr); return 0; }
    uint32_t start=HAL_GetTick(); int pass=0;
    while ((HAL_GetTick()-start)<120) {
        void *rxBuff=NULL;
            // Set checksum mode if needed
            // Checksum mode setting code here
        
        if (HAL_ETH_ReadData(&heth,&rxBuff)==HAL_OK) {
            uint32_t rxlen=heth.RxDescList.RxDataLength;
            if (rxlen>=22 && ((uint8_t*)rxBuff)[12]==0x88 && ((uint8_t*)rxBuff)[13]==0x5A && memcmp((uint8_t*)rxBuff+14,pat,8)==0) { pass=1; break; }
        }
    }
    HAL_ETH_WritePHYRegister(&heth, g_phy_addr, PHY_BCR, orig_bcr);
    HAL_Delay(2);
        // Set checksum mode if needed
        // Checksum mode setting code here
    
    return pass;
}
#endif

/* DMA descriptor and buffer definitions (use counts from HAL config if defined) */
/* Place descriptor tables in CCMRAM for faster access and to avoid FLASH overlap; linker script maps .ccm_data to CCMRAM */
__attribute__((section(".ccmram"), aligned(4))) static ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
__attribute__((section(".ccmram"), aligned(4))) static ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
static uint8_t Rx_Buff[ETH_RX_DESC_CNT][1524];
// Removed unused Tx_Buff and txIndex; TX path uses stack buffer copy in ethernetif.c

/* Simple rx allocate callback just points to pre-allocated ring buffers */
static void rx_allocate(uint8_t **buff)
{
    // HAL requests a new buffer when rebuilding descriptors
    static uint32_t idx = 0;
    *buff = Rx_Buff[idx];
    idx = (idx + 1) % ETH_RX_DESC_CNT;
}

/* Rx link callback unused for now (single buffer frames) */
static void rx_link(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
    (void)buff; (void)Length;
    *pStart = buff;
    *pEnd = buff + Length;
}

void HAL_ETH_MspInit(ETH_HandleTypeDef* handle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (handle->Instance == ETH) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    /* Some HAL variants separate MAC / TX / RX clocks */
#ifdef __HAL_RCC_ETH_MAC_CLK_ENABLE
    __HAL_RCC_ETH_MAC_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_ETH_MAC_TX_CLK_ENABLE
    __HAL_RCC_ETH_MAC_TX_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_ETH_MAC_RX_CLK_ENABLE
    __HAL_RCC_ETH_MAC_RX_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_ETH_CLK_ENABLE
    __HAL_RCC_ETH_CLK_ENABLE(); /* fallback macro */
#endif

    /* Ensure peripheral reset for clean state */
#ifdef __HAL_RCC_ETH_FORCE_RESET
    __HAL_RCC_ETH_FORCE_RESET();
    HAL_Delay(1);
    __HAL_RCC_ETH_RELEASE_RESET();
#endif

    /* Ensure RMII interface selected (clear MII bit, set RMII) */
#if defined(SYSCFG_PMC_MII_RMII_SEL)
    /* Older device headers define SYSCFG_PMC_MII_RMII_SEL as the RMII select bit */
    SYSCFG->PMC |= SYSCFG_PMC_MII_RMII_SEL;
#else
    /* Fallback: bit 23 for RMII on STM32F4 (reference RM0090) */
    SYSCFG->PMC |= (1U << 23);
#endif

    /* PHY reset handling: only if dedicated reset pin is present */
#if ETH_PHY_HAS_DEDICATED_RESET
    GPIO_InitStruct.Pin = ETH_PHY_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ETH_PHY_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ETH_PHY_RST_PORT, ETH_PHY_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);  /* >=10ms per DP83848I spec */
    HAL_GPIO_WritePin(ETH_PHY_RST_PORT, ETH_PHY_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50);  /* oscillator + strap settle */
    HAL_Delay(5);   /* guard time */
#else
    /* No dedicated reset (RESET_N tied to NRST). Provide settle delay only. */
    HAL_Delay(60);
#endif

        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF11_ETH;

        /* PA1 REF_CLK, PA2 MDIO, PA7 CRS_DV */
        GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB12 TXD0, PB13 TXD1 (board-specific routing) */
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC1 MDC, PC4 RXD0, PC5 RXD1 */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* TX_EN selectable: default PG11 (reference design). Override with -DUSE_PG11_TXEN=0 to use PB11. */
#if USE_PG11_TXEN
    __HAL_RCC_GPIOG_CLK_ENABLE(); /* ensure clock for PG11 if not already */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
#else
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif

#if ETH_PHY_INT_PRESENT
    /* Configure optional PHY INT pin as input pull-up (no EXTI yet). */
    GPIO_InitTypeDef gi_int = {0};
    gi_int.Pin = ETH_PHY_INT_PIN;
    gi_int.Mode = GPIO_MODE_INPUT;
    gi_int.Pull = GPIO_PULLUP;
    gi_int.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ETH_PHY_INT_PORT, &gi_int);
#endif
    }
}

/* Snapshot of first N PHY registers captured early for debug */
static uint16_t g_phy_reg_snapshot[8];
volatile uint8_t eth_rx_flag = 0; /* set in ETH IRQ when frame received */

/* MDIO helper with simple retry/backoff */
static HAL_StatusTypeDef mdio_read(uint16_t phy_addr, uint16_t reg, uint32_t *pval)
{
    const int max_attempts = 3;
    HAL_StatusTypeDef st;
    for (int attempt=0; attempt<max_attempts; ++attempt) {
        st = HAL_ETH_ReadPHYRegister(&heth, phy_addr, reg, pval);
        if (st == HAL_OK) {
            g_mdio_stats.rd_ok++;
            if (attempt) g_mdio_stats.rd_retry += attempt;
            return HAL_OK;
        }
        HAL_Delay(1);
    }
    g_mdio_stats.rd_fail++;
    return st;
}

static HAL_StatusTypeDef mdio_write(uint16_t phy_addr, uint16_t reg, uint32_t val)
{
    const int max_attempts = 3;
    HAL_StatusTypeDef st;
    for (int attempt=0; attempt<max_attempts; ++attempt) {
        st = HAL_ETH_WritePHYRegister(&heth, phy_addr, reg, val);
        if (st == HAL_OK) {
            g_mdio_stats.wr_ok++;
            if (attempt) g_mdio_stats.wr_retry += attempt;
            return HAL_OK;
        }
        HAL_Delay(1);
    }
    g_mdio_stats.wr_fail++;
    return st;
}

int ETH_GetMDIOStats(eth_mdio_stats_t *out_stats)
{
    if (!out_stats) return 0;
    *out_stats = g_mdio_stats;
    return 1;
}

static void phy_scan_and_select(void)
{
    g_phy_scan_bitmap = 0;
    uint16_t first = 0xFFFF;
    for (uint16_t addr = 0; addr < 32; ++addr) {
        uint32_t id1 = 0, id2 = 0;
        if (mdio_read(addr, 0x02, &id1) == HAL_OK &&
            mdio_read(addr, 0x03, &id2) == HAL_OK) {
            if (id1 != 0x0000 && id1 != 0xFFFF) {
                g_phy_scan_bitmap |= (1UL << addr);
                if (first == 0xFFFF) first = addr;
            }
        }
    }
    if (first != 0xFFFF) {
        g_phy_addr = first;
    } else {
        g_phy_addr = PHY_ADDRESS; /* fallback */
    }
}

static void phy_snapshot_base_regs(void)
{
    for (uint8_t r = 0; r < 8; ++r) {
        uint32_t v = 0xFFFF;
        mdio_read(g_phy_addr, r, &v);
        g_phy_reg_snapshot[r] = (uint16_t)(v & 0xFFFF);
    }
}

/* Allow main to store detected PHY IDs (for external debug / LED) */
void ETH_SetDetectedPHY(uint32_t id_combined)
{
    g_phy_id_combined = id_combined;
}

void ETH_GetPhyRegSnapshot(uint16_t *out, uint8_t count)
{
    if (!out) return;
    if (count > 8) count = 8;
    for (uint8_t i=0;i<count;i++) out[i] = g_phy_reg_snapshot[i];
}

uint32_t ETH_GetPhyScanBitmap(void)
{
    return g_phy_scan_bitmap;
}

uint16_t ETH_GetPHY_PHYSTS(void) { return g_phy_physr; } /* legacy name kept */
uint16_t ETH_GetPHY_PHYSR(void)  { return g_phy_physr; }

/* Scan all PHY addresses in bitmap and return 1 if any has link; optionally update active address */
int ETH_AnyLinkUp(void)
{
    uint16_t chosen = 0xFFFF;
    for (uint16_t addr = 0; addr < 32; ++addr) {
        if (g_phy_scan_bitmap & (1UL << addr)) {
            uint32_t bsr1 = 0, bsr2 = 0;
            if (mdio_read(addr, PHY_BSR, &bsr1) == HAL_OK) {
                mdio_read(addr, PHY_BSR, &bsr2);
                uint32_t bsr = bsr2 ? bsr2 : bsr1;
                if (bsr & PHY_LINKED_STATUS) {
                    chosen = addr;
                    break;
                }
            }
        }
    }
    if (chosen != 0xFFFF && chosen != g_phy_addr) {
        g_phy_addr = chosen; /* dynamically switch to linked PHY */
    }
    return (chosen != 0xFFFF) ? 1 : 0;
}

/* Force 100M full duplex (disable autoneg). Returns 1 on write success. */
/* Generic force mode API: speed100=1 for 100M, 0 for 10M (if PHY supports), fullDuplex=1 for full */
int ETH_ForceMode(int speed100, int fullDuplex)
{
    uint32_t bcr = 0;
    if (mdio_read(g_phy_addr, PHY_BCR, &bcr) != HAL_OK) return 0;
    /* Disable autoneg (bit12) */
    bcr &= ~(1U << 12);
    /* Clear speed (bit13) + duplex (bit8) first */
    bcr &= ~((1U<<13) | (1U<<8));
    if (speed100) bcr |= (1U<<13);
    if (fullDuplex) bcr |= (1U<<8);
    if (mdio_write(g_phy_addr, PHY_BCR, bcr) != HAL_OK) return 0;
    g_forced_mode = 0x01 | (speed100 ? 0x02 : 0x00) | (fullDuplex ? 0x04 : 0x00);
    /* Update MACCR immediately to requested mode (even if link down) */
    uint32_t maccr = ETH->MACCR;
    maccr &= ~((uint32_t)(1U<<14) | (1U<<11));
    if (speed100) maccr |= (1U<<14);
    if (fullDuplex) maccr |= (1U<<11);
    ETH->MACCR = maccr; (void)ETH->MACCR; HAL_Delay(1);
    return 1;
}

/* Backwards-compatible helper for 100M full */
int ETH_Force100Full(void)
{
    return ETH_ForceMode(1,1);
}

uint8_t ETH_IsForcedMode(void)
{
    return (g_forced_mode & 0x01) ? 1 : 0;
}

/* Return last forced parameters (only valid if ETH_IsForcedMode()==1) */
void ETH_GetForcedParams(uint8_t *speed100, uint8_t *fullDuplex)
{
    if (speed100) *speed100 = (g_forced_mode & 0x02) ? 1 : 0;
    if (fullDuplex) *fullDuplex = (g_forced_mode & 0x04) ? 1 : 0;
}

/* Re-enable autonegotiation (clears forced mode state) */
int ETH_SetAuto(void)
{
    uint32_t bcr=0;
    if (mdio_read(g_phy_addr, PHY_BCR, &bcr) != HAL_OK) return 0;
    bcr |= (1U<<12); /* autoneg enable */
    bcr |= (1U<<9);  /* autoneg restart */
    if (mdio_write(g_phy_addr, PHY_BCR, bcr) != HAL_OK) return 0;
    g_forced_mode = 0; /* clear forced flags */
    return 1;
}

/* Clear MDIO statistics counters */
void ETH_ClearMDIOStats(void)
{
    memset(&g_mdio_stats, 0, sizeof(g_mdio_stats));
}

void MX_ETH_Init(void)
{
    memset(&heth, 0, sizeof(heth));
    heth.Instance = ETH;

    /* Perform early PHY address scan before full HAL init so we pass correct PhyAddress */
    phy_scan_and_select();
    heth.Init.MACAddr[0] = 0x02; /* locally administered OUI */
    heth.Init.MACAddr[1] = 0x00;
    heth.Init.MACAddr[2] = 0x00;
    heth.Init.MACAddr[3] = 0x00;
    heth.Init.MACAddr[4] = 0x00;
    heth.Init.MACAddr[5] = 0x01;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.RxDesc = (ETH_DMADescTypeDef*)0; /* will be set after allocation */
    heth.Init.TxDesc = (ETH_DMADescTypeDef*)0;
    heth.Init.RxBuffLen = 1524; /* full frame space */

    heth.Init.RxDesc = (void*)0; /* placeholder to silence warnings if any */
    heth.Init.RxDesc = NULL; /* ensure NULL before HAL init sets list */

    /* Configure checksum offload prior to HAL init if supported */
#if ETH_HW_CHECKSUM
    heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
#endif
    if (HAL_ETH_Init(&heth) != HAL_OK) {
        while(1){ __NOP(); }
    }

#if (USE_HAL_ETH_REGISTER_CALLBACKS == 1)
    HAL_ETH_RegisterRxAllocateCallback(&heth, rx_allocate);
    HAL_ETH_RegisterRxLinkCallback(&heth, rx_link);
#else
    heth.rxAllocateCallback = rx_allocate;
    heth.rxLinkCallback = rx_link;
#endif

    /* Assign descriptor tables (already statically allocated above) */
    heth.Init.RxDesc = NULL; /* HAL keeps internal list; just ensure our buffers are linked */
    /* Reference descriptor arrays to avoid unused warnings if HAL variant doesn't touch them directly */
    (void)DMARxDscrTab; (void)DMATxDscrTab;

    /* (Re)apply checksum mode after init if needed (some HAL versions may overwrite) */
#if ETH_HW_CHECKSUM
    heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
#endif

    HAL_ETH_Start(&heth);

    ETH->DMAIER |= (1U<<16) | (1U<<6);
#if ETH_DEBUG_PROMISCUOUS
    ETH->MACFFR |= (1U << 0);
#endif

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 100) { __NOP(); }

    uint32_t phy_id1 = 0xFFFF, phy_id2 = 0xFFFF;
    mdio_read(g_phy_addr, 0x02, &phy_id1);
    mdio_read(g_phy_addr, 0x03, &phy_id2);
    g_phy_id_combined = ((phy_id1 & 0xFFFF) << 16) | (phy_id2 & 0xFFFF);
    phy_snapshot_base_regs();

    /* PHY OUI validation for DP83848I
     * Reference (TI DP83848I):
     *  PHYIDR1 = 0x2000 (TI OUI bits 3..18)
     *  PHYIDR2 = bbbbbbbb rrrrvvvv where upper bits contain continuation of OUI (expected pattern 0x5C90 after including model/rev for many DP83848 variants).
     * We'll apply:
     *   - Exact match on ID1 (0x2000)
     *   - Mask ID2 with 0xFFF0 to ignore revision (lower 4 bits) and compare to 0x5C90.
     */
    uint16_t id1 = (uint16_t)(phy_id1 & 0xFFFF);
    uint16_t id2 = (uint16_t)(phy_id2 & 0xFFFF);
    int phy_id_ok = (id1 == 0x2000) && ((id2 & 0xFFF0U) == 0x5C90U);
#if ETH_DEBUG_UART
    if (!phy_id_ok) {
        printf("[ETH][WARN] PHY ID mismatch: ID1=%04x ID2=%04x (expected 2000 / 5C90x). Continuing anyway.\n", id1, id2);
    } else {
        printf("[ETH] PHY OUI validated (DP83848I)\n");
    }
#endif

    uint32_t tmp = 0;
    if (mdio_read(g_phy_addr, PHY_BCR, &tmp) == HAL_OK) {
        g_phy_bcr_initial = (uint16_t)tmp; g_phy_bcr_last = g_phy_bcr_initial;
    }
    if (mdio_read(g_phy_addr, PHY_BSR, &tmp) == HAL_OK) {
        g_phy_bsr_initial = (uint16_t)tmp; g_phy_bsr_last = g_phy_bsr_initial;
    }
    if (mdio_read(g_phy_addr, DP83848_REG_PHYSTS, &tmp) == HAL_OK) {
        g_phy_physts_initial = (uint16_t)tmp; g_phy_physr = g_phy_physts_initial;
    }

    /* Print PHY identity early */
#if ETH_DEBUG_UART
    printf("[ETH] PHY addr=%u id=%04lx:%04lx initial PHYSTS=%04x\n", (unsigned)g_phy_addr,
           (unsigned long)((g_phy_id_combined>>16)&0xFFFF), (unsigned long)(g_phy_id_combined&0xFFFF),
           (unsigned)g_phy_physts_initial);
#if USE_PG11_TXEN
    printf("[ETH] TX_EN pin: PG11 (USE_PG11_TXEN=1)\n");
#else
    printf("[ETH] TX_EN pin: PB11 (USE_PG11_TXEN=0)\n");
#endif
#endif

    uint32_t bsr = 0;
    if (mdio_read(g_phy_addr, PHY_BSR, &bsr) == HAL_OK) {
        if ((bsr & PHY_LINKED_STATUS) == 0) {
            uint32_t bcr = 0;
            if (mdio_read(g_phy_addr, PHY_BCR, &bcr) == HAL_OK) {
                bcr |= (1U << 9) | (1U << 12);
                mdio_write(g_phy_addr, PHY_BCR, bcr);
            }
        }
    }
}

/* Lightweight board-level wrapper for higher level init sequencing */
void Board_Ethernet_Init(void)
{
    MX_ETH_Init();
    uint32_t start = HAL_GetTick();
    /* Autoneg already (re)started inside MX_ETH_Init if needed; poll PHYSR for up to 3000ms */
    while ((HAL_GetTick() - start) < 3000U) {
        if (ETH_LinkUp()) {
            ETH_UpdateMACFromPHY();
#if ETH_DEBUG_UART
            uint16_t ph = ETH_GetPHY_PHYSTS();
            uint8_t speed10 = (ph & DP83848_PHYSTS_SPEED10)?1:0; /* 1=10M */
            uint8_t full = (ph & DP83848_PHYSTS_FULLDUP)?1:0;
            printf("[ETH] Link up: %s %s\n", speed10?"10M":"100M", full?"FullDuplex":"HalfDuplex");
#endif
            break;
        }
    }
    if (!ETH_LinkUp()) {
#if ETH_DEBUG_UART
        printf("[ETH] Link timeout (no carrier within 3s)\n");
#endif
    }
}


int ETH_LinkUp(void)
{
    uint32_t bsr1 = 0, bsr2 = 0;
    if (mdio_read(g_phy_addr, PHY_BSR, &bsr1) != HAL_OK) { return 0; }
    mdio_read(g_phy_addr, PHY_BSR, &bsr2);
    uint32_t bsr = bsr2 ? bsr2 : bsr1;
    return (bsr & PHY_LINKED_STATUS) ? 1 : 0;
}

void ETH_RestartAutoNeg(void)
{
    uint32_t bcr = 0;
    if (mdio_read(g_phy_addr, PHY_BCR, &bcr) == HAL_OK) {
        bcr |= (1U<<9) | (1U<<12);
        mdio_write(g_phy_addr, PHY_BCR, bcr);
    }
}

uint32_t ETH_GetPhyId(void)
{
    return g_phy_id_combined;
}

uint16_t ETH_GetPhyAddress(void)
{
    return g_phy_addr;
}

void ETH_UpdateMACFromPHY(void)
{
    uint32_t v = 0;
    uint32_t rbcr=0, rbsr=0;
    if (mdio_read(g_phy_addr, PHY_BCR, &rbcr) == HAL_OK) { g_phy_bcr_last = (uint16_t)rbcr; }
    if (mdio_read(g_phy_addr, PHY_BSR, &rbsr) == HAL_OK) { g_phy_bsr_last = (uint16_t)rbsr; }
    if (mdio_read(g_phy_addr, DP83848_REG_PHYSTS, &v) == HAL_OK) {
        g_phy_physr = (uint16_t)(v & 0xFFFF);
        /* DP83848: SPEED10 bit1 (1=10M, 0=100M) */
        uint32_t speed100 = (g_phy_physr & DP83848_PHYSTS_SPEED10) ? 0U : 1U;
        uint32_t fullDuplex = (g_phy_physr & DP83848_PHYSTS_FULLDUP) ? 1U : 0U;
        uint32_t maccr = ETH->MACCR;
        maccr &= ~((uint32_t)(1U << 14) | (1U << 11));
        if (speed100) maccr |= (1U << 14);
        if (fullDuplex) maccr |= (1U << 11);
        ETH->MACCR = maccr;
        (void)ETH->MACCR;
        HAL_Delay(1);
    }
}

int ETH_GetPHY_Debug(eth_phy_debug_t *out)
{
    if (!out) return 0;
    out->phy_addr = g_phy_addr;
    out->id1 = (uint16_t)(g_phy_id_combined >> 16);
    out->id2 = (uint16_t)(g_phy_id_combined & 0xFFFF);
    out->bcr_initial = g_phy_bcr_initial;
    out->bsr_initial = g_phy_bsr_initial;
    out->physts_initial = g_phy_physts_initial;
    out->bcr_last = g_phy_bcr_last;
    out->bsr_last = g_phy_bsr_last;
    out->physts_last = g_phy_physr;
    return 1;
}

#if ETH_DEBUG_PHY_MON
void ETH_DebugDumpOnce(void)
{
    uint32_t bcr=0, bsr=0, ph=0, maccr=ETH->MACCR, macffr=ETH->MACFFR, dmasr=ETH->DMASR, dmaomr=ETH->DMAOMR;
    mdio_read(g_phy_addr, PHY_BCR, &bcr);
    mdio_read(g_phy_addr, PHY_BSR, &bsr);
    mdio_read(g_phy_addr, DP83848_REG_PHYSTS, &ph);
    printf("[PHYM] addr=%u BCR=%04lx BSR=%04lx PHYSTS=%04lx MACCR=%08lx MACFFR=%08lx DMASR=%08lx DMAOMR=%08lx\n",
           (unsigned)g_phy_addr, bcr & 0xFFFF, bsr & 0xFFFF, ph & 0xFFFF,
           maccr, macffr, dmasr, dmaomr);
}
#endif

#if ETH_DEBUG_PERIODIC_PHY_LOG
void ETH_PeriodicPHYLog(uint32_t now_ms)
{
    static uint32_t last = 0;
    if ((now_ms - last) < 1000) return;
    last = now_ms;
    uint32_t bsr=0, ph=0;
    mdio_read(g_phy_addr, PHY_BSR, &bsr);
    mdio_read(g_phy_addr, DP83848_REG_PHYSTS, &ph);
    uint8_t link = (bsr & PHY_LINKED_STATUS) ? 1:0;
    if (link) {
        uint8_t speed10 = (ph & DP83848_PHYSTS_SPEED10) ? 1:0; /* 1=10M */
        uint8_t full = (ph & DP83848_PHYSTS_FULLDUP) ? 1:0;
        printf("[PHY] link=1 %s %s\n", speed10?"10M":"100M", full?"FullDuplex":"HalfDuplex");
    } else {
        printf("[PHY] link=0\n");
    }
}
#endif
