#!/usr/bin/env bash
# Build installers for whatever OS this machine is.
#   Linux  -> .deb and .rpm (rpm if rpmbuild is installed)
#   Windows (Git Bash / CI) -> .exe installer and .zip
#   macOS  -> .dmg
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build-package"
DIST="${ROOT}/dist"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

mkdir -p "${DIST}"

if [[ "$(uname -s)" == "Linux" ]]; then
  if [[ -f "${ROOT}/resources/netgreeting.png" && ! -f "${ROOT}/packaging/netgreeting.ico" ]]; then
    if command -v ffmpeg >/dev/null 2>&1; then
      ffmpeg -y -i "${ROOT}/resources/netgreeting.png" -vf scale=256:256 \
        "${ROOT}/packaging/netgreeting.ico" >/dev/null 2>&1 || true
    fi
  fi
fi

cmake -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD}" -j"${JOBS}"

cd "${BUILD}"

os="$(uname -s)"
case "${os}" in
  Linux)
    echo "Building Linux packages (.deb, and .rpm if possible)..."
    cpack -G DEB
    if command -v rpmbuild >/dev/null 2>&1; then
      cpack -G RPM || echo "RPM packaging skipped (rpmbuild failed)."
    else
      echo "RPM packaging skipped (install rpm-build to produce .rpm)."
    fi
    ;;
  Darwin)
    echo "Building macOS disk image (.dmg)..."
    cpack -G DragNDrop
    ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    echo "Building Windows installer (.exe) and zip..."
    cpack -G NSIS || echo "NSIS installer skipped (install NSIS)."
    cpack -G ZIP
    ;;
  *)
    echo "Unknown OS ${os}. Running default cpack."
    cpack
    ;;
esac

echo
echo "Packages are in: ${DIST}"
ls -lh "${DIST}"
echo
if ls "${DIST}"/*.deb >/dev/null 2>&1; then
  echo "Install the Debian/Ubuntu package with:"
  echo "  sudo apt install ${DIST}/*.deb"
fi
if ls "${DIST}"/*.rpm >/dev/null 2>&1; then
  echo "Install the Fedora/RHEL package with:"
  echo "  sudo dnf install ${DIST}/*.rpm"
fi
if ls "${DIST}"/*.exe >/dev/null 2>&1; then
  echo "Windows: run the .exe installer."
fi
if ls "${DIST}"/*.dmg >/dev/null 2>&1; then
  echo "Mac: open the .dmg and drag NetGreeting to Applications."
fi
