# Documentación completa — MIA Proyecto 2

Proyecto: **MIA Proyecto 2, segundo semestre 2026, 202405047**. Backend C++17; frontend React/TypeScript; discos binarios `.mia`; scripts `.smia`.

Manual de usuario y manual técnico basado en los archivos finales de `/workspace/incuba`. Incluye el funcionamiento conservado del Proyecto 1 y las ampliaciones del Proyecto 2. Los documentos de referencia son los dos PDF adjuntos y el TXT proporcionado; el TXT fija las exclusiones de CHMOD/CHOWN y de uso de AWS. No se ejecutó un despliegue.

El backend es C++17 con Crow; React utiliza el mismo analizador para editor y scripts. EXT2/EXT3 son simulaciones sobre archivos binarios, no formatos que puedan montarse con el kernel Linux. El código conserva managers y estructuras del Proyecto 1.

## Alcance y decisiones comprobadas

- El PDF del Proyecto 2 muestra tablas como imágenes; se verificó visualmente la página física 13: `Journal.j_count` entero, `j_content` Information; Information contiene `i_operation[10]`, `i_path[32]`, `i_content[64]` y `i_date` float.
- Se interpreta la constante 50 como 50 registros Information dentro de un Journal por partición. El layout reserva `sizeof(Journal)` una vez, antes de los bitmaps. El PDF presenta además una ecuación con `n * sizeof(Journaling)`; su observación de capacidad fija y sus tablas no especifican inequívocamente esa multiplicidad. Esta implementación usa la capacidad fija solicitada, sin reservar n bitácoras de 50 registros. Esta decisión está explícita y se verifica con el lector binario independiente.
- El texto normativo de REMOVE exige no eliminar nada cuando un descendiente impide la eliminación; el ejemplo posterior sugiere eliminación parcial. Se aplica la regla normativa: validación completa y operación sin cambios si hay permisos insuficientes.
- FIND interpreta `?` como un carácter y `*` como uno o más, exactamente como la tabla del PDF; no aplica el glob habitual de cero o más. Muestra rutas completas, que permiten identificar sus carpetas padres.
- COPY omite únicamente ramas sin lectura. Los elementos nuevos pertenecen al usuario/grupo de la sesión y reciben los permisos predeterminados conservados de creación. No hay obligación explícita de preservar UID/GID originales.
- MOVE utiliza la partición activa; los parámetros son rutas internas sin selector de segunda partición. El PDF explica el movimiento en la misma partición y no define un protocolo entre particiones. No se inventa ese protocolo.
- Se montan únicamente primarias, como exige el Proyecto 1. Extendidas/lógicas se administran y muestran en el selector, pero no se formatean ni se usan para LOGIN.
- FDISK toma la primera operación `size`, `add` o `delete` que aparezca; ignora opciones exclusivas de las otras operaciones. DELETE se confirma desde web, nunca con `cin`.
- ADD no mueve datos ni recalcula inodos. Ampliar deja el espacio adicional fuera del layout formateado; reducir no puede cortar ninguna estructura reservada. Por eso un sistema previamente formateado sigue siendo legible después de un crecimiento.
- Los campos Journal tienen tamaños fijos. Ruta y contenido se limitan a 32/64 bytes; la fecha Unix en float pierde precisión a nivel de segundos (en fechas actuales, aproximadamente 128 segundos de resolución). Es una consecuencia del tipo float del PDF, no un reloj de alta precisión. La GUI muestra fecha y hora en la zona del navegador. Una bitácora llena rechaza nuevas modificaciones antes de escribir.
- Una restauración por error de operación devuelve el disco al respaldo. No es recuperación de cortes de energía ni recuperación automática de LOSS. LOSS conserva superbloque/journal, borra las cuatro áreas de datos e invalida sesiones; para volver a operar se usa MKFS.

## Manual de usuario: inicio y demostración

1. Instale dependencias y compile según README. Ejecute Crow y Vite en dos terminales. Configure primero los directorios base.
2. Abra la interfaz, cargue `tests/08_proyecto2.smia` y ejecute. Con datos nuevos crea `proyecto2.mia`, primaria de 8 MiB, montaje `471A`, EXT3, grupo/usuario, carpetas, archivos y reportes.
3. Seleccione el disco. La tabla muestra tamaño, fit, tipo, estado, formato e ID. El formulario requiere ID, usuario y contraseña; el script ya deja login root iniciado, por lo que muestra Cerrar sesión.
4. En Archivos entre a `home`, luego `docs`, abra `b.txt`; use Subir para volver. La tabla muestra nombre, tipo, tamaño, UID, GID y permisos. El contenido se renderiza como texto UTF-8; un archivo binario recibe un error de compatibilidad.
5. En Journaling seleccione una primaria EXT3 montada. Se muestran operación, ruta, contenido y fecha/hora. EXT2 presenta un mensaje de ausencia de journaling.
6. En Reportes abra un archivo generado con REP. Se descarga con el token del navegador y se representa en un iframe aislado. Los bitmaps se sirven como texto.
7. Las modificaciones se escriben en el editor; no hay botones que cambien archivos. La carga de scripts `.smia` o `.txt` conserva comentarios y saltos de línea. Un error por línea mantiene la salida y permite seguir.
8. Una sobrescritura o DELETE muestra un formulario de confirmación. Responda `y`/`yes` para aceptar o `n`/`no` para cancelar. La respuesta inválida conserva la pregunta. El script sigue desde donde se pausó.
9. Para probar LOSS use `tests/09_loss.smia`. Compare los bitmaps antes/después y consulte el mismo journal. El logout es distinto de UNMOUNT; UNMOUNT elimina el registro de montaje y reinicia correlativo a 0.

## Flujo general

```text
usuario escribe o carga .smia
        |
        v
App -> commandAPI -> POST /api/analyze
                         |
                         v
                    processInput
                         |
              CommandToken (tokens)
                         |
              CommandParser (gramática)
                         |
                  ParsedCommand
                         |
                  executeCommand
                         |
                clase del comando
                         |
           manager de disco / EXT2 / usuarios
                         |
                BinaryUtils -> archivo .mia
                         |
           ValidationResult -> JSON -> OutputPanel
```

El editor contiene texto; no contiene el sistema de archivos. Un comando se valida primero y después se ejecuta. Los managers vuelven a abrir el disco para leer sus estructuras. No existe un árbol permanente de carpetas en RAM. Los vectores que reúnen entradas, posiciones, líneas de users.txt o nodos visitados son temporales para la operación en curso. La sesión, los montajes, las rutas de reportes y las confirmaciones de cada navegador sí sobreviven entre peticiones mientras viva el servidor.

## Cómo estudiar los archivos

Los `.hpp` declaran los contratos que pueden llamar otros módulos; los `.cpp` contienen los cuerpos. Los include guards evitan definiciones duplicadas. Los métodos `const` no modifican el objeto manager; **sí pueden escribir en el disco o en referencias recibidas**. Los managers no guardan el estado de EXT2 como atributos. Por eso `FileManager fileManager;` no carga un disco completo.

Una función que devuelve `bool` indica si completó la operación. Cuando tiene `string& message`, devuelve allí la explicación para el usuario; los datos de salida como `Inode& inode`, `int& inodeIndex` o `string& content` también se pasan por referencia. No se deben utilizar esos resultados si devuelve `false`. Las funciones de búsqueda con resultado `int` usan normalmente `-1` como ausencia o error. `ValidationResult` agrega el resultado y el mensaje para enviarlos a la API.

## Vocabulario de parámetros

| Parámetro | Significado concreto |
|---|---|
| `path`, `diskPath`, `inputPath` | Ruta del archivo físico de la computadora en managers de disco y estructuras; ruta interna que empieza con `/` en las operaciones públicas de FileManager. |
| `outputPath` | Archivo físico que recibirá un reporte. |
| `internalPath`, `pathFileLs` | Ruta dentro de EXT2 que REP FILE/LS debe consultar. |
| `partitionStart` | Byte absoluto del comienzo de una partición primaria en el `.mia`. |
| `partitionSize`, `sizeBytes` | Cantidad de bytes reservados. No se confunde con la posición inicial. |
| `superBlock` | Copia temporal del encabezado EXT2; las funciones de asignación la actualizan por referencia y la persisten. |
| `inodeIndex`, `newInodeIndex`, `directoryInodeIndex` | Índice dentro de la tabla de inodos; no es una dirección en bytes. |
| `blockIndex`, `pointerBlockIndex` | Índice dentro del área común de bloques. |
| `logicalBlock` | Número ordinal del bloque de contenido del archivo, entre 0 y 4379. |
| `position`, `entryIndex` | Posición local dentro de un bloque de apuntadores o carpeta; en BinaryUtils, posición absoluta del archivo. |
| `uid`, `gid` | Identificadores numéricos de propietario y grupo. |
| `id`, `partitionId` | Identificador de montaje, por ejemplo 471A. |
| `recursive` | Autoriza crear padres faltantes mediante `-r` o `-p`. |
| `overwrite` | Autoriza reemplazar el archivo después de recibir una respuesta afirmativa. |
| `content` | Contenido transitorio que se escribirá o que se acaba de leer. No es una caché permanente. |
| `name`, `user`, `password`, `group` | Valores de los parámetros; usuarios, contraseñas y grupos conservan sus mayúsculas. |
| `fit`, `type` | Caracteres internos B/F/W y P/E/L, obtenidos de los valores enumerados del comando. |
| `addBytes` | Variación con signo: positivo crece, negativo reduce. |
| `appState` | Montajes y sesión de la aplicación, además de la interacción pendiente. |
| `message` | Mensaje de salida; explica errores o finalización. |

# Estructuras físicas

## backend/include/structures/DiskStructures.hpp

Existe para que disco, montaje y reportes compartan exactamente el mismo formato. Contiene `Partition`, `MBR` y `EBR`; no tiene funciones de negocio. DiskManager escribe estas estructuras y MountManager actualiza los campos de montaje. ReportManager las lee para MBR y DISK.

| Estructura / atributo | Qué representa y cuándo cambia |
|---|---|
| MBR.mbr_tamano | Tamaño total e invariable del archivo contenedor, en bytes; lo establece MKDISK. |
| MBR.mbr_fecha_creacion | Fecha de creación del disco con `time_t`; no se vuelve a crear al particionar. |
| MBR.mbr_dsk_signature | Firma numérica pseudoaleatoria generada al crear el disco. |
| MBR.dsk_fit | Ajuste del disco para seleccionar huecos de primarias/extendidas. |
| MBR.mbr_partitions[4] | Cuatro descriptores embebidos; una primaria o extendida ocupa una entrada. |
| Partition.part_status | '0' antes del montaje y '1' al montar. |
| Partition.part_type | 'P' o 'E' para una entrada usada. |
| Partition.part_fit | Ajuste guardado para la partición. |
| Partition.part_start | Byte absoluto del inicio de la región reservada. |
| Partition.part_s | Tamaño de la región; FDISK add lo cambia. |
| Partition.part_name[16] | Nombre de longitud fija, copiado sin escribir fuera de los 16 bytes. |
| Partition.part_correlative | -1 inicialmente; ordinal de montaje dentro del disco. |
| Partition.part_id[4] | Los cuatro caracteres del ID; no necesita terminador nulo. |
| EBR.part_mount | Estado de la lógica; permanece '0' porque esta simulación monta solo primarias. |
| EBR.part_fit | Ajuste usado para ubicar una lógica. |
| EBR.part_start | Inicio del contenido lógico, inmediatamente después de su EBR. |
| EBR.part_s | Tamaño del contenido lógico, sin sumar nuevamente el EBR. |
| EBR.part_next | Byte del siguiente EBR, o -1 al final. |
| EBR.part_name[16] | Nombre de la lógica. |

El MBR está en el byte 0. No se escribe otro objeto Partition al comienzo de una primaria: su descriptor ya forma parte del MBR. Al crear una extendida se escribe un EBR inicial vacío en `part_start`. Cada lógica ocupa `sizeof(EBR) + part_s`. La lista de EBR debe avanzar físicamente sin salirse de la extendida.

Con el ABI comprobado de Linux de 64 bits: Partition ocupa 36 bytes; MBR 168; EBR 32. Estos tamaños incluyen padding del compilador. El PDF usa `sizeof`, por lo que no se añadió empaquetado ni atributos auxiliares. Un archivo creado con un ABI distinto puede no ser compatible.

**Defensa:** ¿Por qué las lógicas no consumen otra entrada del MBR? Porque se describen mediante EBR dentro de la única extendida. ¿Por qué `part_id` no se lee como `char*` terminado en cero? Porque puede llenar sus cuatro posiciones.

## backend/include/structures/Ext2Structures.hpp

Declara `SuperBlock`, `Inode`, `Content`, `FolderBlock`, `FileBlock` y `PointerBlock`. Todos los managers de EXT2 y los reportes comparten este contrato. Solo Inode y PointerBlock tienen constructores: sus bucles inicializan los apuntadores con -1. Los constructores no escriben archivos ni agregan campos binarios.

| Atributo de SuperBlock | Interpretación |
|---|---|
| s_filesystem_type | 2 para EXT2. |
| s_inodes_count | Total reservado de inodos. |
| s_blocks_count | Triple del total de inodos. |
| s_free_blocks_count | Bloques cuyo bitmap está en '0'. Incluye el costo de los bloques apuntadores. |
| s_free_inodes_count | Inodos cuyo bitmap está en '0'. |
| s_mtime | Fecha del último montaje; MKFS inicia el valor y un montaje posterior lo actualiza. |
| s_umtime | Fecha de desmontaje; queda en 0 al no existir UNMOUNT en este proyecto. LOGOUT no desmonta. |
| s_mnt_count | Cantidad de montajes registrados del sistema formateado. |
| s_magic | Firma 0xEF53 para identificar EXT2. |
| s_inode_s | sizeof(Inode), 104 en el equipo auditado. |
| s_block_s | 64 bytes. |
| s_firts_ino | Dirección absoluta del primer inodo libre; -1 si no queda ninguno. Se conserva la errata del nombre del PDF. |
| s_first_blo | Dirección absoluta del primer bloque libre; -1 si no queda ninguno. |
| s_bm_inode_start | Primer byte del bitmap de inodos. |
| s_bm_block_start | Primer byte del bitmap de bloques. |
| s_inode_start | Primer byte de la tabla de inodos. |
| s_block_start | Primer byte del área de bloques. |

SuperBlock ocupa 80 bytes en este ABI. Se escribe al inicio de la primaria formateada y se actualiza cuando se asignan o liberan estructuras.

| Atributo de Inode | Interpretación |
|---|---|
| i_uid / i_gid | Propietario y grupo que creó el objeto. |
| i_s | Longitud del archivo en bytes; en carpetas, bytes de bloques carpeta asignados. |
| i_atime | Fecha de lectura; CAT/readFile la actualiza. |
| i_ctime | Fecha de creación, conservada al reemplazar contenido. |
| i_mtime | Última modificación de contenido o entradas. |
| i_block[15] | Doce índices directos, uno simple, uno doble y uno triple. |
| i_type | '0' carpeta, '1' archivo. |
| i_perm[3] | Tres caracteres octales, por ejemplo '6','6','4'. No es un entero decimal. |

Content contiene `b_name[12]` y `b_inodo`. Una carpeta tiene cuatro Content, por tanto 4 × (12 + 4) = 64 bytes. El primer bloque reserva `.` para sí mismo y `..` para el padre. Un FileBlock tiene `b_content[64]`. Un PointerBlock tiene `b_pointers[16]`, dieciséis enteros de cuatro bytes. Los tres tipos comparten el área de bloques y su bitmap; el tipo se deduce del inodo y del nivel del recorrido.

**Defensa:** ¿Por qué el bitmap de bloques no guarda tipos 2 y 3? Porque el enunciado especifica libre/ocupado, y el recorrido desde el inodo indica cómo interpretar esos 64 bytes.

# Cómo funciona EXT2

## Cómo se calcula el número de inodos

```text
n = floor((tamaño_partición - sizeof(SuperBlock)) /
          (1 + 3 + sizeof(Inode) + 3 * 64))
```

Los cuatro bytes por estructura son un byte para el bitmap de inodos y tres para el bitmap de bloques. Para cada inodo se reserva también el propio inodo y tres bloques de 64 bytes. En este equipo el denominador es 300. La división entera de cantidades positivas ya aplica floor. Se rechaza n < 2 porque hacen falta raíz y users.txt.

## Layout físico y direcciones

```text
inicio primaria
  [SuperBlock]
  [bitmap inodos: n bytes]
  [bitmap bloques: 3n bytes]
  [tabla: n * sizeof(Inode)]
  [bloques: 3n * 64]
  [sobrante por redondeo]
```

Para un índice de inodo k: `s_inode_start + k * s_inode_s`. Para un índice de bloque b: `s_block_start + b * 64`. El índice 0 es perfectamente válido; el centinela de ausencia es -1. Los campos de primeros libres guardan la dirección calculada, no el índice encontrado al recorrer el bitmap.

## Cómo funcionan los bitmaps

