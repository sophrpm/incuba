# Explicación del código

Proyecto: **MIA_2S2026_P1_202405047**. Backend C++17; frontend React/TypeScript; discos binarios `.mia`; scripts `.smia`.

Esta guía explica la implementación existente después de la auditoría. Es material de estudio y no sustituye los manuales oficiales en PDF. Las contradicciones y decisiones de alcance están en `CAMBIOS_FINALES.md`; los documentos de entrega que faltan, en `PENDIENTES_DOCUMENTACION.md`. Los números de páginas citados allí son páginas físicas del PDF, porque su numeración impresa se reinicia.

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

El editor contiene texto; no contiene el sistema de archivos. Un comando se valida primero y después se ejecuta. Los managers vuelven a abrir el disco para leer sus estructuras. No existe un árbol permanente de carpetas en RAM. Los vectores que reúnen entradas, posiciones, líneas de users.txt o nodos visitados son temporales para la operación en curso. La sesión, los montajes, las rutas de reportes y una confirmación pendiente sí sobreviven entre peticiones mientras viva el servidor.

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

Se convierte el carácter octal a número y se aplica una máscara: lectura `valor & 4`, escritura `valor & 2`. MKDIR y MKFILE revisan escritura en la carpeta padre. Al sobrescribir también se exige escritura en el archivo. CAT exige lectura en el archivo. Los objetos nuevos tienen 664, dueño/grupo de la sesión y fechas actuales. La raíz se inicia con 777 para permitir crear objetos a los usuarios. CHMOD corresponde al proyecto 2; no se agregó ese comando.

# Cómo funciona users.txt

Es un archivo normal de EXT2. MKFS crea `1,G,root` y `1,U,root,root,123`, cada registro seguido de salto de línea. LOGIN lee el archivo con un estado temporal suficiente para autenticar antes de tener sesión. Compara nombre y contraseña exactamente; después obtiene el GID de un grupo activo.

MKGRP y MKUSR añaden registros. RMGRP y RMUSR cambian el primer campo a 0, conservando la línea. CHGRP cambia el campo del grupo del usuario. Solo root realiza estas operaciones. Los nombres y contraseñas se limitan a diez caracteres y se rechazan comas/saltos de línea que romperían el formato. El siguiente ID tiene en cuenta el número histórico de registros y el mayor ID activo; por eso borrar el último usuario no provoca que su UID vuelva a utilizarse. Los cambios de grupo se reflejan en una sesión al volver a iniciar sesión.

# Cómo funcionan MBR, Partition y EBR

DiskManager calcula huecos ordenando descriptores por `part_start`. FF toma el primer hueco suficiente; BF el más pequeño suficiente; WF el mayor. La implementación conservada utiliza `dsk_fit` para ubicar primarias/extendidas y el fit solicitado al crear una lógica para elegir su hueco. El valor de fit de cada partición también queda en su descriptor.

Las lógicas se insertan en una cadena ascendente de EBR. Al borrar la primera se conserva un EBR vacío que sigue apuntando al siguiente. Al borrar una intermedia se cambia el enlace del anterior y se limpia su región. Aumentar exige espacio contiguo hasta el siguiente objeto; reducir exige tamaño positivo y, para una extendida, no cortar EBR ni lógicas. El contenedor no cambia de longitud.

# Cómo funciona la API

`GET /api/health` informa disponibilidad. `POST /api/analyze` recibe `{ "input": "..." }`. Comprueba que input sea texto y mantiene un mutex durante ejecución: dos peticiones no pueden intercalar cambios sobre bitmaps o sobre la sesión única. Un JSON inválido recibe 400; los errores de comandos se devuelven en una respuesta 200 con `success:false`, porque el transporte HTTP sí funcionó.

```json
{
  "success": true,
  "messages": ["Archivo /a creado correctamente."],
  "stats": {"successfulCommands": 1, "lexicalErrors": 0, "syntaxErrors": 0},
  "pendingConfirmation": false,
  "reports": []
}
```

