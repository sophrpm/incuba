# Pasos siguientes para subir MIA Proyecto 2 a AWS

Guía del 13 de octubre de 2026. Estos pasos son instrucciones para ejecutar posteriormente; **no se han ejecutado en AWS**. El PDF exige **frontend en S3 y backend en EC2**. Se usa una instancia EC2 Ubuntu y un bucket S3. No se requiere Docker, CloudFront, balanceador, base de datos, Lambda, SDK de AWS en C++, ni otro servicio AWS.

## 1. Qué se publica y qué permanece privado

| Destino | Contenido |
|---|---|
| EC2 /opt/mia | Código C++, ejecutable compilado, ejemplos existentes y configuración |
| EC2 /opt/mia/data/disks/ | Archivos .mia, acceso de escritura exclusivo del servicio |
| EC2 /opt/mia/data/inputs/ | Scripts/entradas reales para EXEC y MKFILE -cont |
| EC2 /opt/mia/data/reports/ | Reportes Graphviz/texto; se sirven mediante Crow con token |
| S3 dedicado al sitio | Únicamente los archivos dentro de frontend/dist/ |

El bucket no recibe discos, reportes, scripts privados, código fuente, `.env`, credenciales, manuales internos ni archivos de prueba. No se usa S3 para editar .mia por offsets.

Esta guía describe una instalación inicial. Si ya existe /opt/mia o hay datos, **no vuelva a clonar encima ni copie datos de demostración sobre ellos**. Revise la instalación existente y aplique solamente la configuración que falte. Las pruebas usan un disco de nombre nuevo. Nunca ejecute los scripts LOSS/MKFS/DELETE sobre datos que quiera conservar.

## 2. Crear EC2 y limitar los puertos

Desde la cuenta AWS autorizada:

1. Cree una sola instancia Ubuntu Server 24.04 LTS, arquitectura x86_64 para los ejemplos. El binario del entorno local no sustituye compilarlo en Ubuntu.
2. Use el almacenamiento EBS local de esa EC2 para el repositorio y data. Elija capacidad que incluya los discos .mia, reportes y espacio adicional equivalente al mayor disco para los respaldos transitorios. No utilice almacenamiento efímero para data.
3. Asigne una IP pública y conserve una clave SSH fuera del repositorio. Si la IP cambia al detener/iniciar EC2, será necesario actualizar URL de API y recompilar/subir frontend.
4. Security Group: TCP 22 solo desde la IP pública de administración /32. TCP 80 desde las direcciones de evaluación, o 0.0.0.0/0 si el sitio debe estar accesible públicamente. **No abra 2611**: Crow escuchará en loopback y Nginx será el punto de entrada.
5. No necesita un rol IAM para esta instancia: el backend no usa S3 ni llama a otras APIs AWS.

Conecte por SSH como ubuntu usando su IP y clave existentes. No copie la clave privada a la instancia ni al repositorio.

## 3. Instalar dependencias y clonar

