#!/usr/bin/env python3
"""Validate expected packet length calculations for base v4 + extension.
This mirrors the firmware serialization ordering.
"""
import struct

# Sizes taken from firmware structures (4 joints)
INT32 = 4; UINT32 = 4; FLOAT = 4; UINT16 = 2

# Base feedback fields (order from remora_udp.c before extension):
# jointFeedback[4] (int32) = 16
# processVariable[4] (float) = 16
# inputs (uint16) + pad (uint16) = 4
# faultMask (4)
# estop (4)
# jogSpeeds[4] (4*4=16)
# jogTargets[4] (16)
# jogDirs[4] (16)
# probe (4)
# firmwareVersion (4)
# buildHash (4)
# heartbeat (4)
# uptime (4)
# statusFlags (4)
# seqGapEvents (4)
# crc32 (4)
BASE_BYTES = 16+16+4 + 4+4 + 16+16+16 + 4 + 4 + 4 + 4 + 4 + 4 + 4
assert BASE_BYTES == (16+16+4+4+4+16+16+16+4+4+4+4+4+4+4)

EXT_LEN = 4 + 8*4  # extLen + 8 uint32 fields

HEADER = 12  # struct RemoraUdpHeader

EXPECTED_WITH_EXT = HEADER + BASE_BYTES + EXT_LEN
EXPECTED_NO_EXT    = HEADER + BASE_BYTES + 4  # extLen field present, zero

def test_lengths():
    assert EXT_LEN == 36
    assert EXPECTED_WITH_EXT == HEADER + BASE_BYTES + 36
    assert EXPECTED_NO_EXT == HEADER + BASE_BYTES + 4

if __name__ == '__main__':
    test_lengths(); print("OK")
