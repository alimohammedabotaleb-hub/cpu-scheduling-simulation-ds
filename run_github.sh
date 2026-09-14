#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"
mkdir -p build
g++ -std=c++17 -Wall -Wextra -pedantic \
    src/CPU_Scheduling_Simulator.cpp -o build/CPU_Scheduling_Simulator
./build/CPU_Scheduling_Simulator "$@"
