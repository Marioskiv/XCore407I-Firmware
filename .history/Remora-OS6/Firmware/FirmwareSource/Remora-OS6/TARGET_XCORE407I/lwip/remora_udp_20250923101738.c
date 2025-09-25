#include "remora_udp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "remora.h"
#include "stepgen.h"
#include "encoder.h"
#include <string.h>
#include <stdio.h>
#include "board_xcore407i.h" /* for motion_get_fault_mask */
#include "jog.h"
#include "homing.h"
#include "config_store.h"
#include "auth.h"

#define U16_SAFE(x) ((u16_t)(((x) > 0xFFFFu) ? 0xFFFFu : (x)))

extern volatile rxData_t rxData;
extern volatile txData_t txData;

#define REMORA_UDP_PORT 27181
#define REMORA_MAGIC 0x524D5241u /* 'RMRA' */
#define REMORA_PROTOCOL_VERSION 4u
#include "version.h"
#include "stm32f4xx_hal.h"

/* Simple header: magic, sequence, payload length */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t seq;
    uint16_t payloadLen; /* bytes following header */
    uint16_t reserved;
} RemoraUdpHeader;

static struct udp_pcb *remora_pcb;
static uint32_t tx_seq = 0;
static uint32_t rx_seq_last = 0;
static uint32_t rx_seq_gap_count = 0;
static uint32_t heartbeat_counter = 0;
static uint32_t last_rx_tick = 0; /* HAL_GetTick() at last valid command */
static uint8_t  failsafe_active = 0;
/* Extended telemetry counters (appended via negotiated extension later) */
static uint32_t g_crc_error_count = 0;       /* increments on inbound CRC mismatch (future when CRC added to RX) */
static uint32_t g_auth_failure_count = 0;    /* failed / missing auth tag for protected opcode */
static uint32_t g_estop_edge_count = 0;      /* number of rising edges of estop */
static uint32_t g_loop_interval_last = 0;    /* last feedback send delta (ms) */
static uint32_t g_loop_interval_min = 0xFFFFFFFFu; /* min observed */
static uint32_t g_loop_interval_max = 0;     /* max observed */
static uint32_t g_last_feedback_tick = 0;    /* timestamp of last feedback packet */
static uint8_t  g_ext_telemetry_enabled = 1; /* negotiation can disable */
static uint8_t  g_negotiated_once = 0; /* track if client explicitly negotiated */

/* Accessors for other modules */
uint32_t remora_get_auth_failures(void){ return g_auth_failure_count; }
void remora_count_auth_failure(void){ g_auth_failure_count++; }
void remora_count_crc_error(void){ g_crc_error_count++; }
void remora_count_estop_edge(void){ g_estop_edge_count++; }
void remora_disable_ext_telemetry(void){ g_ext_telemetry_enabled = 0; }
void remora_enable_ext_telemetry(void){ g_ext_telemetry_enabled = 1; }
static ip_addr_t last_client_ip;
static u16_t     last_client_port = 0;

struct remora_diag_stats {
    uint32_t last_rx_seq;
    uint32_t seq_gap_events;
};

const struct remora_diag_stats *remora_get_diag(void) {
    static struct remora_diag_stats s; s.last_rx_seq = rx_seq_last; s.seq_gap_events = rx_seq_gap_count; return &s; }

