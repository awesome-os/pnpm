//! `libpnpm-ffi`: `B8G_ABI_MAGIC_V3` (`0x33564742`) C-ABI shared library
//! exposing pnpm's CAS store and home directory resolvers in-process.

#![allow(unsafe_code)]

use pnpm_config::{
    Config,
    Host,
    PNPM_VERSION,
    default_pnpm_home_dir,
};
use pnpm_store_dir::StoreDir;
use std::{
    ffi::{
        CString,
        c_char,
    },
    path::PathBuf,
    ptr,
    sync::OnceLock,
};

/// Canonical `B8G_ABI_MAGIC_V3` magic constant (`0x33564742`).
pub const B8G_PNPM_ABI_MAGIC_V3: u32 = 0x3356_4742;

static VERSION_CSTR: OnceLock<CString> = OnceLock::new();

fn write_path_to_c_buf(path: &std::path::Path, out_buf: *mut c_char, out_cap: usize) -> i32 {
    if out_buf.is_null() || out_cap == 0 {
        return -1;
    }

    let utf8 = path.to_string_lossy();
    let bytes = utf8.as_bytes();
    let Some(required) = bytes.len().checked_add(1) else {
        return -1;
    };
    if required > out_cap {
        return -1;
    }

    let Ok(len_i32) = i32::try_from(bytes.len()) else {
        return -1;
    };

    // SAFETY: Caller guarantees `out_buf` is valid for writes of `out_cap` bytes,
    // and we verified `out_buf` is non-null and `bytes.len() + 1 <= out_cap`.
    unsafe {
        ptr::copy_nonoverlapping(bytes.as_ptr(), out_buf.cast::<u8>(), bytes.len());
        *out_buf.add(bytes.len()) = 0;
    }

    len_i32
}

/// Returns the `B8G_ABI_MAGIC_V3` constant (`0x33564742`).
#[unsafe(no_mangle)]
pub extern "C" fn b8g_pnpm_abi_magic() -> u32 {
    B8G_PNPM_ABI_MAGIC_V3
}

/// Returns a static NUL-terminated UTF-8 pnpm version string.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_pnpm_version() -> *const c_char {
    VERSION_CSTR
        .get_or_init(|| {
            CString::new(PNPM_VERSION).unwrap_or_else(|_| CString::from(c"0.0.0"))
        })
        .as_ptr()
}

/// Resolves pnpm's CAS `StoreDir` path (`PNPM_HOME` -> `B8G_GHOST_CACHE_DIR/pnpm/store` -> OS default)
/// into `out_buf` as a NUL-terminated UTF-8 string.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_pnpm_resolve_cas_store(out_buf: *mut c_char, out_cap: usize) -> i32 {
    let store_dir: StoreDir = Config::default().store_dir;
    let path: PathBuf = store_dir.into();
    write_path_to_c_buf(&path, out_buf, out_cap)
}

/// Resolves pnpm's CAS home directory (`PNPM_HOME` -> `B8G_GHOST_CACHE_DIR/pnpm/home` -> OS default)
/// into `out_buf` as a NUL-terminated UTF-8 string.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_pnpm_resolve_cas_home(out_buf: *mut c_char, out_cap: usize) -> i32 {
    let Some(home_dir) = default_pnpm_home_dir::<Host>() else {
        return -1;
    };
    write_path_to_c_buf(&home_dir, out_buf, out_cap)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;

    #[test]
    fn abi_magic_matches_v3() {
        assert_eq!(b8g_pnpm_abi_magic(), 0x3356_4742);
    }

    #[test]
    fn version_matches_pnpm_version() {
        let ptr = b8g_pnpm_version();
        assert!(!ptr.is_null());
        // SAFETY: `b8g_pnpm_version` returns a valid static NUL-terminated string.
        let version = unsafe { CStr::from_ptr(ptr) }.to_str().expect("valid UTF-8");
        assert_eq!(version, PNPM_VERSION);
    }

    #[test]
    fn resolve_cas_store_and_home_write_non_empty_paths() {
        let mut store_buf = [0 as c_char; 4096];
        let store_len = b8g_pnpm_resolve_cas_store(store_buf.as_mut_ptr(), store_buf.len());
        assert!(store_len > 0);
        // SAFETY: `b8g_pnpm_resolve_cas_store` NUL-terminates on success.
        let store_str = unsafe { CStr::from_ptr(store_buf.as_ptr()) }.to_str().expect("valid UTF-8");
        assert!(!store_str.is_empty());

        let mut home_buf = [0 as c_char; 4096];
        let home_len = b8g_pnpm_resolve_cas_home(home_buf.as_mut_ptr(), home_buf.len());
        assert!(home_len > 0);
        // SAFETY: `b8g_pnpm_resolve_cas_home` NUL-terminates on success.
        let home_str = unsafe { CStr::from_ptr(home_buf.as_ptr()) }.to_str().expect("valid UTF-8");
        assert!(!home_str.is_empty());
    }
}
