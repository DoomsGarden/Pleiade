# Visor WebGL para Windows

`Pleiade-<versión>-Windows-x64-Setup.exe` instala el visor para el usuario actual en
`%LOCALAPPDATA%\Programs\Pleiade`. Se abre desde «Visor de Pléiade» en Inicio y se
desinstala desde Aplicaciones instaladas de Windows.

Requiere Windows x64 y Microsoft Edge WebView2 Runtime. El instalador y el visor
funcionan sin conexión con el Runtime instalado. El visor utiliza su propia carpeta
local de datos de WebView2. El instalador incluye el contenido completo de `dist`
y el cargador de Microsoft con su licencia. [Descarga del Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)

## Compilar en Linux

Se necesitan Python 3, LLVM-MinGW con `x86_64-w64-mingw32-clang++` y
`x86_64-w64-mingw32-windres`, una extracción del SDK WebView2 y NSIS con MUI2.
La versión se lee desde `dist/assets/manifest.json`.

Para el ensamblado de referencia se usaron el SDK Microsoft.Web.WebView2
`1.0.2903.40` y NSIS `3.08-3+deb12u1` de Debian. Si NSIS se extrae en una
carpeta, define `NSISDIR` para apuntar a su `usr/share/nsis`.
[WebView2 SDK 1.0.2903.40](https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.2903.40/microsoft.web.webview2.1.0.2903.40.nupkg)
[Paquetes NSIS](https://deb.debian.org/debian/pool/main/n/nsis/)

```bash
windows/build-viewer.sh dist "$WEBVIEW2_SDK" "$MINGW_BIN" "$MAKENSIS" "$SETUP_EXE"
```

## Verificar los dos paquetes

El verificador extrae el instalador, compara cada archivo empaquetado contra
`dist`, comprueba versión y arquitectura y valida identidad y alineación del APK.
Requiere que los dos paquetes conserven sus nombres `Pleiade-<versión>.apk` y
`Pleiade-<versión>-Windows-x64-Setup.exe`.

```bash
windows/verify-bundles.py dist "$PACKAGES" "$ANDROID_BUILD_TOOLS" \
  "$REFERENCE_APK_018" "$WEBVIEW2_SDK" "$SEVENZIP"
```

La comprobación de funcionamiento en Windows consiste en abrir el instalador,
iniciar «Visor de Pléiade» con el equipo desconectado y navegar por las escenas
con el Runtime ya instalado.
