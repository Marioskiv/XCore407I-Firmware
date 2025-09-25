#include "config_store.h"
#include "jog.h"
#include "homing.h"
#include "stm32f4xx_hal.h"

/* Simple single-sector storage strategy.
 * Choose a flash sector near end of user flash (example: Sector 11 on 1MB part @ 0x080E0000 size 128KB).
 * Adjust if part differs. For STM32F407IG (1MB) sectors 0-11 available.
 */
#define CFG_FLASH_BASE 0x080E0000u
#define CFG_FLASH_SECTOR FLASH_SECTOR_11

static config_store_t g_cfg;
static int g_loaded = 0;
static int g_dirty = 0;

static uint32_t crc32_calc(const uint8_t *data, uint32_t len){
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i=0;i<len;i++){
        crc ^= data[i];
        for (int b=0;b<8;b++){
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static void flash_unlock(void){ HAL_FLASH_Unlock(); }
static void flash_lock(void){ HAL_FLASH_Lock(); }

static int flash_sector_erase(void){
    FLASH_EraseInitTypeDef ei = {0};
    uint32_t err = 0;
    ei.TypeErase = FLASH_TYPEERASE_SECTORS;
    ei.Sector = CFG_FLASH_SECTOR;
    ei.NbSectors = 1;
    ei.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&ei, &err) != HAL_OK){ return -1; }
    return 0;
}

static int flash_program_words(uint32_t addr, const uint32_t *words, uint32_t count){
    for (uint32_t i=0;i<count;i++){
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, words[i]) != HAL_OK) return -1;
    }
    return 0;
}

static int config_valid(const config_store_t *c){
    if (c->magic != CONFIG_MAGIC) return 0;
    if (c->version != CONFIG_STORE_VERSION) return 0;
    uint32_t expect_len = (uint32_t)((uint8_t*)&c->crc32 - (uint8_t*)c);
    if (c->length != expect_len) return 0;
    uint32_t crc = crc32_calc((const uint8_t*)c, expect_len);
    return (crc == c->crc32);
}

void config_store_init(void){
    /* Attempt to read existing */
    const config_store_t *flash_cfg = (const config_store_t*)CFG_FLASH_BASE;
    if (config_valid(flash_cfg)){
        g_cfg = *flash_cfg;
        g_loaded = 1;
    } else {
        /* Initialize defaults */
        g_cfg.magic = CONFIG_MAGIC;
        g_cfg.version = CONFIG_STORE_VERSION;
        g_cfg.length = (uint32_t)((uint8_t*)&g_cfg.crc32 - (uint8_t*)&g_cfg);
        for (int i=0;i<CONFIG_MAX_AXES;i++){
            g_cfg.homing_offsets[i] = 0;
            g_cfg.jog_target_speed[i] = 2000;
            g_cfg.jog_accel[i] = 4000;
        }
        /* Default static network 192.168.2.50/24 gw 192.168.2.1 (big endian network order) */
        g_cfg.static_ip = (192u<<24)|(168u<<16)|(2u<<8)|50u;
        g_cfg.static_netmask = (255u<<24)|(255u<<16)|(255u<<8)|0u;
        g_cfg.static_gateway = (192u<<24)|(168u<<16)|(2u<<8)|1u;
        g_cfg.dhcp_enable = 1; /* try DHCP by default */
        g_cfg.crc32 = crc32_calc((const uint8_t*)&g_cfg, g_cfg.length);
        g_loaded = 1;
        g_dirty = 1; /* will need save */
    }
}

config_store_t *config_store_get(void){ return &g_cfg; }
void config_store_mark_dirty(void){ g_dirty = 1; }

int config_store_save(void){
    if (!g_loaded) return -1;
    if (!g_dirty) return 0;
    g_cfg.length = (uint32_t)((uint8_t*)&g_cfg.crc32 - (uint8_t*)&g_cfg);
    g_cfg.crc32 = crc32_calc((const uint8_t*)&g_cfg, g_cfg.length);
    flash_unlock();
    if (flash_sector_erase() != 0){ flash_lock(); return -1; }
    int rc = flash_program_words(CFG_FLASH_BASE, (const uint32_t*)&g_cfg, (sizeof(g_cfg)+3)/4);
    flash_lock();
    if (rc == 0) g_dirty = 0;
    return rc;
}

void config_store_apply(void){
    /* Apply jog params */
    for (int i=0;i<CONFIG_MAX_AXES;i++){
        jog_set_speed_axis(i, g_cfg.jog_target_speed[i]);
        jog_set_accel_axis(i, g_cfg.jog_accel[i]);
        homing_set_offset(i, g_cfg.homing_offsets[i]);
    }
    /* Apply homing offsets into homing module internal storage if completed.
     * We expose a weak hook by directly updating internal offsets via homing_get_offset not provided, so extend homing module if needed.
     */
    /* For now, offsets are fetched in feedback; we can seed them by starting in DONE state (not implemented). */
}
