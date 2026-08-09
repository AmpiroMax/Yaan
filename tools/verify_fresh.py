#!/usr/bin/env python3
#
# Created: 09:08:2026 - 21:09:10
# Last updated: 09:08:2026 - 21:09:10
# File: tools/verify_fresh.py
#
# Responsibility:
# - Refuse to let a verification claim rest on an artifact that was never built.
#   Absence and staleness both present as NEUTRAL states: a missing binary is
#   not a failing test, an old binary is not a failing test, and nothing in the
#   toolchain distinguishes "this passed" from "this was never asked". This
#   makes the unasked question loud.
#
# Dependencies:
# - Uses: Python stdlib only.
# - Used by: every agent before claiming a suite green (--build) or a frame
#   verified (--app, Rule 27).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
# - It compares against the newest source TREE-WIDE rather than per target, so
#   it OVER-reports. That direction is deliberate for a safety guard: do not
#   "fix" it into precision and quietly reintroduce the hole.
#
# UPD:
# - 09:08:2026 - 21:09:10: Created by core after four incidents in one day that
#                          shared one shape - the verification machinery
#                          reporting success for a question it never asked.
#                          Lives in tools/ (lead zone) so every agent finds it.
"""Refuse to let a verification claim rest on an artifact that was never built.

WHY THIS EXISTS
---------------
Four separate incidents in one day, all the same failure wearing different
clothes: THE VERIFICATION MACHINERY REPORTED SUCCESS FOR A QUESTION IT NEVER
ASKED.

  1. A `--target foo` build left test binaries un-relinked, so ctest measured
     old code and reported green.
  2. ctest reports a test that fails to COMPILE as "Not Run", which reads like
     a missing test rather than a broken one.
  3. A build directory configured with the wrong toolchain could not produce
     the app AT ALL, so a zone "verifying by Rule 27" had only ever been
     running unit tests -- for its entire existence, silently.
  4. A peer's zone breaking the build leaves every downstream binary stale
     while ctest happily runs yesterday's executables and prints PASS.

The common shape is that ABSENCE and STALENESS both present as neutral states.
A missing binary is not a failing test; an old binary is not a failing test.
Nothing in the toolchain distinguishes "this passed" from "this was not asked".

WHAT IT CHECKS
--------------
  * every ctest-registered test has an executable that EXISTS
  * every such executable is NEWER than the newest source file
  * the app binary exists and is newer than the newest source, when --app is
    given (required before any claim about a rendered frame)
  * the build directory's compiler matches the expected toolchain

Exit code is non-zero when any check fails, so it composes into a script or a
pre-verification hook.

USAGE
-----
    python3 tools/verify_fresh.py --build build_core
    python3 tools/verify_fresh.py --build build_core --app   # before a frame claim
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# Only things that actually feed the compiler. Scripts under tools/ are
# deliberately excluded: this file being newer than every binary would
# otherwise make the guard fail on itself, which is a false alarm and the
# fastest way to teach people to ignore it.
SOURCE_DIRS = ("engine", "tests")
SOURCE_SUFFIXES = (".cpp", ".h", ".hpp", ".c", ".sc", ".cmake")
EXPECTED_CXX = "/opt/homebrew/opt/llvm/bin/clang++"


def newest_source(root: Path) -> tuple[float, Path | None]:
    """Newest mtime across tracked sources, and which file it was."""
    best, best_path = 0.0, None
    for d in SOURCE_DIRS:
        base = root / d
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            m = path.stat().st_mtime
            if m > best:
                best, best_path = m, path
    # CMakeLists.txt files change what gets built at all.
    for path in root.rglob("CMakeLists.txt"):
        if "build" in path.parts[0:1] or not path.is_file():
            continue
        m = path.stat().st_mtime
        if m > best:
            best, best_path = m, path
    return best, best_path


def registered_tests(build: Path) -> list[str]:
    try:
        out = subprocess.run(
            ["ctest", "--test-dir", str(build), "-N"],
            capture_output=True, text=True, check=True,
        ).stdout
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(f"FAIL: could not enumerate tests: {exc}", file=sys.stderr)
        return []
    return re.findall(r"Test\s+#\d+:\s+(\S+)", out)


def find_binary(build: Path, name: str) -> Path | None:
    direct = build / "tests" / name
    if direct.is_file():
        return direct
    for path in build.rglob(name):
        if path.is_file() and os.access(path, os.X_OK):
            return path
    return None


def check_toolchain(build: Path) -> list[str]:
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return [f"{build}/CMakeCache.txt missing -- build dir is not configured"]
    text = cache.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"CMAKE_CXX_COMPILER:[^=]*=(.*)", text)
    if not m:
        return ["CMAKE_CXX_COMPILER not set in the cache"]
    actual = m.group(1).strip()
    if actual != EXPECTED_CXX:
        return [
            f"toolchain is {actual}, expected {EXPECTED_CXX}. "
            "A non-primary toolchain may be unable to build the app at all, "
            "which makes every 'verified in a frame' claim from this tree void."
        ]
    if not re.search(r"CMAKE_BUILD_TYPE:[^=]*=\s*\S", text):
        return ["CMAKE_BUILD_TYPE is EMPTY -- no optimisation; timings from "
                "this tree are meaningless (cost us a day once already)"]
    return []


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", default="build_core")
    ap.add_argument("--app", action="store_true",
                    help="also require a fresh app binary (before any frame claim)")
    ap.add_argument("--root", default=".")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    build = (root / args.build).resolve()
    problems = check_toolchain(build)

    newest, newest_path = newest_source(root)
    if newest_path is None:
        print("FAIL: found no source files to compare against", file=sys.stderr)
        return 2
    print(f"newest source: {newest_path.relative_to(root)}")

    names = registered_tests(build)
    if not names:
        problems.append("ctest registered ZERO tests -- a suite that does not "
                        "exist cannot fail, which is the trap this tool exists for")

    missing, stale = [], []
    for name in names:
        binary = find_binary(build, name)
        if binary is None:
            missing.append(name)
        elif binary.stat().st_mtime < newest:
            stale.append(name)

    if missing:
        problems.append(
            "test binaries MISSING (ctest would report these 'Not Run', which "
            f"reads as absent rather than broken): {', '.join(sorted(missing))}")
    if stale:
        problems.append(
            "test binaries OLDER than sources -- ctest would measure old code "
            f"and print PASS: {', '.join(sorted(stale))}")

    if args.app:
        app = find_binary(build, "dfn_app")
        if app is None:
            problems.append("dfn_app does NOT EXIST: no frame claim can be made "
                            "from this tree")
        elif app.stat().st_mtime < newest:
            problems.append("dfn_app is STALE: a frame shot now shows the world "
                            "as of an older build, and nothing on screen says so")

    if problems:
        print("\nNOT SAFE TO CLAIM VERIFICATION:\n", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1

    scope = "tests + app" if args.app else "tests"
    print(f"OK: {len(names)} test binaries present and newer than sources ({scope}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
