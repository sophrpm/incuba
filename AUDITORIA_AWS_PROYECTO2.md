# Auditoría del PDF y adaptación mínima a AWS

Fecha: 13 de octubre de 2026. Proyecto MIA_2S2026_P2_202405047.

## Alcance y fuentes

Se revisaron las 36 páginas físicas del PDF `773_Proyecto_2_2S2026.pdf`, el TXT de esta solicitud, el código actual y los archivos de despliegue existentes. Las páginas citadas son posiciones físicas del PDF, no los números abreviados de su índice. El TXT define cambios mínimos y excluye expresamente la implementación de CHMOD/CHOWN.

El repositorio ya contenía numerosos cambios de la implementación anterior. Se registró su estado y hashes antes de editar; no se restauraron, sobrescribieron ni incorporaron a un commit. En esta auditoría no se modificó ningún `.cpp`, `.hpp`, `.tsx`, `.ts`, CMakeLists ni dependencia. La única configuración existente modificada fue `deploy/nginx.conf.example`. Se agregaron este informe y la guía `PASOS_DESPLIEGUE_AWS.md`, con enlaces en README.

No se consultaron ni modificaron recursos AWS. El entorno no tiene una identidad AWS conectada. No se afirma que el despliegue remoto exista o haya sido probado.

## Arquitectura exigida y decisión

El PDF exige explícitamente frontend en bucket S3 y backend en EC2 Linux en las páginas 4–5, 27–29 y 32–35. Por tanto, publicar solo React en EC2 y utilizar S3 únicamente para respaldos no cumple ese requisito.

Arquitectura mínima:

```text
Navegador carga React compilado desde S3
    └── fetch a http://IP_PUBLICA_EC2/api/...
            └── Nginx EC2 puerto 80
                    └── Crow 127.0.0.1:2611, servicio systemd
                            ├── data/disks/*.mia
                            ├── data/inputs/
                            └── data/reports/ + Graphviz
```

Solo una EC2 y un bucket S3. Discos binarios y reportes permanecen en almacenamiento local persistente de EC2. Crow no necesita SDK, credenciales, rol IAM ni llamadas S3. La comunicación entre orígenes requiere `VITE_API_URL` al backend y `MIA_CORS_ORIGIN` al origen exacto del sitio S3; ambas opciones ya existen.

El sitio web estático público de S3 requiere lectura pública de los objetos del build. Se justifica únicamente `s3:GetObject` en un bucket dedicado a `frontend/dist/`; no se publican `.mia`, entradas, reportes, claves, fuentes ni otros datos. No se desactiva indiscriminadamente la protección de otros buckets. La guía explica los controles de acceso y las restricciones a nivel de cuenta.

## Matriz de requisitos del PDF

“Cumple” significa implementación integrada e inspeccionada con prueba local de los casos indicados; no certifica AWS. “Parcial” identifica una diferencia o ambigüedad real. “No verificado” corresponde a infraestructura/entrega que no puede comprobarse aquí. Las referencias agrupadas a managers incluyen sus encabezados homónimos.

