@echo off
setlocal
cd /d "%~dp0"

set "PYVER=3.13.7"
set "ZIP=python-%PYVER%-embed-amd64.zip"
set "URL=https://www.python.org/ftp/python/%PYVER%/%ZIP%"

if not exist "runtime" mkdir "runtime"
if not exist "runtime\python.exe" (
  echo Downloading CPython %PYVER% Windows x64 embeddable runtime...
  curl -fL "%URL%" -o "%ZIP%"
  if errorlevel 1 exit /b 1
  powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory((Resolve-Path '%ZIP%').Path, (Resolve-Path 'runtime').Path)"
  if errorlevel 1 exit /b 1
  del "%ZIP%"
)
if not exist "runtime\python.exe" (
  echo CPython extraction failed: runtime\python.exe was not created.
  exit /b 1
)

copy /y "host.py" "runtime\pyhost.py" >nul
if errorlevel 1 exit /b 1

if not exist "shentrypak.exe" (
  echo Copy the updated shentrypak.exe into this folder first.
  exit /b 1
)

shentrypak.exe "main.shay" -o "pyinit.shpak" --resource-dir "runtime"
if errorlevel 1 exit /b 1

set "ENTRY_HOME=%USERPROFILE%"
if defined HOME (
  for /f "delims=" %%P in ('cygpath -w "%HOME%" 2^>nul') do set "ENTRY_HOME=%%P"
)
if not defined ENTRY_HOME set "ENTRY_HOME=%USERPROFILE%"
if not exist "%ENTRY_HOME%\.shay\entries" mkdir "%ENTRY_HOME%\.shay\entries"
copy /y "pyinit.shpak" "%ENTRY_HOME%\.shay\entries\pyinit.shpak" >nul
if errorlevel 1 exit /b 1

echo.
echo Built and installed pyinit.shpak.
echo You can now run Shay programs that use $useentry pyinit.
