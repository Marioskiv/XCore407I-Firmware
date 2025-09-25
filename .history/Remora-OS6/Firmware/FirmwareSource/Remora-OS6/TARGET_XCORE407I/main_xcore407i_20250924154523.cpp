#include "stm32f4xx_hal.h"
#include <cstring>
#include <cstdio>
#include "board_xcore407i.h"
#include "lwip_port.h"
#include "ethernetif.h" /* new lightweight driver */
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h" /* for ethernet_input */
extern "C" void udp_echo_init(void);
#include "lwip/etharp.h" /* for gratuitous ARP */
#include "ethernet_init.h"
#include "remora_udp.h"
#include "stepgen.h"
#include "encoder.h"
#include "drivers/drivers_init.h"
#if defined(USE_ABSTRACT_DRIVERS) && defined(ENABLE_QEI_DIAG) && (ENABLE_QEI_DIAG==1)
#include "encoder_access.h"
#include "drivers/qei/qeiDriver.h"
extern "C" void qei_diag_poll(uint32_t nowMs, qei_t *q);
#endif
#include "board_xcore407i.h"
#include "jog.h"
#include "homing.h"
#include "config_store.h"
#include "build_id.h"
/* USB CDC */
#include "usb/usbd_cdc_if.h"
/* Low-level helpers */
extern "C" uint8_t USBD_FS_HasSOF(void);
extern "C" void USBD_FS_ForceReconnect(void);

extern "C" void motion_control_io_init(void); // implemented in motion_io.c

extern "C" void SystemClock_Config(void); // provided in system_clock.c
/* MX_ETH_Init declared in ethernet_init.h */
/* USB device handle from usbd_cdc_if.c */
extern "C" USBD_HandleTypeDef hUsbDeviceFS;

static __attribute__((unused)) void ErrorTrap(const char* msg){
    (void)msg;
    while(1){ __asm__("nop"); }
}

/* Jump to STM32 system memory (ROM) DFU bootloader.
 * Preconditions: Ensure all peripherals are quiesced before call.
 */
extern "C" void enter_system_dfu(void) {
    __disable_irq();

    /* Deinit subsystems that might leave clocks/peripherals active */
    HAL_RCC_DeInit();
    HAL_DeInit();

    /* System bootloader base for STM32F407: 0x1FFF0000 */
    constexpr uint32_t BOOT_BASE = 0x1FFF0000UL;
    uint32_t sp = *reinterpret_cast<uint32_t const*>(BOOT_BASE + 0);
    uint32_t rv = *reinterpret_cast<uint32_t const*>(BOOT_BASE + 4);

    /* Set vector table offset just in case (not strictly needed for ROM) */
    SCB->VTOR = BOOT_BASE;

    /* Set MSP to system memory's stack pointer */
    __set_MSP(sp);
    auto entry = reinterpret_cast<void (*)()>(rv);
    __enable_irq(); /* Some ROM loaders expect IRQs enabled */
    entry();
    while(1){ /* Should not return */ }
}

int main(void)
{
    HAL_Init();

    /* Very-early debug: bring up UART at default reset clocks to prove we reached main().
       This uses the same pins (USART3 on PD8/PD9). After SystemClock_Config we re-init UART. */
    Board_DebugUART_Init();
    printf("\n[BOOT] main() entered (pre-clock)\n");

#if BOARD_HAS_USER_LEDS
    /* ---------------------------------------------------------
     * EARLY LED DIAGNOSTIC (before SystemClock_Config):
     * Blink ALL candidate on-board LED pins (PD12..PD15) so we can
     * visually confirm the MCU is executing even if it later hardfaults
     * or the clock setup / Ethernet init fails.
     * --------------------------------------------------------- */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef earlyGpio{};
    earlyGpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; /* cover all Discovery-style LEDs */
    earlyGpio.Mode = GPIO_MODE_OUTPUT_PP;
    earlyGpio.Pull = GPIO_NOPULL;
    earlyGpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &earlyGpio);
    for (int i=0;i<4;i++) { /* shorten when real LEDs absent */
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_SET);
        HAL_Delay(60);
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
        HAL_Delay(60);
    }