| Requisito del PDF | Archivo responsable | Estado | Problema | Acción |
|---|---|---|---|---|
| Backend exclusivamente C++, Linux (pp. 4–5, 28, 32, 34) | backend/CMakeLists.txt; backend/src/main.cpp | Cumple local / No verificado Ubuntu EC2 | Build limpio probado en Debian 13, no en una EC2 Ubuntu nueva | Mantener; compilar en EC2 según guía |
| Framework moderno frontend (pp. 4, 28, 32) | frontend/package.json; frontend/src/main.tsx | Cumple | React/TypeScript/Vite, build productivo pasa | Mantener |
| Frontend en S3 (pp. 4–5, 27–29, 32, 34–35) | frontend/src/services/commandAPI.ts; frontend/dist generado | No verificado remoto | No se publicaron objetos ni se comprobó URL S3 | Mantener código; publicar solo dist según guía |
| Backend en EC2 y Security Groups (pp. 5, 27–29, 32, 34) | deploy/mia-backend.service.example; deploy/nginx.conf.example | No verificado remoto | No hay EC2 accesible/configurada en esta sesión | Corregir ejemplo Nginx; desplegar según guía |
| Conexión S3 → EC2 (p. 35) | frontend/src/services/commandAPI.ts; backend/src/main.cpp CORS | Cumple local / No verificado remoto | Producción requiere URL/origen reales; copiar localhost a build remoto falla | Mantener; configurar variables antes de build/inicio |
| Terminal, botones y salida (pp. 5–6, 27, 29) | frontend/src/App.tsx; components/CommandEditor.tsx, Buttons.tsx, OutputPanel.tsx | Cumple | Prueba de Chromium estático ejecuta comandos reales | Mantener |
| Scripts .smia y comentarios en salida (pp. 27, 29) | App.tsx; FileChooser.tsx; backend/src/main.cpp processInput; analysis/ | Cumple | Archivo cargado/ejecutado, comentarios preservados, errores por línea aislados | Mantener |
| Login por interfaz y sesión para comandos (pp. 6, 10, 27) | frontend/src/components/Explorer.tsx; backend/src/main.cpp /api/login; managers/UserManager.cpp | Cumple | LOGIN de terminal se conserva adicionalmente por compatibilidad; GUI funciona | Mantener |
| Botón logout (p. 10) | Explorer.tsx; /api/logout; UserManager.cpp | Cumple | Cierra identidad y limpia contenido visible | Mantener |
| Selección de disco con capacidad/fit/fecha/montajes (p. 7) | Explorer.tsx; backend/src/main.cpp /api/disks; DiskManager.cpp | Cumple | Datos leídos de MBR; tabla muestra particiones montadas | Mantener |
| Selección de partición y tamaño/fit/estado (p. 8) | Explorer.tsx; /api/disks | Cumple en primaria de sesión | Para navegar otra primaria se requiere su login; seleccionarla no suplanta identidad | Mantener |
| Navegación desde / y carpetas (pp. 8–9) | Explorer.tsx; /api/files; managers/FileManager.cpp | Cumple | Navegación y Subir probados | Mantener |
| Explorador de solo lectura (pp. 7, 9, 27–28) | Explorer.tsx | Cumple | Cambios se realizan por terminal; visor no modifica archivos | Mantener |
| Metadatos, permisos y contenido de texto (pp. 9–10, 28, 35) | Explorer.tsx; /api/files/content; FileManager.cpp; utils/StringUtils.cpp | Cumple | UTF-8 correcto; binarios reciben 415 | Mantener |
| Conservar comandos previos (pp. 14, 34) | backend/src/commands/; managers/; analysis/CommandParser.cpp | Cumple en regresión | 1,291 aserciones del Proyecto 1 pasan | Mantener |
| Discos .mia y estructuras binarias (p. 4, estructuras pp. 11–13) | managers/DiskManager.cpp; include/structures/DiskStructures.hpp; utils/BinaryUtils.hpp | Cumple | MBR/EBR/límites/huecos validados independientemente | Mantener |
| FDISK size/unit/path/type/fit/name, P/E/L, 4 entradas/1 extendida (pp. 14–16) | commands/FDiskCommand.cpp; managers/DiskManager.cpp | Cumple | Parámetros, límites y BF/FF/WF comprobados | Mantener |
| FDISK ADD positivo/negativo y espacio contiguo (pp. 16–17) | FDiskCommand.cpp; DiskManager.cpp | Cumple | Recorte de estructura formateada se rechaza; crecer conserva layout anterior | Mantener |
| FDISK primera operación ignora otras (p. 17) | backend/src/main.cpp executeCommand; FDiskCommand.cpp | Cumple | ADD anterior a SIZE produce delta esperado | Mantener |
| DELETE FAST/FULL con confirmación (pp. 16–17) | main.cpp processInput; FDiskCommand.cpp; DiskManager.cpp | Cumple | Pregunta web, cancelación, FAST conserva datos y FULL NUL | Mantener |
| Eliminar extendida elimina referencias lógicas (p. 16) | DiskManager.cpp | Cumple | FULL limpia EBR/datos; FAST retira contenedor y conserva bytes desconectados | Mantener |
| UNMOUNT id, correlativo 0 y desaparición de MOUNTED (p. 17) | main.cpp; MountManager.cpp; MountedCommand.cpp | Cumple | Campos MBR y lista verificados; sesiones dependientes invalidadas | Mantener |
| MKFS full, 2fs/3fs y EXT2 por defecto (pp. 18–19) | commands/MkFsCommand.cpp; managers/FileSystemManager.cpp | Cumple | Ambos formatos, raíz/users.txt, contadores y bitmaps probados | Mantener |
| EXT3: 3n bloques, zonas y fórmula de n (pp. 11–13) | FileSystemManager.cpp format; include/structures/Ext2Structures.hpp | Parcial respecto a lectura literal de ecuación | PDF escribe n*sizeof(Journaling), pero también fija constante 50. Código reserva un Journal de 50 Information una sola vez | Mantener formato probado; documentar y confirmar interpretación con evaluador antes de cambiar ABI/layout |
| Campos Journal/Information (p. 13) | Ext2Structures.hpp; FileSystemManager.cpp | Cumple con capacidad fija | count int, arrays 10/32/64, date float, 50 entradas; campos y fecha tienen límites de precisión/tamaño | Mantener |
| REMOVE recursivo y escritura (pp. 19–20) | FileManager.cpp remove/collectTree | Cumple regla normativa / Parcial frente al ejemplo | Texto exige no eliminar nada si un descendiente deniega; ejemplo sugiere borrar solo hijos permitidos. Se aplica todo o nada | Mantener decisión documentada; no cambiar una operación segura por ejemplo contradictorio |
| RENAME escritura, nuevo nombre sin duplicados (p. 20) | FileManager.cpp rename/changeEntry | Cumple | Cambia referencia y conserva contenido; errores comprobados | Mantener |
| COPY recursivo y omisión de ramas sin lectura (pp. 21–22) | FileManager.cpp copy/copyTree | Cumple | Contenido e indirectos, permisos, ciclos, duplicados y espacio insuficiente probados | Mantener |
| MOVE referencias en misma partición, permisos destino/origen (pp. 22–23) | FileManager.cpp move | Cumple caso definido | Conserva índice/inodo, modifica ..; no hay sintaxis definida para segunda partición | Mantener |
| FIND recursivo, ? uno, * uno o más y lectura (pp. 23–24) | FileManager.cpp find/findTree/matchesPattern | Cumple búsqueda | Salida usa rutas completas en vez del dibujo de árbol del ejemplo | Mantener |
| LOSS NUL en dos bitmaps, inodos y bloques (p. 25) | FileSystemManager.cpp simulateLoss; main.cpp | Cumple | Comparación binaria antes/después prueba zonas exactas y preservación de journal | Mantener |
| Bitmaps antes/después visibles (pp. 25–26) | ReportManager.cpp; Explorer.tsx vista Reportes | Cumple | REP genera ambos momentos; reporte posterior visible tras LOSS | Mantener |
| JOURNALING operación/ruta/contenido/fecha/hora en GUI (p. 26) | FileSystemManager.cpp readJournal/appendJournal; main.cpp /api/journaling; Explorer.tsx | Cumple casos probados | No depende de Graphviz; persiste tras reinicio/LOSS; 50 registros máximos | Mantener |
| Graphviz obligatorio (p. 32) | managers/ReportManager.cpp generateGraphviz | Cumple | dot invocado con argumentos separados; SVG real servido y mostrado en Chromium | Mantener |
| Manual técnico, estructuras/comandos y despliegue (pp. 27–29, 33) | DOCUMENTACION_COMPLETA_PROYECTO2.md; este informe; PASOS_DESPLIEGUE_AWS.md | Parcial como entrega final | Documentación local amplia; no hay evidencia/URLs de despliegue real | Mantener; completar evidencia después de desplegar |
| Manual usuario con capturas de cada paso (pp. 27, 29) | DOCUMENTACION_COMPLETA_PROYECTO2.md; tests/evidence/ | Parcial | Hay capturas generales, no una captura por cada paso remoto | Mantener; añadir capturas del sitio final durante entrega |
| Repositorio GitHub privado y acceso auxiliar (pp. 28–29, 32) | Configuración externa de GitHub | No verificado | No se consultó privacidad, permisos ni entregas parciales | Mantener; verificar desde cuenta propietaria |
| Sitio nube, conexión y URLs de evaluación (pp. 29, 32–35) | EC2/S3 externos | No verificado | Pruebas locales no certifican nube | Ejecutar checklist remoto de la guía |
| CHMOD/CHOWN mencionados (pp. 24–25, 31, 35) | Fuera de implementación autorizada | No verificado: excluidos | Exclusión expresa del TXT | Mantener exclusión; no implementar ni modificar |