En EC2:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git libasio-dev graphviz nginx curl
```

Clone el repositorio privado con el método de acceso que ya tenga autorizado, sin escribir tokens en la URL. Sustituya la URL SSH siguiente:

```bash
sudo install -d -m 755 -o ubuntu -g ubuntu /opt/mia
git clone git@github.com:TU_USUARIO/TU_REPOSITORIO.git /opt/mia
cd /opt/mia
```

El directorio debe estar vacío al clonar. No ejecute esta secuencia sobre una instalación existente. No se necesita Node.js para ejecutar Crow ni para servir el frontend ya compilado.

## 4. Compilar el backend en Ubuntu

```bash
cd /opt/mia
cmake -S backend -B backend/build -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
cmake --build backend/build --parallel 2
dot -V
test -x backend/build/202405047_ext2_backend
```

CMake descarga Crow v1.2.1 de GitHub la primera vez. Asio viene de libasio-dev. Si CMake no encuentra el paquete instalado, compruebe `/usr/include/asio.hpp` y use:

```bash
cmake -S backend -B backend/build -DASIO_INCLUDE_DIR=/usr/include -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF
cmake --build backend/build --parallel 2
```

Mantenga el nombre del ejecutable existente aunque contenga “ext2”: también implementa EXT3. No cambie archivos fuente ni nombres de managers para desplegar. Si aparece un error, conserve la salida exacta y resuelva la dependencia/versión; no reutilice CMakeCache de otra computadora.

## 5. Crear usuario y directorios del servicio

Si el usuario no existe:

```bash
id mia || sudo useradd --system --user-group --home-dir /opt/mia --no-create-home --shell /usr/sbin/nologin mia
```

En una instalación nueva:

```bash
sudo install -d -m 700 -o mia -g mia /opt/mia/data/disks
sudo install -d -m 700 -o mia -g mia /opt/mia/data/inputs
sudo install -d -m 700 -o mia -g mia /opt/mia/data/reports
sudo -u mia test -r /opt/mia/backend/build/202405047_ext2_backend
sudo -u mia test -x /opt/mia/backend/build/202405047_ext2_backend
sudo -u mia test -w /opt/mia/data/disks
sudo -u mia test -w /opt/mia/data/inputs
sudo -u mia test -w /opt/mia/data/reports
```

Los comandos install solo preparan directorios; no formatean ni eliminan .mia. Si hay datos existentes, compruebe que mia pueda leer/escribir cada archivo requerido antes de iniciar; ajuste únicamente los archivos identificados, no aplique cambios de propietario masivos. Nginx no necesita leer data: descarga reportes a través de la API.

Mantenga /opt y /opt/mia transitables y las fuentes/ejecutable legibles para mia. WorkingDirectory del servicio será /opt/mia, de modo que `./data/...` resolve correctamente. El proceso crea su carpeta de respaldos transitorios `data/disks/.transactions` y elimina los respaldos al completar operaciones.

## 6. Crear el bucket S3 del frontend

Use un nombre globalmente único, por ejemplo `mia-p2-202405047-TU_SUFIJO`, y una región definida. Los ejemplos de URL de esta guía usan **us-east-1**; si elige otra región copie el endpoint real de la consola.

1. Cree un bucket S3 de propósito general dedicado exclusivamente a los archivos públicos del sitio. Mantenga Object Ownership en **Bucket owner enforced**, sin ACL por objeto.
2. En Properties → Static website hosting, active alojamiento estático. Index document: `index.html`. La app usa estado/tabs y no necesita rutas profundas; puede definir `index.html` como documento de error si lo requiere su publicación.
3. Copie el **Bucket website endpoint** mostrado por la consola, por ejemplo `http://TU_BUCKET.s3-website-us-east-1.amazonaws.com`. Ese origen, sin barra final, será MIA_CORS_ORIGIN.
4. Para que un navegador público pueda cargar un sitio S3 website, los objetos del build deben ser legibles públicamente. En ese bucket permita la política pública: desactive los controles **BlockPublicPolicy** y **RestrictPublicBuckets** que bloquean la política/lectura pública. Puede mantener activados **BlockPublicAcls** e **IgnorePublicAcls**, porque no se usan ACL. No desactive controles de otros buckets.
5. Si el bloqueo a nivel de cuenta u organización impide esta política, solicite la excepción autorizada antes de continuar. No afirme que un website privado cumple el acceso público requerido ni agregue servicios para eludir la restricción.
6. Instale esta política de lectura, sustituyendo TU_BUCKET en el ARN:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "ReadOnlyFrontendWebsite",
      "Effect": "Allow",
      "Principal": "*",
      "Action": "s3:GetObject",
      "Resource": "arn:aws:s3:::TU_BUCKET/*"
    }
  ]
}
```

La publicación pública se justifica aquí porque el PDF exige el frontend web en S3. La política no concede escritura, borrado ni listado. Su seguridad depende de que el bucket contenga solo dist. No use ACL public-read, no suba la carpeta completa del repositorio y no coloque datos privados en ese bucket.

El CORS del bucket S3 no conecta React con Crow: las solicitudes API deben ser permitidas por el CORS del backend. No es necesario agregar un CORS S3 abierto para esta aplicación.

## 7. Configurar Crow con el origen S3 exacto

En EC2, cree o edite `/opt/mia/.env` con `sudoedit /opt/mia/.env`; conserve configuraciones existentes que aún sean válidas. El archivo **no necesita contraseñas ni claves AWS**. Use:

```dotenv
MIA_HOST=127.0.0.1
MIA_PORT=2611
MIA_DISK_DIR=./data/disks
MIA_REPORT_DIR=./data/reports
MIA_INPUT_DIR=./data/inputs
MIA_CORS_ORIGIN=http://TU_BUCKET.s3-website-us-east-1.amazonaws.com
```

Sustituya por el website endpoint real de S3 sin barra final. El systemd de la sección siguiente carga EnvironmentFile; C++ no carga .env automáticamente por su cuenta.

Las rutas relativas físicas de MKDISK, FDISK, MOUNT y RMDISK viven bajo data/disks. REP escribe bajo data/reports. EXEC y -cont leen bajo data/inputs. Las rutas virtuales `/home/...` permanecen dentro de la primaria de la sesión; no son rutas Linux.

## 8. Instalar y probar systemd

Se reutiliza el archivo existente, sin crear otro servicio equivalente:

```bash
cd /opt/mia
sudo install -m 644 deploy/mia-backend.service.example /etc/systemd/system/mia-backend.service
sudo systemd-analyze verify /etc/systemd/system/mia-backend.service
sudo systemctl daemon-reload
sudo systemctl enable --now mia-backend
sudo systemctl status mia-backend --no-pager
curl --fail http://127.0.0.1:2611/api/health
```

Debe recibir JSON con success true. El servicio usa User/Group mia, WorkingDirectory /opt/mia, el ejecutable compilado, Restart=on-failure y UMask=0077. No ejecute otra instancia del backend sobre los mismos discos.

Si no arranca:

```bash
sudo journalctl -u mia-backend -n 80 --no-pager
```

Compruebe existencia y acceso a .env/directorios, puerto libre y compilación. No cambie permisos abriendo data a todo el mundo para ocultar el error. Reiniciar Crow conserva archivos y journal, pero exige login nuevo y reinicia los índices privados de reportes de navegador.

## 9. Configurar Nginx como entrada de API

El ejemplo existente ahora es un bloque server completo:

```bash
cd /opt/mia
sudo install -m 644 deploy/nginx.conf.example /etc/nginx/sites-available/mia
sudo ln -s /etc/nginx/sites-available/mia /etc/nginx/sites-enabled/mia
sudo nginx -t
```

Si el enlace ya existe, no lo duplique. En una EC2 nueva, revise si `/etc/nginx/sites-enabled/default` compite por el mismo server_name `_` y puerto 80. Deshabilite únicamente ese enlace predeterminado si corresponde; conserve cualquier otro sitio existente. No borre configuraciones de sitios ajenos.

```bash
sudo systemctl enable --now nginx
sudo systemctl reload nginx
curl --fail http://127.0.0.1/api/health
```

Desde su computadora, con la IP real de EC2:

```bash
curl --fail http://IP_PUBLICA_EC2/api/health
```

`proxy_pass http://127.0.0.1:2611;` no lleva barra final: debe conservar `/api/` para que Crow encuentre sus rutas. Las cabeceras CORS las envía Crow; no agregue otro Access-Control-Allow-Origin contradictorio en Nginx. X-MIA-Session pasa normalmente por el proxy.

