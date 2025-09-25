#ifndef BOARD_XCORE407I_H
#define BOARD_XCORE407I_H

#include <stdint.h>
#include "stm32f4xx.h"

/*
 * XCore407I (STM32F407IGT6) Core Board Pin Mapping Summary
 *
 * Clocks:
 *  - HSE 8 MHz crystal
 *  - LSE 32.768 kHz crystal
 *  - SYSCLK 168 MHz via PLL (HSE as source)
 *
 * Ethernet RMII (DP83848I PHY) — current validated mapping:
 *  REF_CLK  PA1
 *  MDIO     PA2
 *  CRS_DV   PA7
 *  MDC      PC1
 *  RXD0     PC4
 *  RXD1     PC5
 *  TXD0     PB12  (board routing)
 *  TXD1     PB13  (board routing)
*  TX_EN    PB11 (schematic). Optional variant: PG11 (set USE_PG11_TXEN=1 for boards wiring TX_EN there).
*  PHY_RST  (none) DP83848I RESET_N tied to MCU NRST on schematic (no independent GPIO control).
*  PHY_INT  INT/PWRDOWN not routed (leave disabled unless board revision adds it).
*  Notes:
*   - `USE_PG11_TXEN` default 0 (PB11). Set to 1 only if your hardware routes TX_EN to PG11.
 *   - MAC speed/duplex derived from DP83848I PHYSTS (0x10) bits.
 *   - If future schematic variants repurpose PG13 for TXD0, choose an alternate reset GPIO.
 *
 * USB HS ULPI (USB3300):
 *  ULPI_D0  PA3
 *  ULPI_D1  PB0
 *  ULPI_D2  PB1
 *  ULPI_D3  PB10
 *  ULPI_D4  PB11
 *  ULPI_D5  PB12 (shared? check design) -> NOTE: TXD0 uses PB12, confirm multiplexing / alt fn
 *  ULPI_D6  PB13 (shared) -> conflicts with TXD1
 *  ULPI_D7  PB5
 *  ULPI_CK  PA5 (60MHz input from PHY)
 *  ULPI_DIR PC2
 *  ULPI_NXT PC3
 *  ULPI_STP PC0
 *  USB_RST  (define pin, TBD)
 *  VBUS_SW  (define pin, TBD)
 *  NOTE: If PB12/PB13 are required for Ethernet RMII TXD0/TXD1 they cannot share ULPI D5/D6.
 *        Board schematic must be clarified. For now prioritise Ethernet; ULPI is disabled by default.
 *        Default TX_EN uses PG11 unless USE_PG11_TXEN=0 forces PB11.
 *
 * NAND Flash (K9F1G08U0C) via FSMC 8-bit:
 *  D0  PD14
 *  D1  PD15
 *  D2  PD0
 *  D3  PD1
 *  D4  PE7
 *  D5  PE8
 *  D6  PE9
 *  D7  PE10
 *  NOE PD4
 *  NWE PD5
 *  CE  PD7
 *  CLE PD12
 *  ALE PD11
 *  R/B PD6
 *  WP  (GPIO pin TBD)
 *
 * Motion Control (final mapping):
 *  Axis0 (X): STEP PB6 (TIM4_CH1 AF2), DIR PB7, ENA PD0, ALM PE0 (EXTI0)
 *  Axis1 (Y): STEP PB8 (TIM4_CH3 AF2), DIR PB9, ENA PD1, ALM PE1 (EXTI1)
 *  Axis2 (Z): STEP PA6 (TIM3_CH1 AF2), DIR PB14, ENA PD2, ALM PE2 (EXTI2)
 *  Axis3 (A): STEP PC6 (TIM8_CH1 AF3), DIR PC7, ENA PD3, ALM PE3 (EXTI3)
 *  NOTE: ENA = Enable output (active high default), ALM = Alarm/Fault input with EXTI interrupt.
 *
 * Encoders (reassigned to avoid PA1/PA7 conflicts):
 *  Enc0: TIM3_CH1 PB4, TIM3_CH2 PB5 (TIM3 alt pins)
 *  Enc1: TIM1_CH1 PA8, TIM1_CH2 PA9
 *  Enc2: TIM2_CH1 PA15, TIM2_CH2 PB3 (remapped channels)
 *  Enc3: (placeholder not wired) returns 0
 *  NOTE: Actual hardware must verify PB4/PB5 availability and AF mapping.
 *
 * Limit Switch Inputs (placeholders) and LEDs etc.
 */

