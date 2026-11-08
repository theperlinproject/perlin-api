@echo off
setlocal EnableExtensions
set "here=%~dp0"
set "target=wasm64"
if /i "%~1"=="--wasm32" (
    set "target=wasm32"
    shift
) else if /i "%~1"=="--wasm64" (
    shift
)
set "src=%~1"
if "%src%"=="" (
    echo usage: build.cmd [--wasm32^|--wasm64] ^<plugin.cpp^> [outdir]
    exit /b 2
)
set "out=%~2"
if "%out%"=="" set "out=build"
for %%I in ("%src%") do (
    set "name=%%~nI"
    set "srcdir=%%~dpI"
)
if not exist "%out%" mkdir "%out%"
for %%I in ("%out%") do set "outfull=%%~fI\"
set "gen=%outfull%gen\%name%"
if not exist "%gen%" mkdir "%gen%"
set "manifest=%srcdir%%name%.json"
if exist "%manifest%" (
    where python >nul 2>&1 && (set "py=python") || (set "py=py -3")
    %py% "%here%tools\perlin_settings.py" "%manifest%" "%gen%\mod_settings.hpp" || exit /b 1
    if /i not "%srcdir%"=="%outfull%" copy /y "%manifest%" "%outfull%%name%.json" >nul
)
if "%target%"=="wasm32" goto wasm32

set "emxx=em++"
where em++ >nul 2>&1
if not errorlevel 1 goto haveem
if defined EMSDK if exist "%EMSDK%\upstream\emscripten\em++.bat" (
    set "emxx=%EMSDK%\upstream\emscripten\em++.bat"
    goto haveem
)
if defined EMSDK if exist "%EMSDK%\upstream\emscripten\em++.exe" (
    set "emxx=%EMSDK%\upstream\emscripten\em++.exe"
    goto haveem
)
echo em++ not found: run emsdk_env.bat first, or build with --wasm32 and wasi-sdk 1>&2
exit /b 1
:haveem
call "%emxx%" -std=c++23 -O2 -fno-exceptions -fno-rtti -I"%here%sdk" -I"%srcdir%." -I"%gen%" -Wall -Wextra "%src%" -o "%outfull%%name%.wasm" ^
    -sMEMORY64=1 -sSTANDALONE_WASM=1 -sPURE_WASI=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=1MB -sMAXIMUM_MEMORY=1GB -sSTACK_SIZE=256KB ^
    -sERROR_ON_UNDEFINED_SYMBOLS=0 --no-entry ^
    "-sEXPORTED_FUNCTIONS=_perlin_alloc,_perlin_free,_perlin_event,_perlin_subscriptions,_perlin_api_revision"
if errorlevel 1 exit /b 1
goto done

:wasm32
if not defined WASI_SDK_PATH (
    echo set WASI_SDK_PATH to a wasi-sdk install, or drop --wasm32 to build with Emscripten 1>&2
    exit /b 1
)
"%WASI_SDK_PATH%\bin\clang++.exe" --sysroot="%WASI_SDK_PATH%\share\wasi-sysroot" --target=wasm32-wasip1 -std=c++23 -O2 -fno-exceptions -fno-rtti ^
    -mexec-model=reactor -I"%here%sdk" -I"%srcdir%." -I"%gen%" -Wall -Wextra "%src%" -o "%outfull%%name%.wasm" ^
    "-Wl,--export=perlin_api_revision" "-Wl,--export=perlin_alloc" "-Wl,--export=perlin_free" "-Wl,--export=perlin_event" ^
    "-Wl,--export=perlin_subscriptions" "-Wl,--export=perlin_subscriptions_hi" -Wl,--strip-all
if errorlevel 1 exit /b 1

:done
echo %outfull%%name%.wasm (%target%)
exit /b 0
