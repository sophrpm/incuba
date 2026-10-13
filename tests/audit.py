#!/usr/bin/env python3
"""pruebas de caja negra y lectura independiente del binario, sin dependencias externas."""
import ctypes as c
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
import urllib.request
import urllib.error

ROOT = Path(__file__).resolve().parents[1]
BASE = Path(tempfile.mkdtemp(prefix='mia-audit-'))
PORT = int(os.environ.get('MIA_TEST_PORT', '2612'))
URL = f'http://127.0.0.1:{PORT}'
checks = []
TOKEN = ""

class Partition(c.Structure):
    _fields_ = [('status',c.c_char),('type',c.c_char),('fit',c.c_char),('start',c.c_int),('size',c.c_int),('name',c.c_char*16),('correlative',c.c_int),('id',c.c_char*4)]
class MBR(c.Structure):
    _fields_ = [('size',c.c_int),('created',c.c_long),('signature',c.c_int),('fit',c.c_char),('partitions',Partition*4)]
class EBR(c.Structure):
    _fields_ = [('mount',c.c_char),('fit',c.c_char),('start',c.c_int),('size',c.c_int),('next',c.c_int),('name',c.c_char*16)]
class SB(c.Structure):
    _fields_ = [(x,c.c_int) for x in ['type','inodes','blocks','free_blocks','free_inodes']] + [('mtime',c.c_long),('umtime',c.c_long)] + [(x,c.c_int) for x in ['mounts','magic','inode_size','block_size','first_inode','first_block','bm_inode','bm_block','inode_start','block_start']]
class Inode(c.Structure):
    _fields_ = [('uid',c.c_int),('gid',c.c_int),('size',c.c_int),('atime',c.c_long),('ctime',c.c_long),('mtime',c.c_long),('pointers',c.c_int*15),('type',c.c_char),('perm',c.c_char*3)]
class Entry(c.Structure):
    _fields_ = [('name',c.c_char*12),('inode',c.c_int)]

def check(condition, name):
    if not condition:
        raise AssertionError(name)
    checks.append(name)

def read(path, typ, offset=0):
    with open(path,'rb') as f:
        f.seek(offset)
        data=f.read(c.sizeof(typ))
    return typ.from_buffer_copy(data)

def api(command, success=None):
    data=json.dumps({'input':command}).encode()
    global TOKEN
    if not TOKEN:
        with urllib.request.urlopen(URL+'/api/session') as response: TOKEN=json.load(response)['token']
    req=urllib.request.Request(URL+'/api/analyze', data, {'Content-Type':'application/json','X-MIA-Session':TOKEN})
    try:
        with urllib.request.urlopen(req,timeout=180) as response: result=json.load(response)
    except urllib.error.HTTPError as response:
        result=json.load(response)
    if command.startswith('fdisk ') and result.get('pendingConfirmation'):
        return api('y',success)
    if success is not None:
        check(result['success'] is success, command[:110]+' -> '+str(result['messages'])[:180])
    return result

def ok(command): return api(command,True)
def bad(command):
    result=api(command,False)
    check(result['stats']['successfulCommands']==0,'el error no cuenta como exito: '+command[:90])
    return result

def pattern(n): return ''.join(str(i%10) for i in range(n))
def cat(path): return ok('cat -file1='+path)['messages'][0]

def data_blocks(path,sb,inode):
    result=[]
    def pointers(index, level):
        block=read(path,c.c_int*16,sb.block_start+index*64)
        for child in block:
            if child==-1: continue
            if level==1: result.append(child)
            else: pointers(child,level-1)
    result.extend(x for x in inode.pointers[:12] if x!=-1)
    for i in range(12,15):
        if inode.pointers[i]!=-1: pointers(inode.pointers[i],i-11)
    return result

def inode_at(path,sb,internal):
    index=0
    for part in internal.strip('/').split('/'):
        if not part: continue
        inode=read(path,Inode,sb.inode_start+index*sb.inode_size)
        found=None
        for block in data_blocks(path,sb,inode):
            entries=read(path,Entry*4,sb.block_start+block*64)
            for entry in entries:
                if entry.inode!=-1 and entry.name.decode()==part: found=entry.inode
        if found is None: raise AssertionError('ruta no encontrada en binario '+internal)
        index=found
    return index,read(path,Inode,sb.inode_start+index*sb.inode_size)

