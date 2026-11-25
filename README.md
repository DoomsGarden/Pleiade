# Pléiade

Pléiade es un motor gráfico propio. Este repositorio conserva el visor WebGL 2, su paquete Android y la fuente C++ recuperada de la base PC.

## Instaladores de prueba

La APK y el instalador EXE x64 de Windows incluyen el visor WebGL y sus recursos para usarlo sin conexión. Android requiere la versión 7 o posterior y un WebView con WebGL 2. Windows requiere Microsoft Edge WebView2 Runtime. [Descargas en GitHub Releases](https://github.com/DoomsGarden/Pleiade/releases)

La demo nativa de C++ se distribuyó en un paquete x64 con SDL2, materiales y los tres recorridos de prueba. [Paquete nativo .018](https://github.com/DoomsGarden/Pleiade/releases/tag/v0.000.00.018)

- **Orilla** permite explorar una cuenca de 104 m de lado en modo lago, río o cascada. En **Ajustes → Mirar bajo el agua** se puede inspeccionar el fondo. En **Río** y **Cascada**, **Ver pasarela** encuadra el cruce de madera aguas arriba, con apoyos adaptados a la semilla.
- **Edificación** permite alternar entre la cabaña, la casa de teja, el refugio de piedra y el pabellón abierto. **Biomas** abre cinco paisajes que se regeneran con la semilla del mundo.
- En **Ajustes → Estación y clima** se puede seleccionar la estación o vincularla al reloj del mundo. En **Materiales**, ese mismo panel permite desprender una rama de prueba y restaurarla.

Las sombras suavizadas atenúan el borde de cobertura y estabilizan la corrección en superficies rasantes. En C++, la cámara estratégica concentra la resolución cerca del jugador y las muestras de contacto cubren dos distancias.

Las pruebas de rendimiento en la tablet quedan pendientes. Los instaladores comparten la versión del visor publicada en GitHub.

## Archivos del proyecto

| Ruta | Contenido |
| --- | --- |
| `dist/` | Visor WebGL 2 y recursos listos para servir. |
| `android/` | Contenedor Android y script para compilar y firmar. |
| `windows/` | [Contenedor WebView2 e instalador x64](windows/README.md). |
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

### Vuelo atmosférico · candidato `0.000.00.019`

En **Ajustes → Cielo y mundo → Volar por la escena**, la cámara toma la posición y dirección actuales. Mantén **WASD** para desplazarte y **E/Q** para subir o bajar. Arrastra para mirar. En pantalla táctil se pueden mantener pulsados los controles de movimiento y arrastrar la vista con otro dedo. **Salir del vuelo** o Esc recupera la cámara anterior.

La cámara mantiene margen sobre el terreno y el agua. Puedes despegar desde las escenas locales y ascender hasta 6 km sobre el nivel de referencia, por encima de las dos capas de nubes. El recorrido conserva los límites horizontales de las escenas de prueba.

Desde `0.000.00.021`, activa **Cobertura de nubes**, fija la altura de inspección en **2.200 m** y pulsa **Volar por la escena**. Arrastra para mirar hacia el lago a través de la capa inferior. Repite a **350 m** para ver la orilla desde debajo de la capa. Las nubes siguen desactivadas al inicio. Desde `0.000.00.023`, las nubes que quedan delante de una ventana también atenúan su reflejo.

Desde `0.000.00.029`, fija **Altura de inspección** en **6.000 m** y **Campo de visión** en **8°**. Arrastra hacia arriba o usa la flecha abajo con el lienzo enfocado para mirar verticalmente al suelo. Puedes girar ese encuadre y desplazarlo con dos dedos. **Volar por la escena** toma el mismo rumbo e inclinación.

## Regenerar materiales y escenas

Blender genera los mapas de color, opacidad, normales tangentes y rugosidad del atlas. El exportador C++ actualiza las mallas y las escenas de referencia del visor desde el renderer.

```sh
blender -b -t 2 -P tools/blender/bake_surfaces.py -- dist/assets assets/blender/pleiade-surfaces.blend
python3 tools/prepare_shaders.py
python3 tools/prepare_desktop_demo.py build/materials
./build/pleiade-export-preview build/materials dist/assets
```
## GitHub Pages

En **Actions → Visor en GitHub Pages**, selecciona `main` e introduce la etiqueta de una versión ya publicada en GitHub Releases. La ejecución prepara y valida `dist/` de esa versión. El artefacto de la ejecución permite revisar el contenido antes de publicar.

Para publicar, el propietario debe elegir **GitHub Actions** como fuente en **Settings → Pages**. Después puede iniciar la misma ejecución con **Publicar el visor en GitHub Pages** activado. El sitio sirve el JavaScript, los estilos y los recursos de `dist/` de la versión seleccionada. En esta cuenta personal, ese contenido será público si se activa Pages. [Configuración de GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)

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
| Estrategia | `./build/pleiade --fow-demo`. En el candidato `0.000.00.020`, haz clic en un explorador y usa Mayús+clic para añadir otros. Haz clic sobre terreno accesible para ordenar el desplazamiento. El minimapa distingue los exploradores seleccionados. |
| Cinemática | `./build/pleiade --water-demo --asset-dir build/materials --cinematic --no-hud`. La secuencia reproduce dos tomas en el renderer. Con `--cinematic-time 6.8` puedes saltar al corte. Al usar `--frames`, el tiempo avanza a pasos fijos de 1/60 s. |

En Windows, un generador como Visual Studio deja el ejecutable en `build/Release/pleiade.exe`. SDL2 debe estar disponible al ejecutar. La versión de prueba se compiló de forma cruzada para Windows x64. Queda pendiente ejecutarla en un equipo Windows. La distribución original de PC y sus texturas de 1024 px quedan pendientes de recuperar.

Consulta el [historial](CHANGELOG.md) y el [manifiesto de materiales](dist/assets/manifest.json) para ver procedencia, créditos y límites conocidos.
