#!/bin/bash

# Build and run script for CplusplusPiano

set -e  # Exit on error

echo "🔨 Building CplusplusPiano..."

# Run qmake
qmake CplusplusPiano.pro

# Build the project
make

echo "✅ Build complete!"
echo "🎹 Launching Virtual Piano..."

# Run the application
./build/CplusplusPiano.app/Contents/MacOS/CplusplusPiano

