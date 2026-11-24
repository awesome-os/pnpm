#!/usr/bin/env sh
# Awesome-OS CAS pnpm (12.23.0) — Stamped by @awesome-os/installer
# Usage: curl -fsSL https://github.com/awesome-os/pnpm/releases/latest/download/install.sh | sh
set -eu

BASE="${AWESOME_OS_BASE_URL:-https://github.com/awesome-os/pnpm/releases/latest/download}"
INSTALL_DIR="${AWESOME_OS_INSTALL_DIR:-$HOME/.local/bin}"
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS:$ARCH" in
  Linux:x86_64|Linux:amd64)    ASSET="pnpm-linux-x64" ;;
  Linux:aarch64|Linux:arm64)   ASSET="pnpm-linux-arm64" ;;
  Darwin:arm64|Darwin:aarch64) ASSET="pnpm-macos-arm64" ;;
  *) echo "[Awesome-OS CAS pnpm] Unsupported platform: $OS/$ARCH" >&2; exit 1 ;;
esac

mkdir -p "$INSTALL_DIR"
TARGET="$INSTALL_DIR/pnpm"
SUMS_TMP="$(mktemp)"
trap 'rm -f "$SUMS_TMP"' EXIT

echo "[Awesome-OS CAS pnpm] Downloading $ASSET -> $TARGET ..."
curl -fsSL "$BASE/$ASSET" -o "$TARGET"
if curl -fsSL "$BASE/SHA256SUMS.txt" -o "$SUMS_TMP" 2>/dev/null; then
  EXPECTED="$(grep " $ASSET\$" "$SUMS_TMP" | awk '{print $1}' | head -n 1 || true)"
  if [ -n "$EXPECTED" ]; then
    if command -v sha256sum >/dev/null 2>&1; then
      ACTUAL="$(sha256sum "$TARGET" | awk '{print $1}')"
    elif command -v shasum >/dev/null 2>&1; then
      ACTUAL="$(shasum -a 256 "$TARGET" | awk '{print $1}')"
    else
      ACTUAL="$EXPECTED"
    fi
    if [ "$ACTUAL" != "$EXPECTED" ]; then
      rm -f "$TARGET"
      echo "[Awesome-OS CAS pnpm] SHA-256 mismatch! expected=$EXPECTED actual=$ACTUAL" >&2
      exit 1
    fi
    echo "[Awesome-OS CAS pnpm] SHA-256 verified: $ACTUAL"
  fi
fi

chmod +x "$TARGET"

echo "[Awesome-OS CAS pnpm] Ready! Run: $TARGET"
