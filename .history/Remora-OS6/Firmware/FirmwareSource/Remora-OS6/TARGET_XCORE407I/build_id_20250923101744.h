#ifndef REMORA_BUILD_ID_H
#define REMORA_BUILD_ID_H
#ifdef __cplusplus
extern "C" {
#endif
const char* remora_get_build_id(void);
/* Convenience extern for banner printing; defined in build_id_string.c */
extern const char* const build_id_string;
#ifdef __cplusplus
}
#endif
#endif /* REMORA_BUILD_ID_H */
