#pragma once

/* Minimal shim for ethernetif.c when full Remora board header is not present.
 * Provides ETH_GetPhyAddress() used to read PHY registers via HAL.
 * If your PHY strap differs, change the returned address accordingly.
 */

#ifndef ETH_PHY_ADDRESS
#define ETH_PHY_ADDRESS 0x01 /* DP83848I default/strap address on XCore407I */
#endif

static inline unsigned ETH_GetPhyAddress(void) {
    return (unsigned)ETH_PHY_ADDRESS;
}