En el `.mia` se usan los caracteres '0' y '1', un byte por registro. Crear un archivo consume un inodo y los bloques de contenido/apuntadores necesarios; añadir una entrada puede ampliar la carpeta padre. Liberar restaura '0', limpia la estructura y aumenta el contador libre. `updateFirstFree` busca la primera posición libre y calcula su dirección. Los reportes de bitmap muestran todos los registros, veinte por línea.

Las pruebas vuelven a leer ambos bitmaps independientemente, cuentan los libres, comparan los campos del superbloque y recorren todos los apuntadores ocupados. Comprueban que ningún bloque esté repetido y que todo bloque marcado como usado tenga una referencia.

# Cómo funcionan los bloques indirectos

```text
0..11:     inodo -> datos
12:        inodo -> apuntadores -> datos
13:        inodo -> apuntadores -> apuntadores -> datos
14:        inodo -> apuntadores -> apuntadores -> apuntadores -> datos
```

Cada bloque apuntador almacena dieciséis índices. El simple direcciona 16 bloques; el doble 16² = 256; el triple 16³ = 4096. En total hay 12 + 16 + 256 + 4096 = 4380 bloques de contenido, equivalentes a **280320 bytes**. Los bloques apuntadores también ocupan espacio físico y se descuentan del bitmap, pero no incrementan el tamaño del archivo.

| Contenido | Último mecanismo necesario |
|---|---|
| 0..768 bytes | Directos; un archivo vacío no necesita FileBlock. |
| 769..1792 | Simple. |
| 1793..18176 | Doble. |
| 18177..280320 | Triple. |

En el doble se resta 28 al número lógico y se divide entre 16: cociente para el primer nivel, residuo para el segundo. En el triple se resta 284; dividir entre 256 selecciona el primer nivel, y el residuo se divide entre 16 para obtener segundo y tercero. Las asignaciones crean los bloques apuntadores solo cuando se necesitan y dejan sus entradas libres en -1.

La escritura comprueba el límite y el espacio total, incluyendo apuntadores, antes de liberar el contenido anterior. La liberación recorre de las hojas hacia arriba: primero datos, después apuntadores inferiores y finalmente la raíz indirecta. La lectura usa `i_s` para recuperar exactamente los bytes válidos; no usa strlen, por lo que un cero dentro del contenido no termina el archivo.

# Cómo funcionan los permisos

Primero se comprueba uid = 1: root siempre pasa. Para los demás, si uid coincide con i_uid se usa U; si no coincide pero gid coincide con i_gid se usa G; en otro caso se usa O. No se suman las categorías: un propietario sin lectura no toma prestados los permisos del grupo.

Se convierte el carácter octal a número y se aplica una máscara: lectura `valor & 4`, escritura `valor & 2`. MKDIR y MKFILE revisan escritura en la carpeta padre. Al sobrescribir también se exige escritura en el archivo. CAT exige lectura en el archivo. Los objetos nuevos tienen 664, dueño/grupo de la sesión y fechas actuales. La raíz se inicia con 777 para permitir crear objetos a los usuarios.

# Cómo funciona users.txt

Es un archivo normal de EXT2. MKFS crea `1,G,root` y `1,U,root,root,123`, cada registro seguido de salto de línea. LOGIN lee el archivo con un estado temporal suficiente para autenticar antes de tener sesión. Compara nombre y contraseña exactamente; después obtiene el GID de un grupo activo.

MKGRP y MKUSR añaden registros. RMGRP y RMUSR cambian el primer campo a 0, conservando la línea. CHGRP cambia el campo del grupo del usuario. Solo root realiza estas operaciones. Los nombres y contraseñas se limitan a diez caracteres y se rechazan comas/saltos de línea que romperían el formato. El siguiente ID tiene en cuenta el número histórico de registros y el mayor ID activo; por eso borrar el último usuario no provoca que su UID vuelva a utilizarse. Los cambios de grupo se reflejan en una sesión al volver a iniciar sesión.

# Cómo funcionan MBR, Partition y EBR

DiskManager calcula huecos ordenando descriptores por `part_start`. FF toma el primer hueco suficiente; BF el más pequeño suficiente; WF el mayor. La implementación conservada utiliza `dsk_fit` para ubicar primarias/extendidas y el fit solicitado al crear una lógica para elegir su hueco. El valor de fit de cada partición también queda en su descriptor.

Las lógicas se insertan en una cadena ascendente de EBR. Al borrar la primera se conserva un EBR vacío que sigue apuntando al siguiente. Al borrar una intermedia se cambia el enlace del anterior y FULL limpia su región; FAST conserva los bytes sin referencias. Aumentar exige espacio contiguo hasta el siguiente objeto; reducir exige tamaño positivo y, para una extendida, no cortar EBR ni lógicas. El contenedor no cambia de longitud.

# Cómo funciona la API

Primero `GET /api/session` entrega un token aleatorio. El frontend conserva el token en sessionStorage y envía `X-MIA-Session` en las siguientes peticiones. `browsers` guarda Session, PendingConfirmation y los índices de reportes por token. Bajo `appMutex`, useBrowser carga el estado de ese navegador en AppState; saveBrowser lo devuelve a su registro y limpia la sesión temporal. Los montajes son compartidos por el simulador, las identidades no.

LOGIN verifica `users.txt` mediante UserManager; el navegador no envía UID/GID elegidos. LOGIN por formulario recibe password, el comando de editor usa pass. Ambos llegan a la misma función. No se registra el cuerpo de solicitudes ni el password en el journal. Logout limpia sesión y confirmación. UNMOUNT, MKFS, LOSS y RMDISK invalidan las sesiones dependientes en todos los navegadores.

Un mutex global serializa comandos, reportes y lecturas: solución simple para una sola instancia. Las operaciones que modifican un disco guardan primero una copia en `.transactions`, ejecutan la acción y escriben journal si corresponde. Si fallan se restaura la copia. Los backups transitorios requieren espacio equivalente al disco. No ejecute dos procesos Crow modificando los mismos datos: el mutex es del proceso.

Crow responde 400 a errores de comandos o JSON inválido; 401 a token/login inválido o falta de autenticación; 403 a lectura no permitida; 404 a partición/reporte ausente; 413 a scripts demasiado grandes; 415 a archivos binarios/no UTF-8; 500 si no puede enumerar discos. El POST conserva messages y stats incluso con 400, para que React no pierda salida por comando.

```json
{"success":true,"messages":["Archivo /a creado correctamente."],"stats":{"successfulCommands":1,"lexicalErrors":0,"syntaxErrors":0},"pendingConfirmation":false,"reports":[]}
```

El servidor conserva discos/bitácoras/reportes físicamente. Al reiniciar reconstruye montajes desde los campos MBR persistentes. Tokens, login, continuaciones e índices de reportes de navegador viven en RAM y se reinician. Debe iniciar sesión otra vez; los archivos y journal siguen en disco.

# Flujo completo de un script .smia

App lee el archivo seleccionado con `File.text()` y conserva los saltos de línea en el editor. Al ejecutar, processInput recorre las líneas en orden; muestra intactos los comentarios completos y los espacios de las líneas vacías. Para cada línea de comando ejecuta lexer, parser y clase Command. Un error descarta esa línea, pero el bucle sigue con la siguiente. Una sobrescritura o eliminación es una pausa de interacción: guarda el texto restante y vuelve al navegador sin leer stdin ni dejar esperando un hilo del servidor.

# Errores léxicos vs sintácticos vs errores de ejecución

- Léxico: no se puede formar un token, por ejemplo una cadena sin cerrar o un símbolo prohibido sin comillas. Nace en CommandToken.
- Sintáctico: hay tokens pero su orden no representa un comando, por ejemplo `=incorrecto`, falta un valor después de `=`, o no hay separación entre parámetros. Nace en CommandParser.
- Ejecución/validación: estructura reconocida pero opción inválida, parámetro desconocido/repetido, tamaño negativo, partición inexistente o permiso denegado. Nace en Command o Manager.

Si el lexer falla no se vuelve a contar la misma línea como error de parser. Los comandos desconocidos se informan como sintácticos porque no pertenecen al vocabulario admitido. Los valores se conservan; solo el nombre del comando y de sus parámetros se normalizan. Las opciones enumeradas como FF/full se validan sin distinguir mayúsculas.

# Contadores de ejecución

`successfulCommands` aumenta únicamente después de un ValidationResult exitoso. Comentarios, líneas vacías, preguntas pendientes, cancelaciones y comandos fallidos no lo aumentan. `lexicalErrors` suma los diagnósticos del lexer y `syntaxErrors` los del parser. Un script puede tener dos comandos correctos, un léxico, un sintáctico y cinco errores de negocio: su resumen sería 2, 1 y 1. OutputPanel presenta el resumen al finalizar; mientras hay confirmación se oculta para no aparentar que el script terminó.

## backend/include/managers/DiskManager.hpp

Declara la clase `DiskManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/DiskManager.hpp`, `utils/BinaryUtils.hpp`, `utils/PathUtils.hpp`, `utils/StringUtils.hpp`. La implementación está en `backend/src/managers/DiskManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/DiskManager.cpp

Implementa `DiskManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### createDisk()

**Recibe:** `const string& path, int sizeBytes, char fit, string& message`.

Valida tamaño, ajuste, extensión y ausencia del archivo. Crea padres, escribe ceros en tandas de 1024 bytes y finalmente guarda el MBR en el byte 0. Si falla la creación intenta retirar el archivo incompleto.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MkDiskCommand.cpp`.

### removeDisk()

**Recibe:** `const string& path, string& message`.

Valida que exista un archivo .mia y lo elimina del sistema anfitrión. El despachador limpia después los montajes y la sesión que dependían de ese disco.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `RmDiskCommand.cpp`.

### createPartition()

**Recibe:** `const string& path, int sizeBytes, char type, char fit, const string& name, string& message`.

Valida tipo/tamaño/nombre/ajuste, lee el MBR y comprueba nombres en todo el disco. Deriva a createLogical si es L o a createPrimaryOrExtended para P/E.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FDiskCommand.cpp`.

### deletePartition()

**Recibe:** `const string& path, const string& name, string& message, bool full`.

Busca primero entre las cuatro entradas MBR y después en la cadena lógica. Invoca la eliminación apropiada; una ausencia es error.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FDiskCommand.cpp`.

### resizePartition()

**Recibe:** `const string& path, const string& name, int addBytes, string& message`.

Comprueba delta distinto de cero y existencia del disco. Localiza P/E o L y deriva al algoritmo correspondiente sin cambiar el tamaño del contenedor.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FDiskCommand.cpp`.

### readMBR()

**Recibe:** `const string& path, MBR& mbr`.

Lee desde el byte 0. Compara mbr_tamano con el tamaño físico y rechaza particiones fuera de rango o superpuestas. Entrega el MBR por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp`, `FileSystemManager.cpp`, `MountManager.cpp` y métodos de esta clase.

### writeMBR()

**Recibe:** `const string& path, const MBR& mbr`.

Escribe el descriptor completo al comienzo del archivo ya existente mediante BinaryUtils; no abre el disco en modo truncado.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MountManager.cpp` y métodos de esta clase.

### findPartition()

**Recibe:** `const MBR& mbr, const string& name`.

Recorre las cuatro entradas usadas y compara el nombre. Conserva la comparación sin distinguir mayúsculas de la implementación previa de particiones. Devuelve su posición o -1.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** `MountManager.cpp` y métodos de esta clase.

### findLogicalPartition()

**Recibe:** `const string& path, const string& name, EBR& ebr, int& ebrPosition`.

Encuentra la extendida y sigue part_next en orden ascendente. Entrega tanto el EBR como la posición física donde fue leído; devuelve false si no hay coincidencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### diskExists()

**Recibe:** `const string& path`.

Consulta BinaryUtils::fileExists; no interpreta el contenido como EXT2.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### validExtension()

**Recibe:** `const string& path`.

Comprueba .mia sin distinguir mayúsculas en la extensión.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### partitionNameExists()

**Recibe:** `const string& path, const MBR& mbr, const string& name`.

Consulta primero findPartition y después findLogicalPartition para impedir repetición entre todos los tipos.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### findExtendedPartition()

**Recibe:** `const MBR& mbr`.

Busca la entrada usada cuyo part_type es E; devuelve índice o -1.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### findFreePartitionSlot()

**Recibe:** `const MBR& mbr`.

Devuelve la primera entrada con part_s no positivo; no significa que haya un hueco físico suficiente.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### getDiskFreeSpaces()

**Recibe:** `const MBR& mbr`.

Reúne descriptores usados, los ordena por inicio y calcula huecos desde sizeof(MBR) hasta mbr_tamano. Devuelve rangos temporales start/size.

**Devuelve:** `vector<DiskManager::FreeSpace>`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### chooseSpace()

**Recibe:** `const vector<FreeSpace>& freeSpaces, int requiredSize, char fit`.

Recorre huecos y devuelve el índice de FF, BF o WF que alcanza requiredSize. Si ninguno sirve devuelve -1; no asigna ni escribe.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### createPrimaryOrExtended()

**Recibe:** `const string& path, MBR& mbr, int sizeBytes, char type, char fit, const string& name, string& message`.

Exige entrada libre, máximo una extendida y espacio para su EBR. Elige un hueco usando dsk_fit, llena un Partition y escribe el MBR; si es E crea el EBR vacío.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### createLogical()

**Recibe:** `const string& path, MBR& mbr, int sizeBytes, char fit, const string& name, string& message`.

Calcula el costo EBR + contenido sin desbordamiento. Lee posiciones de la cadena, incluye hueco inicial y huecos intermedios/finales, elige por fit y enlaza el nuevo EBR entre anterior y siguiente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### createFirstEBR()

**Recibe:** `const string& path, const Partition& extendedPartition`.

Escribe un EBR con tamaño 0, next -1 y comienzo lógico después de sizeof(EBR). Mantiene anclada la cadena aunque aún no existan lógicas.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### deletePrimaryOrExtended()

**Recibe:** `const string& path, MBR& mbr, int partitionIndex, string& message, bool full`.

Rechaza una partición montada; FULL pone a cero la región, FAST conserva datos, y ambos sustituyen la entrada MBR por Partition vacío.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### deleteLogical()

**Recibe:** `const string& path, const string& name, string& message, bool full`.

Si borra el primer nodo deja un EBR vacío con el enlace previo. Para otros nodos enlaza anterior con siguiente y FULL limpia EBR/contenido, mientras FAST conserva la región desconectada. Conserva el resto de la cadena.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### resizePrimaryOrExtended()

**Recibe:** `const string& path, MBR& mbr, int partitionIndex, int addBytes, string& message`.

Al crecer limita por la siguiente partición o fin del disco. Al reducir exige tamaño positivo y calcula el último byte ocupado por EBR/lógicas para no cortarlos; limpia la cola liberada.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### resizeLogical()

**Recibe:** `const string& path, const string& name, int addBytes, string& message`.

Limita crecimiento por el siguiente EBR o fin de la extendida. La reducción mantiene tamaño positivo, limpia la cola y guarda el EBR con nuevo tamaño.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### clearSpace()

**Recibe:** `const string& path, int start, int size`.

Escribe ceros en un rango físico, validando posiciones no negativas y que el rango no exceda el archivo. No se usa para crear discos desde cero.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileSystemManager.cpp`, `BinaryUtils.cpp` y métodos de esta clase.

**Preguntas de defensa de DiskManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/managers/FileSystemManager.hpp

Declara la clase `FileSystemManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/FileSystemManager.hpp`, `utils/BinaryUtils.hpp`, `managers/DiskManager.hpp`. La implementación está en `backend/src/managers/FileSystemManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/FileSystemManager.cpp

Implementa `FileSystemManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### formatExt2()

**Recibe:** `const string& path, int partitionStart, int partitionSize, string& message`.

Valida el rango físico y n mínimo. Limpia la partición completa, crea superbloque, bitmaps, tabla vacía y bloques; crea raíz/users.txt y guarda los contadores finales.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MkFsCommand.cpp`.

### readSuperBlock()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock`. **Devuelve:** bool. Lee el encabezado mediante BinaryUtils. Si no tiene magic EXT devuelve la lectura para que el llamador detecte falta de formato. Si está formateado, localiza la primaria en MBR y valida tipo 2/3, n >= 2, 3n bloques, tamaños nativos, offsets exactos y fin del layout dentro de la partición. Usa el n persistido, no recalcula n a partir de un tamaño ampliado. `journalSize`, `bitmapStart`, `inodeStart`, `blockStart` y `end` expresan posiciones físicas con cálculo intermedio long long. No cambia el disco; devuelve false ante encabezado incompatible.

### writeSuperBlock()

**Recibe:** `const string& path, int partitionStart, const SuperBlock& superBlock`.

Persiste una copia del encabezado en partitionStart mediante escritura binaria limitada al archivo.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MountManager.cpp` y métodos de esta clase.

### readInode()

**Recibe:** `const string& path, const SuperBlock& superBlock, int inodeIndex, Inode& inode`.

Valida el índice contra el total de inodos, calcula `s_inode_start + índice * s_inode_s` y lee exactamente un Inode con BinaryUtils. Entrega la estructura por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`.

### writeInode()

**Recibe:** `const string& path, const SuperBlock& superBlock, int inodeIndex, const Inode& inode`.