## Problemas encontrados y acción mínima

### Compilación

No se reprodujo ningún error C++. Las 29 fuentes `.cpp` figuran en CMake. Crow cacheado corresponde a v1.2.1; el build limpio usa C++17 y Asio. `project(...)` aún contiene P1 y el ejecutable se llama `202405047_ext2_backend`; son identificadores conservados, no fallos de despliegue. No se renombraron por estilo.

El entorno probado es Debian 13 con GCC 14.2, Node 24.19, Crow 1.2.1 y Asio 1.30.2. La configuración desde cero reutilizó la fuente instalada de Crow mediante FETCHCONTENT_SOURCE_DIR_CROW para no depender de descargas. No se verificó una instalación Ubuntu nueva ni una descarga nueva de Crow; la guía exige compilar en EC2 usando libasio-dev y CMake de Ubuntu.

### Despliegue

1. **Nginx incompleto:** el archivo anterior solo tenía `location /api/` y era un fragmento para pegar dentro de un server existente. Copiarlo directamente a sites-available no producía un sitio completo. Se incorporó server/listen/server_name, index/root y location /, conservando el proxy y sus cabeceras/límites/timeout.
2. **URL/origen de desarrollo:** los ejemplos env contienen localhost para desarrollo. No es necesario cambiar código: VITE_API_URL debe definirse al compilar para el backend público y MIA_CORS_ORIGIN al arrancar para S3. Cambiar solo una variable después de subir dist no modifica el JS ya compilado; se debe recompilar y subir el build actualizado.
3. **Permisos/WorkingDirectory:** el servicio existente ya usa /opt/mia y data relativa; la guía crea usuario/directorios y exige comprobar acceso al iniciar. No se tocaron datos existentes ni se realizaron cambios masivos de propietario.
4. **S3 mal usado en arquitectura sugerida:** el PDF lo exige para frontend, no solo respaldos. La guía publica únicamente el build y mantiene .mia local.

