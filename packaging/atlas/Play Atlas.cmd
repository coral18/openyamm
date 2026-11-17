@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup-Content.ps1"
if errorlevel 1 (
    pause
    exit /b 1
)
"%~dp0OpenYAMMAtlas.exe" --world mm7 --asset-root "%~dp0assets" --user-data "%~dp0."
