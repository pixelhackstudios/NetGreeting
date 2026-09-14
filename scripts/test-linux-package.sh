#!/usr/bin/env bash
# Runs inside a fresh distribution container; never install build-time Qt here.
set -euo pipefail
if command -v apt-get >/dev/null; then
  apt-get update -qq
  apt-get install -y /work/dist/*.deb xvfb xauth python3
  if dpkg-query -W 'libqt6*' 2>/dev/null | grep -q .; then
    echo 'Unexpected system Qt installation' >&2
    exit 1
  fi
else
  dnf install -y /work/dist/*.rpm xorg-x11-server-Xvfb xorg-x11-xauth python3
  if rpm -qa | grep -q '^qt6-'; then
    echo 'Unexpected system Qt installation' >&2
    exit 1
  fi
fi
unset LD_LIBRARY_PATH QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
python3 - <<'PY'
import pathlib, runpy
bundle = runpy.run_path('/work/scripts/bundle-linux.py')
base = pathlib.Path('/usr/lib/netgreeting')
for path in list((base / 'bin').glob('*')) + list((base / 'lib').glob('*')) + list((base / 'plugins').rglob('*.so')):
    if path.name == 'qt.conf':
        continue
    for dep in bundle['dependencies'](path):
        if not bundle['GLIBC'].match(dep.name) and not dep.is_relative_to(base):
            raise SystemExit(f'Unbundled dependency: {path}: {dep}')
PY
greetingpost --version
QT_QPA_PLATFORM=offscreen netgreeting --version
# Exercise the actual XCB plugin as well as a call/chat exchange.
xvfb-run -a bash -c '
  set -euo pipefail
  netgreeting --name PackageTest --port 21720 --auto-accept >/tmp/netgreeting.log 2>&1 &
  app=$!
  trap "kill $app 2>/dev/null || true" EXIT
  python3 - <<"PY"
import socket, time
for attempt in range(50):
    try:
        socket.create_connection(("127.0.0.1", 21720), timeout=0.2).close()
        break
    except OSError:
        time.sleep(0.1)
else:
    raise SystemExit("Packaged GUI did not start")
PY
  python3 /work/tests/smoke_call.py 127.0.0.1 21720
' || { cat /tmp/netgreeting.log; exit 1; }
