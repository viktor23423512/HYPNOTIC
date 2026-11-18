"""Build pinned projectM-Eval with existing tools for PS4 and host checks."""
from pathlib import Path
import json
import subprocess
import sys
from eval_optimizations import generate

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT/'projectm-eval'
SDK=ROOT.parents[2]/'OpenOrbis/PS4Toolchain'
LLVM=Path('C:/Program Files/LLVM/bin')
DEVKIT=ROOT/'build-tools/w64devkit/bin'
PARSER=ROOT.parent/'milkdrop_spike/projectm-4.1.8/src/libprojectM/MilkdropPreset'
units=sorted((SOURCE/'projectm-eval').glob('*.c'))+[SOURCE/'projectm-eval/api/projectm-eval.c']
includes=['-I'+str(SOURCE),'-I'+str(SOURCE/'projectm-eval'),'-I'+str(SOURCE/'projectm-eval/api')]
OUT=ROOT/'eval-orbis'
OUT.mkdir(exist_ok=True)
optimized_tree=generate(SOURCE/'projectm-eval/TreeFunctions.c',OUT/'generated/TreeFunctions.c')
units=[optimized_tree if source.name=='TreeFunctions.c' else source for source in units]
objects=[]
with (ROOT/'eval-build.log').open('w',encoding='utf-8') as log:
    for i,source in enumerate(units):
        obj=OUT/(str(i)+'-'+source.stem+'.o')
        subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-freebsd12-elf','-std=c11','-O3','-DNDEBUG','-fPIC',
            '-ffunction-sections','-fdata-sections','-D_GNU_SOURCE','-DPRJM_F_SIZE=8',
            '-isystem',str(SDK/'include'),*includes,'-c',str(source),'-o',str(obj)],stdout=log,stderr=log,check=True)
        objects.append(str(obj))
    subprocess.run([str(LLVM/'llvm-ar.exe'),'rcs',str(OUT/'libprojectm-eval.a'),*objects],stdout=log,stderr=log,check=True)
    host=ROOT/'eval-host'
    host.mkdir(exist_ok=True)
    host_objects=[]
    for i,source in enumerate(units):
        obj=host/(str(i)+'-'+source.stem+'.o')
        subprocess.run([str(DEVKIT/'gcc.exe'),'-std=c11','-O3','-DNDEBUG','-D_GNU_SOURCE','-DPRJM_F_SIZE=8',
            *includes,'-c',str(source),'-o',str(obj)],stdout=log,stderr=log,check=True)
        host_objects.append(str(obj))
    exe=host/'equation-checks.exe'
    subprocess.run([str(DEVKIT/'g++.exe'),'-std=c++17','-O2','-static',*includes,
        '-I'+str(PARSER),'-I'+str(ROOT/'native_graphics'),
        str(ROOT/'equations-test.cpp'),str(ROOT/'native_graphics/preset_equations.cpp'),
        str(PARSER/'PresetFileParser.cpp'),*host_objects,'-o',str(exe)],stdout=log,stderr=log,check=True)
    result=subprocess.run([str(exe),str(ROOT/'assets/misc/equations-smoke.milk')],capture_output=True,text=True)
    log.write(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end='')
    if result.returncode:sys.exit(result.returncode)
    exe=host/'geometry-checks.exe'
    subprocess.run([str(DEVKIT/'g++.exe'),'-std=c++17','-O2','-static',*includes,
        '-I'+str(PARSER),'-I'+str(ROOT/'native_graphics'),
        str(ROOT/'geometry-test.cpp'),str(ROOT/'native_graphics/preset_equations.cpp'),
        str(PARSER/'PresetFileParser.cpp'),*host_objects,'-o',str(exe)],stdout=log,stderr=log,check=True)
    result=subprocess.run([str(exe),str(ROOT/'assets/misc/geometry-smoke.milk')],capture_output=True,text=True)
    log.write(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end='')
    if result.returncode:sys.exit(result.returncode)
    exe=host/'audio-checks.exe'
    subprocess.run([str(DEVKIT/'g++.exe'),'-std=c++17','-O2','-static',*includes,
        '-I'+str(PARSER),'-I'+str(ROOT/'native_graphics'),
        str(ROOT/'audio-test.cpp'),str(ROOT/'native_graphics/preset_equations.cpp'),
        str(PARSER/'PresetFileParser.cpp'),*host_objects,'-o',str(exe)],stdout=log,stderr=log,check=True)
    result=subprocess.run([str(exe),str(ROOT/'assets/audio/audio-smoke.wav'),str(ROOT/'assets/misc/geometry-smoke.milk')],capture_output=True,text=True)
    log.write(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end='')
    if result.returncode:sys.exit(result.returncode)
    exe=host/'community-checks.exe'
    subprocess.run([str(DEVKIT/'g++.exe'),'-std=c++17','-O2','-static',*includes,
        '-I'+str(ROOT/'community-reference'),'-I'+str(PARSER),'-I'+str(PARSER.parent),'-I'+str(ROOT/'native_graphics'),
        str(ROOT/'community-test.cpp'),str(ROOT/'native_graphics/preset_equations.cpp'),str(PARSER/'PresetFileParser.cpp'),
        str(PARSER/'Waveforms/WaveformMath.cpp'),str(PARSER/'Waveforms/CenteredSpiro.cpp'),str(PARSER/'Waveforms/XYOscillationSpiral.cpp'),str(PARSER.parent/'Audio/Loudness.cpp'),
        *host_objects,'-o',str(exe)],stdout=log,stderr=log,check=True)
    result=subprocess.run([str(exe),*[str(ROOT/('assets/misc/community-'+slug+'.milk')) for slug in ['shake','bass-zoom','microcosm','volume-zoom','constant-velocity','starfish']]],capture_output=True,text=True)
    log.write(result.stdout+result.stderr);print(result.stdout+result.stderr,end='')
    if result.returncode:sys.exit(result.returncode)
    exe=host/'pipeline-checks.exe'
    subprocess.run([str(DEVKIT/'g++.exe'),'-std=c++17','-O2','-static',*includes,
        '-I'+str(ROOT/'community-reference'),'-I'+str(PARSER),'-I'+str(PARSER.parent),'-I'+str(ROOT/'native_graphics'),
        str(ROOT/'pipeline-test.cpp'),str(ROOT/'native_graphics/preset_equations.cpp'),str(PARSER/'PresetFileParser.cpp'),
        str(PARSER/'Waveforms/WaveformMath.cpp'),str(PARSER/'Waveforms/CenteredSpiro.cpp'),str(PARSER/'Waveforms/XYOscillationSpiral.cpp'),
        *host_objects,'-o',str(exe)],stdout=log,stderr=log,check=True)
    result=subprocess.run([str(exe),str(ROOT/'assets/misc/community-cauldron.milk'),str(ROOT/'assets/misc/community-brain-zoom.milk')],capture_output=True,text=True)
    log.write(result.stdout+result.stderr);print(result.stdout+result.stderr,end='')
    if result.returncode:sys.exit(result.returncode)
print('PS4 equation library and equation/geometry/audio/community host checks passed.')
