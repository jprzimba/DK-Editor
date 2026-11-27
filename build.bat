@echo off
setlocal
set ROOT=%~dp0
if not exist "%ROOT%build\CMakeCache.txt" (
  cmake -S "%ROOT%." -B "%ROOT%build" -A x64
  if errorlevel 1 exit /b 1
)
cmake --build "%ROOT%build" --config Release --parallel
if errorlevel 1 exit /b 1
echo.
echo Built:
echo   %ROOT%build\Release\DKEditor.exe
echo   %ROOT%build\Release\DKEditorSelftest.exe