/* On receive: validate header, copy motion commands into rxData structure */
static void remora_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                            const ip_addr_t *addr, u16_t port)
{
    (void)arg; (void)pcb;
    if (!p) return;

    if (p->tot_len < sizeof(RemoraUdpHeader)) { pbuf_free(p); return; }

    RemoraUdpHeader hdr; pbuf_copy_partial(p, &hdr, sizeof(hdr), 0);
    if (hdr.magic != REMORA_MAGIC) { pbuf_free(p); return; }

    if (hdr.seq != rx_seq_last + 1 && rx_seq_last != 0) {
        rx_seq_gap_count++; /* missed or out-of-order */
    }
    rx_seq_last = hdr.seq;

    last_rx_tick = HAL_GetTick();
    if (failsafe_active) {
        /* Clear failsafe once we get a valid packet */
        failsafe_active = 0;
    }

    /* Track client for unicast feedback */
    ip_addr_set(&last_client_ip, addr);
    last_client_port = port;

    size_t offset = sizeof(RemoraUdpHeader);
    size_t remain = (size_t)p->tot_len - offset;

    /* Base command payload expected */
    const size_t needed = sizeof(rxData.jointFreqCmd) + sizeof(rxData.setPoint) + sizeof(rxData.jointEnable) + sizeof(rxData.outputs);
    if (remain < needed) { pbuf_free(p); return; }

    pbuf_copy_partial(p, (void*)rxData.jointFreqCmd, U16_SAFE(sizeof(rxData.jointFreqCmd)), offset); offset += sizeof(rxData.jointFreqCmd);
    pbuf_copy_partial(p, (void*)rxData.setPoint, U16_SAFE(sizeof(rxData.setPoint)), offset); offset += sizeof(rxData.setPoint);
    pbuf_copy_partial(p, (void*)&rxData.jointEnable, U16_SAFE(sizeof(rxData.jointEnable)), offset); offset += sizeof(rxData.jointEnable);
    pbuf_copy_partial(p, (void*)&rxData.outputs, U16_SAFE(sizeof(rxData.outputs)), offset); offset += sizeof(rxData.outputs);

    /* Optional extended opcode (uint32_t) + value (uint32_t) + optional auth tag (8 bytes) if present */
    uint32_t opcode = REMORA_OPCODE_NOP;
    uint32_t opvalue = 0;
    if ((remain - needed) >= (sizeof(uint32_t) * 2)) {
        pbuf_copy_partial(p, &opcode, U16_SAFE(sizeof(uint32_t)), offset); offset += sizeof(uint32_t);
        pbuf_copy_partial(p, &opvalue, U16_SAFE(sizeof(uint32_t)), offset); offset += sizeof(uint32_t);

        int protected = 0;
        switch(opcode) {
            case REMORA_OPCODE_CLEAR_FAULTS:
                protected = 1; break;
            case REMORA_OPCODE_ENTER_DFU:
                protected = 1; break; /* Require auth for DFU jump */
            default: break;
        }

        if (protected) {
            uint8_t tag_buf[AUTH_TAG_LEN];
            if ((remain - needed) >= (sizeof(uint32_t)*2 + AUTH_TAG_LEN)) {
                /* Tag follows opcode+value */
                pbuf_copy_partial(p, tag_buf, U16_SAFE(AUTH_TAG_LEN), offset); offset += AUTH_TAG_LEN;
                /* Compute HMAC over opcode + value (8 bytes) */
                uint8_t calc[AUTH_TAG_LEN];
                uint8_t msg[8]; memcpy(&msg[0], &opcode, 4); memcpy(&msg[4], &opvalue, 4);
                auth_hmac_tag(msg, sizeof(msg), calc);
                uint8_t diff=0; for (size_t i=0;i<AUTH_TAG_LEN;i++) diff |= (uint8_t)(calc[i]^tag_buf[i]);
                if (diff != 0) {
                    remora_count_auth_failure();
                    protected = 2; /* mark as failed */
                }
            } else {
                /* Missing tag */
                remora_count_auth_failure();
                protected = 2; /* failed */
            }
            if (protected == 2) {
                /* Skip executing the protected opcode */
                opcode = REMORA_OPCODE_NOP;
            }
        }

        switch(opcode) {
            case REMORA_OPCODE_CLEAR_FAULTS:
                motion_try_clear_faults();
                break;
            case REMORA_OPCODE_ENTER_DFU:
                /* Jump to ROM DFU bootloader */
                extern void enter_system_dfu(void);
                enter_system_dfu();
                break;
            case REMORA_OPCODE_SET_JOG_SPEED:
                motion_set_jog_speed(opvalue);
                /* Persist new target speed into config (all axes) */
                {
                    config_store_t *c = config_store_get();
                    for (int ax=0; ax<4; ++ax) c->jog_target_speed[ax] = opvalue;
                    config_store_mark_dirty();
                }
                break;
            case REMORA_OPCODE_SET_JOG_ACCEL:
                motion_set_jog_accel(opvalue);
                {
                    config_store_t *c = config_store_get();
                    for (int ax=0; ax<4; ++ax) c->jog_accel[ax] = opvalue;
                    config_store_mark_dirty();
                }
                break;
            case REMORA_OPCODE_HOME_AXIS:
                homing_start((int)opvalue);
                break;
            case REMORA_OPCODE_ABORT_HOMING:
                homing_abort();
                break;
            case REMORA_OPCODE_SAVE_CONFIG:
                config_store_save();
                break;
            case REMORA_OPCODE_LOAD_CONFIG:
                config_store_init();
                config_store_apply();
                break;
            case REMORA_OPCODE_NEGOTIATE_EXT: {
                /* opvalue: bit0 = 1 enable ext, 0 disable ext */
                if (opvalue & 0x1u) remora_enable_ext_telemetry(); else remora_disable_ext_telemetry();
                g_negotiated_once = 1;
                break; }
            default: break; /* ignore unknown */
        }
    }

    rxData.header = (int32_t)hdr.seq;

    for (int axis = 0; axis < JOINTS; ++axis) {
        int32_t freq = rxData.jointFreqCmd[axis];
        uint8_t enableMask = rxData.jointEnable & (1u << axis);
        /* Apply soft limit enforcement */
        extern void motion_apply_limited_step(int axis, int32_t freq, int enable);
        motion_apply_limited_step(axis, freq, enableMask ? 1 : 0);
    }

    pbuf_free(p);
}

