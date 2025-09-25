#include "board_xcore407i.h"
#include <stdio.h>

/* Stub ULPI (USB HS) initialization gated by BOARD_ENABLE_ULPI.
 * Ethernet RMII (BOARD_ENABLE_ETH) cannot coexist due to PB12/PB13 pin overlap
 * with ULPI_D5/D6 on this board variant. The compile-time #error in
 * board_xcore407i.h prevents simultaneous enable. This file exists so future
 * USB HS bring-up can hook here without disturbing Ethernet-only builds.
 */

#if BOARD_ENABLE_ULPI && !BOARD_ENABLE_ETH
void Board_ULPI_Init(void)
{
    /* TODO: Configure GPIOs for ULPI and perform PHY reset sequencing. */
    printf("[ULPI] Stub init (pins unconfigured)\n");
}
#else
void Board_ULPI_Init(void) { /* no-op */ }
#endif
