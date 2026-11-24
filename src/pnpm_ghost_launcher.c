/*
 * pnpm_ghost_launcher.c — Tiny (~60 KB) B8G Ghost CAS launcher for awesome-os/pnpm.
 *
 * Features:
 *   - Resolves B8G_GHOST_CACHE_DIR (default: ~/.b8g/cas/pnpm).
 *   - `--ghost-info`: Dynamically loads libpnpm (.dll/.so/.dylib), verifies
 *     B8G_ABI_MAGIC_V3 (0x33564742), and prints version + CAS store/home paths.
 *   - Normal execution: Locates hydrated pnpm binary in Ghost CAS
 *     (<cas>/bin/pnpm[.exe] or D:\pnpm\bin\pnpm.exe on Windows), auto-downloads
 *     from https://github.com/awesome-os/pnpm/releases/latest/download/ if missing,
 *     and execs it with all original CLI arguments.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#define PATH_SEP '\\'
#define EXE_EXT ".exe"
#define LIB_NAME "libpnpm.dll"
#else
#include <dlfcn.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#define PATH_SEP '/'
#define EXE_EXT ""
#ifdef __APPLE__
#define LIB_NAME "libpnpm.dylib"
#else
#define LIB_NAME "libpnpm.so"
#endif
#endif

#define B8G_PNPM_ABI_MAGIC_V3 0x33564742u
#define MAX_PATH_BUF 4096

#if defined(_WIN32)
#define PNPM_PLATFORM_ASSET "pnpm-windows-x64.exe"
#elif defined(__APPLE__) && defined(__aarch64__)
#define PNPM_PLATFORM_ASSET "pnpm-macos-arm64"
#elif defined(__APPLE__)
#define PNPM_PLATFORM_ASSET "pnpm-macos-x64"
#elif defined(__aarch64__)
#define PNPM_PLATFORM_ASSET "pnpm-linux-arm64"
#else
#define PNPM_PLATFORM_ASSET "pnpm-linux-x64"
#endif

typedef uint32_t (*fn_b8g_pnpm_abi_magic)(void);
typedef const char* (*fn_b8g_pnpm_version)(void);
typedef int32_t (*fn_b8g_pnpm_resolve_cas_store)(char* out_buf, size_t out_cap);
typedef int32_t (*fn_b8g_pnpm_resolve_cas_home)(char* out_buf, size_t out_cap);

static int file_exists(const char* path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static void mkdir_p(const char* dir) {
    char tmp[MAX_PATH_BUF];
    size_t len = strlen(dir);
    if (len == 0 || len >= sizeof(tmp)) {
        return;
    }
    memcpy(tmp, dir, len + 1);
    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];
            tmp[i] = '\0';
#ifdef _WIN32
            if (!(i == 2 && tmp[1] == ':')) {
                _mkdir(tmp);
            }
#else
            mkdir(tmp, 0755);
#endif
            tmp[i] = saved;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

static void resolve_ghost_cas_root(char* out, size_t cap) {
    const char* env_cas = getenv("B8G_GHOST_CACHE_DIR");
    if (env_cas && env_cas[0] != '\0') {
        snprintf(out, cap, "%s%cpnpm", env_cas, PATH_SEP);
        return;
    }
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (!home || home[0] == '\0') {
        home = getenv("HOME");
    }
    if (!home || home[0] == '\0') {
        home = "C:\\Users\\Public";
    }
    snprintf(out, cap, "%s\\.b8g\\cas\\pnpm", home);
#else
    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        home = "/tmp";
    }
    snprintf(out, cap, "%s/.b8g/cas/pnpm", home);
#endif
}

static void get_exe_dir(char* out, size_t cap, const char* argv0) {
    out[0] = '\0';
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    if (n == 0 || n >= cap) {
        out[0] = '\0';
    }
#else
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    if (n > 0) {
        out[n] = '\0';
    } else if (argv0) {
        snprintf(out, cap, "%s", argv0);
    }
#endif
    (void)argv0;
    char* last_slash = strrchr(out, '/');
    char* last_bslash = strrchr(out, '\\');
    char* sep = last_slash > last_bslash ? last_slash : last_bslash;
    if (sep) {
        *sep = '\0';
    } else {
        snprintf(out, cap, ".");
    }
}

static int run_ghost_info(const char* cas_root, const char* exe_dir) {
    char candidates[5][MAX_PATH_BUF];
    int count = 0;

    snprintf(candidates[count++], MAX_PATH_BUF, "%s%c%s", exe_dir, PATH_SEP, LIB_NAME);
    snprintf(candidates[count++], MAX_PATH_BUF, "%s%cbin%c%s", cas_root, PATH_SEP, PATH_SEP, LIB_NAME);
    snprintf(candidates[count++], MAX_PATH_BUF, "%s%clib%c%s", cas_root, PATH_SEP, PATH_SEP, LIB_NAME);
#ifdef _WIN32
    snprintf(candidates[count++], MAX_PATH_BUF, "D:\\pnpm\\bin\\%s", LIB_NAME);
#endif
    snprintf(candidates[count++], MAX_PATH_BUF, "%s", LIB_NAME);

    const char* loaded_path = NULL;
#ifdef _WIN32
    HMODULE lib = NULL;
    for (int i = 0; i < count; ++i) {
        lib = LoadLibraryA(candidates[i]);
        if (lib) {
            loaded_path = candidates[i];
            break;
        }
    }
    if (!lib) {
        fprintf(stderr, "pnpm-ghost: failed to load %s (last error=%lu)\n",
                LIB_NAME, (unsigned long)GetLastError());
        return 1;
    }
    fn_b8g_pnpm_abi_magic p_magic = (fn_b8g_pnpm_abi_magic)(void*)GetProcAddress(lib, "b8g_pnpm_abi_magic");
    fn_b8g_pnpm_version p_ver = (fn_b8g_pnpm_version)(void*)GetProcAddress(lib, "b8g_pnpm_version");
    fn_b8g_pnpm_resolve_cas_store p_store = (fn_b8g_pnpm_resolve_cas_store)(void*)GetProcAddress(lib, "b8g_pnpm_resolve_cas_store");
    fn_b8g_pnpm_resolve_cas_home p_home = (fn_b8g_pnpm_resolve_cas_home)(void*)GetProcAddress(lib, "b8g_pnpm_resolve_cas_home");
#else
    void* lib = NULL;
    for (int i = 0; i < count; ++i) {
        lib = dlopen(candidates[i], RTLD_NOW);
        if (lib) {
            loaded_path = candidates[i];
            break;
        }
    }
    if (!lib) {
        fprintf(stderr, "pnpm-ghost: failed to load %s (%s)\n", LIB_NAME, dlerror());
        return 1;
    }
    fn_b8g_pnpm_abi_magic p_magic = (fn_b8g_pnpm_abi_magic)dlsym(lib, "b8g_pnpm_abi_magic");
    fn_b8g_pnpm_version p_ver = (fn_b8g_pnpm_version)dlsym(lib, "b8g_pnpm_version");
    fn_b8g_pnpm_resolve_cas_store p_store = (fn_b8g_pnpm_resolve_cas_store)dlsym(lib, "b8g_pnpm_resolve_cas_store");
    fn_b8g_pnpm_resolve_cas_home p_home = (fn_b8g_pnpm_resolve_cas_home)dlsym(lib, "b8g_pnpm_resolve_cas_home");
#endif

    if (!p_magic || !p_ver || !p_store || !p_home) {
        fprintf(stderr, "pnpm-ghost: missing required B8G ABI symbols in %s\n", loaded_path);
#ifdef _WIN32
        FreeLibrary(lib);
#else
        dlclose(lib);
#endif
        return 2;
    }

    uint32_t magic = p_magic();
    const char* version = p_ver();
    char store_buf[MAX_PATH_BUF];
    char home_buf[MAX_PATH_BUF];
    int32_t store_len = p_store(store_buf, sizeof(store_buf));
    int32_t home_len = p_home(home_buf, sizeof(home_buf));

    printf("pnpm-ghost: asset=%s\n", PNPM_PLATFORM_ASSET);
    printf("pnpm-ghost: cas_root=%s\n", cas_root);
    printf("pnpm-ghost: libpnpm=%s\n", loaded_path);
    printf("pnpm-ghost: abi_magic=0x%08X (%s)\n",
           magic, magic == B8G_PNPM_ABI_MAGIC_V3 ? "B8G_ABI_MAGIC_V3 OK" : "MISMATCH");
    printf("pnpm-ghost: version=%s\n", version ? version : "(null)");
    printf("pnpm-ghost: cas_store=%s\n", store_len > 0 ? store_buf : "(error)");
    printf("pnpm-ghost: cas_home=%s\n", home_len > 0 ? home_buf : "(error)");

#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
    return (magic == B8G_PNPM_ABI_MAGIC_V3 && store_len > 0 && home_len > 0) ? 0 : 3;
}

static int hydrate_pnpm_binary(const char* target_bin) {
    char parent[MAX_PATH_BUF];
    snprintf(parent, sizeof(parent), "%s", target_bin);
    char* sep = strrchr(parent, PATH_SEP);
    if (sep) {
        *sep = '\0';
        mkdir_p(parent);
    }

    char cmd[MAX_PATH_BUF * 2];
    const char* url_base = "https://github.com/awesome-os/pnpm/releases/latest/download/" PNPM_PLATFORM_ASSET;
    fprintf(stderr, "pnpm-ghost: hydrating %s -> %s\n", url_base, target_bin);

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
             "curl.exe -fsSL \"%s\" -o \"%s.tmp\" && move /Y \"%s.tmp\" \"%s\" >nul",
             url_base, target_bin, target_bin, target_bin);
#else
    snprintf(cmd, sizeof(cmd),
             "curl -fsSL \"%s\" -o \"%s.tmp\" && chmod +x \"%s.tmp\" && mv \"%s.tmp\" \"%s\"",
             url_base, target_bin, target_bin, target_bin, target_bin);
#endif
    int rc = system(cmd);
    if (rc != 0 || !file_exists(target_bin)) {
        fprintf(stderr, "pnpm-ghost: hydration failed (exit=%d)\n", rc);
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    char cas_root[MAX_PATH_BUF];
    char exe_dir[MAX_PATH_BUF];
    resolve_ghost_cas_root(cas_root, sizeof(cas_root));
    get_exe_dir(exe_dir, sizeof(exe_dir), argc > 0 ? argv[0] : "");

    if (argc >= 2 && strcmp(argv[1], "--ghost-info") == 0) {
        return run_ghost_info(cas_root, exe_dir);
    }

    char cas_bin[MAX_PATH_BUF];
    snprintf(cas_bin, sizeof(cas_bin), "%s%cbin%cpnpm%s", cas_root, PATH_SEP, PATH_SEP, EXE_EXT);

    char resolved_bin[MAX_PATH_BUF];
    resolved_bin[0] = '\0';

    if (file_exists(cas_bin)) {
        snprintf(resolved_bin, sizeof(resolved_bin), "%s", cas_bin);
    }
#ifdef _WIN32
    else if (file_exists("D:\\pnpm\\bin\\pnpm.exe")) {
        snprintf(resolved_bin, sizeof(resolved_bin), "D:\\pnpm\\bin\\pnpm.exe");
    }
#endif
    else {
        char sibling_bin[MAX_PATH_BUF];
        snprintf(sibling_bin, sizeof(sibling_bin), "%s%cpnpm%s", exe_dir, PATH_SEP, EXE_EXT);
        if (file_exists(sibling_bin)) {
            snprintf(resolved_bin, sizeof(resolved_bin), "%s", sibling_bin);
        } else {
            if (hydrate_pnpm_binary(cas_bin) != 0) {
                return 1;
            }
            snprintf(resolved_bin, sizeof(resolved_bin), "%s", cas_bin);
        }
    }

#ifdef _WIN32
    const char** child_argv = (const char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!child_argv) {
        return 1;
    }
    child_argv[0] = resolved_bin;
    for (int i = 1; i < argc; ++i) {
        child_argv[i] = argv[i];
    }
    child_argv[argc] = NULL;
    intptr_t rc = _spawnv(_P_WAIT, resolved_bin, child_argv);
    free(child_argv);
    return (int)rc;
#else
    char** child_argv = (char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!child_argv) {
        return 1;
    }
    child_argv[0] = resolved_bin;
    for (int i = 1; i < argc; ++i) {
        child_argv[i] = argv[i];
    }
    child_argv[argc] = NULL;
    execv(resolved_bin, child_argv);
    perror("pnpm-ghost: execv failed");
    free(child_argv);
    return 127;
#endif
}
