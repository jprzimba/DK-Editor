@echo off
setlocal
set ROOT=%~dp0
set EXE=%ROOT%build\Release\DKEditor.exe
if not exist "%EXE%" (
  echo DKEditor.exe not found. Run build.bat first.
  exit /b 1
)
if "%~1"=="" (
  start "" "%EXE%"
) else (
  start "" "%EXE%" "%~1" %2
)
