#!/usr/bin/env python3
#
# Created: 09:08:2026 - 00:06:00
# Last updated: 11:08:2026 - 13:59:10
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
# - 09:08:2026 - 19:42:19: Read the whole leading comment region instead of a
#                          fixed 80-line window: the old window made the
#                          contract depend on how much UPD history a file had,
#                          and reported a mismatch on a correct header once a
#                          doc outgrew it.
#                          settings, also gitignored).
# - 10:08:2026 - 01:58:01: --files mode: the pre-commit hook now checks only the
#                          staged set. Six agents share one working tree, and the
#                          whole-tree gate let any agent's mid-edit file block every
#                          other agent's commit -- design was blocked twice in one
#                          night by files it had never touched.
# - 10:08:2026 - 20:16:01: captures/ и playtest_test_artifacts/ исключены из обхода —
#                          это ВЫВОД прогонов, и они держали --all вечно красным.
# - 10:08:2026 - 22:17:20: В режиме --files проверяется то, чего файл-локальная проверка не может в
#                          принципе: ПОЯВИЛАСЬ ЛИ запись UPD в этом коммите. Всё
#                          остальное — проверка внутренней согласованности, и файл,
#                          отредактированный без записи, проходил её безупречно.
# - 11:08:2026 - 13:31:43: Both gates now share the skip lists. check_file() returned
#                          clean for formats that cannot carry a header, but the
#                          HEAD-comparison gate ran on them anyway and demanded a
#                          UPD entry they had nowhere to put -- .gitignore blocked a
#                          commit while being named in SKIP_FILENAMES. The gate I
#                          added yesterday to catch a wrong-block entry acquired the
#                          same shape of defect it was written to catch: two checks
#                          disagreeing about their own subject (Rule 41).
# - 11:08:2026 - 13:59:10: .log добавлен в исключения: это УЛИКА, а не исходник — вывод пробы,
#                          сложенный рядом с приёмочными кадрами. Его пишет инструмент
#                          побайтно, и дописанный руками заголовок делает файл уже не тем,
#                          что было измерено. Та же причина, по которой пропускаются
#                          captures/ и playtest_test_artifacts/.

from __future__ import annotations

import argparse
import re
import subprocess
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
             # RUN OUTPUT, not source. These are written BY the tools whose
             # evidence discipline this script enforces -- capture sidecars,
             # playtest summaries and incident logs -- and a header comment is
             # meaningless on a file that a program regenerates every run.
             #
             # They were making `--all` permanently red, which is worse than it
             # sounds: a check that is always failing for a reason nobody can
             # fix stops being read, and then the real failure underneath it is
             # invisible. Same failure mode as a test that goes red on correct
             # code (Rule 38), aimed at a tool instead of a suite.
             "screenshots", "captures", "playtest_test_artifacts"}
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
                   ".sc", ".sh.bin", ".dfw", ".dfs",
                   # .log is EVIDENCE, not source: probe output archived beside
                   # the acceptance frames it belongs to. It is written by a
                   # tool, byte for byte, and a hand-added header would make it
                   # no longer the thing that was measured. Same reason
                   # captures/ and playtest_test_artifacts/ are skipped by path.
                   ".log"}
# feature_requests.md: the user's personal wishlist — user-authored, never
# agent-edited, exempt from the agent header contract.
SKIP_FILENAMES = {".gitignore", ".gitattributes", "LICENSE", "varying.def.sc",
                  "feature_requests.md", "settings.cfg"}


# The header contract lives in the leading comment region, whose length grows
# with a file's UPD history. A fixed line window therefore makes the check
# silently depend on how much history a file has accumulated: once the block
# outgrows the window the checker compares 'Last updated' against a TRUNCATED
# list of entries and reports a mismatch on a file whose header is correct.
# That fired once, on a doc with eleven entries, and cost real time to diagnose.
# So: read until the leading comment region ends, with a generous hard cap as a
# backstop for files that have no such region at all.
COMMENT_CLOSERS = ("-->", "*/")
HARD_CAP_LINES = 4000


