# Ethernet working profile: central place to lock known-good defaults.
# Can be included manually with: cmake -C cmake/eth_good_profile.cmake -B build -S .
# Values still can be overridden later on the command line.
set(USE_ABSTRACT_DRIVERS ON CACHE BOOL "Working profile: abstract drivers")
set(ENABLE_QEI_DIAG ON CACHE BOOL "Working profile: qei diagnostics")
set(BOARD_ENABLE_ETH ON CACHE BOOL "Working profile: Ethernet enabled")
set(BOARD_ENABLE_ULPI OFF CACHE BOOL "Working profile: ULPI disabled")
set(BOARD_ENABLE_NAND OFF CACHE BOOL "Working profile: NAND disabled")
# Keep checksum off unless specifically tested
set(ETH_HW_CHECKSUM OFF CACHE BOOL "Working profile: HW checksum off")
# Keep trimmed DFU binary
set(DFU_TRIM_BIN ON CACHE BOOL "Produce trimmed DFU image")