Valida el índice contra el total de inodos, calcula `s_inode_start + índice * s_inode_s` y escribe exactamente un Inode con BinaryUtils. Persiste la estructura recibida; no asigna por sí sola el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp` y métodos de esta clase.

### readFolderBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, FolderBlock& folderBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y lee exactamente un FolderBlock con BinaryUtils. Entrega la estructura por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`.

### writeFolderBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, const FolderBlock& folderBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y escribe exactamente un FolderBlock con BinaryUtils. Persiste la estructura recibida; no asigna por sí sola el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp` y métodos de esta clase.

### readFileBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, FileBlock& fileBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y lee exactamente un FileBlock con BinaryUtils. Entrega la estructura por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`.

### writeFileBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, const FileBlock& fileBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y escribe exactamente un FileBlock con BinaryUtils. Persiste la estructura recibida; no asigna por sí sola el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp` y métodos de esta clase.

### readPointerBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, PointerBlock& pointerBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y lee exactamente un PointerBlock con BinaryUtils. Entrega la estructura por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`.

### writePointerBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, const PointerBlock& pointerBlock`.

Valida el índice contra el total de bloques, calcula `s_block_start + índice * s_block_s` y escribe exactamente un PointerBlock con BinaryUtils. Persiste la estructura recibida; no asigna por sí sola el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`.

### findFreeInode()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Abre una sola vez el bitmap y lo recorre desde el inicio hasta encontrar '0'. Devuelve el índice libre del inodo o -1 si no hay espacio o falla lectura.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### findFreeBlock()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Abre una sola vez el bitmap y lo recorre desde el inicio hasta encontrar '0'. Devuelve el índice libre del bloque o -1 si no hay espacio o falla lectura.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### allocateInode()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock`.

Busca un inodo libre, marca '1', reduce el contador, actualiza primeras direcciones y persiste SuperBlock. Devuelve índice asignado; -1 si no pudo reservar.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** `FileManager.cpp`.

### allocateBlock()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock`.

Busca un bloque libre, marca '1', reduce el contador, actualiza primeras direcciones y persiste SuperBlock. Devuelve índice asignado; -1 si no pudo reservar.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** `FileManager.cpp`.

### freeInode()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock, int inodeIndex`.

Valida índice, comprueba bitmap y evita liberar dos veces. Marca '0', limpia el inodo, incrementa libres y guarda SuperBlock.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`.

### freeBlock()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock, int blockIndex`.

Valida índice, comprueba bitmap y evita liberar dos veces. Marca '0', limpia el bloque, incrementa libres y guarda SuperBlock.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`.

### readInodeBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock, int inodeIndex, char& value`.

Lee por referencia un carácter del bitmap de inodos en la posición solicitada, con comprobación de índice. No cambia el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp` y métodos de esta clase.

### readBlockBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, char& value`.

Lee por referencia un carácter del bitmap de bloques en la posición solicitada, con comprobación de índice. No cambia el bitmap.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp` y métodos de esta clase.

### calculateInodes()

**Recibe:** `int partitionSize`.

Resta sizeof(SuperBlock) y divide el espacio disponible por 4 + sizeof(Inode) + 3*64. Devuelve 0 si no hay espacio.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### createSuperBlock()

**Recibe:** `int partitionStart, int, int inodeCount`.

Construye en memoria el encabezado con n/3n y offsets consecutivos, magic, tamaños, fecha y montaje inicial. No escribe por sí misma.

**Devuelve:** `SuperBlock`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### clearPartition()

**Recibe:** `const string& path, int partitionStart, int partitionSize`.

Delega la puesta a cero de toda la región de la primaria a BinaryUtils.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### initializeInodeBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Abre el archivo y escribe '0' en cada una de las n posiciones del bitmap de inodos.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### initializeBlockBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Escribe '0' en las 3n posiciones del bitmap de bloques.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### initializeInodes()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Escribe n Inode vacíos consecutivos; el constructor ya puso sus quince apuntadores en -1.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### initializeBlocks()

**Recibe:** `const string& path, const SuperBlock& superBlock`.

Limpia los 3n bloques de 64 bytes sin asignarlos todavía.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### createRoot()

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock`.

Ocupa inodos/bloques 0 y 1, escribe raíz con . y .., crea la entrada users.txt y su contenido root. Ajusta contadores y primeras direcciones libres.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### writeInodeBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock, int inodeIndex, char value`.

Escribe value en el bitmap de inodos tras validar índice. El llamador administra el contador y la actualización del superbloque.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### writeBlockBitmap()

**Recibe:** `const string& path, const SuperBlock& superBlock, int blockIndex, char value`.

Escribe value en el bitmap de bloques tras validar índice. El llamador administra el contador y la actualización del superbloque.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### updateFirstFree()

**Recibe:** `const string& path, SuperBlock& superBlock`.

Busca índices libres y los transforma en offsets de tabla/área de bloques; usa -1 si se agotó un bitmap. Modifica la copia del superbloque recibida.

**Devuelve:** `void`: sin valor de retorno. **Quién la llama:** otros métodos de esta misma clase.

**Preguntas de defensa de FileSystemManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/managers/FileManager.hpp

Declara la clase `FileManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/FileManager.hpp`, `managers/FileSystemManager.hpp`, `managers/MountManager.hpp`, `state/AppState.hpp`, `utils/BinaryUtils.hpp`. La implementación está en `backend/src/managers/FileManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/FileManager.cpp

Implementa `FileManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### createDirectory()

**Recibe:** `const string& path, bool recursive, AppState& appState, string& message`.

Valida sesión, recorre componentes desde raíz, verifica que sean carpetas y crea faltantes con permiso de escritura. Sin recursive no crea padres. Con -p una carpeta ya existente se considera satisfecha.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MkDirCommand.cpp`.

### createFile()

**Recibe:** `const string& path, const string& content, bool recursive, bool overwrite, AppState& appState, string& message`.

Valida sesión y nombre; resuelve o crea padres, exige permiso en el padre y distingue archivo/carpeta existente. Sin overwrite guarda una pregunta; con autorización escribe contenido. Para nuevos archivos asigna inodo y entrada.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `main.cpp`, `MkFileCommand.cpp`.

### readFile()

**Recibe:** `const string& path, const AppState& appState, string& content, string& message`.

Resuelve la ruta, exige tipo archivo y permiso de lectura, concatena datos por i_s y actualiza i_atime. Entrega contenido por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp`, `UserManager.cpp`, `CatCommand.cpp`.

### writeFile()

**Recibe:** `const string& path, const string& content, AppState& appState, string& message`.

Resuelve un archivo existente, comprueba permiso de escritura y reemplaza su contenido. UserManager lo usa para users.txt; no es un comando adicional del lenguaje.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `UserManager.cpp`.

### findInodeByPath()

**Recibe:** `const string& path, const AppState& appState, int& inodeIndex, Inode& inode, string& message`.

Lee raíz y busca cada componente en bloques carpeta. Devuelve índice e inodo encontrado; no conserva el árbol de rutas entre llamadas.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp` y métodos de esta clase.

### getDirectoryContent()

**Recibe:** `const string& path, const AppState& appState, vector<Content>& entries, string& message`.

Resuelve una carpeta, comprueba lectura y reúne temporalmente las cuatro entradas de cada bloque de datos alcanzable; lo usa REP LS.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ReportManager.cpp`.

### pathExists()

**Recibe:** `const string& path, const AppState& appState`.

Invoca findInodeByPath con salidas locales y devuelve únicamente si pudo resolver la ruta.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### splitPath()

**Recibe:** `const string& path`.

Separa componentes por / y omite componentes vacíos. Conserva los nombres y no consulta el disco.

**Devuelve:** `vector<string>`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### findEntry()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, const Inode& directoryInode, const string& name`.

Recorre bloques de datos de un inodo carpeta, interpreta sus cuatro Content y compara nombres. Devuelve índice del hijo o -1.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### findFreeDirectoryEntry()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, int directoryInodeIndex, Inode& directoryInode, int& blockIndex, int& entryIndex`.

Busca una entrada b_inodo=-1. Si los bloques existentes están llenos verifica costo de expansión, asigna otro FolderBlock y actualiza tamaño/apuntadores del inodo padre.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### addDirectoryEntry()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, int directoryInodeIndex, Inode& directoryInode, const string& name, int newInodeIndex`.

Obtiene bloque y casilla libres, copia nombre e índice del hijo y persiste el bloque; actualiza mtime del padre.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### createSingleDirectory()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, int parentInodeIndex, const string& name, int uid, int gid, int& newInodeIndex`.

Asigna un inodo y un bloque; llena uid/gid, 664, fechas y . / ..; enlaza la carpeta desde su padre. Libera asignaciones nuevas si no consigue completar la creación.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### createFileInode()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, int uid, int gid, const string& content, int& inodeIndex`.

Reserva un inodo nuevo con dueño/grupo de la sesión, 664 y fechas; escribe el contenido mediante writeFileContent. Libera el inodo ante error.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### writeFileContent()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, int inodeIndex, Inode& inode, const string& content`.

Comprueba máximo y costo de datos/apuntadores usando también los bloques recuperables del archivo actual. Después libera el contenido previo, escribe bloques de 64 bytes, coloca referencias y actualiza i_s/i_mtime.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### readFileContent()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, const Inode& inode, string& content`.

Recorre ceil(i_s/64) bloques lógicos, los resuelve mediante getDataBlock y copia solo los bytes válidos del último. No busca terminador nulo.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getDataBlock()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, const Inode& inode, int logicalBlock`.

Clasifica el bloque lógico en directo, simple, doble o triple usando umbrales 12, 28, 284 y 4380 y devuelve índice físico de bloque.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### setDataBlock()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex`.

Selecciona el método de asignación correspondiente al rango lógico; en directos modifica i_block directamente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getSimpleIndirectBlock()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position`.

Lee un PointerBlock y devuelve su entrada position, validando el rango 0..15.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### getDoubleIndirectBlock()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position`.

Divide position entre 16; lee los dos niveles de apuntadores y devuelve el índice del bloque de datos.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### getTripleIndirectBlock()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position`.

Divide position entre 256 y después el residuo entre 16; sigue tres niveles hasta el bloque de datos.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### setSimpleIndirectBlock()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex`.

Crea el bloque apuntador de i_block[12] cuando está ausente; escribe el índice de datos en la casilla correspondiente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### setDoubleIndirectBlock()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex`.

Crea raíz y segundo nivel del doble según necesidad, dejando -1 en casillas sin usar; enlaza el bloque de datos.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### setTripleIndirectBlock()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex`.

Crea hasta tres niveles de PointerBlock y escribe cada enlace persistente antes de colocar el bloque de datos en la hoja.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### freeFileBlocks()

**Recibe:** `const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode`.

Libera doce directos y cada árbol indirecto desde las hojas hacia la raíz. Pone i_block en -1 e i_s en 0 y actualiza bitmaps/contadores mediante FileSystemManager.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### canRead()

**Recibe:** `const Inode& inode, int uid, int gid`.

Devuelve true para uid 1; para otros consulta getPermission y aplica la máscara 4.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### canWrite()

**Recibe:** `const Inode& inode, int uid, int gid`.

Devuelve true para uid 1; para otros aplica la máscara 2 del permiso seleccionado.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getPermission()

**Recibe:** `const Inode& inode, int uid, int gid`.

Selecciona U si coincide uid, G si coincide gid o O. Rechaza un carácter fuera de 0..7 devolviendo permiso 0.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

**Preguntas de defensa de FileManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/managers/UserManager.hpp

Declara la clase `UserManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/UserManager.hpp`, `managers/FileManager.hpp`, `managers/MountManager.hpp`, `state/AppState.hpp`, `state/Session.hpp`, `utils/StringUtils.hpp`. La implementación está en `backend/src/managers/UserManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/UserManager.cpp

Implementa `UserManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### login()

**Recibe:** `const string& user, const string& password, const string& id, AppState& appState, string& message`.

Impide otra sesión en el mismo navegador, resuelve montaje y lee users.txt con un estado temporal root. Distingue usuario inexistente de credenciales incorrectas y guarda la sesión autenticada.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `LoginCommand.cpp`.

### logout()

**Recibe:** `AppState& appState, string& message`.

Exige sesión activa y la sustituye por Session vacío; no desmonta ni reformatea.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `LogoutCommand.cpp`.

### createGroup()

**Recibe:** `const string& name, AppState& appState, string& message`.

Exige root, longitud válida y grupo no duplicado; calcula siguiente GID y añade línea G al archivo users.txt.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MkGrpCommand.cpp`.

### removeGroup()

**Recibe:** `const string& name, AppState& appState, string& message`.

Exige root, impide borrar el grupo root y marca con ID 0 el grupo existente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `RmGrpCommand.cpp`.

### createUser()

**Recibe:** `const string& user, const string& password, const string& group, AppState& appState, string& message`.

Exige root, campos no vacíos de hasta diez caracteres, grupo activo y usuario no duplicado. Añade registro U con nuevo UID.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MkUsrCommand.cpp`.

### removeUser()

**Recibe:** `const string& user, AppState& appState, string& message`.

Exige root y usuario existente distinto de root. Marca el registro con ID 0 sin eliminar la línea histórica.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `RmUsrCommand.cpp`.

### changeUserGroup()

**Recibe:** `const string& user, const string& group, AppState& appState, string& message`.

Exige root, usuario activo y grupo activo; actualiza el campo de grupo y reescribe users.txt.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `ChGrpCommand.cpp`.

### readUsersFile()

**Recibe:** `const AppState& appState, string& content, string& message`.

Lee /users.txt por medio de FileManager y propaga contenido/mensaje.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### writeUsersFile()

**Recibe:** `AppState& appState, const string& content, string& message`.

Escribe /users.txt con FileManager, reutilizando sus bloques, límites y controles de escritura.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### hasActiveSession()

**Recibe:** `const AppState& appState`.

Consulta session.active sin acceder al disco.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### isRoot()

**Recibe:** `const AppState& appState`.

Comprueba sesión activa, nombre root y uid 1 para autorizar administración de cuentas.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### groupExists()

**Recibe:** `const string& content, const string& group`.

Separa temporalmente líneas/campos; busca un registro G de tres campos, ID distinto de cero y nombre exacto.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### userExists()

**Recibe:** `const string& content, const string& user`.

Busca un registro U activo de cinco campos por el nombre exacto del usuario.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### validLogin()

**Recibe:** `const string& content, const string& user, const string& password, int& uid, int& gid`.

Compara usuario y contraseña sin normalizarlos; convierte UID sin excepciones y obtiene el GID activo. Entrega ambos identificadores por referencia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getGroupId()

**Recibe:** `const string& content, const string& group`.

Busca un grupo activo y convierte su ID; devuelve -1 si no existe o el ID no es válido.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### getNextGroupId()

**Recibe:** `const string& content`.

Cuenta registros G históricos, incluidos los eliminados, y obtiene el mayor ID legible. Devuelve uno más que el mayor de ambas cantidades.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### getNextUserId()

**Recibe:** `const string& content`.

Hace el mismo cálculo histórico para registros U; así evita reutilizar el último UID borrado.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### markGroupDeleted()

**Recibe:** `string& content, const string& group`.

Reconstruye el texto de users.txt cambiando solo a 0 el ID de la línea G objetivo. Modifica content por referencia; no escribe disco directamente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### markUserDeleted()

**Recibe:** `string& content, const string& user`.

Reconstruye el texto conservando grupo, usuario y contraseña, pero sustituyendo UID por 0 para el usuario objetivo.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### updateUserGroup()

**Recibe:** `string& content, const string& user, const string& group`.

Reconstruye la línea U con el nuevo campo grupo y conserva UID/nombre/contraseña.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

**Preguntas de defensa de UserManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/managers/MountManager.hpp

Declara la clase `MountManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/MountManager.hpp`, `managers/DiskManager.hpp`, `managers/FileSystemManager.hpp`, `utils/PathUtils.hpp`, `state/AppState.hpp`, `utils/BinaryUtils.hpp`, `utils/StringUtils.hpp`. La implementación está en `backend/src/managers/MountManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/MountManager.cpp

Implementa `MountManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### mountPartition()

**Recibe:** `const string& inputPath, const string& name, AppState& appState, string& message`.

Canoniza la ruta, localiza una primaria, impide duplicados y obtiene letra/correlativo/ID. Actualiza el descriptor y el superbloque si ya existe EXT2/EXT3, y agrega MountedPartition en RAM.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `MountCommand.cpp`.

### getMountedPartition()

**Recibe:** `const AppState& appState, const string& id, MountedPartition& mountedPartition`.

