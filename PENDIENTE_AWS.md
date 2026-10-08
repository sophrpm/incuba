# Despliegue posterior en AWS

No se han creado, configurado ni utilizado recursos de AWS. La implementación y pruebas locales están documentadas en el README y el manual.

1. Elegir región, instancia EC2 Ubuntu, dominio de API y bucket S3 para el frontend. Configurar acceso y políticas correspondientes desde la cuenta autorizada del propietario.
2. Transferir el repositorio a EC2 e instalar C++17, CMake, Asio, Graphviz y las dependencias Linux indicadas en README. Compilar el backend.
3. Crear el usuario de servicio, sus directorios persistentes de discos, reportes y entradas, y una configuración `.env` con rutas absolutas. Reservar espacio adicional equivalente al disco que se modifica para los respaldos transitorios.
4. Adaptar `deploy/mia-backend.service.example` a las rutas instaladas y activar el servicio systemd. Ejecutar una sola instancia del backend para el mismo conjunto de discos.
5. Configurar HTTPS y proxy inverso hacia `/api/`; adaptar `deploy/nginx.conf.example`. Mantener Crow en loopback detrás del proxy. Ajustar Security Groups a la arquitectura elegida y restringir SSH.
6. Configurar `MIA_CORS_ORIGIN` al origen exacto del frontend. Evitar mezclar frontend HTTPS y backend HTTP.
7. Establecer `VITE_API_URL` al dominio HTTPS real de la API antes de ejecutar `npm ci` y `npm run build`. Subir únicamente el contenido de `frontend/dist/` a S3 con tipos MIME adecuados.
8. Configurar alojamiento estático y HTTPS del frontend según la arquitectura elegida. La aplicación no utiliza rutas de cliente profundas ni requiere Node.js en S3.
9. Definir respaldo de discos `.mia`, reportes y entradas, almacenamiento persistente, permisos de archivos y supervisión del servicio.
10. Verificar desde el sitio publicado `/api/health`, login, scripts, explorador, journal y reportes. Repetir los casos del manual con un directorio de datos destinado a demostración. Documentar URLs y evidencia de despliegue para la entrega académica.
