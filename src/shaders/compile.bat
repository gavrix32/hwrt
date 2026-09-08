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

:compile
set "NAME=%~1"
set "SRC=%SHADER_DIR%\%NAME%.slang"
set "DST=%OUTPUT_DIR%\%NAME%.spv"

if not exist "%SRC%" (
  echo Shader source not found: "%SRC%"
  exit /b 1
)

if not exist "%DST%" goto :build

REM Pass paths through the environment, so spaces and quotes stay literal.
REM Exit codes: 0 = up to date, 1 = rebuild, 2 = timestamp check failed.
powershell.exe -NoLogo -NoProfile -NonInteractive -Command "$ErrorActionPreference = 'Stop'; try { if ((Get-Item -LiteralPath $env:SRC).LastWriteTimeUtc -gt (Get-Item -LiteralPath $env:DST).LastWriteTimeUtc) { exit 1 }; exit 0 } catch { Write-Error $_ -ErrorAction Continue; exit 2 }"
if errorlevel 2 (
  echo Failed to check timestamps for %NAME%
  exit /b 1
)
if not errorlevel 1 exit /b 0

:build
echo - %NAME%
"%SLANGC_EXE%" -I "%SHADER_DIR%" "%SRC%" -target spirv -profile spirv_1_6 -matrix-layout-column-major -fvk-use-scalar-layout -capability spvShaderClockKHR -o "%DST%"
if not "%errorlevel%"=="0" (
  echo Failed to compile %NAME%
  exit /b 1
)
exit /b 0