/* -------------------------------------------------------------------------- */
/* Feature toggles (override via -D at compile time)                          */
/* -------------------------------------------------------------------------- */
#ifndef BOARD_ENABLE_ETH
#define BOARD_ENABLE_ETH 1   /* Enable Ethernet RMII (DP83848I) */
#endif
#ifndef BOARD_ENABLE_ULPI
#define BOARD_ENABLE_ULPI 0  /* Enable USB HS ULPI (conflicts with Ethernet TX pins PB12/PB13) */
#endif
#ifndef BOARD_ENABLE_NAND
#define BOARD_ENABLE_NAND 0  /* NAND Flash via FSMC (set to 1 if hardware populated) */
#endif

/* Detect hard pin conflicts (PB12/PB13 shared) */
#if BOARD_ENABLE_ETH && BOARD_ENABLE_ULPI
#error "Ethernet RMII TXD0/TXD1 (PB12/PB13) conflict with ULPI_D5/D6. Disable one of BOARD_ENABLE_ETH or BOARD_ENABLE_ULPI."
#endif

/*
 * Board does NOT provide STM32F4-Discovery style user LEDs on PD12..PD15.
 * Present hardware LEDs:
 *  - Power LED (always on when powered)
 *  - USB FS activity LED
 *  - USB HS activity LED
 * Any references to PD12 (GREEN) / PD14 (RED) are legacy placeholders. They are now
 * compiled out via BOARD_HAS_USER_LEDS=0. To add external diagnostic LEDs, wire them
 * to free GPIOs and override macros in a local board extension header.
 */
/* Flag for conditional compilation */
#define BOARD_HAS_USER_LEDS 0

/* Core-board only: no EVK base peripherals (LCD, buttons, buzzer, SD, etc.) */
#ifndef BOARD_CORE_ONLY
#define BOARD_CORE_ONLY 1
#endif

#if BOARD_HAS_USER_LEDS
#define LED_GREEN_PORT GPIOD
#define LED_GREEN_PIN  GPIO_PIN_12
#define LED_RED_PORT   GPIOD
#define LED_RED_PIN    GPIO_PIN_14
#else
/* Define placeholders so existing code compiles; pins not populated on this board. */
#define LED_GREEN_PORT GPIOD
#define LED_GREEN_PIN  GPIO_PIN_12
#define LED_RED_PORT   GPIOD
#define LED_RED_PIN    GPIO_PIN_14
#endif

/* Heartbeat diagnostic GPIO (repurposed from unused pin). Chosen PA0 (simple GPIO) */
#ifndef HEARTBEAT_PORT
#define HEARTBEAT_PORT GPIOA
#endif
#ifndef HEARTBEAT_PIN
#define HEARTBEAT_PIN  GPIO_PIN_0
#endif

/* Optional: enter low-power WFI in main loop when idle */
#ifndef ENABLE_IDLE_WFI
#define ENABLE_IDLE_WFI 0
#endif

/* Optional periodic lightweight PHY log (independent of heavy ETH_DEBUG_PHY_MON) */
#ifndef ETH_DEBUG_PERIODIC_PHY_LOG
#define ETH_DEBUG_PERIODIC_PHY_LOG 1
#endif

/* PHY_ADDRESS provided by stm32f4xx_hal_conf.h; avoid redefining to prevent warnings */

