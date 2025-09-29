#include "stm32f4xx_hal.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "lwipopts.h"
#include "system_stm32f4xx.h"

extern void SystemClock_Config(void);
extern void ethernetif_poll(struct netif*);
extern err_t ethernetif_init(struct netif*);
extern void udp_echo_init(void);

static struct netif gnetif;

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    // Basic GPIO clock enable for future diagnostics
    __HAL_RCC_GPIOB_CLK_ENABLE();

    lwip_init();
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192,168,2,50);
    IP4_ADDR(&netmask, 255,255,255,0);
    IP4_ADDR(&gw, 192,168,2,1);
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, ethernet_input);
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

    udp_echo_init();

    for(;;) {
        ethernetif_poll(&gnetif);
        sys_check_timeouts();
    }
}

// Provide minimal _sbrk if syscalls.c not linked yet
void _init(void) {}