/* Prepare and send feedback packet with joint feedback + process variables */
static void remora_udp_send_feedback_unicast(void)
{
    if (!remora_pcb) return;
    if (last_client_port == 0) return; /* No client yet */

    /* Allocate maximum possible size; we'll set payloadLen after serialization */
    uint8_t buffer[512];
    RemoraUdpHeader *hdr = (RemoraUdpHeader*)buffer;
    hdr->magic = REMORA_MAGIC;
    hdr->seq = tx_seq++;
    /* Will be set later after optional extension appended */
    hdr->reserved = (uint16_t)REMORA_PROTOCOL_VERSION;

    for (int axis = 0; axis < JOINTS; ++axis) {
        int32_t raw = encoder_get_count(axis);
        int32_t offset = homing_get_offset(axis);
        txData.jointFeedback[axis] = raw - offset;
    }

    size_t offset = sizeof(RemoraUdpHeader);
    memcpy(&buffer[offset], (const void*)txData.jointFeedback, sizeof(txData.jointFeedback)); offset += sizeof(txData.jointFeedback);
    memcpy(&buffer[offset], (const void*)txData.processVariable, sizeof(txData.processVariable)); offset += sizeof(txData.processVariable);
    memcpy(&buffer[offset], (const void*)&txData.inputs, sizeof(txData.inputs)); offset += sizeof(txData.inputs);
    uint32_t faultMask = motion_get_fault_mask();
    memcpy(&buffer[offset], &faultMask, sizeof(faultMask)); offset += sizeof(faultMask);
    uint32_t estop = motion_get_estop();
    memcpy(&buffer[offset], &estop, sizeof(estop)); offset += sizeof(estop);
    /* Jog telemetry: current per-axis speed (steps/s) */
    for (int axis=0; axis<4; ++axis){
        uint32_t js = jog_get_current_speed_axis(axis);
        memcpy(&buffer[offset], &js, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    for (int axis=0; axis<4; ++axis){
        uint32_t jt = jog_get_target_speed_axis(axis);
        memcpy(&buffer[offset], &jt, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    for (int axis=0; axis<4; ++axis){
        uint32_t jd = jog_get_direction_axis(axis);
        memcpy(&buffer[offset], &jd, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    uint32_t probe = motion_get_probe();
    memcpy(&buffer[offset], &probe, sizeof(probe)); offset += sizeof(probe);

    uint32_t firmwareVersion = ((uint32_t)FW_VERSION_MAJOR << 24)
                             | ((uint32_t)FW_VERSION_MINOR << 16)
                             | ((uint32_t)FW_VERSION_PATCH << 8)
                             | ((uint32_t)REMORA_PROTOCOL_VERSION & 0xFF);
    memcpy(&buffer[offset], &firmwareVersion, sizeof(firmwareVersion)); offset += sizeof(firmwareVersion);
    uint32_t bh = build_hash_stub();
    memcpy(&buffer[offset], &bh, sizeof(bh)); offset += sizeof(bh);

    uint32_t heartbeat = heartbeat_counter++;
    memcpy(&buffer[offset], &heartbeat, sizeof(heartbeat)); offset += sizeof(heartbeat);
    uint32_t uptime = HAL_GetTick();
    memcpy(&buffer[offset], &uptime, sizeof(uptime)); offset += sizeof(uptime);

    /* statusFlags bit allocation */
    /* bit0=estop, bit1=anyFault, bit2=failsafe, bit3=homingActive, bits8-11 homedMask, bits12-15 limitMask, bit4=DHCP active */
    uint32_t anyFault = motion_get_fault_mask() ? 1u : 0u;
    uint32_t homingActive = homing_is_active() ? 1u : 0u;
    uint32_t homedMask = homing_get_homed_mask();
    extern uint32_t motion_get_limit_mask(void);
    uint32_t limitMask = motion_get_limit_mask();
    extern uint8_t lwip_port_dhcp_active(void);
    uint32_t dhcpActive = lwip_port_dhcp_active()?1u:0u;
    uint32_t statusFlags = (estop & 0x1u)
                         | (anyFault << 1)
                         | ((uint32_t)failsafe_active << 2)
                         | (homingActive << 3)
                         | (dhcpActive << 4)
                         | ((homedMask & 0xF) << 8)
                         | ((limitMask & 0xF) << 12);
    memcpy(&buffer[offset], &statusFlags, sizeof(statusFlags)); offset += sizeof(statusFlags);
    uint32_t seqGaps = rx_seq_gap_count;
    memcpy(&buffer[offset], &seqGaps, sizeof(seqGaps)); offset += sizeof(seqGaps);

    /* Compute CRC32 over base payload portion (excluding header) except we leave space for CRC at end */
    size_t payload_no_crc = offset - sizeof(RemoraUdpHeader);
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *crc_ptr = buffer + sizeof(RemoraUdpHeader);
    for (size_t i=0;i<payload_no_crc;i++) {
        crc ^= crc_ptr[i];
        for (int b=0;b<8;b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    crc ^= 0xFFFFFFFFu;
    memcpy(&buffer[offset], &crc, sizeof(crc)); offset += sizeof(crc);

    /* Append optional extension AFTER base CRC so legacy clients stop earlier.
       Layout:
         uint32_t extLen (number of bytes following this field, can be 0)
         if extLen>0 then extension fields in fixed order:
             crcErrors, authFailures, estopEdges, loopLast, loopMin, loopMax, reserved0, reserved1
    */
    uint32_t *ext_start = (uint32_t*)&buffer[offset];
    uint32_t ext_len_bytes = 0;
    if (g_ext_telemetry_enabled) {
        uint32_t *p32 = ext_start + 1; /* skip len for now */
        *p32++ = g_crc_error_count;
        *p32++ = g_auth_failure_count;
        *p32++ = g_estop_edge_count;
        *p32++ = g_loop_interval_last;
        *p32++ = (g_loop_interval_min==0xFFFFFFFFu)?0u:g_loop_interval_min;
        *p32++ = g_loop_interval_max;
        *p32++ = 0; /* reserved */
        *p32++ = 0; /* reserved */
        ext_len_bytes = (uint32_t)((uint8_t*)p32 - (uint8_t*)(ext_start+1));
    }
    *ext_start = ext_len_bytes; /* write length */
    offset += sizeof(uint32_t) + ext_len_bytes;

    hdr->payloadLen = (uint16_t)(offset - sizeof(RemoraUdpHeader));

    struct pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, (u16_t)offset, PBUF_POOL);
    if (!pb) return;
    pbuf_take(pb, buffer, (u16_t)offset);

    udp_sendto(remora_pcb, pb, &last_client_ip, last_client_port);
    pbuf_free(pb);

    /* Loop interval / jitter tracking */
    uint32_t now = HAL_GetTick();
    if (g_last_feedback_tick != 0) {
        uint32_t delta = now - g_last_feedback_tick;
        g_loop_interval_last = delta;
        if (delta < g_loop_interval_min) g_loop_interval_min = delta;
        if (delta > g_loop_interval_max) g_loop_interval_max = delta;
    }
    g_last_feedback_tick = now;
}

void remora_udp_init(void)
{
    remora_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (!remora_pcb) return;
    if (udp_bind(remora_pcb, IP_ANY_TYPE, REMORA_UDP_PORT) != ERR_OK) { udp_remove(remora_pcb); remora_pcb = NULL; return; }
    udp_recv(remora_pcb, remora_udp_recv, NULL);
}

/* Wrapper expected by main loop (legacy naming) */
void remora_udp_poll(void) { remora_udp_send_feedback_unicast(); }
void remora_udp_send_feedback(void) { remora_udp_send_feedback_unicast(); }

/* Failsafe checker: disable motion if no RX for interval */
void remora_udp_periodic_10ms(void) {
    uint32_t now = HAL_GetTick();
    const uint32_t TIMEOUT_MS = 500; /* configurable */
    if (!failsafe_active && last_rx_tick != 0 && (now - last_rx_tick) > TIMEOUT_MS) {
        failsafe_active = 1;
        /* Disable motion by zeroing frequencies */
        for (int axis=0; axis<JOINTS; ++axis) {
            stepgen_apply_command(axis, 0, 0);
        }
    }
}