#endif

    SystemClock_Config();
    Board_NVIC_Config();
    /* Re-init UART after clocks are configured for correct baud rate */
    Board_DebugUART_Init();
    {
        printf("\n[BOOT] XCore407I Core Board (no EVK base)\n");
        printf("[BOOT] Build: %s\n", build_id_string);
    }

    /* Heartbeat pin init (independent of user LEDs) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef hb{}; hb.Pin = HEARTBEAT_PIN; hb.Mode = GPIO_MODE_OUTPUT_PP; hb.Pull = GPIO_NOPULL; hb.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(HEARTBEAT_PORT, &hb);
    HAL_GPIO_WritePin(HEARTBEAT_PORT, HEARTBEAT_PIN, GPIO_PIN_RESET);

    #if BOARD_HAS_USER_LEDS
    /* Initialize status LED GPIOs early (post clock): RED on during startup */
    GPIO_InitTypeDef gpioInit{};
    gpioInit.Pin = LED_RED_PIN | LED_GREEN_PIN;
    gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_RED_PORT, &gpioInit);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET); /* ensure green off */
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);       /* red on (early init) */
    #endif

    /* Unique build pattern: triple fast GREEN blink */
    #if BOARD_HAS_USER_LEDS
    for (int i=0;i<3;i++) {
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        HAL_Delay(120);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
        HAL_Delay(120);
    }
    #endif

    /* Indicate detected PHY address (addr+1) with GREEN short blinks (e.g. addr=1 -> 2 blinks) */
    #if BOARD_HAS_USER_LEDS
    uint16_t phyAddrTmp = ETH_GetPhyAddress();
    uint16_t blinkCount = (uint16_t)(phyAddrTmp + 1U);
    if (blinkCount > 8) blinkCount = 8; /* safety cap */
    for (uint16_t b = 0; b < blinkCount; ++b) {
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        HAL_Delay(90);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
    }
    #endif

    /* MDIO scan bitmap indication: count how many PHY addresses responded; blink RED that many times (0 => 1 long RED blink) */
    #if BOARD_HAS_USER_LEDS
    {
        uint32_t bitmap = ETH_GetPhyScanBitmap();
        uint8_t count = 0;
        for (uint8_t i = 0; i < 32; ++i) if (bitmap & (1UL << i)) count++;
        if (count == 0) {
            /* One long RED blink to indicate zero PHY responses */
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
            HAL_Delay(600);
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
            HAL_Delay(300);
        } else {
            if (count > 10) count = 10; /* cap diagnostic spam */
            for (uint8_t i=0;i<count;i++) {
                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                HAL_Delay(120);
                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                HAL_Delay(220);
            }
        }
    }
    #endif

    /* USB CDC device init (VCP) */
    MX_USB_DEVICE_Init();
    /* If ο host δεν στέλνει SOF μέσα σε ~1s, κάνε ένα force reconnect */
    {
        uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < 1100) {
            if (USBD_FS_HasSOF()) break;
        }
        if (!USBD_FS_HasSOF()) {
            printf("[USB] No SOF within 1.1s after init, trying ForceReconnect...\n");
            /* 1st attempt: force reconnect */
            USBD_FS_ForceReconnect();
            /* Wait a bit for host */
            uint32_t t1 = HAL_GetTick();
            while ((HAL_GetTick() - t1) < 600) {
                if (USBD_FS_HasSOF()) break;
            }
            if (!USBD_FS_HasSOF()) {
                printf("[USB] Still no SOF, trying USBD Stop/Start + reconnect...\n");
                /* 2nd attempt: USBD Stop/Start + reconnect */
                USBD_Stop(&hUsbDeviceFS);
                HAL_Delay(120);
                USBD_Start(&hUsbDeviceFS);
                HAL_Delay(20);
                USBD_FS_ForceReconnect();
                /* Extended retries: pulse reconnect a few times, up to ~5s total */
                for (int i = 0; i < 5 && !USBD_FS_HasSOF(); ++i) {
                    HAL_Delay(800);
                    if (USBD_FS_HasSOF()) break;
                    USBD_FS_ForceReconnect();
                }
                if (!USBD_FS_HasSOF()) {
                    /* 3rd attempt: full USBD deinit + reinit (descriptor/class/interface) */
                    printf("[USB] No SOF after retries. Performing full USBD reinit...\n");
                    USBD_Stop(&hUsbDeviceFS);
                    HAL_Delay(50);
                    USBD_DeInit(&hUsbDeviceFS);
                    HAL_Delay(150);
                    /* Re-run device stack init */
                    MX_USB_DEVICE_Init();
                    HAL_Delay(100);
                    USBD_FS_ForceReconnect();
                    /* Wait up to ~2s */
                    uint32_t tw = HAL_GetTick();
                    while ((HAL_GetTick() - tw) < 2000) {
                        if (USBD_FS_HasSOF()) break;
                        HAL_Delay(50);
                    }
                    /* Final couple of pulses if still no SOF */
                    if (!USBD_FS_HasSOF()) {
                        for (int i=0; i<2; ++i) {
                            USBD_FS_ForceReconnect();
                            HAL_Delay(600);
                            if (USBD_FS_HasSOF()) break;
                        }
                    }
                }
            }
        }
    }

    /* Bring up Ethernet MAC/PHY then initialize lwIP netif */
    Board_Ethernet_Init();
    MX_LWIP_Init();
    /* Send gratuitous ARP to seed switches/ARP tables */
    struct netif *n_init = lwip_port_netif();
    if (n_init) {
        etharp_gratuitous(n_init);
    }
    udp_echo_init();
    /* keep existing Remora init after lwIP */
    remora_udp_init();
    config_store_init();

    /* Startup network diagnostics */
    {
        uint32_t phyid = ETH_GetPhyId();
        uint16_t phyAddr = ETH_GetPhyAddress();
        uint16_t physts = ETH_GetPHY_PHYSTS();
        bool linkUp = (physts & DP83848_PHYSTS_LINK)!=0;
        bool speed10 = (physts & DP83848_PHYSTS_SPEED10)!=0; /* 1=10M else 100M */
        bool full = (physts & DP83848_PHYSTS_FULLDUP)!=0;
        auto *n = lwip_port_netif();
        const ip4_addr_t *ipa = netif_ip4_addr(n);
        const ip4_addr_t *nma = netif_ip4_netmask(n);
        const ip4_addr_t *gwa = netif_ip4_gw(n);
        printf("[NET] PHY addr=%u id=%04lx:%04lx link=%s %s %s\n",
               (unsigned)phyAddr,
               (unsigned long)((phyid>>16)&0xFFFF), (unsigned long)(phyid & 0xFFFF),
               linkUp?"UP":"DOWN", speed10?"10M":"100M", full?"Full":"Half");
        printf("[NET] IP=%s NM=%s GW=%s\n", ip4addr_ntoa(ipa), ip4addr_ntoa(nma), ip4addr_ntoa(gwa));
    }

    /* Networking + Remora initialized: GREEN steady ON, RED off (unless a fault appears) */
    #if BOARD_HAS_USER_LEDS
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    #endif
    {
        uint32_t phyid = ETH_GetPhyId();
        printf("[ETH] PHY scan bitmap=0x%08lx selected_addr=%u phy_id=%04lx:%04lx\n", (unsigned long)ETH_GetPhyScanBitmap(), (unsigned)ETH_GetPhyAddress(), (unsigned long)((phyid>>16)&0xFFFF), (unsigned long)(phyid&0xFFFF));
    }

    stepgen_init();
    motion_control_io_init();
    jog_init();
    drivers_init();
    config_store_apply();

    uint32_t lastLinkState = 0xFFFFFFFF; /* 0 or 1 once valid */
    uint32_t lastTxTick = HAL_GetTick();
    uint32_t lastBlinkTick = HAL_GetTick();
    uint32_t lastLinkPollTick = HAL_GetTick();
    uint32_t linkGraceStart = HAL_GetTick();
    bool linkGraceExpired = false;
    bool redBlinkOn = false; /* track red fault blink */
    bool linkBlinkOn = false; /* green blink when link down after grace (soft pattern) */
    bool strongAlternate = false; /* strong link-down pattern state */
    uint32_t lastAutoNegRestartTick = 0; /* last AN restart time */
    uint32_t firstLinkDownTick = 0; /* timestamp when link first observed down (for prolonged detection) */
    const uint32_t LINK_POLL_MS = 500; /* poll PHY twice per second */
    const uint32_t LINK_GRACE_MS = 5000; /* 5s grace before indicating link-down */
    const uint32_t LINK_DOWN_BLINK_MS = 800; /* blink period for green when link missing (soft) */
    const uint32_t STRONG_LINK_DOWN_AFTER_MS = 8000; /* after 8s continuous down show alternating pattern */
    const uint32_t STRONG_PATTERN_TOGGLE_MS = 350; /* alternate RED/GREEN cadence */
    const uint32_t AUTONEG_TRIGGER_AFTER_MS = 10000; /* start restarts after 10s down */
    const uint32_t AUTONEG_PERIOD_MS = 5000; /* restart every 5s while still down */
    const uint32_t FORCED_MODE_AFTER_MS = 30000; /* force 100M FD if still down after 30s */
