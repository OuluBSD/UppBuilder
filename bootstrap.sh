#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

CXX="${CXX:-}"
if [ -z "${CXX}" ]; then
    if command -v g++ >/dev/null 2>&1; then
        CXX="g++"
    elif command -v clang++ >/dev/null 2>&1; then
        CXX="clang++"
    else
        echo "Error: Neither g++ nor clang++ was found in PATH." >&2
        exit 1
    fi
fi

OUT_BIN="${SCRIPT_DIR}/uppbuilder"

echo "==> UppBuilder: Compiling standalone binary with ${CXX}..."
"${CXX}" -std=c++17 -O2 -Wall \
    UppBuilder.cpp \
    Bridge.cpp \
    UppParser.cpp \
    DepGraph.cpp \
    HeaderScanner.cpp \
    UmkDriver.cpp \
    RuntimeDeps.cpp \
    FlagAuditor.cpp \
    UseModel.cpp \
    PolicyEngine.cpp \
    EscBuildParser.cpp \
    ProfileManager.cpp \
    ProviderPlanner.cpp \
    OverlayManager.cpp \
    EmergeEngine.cpp \
    VdbManager.cpp \
    main.cpp \
    -lpthread \
    -o "${OUT_BIN}"

echo "==> UppBuilder: Successfully built ${OUT_BIN}"
"${OUT_BIN}" --version
