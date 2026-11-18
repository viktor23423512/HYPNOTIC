"""Reproducible local PS4 compiler build; never installs into the SDK."""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT/'opengnm-stack/opengnm-psbc'
PYTHON=ROOT/'build-tools/venv/Scripts/python.exe'
DEVKIT=ROOT/'build-tools/w64devkit/bin'
SDK=ROOT.parents[2]/'OpenOrbis/PS4Toolchain'
LLVM=Path('C:/Program Files/LLVM/bin')

def generate(script,args,output,stdout=False):
    command=[str(PYTHON),script,*args]
    if stdout:
        with (SOURCE/output).open('wb') as f:
            subprocess.run(command,cwd=SOURCE,stdout=f,check=True)
    else:
        subprocess.run([*command,output],cwd=SOURCE,check=True)

def main():
    # Upstream's standalone Makefile omits some Mesa-generated dependencies.
    for filename,args in [('u_format_gen.h',['--enums']),('u_format_pack.h',['--header']),('u_format_table.c',[])]:
        generate('src/util/format/u_format_table.py',['src/util/format/u_format.yaml',*args],'src/util/format/'+filename,True)
    for kind in ('h','c'):
        generate(f'src/compiler/builtin_types_{kind}.py',[],f'src/compiler/builtin_types.{kind}')
    generate('src/util/process_shader_stats.py',['src/util/shader_stats.rnc','src/util/shader_stats.xml'],'src/util/shader_stats.h',True)
    generate('src/vulkan/util/vk_struct_type_cast_gen.py',['--xml','src/vulkan/registry/vk.xml','--beta','false','--out'],'src/vulkan/util/vk_struct_type_cast.h')
    packet_inputs=[]
    for gfx in ('gfx11','gfx12'):
        packet_inputs += [f'src/amd/packets/cp_pm4_table_data_{gfx}.json',f'src/amd/packets/pm4_it_opcodes_{gfx}.h']
    for gfx in ('gfx11','gfx12'):
        generate('src/amd/packets/parse_cp_pm4_table_data_json.py',[*packet_inputs,gfx,'packets_h'],f'src/amd/common/amd_cp_packets_{gfx}.h',True)
    generate('src/util/format_srgb.py',[],'src/util/format_srgb.c',True)
    generate('src/amd/common/gfx10_format_table.py',['src/util/format/u_format.yaml','src/amd/registers/gfx10-rsrc.json','src/amd/registers/gfx11-rsrc.json'],'src/amd/common/gfx10_format_table.c',True)
    env=os.environ.copy()
    env['OO_PS4_TOOLCHAIN']=SDK.as_posix()
    env['PATH']=os.pathsep.join([str(LLVM),str(PYTHON.parent),str(DEVKIT),env['PATH']])
    command=[str(DEVKIT/'make.exe'),'-f','Makefile.orbis','-j','8',
             'PYTHON='+PYTHON.as_posix(),'SHELL='+(DEVKIT/'sh.exe').as_posix()]
    print('Building PS4 compiler; log:',ROOT/'psbc-build.log',flush=True)
    with (ROOT/'psbc-build.log').open('wb') as log:
        result=subprocess.run(command,cwd=SOURCE,env=env,stdout=log,stderr=subprocess.STDOUT)
    print('Compiler build exit:',result.returncode,flush=True)
    return result.returncode

if __name__=='__main__':sys.exit(main())
