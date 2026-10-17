#!/usr/bin/env bash
# Build the Windows x64 desktop artifacts for Plants vs. Zombies: MODDED.
#
# Outputs (dist/):
#   PVZM-win64-portable.zip  Portable package: PVZM.exe + WebView2Loader.dll +
#                            the complete game/ tree + licenses/docs.
#                            Extract the WHOLE zip anywhere and run PVZM.exe.
#   PVZM-Setup-x64.exe       Self-extracting per-user installer containing the
#                            same content (installs to %LOCALAPPDATA%\PVZM).
#   SHA256SUMS.txt           SHA-256 checksums of both artifacts.
#   stage-path.txt           Absolute path of the staged portable directory
#                            (used by CI to run the packaged app in place).
#
# Runs on Linux x86_64 (cross-compiling) and on Windows (Git Bash, native).
#
# Requirements:
#   - zig 0.16.x         (pip install ziglang==0.16.0 — newest version with both
#                         Linux and Windows wheels — or set ZIG=/path/to/zig)
#   - python3 (or python), npm
#
# The Microsoft WebView2 SDK files (WebView2.h, libWebView2Loader.a,
# WebView2Loader.dll) are fetched from the pinned MIT-licensed npm package
# webview-node@0.1.41 (see desktop/THIRD-PARTY-NOTICES.txt). They are build
# inputs only; the loader DLL is redistributed next to PVZM.exe at runtime.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

# --- diagnostics -----------------------------------------------------------
# CI log downloads are not always available, so failures are also reported as
# GitHub Actions annotations (::error::), which ARE readable through the REST API
# (GET /repos/{owner}/{repo}/check-runs/{job_id}/annotations).
on_error() {
    local line="$1" cmd="$2" code="$3"
    echo "::error::build-windows.sh failed at line ${line} (exit ${code}): ${cmd}"
}
trap 'on_error "$LINENO" "$BASH_COMMAND" "$?"' ERR

say() { echo "== $*"; }
notice() { echo "::notice::$*"; }

# --- host detection --------------------------------------------------------
HOST_OS="linux"
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT) HOST_OS="windows" ;;
    Darwin) HOST_OS="macos" ;;
    Linux) HOST_OS="linux" ;;
esac

# --- toolchain -------------------------------------------------------------
ZIG="${ZIG:-}"
if [ -z "$ZIG" ]; then
    if python3 -m ziglang version >/dev/null 2>&1; then
        ZIG="python3 -m ziglang"
    elif python -m ziglang version >/dev/null 2>&1; then
        ZIG="python -m ziglang"
    elif command -v zig >/dev/null 2>&1; then
        ZIG="zig"
    else
        echo "::error::zig not found (pip install ziglang==0.16.0) and ZIG not set" >&2
        exit 1
    fi
fi
PY="${PYTHON:-}"
if [ -z "$PY" ]; then
    # On Windows runners `python` is the reliable name; on Linux it is python3.
    if [ "$HOST_OS" = "windows" ] && command -v python >/dev/null 2>&1; then
        PY=python
    elif command -v python3 >/dev/null 2>&1; then
        PY=python3
    else
        PY=python
    fi
fi
say "host=$HOST_OS uname=$(uname -s 2>/dev/null || true) pwd=$PWD"
say "zig=$($ZIG version 2>&1) python=$($PY --version 2>&1)"
notice "build host: $HOST_OS, zig $($ZIG version 2>&1), $($PY --version 2>&1)"

BUILD=build/win
DIST=dist
rm -rf "$BUILD"
mkdir -p "$BUILD" "$DIST"

say "fetching WebView2 SDK bits (webview-node@0.1.41, MIT)"
( cd "$BUILD" && npm pack webview-node@0.1.41 >/dev/null && \
  tar xzf webview-node-0.1.41.tgz )
mkdir -p "$BUILD/sdk/include" "$BUILD/sdk/lib" "$BUILD/sdk/dll"
cp "$BUILD/package/bin/source/include/WebView2.h" "$BUILD/sdk/include/"
cp "$BUILD/package/bin/source/lib/x64/libWebView2Loader.a" "$BUILD/sdk/lib/"
cp "$BUILD/package/bin/WebView2Loader.dll" "$BUILD/sdk/dll/"

WV2_LIBS="-lole32 -loleaut32 -luuid -lshell32 -lshlwapi -luser32 -lcomdlg32 -lgdi32 -ladvapi32 -lversion"
TARGET="-target x86_64-windows-gnu"

say "building PVZM.exe (desktop host)"
$ZIG c++ $TARGET -O2 -fno-exceptions -fno-rtti \
    -I "$BUILD/sdk/include" -I desktop/compat \
    desktop/host.cpp -o "$BUILD/PVZM.exe" \
    -L "$BUILD/sdk/lib" -lWebView2Loader $WV2_LIBS \
    -Wl,--subsystem,windows

