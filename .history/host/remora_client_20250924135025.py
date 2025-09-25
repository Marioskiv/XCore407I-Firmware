#!/usr/bin/env python3
"""
Minimal host client for Remora UDP protocol v4 with optional negotiated extension.
- Sends periodic command packets (no motion by default) and receives feedback.
- Supports negotiating extension telemetry on/off.
- Can issue CLEAR_FAULTS with HMAC auth tag.
Usage:
  python remora_client.py --ip 192.168.1.50 --port 27181 --no-ext
"""
import argparse, socket, struct, time, hmac, hashlib

MAGIC = 0x524D5241
PROTOCOL_VERSION = 4
AUTH_TAG_LEN = 8

OP_NOP = 0
OP_CLEAR_FAULTS = 1
OP_SET_JOG_SPEED = 2
OP_SET_JOG_ACCEL = 3
OP_HOME_AXIS = 4
OP_ABORT_HOMING = 5
OP_SAVE_CONFIG = 6
OP_LOAD_CONFIG = 7
OP_NEGOTIATE_EXT = 8

DEFAULT_KEY = b"remora-secret-key"

HEADER_FMT = '<I I H H'  # magic, seq, payloadLen, version (firmware stores protocol version here)

# Base command layout matches firmware (Remora v4):
# jointFreq[4]int32, setPoint[4]float, jointEnable(uint8), outputs(uint16)
# No padding; optional opcode/value follows immediately after these 35 bytes.
BASE_CMD_FMT = '<' + ('i'*4) + ('f'*4) + 'B' + 'H'
BASE_CMD_SIZE = struct.calcsize(BASE_CMD_FMT)

class RemoraClient:
    def __init__(self, ip, port, key=DEFAULT_KEY, enable_ext=True, verbose=True):
        self.ip = ip; self.port = port; self.seq = 1; self.key = key
        self.enable_ext = enable_ext; self.verbose = verbose
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.5)

    def build_base_cmd(self):
        joint_freq = [0, 0, 0, 0]
        setpoint = [0.0, 0.0, 0.0, 0.0]
        joint_enable = 0  # uint8 bitmask
        outputs = 0       # uint16 bitmask
        return struct.pack(BASE_CMD_FMT, *(joint_freq + setpoint + [joint_enable, outputs]))

    def build_packet(self, opcode=OP_NOP, value=0, auth=False):
        base = self.build_base_cmd()
        op_part = struct.pack('<II', opcode, value)
        tag = b''
        if auth:
            # Msg = opcode||value little endian 8 bytes
            msg = op_part
            full = hmac.new(self.key, msg, hashlib.sha256).digest()
            tag = full[:AUTH_TAG_LEN]
        payload = base + op_part + tag
        hdr = struct.pack(HEADER_FMT, MAGIC, self.seq, len(payload), PROTOCOL_VERSION)
        self.seq += 1
        return hdr + payload

    def send(self, opcode=OP_NOP, value=0, auth=False):
        pkt = self.build_packet(opcode, value, auth)
        self.sock.sendto(pkt, (self.ip, self.port))

    def negotiate(self):
        val = 1 if self.enable_ext else 0
        if self.verbose:
            print(f"Negotiating extension: {'enable' if val else 'disable'}")
        self.send(OP_NEGOTIATE_EXT, val, auth=False)

    def clear_faults(self):
        if self.verbose:
            print("Sending CLEAR_FAULTS (authenticated)")
        self.send(OP_CLEAR_FAULTS, 0, auth=True)

    def recv_feedback(self):
        data, _ = self.sock.recvfrom(2048)
        if len(data) < struct.calcsize(HEADER_FMT):
            return None
        magic, seq, payloadLen, version = struct.unpack_from(HEADER_FMT, data, 0)
        if magic != MAGIC or version < 4:
            return None
        payload = data[struct.calcsize(HEADER_FMT):struct.calcsize(HEADER_FMT)+payloadLen]
        # Parse base feedback through crc32 (fixed layout known from firmware)
        # We'll unpack stepwise.
        off = 0
        def take(fmt):
            nonlocal off
            sz = struct.calcsize(fmt)
            val = struct.unpack_from(fmt, payload, off)
            off += sz
            return val if len(val) > 1 else val[0]
        jointFeedback = take('<' + 'i'*4)
        processVar = take('<' + 'f'*4)
        inputs = take('<H')  # uint16, no padding in firmware
        faultMask = take('<I')
        estop = take('<I')
        jogSpeeds = take('<' + 'I'*4)
        jogTargets = take('<' + 'I'*4)
        jogDirs = take('<' + 'I'*4)
        probe = take('<I')
        firmwareVersion = take('<I')
        buildHash = take('<I')
        heartbeat = take('<I')
        uptime = take('<I')
        statusFlags = take('<I')
        seqGapEvents = take('<I')
        crc32_val = take('<I')
        # Remaining (if any): extension
        ext = None
        if off < len(payload):
            if len(payload) - off >= 4:
                extLen = take('<I')[0]
                if extLen and (len(payload) - off) >= extLen:
                    # Expect 8 uint32 values for current revision
                    if extLen >= 32:
                        fields = struct.unpack_from('<' + 'I'*8, payload, off)
                        ext = {
                            'crcErrors': fields[0],
                            'authFailures': fields[1],
                            'estopEdges': fields[2],
                            'loopLast': fields[3],
                            'loopMin': fields[4],
                            'loopMax': fields[5],
                        }
        return {
            'seq': seq,
            'faultMask': faultMask,
            'estop': estop,
            'statusFlags': statusFlags,
            'heartbeat': heartbeat,
            'uptime': uptime,
            'seqGapEvents': seqGapEvents,
            'ext': ext,
        }

    def run(self, duration=5.0, clear_faults=False):
        start = time.time()
        self.negotiate()
        if clear_faults:
            self.clear_faults()
        while time.time() - start < duration:
            # Send a keep-alive NOP each loop (could piggyback commands)
            self.send()
            try:
                fb = self.recv_feedback()
                if fb and self.verbose:
                    line = f"SEQ {fb['seq']} hb={fb['heartbeat']} estop={fb['estop']} faults=0x{fb['faultMask']:X}"
                    if fb['ext']:
                        line += f" ext(authFail={fb['ext']['authFailures']} jitterLast={fb['ext']['loopLast']})"
                    print(line)
            except socket.timeout:
                pass
            time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ip', required=True)
    ap.add_argument('--port', type=int, default=27181)
    ap.add_argument('--no-ext', action='store_true', help='Disable extension telemetry')
    ap.add_argument('--clear-faults', action='store_true')
    ap.add_argument('--duration', type=float, default=5.0)
    ap.add_argument('--quiet', action='store_true')
    ap.add_argument('--key', help='Shared auth key (ASCII)')
    args = ap.parse_args()
    key = args.key.encode() if args.key else DEFAULT_KEY
    client = RemoraClient(args.ip, args.port, key=key, enable_ext=not args.no_ext, verbose=not args.quiet)
    client.run(duration=args.duration, clear_faults=args.clear_faults)

if __name__ == '__main__':
    main()
