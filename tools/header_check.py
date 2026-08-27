#!/usr/bin/env python3
#
# Created: 09:08:2026 - 00:06:00
# Last updated: 27:08:2026 - 23:05:00
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
# ЧТО ЭТОТ ПРИБОР НЕ ЛОВИТ, И ПОЧЕМУ ЭТО НАДО ЗНАТЬ ЗАРАНЕЕ (27.08).
# Он сверяет ДВЕ вещи: что метка 'Last updated' совпадает с ПОСЛЕДНЕЙ записью
# UPD и что изменённый файл ПРИБАВИЛ запись. Он НЕ проверяет, что запись лежит
# В ШАПКЕ, и у файлов без ограничителя комментария (.py, .cmake, CMakeLists.txt
# — у них нет ни '*/', ни '-->') это дыра: read_head читает до потолка в 4000
# строк, поэтому запись, положенная в комментарий ПОСРЕДИ КОДА, засчитывается
# как шапочная и прибор молчит.
#
# ТАК И СЛУЧИЛОСЬ, и стоило это двух волн. Запись волны кроватей легла в
# tools/gen_city.py ниже import и sys.path.insert; хук был зелёный, поэтому
# промах не всплыл. Соседняя волна, чтобы сошлась сверка «метка == последняя
# запись», положила свою запись ТУДА ЖЕ — то есть подстроилась под чужую
# ошибку, приняв зелёный хук за доказательство законности места (правило 34:
# диагноз от непроверенной предпосылки). Починено в 8433835.
#
# ЗАМЕР ПО ВСЕМУ ДЕРЕВУ на 27.08 (все .py/.cmake/.sh/CMakeLists под git):
# остался ОДИН настоящий случай — engine/app/CMakeLists.txt, четыре записи
# ниже кода (строки 162, 166, 167, 169 при коде с 72-й).
#
# И ЭТО НЕ КОСМЕТИКА, А ВРУЩАЯ ШАПКА — проверено, потому что первое прочтение
# было «наверное, там ВТОРАЯ таблица нарочно, журнал по файлам подпапки», и
# оно правдоподобно по тексту записей. Оно не выдержало дат. Блоки
# ЧЕРЕДУЮТСЯ по времени: 18.08 17:36 наверху -> 18.08 18:19 внизу -> 19.08 ->
# 20.08 -> 23.08 внизу -> 27.08 02:20 НАВЕРХУ. Нарочную вторую таблицу так не
# ведут — её ведут подряд; чередование значит, что дописывали то туда, то
# сюда. ЖИВОЕ ПОСЛЕДСТВИЕ: 'Last updated' стоит 23:08 23:50 — это последняя
# запись НИЖНЕГО блока, — а самая свежая запись файла 27:08 02:20 лежит в
# ВЕРХНЕМ. Шапка врёт о возрасте файла на четыре дня, и прибор этого не
# видит, потому что сверяет метку с последней записью В ПОРЯДКЕ ФАЙЛА.
# Правится это не переносом ради красоты, а сведением обоих блоков в один
# по времени и приведением метки к самой поздней записи.
#
# Названо здесь, чтобы владелец файла (зона app, Rule 25 — LEAD-owned) решил
# сам, а не чтобы кто-то нашёл это заново. У самой проверки есть ЛОЖНОЕ СРАБАТЫВАНИЕ, о котором надо знать:
# шапка в тройных кавычках (tools/make_interior_pilot.py) — это НЕ дефект,
# блок '# UPD:' там стоит сразу за докстрокой, то есть в начале файла.
#
# ПОЧЕМУ ДЫРА НЕ ЗАКРЫТА СЕГОДНЯ. Закрыть её просто: у файла без ограничителя
# обрывать read_head на первой строке, которая не комментарий и не пустая.
# Но это НЕМЕДЛЕННО начнёт отвергать коммиты всякого файла, чья история уже
# лежит не там, — а в дереве одновременно работает несколько волн, и запрет,
# уроненный им под руку, стоит дороже дефекта, который он ловит. Делать это
# надо на спокойном дереве и одним заходом с починкой найденного выше.
#
# ЕСЛИ ПРАВИТЕ ШАПКИ СКРИПТОМ — три грабли, все проверены руками:
#   1. ПРАВКА СУЩЕСТВУЮЩЕЙ ЗАПИСИ НЕ СЧИТАЕТСЯ. Прибор сравнивает ЧИСЛО
#      записей с HEAD, поэтому коммит, исправляющий одну лишь метку, обязан
#      завести запись о том, что исправил метку.
#   2. ОБХОД БЛОКА — ПО СТРОКАМ, НЕ ПО СМЕЩЕНИЯМ. После '# UPD:' часто идёт
#      ПУСТАЯ строка; посимвольный обход обрывается на ней и вставляет запись
#      в середину самой строки '# UPD:'.
#   3. В .md БРАТЬ ВТОРОЙ ОГРАНИЧИТЕЛЬ. Первый '-->' закрывает блок
#      Created/Last updated, а не UPD; запись перед ним уходит не туда.
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
# - 14:08:2026 - 16:45:00: assets/maps/ добавлена в SKIP_PATH_RES — раскладка карт/демок (docs/MAP_LAYOUT.md): .map манифесты, .chat.jsonl, .gitkeep. Это ДАННЫЕ конвейера карт, не исходники; контракт живёт в doc, не в пофайловом заголовке.
# - 14:08:2026 - 23:36:19: assets/objects/ в SKIP_PATH_RES — реестр объектов (.dfo, INDEX.md): печёные данные, адресуемые content-hash; у бинарника нет шапки, его identity — хэш.
# - 15:08:2026 - 16:24:04: assets/scenes/ в SKIP_PATH_RES — файлы композиции карт (данные конвейера, контракт в Scene.h).
# - 17:08:2026 - 17:53:25: README.md исключён — это входная дверь для постороннего, а не исходник;
#   шапка контракта агентов была бы первым, что он прочтёт, и ему она ничего не говорит.
# - 20:08:2026 - 15:50:00: assets/houses/ в SKIP_PATH_RES — библиотека построек (.dfh), пишется побайтово-детерминированным write_house.
# - 25:08:2026 - 02:35:21: docs/design/worldmap/ в SKIP_PATH_RES — архив атласа Яан: final.map — FMG-сейв строгого формата, .txt — дословные архивы с шапкой-преамбулой; контракт в WORLD_MAP.md 9.11.
# - 27:08:2026 - 10:42:28: НАЗВАНО ВСЛУХ, ЧЕГО ЭТОТ ПРИБОР НЕ ЛОВИТ (см. врезку выше):
#   у файлов без ограничителя комментария запись, лежащая в комментарии
#   ПОСРЕДИ КОДА, засчитывается как шапочная. На этом две волны подряд
#   положили записи мимо шапки (починено в 8433835), причём вторая
#   подстроилась под ошибку первой, приняв зелёный хук за доказательство
#   законности места. Врезка несёт ЗАМЕР по всему дереву (остался один
#   настоящий случай, назван поимённо), известное ложное срабатывание,
#   довод, почему дыра НЕ закрыта сегодня, и три грабли для тех, кто
#   правит шапки скриптом. Кода не тронуто ни строки — только шапка.
# - 27:08:2026 - 10:47:27: НАЗВАННЫЙ СЛУЧАЙ ДОВЕДЁН ДО ДИАГНОЗА (врезка выше).
#   Соседняя волна усомнилась разумно: четыре записи ниже кода в
#   engine/app/CMakeLists.txt читаются как НАРОЧНАЯ вторая таблица —
#   журнал по файлам подпапки. Проверил датами: блоки ЧЕРЕДУЮТСЯ
#   (18.08 17:36 вверху -> 18.08 18:19 внизу -> ... -> 23.08 внизу ->
#   27.08 вверху), а так нарочную таблицу не ведут. И у этого есть
#   живое последствие: 'Last updated' файла отстал на четыре дня, потому
#   что указывает на последнюю запись НИЖНЕГО блока, тогда как самая
#   свежая лежит в верхнем. Без этой проверки врезка отправляла бы
#   владельца чинить то, что он мог счесть задуманным, — и он был бы
#   вправе отмахнуться.

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
# - 27:08:2026 - 23:05:00: tests/fixtures/ в SKIP_PATH_RES — замороженные
#   ОТВЕРГНУТЫЕ СЛУЧАИ контрольных рук (правило 30): .dfh той же кузницы, чей
#   контракт — строка add_test, которая их зовёт, плюс собственная преамбула
#   каждого («какое число, с какого живого тела снято, какую руку сторожит»).
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
    # assets/maps/ holds DATA authored by the map pipeline, not source code:
    # .map manifests, .chat.jsonl sidecars, .gitkeep markers. See
    # docs/MAP_LAYOUT.md. The contract for these files lives in that doc, not
    # in a per-file header the format cannot carry.
    re.compile(r"^assets/maps/"),
    # assets/objects/ is the OBJECT REGISTRY (.dfo binaries + INDEX.md): baked
    # data addressed by content hash. A binary cannot carry a comment header,
    # and its identity lives in the hash, not in a timestamp.
    re.compile(r"^assets/objects/"),
    # assets/scenes/ holds COMPOSITIONS (.scene): what stands where on a map,
    # edited by humans and agents and judged by dfn_scene_check. Data of the
    # composition pipeline, not source; its contract lives in
    # engine/world/sources/Scene.h.
    re.compile(r"^assets/scenes/"),
    # assets/houses/ is the BUILDING LIBRARY (.dfh): house graphs written
    # byte-deterministically by write_house (two writes of one graph must
    # compare equal — that promise IS the format), regenerated by dfn_houses.
    # A hand-stamped timestamp header would break the determinism the tests
    # stand on; the contract lives in HouseFile.h and tools/forge_houses.cpp.
    re.compile(r"^assets/houses/"),
    # docs/design/worldmap/ is the ARCHIVED WORLD ATLAS (Yaan, lore session's
    # final): final.map is an FMG save with a strict line format — a header
    # would break loading it back into the tool — and the .txt bodies are
    # verbatim archived artefacts (their own headers were prepended where the
    # format allows). Contract: docs/design/WORLD_MAP.md 9.11.
    re.compile(r"^docs/design/worldmap/"),
    # tests/fixtures/ holds FROZEN REJECTED CASES — the geometry a judge must
    # keep failing (Rule 30). Their format is the pipeline's own (.dfh house
    # graphs), and their contract is the add_test line that names them plus the
    # `#` preamble each one carries: what number was measured, on which live
    # body, and which hand it controls. A timestamp header would be a second,
    # weaker answer to the same question, and it would drift the day someone
    # re-measures without touching the geometry.
    re.compile(r"^tests/fixtures/"),
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
# README.md: the FRONT DOOR, written for a stranger who has never seen this
# repository. A Created/Last updated/UPD block at the top of it would be the
# first thing that stranger reads, and it says nothing to him — the contract
# exists so agents can find what changed in a source file, and the README's
# history is the git log.
SKIP_FILENAMES = {".gitignore", ".gitattributes", "LICENSE", "varying.def.sc",
                  "feature_requests.md", "settings.cfg", "README.md"}


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
