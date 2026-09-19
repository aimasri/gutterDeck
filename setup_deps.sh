#!/bin/bash
set -e

echo "Updating package lists..."
sudo apt-get update

echo "Installing C++ and Build Tools..."
sudo apt-get install -y build-essential cmake g++ pkg-config

echo "Installing Qt6 Development Packages..."
sudo apt-get install -y qt6-base-dev qt6-tools-dev qt6-tools-dev-tools libqt6core5compat6-dev qt6-l10n-tools

echo "Installing XCB Development Packages..."
sudo apt-get install -y libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev libxcb-keysyms1-dev libxcb-util-dev libxcb-res0-dev

echo "Installing Sandbox Tools (Xephyr and Openbox)..."
sudo apt-get install -y xserver-xephyr openbox

echo "Dependencies installed successfully!"
