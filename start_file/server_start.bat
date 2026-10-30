@echo off
cd /d "%~dp0..\nfc-projects"

echo ===== Checking Python =====
py --version

echo ===== Starting Flask =====
py -u "app.py"

echo ===== Exit code: %errorlevel% =====
pause