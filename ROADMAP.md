# Motor Pléiade

Los perfiles reutilizables de Codex están en [`.codex/agents/`](.codex/agents/).

| Frente | Base de trabajo | Próxima prueba de aceptación |
| --- | --- | --- |
| Primera persona · Neris | Controlador a 60 Hz con salto, escalones y pendientes | Recorrer una escena con puertas, escaleras y colisiones tridimensionales a distintas tasas de fotogramas |
| Estrategia · Talven | Cámara orbital, selección por rayo y órdenes de movimiento a un explorador | Seleccionar grupos, dar órdenes y conservar la navegación al cargar y descargar sectores |
| Cinemáticas y cielo · Ivara | Pista de cámara con tiempo, objetivo, óptica y cortes | Reproducir la misma secuencia con personajes y luces a distintas tasas de fotogramas |
| Render y espacio · Auren | Sombras ancladas al mundo, cielo ficticio y coordenadas relativas a la cámara | Conservar escala local al pasar de superficie a órbita. Incorporar un nivel independiente para sistemas estelares y otro para ver una galaxia desde fuera |
| Calidad · Vésper | Pruebas C++, WebGL, assets y puente Android | Revisar cada parche. Reproducir fallos y medir regresiones antes de valorar la integración |

El objetivo visual es que luz, materiales, reflejos, movimiento y exposición se comporten de forma coherente en primera persona, estrategia y cinemáticas. Las escenas de prueba deben incluir primeros planos, desplazamiento de cámara, distancias largas y cambios de iluminación. Las mediciones de rendimiento identifican GPU, resolución, ajustes y versión. El render por software comprueba funcionamiento visual y compatibilidad del código.

El cielo de cada mundo se genera por semilla y se puede exportar e importar. La estrella principal, el satélite y el catálogo estelar son ficticios. El agua y el vidrio comparten la radiancia direccional del cielo en el visor WebGL. La navegación espacial se construirá con coordenadas sectorizadas, representación relativa a cámara y niveles de detalle distintos para terreno, planeta, sistema estelar y galaxia. El alcance numérico ya se prueba a 2,5 millones de años luz. La generación y el vuelo a esas escalas requieren integrar esos niveles de representación.

## Revisión y numeración

Cada ejecución elige una mejora comprobable, documenta la prueba y deja sus cambios en revisión. Vésper verifica funcionamiento, limpieza, bugs, rendimiento y cobertura. Lumen recibe sus hallazgos por parche para reproducirlos y valorar el resultado.

Los relevos se programan todos los días en ventanas escalonadas de ocho horas. Cada agente puede repartir sus ocho horas de descanso y dejar al relevo el estado del cambio, las pruebas y los bloqueos.

Los parches públicos conservan el formato `0.000.00.NNN`. En cada múltiplo de diez, Vésper entrega primero a Lumen el informe preliminar y las evidencias. El informe para DoomsGarden se envía después de registrar la valoración de Lumen. El primer bloque abarca `0.000.00.011` a `0.000.00.020`. Incluye fallos corregidos y abiertos, mejoras medibles y las comprobaciones aún pendientes en GPU o Android.
