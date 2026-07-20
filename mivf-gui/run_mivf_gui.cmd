@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
"%SCRIPT_DIR%.venv\Scripts\python.exe" -m mivf_gui
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