La raíz `/opt/mia/frontend/dist` del ejemplo sirve una copia local opcional de React para diagnóstico. No es el alojamiento oficial de la entrega: el frontend final está en S3. La API funciona aunque esa copia todavía no exista; `/api/health` no depende de index.html.

Para no introducir servicios adicionales, el escenario mínimo de esta guía usa HTTP tanto en el website S3 como en Nginx. El website endpoint S3 no ofrece HTTPS. Si el frontend que elija es HTTPS, el backend también debe tener HTTPS para evitar contenido mixto; no afirme que esta configuración HTTP ya verificó certificados/dominos.

## 10. Compilar React para la URL pública real

En su computadora de desarrollo con **Node.js 24 LTS** (o una versión >=22.12 compatible con Vite 8). Puede usar EC2 si allí ya instaló una versión compatible; Ubuntu puede ofrecer una versión antigua en sus paquetes, por lo que compruebe primero:

```bash
node --version
npm --version
```

Desde la raíz del repositorio:

```bash
npm --prefix frontend ci
VITE_API_URL=http://IP_PUBLICA_EC2 npm --prefix frontend run build
npm --prefix frontend run lint
```

Sustituya IP_PUBLICA_EC2; **no agregue /api al final** porque commandAPI lo agrega a cada ruta. No use localhost: cada navegador interpretaría localhost como su propia computadora. La variable de shell de ese comando tiene prioridad sobre la .env de desarrollo de Vite.

