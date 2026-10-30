@echo off
chcp 65001 > nul
echo ==========================================
echo  Flask インストールバッチ (UTF-8 対応版)
echo ==========================================
echo.

:: 1. 通常の pip コマンドを試行
echo [1/3] 'pip' コマンドでインストールを試みています...
where pip >nul 2>nul
if %errorlevel% equ 0 (
    pip install flask
    if %errorlevel% equ 0 goto SUCCESS
)
echo -> 'pip' コマンドが利用できないか、失敗しました。
echo.

:: 2. python -m pip を試行 (環境変数パスが通っていない場合用)
echo [2/3] 'python -m pip' でインストールを試みています...
where python >nul 2>nul
if %errorlevel% equ 0 (
    python -m pip install flask
    if %errorlevel% equ 0 goto SUCCESS
)
echo -> 'python' コマンドが利用できないか、失敗しました。
echo.

:: 3. py ランチャーを試行 (Windows標準のPythonランチャー用)
echo [3/3] 'py -m pip' でインストールを試みています...
where py >nul 2>nul
if %errorlevel% equ 0 (
    py -m pip install flask
    if %errorlevel% equ 0 goto SUCCESS
)

:ERROR
echo.
echo ------------------------------------------
echo 【エラー】Flask のインストールに失敗しました。
echo ------------------------------------------
echo 原因として以下が考えられます:
echo 1. Python がPCにインストールされていない
echo 2. インストール時に「Add Python to PATH」にチェックを入れていない
echo 3. インターネット接続がない
echo.
pause
exit /b 1

:SUCCESS
echo.
echo ------------------------------------------
echo 【成功】Flask のインストールが完了しました！
echo ------------------------------------------
echo.
pause
exit /b 0
