#!/usr/bin/env bash
# Build NetGreeting (installing packages if needed) and launch it.
# Extra arguments are passed through to the app, e.g. ./run.sh --port 1721 --name Alice
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"
BIN="${BUILD}/netgreeting"
POST="${BUILD}/greetingpost"

need_install=0

have() { command -v "$1" >/dev/null 2>&1; }

pkg_ok() {
  have pkg-config && pkg-config --exists "$1"
}

if ! have g++ && ! have clang++; then need_install=1; fi
if ! have cmake; then need_install=1; fi
if ! have pkg-config; then need_install=1; fi
if ! pkg_ok Qt6Widgets || ! pkg_ok Qt6Network; then need_install=1; fi
if ! pkg_ok libpulse-simple; then need_install=1; fi

install_deps() {
  local pkgs=(
    build-essential
    cmake
    pkg-config
    qt6-base-dev
    libpulse-dev
  )

  if have apt-get; then
    echo "Installing build dependencies: ${pkgs[*]}"
    if [[ "$(id -u)" -eq 0 ]]; then
      apt-get update -y
      DEBIAN_FRONTEND=noninteractive apt-get install -y "${pkgs[@]}"
    elif have sudo; then
      sudo apt-get update -y
      sudo DEBIAN_FRONTEND=noninteractive apt-get install -y "${pkgs[@]}"
    else
      echo "Missing packages and no sudo. Install: ${pkgs[*]}" >&2
      exit 1
    fi
    return
  fi

  echo "No apt-get on this system. Install a C++ compiler, cmake, pkg-config, Qt 6 Widgets/Network, and libpulse-simple, then re-run." >&2
  exit 1
}

if [[ "${need_install}" -eq 1 ]]; then
  install_deps
fi

cmake -S "${ROOT}" -B "${BUILD}" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
cmake --build "${BUILD}" -j"$(nproc 2>/dev/null || echo 4)"

if [[ "${1:-}" == "post" || "${1:-}" == "--post" ]]; then
  shift
  exec "${POST}" "$@"
fi

exec "${BIN}" "$@"