/* -------------------------------------------------------------------------- */
/* Ethernet RMII pin mapping macros (used by low-level init code)             */
/* Provided to centralize mapping should future variants reroute signals.    */
/* -------------------------------------------------------------------------- */
#if BOARD_ENABLE_ETH
#define ETH_REF_CLK_PORT GPIOA
#define ETH_REF_CLK_PIN  GPIO_PIN_1
#define ETH_MDIO_PORT    GPIOA
#define ETH_MDIO_PIN     GPIO_PIN_2
#define ETH_CRS_DV_PORT  GPIOA
#define ETH_CRS_DV_PIN   GPIO_PIN_7
#define ETH_MDC_PORT     GPIOC
#define ETH_MDC_PIN      GPIO_PIN_1
#define ETH_RXD0_PORT    GPIOC
#define ETH_RXD0_PIN     GPIO_PIN_4
#define ETH_RXD1_PORT    GPIOC
#define ETH_RXD1_PIN     GPIO_PIN_5
#define ETH_TXD0_PORT    GPIOB
#define ETH_TXD0_PIN     GPIO_PIN_12
#define ETH_TXD1_PORT    GPIOB
#define ETH_TXD1_PIN     GPIO_PIN_13
#if USE_PG11_TXEN
#define ETH_TX_EN_PORT   GPIOG
#define ETH_TX_EN_PIN    GPIO_PIN_11
#else
#define ETH_TX_EN_PORT   GPIOB
#define ETH_TX_EN_PIN    GPIO_PIN_11
#endif
/* RESET_N tied to NRST: no dedicated reset line available. Provide optional macros if variant adds one. */
#ifndef ETH_PHY_HAS_DEDICATED_RESET
#define ETH_PHY_HAS_DEDICATED_RESET 0
#endif
#if ETH_PHY_HAS_DEDICATED_RESET
#ifndef ETH_PHY_RST_PORT
#define ETH_PHY_RST_PORT GPIOG
#endif
#ifndef ETH_PHY_RST_PIN
#define ETH_PHY_RST_PIN  GPIO_PIN_13
#endif
#endif
/* (Removed duplicate ETH_PHY_HAS_DEDICATED_RESET redefine; single definition above) */
/* Optional PHY interrupt (INT/PWRDOWN). Define ETH_PHY_INT_PRESENT=1 and set port/pin if hardware wired */
#ifndef ETH_PHY_INT_PRESENT
#define ETH_PHY_INT_PRESENT 0
#endif
#if ETH_PHY_INT_PRESENT
#ifndef ETH_PHY_INT_PORT
#define ETH_PHY_INT_PORT GPIOG
#endif
#ifndef ETH_PHY_INT_PIN
#define ETH_PHY_INT_PIN  GPIO_PIN_10
#endif
#endif /* ETH_PHY_INT_PRESENT */
#endif /* BOARD_ENABLE_ETH */

/* Axis indices */
#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_A 3

#define STEPPERS 4

/* Step timers and channels */
#define STEP_TIMER0 TIM4
#define STEP_TIMER1 TIM4
#define STEP_TIMER2 TIM3
#define STEP_TIMER3 TIM8

#define STEP_TIMER0_CH LL_TIM_CHANNEL_CH1
#define STEP_TIMER1_CH LL_TIM_CHANNEL_CH3
#define STEP_TIMER2_CH LL_TIM_CHANNEL_CH1
#define STEP_TIMER3_CH LL_TIM_CHANNEL_CH1

/* STEP pins */
#define X_STEP_PORT GPIOB
#define X_STEP_PIN  GPIO_PIN_6
#define Y_STEP_PORT GPIOB
#define Y_STEP_PIN  GPIO_PIN_8
#define Z_STEP_PORT GPIOA
#define Z_STEP_PIN  GPIO_PIN_6
#define A_STEP_PORT GPIOC
#define A_STEP_PIN  GPIO_PIN_6

