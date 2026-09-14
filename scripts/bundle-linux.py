#!/usr/bin/env python3
"""Bundle the ELF closure of both programs and Qt's runtime plugins.

Only glibc and its loader stay on the host. Drivers, display/audio servers,
fonts and trust stores remain OS facilities. Run only on trusted build outputs.
"""
import pathlib
import re
import shutil
import subprocess
import sys

GLIBC = re.compile(r"^(ld-linux.*|lib(c|m|dl|pthread|rt|resolv|util|anl|nss_.*)\.so\..*)$")
PLUGIN_TYPES = (
    "platforms", "imageformats", "iconengines", "platforminputcontexts",
    "networkinformation", "tls", "xcbglintegrations", "egldeviceintegrations",
    "wayland-decoration-client", "wayland-graphics-integration-client",
    "wayland-shell-integration",
)


def dependencies(path):
    output = subprocess.check_output(["ldd", str(path)], text=True)
    if "not found" in output:
        raise RuntimeError(f"Unresolved dependency of {path}:\n{output}")
    return [pathlib.Path(p) for p in re.findall(r"=> (/\S+)", output)]


def main():
    root, qt_plugins, patchelf = sys.argv[1:]
    root = pathlib.Path(root)
    base = root / "lib/netgreeting"
    lib = base / "lib"
    lib.mkdir(parents=True, exist_ok=True)
    plugins = base / "plugins"
    for category in PLUGIN_TYPES:
        source = pathlib.Path(qt_plugins) / category
        if source.is_dir():
            shutil.copytree(source, plugins / category, dirs_exist_ok=True)
    for required in ("platforms/libqxcb.so", "platforms/libqoffscreen.so"):
        if not (plugins / required).exists():
            raise RuntimeError(f"Missing required Qt plugin: {required}")
    binaries = list((base / "bin").iterdir()) + list(plugins.rglob("*.so"))
    # Qt's OpenSSL backend loads these by name rather than ELF DT_NEEDED.
    cache = subprocess.check_output(["ldconfig", "-p"], text=True)
    pending = list(binaries)
    for name in ("libssl.so.3", "libcrypto.so.3"):
        match = re.search(r"^\s*" + re.escape(name) + r"\s+.*=> (/\S+)", cache, re.M)
        if not match:
            raise RuntimeError(f"Missing TLS runtime: {name}")
        pending.append(pathlib.Path(match[1]))
    seen = set()
    copied = {}
    while pending:
        path = pending.pop()
        if GLIBC.match(path.name) or path in seen:
            continue
        seen.add(path)
        pending.extend(dependencies(path))
        if path not in binaries:
            if path.name in copied and copied[path.name] != path.resolve():
                raise RuntimeError(f"Conflicting runtime libraries: {path.name}")
            copied[path.name] = path.resolve()
    # Resolve the complete graph before populating the RPATH destination, so
    # later ldd calls cannot confuse staged copies with their source libraries.
    for name, source in copied.items():
        shutil.copy2(source, lib / name)
    # Preserve distribution copyright/license notices for the bundled runtime.
    notices = base / "licenses"
    notices.mkdir(exist_ok=True)
    sources = set(copied.values()) | set(pathlib.Path(qt_plugins).rglob("*.so"))
    queries = set()
    for source in sources:
        queries.add(str(source))
        queries.add(str(source).removeprefix("/usr"))
    owners = subprocess.run(["dpkg-query", "-S", *sorted(queries)],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    for line in owners.stdout.splitlines():
        if ": /" not in line:
            continue
        package = line.split(": /", 1)[0].split(":", 1)[0]
        copyright_file = pathlib.Path("/usr/share/doc") / package / "copyright"
        if copyright_file.is_file():
            shutil.copy2(copyright_file, notices / f"{package}.copyright")
    shutil.copytree("/usr/share/common-licenses", notices / "common-licenses", dirs_exist_ok=True)
    (base / "runtime-libraries.txt").write_text("\n".join(sorted(copied)) + "\n")
    for path in binaries + list(lib.iterdir()):
        if path.parent == base / "bin":
            rpath = "$ORIGIN/../lib"
        elif path.is_relative_to(plugins):
            rpath = "$ORIGIN/../../lib"
        else:
            rpath = "$ORIGIN"
        subprocess.run([patchelf, "--set-rpath", rpath, str(path)], check=True)
    (base / "bin/qt.conf").write_text("[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\n")
    (root / "bin").mkdir(exist_ok=True)
    for name in ("netgreeting", "greetingpost"):
        launcher = root / "bin" / name
        launcher.write_text(f'#!/bin/sh\nexec /usr/lib/netgreeting/bin/{name} "$@"\n')
        launcher.chmod(0o755)
    # Validate the staged tree, including dependencies only used by plugins.
    for path in binaries + list(lib.iterdir()):
        for dependency in dependencies(path):
            if not GLIBC.match(dependency.name) and not dependency.is_relative_to(base):
                raise RuntimeError(f"Runtime escaped bundle: {path}: {dependency}")
    print(f"Bundled {len(copied)} libraries and {len(binaries) - 2} Qt plugins")


if __name__ == "__main__":
    main()
