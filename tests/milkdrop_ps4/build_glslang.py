"""Build GLSL front end for PS4 using only project-local tools/configuration."""
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parent
cmake=ROOT/'build-tools/w64devkit/bin/cmake.exe'
configure=[str(cmake),'-S',str(ROOT/'glslang'),'-B',str(ROOT/'glslang-orbis-relocated'),'-G','Ninja',
    '-DCMAKE_TOOLCHAIN_FILE='+(ROOT/'openorbis-glslang.cmake').as_posix(),
    '-DCMAKE_MAKE_PROGRAM='+(ROOT/'build-tools/venv/Scripts/ninja.exe').as_posix(),
    '-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=OFF','-DENABLE_GLSLANG_BINARIES=OFF',
    '-DENABLE_HLSL=OFF','-DENABLE_OPT=OFF','-DENABLE_PCH=OFF','-DGLSLANG_ENABLE_INSTALL=OFF']
with (ROOT/'glslang-configure.log').open('wb') as output:
    rc=subprocess.run(configure,stdout=output,stderr=subprocess.STDOUT).returncode
if rc:sys.exit(rc)
with (ROOT/'glslang-build.log').open('wb') as output:
    rc=subprocess.run([str(cmake),'--build',str(ROOT/'glslang-orbis-relocated'),'--parallel','8'],stdout=output,stderr=subprocess.STDOUT).returncode
sys.exit(rc)
