#!/usr/bin/env python3
"""API real, aislamiento de navegadores e inspeccion independiente de .mia."""
import ctypes as c
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
from audit import MBR, EBR, SB, Inode, Entry, data_blocks, inode_at, read

ROOT = Path(__file__).resolve().parents[1]
BASE = Path(tempfile.mkdtemp(prefix='mia-p2-'))
PORT = int(os.environ.get('MIA_P2_PORT', '2614'))
URL = f'http://127.0.0.1:{PORT}'
checks = []
class Information(c.Structure):
    _fields_ = [('operation', c.c_char*10), ('path', c.c_char*32), ('content', c.c_char*64), ('date', c.c_float)]
class Journal(c.Structure):
    _fields_ = [('count', c.c_int), ('entries', Information*50)]

def check(value, label):
    if not value: raise AssertionError(label)
    checks.append(label)

def request(path, body=None, token=''):
    payload = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(URL+path, payload, {'Content-Type':'application/json','X-MIA-Session':token})
    try:
        with urllib.request.urlopen(req, timeout=60) as response: return response.status, json.load(response)
    except urllib.error.HTTPError as response: return response.code, json.load(response)

class Client:
    def __init__(self): self.token = request('/api/session')[1]['token']
    def get(self, path, status=200):
        code, body = request(path, token=self.token)
        check(code == status, f'GET {path}: {code} == {status}')
        return body
    def command(self, text, success=True, confirm=None):
        code, body = request('/api/analyze', {'input':text}, self.token)
        check(body['success'] == success, text[:110]+' -> '+str(body.get('messages'))[:200])
        check(code == (200 if success else 400), 'HTTP de comando '+text[:80])
        if confirm is not None:
            check(body['pendingConfirmation'], 'pregunta antes de modificar')
            return self.command(confirm)
        return body
    def login(self, user='root', password='123', id='471A', status=200):
        code, body = request('/api/login', {'user':user,'password':password,'id':id}, self.token)
        check(code == status, 'login '+user+' '+str(body))
    def logout(self):
        code, body = request('/api/logout', {}, self.token)
        check(code == 200, 'logout '+str(body))

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def integrity(disk):
    start=read(disk,MBR).partitions[0].start
    sb=read(disk,SB,start)
    raw=disk.read_bytes()
    ib=raw[sb.bm_inode:sb.bm_inode+sb.inodes]
    bb=raw[sb.bm_block:sb.bm_block+sb.blocks]
    check(sb.inodes==(read(disk,MBR).partitions[0].size-c.sizeof(SB)-(c.sizeof(Journal) if sb.type==3 else 0))//(4+c.sizeof(Inode)+192),'formula n con journal fijo de 50')
    check(sb.free_inodes==ib.count(b'0') and sb.free_blocks==bb.count(b'0'),'contadores coinciden con bitmap')
    blocks=set()
    reachable=set()
    def mark(block, level):
        check(0<=block<sb.blocks and bb[block]==49 and block not in blocks,'bloque unico ocupado '+str(block))
        blocks.add(block)
        if level:
            for child in read(disk,c.c_int*16,sb.block_start+block*64):
                if child!=-1: mark(child,level-1)
    def walk(index, parent):
        check(index not in reachable and ib[index]==49,'inodo unico alcanzable '+str(index));reachable.add(index)
        inode=read(disk,Inode,sb.inode_start+index*sb.inode_size)
        for i,block in enumerate(inode.pointers):
            if block!=-1: mark(block,0 if i<12 else i-11)
        if inode.type==b'0':
            children=[]
            for block in data_blocks(disk,sb,inode):
                children.extend(read(disk,Entry*4,sb.block_start+block*64))
            check(any(e.name==b'.' and e.inode==index for e in children),'referencia .')
            check(any(e.name==b'..' and e.inode==parent for e in children),'referencia ..')
            for entry in children:
                if entry.inode!=-1 and entry.name not in [b'.',b'..']: walk(entry.inode,index)
        else: check(len(data_blocks(disk,sb,inode))==(inode.size+63)//64,'numero de bloques archivo')
    walk(0,0)
    check(len(reachable)==ib.count(b'1') and len(blocks)==bb.count(b'1'),'sin inodos ni bloques huerfanos')
    return sb

def fixture_permission(disk, path, permission):
    sb=read(disk,SB,read(disk,MBR).partitions[0].start)
    index,inode=inode_at(disk,sb,path)
    inode.perm=permission.encode()
    with disk.open('r+b') as file:
        file.seek(sb.inode_start+index*sb.inode_size);file.write(bytes(inode))

def main():
    a=Client(); b=Client()
    check(a.token!=b.token, 'tokens independientes')
    check(request('/api/disks')[0]==401,'rechazo sin token')
    disk=BASE/'main.mia'
    a.command(f'mkdisk -size=8 -path={disk}')
    a.command(f'fdisk -size=4 -unit=M -path={disk} -name=p')
    a.command(f'mount -path={disk} -name=p')
    a.command('mkfs -id=471A -fs=3fs')
    sb=integrity(disk)
    check(sb.type==3 and sb.bm_inode==read(disk,MBR).partitions[0].start+c.sizeof(SB)+c.sizeof(Journal),'offset EXT3 correcto')
    a.login(); b.get('/api/files',401)
    check(not b.get('/api/session')['active'],'un navegador no hereda login')
    a.command('mkgrp -name=equipo\nmkusr -user=ana -pass=secreto -grp=equipo')
    b.login('ana','incorrecta',status=401);b.login('ana','secreto')
    check(b.get('/api/session')['user']=='ana' and a.get('/api/session')['user']=='root','sesiones independientes simultaneas')
    a.command('mkdir -p -path=/home/docs\nmkdir -path=/destino\nmkfile -path=/home/docs/a.txt -size=20000\nmkfile -path=/home/docs/b.txt -size=12')
    a.command('rename -path=/home/docs/a.txt -name=nuevo.txt')
    a.command('rename -path=/home/docs/nuevo.txt -name=b.txt',False)
    a.command('copy -path=/home/docs -destino=/destino')
    check(a.get('/api/files/content?path=/destino/docs/nuevo.txt')['content']==('0123456789'*2000),'copia contenido con indirectos')
    a.command('copy -path=/home -destino=/home/docs',False)
    a.command('move -path=/destino/docs -destino=/home',False)
    a.command('rename -path=/destino/docs -name=copia')
    before=inode_at(disk,sb,'/destino/copia')[0]
    a.command('move -path=/destino/copia -destino=/home')
    sb=integrity(disk)
    check(inode_at(disk,sb,'/home/copia')[0]==before,'MOVE conserva inodo y actualiza ..')
    a.command('move -path=/home -destino=/home/copia',False)
    found=a.command('find -path=/ -name="?.txt"')['messages'][0]
    check('/home/docs/b.txt' in found and 'nuevo.txt' not in found,'? un caracter')
    check('/home/docs/nuevo.txt' in a.command('find -path=/ -name=*')['messages'][0],'* uno o mas')
    a.command('find -path=/inexistente -name=*',False)
    a.command('remove -path=/home/copia')
    a.command('remove -path=/users.txt',False);a.command('remove -path=/',False)
    a.command('mkdir -path="/con espacio"\nmkfile -path="/con espacio/a.txt" -size=5')
    check(a.get('/api/files?path=/con%20espacio')['entries'][0]['name']=='a.txt','nombres con espacios')
    a.command('rename -path=/home/docs/b.txt -name=../x',False)
    a.command('copy -path=/home/docs -destino=/ausente',False)
    a.command('move -path=/home/docs -destino=/ausente',False)
    a.command('remove -path=/home/../destino',False)
    a.command('fdisk -path=../../etc/passwd -name=x -delete=full',False)
    a.command('mkfile -path=/leak -cont=/etc/passwd',False)
    a.command('rep -name=sb -id=471A -path=/etc/leak.svg',False)
    (BASE/'escape.mia').symlink_to('/etc/passwd')
    a.command('rmdisk -path=escape.mia',False)
    #permisos: COPY omite una rama; REMOVE es todo o nada
    fixture_permission(disk,'/home/docs/b.txt','000')
    fixture_permission(disk,'/destino','777')
    b.command('copy -path=/home/docs -destino=/destino')
    check(len(b.get('/api/files?path=/destino/docs')['entries'])==1,'COPY omite solo archivo sin lectura')
    a.command('mkdir -path=/borrable\nmkfile -path=/borrable/a -size=5\nmkfile -path=/borrable/b -size=5')
    fixture_permission(disk,'/borrable','777');fixture_permission(disk,'/borrable/a','777');fixture_permission(disk,'/borrable/b','000')
    before=digest(disk);b.command('remove -path=/borrable',False);check(digest(disk)==before,'REMOVE sin cambios cuando un descendiente no permite escritura')
    b.command('rename -path=/borrable/b -name=c',False)
    b.command('move -path=/borrable/b -destino=/destino',False)
    #confirmacion separada y journal solo al exito
    count=len(a.get('/api/journaling/471A')['entries'])
    a.command('mkfile -path=/home/docs/nuevo.txt -size=5')
    check(not b.get('/api/session').get('pendingConfirmation',False),'confirmacion pertenece al navegador')
    a.command('n')
    check(len(a.get('/api/journaling/471A')['entries'])==count,'cancelacion sin journal')
    a.command('mkfile -path=/home/docs/nuevo.txt -size=5',confirm='y')
    check(a.get('/api/files/content?path=/home/docs/nuevo.txt')['content']=='01234','sobrescritura confirmada')
    journal=a.get('/api/journaling/471A')['entries']
    check('secreto' not in json.dumps(journal),'journal no expone password')
    check(all(abs(entry['date']-time.time())<180 for entry in journal),'fecha persistida valida en float')
    integrity(disk)
    #scripts EXEC y comentarios, errores por linea
    script=BASE/'demo.smia';script.write_text('# comentario\nmkdir -path=/script\nmkfile -path=/script/a -size=3\n')
    result=a.command(f'exec -path={script}')
    check('# comentario' in result['messages'],'EXEC reutiliza comentarios')
    result=a.command('mounted\n=mal\nfind -path=/ -name=*',False)
    check(result['stats']['successfulCommands']==2 and result['stats']['syntaxErrors']==1,'error de linea no interrumpe resto')
    #reportes y permisos de descarga
    a.command('rep -name=tree -id=471A -path=tree.svg\nrep -name=bm_inode -id=471A -path=before.txt')
    reports=a.get('/api/reports')['reports'];check(len(reports)==2,'reportes visibles')
    req=urllib.request.Request(URL+reports[0]['url'],headers={'X-MIA-Session':a.token})
    with urllib.request.urlopen(req) as response: check(b'<svg' in response.read(),'reporte SVG real servido')
    try:
        urllib.request.urlopen(urllib.request.Request(URL+reports[0]['url'],headers={'X-MIA-Session':b.token}))
        raise AssertionError('descarga ajena permitida')
    except urllib.error.HTTPError as error: check(error.code==401,'reporte privado por navegador')
    #LOSS limpia solamente cuatro areas y preserva journal exactamente
    start=read(disk,MBR).partitions[0].start;sb=read(disk,SB,start)
    old=disk.read_bytes();saved_journal=a.get('/api/journaling/471A')['entries'];a.command('loss -id=471A')
    new=disk.read_bytes();end=sb.block_start+sb.blocks*64
    check(not any(new[sb.bm_inode:end]),'LOSS pone NUL en bitmaps inodos y bloques')
    check(new[:sb.bm_inode]==old[:sb.bm_inode] and new[end:]==old[end:],'LOSS conserva superbloque journal y otras regiones')
    check(a.get('/api/journaling/471A')['entries']==saved_journal,'journal consultable tras LOSS')
    check(not a.get('/api/session')['active'] and not b.get('/api/session')['active'],'LOSS invalida ambas sesiones')
    a.command('rep -name=bm_inode -id=471A -path=after.txt')
    a.command('mkfs -id=471A -fs=2fs');a.login();a.get('/api/journaling/471A',400)
    a.command('loss -id=471A',False);a.command('mkfile -path=/ext2 -size=64');integrity(disk)
    a.logout();a.command('unmount -id=471A')
    check('471A' not in a.command('mounted')['messages'][0],'UNMOUNT quita lista')
    part=read(disk,MBR).partitions[0];check(part.correlative==0 and not part.id and part.status==b'0','UNMOUNT campos binarios')
    a.command('unmount -id=471A',False)
    a.command(f'fdisk -path={disk} -name=p -add=-1000 -unit=K',False)
    a.command(f'fdisk -path={disk} -name=p -add=1 -unit=M')
    a.command(f'mount -path={disk} -name=p');a.login();a.get('/api/files/content?path=/ext2');a.logout();a.command('unmount -id=471A')
    #FAST conserva datos y FULL los limpia con confirmacion cancelable
    before=disk.read_bytes()[part.start:part.start+part.size]
    a.command(f'fdisk -path={disk} -name=p -delete=fast',confirm='n')
    check(read(disk,MBR).partitions[0].size>0,'cancelacion FDISK conserva particion')
    a.command(f'fdisk -path={disk} -name=p -delete=fast',confirm='y')
    check(disk.read_bytes()[part.start:part.start+part.size]==before,'FAST conserva datos fisicos')
    a.command(f'fdisk -path={disk} -name=p -size=4 -unit=M')
    a.command(f'fdisk -path={disk} -name=p -delete=full',confirm='yes')
    check(not any(disk.read_bytes()[part.start:part.start+4*1048576]),'FULL limpia region')
    #FAST en EBR inicial e intermedio y extendida con logicas
    logical=BASE/'logical.mia'
    a.command(f'mkdisk -size=2 -path={logical}')
    a.command(f'fdisk -path={logical} -name=e -size=1500 -type=E')
    a.command(f'fdisk -path={logical} -name=l1 -size=100 -type=L')
    a.command(f'fdisk -path={logical} -name=l2 -size=100 -type=L')
    a.command(f'fdisk -path={logical} -name=l3 -size=100 -type=L')
    ext=read(logical,MBR).partitions[0]
    first=read(logical,EBR,ext.start);middle=read(logical,EBR,first.next);last=read(logical,EBR,middle.next)
    with logical.open('r+b') as file:
        for entry in [first,middle,last]: file.seek(entry.start);file.write(b'contenido-logico')
    a.command(f'fdisk -path={logical} -name=l2 -delete=fast',confirm='y')
    check(read(logical,EBR,ext.start).next==middle.next,'FAST reconecta cadena EBR')
    with logical.open('rb') as file: file.seek(middle.start);check(file.read(16)==b'contenido-logico','FAST conserva datos logicos intermedios')
    a.command(f'fdisk -path={logical} -name=l1 -delete=fast',confirm='y')
    check(read(logical,EBR,ext.start).size==0 and read(logical,EBR,ext.start).next==middle.next,'FAST conserva EBR inicial vacio')
    a.command(f'fdisk -path={logical} -name=e -delete=full',confirm='y')
    check(not any(logical.read_bytes()[ext.start:ext.start+ext.size]),'FULL extendida limpia EBR y logicas')
    a.command(f'fdisk -path={logical} -name=e -size=1500 -type=E')
    a.command(f'fdisk -path={logical} -name=l -size=100 -type=L')
    old=logical.read_bytes()[ext.start:ext.start+ext.size]
    a.command(f'fdisk -path={logical} -name=e -delete=fast',confirm='y')
    check(logical.read_bytes()[ext.start:ext.start+ext.size]==old,'FAST extendida conserva region pero elimina referencias')
    a.command(f'fdisk -path={logical} -name=l -add=1',False)
    a.command(f'fdisk -path={logical} -name=e -size=100 -type=P')
    a.command(f'fdisk -path={logical} -name=e -add=1 -size=500 -unit=K')
    check(read(logical,MBR).partitions[0].size==101*1024,'FDISK primera operacion ADD ignora SIZE')
    a.command(f'fdisk -path={logical} -name=e -delete=full -unknown=1',False)
    a.command(f'rmdisk -path={logical}')
    #capacidad 50 y fallo por espacio con restauracion exacta
    a.command(f'fdisk -path={disk} -name=p -size=4 -unit=M');a.command(f'mount -path={disk} -name=p');a.command('mkfs -id=471A -fs=3fs');a.login()
    a.command('\n'.join(f'mkfile -path=/f{i} -size=0' for i in range(50)))
    check(len(a.get('/api/journaling/471A')['entries'])==50,'journal admite exactamente 50')
    before=digest(disk);a.command('mkfile -path=/extra',False);check(digest(disk)==before,'journal lleno rechaza antes de modificar')
    a.logout();a.command('unmount -id=471A')
    tiny=BASE/'tiny.mia';a.command(f'mkdisk -size=32 -unit=K -path={tiny}');a.command(f'fdisk -size=24 -path={tiny} -name=p');a.command(f'mount -path={tiny} -name=p');a.command('mkfs -id=471A -fs=3fs');a.login()
    a.command('mkdir -path=/a\nmkdir -path=/destino\nmkfile -path=/a/f -size=6000')
    before=digest(tiny);a.command('copy -path=/a -destino=/destino',False);check(digest(tiny)==before,'COPY sin espacio restaura disco exacto');integrity(tiny)
    a.logout();a.command('unmount -id=471A')
    #concurrencia serializada con varios clientes
    a.command(f'mount -path={disk} -name=p');a.command('mkfs -id=471A -fs=2fs');a.login()
    clients=[Client() for _ in range(4)]
    for client in clients: client.login()
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda item: item[1].command(f'mkfile -path=/parallel{item[0]} -size=2048'), enumerate(clients)))
    integrity(disk)
    a.command('unmount -id=471A');check(all(not client.get('/api/session')['active'] for client in clients),'UNMOUNT invalida otras sesiones')
    a.command(f'mount -path={disk} -name=p')
    a.command('mkfs -id=471A -fs=3fs');a.login()
    (BASE/'binary.txt').write_bytes(b'\xff\x00\x01')
    a.command(f'mkfile -path=/binary -cont={BASE}/binary.txt')
    a.get('/api/files/content?path=/binary',415)
    (BASE/'utf8.txt').write_text('Texto español: áéíóú y 世界',encoding='utf8')
    a.command(f'mkfile -path=/texto -cont={BASE}/utf8.txt')
    check(a.get('/api/files/content?path=/texto')['content']=='Texto español: áéíóú y 世界','visor UTF-8 valido')
    a.logout()
    return disk

