#include "drivers_init.h"
#include "board_xcore407i.h"
#include "encoder.h"

/* Future: add pin driver pre-config or comms abstraction init. */

void drivers_init(void)
{
    encoder_init();
}