El resultado publicable es `frontend/dist/index.html`, `frontend/dist/assets/...` y los recursos estáticos copiados. No hay servidor Node de producción. VITE_API_URL queda insertada en el JS del build; cambiar .env después de subirlo no tiene efecto sin recompilar.

Para usar la copia de diagnóstico Nginx, transfiera solo ese dist a `/opt/mia/frontend/dist/` en EC2 manteniendo lectura para www-data. Es opcional y no sustituye S3.

## 11. Subir únicamente dist a S3

Desde la consola S3, suba **el contenido** de frontend/dist al nivel raíz del bucket. No suba una carpeta dist que deje el índice en `/dist/index.html`. Compruebe los Content-Type: HTML text/html, JavaScript text/javascript o application/javascript, CSS text/css y SVG image/svg+xml.

Alternativa si ya tiene AWS CLI con una identidad autorizada en su computadora (no instale ni configure credenciales en el backend):

```bash
aws s3 sync frontend/dist/ s3://TU_BUCKET/ --region us-east-1
```

Este comando es para ejecutar usted después de revisar el build y seleccionar el bucket. No se ejecutó en esta auditoría. No utiliza --delete, no asigna ACL públicas y no sube data. En una actualización, sustituye los objetos del sitio con los mismos nombres; conserva otros objetos, por lo que el bucket debe seguir siendo dedicado al frontend.

### Permisos mínimos

- El usuario administrador de instalación necesita permisos para crear/configurar la EC2, su Security Group y el bucket/website/política; use la identidad de administración ya autorizada.
- El backend EC2 no necesita permisos S3 ni claves AWS.
- Un usuario que solo sube builds necesita, como base, s3:ListBucket para el bucket y s3:PutObject para sus objetos. No necesita s3:* ni publicar datos de otros buckets. Con cifrado SSE-S3 predeterminado no requiere un servicio de claves adicional.

Ejemplo de política IAM para **subida**, distinta de la política pública de **lectura** del bucket:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "s3:ListBucket",
      "Resource": "arn:aws:s3:::TU_BUCKET"
    },
    {
      "Effect": "Allow",
      "Action": "s3:PutObject",
      "Resource": "arn:aws:s3:::TU_BUCKET/*"
    }
  ]
}
```

No permita que el código React contenga credenciales AWS. React obtiene contenido estático públicamente y se autentica ante Crow con el token propio del simulador.

## 12. Verificar CORS y uso desde S3

Abra el website endpoint S3 en un navegador. En DevTools → Network, las solicitudes deben ir a `http://IP_PUBLICA_EC2/api/...`, no a localhost ni al dominio S3. El origen enviado será el website endpoint sin barra final.

Puede inspeccionar la preflight con sus valores reales:

