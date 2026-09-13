@echo off
setlocal
python "%~dp0flash.py" %*
if errorlevel 1 exit /b %errorlevel%
