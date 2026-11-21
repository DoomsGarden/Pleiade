# Versión de prueba 0.000.00.015 — sombras y contacto

- El filtro de sombras de nueve lecturas limita la corrección en superficies rasantes y funde el borde de cobertura. El renderer nativo usa pesos continuos en los bordes de texel y la normal geométrica para evaluar la sombra de la luz solar.
- La cámara estratégica nativa concentra el mapa en el área de juego y conserva la contribución de los objetos próximos fuera de pantalla. La oclusión de contacto reparte sus muestras en dos distancias para alcanzar los detalles pequeños y los objetos cercanos.

Se han comprobado los shaders y la continuidad del filtro, el visor en Chromium WebGL 2 por software y la compilación de la base C++. Queda pendiente medir el rendimiento en la tablet.

---

# Versión de prueba 0.000.00.014 — vivienda y teja

- Edificación incorpora una casa con cubierta de cuatro aguas, aleros, chimenea y huecos de ventana. Su cubierta usa una malla reutilizable de Pléiade.
- El material de teja mide las hiladas en metros y filtra el relieve conforme las tejas quedan por debajo de un píxel. La misma malla y material se integraron en las viviendas de Neón de Asfalto 3D.

Se verificaron el visor con Chromium WebGL 2 por software, las pruebas de escritorio y la firma del APK. El rendimiento en tableta queda pendiente. [Captura](tests/results/visual-014/casa-de-teja.jpg)

---

# Versión de prueba 0.000.00.013 — ventanas, vegetación y cuenca

- Los ocho paños de la cabaña se montan en huecos reales del muro y muestran el interior a través de una capa con reflejo del entorno. La veta de la madera mantiene una escala en metros. Edificación incorpora una casa de piedra y un refugio abierto.
- La mezcla de nueve variantes de los suelos se aplica en coordenadas continuas. El color, las normales y la rugosidad comparten los pesos de mezcla y la escala. Las nueve piezas geométricas de cada bioma conservan alturas y normales coincidentes en los bordes.
- Las copas tienen tres niveles de detalle y transiciones por tamaño proyectado. La corteza del abedul usa la altura y el ángulo alrededor de cada tronco para distribuir lenticelas cortas, zonas de papel desprendido y cicatrices desde la base hasta la copa. Las ramas leñosas se afinan y los puntos de rotura quedan en los datos del ejemplar.
- Orilla incorpora lago, río y cascada en una cuenca de 104 m de lado. Los flotantes avanzan con el caudal incluso con oleaje en Calma. El agua tiene profundidad propia y admite una cámara sumergida. El caudal sigue un cauce parametrizado.
- La estación modifica el follaje del abedul, que pierde hojas en otoño. Las coníferas conservan la copa. Lluvia y nieve modifican el ambiente y el suelo. En Materiales se puede probar un impacto sobre una rama que se desprende junto con sus ramillas y hojas.

El cambio se verificó en Chromium WebGL 2 por software, con cámaras iguales para comparar la cabaña, el suelo y la vegetación con la `.012`. También se comprobó el tronco del abedul desde varios ángulos y con las sombras desactivadas. El visor Android incluye los cambios. Quedan pendientes las mediciones en la tablet, la integración de la cuenca y el clima en Windows, un sistema general de fluidos y colisiones con entidades de juego. [Abedul](tests/results/visual-013/abedul.jpg) · [Primer plano](tests/results/visual-013/comparacion.jpg) · [Cuenca y clima](tests/results/visual-013/cuenca.jpg).

