#!/usr/bin/env python3
"""Reject non-system external Mach-O references in the shipped app."""
import os
import pathlib
import re
import subprocess
import sys

app = pathlib.Path(sys.argv[1])
executables = app / "Contents/MacOS"


def rpaths(path):
    output = subprocess.check_output(["otool", "-l", str(path)], text=True)
    return re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", output)


main_rpaths = rpaths(executables / "netgreeting") + rpaths(executables / "greetingpost")
for path in app.rglob("*"):
    if not path.is_file() or path.is_symlink():
        continue
    kind = subprocess.check_output(["file", "-b", str(path)], text=True)
    if "Mach-O" not in kind:
        continue
    lines = subprocess.check_output(["otool", "-L", str(path)], text=True).splitlines()
    for line in lines:
        # Universal binaries have a separate unindented header per architecture.
        if not line.startswith("\t"):
            continue
        dependency = line.strip().split(" (", 1)[0]
        if dependency.startswith("/") and not dependency.startswith(("/usr/lib/", "/System/Library/")):
            raise SystemExit(f"External dependency: {path}: {dependency}")
        if dependency.startswith("@"):
            def expand(value):
                return pathlib.Path(value.replace("@loader_path", str(path.parent))
                                    .replace("@executable_path", str(executables)))
            if dependency.startswith("@rpath/"):
                candidates = [expand(prefix) / dependency[len("@rpath/"):]
                              for prefix in rpaths(path) + main_rpaths]
            else:
                candidates = [expand(dependency)]
            if not any(candidate.is_file() and candidate.resolve().is_relative_to(app.resolve())
                       for candidate in candidates):
                raise SystemExit(f"Missing bundled dependency: {path}: {dependency}")
env = {key: value for key, value in os.environ.items()
       if not key.startswith(("QT_", "DYLD_"))}
for name in ("netgreeting", "greetingpost"):
    subprocess.run([str(app / "Contents/MacOS" / name), "--version"],
                   env=env, check=True, timeout=30)
