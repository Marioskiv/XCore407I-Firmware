#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

#define FW_VERSION_MAJOR 2
#define FW_VERSION_MINOR 0
#define FW_VERSION_PATCH 0

static inline unsigned build_hash_stub(void) {
    const char *s = __DATE__ __TIME__;
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

#endif
