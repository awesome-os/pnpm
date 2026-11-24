#ifndef B8G_LIBPNPM_H
#define B8G_LIBPNPM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Canonical B8G ABI v3 magic ("BGV3" = 0x33564742). */
#define B8G_PNPM_ABI_MAGIC_V3 0x33564742u

/**
 * Returns the B8G ABI v3 magic constant (0x33564742).
 */
uint32_t b8g_pnpm_abi_magic(void);

/**
 * Returns a static NUL-terminated UTF-8 pnpm version string (e.g. "12.23.0").
 */
const char* b8g_pnpm_version(void);

/**
 * Resolves pnpm's content-addressable store directory (`StoreDir`, including
 * the store layout version suffix).
 *
 * Precedence: PNPM_HOME/store -> B8G_GHOST_CACHE_DIR/pnpm/store ->
 * XDG_DATA_HOME/pnpm/store -> OS default.
 *
 * Writes a NUL-terminated UTF-8 path into `out_buf` (up to `out_cap` bytes).
 * Returns the byte length written (excluding the NUL terminator) on success,
 * or -1 if `out_buf` is NULL, `out_cap` is too small, or resolution fails.
 */
int32_t b8g_pnpm_resolve_cas_store(char* out_buf, size_t out_cap);

/**
 * Resolves pnpm's CAS home directory.
 *
 * Precedence: PNPM_HOME -> B8G_GHOST_CACHE_DIR/pnpm/home ->
 * XDG_DATA_HOME/pnpm -> OS default.
 *
 * Writes a NUL-terminated UTF-8 path into `out_buf` (up to `out_cap` bytes).
 * Returns the byte length written (excluding the NUL terminator) on success,
 * or -1 if `out_buf` is NULL, `out_cap` is too small, or resolution fails.
 */
int32_t b8g_pnpm_resolve_cas_home(char* out_buf, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* B8G_LIBPNPM_H */
