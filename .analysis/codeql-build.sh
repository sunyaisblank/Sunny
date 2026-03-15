#!/bin/bash
# CodeQL build script — called by `codeql database create --command`
# Uses a separate .codeql-build directory to ensure a clean build
# that CodeQL can trace from scratch.
set -e
cmake -B .codeql-build -DCMAKE_BUILD_TYPE=Release
cmake --build .codeql-build -j"${1:-$(nproc)}"
