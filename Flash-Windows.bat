@echo off
cd /d "%~dp0"
py -3 -m venv .flash-env
if errorlevel 1 goto end
.flash-env\Scripts\python.exe -m pip install esptool==5.1.0
if errorlevel 1 goto end
.flash-env\Scripts\python.exe flash.py
:end
pause
