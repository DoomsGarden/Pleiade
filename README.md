# Pléiade

Pléiade es un motor gráfico propio. Este repositorio conserva el visor WebGL 2, su paquete Android y la fuente C++ recuperada de la base PC.

## Versión de prueba `0.000.00.018`

La APK de prueba incluye el visor y los recursos para usarlo sin conexión. Arranca en modo Ahorro. Requiere Android 7 o posterior y un WebView actualizado con WebGL 2. El paquete Windows x64 incluye los tres recorridos nativos, SDL2 y los materiales. [Descargas en GitHub Releases](https://github.com/DoomsGarden/Pleiade/releases/tag/v0.000.00.018)

- **Orilla** permite explorar una cuenca de 104 m de lado en modo lago, río o cascada. En **Ajustes → Mirar bajo el agua** se puede inspeccionar el fondo.
- **Edificación** permite alternar entre la cabaña, la casa de teja, el refugio de piedra y el pabellón abierto. **Biomas** abre cinco paisajes que se regeneran con la semilla del mundo.
- En **Ajustes → Estación y clima** se puede seleccionar la estación o vincularla al reloj del mundo. En **Materiales**, ese mismo panel permite desprender una rama de prueba y restaurarla.

Las sombras suavizadas atenúan el borde de cobertura y estabilizan la corrección en superficies rasantes. En C++, la cámara estratégica concentra la resolución cerca del jugador y las muestras de contacto cubren dos distancias.

Las pruebas de rendimiento en la tablet quedan pendientes. El visor y la APK comparten la numeración `0.000.00.000`.

## Archivos del proyecto

| Ruta | Contenido |
| --- | --- |
| `dist/` | Visor WebGL 2 y recursos listos para servir. |
| `android/` | Contenedor Android y script para compilar y firmar. |
| `assets/blender/` | Biblioteca editable de materiales y escena de muestras. |
| `tools/desktop-source/` | Renderer y generación procedural C++ de la base PC `0.003.0.17`, con los cambios recuperados. |
| `tools/` | Exportador de escenas y preparación de materiales y shaders. |
| `tests/` | Pruebas y resultados de render de las revisiones recuperadas. |
| `AGENTS.md` | Instrucciones del proyecto para Codex. |
| `ROADMAP.md` | [Frentes de desarrollo y revisión por parche](ROADMAP.md). |
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

## Editor privado de mapas

Abre **Editor de mapas** en la edición privada del visor y selecciona un perfil. La semilla, el bioma, el relieve y las densidades alimentan la generación del terreno y del poblamiento. **Aplicar mapa** reconstruye la escena con esos parámetros. Los ajustes de calidad y sombras quedan asociados al mapa.

- **Primera persona** coloca la cámara a la altura de los ojos. Usa WASD o las flechas táctiles para caminar y arrastra para mirar.
- **Isométrica urbana** genera una red de calles con parcelas y usa una cámara ortográfica que sigue al jugador.
- **Estrategia** usa una cámara desplazable. Selecciona una unidad y pulsa un punto del terreno para darle una orden. La ruta evita obstáculos y desniveles pronunciados.

Elige una herramienta y pulsa el terreno para esculpirlo, colocar o retirar árboles, casas y rocas, o cambiar el punto de inicio. El editor guarda el mapa activo en este navegador. Exporta el JSON para crear copias o trasladar el mapa a otro equipo. El prototipo usa mapas de 64 a 180 m de lado y recursos del visor WebGL 2.

## Regenerar materiales y escenas

Blender genera los mapas de color, opacidad, normales tangentes y rugosidad del atlas. El exportador C++ actualiza las mallas y las escenas de referencia del visor desde el renderer.

```sh
blender -b -t 2 -P tools/blender/bake_surfaces.py -- dist/assets assets/blender/pleiade-surfaces.blend
python3 tools/prepare_shaders.py
python3 tools/prepare_desktop_demo.py build/materials
./build/pleiade-export-preview build/materials dist/assets
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

La base C++ incluye tres recorridos de prueba. Los controles del patio siguen el teclado y el ratón. En la vista estratégica, el botón derecho gira la cámara y la rueda ajusta el zoom.

| Recorrido | Inicio y controles |
| --- | --- |
| Primera persona | `./build/pleiade --walk-demo`. WASD para caminar, Mayús para correr, Espacio para saltar, botón derecho para mirar y Tab para alternar la cámara. El patio incluye colisiones con bloques. |
| Estrategia | `./build/pleiade --fow-demo`. Cuando el terreno cargue, haz clic en el explorador azul y luego en un punto accesible. El explorador recorrerá la ruta y se detendrá al llegar. |
| Cinemática | `./build/pleiade --water-demo --asset-dir build/materials --cinematic --no-hud`. La secuencia reproduce dos tomas en el renderer. Con `--cinematic-time 6.8` puedes saltar al corte. Al usar `--frames`, el tiempo avanza a pasos fijos de 1/60 s. |

En Windows, un generador como Visual Studio deja el ejecutable en `build/Release/pleiade.exe`. SDL2 debe estar disponible al ejecutar. La versión de prueba se compiló de forma cruzada para Windows x64. Queda pendiente ejecutarla en un equipo Windows. La distribución original de PC y sus texturas de 1024 px quedan pendientes de recuperar.

Consulta el [historial](CHANGELOG.md) y el [manifiesto de materiales](dist/assets/manifest.json) para ver procedencia, créditos y límites conocidos.