Busca un ID en RAM y vuelve a validar que el disco y descriptor sigan existiendo con el mismo inicio y tamaño antes de entregar la copia.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`, `UserManager.cpp`, `MkFsCommand.cpp`.

### getMountedPartitions()

**Recibe:** `const AppState& appState`.

Devuelve una copia de la lista de montajes; no lee ni reconstruye archivos de EXT2.

**Devuelve:** `vector<MountedPartition>`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### isMounted()

**Recibe:** `const AppState& appState, const string& path, const string& name`.

Comprueba ruta y nombre contra montajes existentes; evita montar dos veces la misma partición durante el proceso.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getDiskLetter()

**Recibe:** `const AppState& appState, const string& path`.

Conserva la letra si ya hay un montaje de ese disco; para uno nuevo busca A..Z libre. Devuelve carácter nulo si no hay letras.

**Devuelve:** `char`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### getNextCorrelative()

**Recibe:** `const AppState& appState, const string& path`.

Recorre números del 1 al 9 y devuelve el primero no utilizado por ese disco. Reutiliza correlativos de particiones desmontadas sin colisionar con otros IDs.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** otros métodos de esta misma clase.

### createMountId()

**Recibe:** `const string& carnet, int correlative, char diskLetter`.

Valida carnet, correlativo 1..9 y letra A..Z; concatena los dos últimos dígitos, ordinal y letra. Devuelve texto vacío ante datos inválidos.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### updatePartitionMount()

**Recibe:** `const string& path, const string& name, int correlative, const string& id`.

Lee el MBR, exige P, escribe status 1, ordinal e ID de cuatro bytes y persiste el MBR.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

**Preguntas de defensa de MountManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/managers/ReportManager.hpp

Declara la clase `ReportManager` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `managers/ReportManager.hpp`, `managers/DiskManager.hpp`, `managers/FileManager.hpp`, `managers/FileSystemManager.hpp`, `managers/MountManager.hpp`, `state/AppState.hpp`, `utils/BinaryUtils.hpp`, `utils/PathUtils.hpp`, `utils/StringUtils.hpp`. La implementación está en `backend/src/managers/ReportManager.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/managers/ReportManager.cpp

Implementa `ReportManager`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### generateReport()

**Recibe:** `const string& name, const string& path, const string& id, const string& pathFileLs, const AppState& appState, string& message`.

Valida extensión, evita sustituir un disco montado y deriva por uno de los diez nombres admitidos. Los reportes de texto se guardan como txt; los gráficos se convierten con Graphviz.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `RepCommand.cpp`.

### reportMBR()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Lee MBR y genera tablas de sus campos, de las cuatro entradas Partition y de los EBR. Incluye descriptores vacíos para mostrar toda la información.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportDisk()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Ordena segmentos MBR, particiones y libres; genera una fila exterior cuyos porcentajes redondeados suman 100. Dentro de la extendida inserta EBR, lógicas y huecos como tabla anidada.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportInode()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Recorre bitmap de inodos y genera tablas solo para ocupados, incluyendo todos los campos y los quince apuntadores.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportBlock()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Recorre los inodos ocupados y sus árboles indirectos; imprime FolderBlock, FileBlock y PointerBlock una sola vez mediante un conjunto temporal de índices.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportBitmapInode()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Comprueba EXT2, recorre todo el bitmap de inodos y genera texto con veinte registros por línea.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportBitmapBlock()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Comprueba EXT2, recorre todo el bitmap de bloques y genera texto con veinte registros por línea.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportTree()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Comienza en el inodo raíz y recorre carpetas/hijos y los tres niveles indirectos. Omite aristas de retorno . y .. para no ciclar; incluye campos de inodos y bloques.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportSuperBlock()

**Recibe:** `const string& outputPath, const string& id, const AppState& appState, string& message`.

Genera tabla con todos los campos de SuperBlock y representa magic en hexadecimal.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportFile()

**Recibe:** `const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message`.

Exige la sesión activa en el montaje solicitado y permisos de lectura, lee la ruta interna completa y escribe nombre, ruta y contenido en texto.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### reportLs()

**Recibe:** `const string& outputPath, const string& id, const string& internalPath, const AppState& appState, string& message`.

Resuelve la ruta solicitada, lee users.txt para traducir UID/GID y genera filas con permisos, propietario, grupo, fechas, hora, tipo y nombre. Para carpeta lista hijos; para archivo una fila.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### generateGraphviz()

**Recibe:** `const string& dotContent, const string& outputPath, string& message`.

Escribe un DOT temporal, inicia dot con fork/exec y espera su resultado. No construye comandos de shell con las rutas. Borra el temporal y devuelve error si dot falla; .dot se entrega directamente.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### generateTextFile()

**Recibe:** `const string& content, const string& outputPath, string& message`.

Crea directorios padre y escribe contenido en el archivo de reporte.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### getExtension()

**Recibe:** `const string& path`.

Obtiene la extensión sin punto; ReportManager la normaliza a minúsculas para escoger formato.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp`, `PathUtils.cpp` y métodos de esta clase.

### createOutputDirectories()

**Recibe:** `const string& path`.

Delega a PathUtils la creación de carpetas para un reporte.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### formatDate()

**Recibe:** `time_t date`.

Convierte time_t a día/mes/año y hora local; para cero devuelve un guion.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### charArrayToString()

**Recibe:** `const char* text, int size`.

Usa BinaryUtils para leer arreglos fijos sin exceder su longitud.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### addLogicalPartitionsMBR()

**Recibe:** `const string& diskPath, const Partition& extendedPartition, string& dotContent`.

Recorre EBR y concatena tablas con todos sus campos y flechas part_next al DOT del MBR.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### addLogicalPartitionsDisk()

**Recibe:** `const string& diskPath, const Partition& extendedPartition, string& dotContent`.

Helper anterior que añade celdas de lógicas. El reporte DISK actual construye su tabla anidada directamente; se conservó sin rediseñar.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### addInodeTree()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int inodeIndex, string& dotContent`.

Helper anterior que genera una tabla resumida de inodo; reportTree actual usa su recorrido local más completo. No es la ruta principal de generación.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### addFolderBlockTree()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent`.

Helper que dibuja las cuatro entradas de un FolderBlock. Se conserva; el recorrido completo activo está dentro de reportTree.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### addFileBlockTree()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int blockIndex, string& dotContent`.

Helper que dibuja contenido de un FileBlock; no reemplaza la función local usada por reportTree.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### addPointerBlockTree()

**Recibe:** `const string& diskPath, const SuperBlock& superBlock, int blockIndex, int level, string& dotContent`.

Helper anterior que dibuja apuntadores y sus niveles inferiores. El árbol activo usa addPointerBlock local y sigue también datos.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### permissionToText()

**Recibe:** `const Inode& inode`.

Convierte cada dígito octal en tres letras rwx o guiones; devuelve nueve posiciones para REP LS.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

**Preguntas de defensa de ReportManager:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/utils/BinaryUtils.hpp

Declara la clase `BinaryUtils` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `utils/BinaryUtils.hpp`. La implementación está en `backend/src/utils/BinaryUtils.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/utils/BinaryUtils.cpp

Implementa `BinaryUtils`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### clearSpace()

**Recibe:** `const string& path, int start, int size`.

Escribe ceros en un rango físico, validando posiciones no negativas y que el rango no exceda el archivo. No se usa para crear discos desde cero.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `DiskManager.cpp`, `FileSystemManager.cpp`.

### fileExists()

**Recibe:** `const string& path`.

Intenta abrir el archivo para lectura binaria y devuelve si pudo hacerlo.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `DiskManager.cpp`.

### getFileSize()

**Recibe:** `const string& path`.

Abre con ios::ate y obtiene la posición final como long long; -1 significa que no pudo abrirlo.

**Devuelve:** `long long`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `DiskManager.cpp`, `FileSystemManager.cpp` y métodos de esta clase.

### copyToFixedChar()

**Recibe:** `char* destination, int size, const string& text`.

Pone a cero destination y copia como máximo size bytes; puede ocupar todo el arreglo sin terminador.

**Devuelve:** `void`: sin valor de retorno. **Quién la llama:** `DiskManager.cpp`, `FileManager.cpp`, `FileSystemManager.cpp`, `MountManager.cpp`.

### fixedCharToString()

**Recibe:** `const char* text, int size`.

Lee como máximo size bytes y se detiene en cero si lo encuentra; es apropiada para nombres/campos fijos.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `DiskManager.cpp`, `FileManager.cpp`, `ReportManager.cpp`, `MountManager.cpp`.

**Preguntas de defensa de BinaryUtils:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/utils/PathUtils.hpp

Declara la clase `PathUtils` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `utils/PathUtils.hpp`. La implementación está en `backend/src/utils/PathUtils.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/utils/PathUtils.cpp

Implementa `PathUtils`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### createDirectories()

**Recibe:** `const string& path`.

Obtiene el padre; si ya es directorio devuelve éxito. Usa error_code para crear padres sin propagar excepciones del filesystem.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `DiskManager.cpp`, `ReportManager.cpp`.

### getParentPath()

**Recibe:** `const string& path`.

Extrae la carpeta padre usando filesystem::path; no crea carpetas.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** otros métodos de esta misma clase.

### getFileName()

**Recibe:** `const string& path`.

Extrae el último componente de una ruta física.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp`.

### getExtension()

**Recibe:** `const string& path`.

Obtiene la extensión sin punto; ReportManager la normaliza a minúsculas para escoger formato.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp`, `ReportManager.cpp` y métodos de esta clase.

### hasExtension()

**Recibe:** `const string& path, const string& extension`.

Compara extensión real y solicitada sin distinguir mayúsculas.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `DiskManager.cpp`.

### isAbsolute()

**Recibe:** `const string& path`.

Consulta si la ruta física es absoluta mediante filesystem::path.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### canonicalPath()

**Recibe:** `const string& path`.

Resuelve ruta absoluta y enlaces existentes para reconocer el mismo disco aunque se escriba de otra forma. Usa error_code y conserva la ruta original si no puede resolverla.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp`, `ReportManager.cpp`, `MountManager.cpp`.

**Preguntas de defensa de PathUtils:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

## backend/include/utils/StringUtils.hpp

Declara la clase `StringUtils` y los contratos descritos a continuación. La responsabilidad de este encabezado es permitir que sus llamadores compilen con firmas únicas. No guarda un árbol de EXT2 ni ejecuta comandos al incluirse. Utiliza `utils/StringUtils.hpp`. La implementación está en `backend/src/utils/StringUtils.cpp`. Las referencias no constantes señalan resultados o estado que puede modificarse.

## backend/src/utils/StringUtils.cpp

Implementa `StringUtils`. En el flujo general trabaja después de la validación del comando o como apoyo de otro manager. Cada método se detalla con su entrada, efecto y resultado; los significados de rutas, índices y referencias están en el vocabulario anterior.

### toLower()

**Recibe:** `const string& text`.

Copia texto y convierte caracteres a minúsculas utilizando unsigned char para cctype.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp`, `ReportManager.cpp`, `CommandParser.cpp`, `MkFsCommand.cpp`, `FDiskCommand.cpp`, `RepCommand.cpp` y métodos de esta clase.

### toUpper()

**Recibe:** `const string& text`.

Copia texto y lo convierte a mayúsculas para opciones enumeradas.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `FDiskCommand.cpp`, `MkDiskCommand.cpp`.

### equalsIgnoreCase()

**Recibe:** `const string& firstText, const string& secondText`.

Compara dos copias normalizadas; no modifica los valores originales.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `DiskManager.cpp`, `MountManager.cpp`.

### trim()

**Recibe:** `const string& text`.

Quita espacios, tabulaciones y terminadores de los extremos. Se usa para clasificar líneas y respuestas de confirmación.

**Devuelve:** `string`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `main.cpp` y métodos de esta clase.

### split()

**Recibe:** `const string& text, char delimiter`.

Separa por un delimitador y conserva campos vacíos; se usa especialmente para líneas y columnas de users.txt.

**Devuelve:** `vector<string>`: resultado descrito; no retorna un código de éxito separado. **Quién la llama:** `ReportManager.cpp`, `UserManager.cpp`.

### isEmpty()

**Recibe:** `const string& text`.

Devuelve si el texto queda vacío después de trim.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** contrato auxiliar conservado, sin llamadas productivas externas actuales.

### toPositiveInt()

**Recibe:** `const string& text`.

Convierte dígitos con comprobación de desbordamiento antes de multiplicar. Acepta cero y devuelve -1 ante texto inválido; sus llamadores exigen positivo cuando corresponda.

**Devuelve:** `int`: índice/ID calculado o centinela indicado en la explicación. **Quién la llama:** `main.cpp`, `UserManager.cpp`.

**Preguntas de defensa de StringUtils:** ¿Qué validación ocurre antes de escribir? ¿Qué referencias son salidas? ¿Dónde se convierte índice en dirección? Para responder, identifique la función concreta anterior y siga sus llamadas a BinaryUtils o al manager correspondiente; no diga que el objeto contiene todo el filesystem.

# Analizador y estado

## backend/include/analysis/CommandToken.hpp

Define TokenType (Word, Parameter, Equal, QuotedValue, EndOfLine, Invalid, Comment), Token y TokenizeResult. Token almacena tipo, texto y `separated`: permite al parser saber si había espacio antes de un parámetro. TokenizeResult reúne tokens y mensajes léxicos. Declara CommandToken::tokenize e isSeparator. Es usado por el parser y main; no cambia archivos.

## backend/src/analysis/CommandToken.cpp

`tokenize(inputText)` recibe el texto y avanza con un índice. Salta espacios, genera EndOfLine para saltos, reúne comentarios desde #, separa = y lee valores entre comillas hasta su cierre. Si las comillas llegan al fin de línea sin cerrar genera Invalid y un error léxico. Reconoce parámetros que empiezan con guion y letra; después de = permite un valor que empiece con guion, como un negativo. Reúne palabras y rechaza controles/símbolos de su alfabeto inválido fuera de comillas. Devuelve TokenizeResult y no escribe disco. Lo llama processInput una vez por línea.