`success` indica si hubo fallos durante ese script; una pregunta pendiente no es un fallo. `pendingConfirmation` indica que aún falta una decisión. La siguiente petición lleva `y`, `yes`, `n` o `no` en input. Se reanuda el texto pendiente y se conservan los contadores acumulados. Una respuesta distinta vuelve a preguntar sin ejecutar ni perder el resto del script. Cancelar no cuenta como comando exitoso.

REP exitoso añade un enlace `/api/reports/N`; ese GET solo acepta índices de reportes registrados en el servidor. No permite pedir una ruta arbitraria de la computadora. Los archivos se muestran con el tipo de contenido correspondiente. La aplicación escucha en loopback; `MIA_PORT` permite cambiar 2611 para pruebas sin interferir con otro servidor. `VITE_API_URL` permite apuntar el frontend a ese puerto.

La sesión es única para la aplicación local. Cerrar sesión no equivale a desmontar. Reiniciar el proceso pierde montajes, sesión, enlaces y preguntas pendientes; no borra discos ni los archivos de reporte. Después de reiniciar hay que ejecutar MOUNT y LOGIN nuevamente.

# Flujo completo de un script .smia

App lee el archivo seleccionado con `File.text()` y conserva los saltos de línea en el editor. Al ejecutar, processInput recorre las líneas en orden; muestra intactos los comentarios completos y los espacios de las líneas vacías. Para cada línea de comando ejecuta lexer, parser y clase Command. Un error descarta esa línea, pero el bucle sigue con la siguiente. Una sobrescritura es la única pausa de interacción: guarda el texto restante y vuelve al navegador sin leer stdin ni dejar esperando un hilo del servidor.

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

**Recibe:** `const string& path, const string& name, string& message`.

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

**Recibe:** `const string& path, MBR& mbr, int partitionIndex, string& message`.

Rechaza una partición montada, pone a cero la región y sustituye la entrada MBR por Partition vacío.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** otros métodos de esta misma clase.

### deleteLogical()

**Recibe:** `const string& path, const string& name, string& message`.

Si borra el primer nodo deja un EBR vacío con el enlace previo. Para otros nodos enlaza anterior con siguiente y limpia EBR/contenido. Conserva el resto de la cadena.

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

**Recibe:** `const string& path, int partitionStart, SuperBlock& superBlock`.

Lee el encabezado. Si tiene magic EXT2 valida tipo, conteos, tamaños y offsets contra la partición del MBR; un encabezado sin magic se entrega para que el llamador diagnostique falta de formato.

**Devuelve:** `bool`: éxito o error de la operación. **Quién la llama:** `FileManager.cpp`, `ReportManager.cpp`, `MountManager.cpp`.

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

Impide sesión simultánea, resuelve montaje y lee users.txt con un estado temporal root. Distingue usuario inexistente de credenciales incorrectas y guarda la sesión autenticada.

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

Canoniza la ruta, localiza una primaria, impide duplicados y obtiene letra/correlativo/ID. Actualiza el descriptor y el superbloque si ya existe EXT2, y agrega MountedPartition en RAM.

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

Obtiene el máximo ordinal de ese disco en RAM y suma uno.

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

Crea un estado temporal de lectura sobre el montaje solicitado, lee la ruta interna completa y escribe nombre, ruta y contenido en texto.

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

Contiene carnet, mountedPartitions, session y pendingConfirmation. PendingConfirmation guarda active, path, content, recursive, remainingInput, tres contadores y allSuccess. El contenido solo está retenido mientras se decide la sobrescritura; no reemplaza al archivo permanente. FileManager prepara la pregunta; processInput guarda continuación y contadores, consume la respuesta y limpia el estado. No se agregaron estos campos a ninguna estructura del PDF.

**Defensa:** ¿Qué pasa si cierro el proceso con una pregunta pendiente? La pregunta y el texto no ejecutado se pierden; el archivo existente todavía no fue sustituido. El disco conserva lo que se ejecutó antes de la pregunta.

## backend/src/main.cpp

Incluye Crow, los comandos, AppState y helpers necesarios. Define una instancia appState, un mutex y reportPaths. `executeCommand(command)` recibe ParsedCommand y usa if por nombre para instanciar la clase correspondiente; devuelve ValidationResult. Al borrar un disco exitosamente elimina sus montajes y limpia una sesión dependiente. Lo llama processInput.