Referencias de la corteza, las hojas y el porte del abedul. [Royal Horticultural Society](https://www.rhs.org.uk/plants/2261/betula-pendula/details) · [Kew](https://powo.science.kew.org/taxon/urn%3Alsid%3Aipni.org%3Anames%3A295174-1/general-information) · [Forest Research](https://www.forestresearch.gov.uk/tools-and-resources/tree-species-database/131561-silver-birch-sbi-2/) · [Atlas europeo de especies forestales](https://forest.jrc.ec.europa.eu/media/atlas/Betula_spp.pdf) · [University College Cork](https://www.ucc.ie/en/tree-explorers/trees/a-z/betulapendula/).

---

# Versión de prueba 0.000.00.012 — copas, suelo y vidrio

- El bosque templado, el bambusal, los núcleos de pícea y el trópico se generan por grupos separados por claros. La pendiente, la humedad y la distancia a otros árboles condicionan cada ejemplar. Cada grupo puede incluir árboles maduros, regeneración y sotobosque. El trópico combina palmeras y árboles de frondosa tropical.
- Los pinos concentran los fascículos en varios brotes terminales. La pícea tiene ramillas alternas y el abedul más hojas por rama. El enebro oculta la base con brotes bajos y ramas más finas. Para mantener el coste, los helechos y las ramas de hoja ancha a distancia usan siluetas recortadas con mapas de opacidad. El presupuesto de agujas cercanas se mantiene en 14.000 triángulos adicionales en Ahorro.
- Cada bioma ensambla nueve piezas. Cada variante altera el relieve interior y reduce su influencia a cero junto al borde. Se pueden permutar con el mismo perfil de altura. Los materiales de suelo, incluidos los de Orilla y Edificación, seleccionan nueve fases de muestreo por semilla y las funden con una proyección continua en metros. Las montañas combinan macizos de distinta anchura y altura, con collados, laderas y un valle con la cabaña.
- La veta de la cabaña se mide en metros en el eje de cada pieza. El testero tiene un muestreo distinto. Cada ventana tiene cuatro paños con reflexión del entorno aproximada por ángulo y hora del mundo, más un fondo oscuro de interior. Los cristales no actúan como proyectores opacos de sombras. El reflejo es analítico y prescinde de capturar la habitación o los objetos vecinos.
- El trópico incluye lianas que cuelgan de copas, crecen desde el suelo y se enrollan en troncos, además de puentes cortos y trenzas entre vecinos. Su crecimiento y posición se calculan al generar la escena.

Las pruebas compararon la `.011` y la `.012` en Chromium WebGL 2 por software. La cabaña y la vegetación de cerca se capturaron desde las mismas cámaras. Los biomas usan el encuadre inicial de su versión. Se verificaron los bordes de las nueve piezas en tres semillas, incluida una permutación de las variantes. En Ahorro, las vistas iniciales del bosque y del trópico enviaron unos 385.000 y 218.000 triángulos por pasada. Queda pendiente medir la `.012` en el móvil. [Paisajes](tests/results/visual-012/paisajes.jpg) · [Primer plano](tests/results/visual-012/primer-plano.jpg).

La corteza del pino incorpora mapas escaneados de Dimitrios Savva, con licencia CC0, y los usa en la biblioteca de Blender. Las referencias de diseño incluyen fotos y descripciones de las especies, la estructura de las copas y los tipos de relieve. [Corteza de pino](https://polyhaven.com/a/pine_bark) · [Pino silvestre](https://www.forestryengland.uk/article/scots-pine) · [Abedul](https://growwild.kew.org/plants/silver-birch) · [Claros y agrupaciones](https://research.fs.usda.gov/treesearch/44828) · [Estratos de la selva](https://striresearch.si.edu/rainforest/home/rainforest-layers/) · [Lianas](https://www.si.edu/newsdesk/releases/lianas-stifle-tree-fruit-and-seed-production-tropical-forests) · [Bambú](https://www.aces.edu/wp-content/uploads/2020/11/FOR-2088-Bamboo-Growth-and-Control_110520L-A.pdf) · [Relieve glaciar](https://www.nps.gov/glac/learn/nature/glacial-geology.htm) · [Construcción de madera en Ballenberg](https://www.holzbaukultur.ch/de/bauten/haus-matten-3856_01.php).

---

# Versión de prueba 0.000.00.011 — agujas, lianas y cubierta

- Las coníferas cercanas usan agujas individuales también en Ahorro, con un presupuesto de 14.000 triángulos adicionales por pasada. El pino tiene fascículos en pares y corteza con placas y grietas. El abedul tiene lenticelas irregulares y una transición oscura hacia la base. El enebro rastrero tiene más follaje en la unión de sus brotes.
- En el trópico, la humedad local agrupa las palmeras. Las frondes orientan su arco respecto a la copa y su punta cae con mayor claridad en las hojas maduras. Las lianas enlazan dos palmeras cercanas con una curva sostenida en ambas copas. Algunas se trenzan por pares. Una palmera aislada lleva una liana colgante sin conexión a sus vecinos.
- La cubierta de la cabaña solapa ambas pendientes con el frontón y añade remates, cabios y una puerta de tablas verticales. La proyección del mapa de sombras de Edificación cubre 24 m de ancho, frente a 64 m en la `.010`, a igual tamaño de textura. Se mantiene el filtro de nueve muestras.
- Las dunas varían de altura y longitud. El mapa de arena reduce el contraste y rompe la repetición de las ondulaciones. Las montañas usan crestas más redondeadas y desplazan el relieve abrupto hacia el fondo. La altura de colocación de las rocas se interpola sobre los triángulos del terreno.
- Pruebas C++, GLSL ES y estado del visor completadas. Se compararon ambas compilaciones con Chromium WebGL 2 por software a 638 × 1280 en Detalle y se verificaron firma y contenido de la APK. Los FPS de la `.011` quedan por medir en Android.

Referencias fotográficas. [Pino silvestre, Woodland Trust](https://www.woodlandtrust.org.uk/trees-woods-and-wildlife/british-trees/a-z-of-british-trees/scots-pine/) · [Corteza de abedul, Kew](https://growwild.kew.org/plants/silver-birch) · [Palmera y frondes, UF/IFAS](https://gardeningsolutions.ifas.ufl.edu/mastergardener/resources/plantid/fruits-and-nuts/coconut/) · [Helecho macho, Plantarium](https://www.plantarium.ru/page/image/id/810263.html) · [Lianas, bioGraphic](https://www.biographic.com/rogues-of-the-rainforest/) · [Construcción de madera en Ballenberg](https://www.holzbaukultur.ch/de/bauten/haus-matten-3856_01.php).

---

# Versión de prueba 0.000.00.010 — materiales, vegetación y biomas

- Las coníferas tienen agujas curvas y el pino distribuye las agujas en pares. El abedul lleva hojas anchas con nervaduras. El helecho tiene frondes arqueadas con pinnas individuales y la campanilla tiene pétalos curvos. Una superficie continua recorre cada tronco y el pino mezcla gradualmente la corteza basal con la rojiza. Las secciones y sus relaciones siguen disponibles en la escena de origen para localizar futuras roturas.
- `assets/blender/pleiade-surfaces.blend` contiene 16 materiales editables. El visor carga dos atlas de capas para color y opacidad, y normales tangentes y rugosidad. La cabaña usa madera, mampostería, biseles y apoyos parcialmente enterrados. Las rocas varían en tamaño, forma y posición.
- Cinco escenas de terreno acotado se regeneran con la semilla del mundo. Las crestas, las dunas y el suelo se forman por altura y pendiente. La vegetación se distribuye según el hábitat local. Ahorro usa follaje, troncos y cañas de menor densidad geométrica. Equilibrado y Detalle reservan un presupuesto para sustituir esas mallas cuando ocupan más pantalla.
- El historial de PC `0.003.0.17` documenta una corrección parcial del autosombreado y una franja dentada pendiente en el contacto con el suelo. La comparación del visor `.009` y `.010` usa una cámara fija por versión, sombras activadas y desactivadas, y Chromium WebGL 2 con SwiftShader a 680 × 920 y mapa de 1024². En esas capturas no aparece una franja triangular repetida como la del informe original de PC. Las escenas tienen geometría distinta. Queda pendiente repetir la escena original de Windows y comprobar el controlador de Android. [Comparación](tests/results/contact-010/comparacion.jpg)
- Pasaron las pruebas C++ de jerarquía de plantas, los programas GLSL ES, las pruebas del visor y el puente Android simulado. Los cinco biomas renderizaron con WebGL 2 sin errores. La APK firmada se verificó junto con los recursos incluidos. Instalación y rendimiento en POCO F7 y Galaxy Tab A9+ pendientes.

---

# Versión de prueba 0.000.00.009 — vegetación procedural y APK Android

La orilla y el modo detallado del mundo usan pícea europea, pino silvestre, enebro rastrero, helecho macho y campanilla. Cada planta se regenera con su semilla. La pícea tiene copa cónica con ramas bajas que caen y puntas superiores ascendentes. El pino forma una copa más alta e irregular, con agujas en pares y corteza rojiza en la parte alta. El enebro conserva las agujas en grupos de tres, el helecho forma una roseta de frondes divididas y la campanilla tiene hojas basales redondeadas y flores colgantes de cinco lóbulos.

Cada sección de una planta conoce su sección de origen. Las raíces salen de la base y algunas raíces largas se bifurcan. Su alcance visible se dimensiona a partir del diámetro de la base y sus puntas se entierran siguiendo la pendiente. El tronco sostiene ramas principales, estas sostienen ramas intermedias y los brotes llevan el follaje. Los patrones de agujas y frondes se generan como mallas compartidas. La semilla varía la forma del ejemplar y la posición de sus brotes. La opción Ahorro del visor usa mallas de follaje simplificadas de la misma especie. En el visor, Materiales permite inspeccionar la vegetación de cerca.

La estructura de secciones deja preparada la conexión para la rotura. Tras ajustar las plantas, un contacto virtual podrá localizar su sección y el motor podrá separar solo lo que dependa del punto de rotura. El contacto, el plano de corte y la resistencia al doblado se resuelven en coordenadas de la sección. El viento queda como otro paso del motor. El campo local debe representar abrigo a sotavento, ráfagas más expuestas y canalización entre edificios o bordes de bosque. La base permanece anclada y los brotes ligeros se flexionan más que el tronco. Una planta ficticia emparentada con el enebro podría conservar las agujas en grupos de tres y variar la altura, el ángulo de las ramas y la longitud de las agujas para crecer en roca expuesta.

Prioridad de prueba del visor en POCO F7 y Galaxy Tab A9+, con la misma escena y nivel de calidad al comparar. Pruebas en dispositivos pendientes. La prueba de secciones se ejecuta con `g++ -std=c++17 -O2 tests/vegetation_check.cpp -o /tmp/pleiade-vegetation && /tmp/pleiade-vegetation`.

El empaquetado Android usa la misma versión que el visor en formato `0.000.00.000`. La APK incluye `dist` y arranca en Ahorro. Usa WebView local y el selector de Android para las capturas, informes y mundos. Requiere Android 7 o posterior y un WebView con WebGL 2. Para generar una APK firmada, ejecuta `android/build-apk.sh ANDROID_JAR BUILD_TOOLS_DIR KEYSTORE OUTPUT_APK` con la plataforma Android 35, Build Tools 35, un JDK y un certificado de firma. Conserva el mismo certificado si quieres instalar futuras revisiones encima de esta.

Referencias botánicas [pícea europea](https://forest.jrc.ec.europa.eu/media/atlas/Picea_abies.pdf), [pino silvestre](https://forest.jrc.ec.europa.eu/media/atlas/Pinus_sylvestris.pdf), [enebro común](https://forest.jrc.ec.europa.eu/media/atlas/Juniperus_communis.pdf), [helecho macho](https://www.worldfloraonline.org/taxon/wfo-0001109081) y [campanilla](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.1365-2745.2012.01963.x).

---

# Actualización 0.000.0.08 — mapa de nubes compartido

Se separa el volumen de nubes en radiancia dispersada RGB y transmisión A, almacenadas en un panorama de baja resolución. El cielo y su reflejo consultan el mismo mapa; estrellas/luna/sol continúan evaluándose de forma independiente. Filtrado bilineal explícito para no depender de filtrado lineal de texturas float. Ahorro 128×64, equilibrado 256×128, detalle 384×192. RGBA16F cuando HDR está disponible, RGBA8 en modo compatible (puede cuantizar/recortar más). El mapa equilibrado ocupa ~0,25 MiB en HDR, sin mipmaps. No se reserva un presupuesto de 4 GB.

Se invalida por posición de cámara, cobertura, parámetros del mundo, calidad y reloj en intervalos de 0,25 segundos. Girar sin trasladarse reutiliza el panorama; moverse continuamente obliga a reconstruirlo por fotograma. Las nubes desactivadas no se calculan. El modo «Reutilizar nubes · optimizado» permite comparar con el recorrido directo anterior a igualdad de ajustes. El informe exportado incluye tamaño del mapa, memoria estimada y número de actualizaciones. No es acumulación temporal ni reproyección entre cámaras.

Aproximación conocida: el reflejo del agua usa el mapa desde la posición del observador en lugar de integrar desde cada punto del agua. Es aceptable como primera aproximación para esta escena de decenas de metros frente a capas a 900+ m, pero puede perder paralaje cerca/dentro de nubes. El modo directo permanece para comparar. La baja resolución suaviza bordes; no se afirma calidad idéntica ni ganancia universal. El horizonte y los volúmenes siguen limitados por el alcance de 6 km del modelo anterior.

Verificación: cuatro programas GLSL ES compilados/enlazados; pruebas JS con mock para feedback de texturas, reutilización, rotación, cambio de altura/reloj, comparación directa y cielo despejado. Seis renders GPU de shader en Mesa llvmpipe: método directo y mapa a 2, 1300 y 4200 m. Diferencia media absoluta normalizada menor de 0,005 en estos casos; diferencias locales mayores en bordes. Resultados y tiempos de software (calentados, no RTX/Android) en tests/results/cloud-08. El coste de construir el mapa se mide separado de consultarlo. La escena completa WebGL no está validada en el controlador Android localmente. Rendimiento real pendiente del equipo de pruebas.

PC sigue siendo la prioridad. Esta entrega publica el visor, no una compilación Windows. Para reproducir los renders: compilar tests/cloud_cache_render.cpp con SDL2 y GL y ejecutar con tests como argumento. El núcleo de generación permanece propio de Pléiade; no se introduce un motor externo.

---

# Actualización 0.000.0.07 — tamaño estelar independiente de la oclusión

Corrección adicional a .06: pointStars utilizaba dFdx/dFdy y el borde solar fwidth dentro de rutas de control dependientes de profundidad. Esas derivadas son indefinidas con control no uniforme (especificación GLSL de Khronos). Se sustituyen por una huella angular acotada obtenida de resolución y campo de visión. La oclusión no debe modificar el tamaño/brillo de una estrella visible. Es una aproximación de huella óptica, también utilizada en el reflejo; no un filtro físico completo de reflexión rugosa.

Prueba nueva de GPU: ocho patrones de obstáculos de un píxel sobre un catálogo luminoso. Compara cada píxel de cielo visible con el render sin obstáculos y exige que los opacos sean negros. Verifica además que el catálogo aporta luz frente a una textura vacía. Compilación GLSL ES y pruebas JS de recursos aprobadas. Ejecutada en Mesa llvmpipe; aún pendiente de repetir la escena real en el controlador Android del usuario. No afirmar que todas las causas posibles de brillos de hojas estén resueltas. La corrección previa de niebla y antialiasing permanece.

Prioridad acordada: motor para PC, referencia Ryzen 7 5700X3D, 32 GB RAM y RTX 2060 12 GB. Tablet A9+ solo como visor de revisión. Sus 10 FPS no bloquean el desarrollo ni definen la calidad del motor. Techo de 4 GB exclusivamente como política de diseño del visor, no límite del motor PC. Consolas fuera del objetivo inmediato. Esta revisión del visor no equivale a una nueva compilación Windows.

Referencia: https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf

---

# Actualización 0.000.0.06 — separación de iluminación, cielo y oclusión

La niebla del material estaba mezclando celestialSky, incluidas estrellas y discos, sobre geometría opaca. Ahora usa solo una aproximación continua de luz atmosférica, sin cuerpos celestes. Se retira el catálogo y el cálculo lunar del shader de materiales. El antialiasing ya no mezcla vecinos de cielo ni discontinuidades importantes de profundidad con objetos opacos. El cielo se calcula una vez por píxel de fondo en vez de cinco; puede recuperar rendimiento, pero no hay medida Android de esta revisión. Puede quedar aliasing geométrico a baja resolución: este cambio corrige contaminación luminosa, no añade MSAA/TAA.

La iluminación lunar sigue siendo direccional y extendida sobre la escena, con sombras opcionales; no depende de que una estrella coincida visualmente con una hoja. Una estimación gruesa de transmisión por las dos capas atenúa la luz directa y parcialmente el ambiente de la pequeña escena. No es un mapa espacial de sombras de nubes y no resuelve luz múltiple ni variación meteorológica completa.

Agua: control de turbidez adicional, extinción a lo largo de un recorrido aproximado dependiente del ángulo. La turbidez reduce la transmisión hacia el fondo; no se usa para borrar artificialmente la reflexión Fresnel de la superficie. Reflejo SSR y aproximación de refracción siguen siendo limitados. No hay calibración fotométrica de exposición nocturna.

Perfil tablet: 4096 MiB como techo de diseño, no objetivo de consumo ni reserva. El navegador/controlador pueden imponer un límite inferior y no se mide toda su memoria desde WebGL. Se conservan las resoluciones y recursos acotados, sin llenar memoria ni aumentar calidad automáticamente. Informe exportado registra esta política. Las capturas del usuario en .05 muestran 20–21 FPS a 640 px; no atribuirlo automáticamente a RAM o a estar fuera de casa.

Validación: compilación GLSL ES y mock de recursos; regresión GPU de composición sintética antes/después con plano opaco y cielo luminoso (Mesa llvmpipe). Conserva antes/después y resultado en tests/results/lighting-06. Catálogo, guardado y orbitales conservados; transmisión de nubes acotada y monotónica. No sustituye la repetición de la escena real en la A9+. Cambio del visor; ejecutable Windows sin nueva entrega.

---

# Actualización 0.000.0.05 — estrellas puntuales, relieve lunar y capas de nubes

Corregido el aspecto de manchas: el catálogo se almacena como datos angulares RGBA32F 512×256 (2 MiB, sin mipmaps). El shader evalúa un perfil luminoso según la huella angular del píxel, con brillo, color y halo débiles. Los 1800 registros exportados permanecen intactos. La caché selecciona la fuente más brillante por celda; en la semilla 12345 representa 1792. Los polos y la selección por celda aún requieren una solución jerárquica para catálogos densos. No simula instrumentos astronómicos ni resuelve superficies estelares lejanas.

Luna ficticia con regiones de albedo, cráteres procedurales, normales aproximadas y terminador por incidencia solar. Se reduce su radio angular de .018 a .007 radianes. No usa mapas lunares terrestres. El sol tiene radio .006 rad, oscurecimiento hacia el limbo y granulación visual aproximada; sin corona ni plasma ni exposición solar dedicada. Ambos siguen siendo cuerpos de dirección/tamaño angular, no superficies planetarias navegables.

Dos capas de densidad: 900–1700 m y 3200–3650 m. Integración de extinción y luz aproximada, hasta 24 muestras por capa y alcance 6 km; viento coherente con reloj y semilla. Incluye atenuación de estrellas/luna/sol y reflejo de nubes en agua. El color responde a espesor óptico e iluminación, no a una etiqueta de humedad. No es simulación termodinámica: faltan condensación, precipitación, sombras de nubes sobre suelo, dispersión múltiple y transición planetaria. Distancia limitada y bajo muestreo pueden generar pérdida de detalle; nubes experimentales desactivadas por defecto.

Controles: cobertura, altura de inspección 2–6000 m y campo de visión 8–75 grados. Se puede mirar hacia abajo en modo cielo y atravesar las capas. El escenario de suelo sigue siendo una pequeña escena exportada con alcance de cámara limitado, no un terreno planetario. La altura/cobertura son controles de inspección, no datos de mundo guardados. Se mantienen importación/exportación v1 y guardado del catálogo/reloj.

Pruebas: catálogo determinista, exportación/importación, orbitales; compilación y enlace de tres programas GLSL ES en Mesa; mock JS a ocho altitudes, controles, recursos y zoom. Ocho renders del shader celeste en Mesa llvmpipe: cuatro horas, nubes debajo/dentro/encima y detalle lunar. Evidencia tests/results/sky-05. No equivalen a ejecutar la escena WebGL completa en Android; el coste real A9+ está pendiente. El ajuste final del sol se recompila y se incluye en las capturas. Fuentes de referencia: https://www.weather.gov/source/zhu/ZHU_Training_Page/clouds/cloud_development/clouds.htm y https://www.eso.org/observing/etc/doc/helpsphere.html .

Siguiente bloque de arquitectura: coordenadas de mundo en doble precisión y origen relativo a cámara; cuerpos con radio, posición, órbita y referencia local; generación por región con semillas persistentes; selección de detalle por tamaño angular. Solo entonces conectar suelo, vuelo atmosférico y aproximación a planetas/estrellas. Galaxias, viaje espacial continuo y vehículos/unidades de estrategia tridimensionales todavía no están implementados. No presentar esta entrega como un universo navegable completo. Cambios del visor aún pendientes de integración Windows.

---

# Actualización 0.000.0.04 — cielo propio de cada mundo

Primera integración celeste en el visor: 1800 estrellas ficticias por semilla, catálogo versionado, rotación de la esfera celeste, sol diario y luna con periodo orbital propio de 19–31 días. No se utilizan catálogos terrestres. Un día dura 720 segundos de ejecución activa. El ciclo se activa en Ajustes; el deslizador permite inspeccionar cualquier hora. El reloj se pausa al ocultar/cerrar la página. No representa astronomía terrestre ni simulación gravitacional.

El mundo activo se conserva en localStorage, con guardado periódico cada cinco segundos y al salir cuando el navegador lo permite. Exportar mundo guarda catálogo, semilla, versión, reloj y parámetros orbitales en JSON; importar restaura ese estado con validación. Exportar antes de sustituirlo conserva varios mundos y permite moverlos entre dispositivos; no hay sincronización de archivos en servidor. La reproducibilidad está ligada al formato v1 y el catálogo explícito evita depender de generadores futuros.

El ciclo afecta luz directa, mapa de sombras, ambiente aproximado, niebla y reflejo del cielo en agua. La luna tiene fase iluminada por el sol. Solo una luz direccional dominante produce sombras; no hay sombras simultáneas solar/lunar. La atmósfera es analítica aproximada, sin dispersión múltiple, nubes volumétricas, estaciones ni eclipse. Referencia de trabajo futuro: https://ebruneton.github.io/precomputed_atmospheric_scattering/index.html (no se ha implementado ese modelo).

Presupuesto: atlas estelar RGBA8 2048×1024 con mipmaps (~10,7 MiB), generado solo al cambiar/importar mundo. El ciclo obliga a actualizar sombras y puede costar más que .03; falta medir en A9+/RTX. La implementación celeste está en dist/celestial.js y tools/shaders/celestial.glsl; aún no integrada en el ejecutable Windows. prepare_assets.py aplica automáticamente el shader celeste al regenerar assets.

Validación: 241 estados orbitales finitos, vectores unitarios, semillas iguales/distintas, catálogo determinista, exportación/importación y rechazo de formatos inválidos; mock JS de recursos y controles, día/noche y cámara; tres programas GLSL ES compilados/enlazados en Mesa de escritorio; cuatro renders del shader de cielo en Mesa llvmpipe (mediodía, crepúsculo, medianoche y 03 h), evidencia en tests/results/sky-04. No equivalen a ejecutar la escena completa en WebGL Android. El navegador remoto sigue sin WebGL2 verificable. No se presenta rendimiento ni calidad final como comprobados en tablet.

Para repetir: node tests/celestial_check.mjs; node tests/runtime_check.mjs; python3 tests/assets_check.py. Compilar tests/sky_render.cpp con SDL2 y GL y ejecutarlo con tests como argumento para capturar PPM. El atlas de pruebas se regenera con celestial_check.mjs.

---

# Actualización 0.000.0.03 — óptica y estabilidad del agua

Reflexión dieléctrica de Fresnel para agua (índice 1,333) y recorrido SSR con interpolación correcta en perspectiva, hasta 48 pasos y cinco refinamientos por cruce candidato. No es una implementación completa de DDA. En el peor caso varios cruces repiten el refinamiento; el coste debe medirse en los dispositivos objetivo. SSR sigue desactivado de inicio. Se añaden agua en calma, ondulación suave (35 %, predeterminada) y ondulación anterior (100 %). La normal y la altura usan la misma intensidad.

Ocho renders nativos 960×540 en Mesa llvmpipe: calma idéntica píxel a píxel entre tiempos 0 y 3; ondulación y SSR producen cambios comprobados; cámara de estrategia ejecutada. La comparación visual elimina las franjas del bloque en esta cámara. No demuestra ausencia de defectos en todas las vistas: siguen faltando datos ocultos/fuera de pantalla y pueden perderse objetos finos. No hay validación de rendimiento RTX/Android ni ejecución WebGL real en esta revisión. Evidencia en tests/results/water-03. El código C++ queda conservado en tools/desktop-source como parche pendiente de una entrega nativa; no es una nueva versión Windows.

Referencias: [PBRT, reflexión y transmisión especular](https://www.pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission), [McGuire y Mara, recorrido de rayos en pantalla](https://jcgt.org/published/0003/04/04/paper.pdf). Una superficie ondulada cambia la dirección reflejada aunque el árbol esté quieto; el objetivo es eliminar saltos artificiales, no inmovilizar la reflexión.

Ruta de trabajo: cerrar defectos de agua y contacto de sombras; después validar materiales a escala real (color, normal, rugosidad), biseles y siluetas; luego iluminación y exposición coherentes en una escena pequeña. Cada bloque requiere comparación controlada, pruebas funcionales y medición en PC/tablet antes de extenderlo al mundo completo. Realismo, estabilidad y presupuesto de recursos se evalúan por separado.

---

# Actualización 0.000.0.02 — sombras del visor

Filtro tent continuo de nueve muestras, con comparación de profundidad del plano receptor por muestra. Elimina el salto brusco del filtro de caja al cambiar de celda; no equivale a sombras de área ni resuelve el contacto irregular terreno/edificio pendiente en PC. Casilla «Suavizar bordes de sombras» permite comparar con filtro anterior a la misma resolución. No se añade ningún mapa ni pasada por el filtro; hay coste aritmético adicional sin medir en la A9+.

Ahorro: pantalla 640 / sombras 512². Equilibrada: pantalla 960 / sombras 1024². Detalle: pantalla 1280 / sombras 2048². El mapa de profundidad Detalle pasa de aproximadamente 4 a 16 MiB si el formato se almacena en 4 bytes/texel; no representa memoria GPU total ni asignación real medida. La pasada de sombras se reutiliza mientras escena/calidad no cambien.

Pruebas: tres programas GLSL ES compilados/enlazados; integridad de assets y pruebas JS con mock; 1000 posiciones para continuidad/normalización del filtro; 16 renders nativos del mismo filtro en Mesa llvmpipe con dos cámaras, dos resoluciones, con/sin sombras/pilar. Plataforma vacía idéntica con/sin sombras, contacto del pilar conservado. Resultados en tests/results/shadow-filter-02.json. El render WebGL de esta revisión y su coste en Android siguen pendientes: el navegador remoto no ofrece WebGL 2. La 0.000.0.01 sí fue confirmada visualmente y fluida por Dooms en su A9+; no se extrapola esa confirmación a .02.

## Objetivo visual y siguiente escena

Space Marine 2 y el remake de Dead Space son referencias de calidad, no una equivalencia alcanzada. Pléiade mantiene render propio. Próxima escena de validación propuesta: interior industrial original con hormigón, metal, bordes biselados, luces locales y zonas húmedas, para evaluar composición, escala de materiales y reflejos juntos. Antes de ampliar el escenario: reproducir/corregir contacto terreno/edificio y defectos del SSR. Modelado, animación, herramientas de creación y rendimiento son líneas de trabajo además del render. No copiar recursos de los juegos de referencia ni introducir un motor externo. Cada entrega exige pruebas y evidencia; no hay promesa de alcanzar calidad AAA completa en una sola sesión.

---

# Pléiade · Tablet 0.000.0.01

Visor WebGL 2 de escenas exportadas de Pléiade PC 0.003.0.17. Objetivo: Samsung Galaxy Tab A9+, 8 GB RAM física, Brave preferido / Chrome alternativo. No es el motor de juego completo ni una compilación WebAssembly del motor C++.

## Compartido y adaptado

El exportador C++ utiliza Renderer, makeShoreScene y sus mallas reales. La carpeta tools/desktop-source contiene la fuente de esa exportación (el único cambio de acceso es friend PreviewExporter). Los shaders se extraen de shaders.hpp, se convierten a GLSL ES 3.00 y se normalizan los literales numéricos que requieren tipos explícitos. La capa de recursos, cámara, controles y render WebGL está escrita en JavaScript. Los mapas originales PLTX se reducen de 1024² a 512², con canales independientes y normales renormalizadas.

Tres selecciones: orilla, estudio de materiales en seco y edificación de diagnóstico sobre terreno irregular. Incluye cámara orbital táctil, pellizco/traslación con dos dedos, tres límites de resolución, captura PNG e informe JSON bajo demanda. Ningún informe se envía automáticamente. Objetivo 30 FPS; sin promesa de rendimiento. Las escenas quedan descargadas durante la sesión, pero abrir/recargar necesita conexión. No hay sincronización automática con ejecutables del PC: las nuevas revisiones se exportan y publican en el mismo Site.

## Limitaciones

Sin streaming, juego, FoW ni capas de vegetación por distancia. SSR experimental desactivado de inicio. Los defectos de contacto de sombras de .17 no se consideran resueltos aquí. Materiales compartidos no implican igualdad visual exacta con PC. Si EXT_color_buffer_float falta, se usa RGBA8 con brillos limitados y contacto ambiental desactivado. Pérdida de contexto muestra recuperación por recarga. Animación se pausa al ocultar la página.

## Verificación realizada

- node --check dist/app.js y dist/test.js: aprobado.
- node tests/runtime_check.mjs: aprobado. Mock estricto de recursos, combinaciones escena/calidad, matrices finitas, feedback de texturas, gestos y cambio a vertical. NO equivale a ejecutar WebGL.
- python3 tests/assets_check.py: aprobado. Índices/mallas, transformaciones, seis mapas y longitud de normales.
- tests/compile_gles.cpp: tres programas GLSL ES 3.00 compilan/enlazan con Mesa mediante contexto GL de escritorio compatible con ES. NO es validación del controlador Android.
- Interfaz abierta con Chrome remoto: navegación de ajustes y aviso de incompatibilidad comprobados. Ese navegador no permite crear WebGL 2; no hubo renderizado WebGL verificable en él. La prueba visual y el rendimiento real en Brave/Chrome Android están pendientes.
- Ruta ?test ejecuta una regresión de imágenes en un navegador compatible. No se presenta como ejecutada aquí.

## Reproducir exportación

Usar el paquete PC 0.003.0.17, sus assets/runtime y SDL2. Compilar tools/export_preview.cpp con tools/desktop-source/renderer.cpp, include tools/desktop-source y SDL2, enlazar SDL2. Ejecutar BIN RUTA_PC/assets/runtime dist/assets. Ejecutar python3 tools/prepare_assets.py RUTA_PC. La preparación requiere NumPy/Pillow. Mantener la declaración friend PreviewExporter en el header utilizado para exportar.

Materiales CC0 de Poly Haven; fuentes, autores y hashes de origen en dist/assets/manifest.json e interfaz. No hay dependencias CDN.
