#!/bin/bash

# Configuration
SANDBOX_DISPLAY=":1"
SANDBOX_RES="1920x1080"

# Kill any existing Xephyr instances on this display
pkill -f "Xephyr $SANDBOX_DISPLAY" || true
sleep 0.5

echo "Starting Xephyr on DISPLAY=$SANDBOX_DISPLAY with resolution $SANDBOX_RES..."
Xephyr $SANDBOX_DISPLAY -ac -screen $SANDBOX_RES -resizeable &
XEPHYR_PID=$!

# Wait for Xephyr to initialize
sleep 1

echo "Starting Openbox inside the sandbox..."
DISPLAY=$SANDBOX_DISPLAY openbox &
OPENBOX_PID=$!

echo "Sandbox is ready."
echo "To run Gutter Deck in the sandbox, use: DISPLAY=$SANDBOX_DISPLAY ./build/gutterdeck"

# Wait for Xephyr to close
wait $XEPHYR_PID

# Clean up openbox when Xephyr closes
kill $OPENBOX_PID || true
echo "Sandbox closed."
