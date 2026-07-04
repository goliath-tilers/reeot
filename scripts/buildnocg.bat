@echo off
cd /d "%~dp0"

echo [1/3] Configuring...
cmake --preset win-amd64-relwithdebinfo
if errorlevel 1 ( echo. & echo [FAILED] Configure step failed. & pause & exit /b 1 )
echo [1/3] Configure OK

echo.
echo [2/3] Reconfiguring (pick up generated sources)...
cmake --preset win-amd64-relwithdebinfo
if errorlevel 1 ( echo. & echo [FAILED] Reconfigure step failed. & pause & exit /b 1 )
echo [2/3] Reconfigure OK

echo.
echo [3/3] Building...
cmake --build --preset win-amd64-relwithdebinfo -j
if errorlevel 1 ( echo. & echo [FAILED] Build step failed. & pause & exit /b 1 )
echo [3/3] Build OK

echo.
echo Build complete.
pause
