#!/usr/bin/env python3
#
# Created: 09:08:2026 - 00:06:00
# Last updated: 09:08:2026 - 13:19:00
# File: tools/header_check.py
#
# Responsibility:
# - Validate the file header and UPD contract across all project source files.
# - Ensures every tracked file has Created/Last updated timestamps and UPD entries.
#
# Dependencies:
# - Uses: Python stdlib only.
# - Used by: CI, pre-commit hook (tools/hooks/pre-commit).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
#
# UPD:
# - 09:08:2026 - 00:06:00: Ported from Quicky Engine; skip lists adapted to the
#                          Daggerfall N layout (games/daggerfall_n assets, model
#                          weights, compiled shaders, voice manifests).
# - 09:08:2026 - 00:40:00: Skip any build*/ directory (per-agent build dirs) and
#                          _deps (FetchContent checkouts) by prefix, not exact name.
# - 09:08:2026 - 13:18:30: Exempt feature_requests.md (user-authored wishlist, no
#                          agent header contract).
# - 09:08:2026 - 13:19:00: Exempt settings.cfg (runtime-generated user graphics
#                          settings, also gitignored).

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TS_RE = re.compile(r"^\d{2}:\d{2}:\d{4} - \d{2}:\d{2}:\d{2}$")
CREATED_RE = re.compile(r"Created:\s*(?P<ts>\d{2}:\d{2}:\d{4}\s*-\s*\d{2}:\d{2}:\d{2})")
UPDATED_RE = re.compile(r"Last updated:\s*(?P<ts>\d{2}:\d{2}:\d{4}\s*-\s*\d{2}:\d{2}:\d{2})")
UPD_ENTRY_RE = re.compile(
    r"^\s*(?:[#*]+)?\s*-\s*(?P<ts>\d{2}:\d{2}:\d{4}\s*-\s*\d{2}:\d{2}:\d{2})\s*:\s+.+"
)

# Directory names excluded anywhere in the tree:
# - Build/tooling output (.git, build*, ...).
# - third_party: vendored dependencies — never hand-edited.
# - __pycache__/.venv: Python artifacts.
SKIP_DIRS = {".git", "target", "node_modules", "dist", ".vite", ".cursor",
             ".claude", "third_party", "__pycache__", ".venv", "_deps",
             "screenshots"}
# Any path part starting with one of these prefixes is skipped (covers the
# per-agent build dirs: build_lead/, build_core/, build_render/, build_sim/...).
SKIP_DIR_PREFIXES = ("build", ".build")


def _skip_parts(parts: tuple[str, ...]) -> bool:
    for p in parts:
        if p in SKIP_DIRS or p.startswith(SKIP_DIR_PREFIXES):
            return True
    return False
# Regexes (posix, relative to root) marking generated/runtime data — not source code.
SKIP_PATH_RES = (
    re.compile(r"^games/[^/]+/(assets|save|logs|screenshots)/"),
    re.compile(r"^tests/golden/"),
)
# Extensions excluded because the format cannot carry a leading source-header comment
# (binary, strict JSON, model weights) or is generated output.
SKIP_EXTENSIONS = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".wav", ".mp3",
                   ".ogg", ".opus", ".ttf", ".otf", ".woff", ".woff2", ".dll", ".so",
                   ".dylib", ".exe", ".lock", ".json", ".toml", ".ini", ".pyc",
                   ".gguf", ".onnx", ".bin", ".glb", ".gltf", ".fbx", ".ozz",
                   ".sc", ".sh.bin", ".dfw", ".dfs"}
# feature_requests.md: the user's personal wishlist — user-authored, never
# agent-edited, exempt from the agent header contract.
SKIP_FILENAMES = {".gitignore", ".gitattributes", "LICENSE", "varying.def.sc",
                  "feature_requests.md", "settings.cfg"}


def read_head(path: Path, max_lines: int = 80) -> list[str]:
    lines: list[str] = []
    try:
        with path.open("r", encoding="utf-8", errors="replace") as f:
            for _ in range(max_lines):
                line = f.readline()
                if not line:
                    break
                lines.append(line.rstrip("\n"))
    except Exception:
        pass
    return lines


def check_file(path: Path) -> list[str]:
    errs: list[str] = []

    if path.suffix.lower() in SKIP_EXTENSIONS:
        return errs
    if path.name in SKIP_FILENAMES:
        return errs

    lines = read_head(path)
    if not lines:
        return errs

    created = None
    updated = None
    upd_entries = 0
    last_upd_ts = None

    for line in lines:
        if created is None:
            m = CREATED_RE.search(line)
            if m:
                created = m.group("ts").strip()
                continue
        if updated is None:
            m = UPDATED_RE.search(line)
            if m:
                updated = m.group("ts").strip()
                continue
        m = UPD_ENTRY_RE.match(line)
        if m:
            upd_entries += 1
            last_upd_ts = m.group("ts").strip()

    if not created:
        errs.append("Missing 'Created:' timestamp in header.")
    if not updated:
        errs.append("Missing 'Last updated:' timestamp in header.")
    if upd_entries == 0:
        errs.append("Missing UPD entries.")
    if updated and last_upd_ts and updated != last_upd_ts:
        errs.append(f"'Last updated' ({updated}) does not match last UPD entry ({last_upd_ts}).")

    return errs


def scan_directory(root: Path) -> list[tuple[Path, list[str]]]:
    failures: list[tuple[Path, list[str]]] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(root)
        if _skip_parts(rel.parts):
            continue
        rel_posix = rel.as_posix()
        if any(rx.match(rel_posix) for rx in SKIP_PATH_RES):
            continue
        errs = check_file(path)
        if errs:
            failures.append((path.relative_to(root), errs))
    return failures


def main() -> int:
    ap = argparse.ArgumentParser(description="Check Daggerfall N header/UPD contract.")
    ap.add_argument("--root", default=".", help="Project root directory.")
    ap.add_argument("--all", action="store_true",
                    help="Scan the whole project tree (default behaviour; accepted for "
                         "compatibility with the documented command).")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    failures = scan_directory(root)

    if failures:
        print("Header/UPD contract violations:", file=sys.stderr)
        for rel, errs in failures:
            print(f"  {rel}", file=sys.stderr)
            for e in errs:
                print(f"    - {e}", file=sys.stderr)
        return 1

    print("OK: all files pass header/UPD check.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
