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
#
# Requirements (Linux x86_64 or Windows with bash):
#   - zig 0.16.x         (pip install ziglang==0.16.0 — newest version with both
#                         Linux and Windows wheels — or set ZIG=/path/to/zig)
#   - python3 (or python), npm
#
# The Microsoft WebView2 SDK files (WebView2.h, libWebView2Loader.a,
# WebView2Loader.dll) are fetched from the pinned MIT-licensed npm package
# webview-node@0.1.41 (see desktop/THIRD-PARTY-NOTICES.txt). They are build
# inputs only; the loader DLL is redistributed next to PVZM.exe at runtime.
set -euo pipefail
cd "$(dirname "$0")/.."

ZIG="${ZIG:-}"
if [ -z "$ZIG" ]; then
    if python3 -m ziglang version >/dev/null 2>&1; then
        ZIG="python3 -m ziglang"
    elif python -m ziglang version >/dev/null 2>&1; then
        ZIG="python -m ziglang"
    elif command -v zig >/dev/null 2>&1; then
        ZIG="zig"
    else
        echo "error: zig not found (pip install ziglang) and ZIG not set" >&2
        exit 1
    fi
fi
PY="${PYTHON:-}"
if [ -z "$PY" ]; then
    if command -v python3 >/dev/null 2>&1; then PY=python3; else PY=python; fi
fi
echo "== toolchain: $($ZIG version), python: $($PY --version 2>&1)"

BUILD=build/win
DIST=dist
rm -rf "$BUILD"
mkdir -p "$BUILD" "$DIST"

echo "== fetching WebView2 SDK bits (webview-node@0.1.41, MIT)"
( cd "$BUILD" && npm pack webview-node@0.1.41 >/dev/null && \
  tar xzf webview-node-0.1.41.tgz )
mkdir -p "$BUILD/sdk/include" "$BUILD/sdk/lib" "$BUILD/sdk/dll"
cp "$BUILD/package/bin/source/include/WebView2.h" "$BUILD/sdk/include/"
cp "$BUILD/package/bin/source/lib/x64/libWebView2Loader.a" "$BUILD/sdk/lib/"
cp "$BUILD/package/bin/WebView2Loader.dll" "$BUILD/sdk/dll/"

WV2_LIBS="-lole32 -loleaut32 -luuid -lshell32 -lshlwapi -luser32 -lcomdlg32 -lgdi32 -ladvapi32 -lversion"

echo "== building PVZM.exe (desktop host)"
$ZIG c++ -target x86_64-windows-gnu -O2 -fno-exceptions -fno-rtti \
    -I "$BUILD/sdk/include" -I desktop/compat \
    desktop/host.cpp -o "$BUILD/PVZM.exe" \
    -L "$BUILD/sdk/lib" -lWebView2Loader $WV2_LIBS \
    -Wl,--subsystem,windows

echo "== building installer stub"
$ZIG c++ -target x86_64-windows-gnu -O2 -fno-exceptions -fno-rtti \
    desktop/setup.cpp -o "$BUILD/PVZM-Setup-raw.exe" \
    $WV2_LIBS -Wl,--subsystem,windows -municode

echo "== staging portable layout"
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

echo "== portable ZIP"
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

echo "== installer"
"$PY" scripts/make-installer.py \
    --setup "$BUILD/PVZM-Setup-raw.exe" \
    --payload "$STAGE" \
    --out "$DIST/PVZM-Setup-x64.exe"

echo "== round-trip test of installer extraction (host build)"
$ZIG c++ -O2 desktop/setup.cpp -o "$BUILD/setup-native"
rm -rf "$BUILD/roundtrip"
"$BUILD/setup-native" --extract "$BUILD/roundtrip" --from "$DIST/PVZM-Setup-x64.exe" >/dev/null
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
    missing = set(a) - set(b)
    extra = set(b) - set(a)
    diff = {k for k in set(a) & set(b) if a[k] != b[k]}
    print("ROUNDTRIP FAIL", {"missing": sorted(missing)[:5], "extra": sorted(extra)[:5], "changed": sorted(diff)[:5]})
    sys.exit(1)
print(f"roundtrip OK: {len(a)} files match exactly")
PY

echo "== checksums"
( cd "$DIST" && sha256sum PVZM-win64-portable.zip PVZM-Setup-x64.exe > SHA256SUMS.txt && cat SHA256SUMS.txt )

echo "== done: $DIST/PVZM-win64-portable.zip, $DIST/PVZM-Setup-x64.exe"
