#!/bin/bash

# QuantumLiquidity macOS .app Build Script
# Run this on macOS to build the desktop app

set -e

echo "🚀 Building QuantumLiquidity.app..."

# Navigate to desktop directory
cd desktop

# Install dependencies
echo "📦 Installing npm dependencies..."
npm install

# Build Tauri app
echo "⚙️ Building Tauri macOS app..."
npm run tauri build --target universal-apple-darwin#

# Get the app path
APP_PATH="src-tauri/target/release/bundle/macos/QuantumLiquidity.app"

if [ -d "$APP_PATH" ]; then
    echo "✅ Build successful!"
    echo ""
    echo "📦 App location: $APP_PATH"
    echo ""
    echo "📝 Next steps:"
    echo "1. cp -r $APP_PATH /Applications/"
    echo "2. Open /Applications/QuantumLiquidity.app"
    echo ""
    echo "🎯 Or zip for distribution:"
    echo "   cd src-tauri/target/release/bundle/macos"
    echo "   zip -r QuantumLiquidity.app.zip QuantumLiquidity.app"
    echo ""
else
    echo "❌ Build failed! App not found at: $APP_PATH"
    exit 1
fi
