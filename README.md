# MIA Proyecto 2 — EXT2 / EXT3

Aplicación académica con backend **C++17 + Crow**, frontend **React + TypeScript**, discos binarios `.mia`, scripts `.smia` y reportes **Graphviz**. Conserva el Proyecto 1 y agrega EXT3, journal de 50 operaciones, LOSS, FDISK ADD/DELETE FAST/FULL, UNMOUNT, REMOVE, RENAME, COPY, MOVE y FIND. El explorador es de solo lectura. El alcance autorizado excluye CHMOD/CHOWN y cualquier operación en AWS.

## Requisitos e instalación en Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git libasio-dev graphviz python3
```

Use Node.js 22.12 o posterior compatible con Vite 8 (Node 24 recomendado). CMake descarga Crow v1.2.1 durante la primera configuración; necesita acceso a GitHub. No se necesita base de datos ni servidor Node para el frontend de producción.

## Compilar

Desde la raíz del repositorio:

```bash
cmake -S backend -B backend/build -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
cmake --build backend/build --parallel 4
npm --prefix frontend ci
npm --prefix frontend run build
npm --prefix frontend run lint
```

En el entorno de esta tarea CMake y Asio están instalados en rutas auxiliares:

```bash
/workspace/.setup-python/cmake/data/bin/cmake -S backend -B backend/build -DASIO_INCLUDE_DIR=/workspace/.setup-asio/asio/include -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
/workspace/.setup-python/cmake/data/bin/cmake --build backend/build --parallel 4
```

Esas rutas son del entorno de verificación; en Ubuntu con los paquetes anteriores se usa `cmake` normalmente.

## Ejecutar

Terminal 1, desde la raíz:

```bash
cp .env.example .env
set -a
source .env
set +a
./backend/build/202405047_ext2_backend
```

Terminal 2, desde la raíz:

```bash
cp frontend/.env.example frontend/.env
npm --prefix frontend run dev -- --host 127.0.0.1
```

Abra `http://localhost:5173`. El backend escucha en `127.0.0.1:2611`; la salud está en `/api/health`. Los ejemplos `.env` se cargan en el shell para C++; Vite carga su propio `.env`. Variables: `MIA_HOST`, `MIA_PORT`, `MIA_DISK_DIR`, `MIA_REPORT_DIR`, `MIA_INPUT_DIR`, `MIA_CORS_ORIGIN` y `VITE_API_URL`.

Las rutas físicas relativas de MKDISK/FDISK/MOUNT/RMDISK se resuelven dentro de `MIA_DISK_DIR`. REP guarda en `MIA_REPORT_DIR`; EXEC y `MKFILE -cont` leen dentro de `MIA_INPUT_DIR`. Las rutas absolutas físicas deben permanecer dentro del directorio correspondiente. Las rutas de archivos virtuales empiezan en `/` dentro de la partición de la sesión.

## Demostración

Cargue `tests/08_proyecto2.smia` en React y pulse **Analizar Comandos**. Con datos nuevos el ID es `471A`. El script deja una sesión root lista para explorar carpetas, archivos, reportes y journal. Seleccione el disco para ver sus particiones. Modifique archivos solo mediante comandos.

`tests/09_loss.smia` permite comparar bitmaps y observar que el journal persiste después de LOSS. Para volver a operar archivos, formatee de nuevo. DELETE y sobrescritura requieren contestar `y/yes` o `n/no` en el formulario web; el resto del script continúa. Los comandos y parámetros se comparan sin distinguir mayúsculas; nombres virtuales, usuarios y contraseñas sí las distinguen.

## Pruebas

```bash
python3 tests/audit.py
python3 tests/project2.py
```

Ambas arrancan/detienen un backend propio y crean discos temporales. La prueba visual requiere Playwright y Chromium instalados:

```bash
node tests/browser.mjs
```

`MIA_PLAYWRIGHT_MODULE` permite indicar un módulo Playwright existente; `MIA_CHROMIUM_PATH` permite indicar Chromium. En esta tarea:

```bash
MIA_PLAYWRIGHT_MODULE=/opt/codex/cua_node/lib/node_modules/playwright/index.mjs node tests/browser.mjs
```

Resultados y casos: [PRUEBAS_PROYECTO2.md](PRUEBAS_PROYECTO2.md). Evidencias: `tests/RESULTADOS*.json` y `tests/evidence/frontend*.png`.

## Documentación y despliegue posterior

- [DOCUMENTACION_COMPLETA_PROYECTO2.md](DOCUMENTACION_COMPLETA_PROYECTO2.md): manual de uso, estructuras, comandos, API, cada archivo propio, variables y guía de defensa.
- [CAMBIOS_PROYECTO2.md](CAMBIOS_PROYECTO2.md): decisiones y archivos modificados/creados.
- [PENDIENTE_AWS.md](PENDIENTE_AWS.md): únicamente tareas de despliegue futuro; no se ha utilizado AWS.
- `deploy/`: ejemplos de servicio Linux y proxy inverso.

Límites del simulador: ABI nativo Linux de 64 bits, bloques de 64 bytes, nombres virtuales de 12 bytes, archivo máximo de 280320 bytes, journal de 50 operaciones con campos fijos del PDF. No hay recuperación automática después de LOSS. Solo se montan primarias, conforme al Proyecto 1. COPY/MOVE trabajan en la partición de la sesión. Consulte las decisiones de interpretación en el manual.

## Auditoría y pasos siguientes para AWS

La revisión del 13 de octubre de 2026 conserva la implementación y ajusta únicamente el ejemplo Nginx. Consulte [AUDITORIA_AWS_PROYECTO2.md](AUDITORIA_AWS_PROYECTO2.md) para la matriz por páginas y resultados locales, y [PASOS_DESPLIEGUE_AWS.md](PASOS_DESPLIEGUE_AWS.md) para desplegar una EC2 y un bucket S3 según el PDF. No se ejecutaron acciones AWS.
