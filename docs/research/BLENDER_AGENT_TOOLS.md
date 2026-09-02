<!--
Module: docs
File: docs/research/BLENDER_AGENT_TOOLS.md

Responsibility:
- Ресёрч по заказу владельца 02.09 («я установил Blender — найди плагины /
  MCP-серверы / тулы, чтобы Claude мог создавать 3D-объекты самостоятельно с
  нормальными пропорциями»): какие MCP-серверы связывают агента с Blender, какие
  генераторы 3D и авториггеры доступны на macOS без NVIDIA, какие параметрические
  люди скриптуются, что из этого на каких лицензиях. Плюс журнал того, что
  РЕАЛЬНО поставлено на машину владельца и как этим пользоваться.

Key items:
- §0 — поставлено и проверено: Blender 5.2.1, MPFB 2.0.17 + ассеты CC0,
  blender-mcp (ahujasid) на 9876, официальный Blender Lab MCP на 9877, conda env
  blender / blender-lab, регистрация в Claude Code; headless-рецепт человека.
- §1 — ресёрч MCP/агентных инструментов Blender дословно (два сервера, форки,
  скиллы, библиотеки ассетов, MPFB2 / HumGen / MB-Lab).
- §2 — ресёрч генеративного 3D (Hunyuan3D, TRELLIS, SF3D, Pixal3D…), облачных
  API (Meshy, Tripo, Rodin…), авторига (UniRig, Mixamo, ARP, Rigify…),
  параметрических людей (MPFB2, HumGen, CC4, MetaHuman) и «текст → пропорции»
  (Anny, SMPL-X) дословно.

Dependencies:
- Uses: docs/design/HUMAN_SCALE.md (канон пропорций — числа для MPFB-ручек),
  tools/check_human_scale.cpp (судья), tools/make_human_body.py и
  tools/graft_head.py (прежние headless-скрипты под Blender 4.2 — модуль теперь
  bl_ext.blender_org.mpfb), docs/research/CHARACTER_PIPELINE.md и
  CHARACTER_EDITOR_TOOLS.md (куда это встраивается).
- Used by: волны линии персонажей (E3-2, E15-4), любой агент, которому нужно
  сделать модель в Blender с картинкой-обратной связью.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Ресёрчи в §1–§2 ДОСЛОВНЫ (правило 30.08: суммаризация запрещена); правки —
  только в §0 и только по факту проверки на машине.
- Питон только через conda (директива 25.08); uv/venv не использовать даже
  если README инструмента предлагает uvx.
-->

# Инструменты, чтобы агент сам делал 3D в Blender: MCP-серверы, генераторы, авториг, параметрические люди

Заказ владельца 02.09.2026: «я установил Blender, найди плагины / MCP-серверы / тулы,
чтобы Claude мог создавать 3D-объекты самостоятельно с нормальными пропорциями и
пониманием размытых промптов». Ниже — два ресёрча ДОСЛОВНО (правило: ресёрчи
хранятся без пересказа, см. `docs/ARCHITECTURE.md`), затем комментарий
ответственного: что поставлено на машину владельца и как этим пользоваться.

## 0. Что поставлено 02.09 (комментарий ответственного)

