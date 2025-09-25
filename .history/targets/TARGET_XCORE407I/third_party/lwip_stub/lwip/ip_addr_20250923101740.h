#pragma once
#include <stdint.h>
typedef struct { uint32_t addr; } ip4_addr_t; typedef ip4_addr_t ip_addr_t; 
#define IP4_ADDR(addr,a,b,c,d) do{ (addr)->addr = ((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a); }while(0)
#define IPADDR_TYPE_V4 0
#define IP_ANY_TYPE NULL