/* DIR pins */
#define X_DIR_PORT GPIOB
#define X_DIR_PIN  GPIO_PIN_7
#define Y_DIR_PORT GPIOB
#define Y_DIR_PIN  GPIO_PIN_9
#define Z_DIR_PORT GPIOB
#define Z_DIR_PIN  GPIO_PIN_14
#define A_DIR_PORT GPIOC
#define A_DIR_PIN  GPIO_PIN_7

/* ENA (enable) pins (optionally remapped for FSMC-safe profile) */
#ifndef MOTION_PROFILE_FSMC_SAFE
#define MOTION_PROFILE_FSMC_SAFE 0
#endif

#if MOTION_PROFILE_FSMC_SAFE
/* Remap off PD0..PD3 to avoid FSMC D2/D3 (and keep contiguous header availability)
 * Proposed mapping (override with -D if needed):
 *  X_ENA->PA4, Y_ENA->PA15, Z_ENA->PC8, A_ENA->PC9
 */
#ifndef X_ENA_PORT
#define X_ENA_PORT GPIOA
#endif
#ifndef X_ENA_PIN
#define X_ENA_PIN  GPIO_PIN_4
#endif
#ifndef Y_ENA_PORT
#define Y_ENA_PORT GPIOA
#endif
#ifndef Y_ENA_PIN
#define Y_ENA_PIN  GPIO_PIN_15
#endif
#ifndef Z_ENA_PORT
#define Z_ENA_PORT GPIOC
#endif
#ifndef Z_ENA_PIN
#define Z_ENA_PIN  GPIO_PIN_8
#endif
#ifndef A_ENA_PORT
#define A_ENA_PORT GPIOC
#endif
#ifndef A_ENA_PIN
#define A_ENA_PIN  GPIO_PIN_9
#endif
#else
#define X_ENA_PORT GPIOD
#define X_ENA_PIN  GPIO_PIN_0
#define Y_ENA_PORT GPIOD
#define Y_ENA_PIN  GPIO_PIN_1
#define Z_ENA_PORT GPIOD
#define Z_ENA_PIN  GPIO_PIN_2
#define A_ENA_PORT GPIOD
#define A_ENA_PIN  GPIO_PIN_3
#endif

/* If NAND flash is enabled, PD0/PD1 are used as D2/D3 and conflict with X/Y ENA outputs.
 * For now we generate a build error forcing the developer to remap enable lines (e.g. to PGx).
 */
#if BOARD_ENABLE_NAND && !MOTION_PROFILE_FSMC_SAFE
#warning "BOARD_ENABLE_NAND=1: PD0/PD1 used by FSMC conflict with X/Y_ENA. Define -DMOTION_PROFILE_FSMC_SAFE=1 or override X/Y_ENA."
#endif

/* ALM (alarm/fault) pins (optional remap to expose all on header) */
#if MOTION_PROFILE_FSMC_SAFE
#ifndef X_ALM_PORT
#define X_ALM_PORT GPIOC
#endif
#ifndef X_ALM_PIN
#define X_ALM_PIN  GPIO_PIN_10
#endif
#ifndef Y_ALM_PORT
#define Y_ALM_PORT GPIOC
#endif
#ifndef Y_ALM_PIN
#define Y_ALM_PIN  GPIO_PIN_11
#endif
#ifndef Z_ALM_PORT
#define Z_ALM_PORT GPIOC
#endif
#ifndef Z_ALM_PIN
#define Z_ALM_PIN  GPIO_PIN_12
#endif
#ifndef A_ALM_PORT
#define A_ALM_PORT GPIOD
#endif
#ifndef A_ALM_PIN
#define A_ALM_PIN  GPIO_PIN_8
#endif
#else
#define X_ALM_PORT GPIOE
#define X_ALM_PIN  GPIO_PIN_0
#define Y_ALM_PORT GPIOE
#define Y_ALM_PIN  GPIO_PIN_1
#define Z_ALM_PORT GPIOE
#define Z_ALM_PIN  GPIO_PIN_2
#define A_ALM_PORT GPIOE
#define A_ALM_PIN  GPIO_PIN_3
#endif

