#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
    printf 'Usage: %s ANDROID_JAR BUILD_TOOLS_DIR KEYSTORE OUTPUT_APK\n' "$0" >&2
    exit 2
fi

android_jar=$(realpath "$1")
build_tools=$(realpath "$2")
keystore=$(realpath "$3")
output=$(realpath -m "$4")
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

read -r version_name version_code < <(python3 - "$project/dist/assets/manifest.json" <<'PY'
import json
import re
import sys

with open(sys.argv[1], encoding="utf-8") as file:
    version = json.load(file)["viewer_version"]
if not re.fullmatch(r"[0-9]\.[0-9]{3}\.[0-9]{2}\.[0-9]{3}", version):
    sys.exit("Viewer version must match 0.000.00.000")
code = int(version.replace(".", ""))
if code < 1:
    sys.exit("APK versionCode must be positive")
print(version, code)
PY
)

mkdir -p "$scratch/assets/viewer" "$scratch/classes" "$scratch/dex"
cp -R "$project/dist/." "$scratch/assets/viewer/"
"$build_tools/aapt2" compile --dir "$project/android/res" -o "$scratch/resources.zip"
"$build_tools/aapt2" link -I "$android_jar" --manifest "$project/android/AndroidManifest.xml" \
    --min-sdk-version 24 --target-sdk-version 35 \
    --version-name "$version_name" --version-code "$version_code" \
    -A "$scratch/assets" -o "$scratch/unsigned.apk" "$scratch/resources.zip"

compiler=(java --module jdk.compiler/com.sun.tools.javac.Main)
if command -v javac >/dev/null; then
    compiler=(javac)
fi
mapfile -t sources < <(find "$project/android/src" -name '*.java' -type f | sort)
"${compiler[@]}" -source 8 -target 8 -Xlint:-options -classpath "$android_jar" -d "$scratch/classes" "${sources[@]}"
jar_path="$scratch/classes.jar"
(cd "$scratch/classes" && zip -q -r "$jar_path" .)
"$build_tools/d8" --release --lib "$android_jar" --min-api 24 --output "$scratch/dex" "$jar_path"
zip -q -j "$scratch/unsigned.apk" "$scratch/dex"/classes*.dex
"$build_tools/zipalign" -f -P 16 4 "$scratch/unsigned.apk" "$scratch/aligned.apk"

sign_args=(--ks "$keystore" --ks-key-alias "${PLEIADE_ANDROID_KEY_ALIAS:-pleiade-test}")
if [[ -n "${PLEIADE_ANDROID_STOREPASS:-}" ]]; then
    sign_args+=(--ks-pass env:PLEIADE_ANDROID_STOREPASS)
fi
"$build_tools/apksigner" sign "${sign_args[@]}" --out "$scratch/signed.apk" "$scratch/aligned.apk"
"$build_tools/apksigner" verify --min-sdk-version 24 "$scratch/signed.apk"
"$build_tools/zipalign" -c -P 16 4 "$scratch/signed.apk"
mkdir -p -- "$(dirname -- "$output")"
cp -- "$scratch/signed.apk" "$output"
printf '%s\n' "$output"
