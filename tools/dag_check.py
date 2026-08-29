#!/usr/bin/env python3
#
# File: tools/dag_check.py
#
# Responsibility:
# - Enforce the dependency DAG from docs/ARCHITECTURE.md by parsing every
#   `#include "engine/..."` in the tree and rejecting edges the contract forbids.
# - Enforce that third-party headers appear ONLY inside platform backend dirs.
#
# Dependencies:
# - Uses: Python stdlib only.
# - Used by: ctest (dag_contract), tools/hooks/pre-commit, CI.
#
# Notes:
# - WHY A SCRIPT AND NOT CMAKE. ARCHITECTURE.md claimed "CMake enforces this:
#   forbidden includes fail the build". It did not, and the audit caught the
#   claim: the repo root sits on every target's include path through
#   `dfn_headers`, so `dfn_core` could include `RenderSystem.h` and compile
#   cleanly. Making CMake do it means per-target include paths -- a large
#   refactor of every CMakeLists, touching six zones' files at once. This is the
#   same guarantee for an afternoon's work instead of a week's, and it can be
#   replaced by the CMake version later without changing what is enforced.
# - WHY IT LANDS GREEN. The audit measured the graph first: 8 checks, 0
#   violations, 1716 include lines. Enforcement that lands green can never be
#   argued down later as disruptive -- which is the only moment a gate like this
#   is cheap to add. A gate proposed while the tree is dirty gets negotiated.
# - THE CLEAN GRAPH WAS A DISCIPLINE RESULT. Six agents held the contract for
#   two days without a breach, which is a real achievement and exactly the wrong
#   thing to keep relying on: yesterday six of us broke a rule we could all
#   quote, while busy. A rule that holds only while you are paying attention
#   fails precisely when you are not (Rule 16's own lesson, generalised).
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly. LEAD-owned (Rule 25). The LAYERS table
#   below is the executable copy of the DAG in ARCHITECTURE.md -- if you change
#   one, change the other in the same commit (Rule 39).

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')

# The DAG, as an executable table. KEY may include VALUES and nothing else
# (plus its own layer and anything under `std`/glm, which are not matched here
# because only "engine/..." paths are collected).
#
# Read this as ARCHITECTURE.md's "arrows = may include", flattened: each layer
# lists every layer it is permitted to reach, transitively closed, because an
# include is a direct edge regardless of how the diagram draws it.
PLATFORM_IFACES = [
    "engine/platform/window", "engine/platform/input", "engine/platform/render",
    "engine/platform/physics", "engine/platform/anim", "engine/platform/audio",
    "engine/platform/llm",
]
MIDDLE = ["engine/world", "engine/physics", "engine/anim", "engine/render"]

LAYERS: dict[str, list[str]] = {
    # core depends on NOTHING (std + glm only, Rule 2).
    "engine/core": [],
    # world depends on core only: pure data + generation, no physics, no render.
    "engine/world": ["engine/core"],
    # Platform interfaces are pure contracts over core types.
    **{p: ["engine/core"] for p in PLATFORM_IFACES},
    # The middle layer: core + platform interfaces, and NO SIBLINGS.
    **{m: ["engine/core"] + PLATFORM_IFACES for m in MIDDLE},
    # gameplay sits above the middle layer and may use all of it.
    "engine/gameplay": ["engine/core"] + PLATFORM_IFACES + MIDDLE,
    "engine/editor": ["engine/core", "engine/gameplay"] + PLATFORM_IFACES + MIDDLE,
    # The composition root may reach everything (Rule 22: main.cpp only).
    "engine/app": ["engine/core", "engine/gameplay", "engine/editor"]
                  + PLATFORM_IFACES + MIDDLE,
}

# world is allowed to see physics only through the interface, never the engine
# layer; the table above already encodes that by omission.

# Third-party headers may appear ONLY under a platform backend directory.
# `sources/<backend>/` is the whole exemption -- interfaces/ never qualifies.
THIRD_PARTY_PREFIXES = (
    "Jolt/", "GLFW/", "bgfx/", "bx/", "bimg/", "miniaudio", "ozz/", "llama",
    "doctest/", "imgui",
)
BACKEND_DIR_RE = re.compile(r"engine/platform/[^/]+/sources/[^/]+/")

SKIP_DIR_PREFIXES = ("build", ".build")
SKIP_DIRS = {".git", "third_party", "_deps", "__pycache__", ".venv", "node_modules"}
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".inl"}

