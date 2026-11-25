# awesome-os/pnpm — CAS-Native Rust Package Manager

Hard fork of MIT-licensed [`pnpm/pnpm`](https://github.com/pnpm/pnpm) (Rust v12) tailored for **Awesome-OS**:

- **Unified Ghost CAS Root (`B8G_GHOST_CACHE_DIR`):** Automatically defaults `store-dir`, `PNPM_HOME`, and `cache-dir` under `<B8G_GHOST_CACHE_DIR>/pnpm/{store,home,cache}` (SHA-512 256-shard CAS store + hardlink/reflink + `node-linker=loaded` CAS loader).
- **Zero-Duplicate Browser & Runtime Governance:** Automatically sets `PUPPETEER_SKIP_DOWNLOAD=1`, `PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1`, `ELECTRON_SKIP_BINARY_DOWNLOAD=1`, `CYPRESS_INSTALL_BINARY=0`, and points `SECTIUM_EXECUTABLE_PATH` / `PUPPETEER_EXECUTABLE_PATH` to `D:\Sectium\sectium.exe` (or `/usr/local/bin/sectium`) unless `B8G_ALLOW_DUP_BROWSERS=1` is set.
- **GitHub Releases Self-Update:** `pnpm self-update` downloads and atomically replaces the running binary directly from `https://github.com/awesome-os/pnpm/releases/latest/download/pnpm-<os>-<arch>` (override via `--github-repo <owner/repo>` or `AWESOME_OS_PNPM_REPO`; pass `--npm` for the legacy npm registry flow).

## Usage

```powershell
# Self-update directly from awesome-os/pnpm GitHub Releases
pnpm self-update

# Install any GitHub or npm package globally into the CAS store
pnpm add -g github:awesome-os/pinokio

# Inspect the active CAS store path
pnpm store path
```

## Build from Source

```powershell
cargo build --release --bin pnpm
```

Licensed under the [MIT License](LICENSE).

## Quick Install & Run (Windows / Linux / macOS)

```sh
# Install / hydrate from v11 CAS via holo-pm
holo-pm install repository://github.com/awesome-os/pnpm@main

# Or project & run directly via holo-project (0-byte hardlink)
holo-project "project://rust/pnpm?from=repository://github.com/awesome-os/pnpm@main"
```
