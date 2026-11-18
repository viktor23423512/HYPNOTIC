"""Build projectM's unmodified MIT HLSL translator with project-local tools."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[2];t=Path(__file__).resolve().parent
source=t.parent/'milkdrop_spike/projectm-4.1.8/vendor/hlslparser/src';sdk=r.parent/'OpenOrbis/PS4Toolchain'
llvm=Path('C:/Program Files/LLVM/bin');out=t/'hlsl-orbis';out.mkdir(exist_ok=True)
units=[source/(name+'.cpp') for name in ['CodeWriter','Engine','GLSLGenerator','HLSLParser','HLSLTokenizer','HLSLTree']]
with (t/'hlsl-build.log').open('w') as log:
    objects=[]
    for unit in units:
        obj=out/(unit.stem+'.o');objects.append(str(obj))
        subprocess.run([str(llvm/'clang++.exe'),'--target=x86_64-pc-freebsd12-elf','-std=c++17','-D_GNU_SOURCE','-D_XOPEN_SOURCE=700','-O2','-fPIC','-ffunction-sections','-fdata-sections','-isystem',str(sdk/'include'),'-isystem',str(sdk/'include/c++/v1'),'-I'+str(source),'-c',str(unit),'-o',str(obj)],stdout=log,stderr=log,check=True)
    subprocess.run([str(llvm/'llvm-ar.exe'),'rcs',str(out/'libhlslparser.a'),*objects],stdout=log,stderr=log,check=True)
print('PASS PS4 HLSL translator library build; vendor sources unchanged.')
