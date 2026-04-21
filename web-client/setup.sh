#!/bin/bash

echo "FreeRDP Web Client Setup"
echo "========================"
echo

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo "ERROR: Node.js is not installed!"
    echo "Please install Node.js from https://nodejs.org/"
    echo "Or use your package manager:"
    echo "  Ubuntu/Debian: sudo apt install nodejs npm"
    echo "  macOS: brew install node"
    echo "  CentOS/RHEL: sudo yum install nodejs npm"
    exit 1
fi

echo "Node.js is installed. Version: $(node --version)"
echo "npm version: $(npm --version)"
echo

# Check if we're in the right directory
if [ ! -f "package.json" ]; then
    echo "ERROR: package.json not found!"
    echo "Please run this script from the web-client directory"
    exit 1
fi

echo "Installing dependencies..."
npm install

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install dependencies!"
    exit 1
fi

echo
echo "Setup complete! You can now start the web proxy server."
echo
echo "To start the server, run:"
echo "  npm start"
echo
echo "Or with custom options:"
echo "  node server.js --port=8080 --width=1920 --height=1080 --fps=60"
echo
echo "Then start the FreeRDP shadow server in another terminal:"
echo "  freerdp-shadow /port:3389 /monitors:0 /size:1920x1080"
echo
echo "For web access, open your browser to:"
echo "  http://localhost:8080"
echo