### Funcionalidad y especificación

No se reprodujeron fallos en los flujos automatizados. Sí se registran dos diferencias de interpretación: fórmula multiplicativa del journal y ejemplo contradictorio de REMOVE. No se ocultan como cumplimiento literal ni se altera el formato binario funcional sin una interpretación inequívoca. La capacidad 50, fecha float y truncamiento de campos son límites existentes, documentados.

## Archivos modificados en esta solicitud

| Archivo | Problema y cambio concreto | Qué se conserva |
|---|---|---|
| deploy/nginx.conf.example | Líneas 1–14 y cierre crean un sitio completo puerto 80, /opt/mia/frontend/dist e index/fallback; líneas 15–23 conservan proxy /api/ y aclaran que proxy_pass no debe llevar barra final | Crow en loopback:2611, prefijo /api, cabeceras, 1 MiB, 180 segundos; frontend oficial sigue en S3 |
| README.md | Se añaden enlaces de descubrimiento a los dos entregables de esta auditoría | Instalación, comandos y contenido existente |
| AUDITORIA_AWS_PROYECTO2.md, nuevo | Matriz, hallazgos, acciones, pruebas locales y límites de verificación | No agrega funcionalidades |
| PASOS_DESPLIEGUE_AWS.md, nuevo | Instrucciones operativas EC2/S3/Nginx/systemd, permisos y comprobaciones | Una EC2 y un bucket, datos locales y arquitectura actual |

