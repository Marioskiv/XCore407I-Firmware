#include "build_id.h"
/* Provide a simple indirection so existing remora_get_build_id can evolve
 * while main prints a stable symbol. If remora_get_build_id returns NULL,
 * fall back to a static placeholder. */
extern const char* remora_get_build_id(void);
/* Expose a stable symbol that points to build id string; resolve at link time */
const char* const build_id_string = (const char*)0; /* will be fixed in accessor */

/* Provide weak alias if linker allows, otherwise inline function fallback */
const char* remora_get_build_id(void);
#undef build_id_string
const char* build_id_string_actual(void) {
    const char* id = remora_get_build_id();
    return id ? id : "(unknown)";
}
/* Macro so users printing build_id_string still compile; change main if needed */
#define build_id_string build_id_string_actual()