#if ETH_DEBUG_LOOPBACK_AUTO
    const uint32_t LOOPBACK_TEST_AFTER_MS = 45000; /* attempt internal loopback after 45s down */
#endif
    const uint32_t LINK_EVENT_PULSE_MS = 60; /* pulse width for transition pulses */
    uint8_t linkEventPhase = 0; /* 0 idle, 1 first pulse on, 2 gap, 3 second pulse on */
    uint32_t linkEventStartTick = 0;
    bool linkEventIsUp = false; /* track whether event is link-up (green pulses) */
    bool forcedPatternShown = false;
    uint32_t lastStrongToggleTick = HAL_GetTick();
    bool sentGratuitousARP = false;
    /* Speed/Duplex indication state machine (after each link-up) */
    enum { SD_IDLE=0, SD_WAIT_START, SD_GREEN_PULSE_ON, SD_GREEN_GAP, SD_GREEN_SECOND_ON, SD_RED_PULSE_ON, SD_DONE };
    uint8_t sd_state = SD_IDLE;
    uint32_t sd_lastTick = 0;
    uint8_t sd_greenPulsesTarget = 0; /* 1=10M,2=100M */
    uint8_t sd_greenPulsesDone = 0;
    bool sd_halfDuplex = false;
    const uint32_t SD_PULSE_MS = 120;  /* length of each pulse */
    const uint32_t SD_GAP_MS = 140;    /* gap between pulses */

    uint32_t lastLwipService = HAL_GetTick();
    const uint32_t LWIP_SERVICE_INTERVAL_MS = 50; /* periodic for timers (ARP, etc.) */
    uint32_t lastAnyRxActivityTick = HAL_GetTick();
    const uint32_t LINK_IDLE_WARN_MS = 15000; /* 15s of link-up inactivity -> warn */
    const uint32_t LINK_IDLE_RESTART_MS = 30000; /* 30s inactivity -> autoneg restart */
    uint8_t idleWarnIssued = 0;
    uint8_t idleRestartIssued = 0;
    /* DebugUART_PollCommands declared in debug_uart.c; prototype provided via board header include elsewhere if needed */
    while (1)
    {
        if (eth_rx_flag) {
            eth_rx_flag = 0;
            MX_LWIP_Process(); /* ingest pending frames */
            lastAnyRxActivityTick = HAL_GetTick();
        } else {
            uint32_t nowTick = HAL_GetTick();
            if ((nowTick - lastLwipService) >= LWIP_SERVICE_INTERVAL_MS) {
                lastLwipService = nowTick;
                MX_LWIP_Process();
            }
        }
        DebugUART_PollCommands();
        /* Simple heartbeat: toggle selected HEARTBEAT pin every 500ms */
        static uint32_t hbLast = 0; uint32_t hbNow = HAL_GetTick();
        if ((hbNow - hbLast) >= 500) { hbLast = hbNow; HAL_GPIO_TogglePin(HEARTBEAT_PORT, HEARTBEAT_PIN); }
        remora_udp_poll();
        homing_poll();
        if (!homing_is_active()) {
            jog_poll();
        }

#if ETH_DEBUG_TX_SPAM
        static uint32_t lastSpamTick = 0;
        uint32_t nowSpam = HAL_GetTick();
        if (lastLinkState == 1 && (nowSpam - lastSpamTick) >= 1000) {
            extern void lwip_port_debug_broadcast(void);
            lwip_port_debug_broadcast();
            lastSpamTick = nowSpam;
        }
#endif

        /* Periodic feedback TX (every 10ms) */
        uint32_t now = HAL_GetTick();
        if ((now - lastTxTick) >= 10) {
            remora_udp_send_feedback();
            lastTxTick = now;
        }

        /* USB CDC periodic hello (every 1000ms) */
        static uint32_t lastUsbHello = 0;
        if ((now - lastUsbHello) >= 1000) {
            lastUsbHello = now;
            static const char hello[] = "Hello from STM32 via USB CDC!\r\n";
            (void)CDC_Transmit_FS((uint8_t*)hello, (uint16_t)sizeof(hello) - 1);
        }

#if defined(USE_ABSTRACT_DRIVERS) && defined(ENABLE_QEI_DIAG) && (ENABLE_QEI_DIAG==1)
    /* QEI diagnostic (axis 0) prints once per second; lightweight call */
    qei_diag_poll(now, encoder_get_qei(0));
#endif

        /* Periodic PHY link poll */
        if ((now - lastLinkPollTick) >= LINK_POLL_MS) {
            lastLinkPollTick = now;
            uint32_t phyUp = (uint32_t)ETH_AnyLinkUp();
            if (phyUp != lastLinkState) {
                lastLinkState = phyUp;
                if (phyUp) { /* reset inactivity tracking on link transition up */
                    lastAnyRxActivityTick = now;
                    idleWarnIssued = 0; idleRestartIssued = 0;
                }
                linkGraceStart = now; /* reset grace window on any transition */
                linkGraceExpired = false;
                strongAlternate = false;
                firstLinkDownTick = (phyUp ? 0 : now);
                /* Start link event pulse sequence (non-blocking): two short pulses */
                linkEventPhase = 1;
                linkEventStartTick = now;
                linkEventIsUp = (phyUp != 0);
                /* Ensure LEDs in known state */
                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                /* Propagate to lwIP */
                lwip_port_set_link_state(phyUp ? 1 : 0);
                forcedPatternShown = false; /* re-arm forced pattern if link flaps */
                if (phyUp) {
                    /* Sync MAC speed/duplex from PHY and schedule gratuitous ARP */
                    ETH_UpdateMACFromPHY();
                    sentGratuitousARP = false; /* allow one ARP send after stable up */
                    /* Initialize speed/duplex pattern */
                    uint16_t physr = ETH_GetPHY_PHYSTS(); /* legacy accessor -> PHYSR */
                    /* DP83848: SPEED10 bit1 (1=10M, 0=100M); FULLDUP bit2 (1=Full) */
                    uint8_t speed10 = (physr & DP83848_PHYSTS_SPEED10)?1:0;
                    uint8_t full = (physr & DP83848_PHYSTS_FULLDUP)?1:0;
                    uint8_t is100 = speed10 ? 0 : 1;
                    sd_greenPulsesTarget = is100 ? 2 : 1; /* pulses: 1=10M, 2=100M (unchanged semantic) */
                    sd_greenPulsesDone = 0;
                    sd_halfDuplex = full ? false : true;
                    sd_state = SD_WAIT_START;
                    sd_lastTick = now;
#if ETH_DEBUG_UART
                    eth_phy_debug_t dbg; /* requires stdio already initialized */
                    if (ETH_GetPHY_Debug(&dbg)) {
                        printf("[PHY] addr=%u ID=%04x:%04x BCRi=%04x BSRi=%04x PHYi=%04x BCR=%04x BSR=%04x PHYSTS=%04x speed=%s duplex=%s\n",
                               dbg.phy_addr, dbg.id1, dbg.id2, dbg.bcr_initial, dbg.bsr_initial, dbg.physts_initial,
                               dbg.bcr_last, dbg.bsr_last, dbg.physts_last,
                               is100?"100M":"10M", full?"Full":"Half");
                    }
#endif
                }
            } else {
                if (!linkGraceExpired && (now - linkGraceStart) > LINK_GRACE_MS) {
                    linkGraceExpired = true;
                }
                if (lastLinkState == 0) {
                    if (firstLinkDownTick == 0) firstLinkDownTick = now; /* record start */
                }
            }
        }

        /* Link transition pulses (non-blocking) */
        if (linkEventPhase) {
            uint32_t dt = now - linkEventStartTick;
            if (linkEventPhase == 1) {
                /* first pulse ON */
                if (linkEventIsUp) {
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                } else {
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                }
                if (dt >= LINK_EVENT_PULSE_MS) {
                    linkEventPhase = 2; linkEventStartTick = now;
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, linkEventIsUp ? GPIO_PIN_RESET : HAL_GPIO_ReadPin(LED_GREEN_PORT, LED_GREEN_PIN));
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, linkEventIsUp ? HAL_GPIO_ReadPin(LED_RED_PORT, LED_RED_PIN) : GPIO_PIN_RESET);
                }
            } else if (linkEventPhase == 2) {
                /* gap */
                if (dt >= LINK_EVENT_PULSE_MS) {
                    linkEventPhase = 3; linkEventStartTick = now;
                }
            } else if (linkEventPhase == 3) {
                /* second pulse ON */
                if (linkEventIsUp) {
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                } else {
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                }
                if (dt >= LINK_EVENT_PULSE_MS) {
                    /* end sequence */
                    linkEventPhase = 0;
                    if (linkEventIsUp) {
                        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                    } else {
                        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                    }
                }
            }
        }

        /* Link-down indications */
        if (lastLinkState == 0) {
            uint32_t downDuration = (firstLinkDownTick && now >= firstLinkDownTick) ? (now - firstLinkDownTick) : 0;
            bool strongMode = (downDuration >= STRONG_LINK_DOWN_AFTER_MS);
            if (strongMode && !strongAlternate) {
                strongAlternate = true; /* enter strong pattern */
            }

            if (strongAlternate) {
                if ((now - lastStrongToggleTick) >= STRONG_PATTERN_TOGGLE_MS) {
                    lastStrongToggleTick = now;
                    /* Alternate: RED and GREEN opposite states */
                    static bool phase = false;
                    phase = !phase;
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, phase ? GPIO_PIN_SET : GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, phase ? GPIO_PIN_RESET : GPIO_PIN_SET);
                }
            } else if (linkGraceExpired) {
                if ((now - lastBlinkTick) >= LINK_DOWN_BLINK_MS) {
                    lastBlinkTick = now;
                    linkBlinkOn = !linkBlinkOn;
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, linkBlinkOn ? GPIO_PIN_SET : GPIO_PIN_RESET);
                }
            }
        } else if (lastLinkState == 1) {
            /* Ensure GREEN solid on link up; RED off unless fault logic overrides */
            if (sd_state == SD_IDLE || sd_state == SD_DONE) {
                HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
            }
            if (!redBlinkOn) HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
            /* Send one gratuitous ARP shortly after link up (after grace) */
            if (!sentGratuitousARP && linkGraceExpired) {
                struct netif* nif = lwip_port_netif();
                if (nif) {
                    etharp_gratuitous(nif);
                }
                sentGratuitousARP = true;
            }
            /* Link inactivity watchdog (RX silence) */
            if (linkGraceExpired) {
                uint32_t idleMs = now - lastAnyRxActivityTick;
                if (!idleWarnIssued && idleMs >= LINK_IDLE_WARN_MS) {
                    idleWarnIssued = 1;
                    printf("[ETH][WARN] No RX frames for %lu ms while link up\n", (unsigned long)idleMs);
                }
                if (!idleRestartIssued && idleMs >= LINK_IDLE_RESTART_MS) {
                    idleRestartIssued = 1;
                    printf("[ETH][ACT] Inactivity restart autoneg\n");
                    ETH_RestartAutoNeg();
                }
            }
        }

        /* Speed/Duplex LED pattern state machine */
        if (lastLinkState == 1) {
            /* Optional extra gratuitous ARP spam for debugging visibility */
#if ETH_DEBUG_EXTRA_ARP
            static uint32_t lastExtraArp = 0;
            if ((now - lastExtraArp) >= 2000 && linkGraceExpired) {
                struct netif* nif = lwip_port_netif();
                if (nif) etharp_gratuitous(nif);
                lastExtraArp = now;
            }
#endif
            if (sd_state == SD_WAIT_START) {
                if ((now - sd_lastTick) >= 400) { /* short delay after link event pulses */
                    sd_state = SD_GREEN_PULSE_ON;
                    sd_lastTick = now;
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                }
            } else if (sd_state == SD_GREEN_PULSE_ON) {
                if ((now - sd_lastTick) >= SD_PULSE_MS) {
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
                    sd_lastTick = now;
                    sd_state = SD_GREEN_GAP;
                }
            } else if (sd_state == SD_GREEN_GAP) {
                if ((now - sd_lastTick) >= SD_GAP_MS) {
                    sd_greenPulsesDone++;
                    if (sd_greenPulsesDone < sd_greenPulsesTarget) {
                        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                        sd_lastTick = now;
                        sd_state = SD_GREEN_PULSE_ON; /* next pulse */
                    } else {
                        if (sd_halfDuplex) {
                            /* Half duplex => single red pulse */
                            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                            sd_lastTick = now;
                            sd_state = SD_RED_PULSE_ON;
                        } else {
                            sd_state = SD_DONE;
                            HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                        }
                    }
                }
            } else if (sd_state == SD_RED_PULSE_ON) {
                if ((now - sd_lastTick) >= SD_PULSE_MS) {
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET); /* end in steady green */
                    sd_state = SD_DONE;
                }
            }
        } else {
            /* If link drops abort pattern */
            if (sd_state != SD_IDLE) sd_state = SD_IDLE;
        }

        /* Periodic auto-negotiation restarts when prolonged link down */
        if (lastLinkState == 0 && firstLinkDownTick && (now - firstLinkDownTick) >= AUTONEG_TRIGGER_AFTER_MS) {
            if ((now - lastAutoNegRestartTick) >= AUTONEG_PERIOD_MS) {
                lastAutoNegRestartTick = now;
                ETH_RestartAutoNeg();
                /* Brief RED pulse (only if not in strong alternate which already uses RED) */
                if (!strongAlternate) {
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                    /* Use non-blocking style: schedule off after next loop */
                }
            }
        }

        /* Force 100M Full Duplex fallback if still no link after threshold */
        if (lastLinkState == 0 && firstLinkDownTick && (now - firstLinkDownTick) >= FORCED_MODE_AFTER_MS && !ETH_IsForcedMode()) {
            if (ETH_Force100Full()) {
                /* Show forced pattern once: 3 quick RED + 1 long GREEN */
                if (!forcedPatternShown) {
                    forcedPatternShown = true;
                    for (int i=0;i<3;i++) {
                        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                        HAL_Delay(90);
                        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                        HAL_Delay(120);
                    }
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                    HAL_Delay(500);
                    /* Restore to appropriate state after pattern */
                    if (lastLinkState == 0) {
                        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
                    }
                }
            }
        }

