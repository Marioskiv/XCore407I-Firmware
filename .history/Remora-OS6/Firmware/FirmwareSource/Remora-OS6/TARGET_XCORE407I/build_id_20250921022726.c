#include "build_id.h"
/* Build identifier string (update automatically by regeneration if needed) */
__attribute__((used)) const char* const build_id_string =
  "REMORA_BUILD 2025-09-20T00:00Z base";
const char* remora_get_build_id(void){ return build_id_string; }
