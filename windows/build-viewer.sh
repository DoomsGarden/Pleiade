#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 5 ]]; then
  echo "Uso: $0 DIST WEBVIEW2_SDK MINGW_BIN MAKENSIS INSTALADOR.exe" >&2
  exit 2
fi

dist=$(realpath "$1")
webview2_sdk=$(realpath "$2")
mingw_bin=$(realpath "$3")
makensis=$(realpath "$4")
output=$(realpath -m "$5")
source=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build=$(mktemp -d)
trap 'rm -rf -- "$build"' EXIT

read -r version version_resource version_csv < <(python3 - "$dist/assets/manifest.json" <<'PY'
import json
import re
import sys
from pathlib import Path

version = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))["viewer_version"]
if not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", version):
    sys.exit("La versión del visor debe tener cuatro componentes numéricos")
components = [int(component) for component in version.split(".")]
if any(component > 65535 for component in components):
    sys.exit("Los componentes de la versión de Windows no pueden superar 65535")
components = list(map(str, components))
print(version, ".".join(components), ",".join(components))
PY
)

mkdir -p "$(dirname "$output")"
printf '#define PRODUCT_VERSION_NUMERIC %s\n#define PRODUCT_VERSION_STRING "%s"\n' \
  "$version_csv" "$version" > "$build/version.h"
"$mingw_bin/x86_64-w64-mingw32-windres" -I "$build" \
  "$source/version.rc" "$build/version.obj"
"$mingw_bin/x86_64-w64-mingw32-clang++" \
  -std=c++17 -O2 -municode -mwindows -static -Wall -Wextra \
  -I "$source" -isystem "$webview2_sdk/build/native/include" \
  "$source/viewer.cpp" "$build/version.obj" -lole32 -luuid -lshell32 -lgdi32 -o "$build/Pleiade.exe"
cp "$webview2_sdk/build/native/x64/WebView2Loader.dll" "$build/"
cp "$webview2_sdk/LICENSE.txt" "$build/WebView2.LICENSE.txt"
"$makensis" -V2 "-DPRODUCT_VERSION=$version" "-DPRODUCT_VERSION_NUMERIC=$version_resource" \
  "-DPAYLOAD_DIR=$build" "-DDIST_DIR=$dist" "-DOUTPUT_FILE=$output" "$source/installer.nsi"
printf '%s\n' "$output"