if __name__=='__main__':
    log=(BASE/'server.log').open('w')
    server=subprocess.Popen([str(ROOT/'backend/build/202405047_ext2_backend')],env={**os.environ,'MIA_PORT':str(PORT),'MIA_DISK_DIR':str(BASE),'MIA_REPORT_DIR':str(BASE),'MIA_INPUT_DIR':str(BASE)},stdout=log,stderr=log)
    status='failed'
    try:
        for attempt in range(60):
            if server.poll() is not None: raise RuntimeError((BASE/'server.log').read_text())
            try:
                request('/api/health');break
            except OSError: time.sleep(.1)
        disk=main()
        before=disk.read_bytes()
        server.terminate();server.wait(timeout=5)
        server=subprocess.Popen([str(ROOT/'backend/build/202405047_ext2_backend')],env={**os.environ,'MIA_PORT':str(PORT),'MIA_DISK_DIR':str(BASE),'MIA_REPORT_DIR':str(BASE),'MIA_INPUT_DIR':str(BASE)},stdout=log,stderr=log)
        for attempt in range(60):
            try:
                request('/api/health');break
            except OSError: time.sleep(.1)
        client=Client()
        check('471A' in client.command('mounted')['messages'][0],'montaje persistente tras reiniciar')
        check(len(client.get('/api/journaling/471A')['entries'])==2,'journal persiste tras reiniciar')
        check(not client.get('/api/session')['active'],'reinicio requiere nuevo login')
        check(disk.read_bytes()==before,'reinicio no altera el disco')
        client.login();check(client.get('/api/files/content?path=/texto')['content']=='Texto español: áéíóú y 世界','archivo persiste tras reiniciar')
        status='passed';print(f'PASS P2: {len(checks)} verificaciones. Datos: {BASE}')
    finally:
        server.terminate();server.wait(timeout=5);log.close()
        (ROOT/'tests/RESULTADOS_PROYECTO2.json').write_text(json.dumps({'status':status,'checks':len(checks),'base':str(BASE),'verified':checks},ensure_ascii=False,indent=2))