| Что | Где | Проверено |
|---|---|---|
| Blender 5.2.1 LTS (Python 3.13) | `/Applications/Blender.app` | headless `--python-expr` работает |
| MPFB 2.0.17 (build 20260722) | extension `bl_ext.blender_org.mpfb` в профиле 5.2 | `HumanService.create_human()` headless: 19 158 вершин, экспорт .glb |
| Системные ассеты MakeHuman CC0 (267 МБ) | `~/Library/Application Support/Blender/5.2/extensions/.user/blender_org/mpfb/data` | скины young/middleage/old × caucasian/asian/african × м/ж, глаза, брови, волосы, одежда, прокси, зубы |
| blender-mcp 1.9.1 (ahujasid, MIT) | conda env `blender`, `~/miniconda3/envs/blender/bin/blender-mcp`; addon `blender_mcp_addon` в Blender | сокет 9876: get_scene_info, execute_code, get_viewport_screenshot — кадр снят |
| Blender Lab MCP 1.0.0 (официальный, GPL, только Blender ≥ 5.1) | conda env `blender-lab`; extension `bl_ext.user_default.mcp`, порт 9877, autostart | серверу нужен `mcp<2` (пин поставлен) |
| Регистрация в Claude Code | `claude mcp add -s local blender …` и `blender-lab … --port 9877` (в `~/.claude.json`, проект Daggerfall N) | новая сессия Claude Code видит оба |

Правила площадки: питон только через conda (директива 25.08); addon-интеграции
Poly Haven / Sketchfab / Poly Pizza / Hyper3D / Hunyuan3D в blender-mcp по
умолчанию ВЫКЛЮЧЕНЫ и включаются галочками в N-панели «MCP for Blender»;
телеметрия blender-mcp по умолчанию ВКЛ (выключать `DISABLE_TELEMETRY=true` в
env сервера).

Запуск GUI с сервером одной командой (скрипт таймером зовёт
`bpy.ops.blendermcp.start_server()`, Lab-сервер стартует сам):

```
open -a Blender --args --python artifacts/reports/blender-agent/start_mcp.py
```

Первый рубеж (тот же день): «нордский воин, около 1,88 м, крепкий, средних лет»
переведён в ручки MPFB (gender 1.0, age 0.6, muscle 0.7, weight 0.55, height
0.62, proportions 0.7, caucasian 1.0) → тело 1,891 м, скин CC0
young_caucasian_male, рендер Workbench и .glb с текстурой лежат в
`artifacts/reports/blender-agent/` (mpfb_nord.png, mpfb_nord.glb). Владелец:
«он прям супер выглядит как надо» → решение 02.09: это тело становится базовой
моделью игрока (HumanBase.glb), манекен Quaternius остаётся донором клипов
(UAL_Clips.glb).

**Бисект «кринжа» в движке (кадры 00–12 в той же папке).** В игре норд вышел с
пальцами-иглами и сбитыми пропорциями. Два ложных следа по дороге, оба мои:
«экспортёр glTF 5.2 запекает позу» (нет: мини-репро чист, а «лечение»
`pose_position="REST"` перед экспортом молча обнуляет анимацию) и «кисть весится
на стопы» (ошибка моего замера). Настоящее: (1) Blender при импорте .glb с
клипом показывает позу клипа, а не бинд, и смотровая движка для модели с
клипами тоже показывает позу; (2) КОРЕНЬ — MPFB подгонял риг по НЕЙТРАЛЬНОМУ
телу 1,67 м, потому что `create_human(detailed_helpers=False)` выкидывает
группы joint-*, а меш после макро-ручек 1,91 м: суставы ниже на 14 %, плечи
уже на 32 %, кости кисти в 10 см от вершин кисти, и любой клип рвал кисть;
(3) поверх этого покой MPFB (A-поза, ролл ладони, согнутые пальцы) отличался от
T-позы донора на 48–98°, а ретаргет копирует мировую ориентацию, так что
фаланга крутилась на 96–155° от своего бинда. Починено в
`tools/make_human_body.py`: detailed_helpers=True, покой рига приводится к
покою донора и запекается в меш (разница 0,000°), лодыжка остаётся на месте.
Проверка: бинд — ровная T-поза, Idle кадр 1 — кулак, суставы по судье: таз
+2,2 %, размах рук −2,2 % (кадры 09–12).

Headless-рецепт человека (важно: модуль называется `bl_ext.blender_org.mpfb`, а
словарь макро-ручек ОБЯЗАН содержать ключ `race`):