`processInput(inputText)` recibe una copia del texto, resuelve primero una respuesta pendiente, procesa líneas y devuelve crow::json::wvalue con messages/stats/pendingConfirmation/reports. Modifica discos mediante comandos, estado mediante montajes/sesión/confirmación y registro de reportes. Mantiene orden y permite continuar después de errores. No espera stdin.

`main()` configura CORS, health, analyze y GET de reportes, valida MIA_PORT y arranca Crow en loopback. Devuelve 1 si el puerto configurado es inválido; en operación permanece atendiendo hasta detener el servidor. Los lambdas de rutas son handlers HTTP: health devuelve JSON, analyze valida cuerpo y serializa ejecución con mutex, reports verifica índice y sirve el archivo registrado con su MIME. Pregunta: ¿por qué multithreaded y mutex? Hay varios hilos HTTP, pero el estado global y las escrituras se serializan para no asignar simultáneamente el mismo bit.

# Clases de comandos y sus archivos

## backend/include/commands/MkDiskCommand.hpp

Declara la clase `MkDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkDiskCommand.cpp`.

## backend/src/commands/MkDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKDISK.

Obligatorios: **size, path**. Opcionales/defaults: **fit=FF; unit=M**. Se apoya en `DiskManager::createDisk`. Crea padres físicos, llena ceros y escribe MBR; rechaza valores no positivos y tamaños que exceden int.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getSizeInBytes(size, unit)` recibe entero y unidad K/M y devuelve long long multiplicado por 1024 o 1048576; -1 para unidad no admitida. `getFit(fit)` devuelve B/F/W o carácter nulo. Son auxiliares privados de execute y no escriben archivos.

## backend/include/commands/RmDiskCommand.hpp

Declara la clase `RmDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmDiskCommand.cpp`.

## backend/src/commands/RmDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de RMDISK.

Obligatorios: **path**. Opcionales/defaults: **ninguno**. Se apoya en `DiskManager::removeDisk`. Elimina un .mia existente y el despachador retira sus referencias en RAM.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/FDiskCommand.hpp

Declara la clase `FDiskCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando FDISK al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/FDiskCommand.cpp`.

## backend/src/commands/FDiskCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de FDISK.

Obligatorios: **path, name; size para crear**. Opcionales/defaults: **unit=K; type=P; fit=WF; delete=full y add existentes**. Se apoya en `DiskManager::createPartition/deletePartition/resizePartition`. Distingue creación, eliminación y cambio de tamaño; no admite mezclar operaciones. Los números se convierten manualmente y el delta se limita antes de multiplicar unidades.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getSizeInBytes(size, unit)` convierte B/K/M a long long. `getType(type)` devuelve P/E/L o nulo. `getFit(fit)` devuelve B/F/W o nulo. `isDeleteOperation(command)` e `isAddOperation(command)` consultan presencia de las opciones; devuelven bool. Son llamados por execute y no modifican estado; las conversiones preparan los argumentos de DiskManager.

## backend/include/commands/MountCommand.hpp

Declara la clase `MountCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MOUNT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MountCommand.cpp`.

## backend/src/commands/MountCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MOUNT.

Obligatorios: **path, name**. Opcionales/defaults: **ninguno**. Se apoya en `MountManager::mountPartition`. Busca una primaria, valida duplicados y genera el ID 47 + ordinal + letra.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MountedCommand.hpp

Declara la clase `MountedCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MOUNTED al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MountedCommand.cpp`.

## backend/src/commands/MountedCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MOUNTED.

Obligatorios: **ninguno**. Opcionales/defaults: **ninguno**. Se apoya en `AppState::mountedPartitions`. No recibe parámetros. Devuelve ID, nombre y disco de cada montaje o informa que no hay montajes.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkFsCommand.hpp

Declara la clase `MkFsCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKFS al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkFsCommand.cpp`.

## backend/src/commands/MkFsCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKFS.

