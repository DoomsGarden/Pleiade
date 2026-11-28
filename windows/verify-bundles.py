#!/usr/bin/env python3
import hashlib
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

if len(sys.argv) != 7:
    sys.exit("Uso: verify-bundles.py DIST CARPETA_PAQUETES ANDROID_BUILD_TOOLS APK_018 WEBVIEW2_SDK 7ZZ")

dist, packages, android, baseline, webview2, sevenzip = map(Path, sys.argv[1:])
version = json.loads((dist / "assets/manifest.json").read_text(encoding="utf-8"))["viewer_version"]
setup = packages / f"Pleiade-{version}-Windows-x64-Setup.exe"
apk = packages / f"Pleiade-{version}.apk"

def call(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)

def check(condition, message):
    if not condition:
        sys.exit(message)

def hash_bytes(data):
    return hashlib.sha256(data).hexdigest()

def hash_tree(tree):
    return {file.relative_to(tree).as_posix(): hash_bytes(file.read_bytes())
            for file in sorted(tree.rglob("*")) if file.is_file()}

def matching_trees(expected, actual, label):
    mismatches = sorted(name for name in expected.keys() | actual.keys()
                        if expected.get(name) != actual.get(name))
    check(not mismatches, f"{label} difiere de dist en {mismatches}")
    print(f"{label} · {len(expected)} archivos idénticos a dist")

def machine(path):
    data = path.read_bytes()
    check(data[:2] == b"MZ", f"{path.name} no tiene cabecera MZ")
    offset = int.from_bytes(data[60:64], "little")
    check(data[offset:offset + 4] == b"PE\x00\x00", f"{path.name} no tiene cabecera PE")
    return int.from_bytes(data[offset + 4:offset + 6], "little")

def signing_certificate(path):
    output = call(android / "apksigner", "verify", "--print-certs", path)
    prefix = "Signer #1 certificate SHA-256 digest: "
    return next(line.removeprefix(prefix) for line in output.splitlines() if line.startswith(prefix))

print(f"Pléiade {version}")
print(f"Commit del visor · {call('git', '-C', dist, 'rev-parse', 'HEAD').strip()}")
expected = hash_tree(dist)

check(machine(setup) == 0x014c, "Arquitectura inesperada del bootstrap de NSIS")
with tempfile.TemporaryDirectory(prefix="pleiade-setup-verify-") as temporary:
    folder = Path(temporary)
    call(sevenzip, "x", "-y", f"-o{folder}", setup)
    launcher = folder / "Pleiade.exe"
    loader = folder / "WebView2Loader.dll"
    check(machine(launcher) == 0x8664, "El visor de Windows debe ser x64")
    check(machine(loader) == 0x8664, "El cargador WebView2 debe ser x64")
    utf16_version = f"{version}\x00".encode("utf-16le")
    check(utf16_version in launcher.read_bytes(), "La versión del lanzador difiere")
    check(utf16_version in setup.read_bytes(), "La versión del instalador difiere")
    check(loader.read_bytes() == (webview2 / "build/native/x64/WebView2Loader.dll").read_bytes(),
          "El cargador difiere del SDK oficial")
    check((folder / "WebView2.LICENSE.txt").read_bytes() == (webview2 / "LICENSE.txt").read_bytes(),
          "Falta la licencia del cargador")
    matching_trees(expected, hash_tree(folder / "viewer"), "Instalador Windows")
    print("NSIS instala por usuario Pleiade.exe x64 y WebView2Loader.dll x64")
    print("Requiere Windows x64 con Microsoft Edge WebView2 Runtime instalado")
    print("El visor lee sus archivos del disco por el origen local https://pleiade-viewer.local")
    print("El instalador de Windows se compiló en Linux. Validación de archivero, PE y recursos.")

with zipfile.ZipFile(apk) as archive:
    prefix = "assets/viewer/"
    actual = {entry.filename.removeprefix(prefix): hash_bytes(archive.read(entry))
              for entry in archive.infolist() if entry.filename.startswith(prefix) and not entry.is_dir()}
    matching_trees(expected, actual, "APK Android")
certificate = signing_certificate(apk)
check(certificate == signing_certificate(baseline), "El certificado del APK difiere del de .018")
badging = call(android / "aapt2", "dump", "badging", apk).splitlines()[0]
check("name='com.doomsgarden.pleiade.viewer'" in badging, f"Paquete Android inesperado: {badging}")
check(f"versionName='{version}'" in badging, f"Versión Android inesperada: {badging}")
check(f"versionCode='{int(version.split('.')[-1])}'" in badging, f"Código Android inesperado: {badging}")
call(android / "zipalign", "-c", "-P", "16", "4", apk)
print(badging)
print(f"Firma Android · mismo certificado que .018 · SHA-256 {certificate}")
print("Alineación APK de 16 KiB verificada")

print("\nSHA-256 de los paquetes")
for path in (setup, apk):
    print(f"{hash_bytes(path.read_bytes())}  {path.name}")
print("\nSHA-256 del contenido común de dist")
for name, digest in expected.items():
    print(f"{digest}  {name}")
