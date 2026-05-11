@echo off
setlocal

set "SLANGC_EXE=%SLANGC%"
if "%SLANGC_EXE%"=="" set "SLANGC_EXE=slangc"

set "SHADER_DIR=%~dp0slang"
set "OUTPUT_DIR=%~dp0spirv"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo Compiling shaders...

call :compile raytrace.rgen || exit /b 1
call :compile raytrace.rmiss || exit /b 1
call :compile raytrace.rchit || exit /b 1
call :compile raytrace.rahit || exit /b 1
call :compile compute       || exit /b 1

echo Done
exit /b 0

:get_ts
set "TS="
for /f "skip=1 tokens=1 delims=." %%A in ('wmic datafile where "name='%~1'" get LastModified 2^>nul') do (
  if not "%%A"=="" (
    set "TS=%%A"
    goto :eof
  )
)
goto :eof

:compile
set "NAME=%~1"
set "SRC=%SHADER_DIR%\%NAME%.slang"
set "DST=%OUTPUT_DIR%\%NAME%.spv"

if not exist "%SRC%" (
  echo Shader source not found: "%SRC%"
  exit /b 1
)

REM
set "NEEDS_BUILD=0"
if not exist "%DST%" (
  set "NEEDS_BUILD=1"
) else (
  set "SRC_WMIC=%SRC:\=\\%"
  set "DST_WMIC=%DST:\=\\%"
  call :get_ts "%SRC_WMIC%"
  set "SRC_TS=%TS%"
  call :get_ts "%DST_WMIC%"
  set "DST_TS=%TS%"
  if not "%SRC_TS%"=="" if not "%DST_TS%"=="" (
    if "%SRC_TS%" GTR "%DST_TS%" set "NEEDS_BUILD=1"
  ) else (
    set "NEEDS_BUILD=1"
  )
)

if "%NEEDS_BUILD%"=="1" (
  echo - %NAME%
  "%SLANGC_EXE%" -I "%SHADER_DIR%" "%SRC%" -target spirv -profile spirv_1_6 -matrix-layout-column-major -fvk-use-scalar-layout -capability spvShaderClockKHR -o "%DST%"
  if errorlevel 1 (
    echo Failed to compile %NAME%
    exit /b 1
  )
)
exit /b 0