Obligatorios: **id**. Opcionales/defaults: **type=full**. Se apoya en `FileSystemManager::formatExt2`. Solo EXT2. -fs no pertenece al PDF y se rechaza. Reinicia la sesión si reformatea su partición para no conservar credenciales borradas.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/CatCommand.hpp

Declara la clase `CatCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando CAT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/CatCommand.cpp`.

## backend/src/commands/CatCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de CAT.

Obligatorios: **uno o más fileN**. Opcionales/defaults: **ninguno**. Se apoya en `FileManager::readFile`. Valida N positivo canónico, duplicados, valor y ruta absoluta; ordena numéricamente y separa contenidos con salto de línea.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`getFiles(command)` recibe parámetros ya validados, convierte el sufijo N, ordena pares N/ruta y devuelve vector de rutas. No lee contenido ni actualiza atime; execute llama después a FileManager en ese orden.

## backend/include/commands/LoginCommand.hpp

Declara la clase `LoginCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando LOGIN al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/LoginCommand.cpp`.

## backend/src/commands/LoginCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de LOGIN.

Obligatorios: **user, pass, id**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::login`. No normaliza usuario ni contraseña. Impide iniciar una segunda sesión.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/LogoutCommand.hpp

Declara la clase `LogoutCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando LOGOUT al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/LogoutCommand.cpp`.

## backend/src/commands/LogoutCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de LOGOUT.

Obligatorios: **ninguno**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::logout`. Rechaza parámetros y requiere sesión activa.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkGrpCommand.hpp

Declara la clase `MkGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkGrpCommand.cpp`.

## backend/src/commands/MkGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKGRP.

Obligatorios: **name**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::createGroup`. Exige nombre no vacío; el manager valida root, longitud y duplicados.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RmGrpCommand.hpp

Declara la clase `RmGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmGrpCommand.cpp`.

## backend/src/commands/RmGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de RMGRP.

Obligatorios: **name**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::removeGroup`. Valida parámetro obligatorio y el manager marca como eliminado un grupo activo.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkUsrCommand.hpp

Declara la clase `MkUsrCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKUSR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkUsrCommand.cpp`.

## backend/src/commands/MkUsrCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKUSR.

Obligatorios: **user, pass, grp**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::createUser`. Valida campos no vacíos; el manager comprueba root, longitud y existencia de grupo.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RmUsrCommand.hpp

Declara la clase `RmUsrCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando RMUSR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RmUsrCommand.cpp`.

## backend/src/commands/RmUsrCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de RMUSR.

Obligatorios: **user**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::removeUser`. El manager exige root y marca el UID con 0.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/ChGrpCommand.hpp

Declara la clase `ChGrpCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando CHGRP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/ChGrpCommand.cpp`.

## backend/src/commands/ChGrpCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de CHGRP.

Obligatorios: **user, grp**. Opcionales/defaults: **ninguno**. Se apoya en `UserManager::changeUserGroup`. Valida ambos campos y el manager exige que usuario y nuevo grupo estén activos.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/MkFileCommand.hpp

Declara la clase `MkFileCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKFILE al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkFileCommand.cpp`.

## backend/src/commands/MkFileCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKFILE.

Obligatorios: **path**. Opcionales/defaults: **r sin valor; size=0; cont**. Se apoya en `FileManager::createFile`. Si existe cont importa un archivo físico; si no, genera el patrón numérico. Verifica el límite de 280320 bytes antes de reservar contenido. El manager solicita confirmación para un archivo existente.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

`generateContent(size)` devuelve un string de size bytes repitiendo 0123456789 y vacío si size <= 0. `readExternalFile(path, content)` abre en binario, obtiene tamaño para rechazar más de 280320 bytes, vuelve al inicio y lee hasta EOF; devuelve bool y llena content. Ambas son privadas y las llama execute. -cont se interpreta en la computadora del backend, no en el árbol EXT2 ni como subida automática del navegador.

## backend/include/commands/MkDirCommand.hpp

Declara la clase `MkDirCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando MKDIR al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/MkDirCommand.cpp`.

## backend/src/commands/MkDirCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de MKDIR.