`deploy/mia-backend.service.example` se inspeccionó y mantuvo sin cambios: User/Group mia, WorkingDirectory /opt/mia, EnvironmentFile, ExecStart, Restart=on-failure y UMask ya son adecuados si se crean las rutas/usuario indicados. CMake, Crow, Asio, React, Vite, API, sesiones, formato binario, journal y comandos se mantuvieron. No hay commit, push ni cambio del historial.

## Pruebas ejecutadas de nuevo

Las pruebas se ejecutaron en `/tmp/mia-aws-audit-mqq8f4d9`, con copias de los tests y enlace al ejecutable nuevo. No se sobrescribieron los reportes/resultados anteriores en tests/evidence ni se tocaron discos reales. El snapshot confirma que ningún archivo previo de backend/frontend/tests/data cambió.

| Prueba | Resultado y evidencia |
|---|---|
| Configuración CMake desde directorio vacío | PASS: detecta C++/Asio/Crow y genera build; 29 fuentes declaradas |
| Compilación limpia con --parallel 4 | PASS: enlace del target al 100%, ejecutable nuevo en /tmp |
| Build productivo React con VITE_API_URL explícita | PASS: index.html y assets fuera del repositorio; TypeScript/Vite correctos |
| Oxlint frontend | PASS, sin diagnósticos |
| Regresión P1 contra ejecutable nuevo | PASS: 1,291 verificaciones |
| Integración P2 contra ejecutable nuevo | PASS: 1,711 verificaciones; incluye persistencia, errores, espacio, permisos, datos y reportes |
| Chromium con build estático, sin Vite | PASS: 12 flujos; carga scripts, login/logout, datos, confirmación, journal, SVG, aislamiento y LOSS. Python http.server sirve los assets temporales, Crow sirve API desde otro puerto/origen |
| systemd-analyze verify --man=no | PASS en copia temporal del servicio con WorkingDirectory/EnvironmentFile/ExecStart adaptados a rutas que existen aquí. No se instaló/activó servicio |
| git diff --check | PASS |

La primera invocación de systemd-analyze bajo sandbox falló por SO_PASSCRED/Operation not permitted. Se repitió con permisos de verificación y terminó sin diagnósticos. No fue un fallo del archivo unit.

### Verificaciones estáticas y pendientes

- Nginx: configuración inspeccionada; **nginx -t no ejecutado**, porque Nginx no está instalado en este entorno. No se añadió una dependencia para simular una validación. El comando obligatorio está en la guía para EC2.
- Systemd real: no se verificó systemctl start/restart ni el usuario mia de una EC2; la verificación de unidad no sustituye esa prueba.
- EC2 Ubuntu, Security Groups, IP/red, S3 website/policy, subida dist y enlace público: **no verificados**; no hubo llamadas AWS.
- HTTPS: no se probó dominio/certificado. La guía mínima usa HTTP en sitio S3 y API Nginx, sin servicios AWS adicionales; si se usa un frontend HTTPS el backend también debe ser HTTPS.
- Privacidad/acceso del repositorio y entrega académica: no verificados.

## Conclusión de alcance

El proyecto conserva su implementación funcional local. La adaptación del repositorio y la guía están completas dentro del alcance de cambios mínimos. No se declara cumplimiento total remoto: faltan la ejecución de los pasos en EC2/S3 y las evidencias de entrega; permanecen documentadas las dos interpretaciones del enunciado, sin cambiar layout ni comportamiento probado. Los pasos concretos están en PASOS_DESPLIEGUE_AWS.md.
