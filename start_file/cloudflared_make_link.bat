
@echo off
cd /d "%~dp0"

echo ===== Checking Python =====
py --version
if errorlevel 1 (
    echo Python launcher was not found.
    pause
    exit /b 1
)

echo ===== Checking cloudflared =====
where cloudflared
if errorlevel 1 (
    echo cloudflared was not found.
    pause
    exit /b 1
)

echo ===== Starting Cloudflare Tunnel =====
cloudflared tunnel --url http://localhost:5000 2>&1 | py -u "%~dp0make_link.py"

echo ===== Process finished =====
pause