Obligatorios: **path**. Opcionales/defaults: **p sin valor**. Se apoya en `FileManager::createDirectory`. Exige ruta absoluta y envía la bandera recursive al manager. Sin -p no crea padres.

El comando interpreta opciones y el manager realiza los cambios persistentes o de sesión indicados; no se escribe el JSON desde esta clase. Pregunta de defensa: ¿qué error detecta esta clase antes de llamar al manager y cuál detecta el manager al leer el disco? Muestre como ejemplo un parámetro faltante frente a una estructura inexistente.

## backend/include/commands/RepCommand.hpp

Declara la clase `RepCommand` y su método público `execute`. Su responsabilidad es ofrecer el contrato del comando REP al despachador. Incluye ParsedCommand y ValidationResult; los comandos que usan sesión/montajes reciben AppState. No almacena estructuras físicas como atributos. Su implementación y auxiliares se encuentran en `backend/src/commands/RepCommand.cpp`.

## backend/src/commands/RepCommand.cpp

`execute(command, appState cuando corresponde)` recibe el comando parseado. Revisa parámetros permitidos, repeticiones, obligatoriedad, presencia de valores y valores vacíos; los comandos sin parámetros rechazan cualquier opción. Devuelve `ValidationResult` con éxito/mensaje. Lo llama `executeCommand` de main, que no hace la validación específica de REP.

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
6. El frontend muestra un enlace que abre el reporte en otra pestaña.

# Cómo funciona el frontend

## frontend/src/App.tsx

Componente raíz. Guarda fileName, commandText, messages, isOnline, busy, pending, answer, stats y reports. `App()` devuelve el árbol visual con encabezado, estado, selector, editor, botones, salida, confirmación y enlaces. useEffect llama a `verifyServer()` al montar: esta función async consulta health y actualiza isOnline.

`handleFileSelected(event)` recibe el evento del input, obtiene el primer File, exige extensión .smia y lee su texto; cambia nombre/editor/mensaje. Si la lectura falla muestra error. No ejecuta el contenido al seleccionarlo. `handleAnalyze()` evita doble envío y texto ausente; comprueba disponibilidad, envía commandText o answer según pending, reemplaza mensajes en una entrada nueva o agrega mensajes en continuación, guarda stats/reportes y libera busy en finally. La nueva pregunta cambia pending y limpia answer.

`handleClear()` vacía nombre, editor, mensajes, estadísticas y enlaces. Los botones están deshabilitados durante ejecución o confirmación para evitar borrar una interacción a medio resolver. El submit del formulario evita recargar la página y llama handleAnalyze. Todos estos callbacks devuelven void o Promise<void>; sus efectos son sobre estado de React, no escrituras directas de disco.

Usa Buttons, CommandEditor, FileChooser, Header, OutputPanel, ServerStatus y commandAPI. Lo monta main.tsx. Pregunta de defensa: ¿dónde se guarda el resto del script durante una pregunta? En el backend; el editor puede mostrar todo el texto, pero no se vuelve a ejecutar desde el comienzo.

## frontend/src/services/commandAPI.ts

Centraliza BACKEND_URL, configurable con VITE_API_URL y default localhost:2611. `checkServerStatus()` hace GET a health, devuelve Promise<boolean> y convierte errores de red en false. `analyzeCommands(commandText)` hace POST JSON con input y devuelve Promise<AnalysisResponse>; un estado HTTP no exitoso lanza Error para que App lo muestre. Los errores de comandos con HTTP 200 no se confunden con una caída del servidor. Ambas funciones las llama App.

## frontend/src/types/analysis.ts

Define ExecutionStats, GeneratedReport, AnalysisResponse y HealthResponse. No tiene funciones ni efectos en ejecución; TypeScript comprueba que el frontend usa los campos del JSON con los tipos previstos. GeneratedReport contiene name/url; AnalysisResponse contiene success, messages, stats, pendingConfirmation y reports. Lo importan API, App y OutputPanel.

## frontend/src/components/OutputPanel.tsx

