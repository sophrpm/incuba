# Pruebas ejecutadas — Proyecto 2

El backend se compiló en Linux x86_64 con C++17/Crow y Asio; el frontend con React/TypeScript/Vite. La fecha de esta sesión para el usuario es 7 de octubre de 2026. Las evidencias incluyen timestamps del entorno de ejecución UTC.

## Compilación y análisis

| Comando ejecutado | Esperado | Obtenido | Estado |
|---|---|---|---|
| `/workspace/.setup-python/cmake/data/bin/cmake --build backend/build --parallel 4` | Compilar y enlazar C++ | Target 202405047_ext2_backend al 100% | PASS |
| `npm --prefix frontend run build` | TypeScript válido y dist/ | Build Vite 8 con index.html, JS y CSS | PASS |
| `npm --prefix frontend run lint` | Sin diagnósticos | Oxlint terminó correctamente | PASS |
| `git diff --check` | Sin errores de espacios/parche | Sin diagnósticos | PASS |

Estas rutas auxiliares de CMake/Asio corresponden al entorno verificador; Ubuntu usa los paquetes y comandos estándar de README. El sandbox inicialmente impidió abrir sockets; las pruebas HTTP se ejecutaron con permisos adicionales autorizados para servidores locales. No se llamó a AWS.

## Regresión del Proyecto 1

`python3 tests/audit.py`: **1,291 verificaciones**, PASS. La cantidad cuenta aserciones por bloque, no escenarios independientes. `tests/RESULTADOS.json` contiene el detalle y ABI; `tests/evidence/` contiene diez reportes y fuentes DOT.

| Casos | Comandos representativos | Esperado / obtenido |
|---|---|---|
| Discos y particiones P/E/L, ajustes FF/BF/WF, huecos y límites | MKDISK, FDISK create/add/delete | Tamaños físicos exactos, elección correcta de huecos, MBR/EBR válidos, disco sin crecimiento: PASS |
| Montaje, duplicados, IDs, limpieza | MOUNT, MOUNTED, RMDISK | Solo primarias, misma ruta canónica no se duplica, IDs correctos, limpieza al quitar disco: PASS |
| EXT2 y users.txt | MKFS, LOGIN/LOGOUT | Fórmula n/3n, raíz/usuarios correctos, credenciales exactas: PASS |
| Usuarios y grupos | MKGRP/RMGRP/MKUSR/RMUSR/CHGRP | Root exclusivo, registros eliminados ID 0, identificadores monotónicos, separadores rechazados: PASS |
| Archivos y permisos | MKDIR, MKFILE, CAT | Propietario/grupo/otros, root, padres recursivos, -cont prioritario, 664, permisos denegados: PASS |
| Directos y tres niveles indirectos | MKFILE tamaños 768,769,1793,20000,280320; CAT | Contenido exacto, índices ocupados y únicos, liberación completa al sobrescribir: PASS |
| Confirmación y errores | MKFILE existente; y/n; líneas malformadas | Pausa/continuación, cancelar conserva archivo, contadores y recuperación por línea: PASS |
| Reportes Graphviz/texto | REP diez tipos y GET privado | Salida real accesible, SVG válido y bitmap completo, FILE exacto: PASS |

Se adaptó el cliente de prueba al token X-MIA-Session, HTTP 400 real y confirmación de DELETE. MKFS -fs=2fs ahora es válido, como exige P2. Las fixtures de permisos escriben bytes solo en discos de prueba; no añaden comandos excluidos.

## Integración del Proyecto 2

`python3 tests/project2.py`: **1,711 verificaciones**, PASS. Detalle en `tests/RESULTADOS_PROYECTO2.json`. Crea discos temporales, inicia/detiene su propio Crow y vuelve a iniciarlo para probar persistencia. Comprueba estructuras con ctypes, sin pedir al backend que certifique su propia integridad.