```python
import bpy, importlib
HS = importlib.import_module('bl_ext.blender_org.mpfb.services.humanservice').HumanService
TS = importlib.import_module('bl_ext.blender_org.mpfb.services.targetservice').TargetService
macro = TS.get_default_macro_info_dict()
macro.update({'gender': 1.0, 'age': 0.6, 'muscle': 0.7, 'weight': 0.55,
              'height': 0.62, 'proportions': 0.7,
              'race': {'caucasian': 1.0, 'asian': 0.0, 'african': 0.0}})
h = HS.create_human(macro_detail_dict=macro, detailed_helpers=True, extra_vertex_groups=False)  # True: иначе риг сядет по нейтральному телу
HS.set_character_skin('<data>/skins/young_caucasian_male/young_caucasian_male.mhmat', h, skin_type='MAKESKIN')
bpy.ops.export_scene.gltf(filepath='out.glb', export_format='GLB')
```

«Понимание размытых промптов» — это не плагин, а связка: агент переводит слова в
числа по канону (`docs/design/HUMAN_SCALE.md`, судья `dfn_human_scale`), MPFB
даёт анатомически честное тело по этим числам, скриншот вьюпорта / рендер
возвращает картинку агенту, и цикл повторяется до приёмки. Ни один найденный
инструмент не делает «текст → параметры MakeHuman» сам (см. §2, п. 5).

## 1. Ресёрч: MCP-серверы и агентные инструменты для Blender (дословно, 02.09.2026)

### 1. ahujasid/blender-mcp — renamed "MCP for Blender"

- **PyPI `blender-mcp` 1.9.1, released 2026-09-02 (today)**; MIT; `requires-python >=3.10`; deps only `mcp>=1.9.0,<2` + `httpx`. Prior: 1.9.0 (08-30), 1.8.8 (08-30), 1.8.7 (08-24). GitHub 26.7k stars, last commit 2026-09-02, active.
- Renamed 2026-09-01 (commit: *"change name to mcp for blender to avoid confusion with official blender foundation"*).
- **28 tools** (`src/blender_mcp/server.py`): `get_scene_info`, `get_object_info`, `get_viewport_screenshot(max_size=1000)` → Image, `execute_blender_code`, `get_addon_status`, `disable_telemetry`, `record_trajectory_feedback`; Poly Haven (`get_polyhaven_categories/status`, `search_polyhaven_assets`, `download_polyhaven_asset`, `set_texture`); Sketchfab (`status/search/get_..._preview/download`); **Poly Pizza** (`status/search/download`); Hyper3D Rodin (`generate_..._via_text`, `via_images`, `poll_rodin_job_status`, `import_generated_asset`); **Hunyuan3D** (`status/generate/poll/import`).
- **conda / pip: works.** `pyproject.toml` declares `[project.scripts] blender-mcp = blender_mcp.server:main`, and `main()` dispatches `install-addon` / `addon-paths` from `sys.argv`. So `pip install blender-mcp` in a conda env gives the `blender-mcp` command plus both subcommands. README's default path is uv/uvx but it documents a no-uv route ("Install without uv", pipx) and says to use the absolute path of the installed command with `args` omitted. README also warns uv can mis-pick conda interpreters — irrelevant if you skip uv.
- **Claude Code config:** `claude mcp add blender uvx blender-mcp` → conda equivalent `claude mcp add blender /path/to/conda/envs/<env>/bin/blender-mcp`. JSON: `{"mcpServers":{"blender":{"command":"<abs path>/blender-mcp"}}}` (env block accepts `DISABLE_TELEMETRY`, `BLENDER_MCP_SAFE_MODE`).
- **Addon:** `blender-mcp install-addon` copies `addon.py` → Blender addons dir as `blender_mcp.py` (override `BLENDERMCP_ADDONS_DIR`; keeps `.bak`); or manual Install… from disk. Enable **Interface: MCP for Blender**, then N-panel → **Start MCP Server** (TCP localhost:9876).
- **Blender versions:** `bl_info` = `"blender": (3,0,0)`, addon version (1,6), **no max declared**; README prerequisite "Blender 3.0 or newer". It is a *legacy* bl_info addon, not an extension — Blender 5.x still installs those via "Install legacy Add-on", but **no explicit 4.5/5.x support statement exists** and issue #260 ("Cannot connect to Blender 4.5 LTS on Mac", closed 2026-08-09) is the only 4.5 datapoint. Treat 5.x as unverified.
- **Notable:** telemetry consent is **ON by default** (`DISABLE_TELEMETRY=true` or the addon checkbox). `BLENDER_MCP_SAFE_MODE=1` (added today) statically screens generated Python for file/network/subprocess access.