def integrity(path,start):
    sb=read(path,SB,start)
    raw=path.read_bytes()
    ib=raw[sb.bm_inode:sb.bm_inode+sb.inodes]
    bb=raw[sb.bm_block:sb.bm_block+sb.blocks]
    check(sb.blocks==3*sb.inodes and sb.block_size==64 and sb.inode_size==c.sizeof(Inode),'layout 3n y tamaños '+path.name)
    check(sb.inodes==(read(path,MBR).partitions[0].size-c.sizeof(SB))//(4+c.sizeof(Inode)+192),'formula n '+path.name)
    check(set(ib)<=set(b'01') and set(bb)<=set(b'01'),'bits validos')
    check(ib.count(b'0')==sb.free_inodes and bb.count(b'0')==sb.free_blocks,'contadores libres coinciden')
    check(sb.first_inode==(-1 if b'0' not in ib else sb.inode_start+ib.index(b'0')*sb.inode_size),'direccion primer inodo libre')
    check(sb.first_block==(-1 if b'0' not in bb else sb.block_start+bb.index(b'0')*64),'direccion primer bloque libre')
    seen=set()
    def mark(block,level):
        check(0<=block<sb.blocks and bb[block]==49 and block not in seen,'bloque unico ocupado '+str(block))
        seen.add(block)
        if level:
            for child in read(path,c.c_int*16,sb.block_start+block*64):
                if child!=-1: mark(child,level-1)
    for index,bit in enumerate(ib):
        if bit!=49: continue
        inode=read(path,Inode,sb.inode_start+index*sb.inode_size)
        for i,block in enumerate(inode.pointers):
            if block!=-1: mark(block,0 if i<12 else i-11)
        if inode.type==b'1':
            check(len(data_blocks(path,sb,inode))==(inode.size+63)//64,'bloques exactos del archivo '+str(index))
    check(len(seen)==bb.count(b'1'),'sin bloques huerfanos')
    check(path.stat().st_size==read(path,MBR).size,'el disco no crecio')
    return sb

def main():
    disk=BASE/'main.mia'
    ok(f'mkdisk -size=12 -path="{disk}"')
    check(disk.stat().st_size==12*1024*1024,'mkdisk tamaño fisico')
    check(set(disk.read_bytes()[c.sizeof(MBR):])=={0},'mkdisk ceros binarios')
    ok(f'fdisk -size=6 -unit=M -path={disk} -name=primaria')
    ok(f'fdisk -size=3 -unit=M -type=E -path={disk} -name=extendida')
    ok(f'fdisk -size=100 -type=L -path={disk} -name=logica')
    bad(f'mount -path={disk} -name=extendida')
    bad(f'mount -path={disk} -name=logica')
    ok(f'mount -path={disk} -name=primaria')
    bad(f'mount -path={BASE}/./main.mia -name=primaria')
    check('471A' in ok('mounted')['messages'][0],'mounted incluye id')
    bad(f'rep -name=bm_inode -id=471A -path={BASE}/unformatted.txt')
    ok('mkfs -id=471A -fs=2fs')
    ok('mkfs -id=471A')
    start=read(disk,MBR).partitions[0].start
    sb=integrity(disk,start)
    ok('login -user=root -pass=123 -id=471A')
    check(cat('/users.txt')=='1,G,root\n1,U,root,root,123\n','users inicial')
    bad('login -user=root -pass=123 -id=471A')
    ok('mkgrp -name=Grupo')
    ok('mkgrp -name=grupo')
    bad('mkgrp -name=Grupo')
    ok('mkusr -user=Ana -pass=Clave -grp=Grupo')
    ok('mkusr -user=Beto -pass=abc -grp=Grupo')
    ok('mkusr -user=Otro -pass=abc -grp=grupo')
    ok('mkusr -user=tmp -pass=abc -grp=Grupo')
    ok('rmusr -user=tmp')
    ok('mkusr -user=nuevo -pass=abc -grp=Grupo')
    check('6,U,Grupo,nuevo,abc' in cat('/users.txt'),'uid no se reutiliza')
    ok('mkgrp -name=temporal')
    ok('rmgrp -name=temporal')
    ok('mkgrp -name=ultimo')
    check('5,G,ultimo' in cat('/users.txt'),'gid no se reutiliza')
    bad('mkgrp -name="a,b"')
    bad('mkusr -user=test -pass="a,b" -grp=Grupo')
    bad('mkusr -user=abcdefghijk -pass=abc -grp=Grupo')
    bad('rmusr -user=nadie')
    bad('chgrp -user=Ana -grp=eliminado')
    ok('chgrp -user=nuevo -grp=grupo')
    ok('logout')
    bad('login -user=ana -pass=Clave -id=471A')
    bad('login -user=Ana -pass=clave -id=471A')
    ok('login -user=Ana -pass=Clave -id=471A')
    ok('mkdir -path=/privada')
    ok('mkfile -path=/privada/a -size=19')
    check(cat('/privada/a')==pattern(19),'propietario lee')
    for cmd in ['mkgrp -name=no','rmgrp -name=Grupo','mkusr -user=no -pass=no -grp=Grupo','rmusr -user=Beto','chgrp -user=Beto -grp=grupo']:
        bad(cmd)
    ok('logout');ok('login -user=Beto -pass=abc -id=471A')
    ok('mkfile -path=/privada/b -size=10')
    check(cat('/privada/a')==pattern(19),'grupo lee y escribe')
    ok('logout');ok('login -user=Otro -pass=abc -id=471A')
    check(cat('/privada/a')==pattern(19),'otros leen 664')
    bad('mkfile -path=/privada/c -size=2')
    bad('mkdir -path=/privada/sub')
    bad('mkfile -path=/privada/a -size=2')
    check(not api('cat -file1=/privada/a')['pendingConfirmation'],'denegar permisos no pide confirmar')
    ok('logout');ok('login -user=root -pass=123 -id=471A')
    # fixture de permisos: no agrega chmod al lenguaje del proyecto
    sb=read(disk,SB,start);index,inode=inode_at(disk,sb,'/privada/a');inode.perm=b'000'
    with disk.open('r+b') as f: f.seek(sb.inode_start+index*sb.inode_size);f.write(bytes(inode))
    check(cat('/privada/a')==pattern(19),'root ignora 000')
    ok('logout');ok('login -user=Ana -pass=Clave -id=471A');bad('cat -file1=/privada/a')
    ok('logout');ok('login -user=root -pass=123 -id=471A')
    bad('mkdir -path=/faltante/hija')
    ok('mkdir -p -path=/uno/dos');ok('mkdir -p -path=/uno/dos')
    bad('mkdir -p=true -path=/invalida')
    bad('mkfile -r=true -path=/invalido')
    bad('mkfile -path=/archivo -cont=/no/existe')
    bad('mkfile -path=/negativo -size=-1')
    ok('mkfile -path=/vacio');check(cat('/vacio')=='','size default cero')
    external=BASE/'contenido real.txt';external.write_text('Hola <EXT2>\nsegunda linea\n',encoding='utf8')
    ok(f'mkfile -path=/cont -size=999 -cont="{external}"')
    check(cat('/cont')==external.read_text(),'cont real tiene prioridad')
    pending=ok('mkfile -path=/cont -size=3\ncat -file1=/cont')
    check(pending['pendingConfirmation'] and pending['stats']['successfulCommands']==0,'sobrescritura pausa sin contar')
    check(ok('quizas')['pendingConfirmation'],'respuesta invalida conserva pregunta')
    cancelled=ok('N');check(cancelled['messages'][-1]==external.read_text(),'cancelar conserva y continua')
    ok('mkfile -path=/cont -size=3\ncat -file1=/cont')
    confirmed=ok('yes');check(confirmed['stats']['successfulCommands']==2 and confirmed['messages'][-1]=='012','confirmar cuenta y continua')
    for name,size in [('directo',768),('simple',769),('doble',1793),('triple',20000)]:
        ok(f'mkfile -path=/{name} -size={size}')
        check(cat('/'+name)==pattern(size),'contenido exacto '+name)
        sb=read(disk,SB,start);_,inode=inode_at(disk,sb,'/'+name)
        depth={'directo':11,'simple':12,'doble':13,'triple':14}[name]
        check(inode.pointers[depth]!=-1 and all(x==-1 for x in inode.pointers[depth+1:]),'profundidad real '+name)
    check(ok('cat -file10=/cont -file2=/vacio -file1=/simple')['messages'][0]==pattern(769)+'\n\n012','cat orden numerico y saltos')
    for cmd in ['cat -file1=/cont -file1=/cont','cat -file0=/cont','cat -file01=/cont','cat -file99999999999999999999=/cont','cat -file1=/cont -file2','cat -file1=/cont -file2=""','cat -file1=relativa']:
        bad(cmd)
    sb=integrity(disk,start)
    reportdir=BASE/'reports';reportdir.mkdir()
    for name in ['mbr','disk','inode','block','bm_inode','bm_block','tree','sb','file','ls']:
        ext='txt' if name in ['file','bm_inode','bm_block'] else 'svg'
        suffix=' -path_file_ls=/triple' if name=='file' else (' -path_file_ls=/' if name=='ls' else '')
        result=ok(f'rep -name={name} -path={reportdir}/{name}.{ext} -id=471A'+suffix)
        with urllib.request.urlopen(urllib.request.Request(URL+result['reports'][0]['url'],headers={'X-MIA-Session':TOKEN})) as response:
            check(response.read()==(reportdir/f'{name}.{ext}').read_bytes(),'reporte accesible API '+name)
        if name in ['inode','block','tree','disk','mbr']:
            ok(f'rep -name={name} -path={reportdir}/{name}.dot -id=471A')
    for name,count in [('bm_inode',sb.inodes),('bm_block',sb.blocks)]:
        lines=(reportdir/(name+'.txt')).read_text().splitlines()
        check(sum(len(x.split()) for x in lines)==count and all(len(x.split())==20 for x in lines[:-1]),'bitmap completo 20 por linea '+name)
    check((reportdir/'file.txt').read_text().split('Contenido:\n',1)[1]==pattern(20000),'reporte file completo')
    tree=(reportdir/'tree.dot').read_text();check('APUNTADORES' in tree and 'i_atime' in tree,'tree indirectos y fechas')
    # fuerza todas las ramas del triple y luego libera todo
    ok('mkfile -path=/maximo -size=280320')
    check(cat('/maximo')==pattern(280320),'maximo 4380 bloques exactos')
    bad('mkfile -path=/maximo -size=280321')
    check(cat('/maximo')==pattern(280320),'rechazo tamaño conserva archivo')
    ok('mkfile -path=/maximo -size=1');ok('Y')
    check(cat('/maximo')=='0','liberacion de todos los niveles')
    integrity(disk,start)
    result=api('  # comentario exacto\n\nMoUnTeD\nmkfile -path="sin cerrar\n=mal\nmkfile -path=/x -size=abc\nmounted\n')
    check(result['messages'][:2]==['  # comentario exacto',''],'comentarios y lineas vacias')
    check(result['stats']=={'successfulCommands':2,'lexicalErrors':1,'syntaxErrors':1},'contadores exactos y recuperacion')
    check(bad('mounted;')['stats']['lexicalErrors']==1,'simbolo lexico invalido')
    check(bad('mkfile -path="/x"-size=1')['stats']['syntaxErrors']==1,'parametros requieren espacios')
    # parametros desconocidos, repetidos y sin valor en todos los comandos
    valid={'mkdisk':'-size=1 -path=/tmp/no.mia','rmdisk':f'-path={BASE}/ausente.mia','fdisk':f'-size=1 -path={disk} -name=no','mount':f'-path={disk} -name=primaria','mounted':'','mkfs':'-id=471A','cat':'-file1=/cont','login':'-user=root -pass=123 -id=471A','logout':'','mkgrp':'-name=nuevo','rmgrp':'-name=nuevo','mkusr':'-user=prueba -pass=123 -grp=Grupo','rmusr':'-user=nuevo','chgrp':'-user=nuevo -grp=Grupo','mkfile':'-path=/prueba','mkdir':'-path=/prueba','rep':f'-name=sb -path={BASE}/sb.svg -id=471A'}
    for command,params in valid.items():
        bad(command+' '+params+' -desconocido=1')
        if params:
            first=params.split()[0]
            bad(command+' '+params+' '+first)
            bad(command+' '+first.split('=')[0])
    ok('logout');bad('logout')
    for cmd in ['cat -file1=/users.txt','mkfile -path=/x','mkdir -path=/x','mkgrp -name=x','rmgrp -name=x','mkusr -user=x -pass=x -grp=x','rmusr -user=x','chgrp -user=x -grp=x']:
        bad(cmd)
    # ajustes en huecos primarios; el ajuste del disco determina la ubicacion
    for fit in ['FF','BF','WF']:
        path=BASE/(fit+'.mia');ok(f'mkdisk -size=2 -fit={fit} -path={path}')
        for name,size in [('a',300),('b',100),('c',200),('d',100)]:ok(f'fdisk -path={path} -name={name} -size={size}')
        before=read(path,MBR)
        bad(f'fdisk -path={path} -name=quinta -size=1')
        ok(f'fdisk -path={path} -name=a -delete=full');ok(f'fdisk -path={path} -name=c -delete=full')
        ok(f'fdisk -path={path} -name=nueva -size=150')
        after=read(path,MBR);part=next(p for p in after.partitions if p.name==b'nueva')
        expected={'FF':before.partitions[0].start,'BF':before.partitions[2].start,'WF':before.partitions[3].start+before.partitions[3].size}[fit]
        check(part.start==expected,'ajuste primario '+fit)
        ok(f'fdisk -path={path} -name=nueva -add=-10');ok(f'fdisk -path={path} -name=nueva -add=10')
        bad(f'fdisk -path={path} -name=nueva -add=9223372036854775807 -unit=M')
        bad(f'fdisk -path={path} -name=nueva -add=-150')
        ok(f'mount -path={path} -name=nueva')
        bad(f'fdisk -path={path} -name=nueva -delete=full')
    # cadena logica: hueco inicial, intermedio y reutilizacion
    path=BASE/'logicas.mia';ok(f'mkdisk -size=4 -path={path}')
    bad(f'fdisk -path={path} -name=sinext -type=L -size=1')
    ok(f'fdisk -path={path} -name=ext -type=E -size=3000')
    bad(f'fdisk -path={path} -name=ext2 -type=E -size=1')
    for name,size in [('l1',300),('l2',100),('l3',200),('l4',100)]:ok(f'fdisk -path={path} -name={name} -type=L -size={size}')
    ext=read(path,MBR).partitions[0];first=read(path,EBR,ext.start);second=read(path,EBR,first.next);third=read(path,EBR,second.next)
    ok(f'fdisk -path={path} -name=l1 -delete=full');ok(f'fdisk -path={path} -name=l3 -delete=full')
    ok(f'fdisk -path={path} -name=bf -type=L -fit=BF -size=150')
    check(read(path,EBR,second.next).name==b'bf','BF logico reutiliza hueco intermedio')
    ok(f'fdisk -path={path} -name=ff -type=L -fit=FF -size=150')
    check(read(path,EBR,ext.start).name==b'ff','FF reutiliza EBR inicial')
    ok(f'fdisk -path={path} -name=wf -type=L -fit=WF -size=150')
    ok(f'fdisk -path={path} -name=wf -add=10');ok(f'fdisk -path={path} -name=wf -add=-10')
    bad(f'fdisk -path={path} -name=ext -add=-2900')
    current=ext.start;last=-1
    while current!=-1:
        ebr=read(path,EBR,current)
        check(current>last and ebr.start==current+c.sizeof(EBR) and ebr.start+ebr.size<=ext.start+ext.size,'cadena ebr valida')
        last=current;current=ebr.next
    # falta de espacio no destruye archivo anterior ni deja bloques huerfanos
    tiny=BASE/'tiny.mia';ok(f'mkdisk -size=4 -unit=K -path={tiny}');ok(f'fdisk -size=2 -path={tiny} -name=p');ok(f'mount -path={tiny} -name=p')
    mountid=read(tiny,MBR).partitions[0].id.decode();ok('mkfs -id='+mountid);ok('login -user=root -pass=123 -id='+mountid)
    ok('mkfile -path=/a -size=64');ok('mkfile -path=/a -size=20000');bad('y')
    check(cat('/a')==pattern(64),'sin espacio conserva contenido')
    integrity(tiny,read(tiny,MBR).partitions[0].start)
    ok('logout');ok(f'rmdisk -path={tiny}');bad(f'rmdisk -path={tiny}')
    check(not tiny.exists() and mountid not in ok('mounted')['messages'][0],'rmdisk limpia montajes')
    destination=ROOT/'tests'/'evidence';destination.mkdir(exist_ok=True)
    for file in reportdir.iterdir():shutil.copy2(file,destination/file.name)

if __name__=='__main__':
    log=(BASE/'server.log').open('w')
    server=subprocess.Popen([str(ROOT/'backend/build/202405047_ext2_backend')],env={**os.environ,'MIA_PORT':str(PORT),'MIA_DISK_DIR':str(BASE),'MIA_REPORT_DIR':str(BASE),'MIA_INPUT_DIR':str(BASE)},stdout=log,stderr=log)
    status='failed'
    try:
        for attempt in range(60):
            if server.poll() is not None: raise RuntimeError((BASE/'server.log').read_text())
            try:
                with urllib.request.urlopen(URL+'/api/health',timeout=1): break
            except OSError: time.sleep(.1)
        main();status='passed'
        print(f'PASS: {len(checks)} verificaciones; evidencia en tests/evidence; discos limpios en {BASE}')
    finally:
        server.terminate()
        try: server.wait(timeout=5)
        except subprocess.TimeoutExpired: server.kill();server.wait()
        log.close()
        (ROOT/'tests/RESULTADOS.json').write_text(json.dumps({'status':status,'checks':len(checks),'base':str(BASE),'abi':{'MBR':c.sizeof(MBR),'Partition':c.sizeof(Partition),'EBR':c.sizeof(EBR),'SuperBlock':c.sizeof(SB),'Inode':c.sizeof(Inode)},'verified':checks},ensure_ascii=False,indent=2))
