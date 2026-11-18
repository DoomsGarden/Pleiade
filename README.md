# Pléiade

Pléiade es un motor gráfico propio. Este repositorio conserva el visor WebGL 2, su paquete Android y la fuente C++ recuperada de la base PC.

## Android `0.000.00.009`

La APK de prueba incluye el visor y los recursos para usarlo sin conexión. Arranca en modo Ahorro. Requiere Android 7 o posterior y un WebView actualizado con WebGL 2. [Descargar desde GitHub Releases](https://github.com/DoomsGarden/Pleiade/releases/tag/v0.000.00.009)

La firma del paquete y los recursos están verificados. La instalación y los FPS siguen pendientes de prueba en el POCO F7 y la Galaxy Tab A9+. El visor y la APK comparten la numeración `0.000.00.000`.

## Archivos del proyecto

| Ruta | Contenido |
| --- | --- |
| `dist/` | Visor WebGL 2 y recursos listos para servir. |
| `android/` | Contenedor Android y script para compilar y firmar. |
| `tools/desktop-source/` | Renderer y generación procedural C++ de la base PC `0.003.0.17`, con los cambios recuperados. |
| `tools/` | Exportador de escenas y preparación de materiales y shaders. |
| `tests/` | Pruebas y resultados de render de las revisiones recuperadas. |
| `AGENTS.md` | Instrucciones del proyecto para Codex. |
| `CHANGELOG.md` | [Historial de cambios y pruebas](CHANGELOG.md). |

## Visor y pruebas

Sirve `dist/` por HTTP y abre `http://localhost:8000` en un navegador con WebGL 2.

```sh
python3 -m http.server 8000 --directory dist
```

Para las pruebas, instala las dependencias de Python en tu entorno y ejecuta los comandos desde la raíz. El segundo comando de Node comprueba la integración del visor con Android mediante una simulación.

```sh
python3 -m pip install -r requirements.txt
npm test
PLEIADE_ANDROID_MOCK=1 node tests/runtime_check.mjs
```

## Compilar Android

El script se ejecuta en Linux o WSL con un JDK, la plataforma Android 35, Android SDK Build Tools 35 y un almacén de claves propio. Usa `PLEIADE_ANDROID_KEY_ALIAS` si tu clave tiene un alias distinto de `pleiade-test`. `apksigner` solicita la contraseña durante la firma.

```sh
android/build-apk.sh RUTA_ANDROID_JAR RUTA_BUILD_TOOLS RUTA_KEYSTORE build/pleiade.apk
```

Para actualizar una instalación existente se necesita el mismo certificado que firmó esa instalación.

## Compilar la base C++

Instala CMake 3.20 o posterior, un compilador C++17 y el paquete de desarrollo de SDL2 con su configuración de CMake. El renderer necesita OpenGL 4.3. Para una prueba de escritorio, el script empaqueta los materiales del visor de 512 px en el formato del ejecutable.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python3 tools/prepare_desktop_demo.py build/materials
./build/pleiade --water-demo --materials --asset-dir build/materials
```

En Windows, un generador como Visual Studio deja el ejecutable en `build/Release/pleiade.exe`. SDL2 debe estar disponible al ejecutar. La distribución original de PC y sus texturas de 1024 px quedan pendientes de recuperar. La compilación de Windows queda pendiente de probar.

Consulta el [historial](CHANGELOG.md) y el [manifiesto de materiales](dist/assets/manifest.json) para ver procedencia, créditos y límites conocidos.