OutputPanel recibe messages y stats (o null). Devuelve un panel con `<pre>`; une mensajes mediante salto de línea, conservando comentarios y líneas vacías. Cuando stats existe construye un resumen con los tres contadores al final. No interpreta HTML enviado por comandos: React representa el contenido como texto. No añade un símbolo > delante de CAT. App lo llama después de cada respuesta. Pregunta: ¿por qué pre? Porque un párrafo HTML normal colapsa espacios y saltos.

## frontend/src/components/CommandEditor.tsx

CommandEditor recibe value y onChange. Devuelve textarea controlado: cada cambio pasa su texto a App. Muestra número de líneas calculado con split, en lugar del número de caracteres anterior. No hace tokenización; el backend es quien valida. Lo llama App.

## frontend/src/components/FileChooser.tsx

FileChooser recibe fileName y onFileSelect y devuelve un input de archivos con accept=.smia y su etiqueta visual. El callback real está en App. El texto de ayuda coincide con la extensión del PDF. No conoce rutas del disco simulado ni importa -cont.

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

## frontend/.oxlintrc.json y frontend/.gitignore

La primera contiene la configuración de lint; la segunda excluye dependencias y artefactos locales de frontend. Se revisaron como configuración de desarrollo. No intervienen en interpretar comandos ni cambian bytes del .mia.

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

## EXPLICACION_CODIGO.md, CAMBIOS_FINALES.md y PENDIENTES_DOCUMENTACION.md

Este archivo explica el funcionamiento. CAMBIOS_FINALES justifica cambios y relaciona pruebas con la rúbrica. PENDIENTES_DOCUMENTACION separa material de estudio de entregables oficiales. No se crean manuales PDF por la instrucción expresa del usuario de posponerlos.

# Preguntas posibles de defensa

## 1. ¿Dónde empieza el MBR?

En el byte cero del archivo .mia; contiene las cuatro Partition embebidas.

## 2. ¿Por qué no crece el disco?

Se crea con tamaño fijo y las escrituras/limpiezas validan que el rango esté dentro de su longitud.

## 3. ¿Qué limita primarias y extendidas?

Las cuatro entradas del MBR y la restricción de una sola extendida.

## 4. ¿Qué contiene la extendida?

Una cadena de EBR y las regiones de datos de sus lógicas.

## 5. ¿Qué significa part_next=-1?

Que ese EBR es el último nodo de la cadena.

## 6. ¿Qué sucede al borrar la primera lógica?

Se conserva el EBR inicial vacío y su enlace al siguiente.

## 7. ¿Cuál es la diferencia entre FF, BF y WF?

Primero suficiente, menor suficiente y mayor suficiente, respectivamente.

## 8. ¿Cómo se forma 471A?

47 son los últimos dígitos del carnet, 1 el primer ordinal y A la letra del primer disco.

## 9. ¿Por qué no se monta una extendida ni lógica?

La sección específica de MOUNT limita esta simulación a primarias; la extendida es contenedora.

## 10. ¿Qué persiste al reiniciar?

El .mia y archivos de reportes. Sesión, montajes, URLs registradas y preguntas son estado del proceso.

## 11. ¿Qué hace MKFS con datos previos?

Limpia toda la partición y crea un EXT2 nuevo con raíz y users.txt.

## 12. ¿De dónde salen los cuatro bytes en la fórmula de n?

Uno del bitmap de inodos y tres del bitmap de bloques por cada inodo reservado.

## 13. ¿Por qué hay 3n bloques?

Es el layout exigido por el PDF, y cada bloque ocupa 64 bytes.

## 14. ¿Es lo mismo índice que dirección?

No. Para acceder se suma inicio de tabla más índice por tamaño de estructura.

## 15. ¿Qué guardan s_firts_ino y s_first_blo?

Direcciones absolutas del primer libre, o -1 cuando no hay libres.

## 16. ¿Por qué un bloque apuntador consume bitmap?

Porque ocupa un bloque físico de 64 bytes, igual que datos o carpeta.

## 17. ¿Cuándo aparece el simple indirecto?

Al necesitar el bloque de contenido número 12, es decir más de 768 bytes.

## 18. ¿Cuándo aparece el doble?

Después de 28 bloques de contenido: a partir de 1793 bytes.

