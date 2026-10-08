# Interfaz MIA Proyecto 2

React 19 + TypeScript + Vite 8. Conserva el editor y los componentes del Proyecto 1; agrega login, selector de discos/particiones, explorador de solo lectura, visor de texto, journal y reportes.

```bash
npm ci
cp .env.example .env
npm run dev
npm run build
npm run lint
```

`VITE_API_URL` debe contener la URL del backend, sin `/api` al final. Desarrollo: `http://localhost:2611`. Producción: dominio HTTPS de la API. Si no se configura en producción se usan rutas del mismo origen. El build está en `dist/` y se aloja como archivos estáticos.

El token vive en `sessionStorage`; usuario/UID/GID se validan en C++. La contraseña se limpia después de LOGIN. Los mensajes se renderizan como texto. Los reportes se descargan con el token y se muestran mediante Blob en un iframe aislado.

Instrucciones completas, pruebas y configuración Linux: [README del proyecto](../README.md) y [manual completo](../DOCUMENTACION_COMPLETA_PROYECTO2.md).
