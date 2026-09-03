#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
g++ -std=c++17 -Wall -Wextra -pedantic \
    "$SCRIPT_DIR/src/CPU_Scheduling_Simulator.cpp" \
    -o "$SCRIPT_DIR/CPU_Scheduling_Simulator"
"$SCRIPT_DIR/CPU_Scheduling_Simulator"