### 2. Official Blender Foundation MCP — exists

- Repo `https://projects.blender.org/lab/blender_mcp.git` (Cloudflare-blocked to fetch, clones fine); docs `blender.org/lab/mcp-server`. Last commit **2026-08-06**.
- Add-on manifest: `id "mcp"`, **version 1.0.0**, maintainer "Blender Lab", **`blender_version_min = "5.1.0"`**, **GPL-3.0-or-later**. Installed via drag-drop of the Blender Lab extension repo, or Install from Disk. **Will not run on Blender 4.5 LTS.**
- Server package `mcp/blmcp`, `pyproject.toml` name **`blender-mcp` v1.0.0**, entry point `blender-mcp = blmcp:main`, deps `docutils, mcp[cli]>=1.2.0, pyyaml`. **Not on PyPI** (that name is ahujasid's) — install from source (`pip install ./mcp` in a conda env works) or a `.mcpb` bundle.
- Tools: `execute_blender_code`, `get_objects_summary`, `get_object_detail_summary`, `get_blendfile_summary_*` (datablocks / missing files / linked libraries / path info / usage guess), `get_screenshot_of_window_as_image|as_json`, `get_screenshot_of_area_as_image`, `render_viewport_to_path`, `render_thumbnail_to_path`, `jump_to_tab_by_name|space_type`, `jump_to_view3d_object_by_name|_data_by_name`, `get_python_api_docs`, `search_api_docs`, `search_manual_docs`. Ships bundled bpy API reference + manual RST. **No asset libraries, no AI generation.** Docs carry an explicit warning that it executes LLM code with no guards.

### 3. Other alternatives

| Project | Last commit | Licence | Notes |
|---|---|---|---|
| `djeada/blender-mcp-server` (PyPI 0.1.3, 2026-06-21) | 2026-06-21 | MIT | 24★, ~22–27 tools, entry point `blender-mcp-server` |
| `dhakalnirajan/blender-open-mcp` | 2026-04-21 | NOASSERTION | 112★, Ollama-oriented, PolyHaven |
| MB-Lab (`animate1978/MB-Lab`) | **archived**, 2024-07-21 | NOASSERTION | last release 1_8_1, 2024-06-08 — dead |
| "Blender MCP Pro" (100+ tools) | — | commercial | announced on BlenderArtists; not verified from primary source |
| Claude Code skills: `kevinbadi/blender-skills` (83★, 2026-04-04, no licence), `ra100/blender-claude-plugin` (12★, 2026-04-29, MIT), `jithinolickal/blender` (9★, 2026-03-16, Apache-2.0) | — | — | all are prompt/skill wrappers **on top of** blender-mcp, not servers |

No official Anthropic Blender skill/plugin found.

### 4. Assets

- **Poly Haven via blender-mcp: yes, all three** — `download_polyhaven_asset(asset_id, asset_type ∈ {hdris, textures, models}, resolution="1k"|2k|4k, file_format)` downloads *and imports* into Blender. CC0.
- Poly Pizza (low-poly, API key), Sketchfab (**licence per asset**, API key), Hyper3D Rodin (free trial key `vibecoding` hardcoded), Hunyuan3D.
- **BlenderKit:** ~114k assets, ~54k free; two licences — CC0 and Royalty-Free (RF forbids reselling the model itself, even modified); addon Blender 3.0+. A documented standalone Python API for scripted download: **not found**.
- **Quaternius: CC0.** **KayKit:** most packs CC0 (itch.io), paid complete bundle.

### 5. Humans / characters

- **MPFB2 v2.0.17, released 2026-07-22** (repo pushed 2026-08-10). Code **GPL-3.0-or-later**; assets under separate `LICENSE.ASSETS.md`. Extensions-platform manifest: **`blender_version_min = 4.2.0`, no max** → installs on 4.5 LTS and 5.x. 150,549 downloads.
- **Scriptable: yes, officially.** `script_samples/` (10 examples, index.md, added ~2026-06 via issue #366): create human, modeling targets, default rig, Rigify, skin, assets, list asset packs/clothes/targets, **full character FBX export**. Entry API `HumanService.create_human()` + `HumanObjectProperties.set_value("caucasian", …)` + `TargetService`. Requires a `dynamic_import()` quirk (extensions have unknown package paths). **Headless `blender --background`: not explicitly documented — unclear.**
- **Human Generator 3D (`OliverJPost/HumGen3D`)**: code GPL-3.0, v4.0.32 bumped 2026-04-19; documented Python API at `help.humgen3d.com/API/Overview`. Assets/textures are a **paid** BlenderMarket/Superhive purchase (royalty-free).
- **MB-Lab: dead** (archived 2024).

**Blender version context:** 4.5 LTS 2025-07-15 (supported to July 2027); 5.0 2025-11-18; 5.1 2026-03-17; 5.2 LTS 2026-07-14 (to July 2028).

Sources: [ahujasid/blender-mcp](https://github.com/ahujasid/blender-mcp) · [PyPI blender-mcp](https://pypi.org/project/blender-mcp/) · [Blender Lab MCP](https://projects.blender.org/lab/blender_mcp) · [blender.org/lab/mcp-server](https://www.blender.org/lab/mcp-server/) · [Official MCP setup writeup](https://zenn.dev/shintama/articles/blender-official-mcp-claude?locale=en) · [mcpservers.org/blender-mcp](https://mcpservers.org/servers/blender-mcp) · [PyPI blender-mcp-server](https://pypi.org/project/blender-mcp-server/) · [blender-open-mcp](https://github.com/dhakalnirajan/blender-open-mcp) · [MB-Lab](https://github.com/animate1978/MB-Lab) · [mpfb2](https://github.com/makehumancommunity/mpfb2) · [MPFB on Blender Extensions](https://extensions.blender.org/add-ons/mpfb/) · [HumGen3D](https://github.com/OliverJPost/HumGen3D) · [HumGen API docs](https://help.humgen3d.com/API/Overview) · [BlenderKit licensing](https://www.blenderkit.com/docs/licenses/licensing-faq/) · [Quaternius](https://quaternius.com/) · [KayKit](https://kaylousberg.itch.io/kaykit-adventurers) · [Blender releases](https://www.blender.org/download/releases/) · [Blender 5.0 release notes](https://developer.blender.org/docs/release_notes/5.0/) · [kevinbadi/blender-skills](https://github.com/kevinbadi/blender-skills) · [ra100/blender-claude-plugin](https://github.com/ra100/blender-claude-plugin) · [jithinolickal/blender](https://github.com/jithinolickal/blender)

## 2. Ресёрч: генеративное 3D и авториг (дословно, 02.09.2026)

### 1. Open-weight text/image-to-3D

**Tencent Hunyuan3D**
- **2.0** (github.com/Tencent-Hunyuan/Hunyuan3D-2) and **2.1** (github.com/Tencent-Hunyuan/Hunyuan3D-2.1) are fully open (code + weights). **2.5, 3.0, 3.1, PolyGen are NOT open** — cloud-only at 3d.hunyuanglobal.com; open-sourcing "planned", no date.
- Licence: *Tencent Hunyuan 3D 2.x Community License*. Commercial use allowed **but the licensed territory explicitly EXCLUDES the European Union, United Kingdom and South Korea**. Separate Tencent licence required above 1M MAU. You own derivative works.
- 2.1 = Shape-v2-1 (3.3B) + Paint-v2-1 (2B); true **PBR output** (albedo/metallic/roughness). VRAM: 10 GB shape, 21 GB texture, 29 GB both.
- **Apple Silicon: yes, via fork** github.com/VladimirTalyzin/hunyuan3d-2.1-mac-rocm — shape *and* PBR paint verified on MPS. M4 Pro 24 GB: shape 5.7 min (30 steps, octree 192), PBR texturing 8.5 min (6 views @256px). Limits: no flash-attn on MPS caps multiview res; float64 refused by Metal; paging at 24 GB. Also github.com/Brainkeys/Hunyuan3D-2.1-mac.
- CLI/MCP: official `api_server.py`, `gradio_app.py`, and an official `blender_addon.py` in the 2.x repo. Community MCP: PyPI `blender-mcp-hunyuan`; also wired into ahujasid/blender-mcp. **No first-party MCP.**
- Also open: **Hunyuan3D-Part** (P3-SAM segmentation + X-Part generation, weights on HF, light version only).

**Microsoft TRELLIS / TRELLIS.2**
- **Both MIT — code and weights.** TRELLIS.2-4B released Dec 2025 (huggingface.co/microsoft/TRELLIS.2-4B); O-Voxel structure, **full PBR** (base colour, roughness, metallic, opacity), GLB export.
- Official requirements: NVIDIA ≥24 GB (TRELLIS.2) / ≥16 GB (TRELLIS 1); "tested only on Linux"; CUDA 12.4. No official Mac support.
- **Apple Silicon: yes, community port** github.com/shivampkumar/trellis-mac (MIT port code) — replaces flash_attn/nvdiffrast/cumesh/flex_gemm with pure PyTorch. M4 Pro 24 GB: ~3m20s generate+bake, ~5m13s cold. Has a real **CLI (`generate.py`)** with seed/pipeline/texture-res flags. Limits: hole-filling disabled, mesh pre-simplified to ~200k faces, inference only. Note bundled RMBG-2.0 is **CC BY-NC 4.0** — a licence trap for commercial use.
- TRELLIS 1 outputs Gaussians/radiance field/textured GLB (no PBR); text-to-3D models exist but authors recommend text→image→3D.

**Stability SF3D / SPAR3D** — github.com/Stability-AI/stable-point-aware-3d. *Stability AI Community License*: free commercial use **only below US$1M annual revenue**; above that an enterprise licence is mandatory. Sub-second single-image → UV-unwrapped textured mesh.

**InstantMesh** (TencentARC) — **Apache-2.0**, still maintained. ~10 s/asset, LRM/Instant3D architecture. Older quality tier.

**2026 newcomers**
- **Pixal3D** (TencentARC, SIGGRAPH 2026, code+weights May 2026) — **MIT**, PBR textures, three-stage cascade, 1536 std / 1024 low-VRAM. CUDA (natten).
- **Step1X-3D** (stepfun-ai) — **Apache-2.0**, textured GLB, diffusers impl mentions an `mps` device path (untested).
- **Roblox Cube 3D / CubePart** — **CUBE3D Research-Only RAIL-MS. Commercial use forbidden.**
- Direct3D-S2, Sparc3D, Ultra3D, PartCrafter exist as research; licences not verified here.

### 2. Cloud APIs

| Service | MCP | Price | Output rights |
|---|---|---|---|
| **Meshy** | **Official**, MIT, `meshy-dev/meshy-mcp-server`, ~24 tools | Free 100 cr/mo; Pro $20, Premium $40, Studio $70, Ultra $100 /mo. API needs Pro+. Credits: img-to-3D 20–35, texture 10, **rig 5**, animate 3 | **Free tier = CC BY 4.0 (attribution required)**; paid = you own, may sell |
| **Tripo** | **Official** `VAST-AI-Research/tripo-mcp` (alpha, Blender-oriented) | 1 credit = $0.01, min $1. text-to-3D 10–20, img-to-3D 20–30, **auto-rig 25**, retarget 10/anim, retopo 30 | Free plan reportedly non-commercial; subscription page 403'd — **exact tiers unconfirmed** |
| **Hyper3D Rodin** | via blender-mcp + `DeemosTech/rodin3d-skills` | Free (pay-per-download, $1.50/credit); Creator $30/mo; **Business $120/mo required for full API** | ToS page 404s — **commercial wording not found on a primary source** |
| **Sloyd** | none found | Guest free (1 gen/day, personal only); **Plus $15/mo = commercial**; Pro $50/mo adds resale | Clean quads, target poly counts, REST API (API pricing unpublished) |
| **Luma Genie** | — | **DISCONTINUED, sunset 1 Jan 2026** | — |
| **Kaedim** | none | **not public** (/pricing 404); human-in-the-loop | asset-IP wording not found |
| **CSM (csm.ai)** | none first-party | site unreachable this session — **all figures unverified** | free tier reportedly CC BY 4.0 |

`ahujasid/blender-mcp` (MIT, Blender 3.0+, cross-platform) bundles Poly Haven, Sketchfab, Poly Pizza, **Hyper3D Rodin and Hunyuan3D**.

### 3. Auto-rigging

- **UniRig** (Tsinghua + VAST) github.com/VAST-AI-Research/UniRig — **MIT**, ≥8 GB **CUDA only, no MPS**.
- **Mixamo** — still online, no shutdown notice; unmaintained since 2015, repeated outages. Adobe FAQ: *"free, with no licensing or royalty fees, for unlimited commercial or non commercial use"* — **engine-agnostic, custom engine fine**; forbidden only to redistribute raw character/animation files as asset packs. Free Adobe ID, no CC subscription. **No API — web upload only.** Skeleton: 65-bone `mixamorig:`, **no twist bones**.
- **AccuRIG 2.0** (Reallusion) — **free**, **Windows-only, no macOS build**. FBX + USD, no glTF. **Has twist bones.** Royalty-free commercial per ActorCore EULA. No CLI.
- **Auto-Rig Pro** — superhivemarket.com/products/auto-rig-pro, ~$25 Lite / $50 Full one-time (page 403s to bots; third-party-reported). GPL as a Blender addon. Smart auto-detect; **twist bones with export toggle**; Game Engine Export → **FBX + glTF**, presets Unity/UE/Godot/**generic "Others"**; **documented Python API**, headless: `bpy.ops.arp.arp_export_fbx_panel(...)`.
- **Rigify** — bundled, GPL, twist bones via segmented `DEF-*.01/.02` chains, headless `bpy.ops.pose.rigify_generate()`. Known engine problem: DEF bones sit inside the MCH/ORG/control hierarchy, so export gives an unstructured skeleton; stripping non-deform bones detaches limbs. Fixed via Game Rig Tools or manual DEF re-parenting.
- **Anything World** — REST `POST /rig`, `/animate`, `/text-to-3d`; **`pip install anything-world` gives a Python package + CLI**. FBX/GLB out. Output licence selectable CC0/CC-BY/MIT. Pricing (third-party): 3 free credits/mo, Micro $50, Pro $250. No MCP. Docs mark the API "experimental".
- **Meshy rig**: humanoid biped only, Mixamo bone naming, FBX+GLB skinned; bone count undocumented. **Tripo rig**: 7 creature types, Tripo or Mixamo naming, GLB/FBX; bone count and twist bones not found.
- Other open models: **MagicArticulate** (Apache-2.0, skeleton only, 4.6 GB), **Puppeteer** (Apache-2.0, NeurIPS 2025, rig+animate, successor to MagicArticulate), **Make-It-Animatable** (MIT, ~1 s). All CUDA-targeted, **no MPS path found**. **RigAnything is Adobe Research License — NONCOMMERCIAL ONLY.**

### 4. Parametric humans

- **MPFB2** github.com/makehumancommunity/mpfb2 — v2.0.17 (Jul 2026), active. **Code GPLv3, ALL bundled assets CC0 1.0** (base mesh, targets, textures, clothes, rigs). Project claims no rights over output. Blender 4.2+, pure Python → Apple Silicon fine. Documented service-layer Python API (`HumanService`, `TargetService`). Headless `--background` use not documented.
- **MakeHuman standalone** — 1.3.0 (Apr 2024), dormant; last macOS build 1.2.0 (2020). AGPL3 code, CC0 model data.
- **MB-Lab** — **dead**, final 1.8.0 (Mar 2024), succeeded by CharMorph.
- **Human Generator V4 / HumGen3D** humgen3d.com — **$128 commercial / $68 personal**; addon GPL-3.0, bundled assets royalty-free. **Documented `BatchHumanGenerator` Python API.** Blender 3.6–5.0.
- **Character Creator 4** — **Windows 10/11 only, no macOS build.** $299 perpetual, or since Jan 2026 $29/mo–$99/yr. **FBX export, no native glTF.** SkinGen basic included; **SkinGen Premium is a paid plug-in**. CC Python API partial/never fully released (iClone's RLPy is mature). Licence permits export to any engine incl. custom, but **one Standard Licence = one commercial character**; mass production needs the Extended Licence.
- **MetaHuman** — June 2025 EULA change: usable in **any engine or creative software** (Unity, Godot, custom), sellable on Fab. Standard UE EULA: free under $1M revenue, $1,850/seat/yr above; **no 5% royalty when exporting to other engines**. Official Maya and Houdini plugins. glTF is not a documented export target — FBX/Alembic via DCC. Whether the pipeline works with zero Unreal Editor involvement is **unclear**.

### 5. Text → proportion-correct human

- **Anny — EXISTS.** github.com/naver/anny, Naver Labs Europe, arXiv 2511.03589, v0.6 (Aug 2026). **Apache-2.0 code; assets CC0** (derived from MakeHuman/MPFB2, so no SMPL data taint), WHO-anthropometry-calibrated, all ages. `pip install anny`, differentiable PyTorch body model — fully scriptable phenotype/local-shape/facial-action parameters, PLY export. Interops with SMPL-X and HumGen3D topologies, **but the SMPL-X topology variant is non-commercial-only**. **No text-prompt interface** — structured parameters only.
- **SMPL-X** — research/non-commercial only; forbids training networks for commercial use. **Epic Games acquired Meshcapade (announced 18 Feb 2026)**; Max Planck Innovation now handles SMPL licensing directly; price never public. Meshcapade REST API pricing not public, post-acquisition future unclear.
- **TeCH** — non-commercial research only, requires SMPL-X registration.
- **HumanNorm** — MIT, but needs NVIDIA ≥20 GB CUDA; no Apple Silicon path.
- **DreamHuman** — no code or weights ever released.
- **Text prompt → MakeHuman/MPFB2 parameters**: **NOT FOUND** — no existing project surfaced.

### Key gaps / unresolved
Tripo subscription prices (403), Hyper3D ToS commercial wording (404), CSM everything (site unreachable), Kaedim pricing (404), Meshy USD-per-API-credit, Meshy/Tripo rig bone counts, MetaHuman fully-outside-Unreal path, Auto-Rig Pro exact current price.