#if ETH_DEBUG_LOOPBACK_AUTO
        /* Internal loopback escalation */
        if (lastLinkState == 0 && firstLinkDownTick && (now - firstLinkDownTick) >= LOOPBACK_TEST_AFTER_MS) {
            static uint8_t loopTried = 0;
            if (!loopTried) {
                loopTried = 1;
                int ok = ETH_RunInternalLoopbackTest();
#if ETH_DEBUG_UART
                printf("[LOOPBACK] %s\n", ok?"PASS":"FAIL");
#endif
                if (ok) {
                    /* LED pattern: 5 quick GREEN blinks */
                    for (int i=0;i<5;i++){HAL_GPIO_WritePin(LED_GREEN_PORT,LED_GREEN_PIN,GPIO_PIN_SET);HAL_Delay(80);HAL_GPIO_WritePin(LED_GREEN_PORT,LED_GREEN_PIN,GPIO_PIN_RESET);HAL_Delay(120);}                }
                else {
                    /* LED pattern: 5 quick RED blinks */
                    for (int i=0;i<5;i++){HAL_GPIO_WritePin(LED_RED_PORT,LED_RED_PIN,GPIO_PIN_SET);HAL_Delay(80);HAL_GPIO_WritePin(LED_RED_PORT,LED_RED_PIN,GPIO_PIN_RESET);HAL_Delay(120);}                }
            }
        }
