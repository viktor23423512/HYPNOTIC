SETLOCAL EnableDelayedExpansion

Rem Resolve the copied SDK beside this project; leave the toolchain unchanged.
if not defined OO_PS4_TOOLCHAIN for %%I in ("%~dp0..\..\OpenOrbis\PS4Toolchain") do set "OO_PS4_TOOLCHAIN=%%~fI"
if not exist "%OO_PS4_TOOLCHAIN%\bin\windows\create-fself.exe" exit /b 1

Rem Allow PkgTool (netcoreapp3.0) to use an installed newer runtime.
set DOTNET_ROLL_FORWARD=Major

set PKG_TITLE="HYPNOTIC"
set PKG_VERSION="0.77"
set PKG_ASSETS="assets"
set PKG_TITLE_ID="HYPN00001"
set PKG_CONTENT_ID="IV0000-HYPN00001_00-HYPNOTIC00000000"

Rem Libraries to link in
set libraries=-lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceAudioOut -lSceAudioIn -lScePigletv2VSH -lScePrecompiledShaders -lSceSysmodule -lSceSystemService -lSceGnmDriver

Rem Optimize the CPU framebuffer loops and drawing calls.
set extra_flags=-O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -ffunction-sections -fdata-sections -I../tests/milkdrop_spike/projectm-4.1.8/vendor/hlslparser/src -I../tests/milkdrop_ps4/projectm-eval/projectm-eval/api -I../tests/milkdrop_spike/projectm-4.1.8/src/libprojectM/MilkdropPreset -I../tests/milkdrop_ps4/opengnm-stack/opengnm-psbc/libpsbc -I../tests/milkdrop_ps4/opengnm-stack/opengnm/include -I../tests/milkdrop_ps4/glslang
if "%HYPNOTIC_GPU_DIAGNOSTIC%"=="1" set extra_flags=%extra_flags% -DHYPNOTIC_GPU_DIAGNOSTIC

Rem Read the script arguments into local vars
set intdir=%1
set targetname=%~2
set outputPath=%3

set outputElf=%intdir%\%targetname%.elf
set outputOelf=%intdir%\%targetname%.oelf

@if not exist %intdir% mkdir %intdir%

python ../tools/update_milkdrop_assets.py
if errorlevel 1 exit /b 1

Rem Compile object files for all the source files
for %%f in (*.c) do (
    clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\\include" -I"%OO_PS4_TOOLCHAIN%\\include\\c++\\v1" %extra_flags% -c -o %intdir%\%%~nf.o %%~nf.c
    if errorlevel 1 exit /b 1
)

for %%f in (*.cpp) do (
    clang++ --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\\include" -I"%OO_PS4_TOOLCHAIN%\\include\\c++\\v1" %extra_flags% -c -o %intdir%\%%~nf.o %%~nf.cpp
    if errorlevel 1 exit /b 1
)

for %%f in (..\common\*.c) do (
    clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\\include" -I"%OO_PS4_TOOLCHAIN%\\include\\c++\\v1" %extra_flags% -c -o %intdir%\%%~nf.o ..\common\%%~nf.c
    if errorlevel 1 exit /b 1
)

for %%f in (..\common\*.cpp) do (
    clang++ --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I"%OO_PS4_TOOLCHAIN%\\include" -I"%OO_PS4_TOOLCHAIN%\\include\\c++\\v1" %extra_flags% -c -o %intdir%\%%~nf.o ..\common\%%~nf.cpp
    if errorlevel 1 exit /b 1
)

Rem Build the project-local libjbc helpers; never install into the toolchain.
for %%f in (jailbreak kernelrw) do (
    clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -ffreestanding -mno-red-zone -mgeneral-regs-only -I"%OO_PS4_TOOLCHAIN%\include" -c -o %intdir%\jbc_%%f.o third_party\libjbc\%%f.c
    if errorlevel 1 exit /b 1
)

Rem Get a list of object files for linking
set obj_files=
for %%f in (%1\\*.o) do set obj_files=!obj_files! .\%%f

if "%HYPNOTIC_GPU_DIAGNOSTIC%"=="1" (
    set obj_files=.\%intdir%\gpu_diagnostic.o .\%intdir%\gpu_scene.o
    set libraries=-lc -lkernel -lc++ -lSceVideoOut -lScePigletv2VSH -lScePrecompiledShaders -lSceSysmodule -lSceSystemService -lSceGnmDriver
)

Rem Link the input ELF
ld.lld -m elf_x86_64 -pie --script "hypnotic-link.x" --eh-frame-hdr --gc-sections -o "%outputElf%" "-L%OO_PS4_TOOLCHAIN%\\lib" %libraries% --verbose "%OO_PS4_TOOLCHAIN%\lib\crt1.o" %obj_files% "../tests/milkdrop_ps4/opengnm-stack/opengnm-psbc/libpsbc.orbis.a" "../tests/milkdrop_ps4/opengnm-stack/opengnm/libopengnm.a" "../tests/milkdrop_ps4/eval-orbis/libprojectm-eval.a" "../tests/milkdrop_ps4/hlsl-orbis/libhlslparser.a" "../tests/milkdrop_ps4/glslang-orbis/glslang/libglslang.a" "../tests/milkdrop_ps4/glslang-orbis/glslang/libglslang-default-resource-limits.a" "../tests/milkdrop_ps4/glslang-orbis/glslang/OSDependent/Unix/libOSDependent.a"
if errorlevel 1 exit /b 1

set sys_authinfo="000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

Rem Create the eboot
%OO_PS4_TOOLCHAIN%\bin\windows\create-fself.exe -in "%outputElf%" --out "%outputOelf%" --eboot "eboot.bin" --paid 0x3800000000000035 --authinfo %sys_authinfo%
if errorlevel 1 exit /b 1

Rem Eboot cleanup
copy "eboot.bin" %outputPath%\eboot.bin
if errorlevel 1 exit /b 1
del "eboot.bin"

Rem Create param.sfo
cd ..
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_new sce_sys/param.sfo
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value %PKG_VERSION%
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "gde"
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value %PKG_CONTENT_ID%
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value %PKG_TITLE%
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value %PKG_TITLE_ID%
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%\bin\windows\PkgTool.Core.exe sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value %PKG_VERSION%
if errorlevel 1 exit /b 1

Rem Local asset inventory supports categorized MilkDrop folders without cmd length limits.
python tools/create_player_gp4.py "%OO_PS4_TOOLCHAIN%/bin/windows/create-gp4.exe" %PKG_CONTENT_ID%
if errorlevel 1 exit /b 1
python tests/milkdrop_ps4/fix_gp4_dirs.py pkg.gp4
if errorlevel 1 exit /b 1
%OO_PS4_TOOLCHAIN%/bin/windows/PkgTool.Core.exe pkg_build pkg.gp4 .
if errorlevel 1 exit /b 1
