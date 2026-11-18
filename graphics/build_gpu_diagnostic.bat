@echo off
setlocal
set HYPNOTIC_GPU_DIAGNOSTIC=1
call build.bat obj graphics ..
exit /b %errorlevel%