def layer_of(rel: str) -> str | None:
    """Longest matching layer prefix for a repo-relative posix path."""
    best = None
    for name in LAYERS:
        if rel.startswith(name + "/") and (best is None or len(name) > len(best)):
            best = name
    return best

def scan(root: Path) -> tuple[list[str], int, int]:
    errors: list[str] = []
    engine_includes = 0
    files = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        parts = path.relative_to(root).parts
        if any(p in SKIP_DIRS or p.startswith(SKIP_DIR_PREFIXES) for p in parts):
            continue
        rel = path.relative_to(root).as_posix()
        src_layer = layer_of(rel)
        in_backend = bool(BACKEND_DIR_RE.match(rel))
        files += 1

        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue

        for n, line in enumerate(text.splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)

            # -- third-party containment ---------------------------------------
            if inc.startswith(THIRD_PARTY_PREFIXES):
                # tests may use doctest; backends may use their own library.
                if rel.startswith("tests/") and inc.startswith("doctest/"):
                    pass
                # THE ONE EXEMPTION THE ARCHITECTURE ALREADY GRANTED IN WORDS.
                # This is not a hole being opened; it is the check catching up
                # with the contract it exists to enforce. ARCHITECTURE.md says
                # it twice, in the repository layout ("editor/  In-game editor
                # mode (Dear ImGui - allowed here only)") and again under the
                # dependency DAG ("editor may use Dear ImGui directly
                # (documented exception; nothing else may)"). Until now the
                # script forbade what the document permits, which is the same
                # class of defect as the false CMake-enforcement claim that
                # bought this script in the first place: a rule and its
                # executable copy disagreeing (Rule 39).
                #
                # NARROW IN BOTH DIRECTIONS, on purpose. Only under
                # engine/editor/, and only Dear ImGui: Jolt, bgfx, GLFW, ozz and
                # the rest stay forbidden there. A blanket "the editor is
                # exempt" would become the door every future library walks
                # through, and the next reader would have no way to tell which
                # ones were ever decided on.
                elif rel.startswith("engine/editor/") and inc.startswith("imgui"):
                    pass
                elif not in_backend:
                    errors.append(
                        f"{rel}:{n}: third-party include \"{inc}\" outside a "
                        f"platform backend directory. Rule 2 / Rule 23: backends "
                        f"are the ONLY place a third-party header may appear.")
                continue

            if not inc.startswith("engine/"):
                continue
            engine_includes += 1

            if src_layer is None:
                continue  # tests, tools, games: consumers, not layers

            dst_layer = layer_of(inc)
            if dst_layer is None or dst_layer == src_layer:
                continue

            # NARROW, NAMED EXEMPTION: a platform BACKEND may reach a sibling
            # platform backend, because backends of one technology are not
            # independent -- the GLFW input backend needs the GLFW window's
            # handle, and no amount of layering makes that untrue. The exemption
            # is on the SOURCE being inside `sources/<backend>/`, never on the
            # interface: `IInput.h` reaching `IWindow.h` would be a real
            # violation, because it would put the coupling in the contract
            # instead of in one implementation.
            #
            # Written as an exemption with a reason rather than by loosening the
            # table, so that the next backend pair inherits the reasoning and
            # not just the permission.
            if (in_backend and src_layer.startswith("engine/platform/")
                    and dst_layer.startswith("engine/platform/")):
                continue

            if dst_layer not in LAYERS[src_layer]:
                errors.append(
                    f"{rel}:{n}: {src_layer} must not include {dst_layer} "
                    f"(\"{inc}\"). The DAG in docs/ARCHITECTURE.md permits "
                    f"{src_layer} -> {', '.join(LAYERS[src_layer]) or '(nothing)'}.")
    return errors, engine_includes, files

def main() -> int:
    ap = argparse.ArgumentParser(description="Enforce the dependency DAG.")
    ap.add_argument("--root", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    errors, edges, files = scan(Path(args.root))
    if errors:
        for e in errors:
            print(f"DAG VIOLATION: {e}", file=sys.stderr)
        print(f"\n{len(errors)} violation(s) across {files} files.", file=sys.stderr)
        return 1
    if not args.quiet:
        print(f"OK: DAG holds. {edges} engine includes across {files} files, "
              f"0 violations.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