/* Encoder timers */
#define ENC_TIMER0 TIM3
#define ENC_TIMER1 TIM1
#define ENC_TIMER2 TIM2
#define ENC_TIMER3 NULL

/* Emergency Stop (active low). Remap to PB2 (header) in safe profile */
#if MOTION_PROFILE_FSMC_SAFE
#define ESTOP_PORT GPIOB
#define ESTOP_PIN  GPIO_PIN_2
#else
#define ESTOP_PORT GPIOE
#define ESTOP_PIN  GPIO_PIN_4
#endif

/* Jog inputs (active low) PF0..PF7: +X,-X,+Y,-Y,+Z,-Z,+A,-A */
#define JOG_PORT GPIOF
#define JOG_X_POS_PIN GPIO_PIN_0
#define JOG_X_NEG_PIN GPIO_PIN_1
#define JOG_Y_POS_PIN GPIO_PIN_2
#define JOG_Y_NEG_PIN GPIO_PIN_3
#define JOG_Z_POS_PIN GPIO_PIN_4
#define JOG_Z_NEG_PIN GPIO_PIN_5
#define JOG_A_POS_PIN GPIO_PIN_6
#define JOG_A_NEG_PIN GPIO_PIN_7

/* Aggregate mask for initialization */
#define JOG_ALL_PINS (JOG_X_POS_PIN|JOG_X_NEG_PIN|JOG_Y_POS_PIN|JOG_Y_NEG_PIN|JOG_Z_POS_PIN|JOG_Z_NEG_PIN|JOG_A_POS_PIN|JOG_A_NEG_PIN)

/* Probe input (active low). Optional move to PC8 if not used elsewhere, else keep internal */
#if MOTION_PROFILE_FSMC_SAFE
#ifndef PROBE_PORT
#define PROBE_PORT GPIOC
#endif
#ifndef PROBE_PIN
#define PROBE_PIN  GPIO_PIN_8
#endif
#else
#define PROBE_PORT GPIOG
#define PROBE_PIN  GPIO_PIN_0
#endif

/* Probe feedback semantics: motion_get_probe() returns 0 = triggered (low), 1 = not triggered (high) */

/* Jog configuration */
#define JOG_FIXED_FREQ 1000  /* steps per second during manual jog */
#define JOG_ACCEL_DEFAULT 5000 /* steps/s^2 */
#define JOG_DEBOUNCE_COUNT 5   /* consecutive polls required to validate input */

/* Remora extended opcodes */
#define REMORA_OPCODE_NOP            0u
#define REMORA_OPCODE_CLEAR_FAULTS   1u
#define REMORA_OPCODE_SET_JOG_SPEED  2u /* value = steps/s */
#define REMORA_OPCODE_SET_JOG_ACCEL  3u /* value = steps/s^2 */
#define REMORA_OPCODE_HOME_AXIS      4u /* value = axis index */
#define REMORA_OPCODE_ABORT_HOMING   5u /* value ignored */
#define REMORA_OPCODE_SAVE_CONFIG    6u /* value ignored */
#define REMORA_OPCODE_LOAD_CONFIG    7u /* value ignored */
#define REMORA_OPCODE_NEGOTIATE_EXT  8u /* value: 1=enable extended telemetry, 0=disable */
#define REMORA_OPCODE_ENTER_DFU      9u /* value ignored: jump to ROM DFU bootloader */

/* Forward declarations for motion fault and estop (jog API moved to jog.h) */
#ifdef __cplusplus
extern "C" {
#endif
uint32_t motion_get_fault_mask(void);
uint32_t motion_get_estop(void);
uint8_t  motion_try_clear_faults(void); /* returns 1 if cleared */
uint32_t motion_get_probe(void); /* 0 = triggered, 1 = not triggered */
#ifdef __cplusplus
}
#endif

