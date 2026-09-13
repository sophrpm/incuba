#!/usr/bin/env python3
"""prepara rutas de prueba sin modificar scripts originales ni borrar discos."""
import argparse
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--base',default='/tmp/mia-p1-demo')
args=parser.parse_args()
base=Path(args.base).resolve()
base.mkdir(parents=True,exist_ok=True)
(base/'contenido real.txt').write_text('Contenido real importado desde la computadora.\nSegunda linea de prueba.\n')
source=Path(__file__).resolve().parent
out=source/'generated';out.mkdir(exist_ok=True)
for script in source.glob('*.smia'):
    (out/script.name).write_text(script.read_text().replace('/tmp/mia-p1-demo',str(base)))
print(f'Scripts preparados en {out}. Use un backend nuevo y una carpeta sin principal.mia: {base}')