```bash
curl -i -X OPTIONS http://IP_PUBLICA_EC2/api/analyze \
  -H 'Origin: http://TU_BUCKET.s3-website-us-east-1.amazonaws.com' \
  -H 'Access-Control-Request-Method: POST' \
  -H 'Access-Control-Request-Headers: content-type,x-mia-session'
```

Debe permitir el origen configurado, POST y Content-Type/X-MIA-Session. Para corregir el origen en Crow edite .env y ejecute `sudo systemctl restart mia-backend`. Para corregir la URL API recompile React y vuelva a subir solo dist.

### Flujo de aceptación con un disco nuevo

Antes de ejecutar, compruebe que `validacion-aws-202405047.mia` no exista; si existe elija otro nombre. Cree desde la terminal de la página S3:

```text
mkdisk -size=8 -path=validacion-aws-202405047.mia
fdisk -size=4 -unit=M -path=validacion-aws-202405047.mia -name=prueba
mount -path=validacion-aws-202405047.mia -name=prueba
```

Use el ID **real devuelto por MOUNT**, que puede ser distinto de 471A si hay montajes anteriores:

```text
mkfs -id=ID_REAL -fs=3fs
```

Inicie sesión mediante formulario: ID_REAL, root, 123. En el editor:

```text
mkdir -path=/docs
mkdir -path=/destino
mkfile -path=/docs/a.txt -size=25
rename -path=/docs/a.txt -name=b.txt
copy -path=/docs/b.txt -destino=/destino
move -path=/destino/b.txt -destino=/
find -path=/ -name=*.txt
cat -file1=/b.txt
rep -name=tree -id=ID_REAL -path=validacion-aws-tree.svg
rep -name=bm_inode -id=ID_REAL -path=validacion-aws-antes.txt
journaling -id=ID_REAL
```

Compruebe:

- Discos/particiones muestran datos correctos; carpetas navegan desde /; b.txt muestra 25 caracteres y permisos.
- Journal contiene modificaciones exitosas con campos/fecha/hora; reporte SVG se abre en la interfaz aunque su archivo viva en EC2.
- Un segundo navegador no hereda login. Logout limpia contenido; login vuelve a funcionar.
- Cargar un .smia muestra comentarios/resultados. Un error de comando muestra salida con HTTP 400 y no apaga falsamente el servidor.

Solo en esa primaria temporal, si desea verificar LOSS:

```text
loss -id=ID_REAL
journaling -id=ID_REAL
rep -name=bm_inode -id=ID_REAL -path=validacion-aws-despues.txt
```

El journal debe mantenerse; bitmaps cambian a NUL; login se invalida. No pruebe LOSS sobre discos existentes. La eliminación de prueba es manual y opcional; no hay limpieza automática de datos reales.

## 13. Entrega y mantenimiento mínimo

1. Guarde la URL del website S3, IP/URL API y capturas de cada paso exigido por el PDF; adjunte evidencia de conexión S3 → EC2.
2. Verifique repositorio privado y acceso a auxiliares desde la cuenta propietaria. Esta auditoría no cambió privacidad ni hizo commit/push.
3. Conserve copias de seguridad de data fuera del bucket público del frontend. Antes de una actualización, detenga el servicio para copiar consistentemente .mia; no suba esos discos al sitio S3.
4. Cambie código/build con los mecanismos existentes; recompilar C++ requiere reiniciar el servicio. No ejecute varios Crow sobre data compartida.
5. Si cambia IP pública de EC2, recompile VITE_API_URL y publique el build actualizado. Si cambia endpoint S3, actualice MIA_CORS_ORIGIN y reinicie Crow.
6. Ejecute nginx -t antes de recargar y compruebe systemctl/journalctl ante errores. El estado de “configurado” no sustituye abrir la URL pública y probar login/reportes.

Las pruebas locales de esta auditoría pasaron; EC2 Ubuntu, Nginx real, S3, permisos remotos y URLs de nube siguen pendientes de ejecución por el propietario. Las ambigüedades de journal/REMOVE y la matriz por páginas están en AUDITORIA_AWS_PROYECTO2.md.
