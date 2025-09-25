#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_STORE_VERSION 1u
#define CONFIG_MAGIC 0xC0F16765u

#define CONFIG_MAX_AXES 4

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t length; /* bytes of struct through crc (exclusive) */
    /* Persisted parameters */
    int32_t homing_offsets[CONFIG_MAX_AXES];
    uint32_t jog_target_speed[CONFIG_MAX_AXES];
    uint32_t jog_accel[CONFIG_MAX_AXES];
    uint32_t static_ip;      /* network order IPv4 */
    uint32_t static_netmask; /* network order IPv4 */
    uint32_t static_gateway; /* network order IPv4 */
    uint8_t  dhcp_enable;    /* 1 = try DHCP */
    uint8_t  reserved[7];
    uint32_t crc32;          /* over all bytes before this field */
} config_store_t;

/* Initialize flash mapping and attempt load */
void config_store_init(void);
/* Save current in-memory config to flash; returns 0 ok */
int config_store_save(void);
/* Get mutable reference (thread-unsafe) */
config_store_t *config_store_get(void);
/* Mark data dirty so next SAVE opcode writes */
void config_store_mark_dirty(void);
/* Apply loaded values (jog params, homing offsets) to runtime modules */
void config_store_apply(void);

#ifdef __cplusplus
}
#endif

#endif
