@echo off
chcp 65001 >nul
set "VSLANG=1033"
set "NOVA_VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
call "%NOVA_VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
set "CMAKE_GENERATOR_PLATFORM="
set "PATH=%NOVA_VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%NOVA_VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Git\cmd;%PATH%"
cd /d "%~dp0.."
if "%~1"=="" (cmd /k) else (%*)
