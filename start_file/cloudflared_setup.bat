@echo off
chcp 65001 >nul
echo Cloudflare cloudflared のインストールを開始します...

"%LOCALAPPDATA%\Microsoft\WindowsApps\winget.exe" install Cloudflare.cloudflared

if %ERRORLEVEL% equ 0 (
    echo.
    echo インストールが正常に完了しました。
) else (
    echo.
    echo インストール中にエラーが発生しました。(エラーコード: %ERRORLEVEL%)
)

pause