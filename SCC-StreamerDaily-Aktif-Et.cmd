@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Activate-StreamerDaily.ps1"
if errorlevel 1 pause