#endif

/* Lightweight periodic PHY logging (separate from heavy monitor) */
#if ETH_DEBUG_PERIODIC_PHY_LOG
    ETH_PeriodicPHYLog(now);
#endif

#if ETH_DEBUG_PHY_MON
        /* Periodic register monitor (1 Hz) plus fast visual link pulse: GREEN short if PHYSTS link=1 else RED short */
        {
            static uint32_t lastMon = 0;
            static uint32_t lastPulse = 0;
            if ((now - lastMon) >= 1000) {
                lastMon = now;
                extern void ETH_DebugDumpOnce(void);
                ETH_DebugDumpOnce();
            }
            if ((now - lastPulse) >= 300) {
                lastPulse = now;
                uint16_t ph = ETH_GetPHY_PHYSTS();
                if (ph & 0x1) { /* link bit */
                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                    HAL_Delay(20);
                    if (lastLinkState != 1 || sd_state != SD_IDLE) {/* leave pattern control if active */}
                    else HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                } else {
                    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
                    HAL_Delay(20);
                    if (!redBlinkOn && !strongAlternate) HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
                }
            }
        }
#endif

        /* Fault / estop indication: blink RED at 250ms period if any fault/estop active */
        uint32_t faults = motion_get_fault_mask();
        uint32_t estop = motion_get_estop();
        if (faults || estop) {
            if ((now - lastBlinkTick) >= 250) {
                lastBlinkTick = now;
                redBlinkOn = !redBlinkOn;
                HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, redBlinkOn ? GPIO_PIN_SET : GPIO_PIN_RESET);
            }
        } else if (!strongAlternate) { /* in strongAlternate, RED is driven by link pattern */
            HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
            redBlinkOn = false;
        }

#if ETH_DEBUG_UART
        /* Periodic MDIO stats print (every 5s) to monitor reliability */
        {
            static uint32_t lastMdioPrint = 0;
            if ((now - lastMdioPrint) >= 5000) {
                lastMdioPrint = now;
                eth_mdio_stats_t stats; if (ETH_GetMDIOStats(&stats)) {
                    printf("[MDIO] rd_ok=%lu rd_fail=%lu rd_retry=%lu wr_ok=%lu wr_fail=%lu wr_retry=%lu\n",
                           (unsigned long)stats.rd_ok, (unsigned long)stats.rd_fail, (unsigned long)stats.rd_retry,
                           (unsigned long)stats.wr_ok, (unsigned long)stats.wr_fail, (unsigned long)stats.wr_retry);
                }
            }
        }
#endif
        if (ENABLE_IDLE_WFI) { __WFI(); }
    }
}
