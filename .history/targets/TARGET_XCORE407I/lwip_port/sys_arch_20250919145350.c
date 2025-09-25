#include "lwip/arch.h"
#include "lwip/err.h"
/* NO_SYS=1: most sys_arch functions are not used. Provide minimal stubs. */
#if NO_SYS
u32_t sys_now(void){
    /* If you have a millisecond tick, return it; placeholder 0 for now */
    return 0;
}
#endif