## 19. ¿Cuándo aparece el triple?

Después de 284 bloques: a partir de 18177 bytes.

## 20. ¿Cuál es el máximo por archivo?

4380 bloques de datos, 280320 bytes, siempre que la partición tenga espacio para datos y apuntadores.

## 21. ¿Cómo se lee el último bloque parcial?

Se copian solo los bytes restantes según i_s, no todos los 64.

## 22. ¿Cómo se crean . y ..?

Se guardan como las primeras dos entradas del primer FolderBlock con índices propio y padre.

## 23. ¿Qué significa permiso 664?

Propietario y grupo tienen lectura/escritura; otros solo lectura.

## 24. ¿El propietario puede usar permisos del grupo si los suyos fallan?

No. Se elige exactamente una categoría U, G u O, en ese orden.

## 25. ¿Por qué root puede leer un archivo 000?

La verificación de uid 1 ocurre antes de evaluar los bits UGO.

## 26. ¿Quién puede crear usuarios?

Solo una sesión root; el grupo del usuario debe existir y estar activo.

## 27. ¿Por qué no se elimina físicamente la línea de un usuario?

ID 0 indica eliminado y la línea conserva historia para asignar IDs sin reutilizarlos.

## 28. ¿LOGIN distingue Ana de ana?

Sí; nombre y contraseña se comparan exactamente.

## 29. ¿Qué ruta utiliza -cont?

Una ruta real accesible desde la computadora del backend, no una ruta interna de EXT2.

## 30. ¿Cuál gana entre size y cont?

cont. Si no puede leerse, el comando falla y no genera números como reemplazo.

## 31. ¿Cómo se confirma sin bloquear Crow?

La petición termina con pendingConfirmation; otra petición trae y/n y reanuda el script guardado.

## 32. ¿Cancelar cuenta como comando exitoso?

No. Tampoco cuentan preguntas, comentarios ni líneas vacías.

## 33. ¿Permiso denegado es error sintáctico?

No, es un error de ejecución/validación; la estructura del comando puede ser correcta.

## 34. ¿Qué evita contar dos veces una comilla sin cerrar?

processInput no llama al parser sobre una línea que ya contiene errores léxicos.

## 35. ¿Por qué CAT -file10 va después de -file2?

Se ordenan los sufijos como enteros, no como cadenas lexicográficas.

## 36. ¿Cómo sabemos que funcionan los indirectos?

La prueba crea tamaños que los fuerzan, compara CAT exactamente y recorre punteros/bitmaps con un lector independiente.

## 37. ¿Qué prueba que no hay bloques huérfanos?

Se compara el conjunto de bloques referenciados por todos los inodos ocupados con los bits 1 del bitmap.

## 38. ¿Por qué se usa mutex en la API?

La aplicación comparte sesión y disco; evita intercalar dos reservas del mismo índice entre hilos.

## 39. ¿Cómo se generan imágenes de reportes?

Se construye DOT y se ejecuta Graphviz dot con argumentos separados, verificando su salida.

## 40. ¿Por qué MBR pequeño muestra 0.00%?

El reporte redondea a dos decimales; su tamaño real no es cero y sus posiciones se muestran.

## 41. ¿Por qué REP puede ejecutarse antes de LOGIN?

Es una sección separada de reportes usada para verificar discos y formato; requiere montaje, y los de EXT2 requieren formato.

## 42. ¿Qué no cubre compilar?

No demuestra contenido correcto, permisos, layout ni reportes; por eso hay pruebas funcionales, binarias y visuales.

## 43. ¿Existe CHMOD en este proyecto?

No se implementó: la sección de permisos lo remite al proyecto 2 aunque el cronograma lo mencione.

## 44. ¿Qué limitaciones deben explicarse?

ABI nativo, sesión local única y ausencia de journaling/recuperación de cortes. Los errores esperables se validan, pero no se promete atomicidad ante fallos físicos de escritura.

## 45. ¿Qué falta para entregar formalmente?

Los manuales oficiales en PDF y demás documentos enumerados, además de comprobar repositorio privado/acceso y envío del enlace por el estudiante.
