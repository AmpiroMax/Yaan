#!/usr/bin/env bash
#
# Created: 09:08:2026 - 00:06:00
# Last updated: 09:08:2026 - 00:06:00
# File: tools/install_hooks.sh
#
# Responsibility:
# - Install the project's git hooks into .git/hooks (idempotent).
#
# Dependencies:
# - Uses: tools/hooks/pre-commit.
# - Used by: humans and agents after cloning; documented in AGENTS.md workflow.
#
# AI Agents Notice:
# - Follow docs/ARCHITECTURE.md strictly.
#
# UPD:
# - 09:08:2026 - 00:06:00: Created installer copying tools/hooks/* into .git/hooks.

set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
hooks_src="$repo_root/tools/hooks"
hooks_dst="$repo_root/.git/hooks"

for hook in "$hooks_src"/*; do
    name="$(basename "$hook")"
    cp "$hook" "$hooks_dst/$name"
    chmod +x "$hooks_dst/$name"
    echo "installed: $name"
done