say "building installer stub"
$ZIG c++ $TARGET -O2 -fno-exceptions -fno-rtti \
    desktop/setup.cpp -o "$BUILD/PVZM-Setup-raw.exe" \
    $WV2_LIBS -Wl,--subsystem,windows -municode

say "staging portable layout"
STAGE="$BUILD/portable"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/PVZM.exe" "$STAGE/"
cp "$BUILD/sdk/dll/WebView2Loader.dll" "$STAGE/"
cp LICENSE.md "$STAGE/"
cp desktop/README.md "$STAGE/README-desktop.md"
cp desktop/THIRD-PARTY-NOTICES.txt "$STAGE/"
cp app.webmanifest "$STAGE/" 2>/dev/null || true
cp -a game "$STAGE/game"
STAGE_ABS="$(cd "$STAGE" && pwd)"
echo "$STAGE_ABS" > "$DIST/stage-path.txt"
say "staged at $STAGE_ABS ($(find "$STAGE" -type f | wc -l) files)"

say "portable ZIP"
rm -f "$DIST/PVZM-win64-portable.zip"
DIST_ABS="$(cd "$DIST" && pwd)"
( cd "$STAGE" && "$PY" -c "
import os, sys, zipfile
out = sys.argv[1]
names = ['PVZM.exe','WebView2Loader.dll','LICENSE.md','README-desktop.md','THIRD-PARTY-NOTICES.txt','app.webmanifest','game']
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for name in names:
        if os.path.isdir(name):
            for root, _d, fs in os.walk(name):
                for f in sorted(fs):
                    p = os.path.join(root, f)
                    z.write(p, p.replace(os.sep, '/'))
        elif os.path.exists(name):
            z.write(name, name)
print('zip written:', out)
" "$DIST_ABS/PVZM-win64-portable.zip" )

say "installer"
"$PY" scripts/make-installer.py \
    --setup "$BUILD/PVZM-Setup-raw.exe" \
    --payload "$STAGE" \
    --out "$DIST/PVZM-Setup-x64.exe"

say "round-trip test of installer extraction"
# The extractor used for the round-trip check must run on the BUILD host, so it
# is compiled natively (not cross-compiled). On Windows that means the mingw
# target, the Win32 import libraries that setup.cpp's GUI code needs, and an
# .exe suffix so the shell can execute it.
NATIVE_EXE="$BUILD/setup-native"
if [ "$HOST_OS" = "windows" ]; then
    NATIVE_EXE="$BUILD/setup-native.exe"
    $ZIG c++ $TARGET -O2 -fno-exceptions -fno-rtti \
        desktop/setup.cpp -o "$NATIVE_EXE" $WV2_LIBS -Wl,--subsystem,console
else
    $ZIG c++ -O2 -fno-exceptions -fno-rtti \
        desktop/setup.cpp -o "$NATIVE_EXE"
fi
rm -rf "$BUILD/roundtrip"
"$NATIVE_EXE" --extract "$BUILD/roundtrip" --from "$DIST/PVZM-Setup-x64.exe" >/dev/null
"$PY" - "$STAGE" "$BUILD/roundtrip" <<'PY'
import hashlib, os, sys
stage, out = sys.argv[1], sys.argv[2]
def inventory(root):
    inv = {}
    for r, _d, ns in os.walk(root):
        for n in ns:
            p = os.path.join(r, n)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            h = hashlib.sha256(open(p, "rb").read()).hexdigest()
            inv[rel] = (os.path.getsize(p), h)
    return inv
a, b = inventory(stage), inventory(out)
if a != b:
    missing = sorted(set(a) - set(b))[:5]
    extra = sorted(set(b) - set(a))[:5]
    changed = sorted(k for k in set(a) & set(b) if a[k] != b[k])[:5]
    print(f"::error::installer round-trip mismatch missing={missing} extra={extra} changed={changed}")
    sys.exit(1)
print(f"roundtrip OK: {len(a)} files match exactly")
PY

say "checksums"
( cd "$DIST" && "$PY" -c "
import hashlib, sys
for name in ['PVZM-win64-portable.zip', 'PVZM-Setup-x64.exe']:
    h = hashlib.sha256(open(name, 'rb').read()).hexdigest()
    print(h, name)
" > SHA256SUMS.txt && cat SHA256SUMS.txt )

say "done: $DIST/PVZM-win64-portable.zip, $DIST/PVZM-Setup-x64.exe"
notice "build complete: portable ZIP + installer + SHA256SUMS in dist/"
