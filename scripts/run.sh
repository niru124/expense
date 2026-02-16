#!/bin/bash

# Check if gnuplot is installed (required for sciplot graphs)
if ! command -v gnuplot &>/dev/null; then
    echo "Installing gnuplot (required for yearly expense graphs)..."
    sudo apt-get update && sudo apt-get install -y gnuplot-x11
fi

# Function to clean up background processes on exit
cleanup() {
    echo "Stopping background processes..."
    kill $(jobs -p) 2>/dev/null
}

trap cleanup EXIT

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 1. Build the C++ backend
echo "Building C++ backend..."
cd "$PROJECT_DIR"
cmake -B build && cmake --build build

if [ $? -ne 0 ]; then
    echo "C++ backend build failed. Exiting."
    exit 1
fi

# 2. Start the C++ backend in the background
echo "Starting C++ backend..."
./build/expense &
CPP_PID=$!
echo "C++ backend running with PID: $CPP_PID"

# Give the C++ backend a moment to start up
sleep 2

# 3. Start the Python frontend server in the background
echo "Starting Python frontend server..."
python3 backend_server.py &
PYTHON_PID=$!
echo "Python frontend server running with PID: $PYTHON_PID"

echo ""
echo "Frontend is now accessible at: http://localhost:8000"
echo "Press Ctrl+C to stop both servers."

# Keep the script running so background processes don't exit immediately
wait $CPP_PID $PYTHON_PID