/* Board specific grouped GPIO init for motion & safety IO */
#ifdef __cplusplus
extern "C" {
#endif
void Board_MotionGPIO_Init(void);
#ifdef __cplusplus
}
#endif

/* Ethernet diagnostic helper (from ethernet_init.c) */
#ifdef __cplusplus
extern "C" {
#endif
uint32_t ETH_GetPhyId(void);
uint16_t ETH_GetPhyAddress(void);
uint32_t ETH_GetPhyScanBitmap(void);
void ETH_RestartAutoNeg(void);
int ETH_AnyLinkUp(void);
int ETH_Force100Full(void);
int ETH_LinkUp(void); /* forward declaration to avoid implicit usage in Board_Ethernet_Init */
uint8_t ETH_IsForcedMode(void);
int ETH_ForceMode(int speed100, int fullDuplex);
int ETH_SetAuto(void);
void ETH_GetForcedParams(uint8_t *speed100, uint8_t *fullDuplex);
void ETH_ClearMDIOStats(void);
void ETH_UpdateMACFromPHY(void);
uint16_t ETH_GetPHY_PHYSTS(void);
void ETH_SetDetectedPHY(uint32_t id_combined);
int ETH_RunInternalLoopbackTest(void);
void ETH_DebugDumpOnce(void);
void lwip_port_debug_broadcast(void);
/* MAC level stats accessor (implemented in ethernetif.c) */
typedef struct {
	uint32_t rx_ok;
	uint32_t rx_dropped;
	uint32_t rx_alloc_fail;
	uint32_t rx_errors;
	uint32_t link_up_events;
	uint32_t link_down_events;
	uint32_t tx_ok;
	uint32_t tx_err;
} eth_mac_stats_t;
int ETH_GetMACStats(eth_mac_stats_t *out);
struct netif* lwip_port_netif(void); // added for status IP reporting

/* Enable to allow promiscuous reception for debugging ethernet bring-up */
#ifndef ETH_DEBUG_PROMISCUOUS
#define ETH_DEBUG_PROMISCUOUS 1
#endif
/* Optional UART debug of PHY state (0=off, 1=printf on link events) */
/* Enable UART PHY debug prints */
#ifndef ETH_DEBUG_UART
#define ETH_DEBUG_UART 1
#endif
/* Optional periodic raw broadcast test frame emission */
#ifndef ETH_DEBUG_TX_SPAM
#define ETH_DEBUG_TX_SPAM 1
#endif
/* Optional: send repeated gratuitous ARP frames while link up for debugging */
#ifndef ETH_DEBUG_EXTRA_ARP
#define ETH_DEBUG_EXTRA_ARP 1
#endif
/* Periodic PHY/MAC register monitor & LED pulse assist */
#ifndef ETH_DEBUG_PHY_MON
#define ETH_DEBUG_PHY_MON 1
#endif
/* Attempt internal PHY loopback if no external link after timeout */
#ifndef ETH_DEBUG_LOOPBACK_AUTO
#define ETH_DEBUG_LOOPBACK_AUTO 1
#endif

/* DP83848I specific register & bit definitions (replacing prior LAN8720 assumptions)
 * PHYSTS (addr 0x10) bits:
 *  bit0 LINK_STATUS: 1=Link Up
 *  bit1 SPEED_STATUS: 1=10Mbps, 0=100Mbps  (NOTE: inverse of previous LAN8720_SPEED bit usage)
 *  bit2 DUPLEX_STATUS: 1=Full Duplex
 * Additional bits in PHYSTS not currently consumed (MDIX status, polarity etc.).
 */
#ifndef DP83848_REG_PHYSTS
#define DP83848_REG_PHYSTS     0x10U
#endif
#define DP83848_PHYSTS_LINK     (1U<<0)
#define DP83848_PHYSTS_SPEED10  (1U<<1)  /* 1=10M, 0=100M */
#define DP83848_PHYSTS_FULLDUP  (1U<<2)  /* 1=Full duplex */

/* All prior LAN8720-specific code paths migrated; compatibility aliases removed. */

/* Build-time selection of TX_EN pin routing.
 * Many minimal STM32F407 boards wire TX_EN to PG11 (ST reference design). Some variants repurpose PB11.
 * Default to PG11 unless overridden with -DUSE_PG11_TXEN=0 (then PB11 used).
 */
#ifndef USE_PG11_TXEN
#define USE_PG11_TXEN 0
#endif

/* Optional hardware checksum offload enable */
#ifndef ETH_HW_CHECKSUM
#define ETH_HW_CHECKSUM 0
#endif

/*
 * Hardware checksum offload notes:
 *  Set ETH_HW_CHECKSUM to 1 (e.g. via compiler -DETH_HW_CHECKSUM=1) to request
 *  MAC/DM A generation & checking of IP/UDP/ICMP checksums. When enabled:
 *   - heth.Init.ChecksumMode is set to ETH_CHECKSUM_BY_HARDWARE.
 *   - lwipopts.h disables software checksum generation / verification to avoid
 *     double work.
 *  Fallback: With ETH_HW_CHECKSUM==0 all checksums are computed in software.
 */

/* Public PHY debug structure (mirrors ethernet_init.c internal state) */
typedef struct {
	uint16_t phy_addr;
	uint16_t id1;
	uint16_t id2;
	uint16_t bcr_initial;
	uint16_t bsr_initial;
	uint16_t physts_initial;
	uint16_t bcr_last;
	uint16_t bsr_last;
	uint16_t physts_last;
} eth_phy_debug_t;

/* Updated typed accessor */
int ETH_GetPHY_Debug(eth_phy_debug_t *out_struct);
#ifdef __cplusplus
}
#endif