`isSeparator(character)` devuelve si el carácter termina una palabra (espacios, tabulador, CR/LF, = o #). No convierte texto ni reconoce comandos. Lo usa tokenize. Pregunta de defensa: ¿por qué no todo error de parámetros es léxico? Porque un parámetro desconocido puede estar perfectamente tokenizado.

## backend/include/analysis/CommandParser.hpp

Declara ParseResult con commands y errors y la clase CommandParser. `parse(tokens)` transforma tokens en ParsedCommand. `toLower(text)` normaliza nombres. Depende de CommandToken.hpp y ParsedCommand.hpp; main usa sus resultados.

## backend/src/analysis/CommandParser.cpp

`toLower(text)` devuelve una copia en minúsculas. `parse(tokens)` recorre líneas, conserva comentarios como ParsedCommand de comentario, exige Word inicial y un nombre del vocabulario de este proyecto. Para cada parámetro verifica separación, quita el guion, acepta `=valor` si el token siguiente es Word o QuotedValue y guarda si hubo valor. Al fallar avanza hasta EndOfLine. Devuelve comandos válidos y errores sintácticos; no invoca managers. processInput evita llamarlo en líneas que ya fallaron léxicamente. Un parámetro sin = se conserva como bandera y el Command decide si esa bandera es válida.

## backend/include/analysis/ParsedCommand.hpp

ParsedParam contiene name, value y hasValue. ParsedCommand contiene name, params, isComment y commentText. `hasParam(nombre)` devuelve existencia; `getParam(nombre)` devuelve el primer valor o texto vacío; `paramHasValue(nombre)` devuelve si se escribió con valor; `countParam(nombre)` cuenta repeticiones; `hasRepeatedParam(nombre)` compara el conteo con uno. Todas reciben el nombre normalizado y consultan el vector temporal sin escribir. Las clases Command usan estos métodos para validar opciones; el parser construye las instancias.

## backend/include/validation/ValidationResult.hpp

Struct con success y message. Es el retorno uniforme de las clases Command. No contiene comportamiento ni efectos. executeCommand lo propaga a processInput, que decide si incrementa exitosos. Tener message no implica éxito: se debe consultar success.

## backend/include/state/Session.hpp

Struct de estado de aplicación con active, user, uid, gid y partitionId. LOGIN lo llena y LOGOUT lo restablece. FileManager y UserManager leen sus datos para permisos; no se serializa en el `.mia`.

## backend/include/state/MountedPartition.hpp

Struct con id, path, name, start, size, correlative y diskLetter. MountManager lo crea a partir del MBR y lo guarda en AppState; MKFS, archivos y reportes lo usan para localizar la región física. Es una referencia de montaje, no una representación de carpetas.

## backend/include/state/AppState.hpp

Contiene carnet, mountedPartitions, session y pendingConfirmation. PendingConfirmation guarda active, operation, name, deleteType, path, content, recursive, remainingInput, tres contadores y allSuccess. El contenido solo está retenido mientras se decide la sobrescritura; no reemplaza al archivo permanente. FileManager prepara la pregunta; processInput guarda continuación y contadores, consume la respuesta y limpia el estado. No se agregaron estos campos a ninguna estructura del PDF.

**Defensa:** ¿Qué pasa si cierro el proceso con una pregunta pendiente? La pregunta y el texto no ejecutado se pierden; el archivo existente todavía no fue sustituido. El disco conserva lo que se ejecutó antes de la pregunta.

## backend/src/main.cpp

Inicializa directorios, carga montajes persistentes, configura Crow y coordina comandos/HTTP. Utiliza todos los comandos existentes, FileManager, DiskManager, FileSystemManager, MountManager, UserManager, StringUtils y BinaryUtils. No contiene una segunda representación persistente del árbol virtual.

| Función real | Parámetros y retorno | Lógica, llamadas y fallos |
|---|---|---|
| newToken | ninguno → string | random_device y ostringstream producen ocho enteros hexadecimales, 256 bits; el token no contiene UID/contraseña. |
| useBrowser | const crow::request& → bool | lee X-MIA-Session, busca token existente en browsers, carga Session/PendingConfirmation, establece activeBrowser. Token desconocido devuelve false. |
| saveBrowser | ninguno → void | copia identidad/confirmación a browsers[activeBrowser] y limpia las copias temporales en appState. Se ejecuta bajo el mutex. |
| jsonMessage | int status, bool success, const string& message → crow::response | serializa éxito, message y messages; usa el HTTP indicado. No modifica discos. |
| safeHostPath | string& path, const string& root → bool | rechaza vacío/NUL, resuelve relativo dentro de root, usa canonicalPath y exige prefijo root/; modifica path a absoluto autorizado. También detecta enlaces que salen del directorio. |
| invalidatePartition | const string& id → void | limpia Session y PendingConfirmation en todos los navegadores con partitionId coincidente y en el estado de petición. |
| executeRaw | const ParsedCommand& command → ValidationResult | if por nombre, llama clases Command o los métodos nuevos de los managers. RMDISK limpia montajes; UNMOUNT y LOSS invalidan sesiones. JOURNALING lee y convierte entradas a salida textual. |
| executeCommand | ParsedCommand command, bool confirmed → ValidationResult | selecciona primera operación FDISK, valida parámetros nuevos y rutas, prepara DELETE pendiente; comprueba LOSS/journal lleno antes de cambios, crea backup, llama executeRaw, agrega journal por éxito, restaura en error y elimina backup. confirmed evita volver a preguntar por DELETE ya autorizado. |
| processInput | string inputText → crow::json::wvalue | consume y/n pendiente, confirma con respaldo, procesa línea/token/parser, cuenta resultados, registra reportes y guarda continuación. EXEC llama recursivamente processInput, máximo 8 niveles y 1 MiB. Mantiene comentarios y salida previa. |
| main | ninguno → int | resuelve variables/directorios, recorre MBR de discos autorizados para recuperar montajes, instala rutas/CORS, valida puerto y arranca Crow con bindaddr. Error de configuración devuelve 1. |

Variables globales: `appState` es AppState de trabajo bajo lock; `appMutex` es mutex global; `reportPaths` vector<string> de reportes registrados; `diskRoot`, `reportRoot`, `inputRoot` strings canónicos; `browsers` map<string,BrowserState>; `activeBrowser` string del token en procesamiento; `scriptDepth` int para límite EXEC. BrowserState agrupa Session session, PendingConfirmation pending y vector<unsigned int> reports, todos en RAM.

Variables relevantes de executeCommand: `required` enumera las claves autorizadas; `hostDisk`, `modifying`, `journaled`, `mounted`, `ext3` son bool; `partition` MountedPartition localiza disco/start/size; `sb` SuperBlock valida tipo/layout; `backup` string refiere al archivo temporal; `error` error_code controla filesystem; `previous` AppState captura estado antes del cambio; `result` ValidationResult gobierna commit/restauración. `route` y `content` son los campos que pasan al journal; password nunca se incorpora.

Variables de processInput: `messages` vector<string>; `reports` vector<wvalue>; `successfulCommands`, `lexicalErrors`, `syntaxErrors` int; `allSuccess` bool acumulado; `pending` PendingConfirmation capturada antes de limpiar; `position`, `end`, `next` size_t recorren líneas; `line`, `trimmed` string; `tokens` TokenizeResult; `parsed` ParseResult; `nested` wvalue y `result` rvalue reúnen EXEC. Los bucles avanzan la posición incluso si una línea falla. `remainingInput` se conserva en el registro del navegador al pausar.

Los handlers de rutas son lambdas de Crow porque esa es su interfaz existente. Sus parámetros, respuestas, componentes y funciones llamadas están en la tabla completa de endpoints más adelante.

# Clases de comandos y sus archivos

## backend/include/commands/MkDiskCommand.hpp

Declara la clase `MkDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkDiskCommand.cpp`.

## backend/src/commands/MkDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKDISK.

Obligatorios: **size, path**. Opcionales/defaults: **fit=FF; unit=M**. Se apoya en `DiskManager::createDisk`. Crea padres físicos, llena ceros y escribe MBR; rechaza valores no positivos y tamaños que exceden int.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getSizeInBytes(size, unit)` recibe entero y unidad K/M y devuelve long long multiplicado por 1024 o 1048576; -1 para unidad no admitida. `getFit(fit)` devuelve B/F/W o carácter nulo. Son auxiliares privados de execute y no escriben archivos.

## backend/include/commands/RmDiskCommand.hpp

Declara la clase `RmDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmDiskCommand.cpp`.

## backend/src/commands/RmDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de RMDISK.

Obligatorios: **path**. Opcionales/defaults: **ninguno**. Se apoya en `DiskManager::removeDisk`. Elimina un .mia existente y el despachador retira sus referencias en RAM.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/FDiskCommand.hpp

Declara la clase `FDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando FDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/FDiskCommand.cpp`.

## backend/src/commands/FDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de FDISK.

Obligatorios: **path, name; size para crear**. Opcionales/defaults: **unit=K; type=P; fit=WF; delete=full y add existentes**. Se apoya en `DiskManager::createPartition/deletePartition/resizePartition`. Distingue creación, eliminación y cambio de tamaño; no admite mezclar operaciones. Los números se convierten manualmente y el delta se limita antes de multiplicar unidades.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getSizeInBytes(size, unit)` convierte B/K/M a long long. `getType(type)` devuelve P/E/L o nulo. `getFit(fit)` devuelve B/F/W o nulo. `isDeleteOperation(command)` e `isAddOperation(command)` consultan presencia de las opciones; devuelven bool. Son llamados por execute y no modifican estado; las conversiones preparan los argumentos de DiskManager.

## backend/include/commands/MountCommand.hpp

Declara la clase `MountCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MOUNT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MountCommand.cpp`.

## backend/src/commands/MountCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MOUNT.

Obligatorios: **path, name**. Opcionales/defaults: **ninguno**. Se apoya en `MountManager::mountPartition`. Busca una primaria, valida duplicados y genera el ID 47 + ordinal + letra.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MountedCommand.hpp

Declara la clase `MountedCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MOUNTED al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MountedCommand.cpp`.

## backend/src/commands/MountedCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MOUNTED.

Obligatorios: **ninguno**. Opcionales/defaults: **ninguno**. Se apoya en `AppState::mountedPartitions`. No recibe parámetros. Devuelve ID, nombre y disco de cada montaje o informa que no hay montajes.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkFsCommand.hpp

Declara la clase `MkFsCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKFS al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkFsCommand.cpp`.

## backend/src/commands/MkFsCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKFS.

Obligatorios: **id**. Opcionales/defaults: **type=full; fs=2fs**. Se apoya en `FileSystemManager::formatExt2` o `formatExt3`. -fs acepta 2fs/3fs y conserva EXT2 por defecto. Reinicia la sesión si reformatea su partición para no conservar credenciales borradas.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/CatCommand.hpp

Declara la clase `CatCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando CAT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/CatCommand.cpp`.

## backend/src/commands/CatCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de CAT.

Obligatorios: **uno o más fileN**. Opcionales/defaults: **ninguno**. Se apoya en `FileManager::readFile`. Valida N positivo canónico, duplicados, valor y ruta absoluta; ordena numéricamente y separa contenidos con salto de línea.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getFiles(command)` recibe parámetros ya validados, convierte el sufijo N, ordena pares N/ruta y devuelve vector de rutas. No lee contenido ni actualiza atime; execute llama después a FileManager en ese orden.

## backend/include/commands/LoginCommand.hpp

Declara la clase `LoginCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando LOGIN al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/LoginCommand.cpp`.

## backend/src/commands/LoginCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de LOGIN.

Obligatorios: **user, pass, id**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::login`. No normaliza usuario ni contraseña. Impide iniciar una segunda sesión.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/LogoutCommand.hpp

Declara la clase `LogoutCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando LOGOUT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/LogoutCommand.cpp`.

## backend/src/commands/LogoutCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de LOGOUT.

Obligatorios: **ninguno**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::logout`. Rechaza parámetros y requiere sesión activa.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkGrpCommand.hpp

Declara la clase `MkGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkGrpCommand.cpp`.

## backend/src/commands/MkGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKGRP.

Obligatorios: **name**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::createGroup`. Exige nombre no vacío; el manager valida root, longitud y duplicados.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RmGrpCommand.hpp

Declara la clase `RmGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmGrpCommand.cpp`.

## backend/src/commands/RmGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de RMGRP.

Obligatorios: **name**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::removeGroup`. Valida parámetro obligatorio y el manager marca como eliminado un grupo activo.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkUsrCommand.hpp

Declara la clase `MkUsrCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKUSR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkUsrCommand.cpp`.

## backend/src/commands/MkUsrCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKUSR.

Obligatorios: **user, pass, grp**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::createUser`. Valida campos no vacíos; el manager comprueba root, longitud y existencia de grupo.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RmUsrCommand.hpp

Declara la clase `RmUsrCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMUSR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmUsrCommand.cpp`.

## backend/src/commands/RmUsrCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de RMUSR.

Obligatorios: **user**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::removeUser`. El manager exige root y marca el UID con 0.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/ChGrpCommand.hpp

Declara la clase `ChGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando CHGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/ChGrpCommand.cpp`.

## backend/src/commands/ChGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de CHGRP.

Obligatorios: **user, grp**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::changeUserGroup`. Valida ambos campos y el manager exige que usuario y nuevo grupo estén activos.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkFileCommand.hpp

Declara la clase `MkFileCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKFILE al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkFileCommand.cpp`.

## backend/src/commands/MkFileCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKFILE.

Obligatorios: **path**. Opcionales/defaults: **r sin valor; size=0; cont**. Se apoya en `FileManager::createFile`. Si existe cont importa un archivo físico; si no, genera el patrón numérico. Verifica el límite de 280320 bytes antes de reservar contenido. El manager solicita confirmación para un archivo existente.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`generateContent(size)` devuelve un string de size bytes repitiendo 0123456789 y vacío si size <= 0. `readExternalFile(path, content)` abre en binario, obtiene tamaño para rechazar más de 280320 bytes, vuelve al inicio y lee hasta EOF; devuelve bool y llena content. Ambas son privadas y las llama execute. -cont se interpreta en la computadora del backend, no en el árbol EXT2 ni como subida automática del navegador.

## backend/include/commands/MkDirCommand.hpp

Declara la clase `MkDirCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKDIR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkDirCommand.cpp`.

## backend/src/commands/MkDirCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de MKDIR.

Obligatorios: **path**. Opcionales/defaults: **p sin valor**. Se apoya en `FileManager::createDirectory`. Exige ruta absoluta y envía la bandera recursive al manager. Sin -p no crea padres.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RepCommand.hpp

Declara la clase `RepCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando REP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RepCommand.cpp`.

## backend/src/commands/RepCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeRaw` de main, después de las validaciones de rutas y respaldo de executeCommand; la clase hace la validación específica de REP.

Obligatorios: **name, path, id**. Opcionales/defaults: **path_file_ls para file y ls**. Se apoya en `ReportManager::generateReport`. Valida uno de diez nombres. file/ls requieren ruta interna; los otros rechazan ese parámetro.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

# Cómo funciona cada comando principal, paso a paso

## Cómo funciona MKDISK

1. El parser conserva size/path/fit/unit.
2. El comando valida y convierte unidades sin desbordamiento.
3. DiskManager impide reemplazar un archivo ya existente.
4. Crea directorios físicos faltantes y escribe exactamente el tamaño solicitado con ceros.
5. Construye MBR con cuatro entradas vacías y lo escribe en el byte 0.
6. La API cuenta un éxito y muestra la ruta.

## Cómo funciona FDISK

1. Determina operación de creación, delete o add a partir de los parámetros.
2. Lee y valida MBR y nombres existentes.
3. En creación P/E verifica entradas, teoría de particiones y espacio. En L exige extendida.
4. Escoge hueco y escribe Partition en MBR o EBR enlazado en extendida.
5. En delete limpia región y repara descriptor/enlaces. En add limita por el siguiente objeto.
6. REP MBR y DISK permiten comprobar los cambios sin variar la longitud del .mia.

## Cómo funciona MOUNT

1. Resuelve la ruta física canónica.
2. Lee MBR y busca la primaria por nombre.
3. Comprueba que no esté registrada en RAM.
4. Reutiliza letra del disco y asigna el siguiente ordinal.
5. Actualiza estado/correlativo/ID y registra MountedPartition. Si ya hay EXT2 actualiza fecha/conteo de montaje.
6. MOUNTED muestra el ID para MKFS, LOGIN y REP.

## Cómo funciona MKFS

1. Localiza ID de montaje.
2. Calcula n y verifica al menos dos inodos y rango físico.
3. Pone a cero toda la partición.
4. Escribe layout, bitmaps libres y tabla vacía.
5. Asigna raíz y users.txt con uid/gid 1, entradas . y .. y contenido inicial.
6. Persiste contadores y primeras direcciones; invalida una sesión de la partición reformateada.

## Cómo funciona LOGIN

1. Comprueba que no haya sesión activa.
2. Busca montaje por ID.
3. Lee users.txt utilizando una sesión temporal de autenticación.
4. Busca nombre exacto, compara contraseña y obtiene grupo activo.
5. Guarda user/uid/gid/partitionId en AppState.
6. Las siguientes operaciones usan esa partición hasta LOGOUT.

## Cómo funciona MKFILE

1. Valida ruta, banderas y parámetros.
2. Obtiene contenido real de cont o genera size dígitos.
3. Recorre padres y crea los faltantes solo con -r y permiso.
4. Exige escritura en padre y, al reemplazar, en archivo.
5. Si existe, vuelve a la API con pregunta pendiente y espera otra petición.
6. Con y reemplaza; con n conserva. Para uno nuevo asigna inodo, contenido y entrada.
7. Continúa el script y actualiza conteo de éxito solo si se ejecutó la creación/escritura.

## Cómo funciona MKDIR

1. Valida ruta y -p sin valor.
2. Recorre componentes desde raíz y comprueba que sean carpetas.
3. Sin -p falla al faltar un padre.
4. Comprueba permiso de escritura en el padre de cada carpeta nueva.
5. Crea inodo con 664 y bloque con . y ..; añade entrada en padre.
6. Actualiza bitmap, libres y mtime del padre.

## Cómo funciona CAT

1. Valida cada parámetro fileN, sin duplicados ni valores vacíos.
2. Ordena N numéricamente.
3. Resuelve el archivo y comprueba lectura U/G/O o root.
4. Sigue directos e indirectos y recupera exactamente i_s bytes.
5. Actualiza atime.
6. Concatena los archivos con saltos de línea y devuelve texto sin prefijos agregados al contenido.

## Cómo funciona REP

1. Valida nombre, ID, salida y ruta interna cuando corresponde.
2. Resuelve disco/partición montada.
3. Lee estructuras reales y construye DOT o texto según reporte.
4. Crea padres y ejecuta dot con argumentos separados para los formatos gráficos.
5. Registra la ruta generada y entrega URL del endpoint de reportes.
6. Explorer carga la lista y muestra el reporte seleccionado dentro de un iframe aislado.

# Cómo funciona el frontend

## frontend/src/App.tsx

Componente raíz sin props: App() devuelve JSX. Usa componentes conservados y Explorer. Estados reales: `revision` number provoca refresco del explorador; `fileName`, `commandText`, `answer` string; `messages` string[]; `isOnline`, `busy`, `pending` boolean; `stats` ExecutionStats|null. main.tsx lo monta en StrictMode.

`verifyServer(): Promise<void>` dentro de useEffect llama checkServerStatus y actualiza isOnline al montar. `handleFileSelected(event: React.ChangeEvent<HTMLInputElement>): Promise<void>` obtiene selectedFile File, valida selectedFileName string terminado en .smia/.txt, lee content string y actualiza editor/nombre; no ejecuta al cargar. Errores de archivo aparecen en messages.

`handleAnalyze(): Promise<void>` evita doble envío y entrada vacía, usa checkServerStatus, llama analyzeCommands con answer si pending o commandText si no, conserva mensajes anteriores al responder, actualiza stats/pending, limpia answer, aumenta revision y libera busy en finally. La respuesta de comando fallido sigue llegando como AnalysisResponse y no apaga el indicador online. `handleClear(): void` limpia editor/nombre/mensajes/stats. Submit del formulario impide navegación y llama handleAnalyze. Los eventos no escriben estructuras binarias directamente.

## frontend/src/services/commandAPI.ts

Responsabilidad: transporte HTTP común y token por navegador. `BACKEND_URL` string usa VITE_API_URL, o localhost solo en DEV, o mismo origen en producción. `token` string se inicializa de sessionStorage; `initialization` Promise<SessionInfo>|null evita varias inicializaciones simultáneas de apiRequest.

| Función | Parámetros / retorno | Qué hace y qué pasa si falla |
|---|---|---|
| getSession | ninguno → Promise<SessionInfo> | GET /api/session con token; recibe/genera identidad de navegador, actualiza token/sessionStorage; Error si HTTP falla. |
| apiRequest | path string, body object opcional → Promise<T> | prepara token si falta, GET sin body o POST con body; Content-Type y X-MIA-Session; parsea JSON, lanza Error con message/messages ante HTTP fallido. |
| checkServerStatus | ninguno → Promise<boolean> | GET /api/health; combina response.ok y data.success; red/JSON fallido devuelve false. |
| analyzeCommands | input string → Promise<AnalysisResponse> | inicializa token, POST /api/analyze; conserva JSON con stats aunque HTTP sea 400; si falta stats lanza Error. |
| loadReport | path string → Promise<string> | GET autorizado de reporte; convierte Blob a object URL para iframe; Error si HTTP falla. Explorer revoca URL al reemplazar/desmontar. |

Interfaces reales del mismo archivo: SessionInfo (success, token, active, user, partitionId, uid, gid); PartitionInfo (name, size, fit, type, id, mounted, filesystem); DiskInfo (id, name, size, fit, date, partitions); FileInfo (name, path, type, size, uid, gid, permissions); JournalInfo (operation, path, content, date); ReportInfo (name, url). Los importan Explorer y los helpers; no existen en disco.

## frontend/src/types/analysis.ts

Define ExecutionStats (tres contadores), AnalysisResponse (success, messages, stats, pendingConfirmation) y HealthResponse (success, message). No tiene funciones ni efectos en ejecución. commandAPI, App y OutputPanel usan estos contratos TypeScript. ReportInfo está en commandAPI.ts.

## frontend/src/components/OutputPanel.tsx

OutputPanel recibe messages y stats (o null). Devuelve un panel con `<pre>`; une mensajes mediante salto de línea, conservando comentarios y líneas vacías. Cuando stats existe construye un resumen con los tres contadores al final. No interpreta HTML enviado por comandos: React representa el contenido como texto. No añade un símbolo > delante de CAT. App lo llama después de cada respuesta. Pregunta: ¿por qué pre? Porque un párrafo HTML normal colapsa espacios y saltos.

## frontend/src/components/CommandEditor.tsx

CommandEditor recibe value y onChange. Devuelve textarea controlado: cada cambio pasa su texto a App. Muestra número de líneas calculado con split, en lugar del número de caracteres anterior. No hace tokenización; el backend es quien valida. Lo llama App.

## frontend/src/components/FileChooser.tsx

FileChooser recibe fileName y onFileSelect y devuelve un input de archivos con accept=.txt,text/plain,.smia y su etiqueta visual. El callback real está en App. El texto de ayuda coincide con la extensión del PDF. No conoce rutas del disco simulado ni importa -cont.

## frontend/src/components/Buttons.tsx

Buttons recibe onAnalyze, onClear y disabled. Devuelve dos botones que llaman a los callbacks; no modifica archivos por sí solo. App decide cuándo deshabilitarlos. Los comentarios de props describen el evento, no lógica del analizador.

## frontend/src/components/ServerStatus.tsx

ServerStatus recibe isOnline y devuelve indicador Online/Offline con clases CSS. No hace fetch; la comprobación corresponde a App/commandAPI. Las clases se alinearon con app.css para que el estado tenga su color.

## frontend/src/components/Header.tsx

Header no recibe parámetros y devuelve el encabezado del sistema y carnet. No contiene estado ni funciones de negocio. App lo incluye al inicio.

## frontend/src/main.tsx

Busca el elemento root de index.html y monta App dentro de StrictMode mediante ReactDOM. Es el punto de entrada de Vite. No ejecuta comandos. StrictMode ayuda a detectar efectos incorrectos durante desarrollo.

## frontend/src/styles/app.css y frontend/src/index.css

app.css define colores, paneles, botones, editor, salida, estado del servidor y reglas responsivas. Se conservó el diseño. La salida usa white-space:pre-wrap y overflow-wrap para mostrar saltos y permitir lectura de líneas largas; tiene scroll para CAT grande. index.css es la entrada global mínima, importada por main.tsx. No contienen clases TypeScript ni funciones con retorno.

## frontend/index.html y frontend/public/

index.html aporta el contenedor root y carga src/main.tsx. public/favicon.svg e icons.svg son activos estáticos del frontend original; no participan en EXT2. El título de plantilla no cambia la ejecución del sistema. Los archivos SVG contienen dibujos, no estructuras del disco.

## frontend/package.json y frontend/package-lock.json

package.json define scripts dev, build, lint y preview, además de React, TypeScript y Vite. package-lock.json fija versiones resueltas para npm ci. No contienen funciones productivas. Build ejecuta TypeScript antes de generar dist; lint usa oxlint. No se cambiaron dependencias.

## frontend/vite.config.ts

Exporta defineConfig con el plugin React. Vite lo lee al arrancar o compilar. El puerto se puede pasar por CLI y VITE_API_URL llega a la API por import.meta.env. No agrega rutas del sistema de archivos del backend.

## frontend/tsconfig.json, tsconfig.app.json y tsconfig.node.json

La configuración raíz referencia la app y el archivo Vite. app habilita JSX, tipos de DOM y comprobación de variables/parámetros sin uso; node cubre el entorno de configuración de Vite. No producen JavaScript por sí solos en la comprobación; Vite empaqueta la aplicación.

## frontend/.gitignore

Ignora dependencias, dist y archivos locales propios del frontend. No participa en los comandos ni contiene funciones de ejecución.

# Build, pruebas y archivos de apoyo

## backend/CMakeLists.txt

Declara proyecto, C++17 obligatorio, exportación de compile_commands.json y dependencia Crow v1.2.1 mediante FetchContent. Enumera explícitamente fuentes, incluye backend/include y enlaza Crow::Crow. CMake lo evalúa para generar el build; no forma parte del formato binario del disco. No se agregaron frameworks ni se cambió el lenguaje.

## README.md y frontend/README.md

README de raíz explica cómo compilar/iniciar el proyecto, cambiar puertos, probar y ubicar documentación. El README del frontend conserva información de su plantilla y se complementa con la raíz. Ninguno es el manual oficial final en PDF.

## .gitignore y compile_commands.json

.gitignore excluye binarios .mia, build, node_modules, dist y reportes temporales; los scripts y documentos sí deben versionarse. compile_commands.json es un enlace al archivo generado por CMake y ayuda al editor a resolver includes; no es fuente del proyecto. .vscode contiene configuración local de edición, no lógica productiva.

## tests/audit.py

Prueba externa en Python que inicia su propio backend, envía comandos HTTP y lee el .mia con ctypes. No sustituye el backend C++. Las clases Partition/MBR/EBR/SB/Inode/Entry representan el ABI solo en el lector de prueba. `check` registra o falla; `read` obtiene una estructura; `api/ok/bad` verifican respuestas; `pattern/cat` comparan contenido; `data_blocks` sigue indirectos; `inode_at` resuelve rutas; `integrity` confronta bitmaps, referencias, conteos y tamaño; `main` organiza casos. El bloque final inicia servidor, espera health, ejecuta y termina el proceso que creó. Escribe RESULTADOS.json incluso si falla.

## tests/prepare_scripts.py y tests/*.smia

prepare_scripts recibe --base, crea el contenido real para -cont y copia scripts a generated reemplazando el prefijo de rutas. No elimina discos existentes. Los ocho .smia cubren discos, EXT2, usuarios, archivos, permisos, indirectos, errores y demostración conjunta. Se ejecutan en orden en una sesión nueva; demo_final ya reúne los anteriores. README de tests explica casos deliberadamente fallidos.

## tests/RESULTADOS.json y tests/evidence/

RESULTADOS.json contiene status, número de comprobaciones, ABI y carpeta temporal de la última ejecución. evidence conserva reportes reales en SVG/texto/DOT. Son evidencia reproducible, no otra implementación. Las comprobaciones por bloque cuentan individualmente; no deben presentarse como miles de escenarios diferentes.

## Documentos del proyecto

README.md explica instalación y ejecución; frontend/README.md explica la interfaz; PRUEBAS_PROYECTO2.md registra casos y resultados; CAMBIOS_PROYECTO2.md lista cambios/decisiones; PENDIENTE_AWS.md contiene solo despliegue posterior. EXPLICACION_CODIGO.md es el material histórico del Proyecto 1; este manual describe la implementación actual. Los dos manuales PDF preexistentes se conservan como referencia histórica.

# Funciones auxiliares conservadas, documentadas individualmente

## Auxiliar de backend/src/managers/FileManager.cpp

`storageBlocks(int dataBlocks): int` calcula el costo de un archivo continuo, incluidos datos y bloques de apuntadores. `total` int comienza en dataBlocks; si hay más de 12 datos agrega un apuntador simple; con más de 28, `doubleBlocks` int acota a 256 y suma raíz doble más ceil(datos/16); con más de 284, `tripleBlocks` int suma raíz triple, ceil(datos/256) y ceil(datos/16). No lee/escribe disco. createFileInode/writeFileContent/findFreeDirectoryEntry lo usan para validar capacidad antes de reservar. Evita que falte espacio para los apuntadores aunque alcancen los bloques de contenido.

## Auxiliares de backend/src/managers/ReportManager.cpp

- `escapeGraphvizText(const string& text): string` recorre `character` char y agrega a `result` string escapes de &, <, >, comillas y salto LF; ignora CR. Evita romper tablas HTML de DOT. Lo llaman los generadores de reportes con contenido/nombres. No modifica input ni disco.
- `percentageText(double value): string` usa `stream` stringstream con fixed y setprecision(2); genera un porcentaje legible. Lo usa DISK al representar regiones. No altera tamaños físicos.
- `dateOnly(time_t value): string` devuelve '-' para fecha 0 o localtime fallido; `dateInfo` tm* contiene fecha local del host y `stream` stringstream construye dd/mm/aaaa. No escribe fechas del inodo, solo las muestra en reportes.
- `timeOnly(time_t value): string` sigue las mismas validaciones y devuelve hh:mm:ss con relleno cero; dateInfo apunta a datos de localtime. Se usa para columnas de hora de reportes y tampoco altera el .mia.

Las funciones locales/lambdas de reportTree (`addInode`, `addDataBlock`, `addPointerBlock`) se crean dentro de ese método existente: reciben índices/nivel/tipo y devuelven bool, leen estructuras con FileSystemManager, agregan DOT a dotContent y controlan printedInodes/printedBlocks para no repetir ni ciclar. El apartado reportTree explica su flujo de bloques e inodos.

# Ampliaciones de estructuras y managers

## EXT3: estructuras, tamaños y offsets exactos

`backend/include/structures/Ext2Structures.hpp` conserva SuperBlock, Inode, Content, FolderBlock, FileBlock y PointerBlock. Agrega `JOURNAL_CAPACITY = 50`, Information y Journal. No altera sizeof de las estructuras del Proyecto 1.

| Estructura | Tamaño en el ABI Linux 64 bits verificado | Ubicación y significado |
|---|---:|---|
| Partition | 40 | Una de cuatro entradas dentro de MBR; status/type/fit char, start/s/correlative int, name[16], id[4]. El padding nativo forma parte del archivo. |
| MBR | 184 | Byte 0; tamaño del disco int, fecha time_t, firma int, fit char, cuatro Partition. |
| EBR | 32 | En la extendida; mount/fit char, start/s/next int, name[16]. part_start apunta a datos después del EBR. part_next apunta al siguiente EBR o -1. |
| SuperBlock | 80 | Al principio de la primaria; tipo/counts/sizes/offsets int y fechas time_t. Validación mediante magic 0xEF53. |
| Inode | 104 | Tabla de inodos, s_inode_start + índice × s_inode_s. UID/GID/tamaño int, tres fechas time_t, i_block[15] int, type char y perm[3]. |
| Content | 16 | b_name[12] char y b_inodo int, cuatro por FolderBlock. |
| FolderBlock | 64 | Cuatro entradas; . y .. en el primero. |
| FileBlock | 64 | Contenido bruto; i_s determina bytes válidos del último bloque. |
| PointerBlock | 64 | Dieciséis índices int, inicializados a -1. |
| Information | 112 | operation[10], path[32], content[64] char; date float de 4 bytes; padding de alineación. |
| Journal | 5604 | j_count int y j_content[50] Information. Después del SuperBlock de EXT3. |

Estos tamaños incluyen alineación nativa; no son un formato portátil entre ABI diferentes. BinaryUtils::readStruct/writeStruct usan sizeof(T) y fstream con offsets absolutos sin truncar el disco. El lector ctypes comprueba los mismos tamaños de forma independiente. No se usa packing para cambiar el Proyecto 1.

Sea `S` el start de la primaria, `P` su tamaño, `J=0` en EXT2 o `sizeof(Journal)=5604` en EXT3:

```text
n = floor((P - sizeof(SuperBlock) - J) / (4 + sizeof(Inode) + 3*64))
s_bm_inode_start = S + sizeof(SuperBlock) + J
s_bm_block_start = s_bm_inode_start + n
s_inode_start = s_bm_block_start + 3*n
s_block_start = s_inode_start + n*sizeof(Inode)
fin = s_block_start + 3*n*64 <= S + P
```

La región sobrante por división entera no se asigna. El journal no comparte el bitmap de bloques de datos. Los bitmaps normales usan ASCII '0'/'1'; LOSS escribe NUL real, y no '0' ASCII. Después de LOSS los contadores del superbloque se conservan como información anterior; no se presentan como un sistema operativo válido. Los reportes permiten ver los bytes NUL y el comprobante antes/después.

Information no tiene funciones. Journal comienza con count 0. Cada operación exitosa se guarda en j_content[count] y luego aumenta count. El código lee/escribe Journal completo con BinaryUtils. El área nunca se rota ni sobrescribe registros viejos; count 50 bloquea nuevas operaciones modificadoras. No se registran CAT, FIND, consultas GUI, LOGIN/LOGOUT, fallos ni cancelaciones.

## FileSystemManager.hpp y FileSystemManager.cpp: métodos nuevos

Los encabezados declaran los contratos; el .cpp contiene la implementación y usa DiskManager, BinaryUtils y las estructuras compartidas. Los métodos no mantienen un filesystem en memoria entre solicitudes.

| Método, parámetros y retorno | Lógica / validación / llamadas / efecto / fallo |
|---|---|
| formatExt3(const string& path, int start, int size, string& message) → bool | Delegación a format con type 3. Error si no puede completar formato. |
| format(const string& path, int partitionStart, int partitionSize, int type, string& message) → bool | Valida límites físicos y n>=2. calculateInodes descuenta Journal; clearPartition limpia primaria; createSuperBlock genera offsets; journalSize desplaza bitmaps/tablas/bloques. Inicia Journal, bitmaps, inodos, bloques y createRoot. Persiste SuperBlock. Cualquier escritura fallida devuelve false y executeCommand restaura el backup. |
| readJournal(const string& path, int start, Journal& journal) → bool | readSuperBlock exige magic y tipo 3; readStruct en start+sizeof(SuperBlock); valida count 0..50. No cambia datos. |
| appendJournal(const string& path, int start, const string& operation, const string& route, const string& content, string& message) → bool | readJournal, capacidad, llena Information con copyToFixedChar y time(nullptr) convertido a float, writeStruct del Journal. Si falla no confirma la transacción y se restaura disco. |
| simulateLoss(const string& path, int start, string& message) → bool | Valida EXT3 y journal; end es el final del área de bloques. clearSpace pone NUL desde s_bm_inode_start hasta end. No escribe superbloque ni journal. Retorna mensaje y main invalida sesiones. |

Variables de format: `inodeCount` int es n; `journalSize` int 0/5604; `superBlock` estructura que recibe contadores/offsets; `journal` Journal vacío. De appendJournal: `journal` conserva entradas previas; `entry` Information& refiere a la posición nueva; `operation`, `route`, `content` no exceden físicamente los arrays al copiar. De LOSS: `sb`, `journal` validan áreas; `end` int acota limpieza. formatExt2 conserva su firma y delega a format tipo 2.

## FileManager.hpp y FileManager.cpp: operaciones nuevas

Todas actúan sobre la partición de Session mediante MountManager y FileSystemManager. main valida las rutas y realiza respaldo; los permisos U/G/O existentes se mantienen. La excepción root se basa en UID 1. Directorios/archivos nuevos reciben permisos 664 conservados de createSingleDirectory/createFileInode, salvo raíz 777.

| Método real | Parámetros → retorno | Lógica, validaciones, efectos y errores |
|---|---|---|
| changeEntry | path/name string const&, erase bool, state AppState&, message string& → bool | Resuelve padre con findInodeByPath, lee sb y bloques vía getDataBlock, busca oldName. erase pone Content vacío; si no, cambia solo nombre. writeFolderBlock y writeInode actualizan referencia/mtime del padre. Sin referencia o I/O falla devuelve false. |
| collectTree | path string const&, writing bool, state AppState const&, paths vector<string>&, visited vector<int>&, message string& → bool | Prevalida escritura/lectura, controla inodos repetidos. Para REMOVE obtiene entradas con lector temporal root para no exigir lectura además de escritura; excluye . y .., recurre y agrega padre al final. No cambia disco. Error de permisos/ciclo aborta todo. |
| remove | path string const&, state AppState&, message string& → bool | Protege / y /users.txt, collectTree valida todo antes de tocar. Recorre paths en postorden; changeEntry retira referencia, freeFileBlocks libera datos/indirectos, freeInode libera inodo. Bitmap/conteos/primeros libres se actualizan por los helpers existentes. Error restaura transacción completa. |
| rename | path/name string const&, state AppState&, message string& → bool | Comprueba nombre 1..12 bytes, sin /, . ni ..; protege raíz/users.txt; exige origen y escritura; valida target no existente. changeEntry cambia solo referencia; mantiene inodo y contenido. |
| copyTree | source/target string const&, state AppState&, skipped int&, visited vector<int>&, message string& → bool | Resuelve inodo; sin lectura incrementa skipped y omite rama. Con lectura controla ciclo. Archivo: readFile+createFile. Carpeta: getDirectoryContent+createDirectory; recurre por entradas reales. Error distinto de omisión aborta y main restaura copia parcial. |
| copy | path/destination string const&, state AppState&, message string& → bool | Valida origen, carpeta destino escribible, no origen/descendiente, no nombre duplicado. Construye newPath, llama copyTree, exige al menos copia de raíz de origen. Reporta skipped. Falta de espacio no deja reservas ni contenido parcial por respaldo externo. |
| move | path/destination string const&, state AppState&, message string& → bool | Valida existencia, escritura de origen/destino, carpeta destino, rutas protegidas/ciclo/nombre duplicado. addDirectoryEntry enlaza el mismo sourceIndex; changeEntry retira vínculo anterior. Para carpeta cambia .. en primer bloque; actualiza mtime. No copia contenido ni cambia propietario. |
| matchesPattern | name/pattern string const& → bool | Tabla vector<vector<bool>>: matches[0][0]=true. ? o literal usan diagonal; * requiere primer carácter diagonal o extiende coincidencia anterior. Usa espacio O(longitud patrón × longitud nombre); patrón HTTP limitado a 256. |
| findTree | path/pattern string const&, state AppState const&, visited vector<int>&, matches vector<string>&, message string& → bool | findInodeByPath, lectura, control de repetidos; compara nombre y agrega ruta. Si carpeta recorre entries omitiendo . y ... Omite ramas sin lectura; no modifica contenido. |
| find | path/pattern string const&, state AppState const&, message string& → bool | Requiere carpeta inicial y lectura; llama findTree, genera encabezado y rutas completas, o Sin coincidencias. Error de ruta/permiso inicial/ciclo se informa. |

Variables relevantes: `index`, `parentIndex`, `sourceIndex`, `targetIndex` int son índices, nunca offsets; `inode`, `source`, `target` Inode contienen metadatos; `mounted` MountedPartition aporta path/start; `sb` SuperBlock aporta zonas; `block`, `logical` int corresponden al bloque físico/ordinal; `folder` FolderBlock temporal; `entry` Content& modifica una entrada; `parent`, `oldName`, `target`, `newPath` string construyen referencias. En recursión `visited` evita ciclos, `paths` contiene postorden, `entries` vector<Content> contiene hijos, `skipped` cuenta ramas sin lectura y `matches` reúne rutas. La tabla de patrón usa i/j size_t; cada celda se modifica una sola vez.

findInodeByPath conserva su trabajo de resolución y ahora exige ruta absoluta, tamaño <=4096 y ausencia de . y ..; verifica bitmap raíz '1' para impedir uso después de LOSS. Los campos heredados y helpers de bloques indirectos se documentan en los apartados anteriores.

## MountManager.hpp y MountManager.cpp: UNMOUNT

`unmountPartition(const string& id, AppState& state, string& message): bool` recorre mountedPartitions por i size_t, captura partition MountedPartition, lee mbr con DiskManager y encuentra index por nombre. Cambia status '0', correlative 0 e id vacío; writeMBR. Si hay formato, lee sb, actualiza s_umtime y writeSuperBlock. Borra la entrada en memoria; resetea Session si coincide. ID ausente devuelve error. El wrapper de main restaura en fallo y, en éxito, invalidatePartition limpia identidades de otros navegadores.

`getNextCorrelative` busca `number` int de 1 a 9 y `used` bool entre montajes activos. Reutiliza el primer número libre. Evita agotar identificadores al desmontar/montar una partición repetidas veces. Conserva carnet de AppState, sufijo 47 y letra de disco; el primer montaje es 471A, no 341A de los ejemplos genéricos del PDF.

## DiskManager.hpp y DiskManager.cpp: ADD y DELETE

`deletePartition`, `deletePrimaryOrExtended`, `deleteLogical` reciben `bool full`. full true limpia con clearSpace; false solo modifica tabla/cadena. Ambos impiden tocar particiones montadas. Un DELETE de extendida elimina la referencia contenedora y todas sus lógicas dejan de estar accesibles; FULL limpia también sus EBR/datos. Al borrar la primera lógica se conserva un EBR inicial vacío con su next; al borrar otra se conecta el EBR anterior al siguiente. FAST conserva datos y, para EBR intermedios, los bytes desconectados. Reutilizar espacio puede reemplazarlos después.

`resizePrimaryOrExtended` mantiene validaciones de hueco contiguo, fin de disco, EBR/lógicas y tamaño positivo. Agrega `sb` SuperBlock y `fsEnd` long long para rechazar recortes de estructuras reservadas en primarias formateadas. El delta no aumenta el tamaño del contenedor. `resizeLogical` respeta siguiente EBR o final de extendida; solo administra espacio porque las lógicas no son montables en este proyecto.

La confirmación y respaldo corresponden al wrapper HTTP. La validación inicial usa `FDiskCommand::execute(command, validateOnly=true)`; este argumento nuevo devuelve después de validar DELETE y antes de modificar. Ejecutar confirmado llama executeCommand(command,true), respalda y deriva a executeRaw y al manager.

## StringUtils.hpp y StringUtils.cpp: texto compatible

`isText(const string& content): bool` recorre bytes UTF-8. Permite ASCII imprimible, tab, CR y LF; rechaza NUL/otros controles/DEL. Para multibyte valida `following` int, `code` unsigned int, `minimum` unsigned int y bytes de continuación; rechaza secuencias cortas, codificaciones sobrelargas, sustitutos Unicode y valores >0x10FFFF. `position` size_t avanza por cada byte. No altera content. Lo usa el visor HTTP y la representación del journal; error de compatibilidad devuelve 415. El journal binario conserva los campos del PDF; si sus bytes no pueden representarse como texto se muestra [contenido binario].

# Explorer: archivo nuevo de React

## frontend/src/components/Explorer.tsx

`Explorer({revision,onChange}: Props)` devuelve JSX. Props: revision number representa nueva ejecución; onChange () => void pide refresco al padre. Importa hooks React y los helpers/interfaces de commandAPI. App lo monta encima del editor.

| Estado / derivado | Tipo y valor | Actualización y uso |
|---|---|---|
| session | SessionInfo o null | getSession en refresh; identifica login/partición y decide formulario vs logout. |
| disks | DiskInfo[] | GET /api/disks; tarjetas y tabla de particiones. |
| diskId, partitionId | string | selección de disco/ID. El login define partitionId de sesión; seleccionar otra partición no cambia la identidad activa del explorador. |
| user, password | string | inputs; password se limpia tras éxito. |
| tab | string files/journal/reports | botones de navegación. |
| path | string inicialmente / | carpeta seleccionada; Subir quita el último segmento. |
| entries | FileInfo[] | respuesta de directorio actual; se limpia si falla o termina sesión. |
| content | {name:string,text:string} o null | resultado al abrir archivo; cambia ruta/tab/logout lo limpia. |
| journal | JournalInfo[] | GET journal según partitionId; reemplazo al actualizar selección. |
| reports | ReportInfo[] | lista privada de ese token. |
| reportUrl | string | Blob object URL; se revoca al reemplazar/desmontar. |
| error, busy | string / boolean | error visible role=alert y estado de formulario en progreso. |
| selectedDisk | DiskInfo o undefined | búsqueda de diskId en disks para mostrar tabla. |
| mounted | PartitionInfo[] | discos.flatMap(partitions), filtrado por mounted. Opciones del formulario/journal. |

Funciones propias:

- `refresh(): Promise<void>` en useEffect: getSession y apiRequest('/api/disks'), carga datos; limpia archivos/path si no hay login; conserva partitionId de identidad activa. `cancelled` bool impide actualizar un componente desmontado. Dependencia revision.
- `refreshView(): Promise<void>` en segundo useEffect: limpia error/content, obtiene entries si files+sesión, journal si tab journal+ID, reports si tab reports. Usa encodeURIComponent(path/id). Captura Error y limpia entradas obsoletas. Dependencias tab, path, partitionId, session.active, revision. La limpieza marca cancelled.
- `login(event: React.FormEvent): Promise<void>`: preventDefault, busy=true, POST login con id/user/password; éxito borra password, reinicia path y llama onChange. Error muestra texto; finally busy=false.
- `logout(): Promise<void>`: POST logout, limpia content/entries y llama onChange; error se muestra; finally libera busy.
- `openFile(file: FileInfo): Promise<void>`: carpeta cambia path; archivo obtiene /api/files/content y guarda name/text. Error no deja contenido previo.
- `openReport(report: ReportInfo): Promise<void>`: loadReport con token, guarda URL Blob; error se muestra. Un useEffect independiente revoca la URL anterior.

Los eventos de tarjetas/selects solo cambian selecciones. No hay creación/eliminación/copia gráfica. React representa contenidos/journal/errors como texto; no usa dangerouslySetInnerHTML. La tabla desborda horizontalmente dentro de table-scroll; la prueba móvil verifica que la página no desborde. iframe sandbox sin permisos muestra SVG/texto sin ejecutar scripts del reporte.

# Contratos completos de endpoints

Se conserva `/api/analyze` para comandos y scripts; no se duplican `/api/commands` o `/api/scripts`. Se conserva `/api/health` como salud. Las particiones viajan embebidas en cada disco, evitando un endpoint duplicado por ID físico. Todos los endpoints con estado, salvo bootstrap `/api/session`, requieren el token enviado en X-MIA-Session.

| Método / ruta | Entrada / JSON de salida | Funciones C++ / consumidor React / errores |
|---|---|---|
| GET /api/health | sin datos → success y message | handler health; checkServerStatus en App. Disponible sin token. |
| GET /api/session | token opcional en cabecera → success, token, active, user, partitionId, uid, gid | newToken/map browsers. getSession en Explorer/API; un token perdido/desconocido genera navegador nuevo sin login. |
| POST /api/analyze | {"input":"mkdir -path=/a"}, o script multilínea/y/n → success, messages[], stats{successfulCommands,lexicalErrors,syntaxErrors}, pendingConfirmation, reports[{name,url}] | useBrowser/processInput/executeCommand/executeRaw/managers/saveBrowser. analyzeCommands en App. 400 comandos/JSON, 401 token, 413 tamaño >1 MiB. |
| POST /api/login | {"id":"471A","user":"root","password":"123"} → success, message, messages[] | UserManager::login. Explorer.login. 400 tipo/campos, 401 credenciales/token o login ya activo. |
| POST /api/logout | {} → success, message, messages[] | UserManager::logout y limpieza pending. Explorer.logout. 401 token; 400 si no hay login. |
| GET /api/disks | token → success, disks[] | enumeración segura del directorio, DiskManager::readMBR y BinaryUtils para EBR, FileSystemManager::readSuperBlock. Explorer.refresh. 401 token, 500 error de filesystem. Disk: id relativo, name, size bytes, fit, date Unix, partitions[]. Partition: name,size,fit,type,id,mounted,filesystem 0/2/3. |
| GET /api/files?path=/docs | ruta URL codificada → success, path, entries[] | FileManager::getDirectoryContent, FileSystemManager::readInode; Explorer.refreshView. 401 falta login/token; 403 ruta o permiso. Cada entrada: name,path,type directory/file,size,uid,gid,permissions string octal. |
| GET /api/files/content?path=/docs/a.txt | ruta codificada → success, content string | FileManager::readFile y StringUtils::isText; Explorer.openFile. 401 login/token, 403 lectura/ruta, 415 binario/UTF-8 inválido. readFile actualiza atime como metadato; no genera journal de modificación. |
| GET /api/journaling/471A | ID montado → success, capacity=50, entries[] | MountManager::getMountedPartition y FileSystemManager::readJournal. Explorer.refreshView. 401 token, 404 montaje, 400 EXT2/formato inválido. Entry: operation,path,content,date Unix; GUI muestra fecha/hora. Consultable después de LOSS sin login. |
| GET /api/reports | token → success, reports[{name,url}] | browsers[activeBrowser].reports y reportPaths. Explorer.refreshView. 401 token. Lista de reportes creados desde ese navegador; RAM se reinicia al detener Crow. |
| GET /api/reports/N | token → bytes del archivo con Content-Type adecuado | lectura ifstream desde reportPaths, índice permitido por BrowserState.reports. loadReport en Explorer. 401 reporte de otro token, 404 índice/archivo. nosniff y CSP sandbox. No recibe ruta libre. |

Ejemplo de error de autenticación: `{"success":false,"message":"Debe iniciar sesion.","messages":["Debe iniciar sesion."]}` con HTTP 401. Los endpoints de colección agregan su carga específica; los comandos conservan messages/stats para compatibilidad con el Proyecto 1.

CORS permite Content-Type y X-MIA-Session, GET/POST/OPTIONS, y el origen configurable. El backend limita rutas físicas canónicas por directorio antes de ejecutar comandos. El explorador no recibe rutas físicas ni puede cambiar uid/gid. Las rutas relativas físicas se convierten en absolutas dentro de su base; los nombres virtuales se resuelven con inodos dentro de una sola primaria.

# Referencia de comandos y flujos completos

Los nombres y claves se normalizan a minúsculas; rutas/usuario/password preservan caracteres. Todos rechazan parámetros desconocidos/repetidos y valores faltantes, excepto opciones que el PDF exige ignorar por selección de primera operación FDISK. Se mantienen las validaciones específicas de las clases conservadas.

| Comando y ejemplo de sintaxis | Valores predeterminados / permisos / estructuras / errores / journal |
|---|---|
| mkdisk -size=12 -path=disco.mia [-unit=M] [-fit=FF] | unit M, fit FF; manager DiskManager crea ceros y MBR; entero positivo, .mia, no existente y ruta segura; sin login/journal. |
| rmdisk -path=disco.mia | DiskManager elimina contenedor; despachador quita montajes/invalida sesiones; ausencia/extensión/ruta inválida falla. No modifica otro disco ni journal. |
| fdisk -size=8 -unit=M -path=disco.mia -name=p [-type=P] [-fit=WF] | unit K, type P, fit WF por defecto. MBR/EBR; nombres únicos, cuatro entradas, una extendida, espacio/hueco suficiente. Sin journal. |
| fdisk -add=1 -unit=M -path=disco.mia -name=p | delta con signo, unit K por defecto; DiskManager.resizePartition; no montada, tamaño positivo, límites y estructuras seguras. Cambia s de Partition/EBR; no aumenta archivo .mia. |
| fdisk -delete=fast -path=disco.mia -name=p | fast/full explícito; validación, pregunta y respaldo antes de DiskManager.deletePartition. No permite montada. FAST retira referencias; FULL además NUL en área. |
| mount -path=disco.mia -name=p | MountManager; solo P, no duplicada; cambia MBR status/correlative/id, SuperBlock fecha/count si válido; AppState registro. Primera ID 471A. |
| mounted | MountedCommand enumera AppState.mountedPartitions; sin parámetros; sin cambios ni journal. |
| unmount -id=471A | MountManager.unmountPartition; ID existente; MBR status 0/correlative 0/id vacío, fecha unmount de sb. Invalida identidades; sin journal. |
| mkfs -id=471A [-type=full] [-fs=2fs] | fs 2fs por defecto; acepta 3fs. FileSystemManager.format; n>=2. Limpia primaria, crea sb/bitmaps/inodos/bloques/raíz/users.txt y Journal para 3. Borra datos y bitácora previa, invalida login. |
| login -id=471A -user=root -pass=123 | UserManager valida montaje/users.txt/usuario activo/password exacto/grupo. Cambia Session del navegador. No modifica contenido/journal. |
| logout | UserManager limpia identidad; requiere login; sin parámetros. Login siguiente puede ser otra primaria. No desmonta ni registra journal. |
| mkgrp -name=equipo | Solo root; <=10 caracteres y no separadores, no duplicado; actualiza users.txt (inodo/bloques/sb). Journal mkgrp path /users.txt contenido name. |
| rmgrp -name=equipo | Solo root y grupo activo, raíz protegido; marca ID 0 en users.txt. Journal rmgrp. |
| mkusr -user=ana -pass=abc -grp=equipo | Solo root, grupo existente, campos <=10, no coma/saltos, no duplicado; inserta registro con UID monotónico. Journal mkusr sin password; contenido user. |
| rmusr -user=ana | Solo root, usuario activo/raíz protegido; marca ID 0. Journal rmusr. |
| chgrp -user=ana -grp=equipo | Conservado de P1; root y ambos registros activos; modifica registro de usuario. Journal chgrp. No cambia permisos/inodos de todos sus archivos. |
| mkdir -path=/home/docs [-p] | Sesión, escritura en padre; sin -p exige padres existentes. Inodo/bloque de directorio, . / .., entrada en padre, bitmaps/sb. Permiso nuevo 664. Journal mkdir solo por éxito. |
| mkfile -path=/home/a.txt [-r] [-size=0] [-cont=entrada.txt] | Sesión; size 0 por defecto, patrón 0123456789; cont tiene prioridad y ruta física en inputRoot. Máximo 280320 bytes, nombre <=12; escritura. Sobrescritura exige y/n. Inodo/datos/indirectos/bitmaps/sb; journal contenido limitado a 64 bytes. |
| cat -file1=/home/a.txt [-file2=/otro.txt] | Sesión y lectura de cada archivo; orden numérico de fileN; mensajes contienen contenido. readFile actualiza atime; no journal. |
| remove -path=/home/docs | Sesión, escritura sobre todos los elementos; raíz/users.txt protegidos; preflight y postorden sin cambios parciales. Libera inodos/datos/indirectos; modifica entrada de padre/bitmaps/sb. Journal remove. |
| rename -path=/home/a.txt -name=b.txt | Sesión y escritura en origen; destino libre, nombre <=12 y sin /; no cambia contenido/inodo. Journal rename contenido nuevo nombre. |
| copy -path=/home/docs -destino=/destino | Sesión, lectura de origen y escritura de carpeta destino; omite ramas sin lectura; duplicado/ciclo/ausencia/espacio falla. Crea inodos/bloques nuevos; journal copy con destino. |
| move -path=/home/docs -destino=/destino | Sesión, escritura en origen/destino; no mueve dentro de sí/descendientes, no duplicado. Enlaza mismo inodo, cambia .. si carpeta, actualiza mtime; journal move con destino. |
| find -path=/ -name="?.txt" | Sesión y lectura de carpeta inicial/ramas; * uno o más, ? uno. FileManager.find/matchesPattern; lista rutas, no journal. |
| journaling -id=471A | EXT3 montada; readJournal y salida sin Graphviz; no cambia contenido. El endpoint y la GUI muestran la misma información. |
| loss -id=471A | EXT3 montada y válida; simulateLoss borra NUL en cuatro áreas, conserva journal y sb; invalida identidades. No registra una modificación de archivo y no intenta recuperar. |
| rep -name=tree -id=471A -path=tree.svg | Tipos mbr,disk,inode,block,bm_inode,bm_block,tree,sb,file,ls. Los dos últimos usan -path_file_ls=/ruta y requieren sesión en primaria, respetando lectura. MBR/DISK solo requieren montaje; los de FS formato válido. Graphviz usa fork/execlp argumentos separados. No journal. |
| exec -path=demo.smia | Archivo físico en inputRoot; máximo 1 MiB, anidación <=8. processInput reutiliza lexer/parser, preserva resultados/comentarios/continuación. Registra solo comandos modificadores exitosos contenidos. |

Flujo detallado de las 17 acciones solicitadas:

1. **Crear disco:** editor → lexer/parser → MKDISK valida → DiskManager.createDisk reserva tamaño fijo/ceros → escribe MBR → JSON.
2. **Crear partición:** FDISK convierte unidades, elige primer operador → DiskManager obtiene huecos/fit → MBR o cadena EBR actualizada.
3. **Montar:** MountManager valida primaria y duplicado → escoge letra/número libre → MBR persistente → mountedPartitions.
4. **Formatear EXT3:** MKFS valida ID/fs → respaldo → FileSystemManager.format descuenta Journal → áreas → raíz y users.txt → sb final.
5. **Iniciar sesión:** GUI POST o comando LOGIN → UserManager lee users.txt mediante identidad temporal root solo para autenticar → UID/GID verificados → Session del token.
6. **Crear carpeta:** resolve padres, -p si solicitado → canWrite → allocateInode/Block → . y .. → entrada padre → journal.
7. **Crear archivo:** contenido cont/size → resolve padres y permisos → allocateInode/datos/punteros → entrada padre → journal. Existente pausa antes de cambiar.
8. **Leer archivo:** findInodeByPath → canRead → directos/indirectos → i_s limita último bloque → atime → JSON compatible con texto.
9. **Copiar:** origen/destino válidos → copyTree con visited y omisiones → nuevas carpetas/archivos → journal, o backup restaura por falta de espacio.
10. **Mover:** addDirectoryEntry con mismo índice → changeEntry origen → .. nuevo → mtime → journal.
11. **Renombrar:** destino libre → changeEntry cambia nombre de Content en padre → journal.
12. **Eliminar:** collectTree verifica permisos completos → postorden → cambia referencias/libera bloques e inodos → journal.
13. **Buscar:** findTree recorre ramas legibles → matchesPattern calcula tabla → muestra rutas; no escribe journal.
14. **Registrar journal:** executeCommand precomprueba capacidad → operación exitosa → appendJournal escribe campos en estructura persistente. Fallos/cancelaciones no generan entradas.
15. **Simular LOSS:** validar EXT3 → limpiar intervalo desde bitmap de inodos hasta fin de bloques → conservar cabecera/journal → limpiar identidades.
16. **Visualizar React:** refresh carga discos/Session → selecciona disco → login verificado → GET entries → botón carpeta cambia path; archivo GET content. Los datos vienen del .mia real.
17. **Ejecutar scripts:** input File.text o EXEC → processInput por líneas → comentarios/resultados → y/n si pausa → continuación guardada por token. Un error no reinicia todo.

# Pruebas y archivos adicionales propios

## tests/project2.py

Usa únicamente stdlib de Python y estructuras lectoras importadas de audit.py. Inicia/detiene Crow en datos temporales; no implementa backend alternativo. Define Information y Journal ctypes con los campos reales para validar fórmula/layout.

`check(value,label)` no retorna datos, agrega label a checks o lanza AssertionError; `request(path,body=None,token='')` devuelve (HTTP,JSON) y captura HTTPError; `digest(path)` devuelve SHA256 para comprobar restauración exacta; `integrity(disk)` devuelve SB tras recorrer inodos alcanzables, . / .., bloques directos/indirectos y bitmaps; `fixture_permission(disk,path,permission)` cambia exclusivamente datos de prueba, sin agregar comandos fuera de alcance. `main()` organiza casos y devuelve disco de persistencia.

Client guarda token string generado por /api/session. `__init__()` establece token; `get(path,status=200)` valida HTTP y devuelve JSON; `command(text,success=True,confirm=None)` valida success/HTTP, exige pregunta si se solicita confirm y responde; `login(user,password,id,status)` valida API de autenticación; `logout()` valida cierre. Funciones locales `mark(block,level)` y `walk(index,parent)` dentro de integrity validan bloques únicos y árbol alcanzable, fallando ante corrupción.

Variables: BASE Path de tempfile, PORT int, URL string, checks list[str]. En integrity: start int; sb SB; raw bytes; ib/bb bytes de bitmaps; blocks/reachable set[int] crecen mientras se recorre el árbol. En main: a/b Client, disk/tiny/logical Path, before SHA256 o bytes de región, journal/saved_journal listas JSON, clients lista de identidades concurrentes. El bloque ejecutable configura env temporal, inicia server Popen, espera salud, ejecuta main, reinicia para validar persistencia, detiene solo sus procesos y guarda RESULTADOS_PROYECTO2.json incluso al fallar.

## tests/browser.mjs

Prueba de interfaz sin mocks con Playwright y Chromium. Importa stdlib Node, resuelve root/base, inicia Crow y Vite con spawn. `ready(url): Promise<void>` espera salud mediante fetch con límite; `execute(text)` local en la prueba llena textarea, espera respuesta real de /api/analyze y exige success. Constantes api/front delimitan puertos, checks string[] reúne flujos, browser/context/page son objetos Playwright, failures string[] guarda eventos pageerror. Ejecuta carga .smia, login incorrecto/correcto, lectura, sobrescritura, journal, reportes, segundo contexto, logout y LOSS; verifica viewport móvil. finally cierra navegador/servidores y escribe RESULTADOS_FRONTEND.json. Guarda screenshots en evidence.

## tests/08_proyecto2.smia y tests/09_loss.smia

El primero es la demostración completa previa a LOSS y deja sesión root activa para explorar. El segundo destruye únicamente los datos de la primaria EXT3 del demo, genera bitmaps y muestra la bitácora conservada. No contienen funciones ni otro parser: se cargan en React y pasan por processInput. Los scripts heredados 01..07 y demo_final se conservan; para sus rutas absolutas debe configurar directorios base acordes, o ejecutar prepare_scripts con bases autorizadas.

## .env.example y frontend/.env.example

Configuración de ejemplo, no contienen credenciales. El primero define seis variables de Crow/directorios/CORS; se carga con source en el shell. El segundo contiene VITE_API_URL y lo lee Vite durante dev/build. No tienen funciones propias ni cambian el .mia. Sus equivalentes .env no se versionan.

## deploy/mia-backend.service.example

Plantilla systemd, User/Group mia, WorkingDirectory y EnvironmentFile /opt/mia, ExecStart al ejecutable compilado, restart on-failure, umask 0077. Se adapta en la máquina de despliegue. No fue instalada ni activada. No contiene funciones de aplicación.

## deploy/nginx.conf.example

Fragmento location /api/ para un server HTTPS futuro. proxy_pass preserva la URI hacia loopback Crow; transmite Host y cabeceras del proxy; límite 1 MiB y timeout 180 segundos. No instala certificados ni crea infraestructura. X-MIA-Session se transmite como cabecera HTTP normal.

## Archivos estáticos y configuración restantes

- `frontend/index.html`: contenedor root y script de entrada Vite; ejecuta main.tsx; no lógica de archivos.
- `frontend/public/favicon.svg`: icono estático del navegador.
- `frontend/public/icons.svg`: recurso SVG estático; no contiene funciones TypeScript.
- `frontend/src/index.css`: estilos globales iniciales; sin funciones/estado.
- `frontend/src/styles/app.css`: estilos conservados, más explorer/login-form/view-tabs/table-scroll/file-content/report-view/error-message. Afecta presentación, no comandos/HTTP.
- `frontend/tsconfig.json`: referencias de compilación; `tsconfig.app.json` configura fuente React; `tsconfig.node.json` configura Vite. Sin comportamiento en disco virtual.
- `frontend/package.json`: scripts dev/build/lint/preview y dependencias React/Vite/TypeScript. `package-lock.json` fija versiones para npm ci; no se editó su grafo.
- `frontend/vite.config.ts`: defineConfig con plugin React; produce dev/build, sin servidor persistente de producción.
- `backend/CMakeLists.txt`: target existente 202405047_ext2_backend, C++17, Crow v1.2.1 y listado explícito de fuentes. El nombre del ejecutable se conserva para compatibilidad aunque soporte EXT3.
- `.gitignore` y `frontend/.gitignore`: reglas para no versionar datos/dependencias/caches/build. Se agregaron data, .env y Python caches.
- Los JSON de resultados y SVG/PNG/DOT/TXT de evidence son artefactos de prueba, no fuentes. Los manuales PDF previos son documentos históricos; no participan en compilación.

# Guía de defensa de Proyecto 2

**¿Qué es MBR y qué es EBR?** MBR describe todo el disco y cuatro particiones; EBR describe cada lógica dentro de una extendida y enlaza al siguiente EBR. Sus offsets son absolutos.

**¿Diferencia entre primaria y lógica?** Primaria ocupa una entrada MBR y es montable aquí. Lógica vive dentro de extendida y tiene EBR; se administra pero no se monta conforme al Proyecto 1.

**¿Cómo funcionan FF/BF/WF?** Sobre los huecos suficientes, FF el primero físico, BF el menor y WF el mayor. DiskManager conserva fit del disco para primarias/extendidas y el de la lógica para su selección.

**¿Qué es inodo y cómo se guarda un archivo?** Inodo contiene dueño/grupo, tamaño, fechas, permisos y 15 índices. Doce datos directos y tres niveles indirectos apuntan a bloques de 64 bytes; i_s limita el final.

**¿Cómo encuentra un bloque libre?** FileSystemManager recorre bitmap buscando ASCII '0'; allocateBlock lo convierte a '1', baja contador y actualiza primer libre. Reservar un apuntador también ocupa un bit.

**¿Cómo se representa carpeta?** Inodo type '0' y FolderBlocks con cuatro Content; cada entrada contiene nombre de 12 bytes e índice de inodo. . enlaza propia carpeta y .. al padre.

**¿Diferencia entre EXT2 y EXT3 simulado?** Tienen las mismas tablas y bloques. Tipo 3 agrega un Journal fijo de 50 registros después del superbloque y desplaza zonas; tipo 2 no lo reserva.

**¿Qué contiene journal y qué ocurre si se llena?** Operación, ruta, contenido y fecha float. count máximo 50; el wrapper rechaza nuevas modificaciones antes de escribir. No rota registros ni inventa recuperación.

**¿Qué hace LOSS?** Escribe NUL real en los dos bitmaps, tabla de inodos y área de bloques; conserva superbloque/journal y resto del disco. La sesión se invalida; consultar bitácora sigue funcionando.

**¿Cómo se ejecuta un comando y se detecta error?** Lexer produce tokens, parser construye ParsedCommand y parámetros, executeCommand verifica rutas/respaldo, executeRaw deriva a clase/manager; ValidationResult se convierte a salida HTTP. processInput separa fallos léxicos, sintácticos y de ejecución.

**¿Cómo mantiene sesiones?** Token aleatorio por navegador, map en RAM, UserManager verifica usuario/password. AppState solo contiene temporalmente la identidad de la petición bajo mutex; guardar/restaurar evita compartir login accidentalmente.

**¿Cómo React habla con C++?** fetch JSON con X-MIA-Session, rutas Crow y respuestas reales. El explorador consulta inodos/metadata del .mia, no un árbol inventado en el frontend.

**¿Cómo evita corrupción?** Un mutex impide cambios simultáneos y cada operación modificadora respalda el disco; ante error restaura bytes y estado. Pruebas independientes detectan inodos/bloques huérfanos. Una sola instancia por conjunto de datos.

**¿Cómo genera Graphviz?** ReportManager produce DOT escapando contenido, escribe un archivo temporal, llama dot mediante fork/execlp con argumentos separados y verifica exit status; sirve solo reportes registrados para ese token.

**¿Cómo funciona COPY?** Recorrido recursivo con control de visitados, lectura de origen, escritura de destino y creación con nuevos inodos/bloques. Omite ramas no legibles; otro error restaura todo.

**¿Por qué MOVE solo cambia referencias?** Los datos siguen en la misma primaria y el índice del inodo es válido en ambos directorios; cambiar entradas y .. basta. La prueba comprueba mismo inodo y nuevo padre.

**¿REMOVE puede borrar unos hijos y dejar otros?** No en esta interpretación: la regla normativa exige prevalidar todos y rechazar sin cambios si uno no permite escritura. Se documentó la contradicción del ejemplo PDF.

**¿Qué preserva un reinicio?** Archivos .mia, formatos, contenido, journal y campos de montaje MBR. Se reconstruye lista de montajes. Login, tokens, continuaciones e índices privados de reportes se reinician.

**¿Qué límites son reales?** ABI Linux 64 bits, tamaño basado en int, nombres 12 bytes, archivo 280320 bytes, journal 50 y campos fijos; fecha float aproximada. No son funcionalidades pendientes: son límites del simulador/especificación y decisiones documentadas.

# Preparación para AWS: configuración de producción

No se ejecutó ningún paso en AWS. Backend: `backend/build/202405047_ext2_backend`; compilación en Ubuntu y dependencias en README. MIA_HOST/MIA_PORT permiten bind y puerto (2611 por defecto). MIA_DISK_DIR/MIA_REPORT_DIR/MIA_INPUT_DIR deben apuntar a almacenamiento persistente accesible al usuario del servicio. Respaldos temporales están debajo de diskRoot/.transactions. Si un error de restauración ocurre, el mensaje conserva la ubicación del respaldo para inspección; no se elimina ese respaldo.

`deploy/mia-backend.service.example` permite ejecución persistente en Linux una vez adaptada. Use una sola instancia y proxy inverso HTTPS. Configure MIA_CORS_ORIGIN al origen real del frontend; no registre cuerpos con passwords. El proxy expone /api/ y transmite X-MIA-Session. El frontend se compila con VITE_API_URL antes de npm run build; dist contiene index.html y assets JS/CSS, listos como contenido estático sin Node. La interfaz usa tabs/estado, no rutas profundas, por lo que el alojamiento estático no requiere router fallback complejo.

Frontend HTTPS debe llamar backend HTTPS para evitar contenido mixto. En una arquitectura con mismo origen se puede omitir VITE_API_URL en producción; en S3 y API separados se configura explícitamente el dominio HTTPS. Pendientes exclusivamente de despliegue: EC2/S3/dominios/certificados/red/servicio/transferencia/configuración de persistencia y comprobación de las URLs públicas. La lista precisa está en PENDIENTE_AWS.md.
