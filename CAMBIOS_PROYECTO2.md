# Cambios de Proyecto 2

Se conservó C++17/Crow y React/TypeScript, managers existentes, extensión .mia, formatos de MBR/Partition/EBR/SuperBlock/Inode/bloques y todos los comandos funcionales del Proyecto 1. No se utilizaron recursos AWS y no se ampliaron los comandos excluidos del alcance.

## Funcionalidades implementadas

- FDISK ADD seguro en P/E/L; lectura del formato preservada tras ampliar; recortes que cortan estructuras rechazados.
- DELETE FAST/FULL en primarias, extendidas y lógicas; confirmación web que pausa/reanuda scripts sin cin. Primera operación FDISK decide el modo, conforme al ejemplo del PDF.
- UNMOUNT reinicia campos binarios y registro, invalida sesiones dependientes; correlativos disponibles se reutilizan.
- MKFS conserva EXT2 predeterminado y soporta EXT3 con Journal/Information y 50 registros persistentes.
- Journal por operaciones modificadoras exitosas, sin lecturas/fallos/cancelaciones/passwords; consulta textual y JSON/React. Capacidad llena rechaza nuevos cambios.
- LOSS limpia NUL en los dos bitmaps, tabla de inodos y área de bloques, preservando journal/sb y otras regiones; sesiones invalidadas.
- REMOVE recursivo con prevalidación completa de permisos y liberación de inodos/datos/apuntadores.
- RENAME modifica entrada y conserva contenido; COPY recursivo omite ramas no legibles; MOVE modifica referencias y .. sin duplicar inodos; FIND usa ? un carácter y * uno o más.
- API con token por navegador, login/logout/session, discos con particiones, carpetas/contenido, journaling y reportes privados. Códigos HTTP de error reales y salida de scripts conservada.
- Rutas físicas restringidas a bases configurables, mutex global, respaldos/restauración ante errores de escritura o falta de espacio.
- Reconstrucción de montajes MBR después de reiniciar, con archivos/journal persistentes y login nuevo.
- React conserva editor/componentes/diseño y agrega login, selector de discos/particiones, explorador de solo lectura, texto UTF-8, journaling, reportes SVG/texto y confirmación web.
- EXEC reutiliza el mismo analizador y scripts .smia/.txt funcionan desde selector; errores por línea no pierden salida previa.
- Puerto/host/directorios/CORS configurables, build estático y ejemplos de servicio/proxy para despliegue futuro.

## Archivos fuente modificados

| Archivos | Cambio |
|---|---|
| backend/include/commands/FDiskCommand.hpp; backend/src/commands/FDiskCommand.cpp | FAST/FULL, prioridad de operación, validateOnly para pregunta previa a eliminación. |
| backend/src/commands/MkFsCommand.cpp | Parámetro fs y dispatch EXT2/EXT3. |
| backend/include/managers/DiskManager.hpp; backend/src/managers/DiskManager.cpp | Modo full para delete, validación de reducción formateada. |
| backend/include/managers/FileSystemManager.hpp; backend/src/managers/FileSystemManager.cpp | Formato común, EXT3, validación persistente de offsets, read/appendJournal y LOSS. |
| backend/include/managers/FileManager.hpp; backend/src/managers/FileManager.cpp | REMOVE/RENAME/COPY/MOVE/FIND y helpers; protección de resolución tras LOSS. |
| backend/include/managers/MountManager.hpp; backend/src/managers/MountManager.cpp | UNMOUNT y reutilización de correlativos. |
| backend/include/structures/Ext2Structures.hpp | Information/Journal con 50 registros; estructuras anteriores preservadas. |
| backend/include/state/AppState.hpp | Datos de confirmación DELETE, sin alterar estructuras de disco. |
| backend/include/utils/StringUtils.hpp; backend/src/utils/StringUtils.cpp | Verificación UTF-8 y rechazo de contenido binario. |
| backend/src/analysis/CommandParser.cpp | Vocabulario de nuevos comandos; lexer conservado. |
| backend/src/managers/ReportManager.cpp | REP FILE/LS conserva sesión y respeta permisos de lectura. |
| backend/src/main.cpp | Wrapper transaccional, tokens/Browsers, API/vistas, invalidación, EXEC, raíces/CORS/host configurables y montajes persistentes. |
| frontend/src/App.tsx | Explorer, refrescos, confirmación genérica, compatibilidad .txt/.smia. |
| frontend/src/components/Header.tsx | Título EXT2/EXT3. |
| frontend/src/services/commandAPI.ts | Cabecera/token, helpers de sesión/API/reportes, errores de comandos conservados y API configurable. |
| frontend/src/styles/app.css | Estilos sencillos de login/explorador/tablas/visores adaptables. |
| tests/audit.py | Adaptación al protocolo token/HTTP/confirmación; conserva regresión. |

## Archivos creados

- frontend/src/components/Explorer.tsx.
- tests/project2.py y tests/browser.mjs.
- tests/08_proyecto2.smia y tests/09_loss.smia.
- tests/RESULTADOS_PROYECTO2.json y tests/RESULTADOS_FRONTEND.json.
- tests/evidence/frontend.png y tests/evidence/frontend-mobile.png.
- DOCUMENTACION_COMPLETA_PROYECTO2.md, PRUEBAS_PROYECTO2.md, CAMBIOS_PROYECTO2.md y PENDIENTE_AWS.md.
- .env.example y frontend/.env.example.
- deploy/mia-backend.service.example y deploy/nginx.conf.example.

Se actualizaron README.md, frontend/README.md, tests/README.md, .gitignore, tests/RESULTADOS.json y las evidencias de reportes de regresión. El build C++ y frontend/dist se generaron; son artefactos, no fuentes para versionar.

## Errores corregidos y decisiones

- Sesión global compartida accidentalmente: registros BrowserState aislados, identidad cargada/guardada bajo mutex.
- Offset/n calculado según tamaño actual impedía leer un filesystem tras ADD: layout persistido validado contra límites físicos.
- Operaciones fallidas podían reservar parcialmente datos: respaldo del disco y restauración, incluidas confirmaciones.
- REP FILE/LS utilizaba identidad temporal root: ahora mantiene la identidad y lectura verificadas.
- Registro de reportes relativos apuntaba al directorio incorrecto: se normaliza antes de guardar índice.
- Correlativos agotables por montajes repetidos: se escoge primer disponible.
- Visor podía tratar UTF-8 inválido como texto: validación explícita y error HTTP 415.

Journal se reserva una vez con 50 Information, siguiendo la capacidad fija indicada; el PDF también presenta una ecuación ambigua con n*Journaling. REMOVE aplica la regla normativa de no modificar nada cuando un descendiente deniega escritura, aunque el ejemplo posterior sugiera parcialidad. MOVE trabaja en una misma primaria porque no hay sintaxis definida para segunda partición. Solo primarias se montan, conforme a P1. Las fechas float/campos de longitud fija y límites del ABI se explican en el manual.

Resultados finales: regresión 1,291 verificaciones, integración P2 1,711 verificaciones y Chromium 12 flujos, todos PASS. Compilación C++, build React y lint correctos. El detalle reproducible está en PRUEBAS_PROYECTO2.md.
