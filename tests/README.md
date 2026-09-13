# Pruebas del proyecto

`python3 tests/audit.py` inicia y detiene su propio backend en el puerto 2612, crea discos en una carpeta temporal nueva y comprueba la API real y las estructuras con un lector binario independiente. Requiere Python 3, Graphviz y el backend compilado. No instala paquetes. `MIA_TEST_PORT=2613 python3 tests/audit.py` permite cambiar el puerto.

La prueba falla con excepción ante cualquier resultado inesperado; `RESULTADOS.json` registra el estado, número de verificaciones, ABI y carpeta temporal. `evidence/` conserva los diez reportes y sus fuentes DOT principales. La cantidad de verificaciones incluye comprobaciones por bloque, no representa esa cantidad de casos independientes. La lectura con ctypes usa el ABI nativo de Linux de 64 bits; en otra plataforma hay que comprobar primero los tamaños.

## Demostración desde el frontend

1. Ejecute `python3 tests/prepare_scripts.py --base /tmp/mi-demo-nueva`.
2. Inicie un backend nuevo para que el primer ID sea 471A.
3. Cargue `tests/generated/demo_final.smia`, o los archivos numerados en orden.
4. Al aparecer la pregunta de sobrescritura responda `y`, `yes`, `n` o `no`. El script restante continúa automáticamente.
5. Abra los enlaces de reportes. Los errores de permisos del archivo 05 y los errores del archivo 07 son intencionales.

Los scripts originales usan `/tmp/mia-p1-demo`. El preparador adapta las rutas y crea el archivo externo para `-cont`. Nunca borra discos; para repetir desde cero use otro `--base`. Los scripts son secuenciales; `demo_final.smia` ya los reúne y no debe ejecutarse después de los numerados sobre el mismo disco.

## Qué comprueba la auditoría automática

- Creación física en ceros, tamaños, límites, P/E/L, FF/BF/WF, huecos y cadena EBR.
- Eliminación y crecimiento/reducción ya existentes, sin crecimiento del archivo contenedor.
- Montaje exclusivo de primarias, rutas equivalentes, IDs, duplicados y limpieza al eliminar disco.
- Fórmula de EXT2, n inodos/3n bloques, raíz, users.txt, direcciones de primeros libres.
- Roles U/G/O/root, lectura y escritura, fechas, IDs monotónicos, usuarios y grupos sensibles a mayúsculas.
- Flags, parámetros repetidos/desconocidos, valores faltantes, negativos y enteros fuera de rango.
- Confirmación que pausa/reanuda, respuesta inválida, cancelación, rechazo por permisos y falta de espacio.
- Directos y tres indirectos: 768, 769, 1793, 20000 y 280320 bytes. CAT se compara carácter por carácter.
- Referencias de bloques válidas, únicas y ocupadas; conteos de bitmaps; ausencia de bloques huérfanos tras cambios.
- Diez reportes, bitmaps completos a 20 valores por línea, FILE completo y descarga por API.
- Comentarios, líneas vacías, errores léxicos/sintácticos separados y contadores exactos.

La fixture de permisos escribe `000` en un inodo exclusivamente de prueba para demostrar que root lo ignora y el propietario sin lectura recibe error. No agrega CHMOD al proyecto 1.
