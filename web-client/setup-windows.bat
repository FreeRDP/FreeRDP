@echo off
echo FreeRDP Web Client Setup for Windows
echo ====================================
echo.

REM Check if Node.js is installed
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Node.js is not installed!
    echo Please download and install Node.js from https://nodejs.org/
    echo Then run this script again.
    pause
    exit /b 1
)

echo Node.js is installed. Version:
node --version
echo.

REM Check if npm is available
npm --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: npm is not available!
    pause
    exit /b 1
)

echo Installing dependencies...
cd web-client
npm install

if %errorlevel% neq 0 (
    echo ERROR: Failed to install dependencies!
    pause
    exit /b 1
)

echo.
echo Setup complete! You can now start the web proxy server.
echo.
echo To start the server, run:
echo   npm start
echo.
echo Or with custom options:
echo   node server.js --port=8080 --width=1920 --height=1080 --fps=60
echo.
echo Then start the FreeRDP shadow server in another terminal:
echo   freerdp-shadow /port:3389 /monitors:0 /size:1920x1080
echo.
pause