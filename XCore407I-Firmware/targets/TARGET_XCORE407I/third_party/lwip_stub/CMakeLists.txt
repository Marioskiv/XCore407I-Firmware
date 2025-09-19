add_library(lwip_stub STATIC
  lwip/etharp.c
  lwip/netif.c
)

target_include_directories(lwip_stub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