/* MDIO (PHY) access statistics for reliability diagnostics */
typedef struct {
	uint32_t rd_ok;
	uint32_t rd_fail;
	uint32_t rd_retry; /* total extra read attempts beyond first */
	uint32_t wr_ok;
	uint32_t wr_fail;
	uint32_t wr_retry; /* total extra write attempts beyond first */
} eth_mdio_stats_t;

#ifdef __cplusplus
extern "C" {
#endif
int ETH_GetMDIOStats(eth_mdio_stats_t *out_stats);
#ifdef __cplusplus
}
#endif

/* System level NVIC priority configuration (implemented in system_clock.c) */
#ifdef __cplusplus
extern "C" {
#endif
void Board_NVIC_Config(void);

/* Lightweight periodic PHY logger (implemented in ethernet_init.c) */
void ETH_PeriodicPHYLog(uint32_t now_ms);
#ifdef __cplusplus
extern volatile uint8_t eth_rx_flag;
#else
extern volatile uint8_t eth_rx_flag;
#endif
#ifdef __cplusplus
}
#endif

/* Debug UART (USART3 PD8/PD9) */
#ifdef __cplusplus
extern "C" {
#endif
void Board_DebugUART_Init(void);
void Board_Ethernet_Init(void); /* Initializes MAC + PHY (DP83848I) and starts autoneg */
void DebugUART_PollCommands(void); /* periodic polling of UART command interface */
#ifdef __cplusplus
}
#endif

/* -------------------------------------------------------------------------- */
/* Firmware version metadata (version.c)                                      */
/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
const char *FW_VersionString(void);    /* e.g. "v0.1.0" */
const char *FW_BuildInfoString(void);  /* e.g. "2024-06-02 12:34:56 git:abcdef1" */
#ifdef __cplusplus
}
#endif

/* jog.h provides jog_init/jog_poll and per-axis config/telemetry; included where needed */
/* homing.h supplies homing_start/homing_poll */


#endif /* BOARD_XCORE407I_H */