def read_head(path: Path, max_lines: int = HARD_CAP_LINES) -> list[str]:
    """Lines of the leading comment region (plus a little), never truncated
    mid-history. Stops once the header region is provably over."""
    lines: list[str] = []
    try:
        with path.open("r", encoding="utf-8", errors="replace") as f:
            blocks_closed = 0
            content_lines_after = 0
            for _ in range(max_lines):
                line = f.readline()
                if not line:
                    break
                stripped = line.rstrip("\n")
                lines.append(stripped)
                text = stripped.strip()
                if text.endswith(COMMENT_CLOSERS):
                    blocks_closed += 1
                    content_lines_after = 0
                    continue
                # Files carry up to two leading blocks (header, then UPD). Once
                # both have closed, a run of ordinary content means we are past
                # the header for good.
                if blocks_closed >= 2 and text:
                    content_lines_after += 1
                    if content_lines_after > 20:
                        break
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


def upd_entry_count(text: str) -> int:
    """UPD entries in a blob's leading comment region."""
    n = 0
    for line in text.splitlines()[:HARD_CAP_LINES]:
        if UPD_ENTRY_RE.match(line):
            n += 1
    return n


def head_blob(root: Path, rel: str) -> str | None:
    """The file as it exists in HEAD, or None if it is new / not a git tree."""
    try:
        out = subprocess.run(["git", "show", f"HEAD:{rel}"], cwd=root,
                             capture_output=True, check=False)
    except OSError:
        return None
    if out.returncode != 0:
        return None
    return out.stdout.decode("utf-8", errors="replace")


def check_files(root: Path, rel_paths: list[str]) -> list[tuple[Path, list[str]]]:
    """Checks only the named files (repo-relative). Skip rules still apply, so a
    staged file the scanner would ignore is ignored here too.

    THIS MODE ALSO CHECKS THE THING THE FILE-LOCAL CHECK STRUCTURALLY CANNOT:
    that a file being COMMITTED actually gained a UPD entry. Everything in
    check_file() is an internal-consistency test -- `Last updated` agrees with
    the newest entry -- and a file edited without touching either passes it
    perfectly, because the two fields it compares still agree with each other.
    That happened: an agent published an edit whose UPD entry landed in the
    WRONG BLOCK and whose `Last updated` stayed stale, and this script stayed
    green through both, because the defect was the placement of a THIRD thing
    (Rule 41 -- the instrument measured agreement between two fields while the
    question was about a third).

    Comparing against HEAD is the only way to ask "was this file updated", and
    it is available exactly here, in the hook, where the answer matters.
    """
    failures: list[tuple[Path, list[str]]] = []
    for rel_str in rel_paths:
        path = root / rel_str
        if not path.is_file():
            continue  # deleted in this commit
        rel = path.relative_to(root)
        if _skip_parts(rel.parts):
            continue
        if any(rx.match(rel.as_posix()) for rx in SKIP_PATH_RES):
            continue
        # A format that cannot carry a header cannot gain a UPD entry either.
        # check_file() returns clean for these, but the HEAD comparison below is
        # a SECOND gate and used to run on them anyway -- so .gitignore, a JSON
        # asset or a PNG could be edited and then demanded an entry it has
        # nowhere to put. Same skip lists, both gates (Rule 41: the two gates
        # answer different questions and must agree on their subject).
        if path.suffix.lower() in SKIP_EXTENSIONS or path.name in SKIP_FILENAMES:
            continue
        errs = check_file(path)
        # Did this commit actually add an entry? Only answerable against HEAD.
        before = head_blob(root, rel.as_posix())
        if before is not None:
            now_text = path.read_text(encoding="utf-8", errors="replace")
            if now_text != before:
                if upd_entry_count(now_text) <= upd_entry_count(before):
                    errs.append(
                        "file changed but gained no UPD entry (Rule 17). An entry "
                        "filed in the wrong comment block does not count -- it must "
                        "be in the UPD block, which is what a reader looks at.")
        if errs:
            failures.append((rel, errs))
    return failures


def main() -> int:
    ap = argparse.ArgumentParser(description="Check Daggerfall N header/UPD contract.")
    ap.add_argument("--root", default=".", help="Project root directory.")
    ap.add_argument("--all", action="store_true",
                    help="Scan the whole project tree (default behaviour; accepted for "
                         "compatibility with the documented command).")
    ap.add_argument("--files", nargs="*", default=None,
                    help="Check ONLY these repo-relative files (the pre-commit hook "
                         "passes the staged set). Six agents share one working tree, "
                         "so a whole-tree gate lets any agent's mid-edit file block "
                         "every other agent's commit.")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if args.files is not None:
        failures = check_files(root, args.files)
    else:
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
