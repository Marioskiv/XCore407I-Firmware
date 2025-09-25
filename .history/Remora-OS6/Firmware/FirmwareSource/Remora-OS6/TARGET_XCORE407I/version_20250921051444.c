#include <stdio.h>
#include "board_xcore407i.h"

#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 0
#endif
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 1
#endif
#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH 0
#endif

#ifndef FW_GIT_HASH
#define FW_GIT_HASH "nogit"
#endif

static char g_version[16];
static char g_build[64];

__attribute__((constructor)) static void version_init(void)
{
    snprintf(g_version, sizeof(g_version), "v%u.%u.%u", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    snprintf(g_build, sizeof(g_build), "%s %s git:%s", __DATE__, __TIME__, FW_GIT_HASH);
}

const char *FW_VersionString(void) { return g_version; }
const char *FW_BuildInfoString(void) { return g_build; }