| Caso / comandos | Resultado esperado | Resultado obtenido |
|---|---|---|
| MKFS -fs=3fs y lectura de estructuras | Journal fijo 50, offsets exactos, n/3n y raíz | Layout y contadores correctos, sin inodos/bloques huérfanos: PASS |
| Login desde API y dos clientes | Identidades separadas, fallo con credenciales incorrectas | Root/ana simultáneos; segundo navegador no hereda login: PASS |
| RENAME y nombres con espacios | Cambio de entrada, contenido intacto; duplicado inválido | Ruta nueva válida, duplicados y ../ rechazados: PASS |
| COPY archivo/carpeta con indirectos | Nuevos recursos y contenido exacto | Copia de 20000 bytes íntegra y árbol válido: PASS |
| COPY y permisos | Solo ramas sin lectura se omiten | Archivo 000 omitido, hermano legible conservado: PASS |
| COPY sin espacio | Error y restauración completa | SHA256 del disco idéntico tras fallo; sin huérfanos: PASS |
| MOVE | Mismo inodo y nuevo .., sin copiar datos | Índice original conservado, padre correcto: PASS |
| Rutas inválidas/ciclos/duplicados | Rechazo antes de corrupción | COPY/MOVE a descendiente, origen duplicado y destinos ausentes rechazados: PASS |
| REMOVE archivo/carpeta, restricciones | Liberación en postorden; denegado sin cambios | Sin huérfanos; SHA256 idéntico si un descendiente no tiene escritura: PASS |
| FIND * y ? | * uno o más, ? uno, solo ramas legibles | Coincidencias esperadas y errores de ruta: PASS |
| Journal | Solo éxito, persistencia, campos, sin passwords | Cancelación sin entrada, operaciones correctas, fecha válida, password ausente: PASS |
| Journal lleno | Máximo 50; rechazar registro 51 antes de modificar | Count 50, rechazo y SHA256 idéntico: PASS |
| LOSS | NUL en cuatro áreas; conservar sb/journal/otras regiones | Comparación byte por byte correcta, bitácora idéntica consultable: PASS |
| Invalidation | UNMOUNT/LOSS limpian sesiones de todos | Clientes quedan sin login y no ven archivos obsoletos: PASS |
| EXT2 regresión después de EXT3 | Formato predeterminado y rechazo de journal/LOSS | Archivos EXT2 válidos, errores correctos: PASS |
| UNMOUNT binario | status '0', correlativo 0, id vacío | Campos exactos y montaje ausente de MOUNTED: PASS |
| ADD después de formato | Crecimiento legible; recorte de estructuras rechazado | Archivo anterior legible tras crecer y volver a montar: PASS |
| DELETE FAST/FULL primaria | Confirmación cancelable; preservar/limpiar datos | Cancelación conserva descriptor, FAST conserva bytes, FULL NUL: PASS |
| DELETE FAST lógica/extendida | Reconectar EBR y preservar bytes; FULL borrar lógicas | Primer EBR vacío, cadena intermedia válida, datos FAST intactos, región FULL vacía: PASS |
| Primera operación FDISK | ADD inicial ignora SIZE posterior | Partición pasa 100 a 101 KiB: PASS |
| Seguridad de rutas reales | No leer/escribir /etc ni enlaces externos | -cont, REP, FDISK y RMDISK fuera de bases rechazados: PASS |
| API files/content | Metadatos y texto UTF-8; binarios rechazados | Español/Unicode correcto, bytes binarios reciben 415: PASS |
| EXEC y multilinea | Analizador compartido, comentarios y errores aislados | Comentario visible, resto correcto tras línea fallida: PASS |
| Reportes privados | SVG real, otro token no descarga | SVG válido, descarga ajena 401: PASS |
| Concurrencia | Reservas serializadas sin corrupción | Cuatro navegadores crean archivos simultáneos; árbol/bitmap válido: PASS |
| Reinicio | Persisten archivos/journal/montajes; login reiniciado | Bytes del disco intactos, dos entradas journal, archivo leído después de login: PASS |

## Interfaz real en Chromium

Comando: `MIA_PLAYWRIGHT_MODULE=/opt/codex/cua_node/lib/node_modules/playwright/index.mjs node tests/browser.mjs`.

**12 flujos**, PASS. Usa React y Crow reales, sin mocks. `tests/RESULTADOS_FRONTEND.json` registra los flujos; capturas en `tests/evidence/frontend.png` y `frontend-mobile.png`.

1. Cargar archivo .smia, conservar contenido/comentarios y ejecutar editor.
2. Mostrar error de credenciales incorrectas.
3. Login correcto, discos/particiones, navegación de carpetas y lectura exacta de archivo.
4. Confirmación de sobrescritura desde formulario y lectura posterior.
5. Journal visible con tres operaciones exitosas.
6. Reporte Graphviz SVG real dentro del iframe.
7. Dos contextos Chromium con identidades independientes.
8. Logout limpia sesión/contenido.
9. Journal sigue visible tras LOSS.
10. Bitmap posterior aparece en reportes.
11. Vista de 390 px sin desbordamiento horizontal de página.
12. Sin eventos pageerror JavaScript.

## Errores encontrados y correcciones

- Crow wvalue no tiene b(): el handler convierte el JSON a rvalue para leer success; compilación corregida.
- El cliente previo no enviaba token al descargar reportes: se adaptó al nuevo protocolo y se comprobó aislamiento.
- La fixture inicial para falta de espacio usaba un archivo que sí cabía dos veces: se aumentó a 6000 bytes para forzar el fallo y comprobar rollback exacto.
- Las rutas relativas REP estaban registrándose sin normalizar: ahora el índice privado conserva el archivo absoluto autorizado.
- La confirmación de sobrescritura y DELETE inicialmente ejecutaba fuera del respaldo: ahora se reserva y restaura también en confirmación; se verifica journal solo por éxito.
- ReadSuperBlock recalculaba n según tamaño actual: ahora valida layout persistido, preservando lectura después de ADD.
- La API tenía sesión global: BrowserState/token y mutex separan usuarios; invalidación cruza todas las identidades dependientes.
- El visor solo comprobaba caracteres de control: ahora StringUtils::isText valida UTF-8 completo y rechaza datos binarios.
- El correlativo crecía sin reutilizar números desmontados: ahora MountManager busca el primer número libre.

No quedan fallos conocidos que impidan los flujos verificados. No se probó una máquina EC2/S3 ni un dominio HTTPS, porque el alcance excluye utilizar AWS. Los límites de ABI, journal/fecha float, nombres y simulación LOSS están descritos en el manual; no se presentan como tareas de despliegue.
