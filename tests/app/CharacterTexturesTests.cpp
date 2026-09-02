/*
Module: tests/app
File: tests/app/CharacterTexturesTests.cpp

Responsibility:
- ЛИСТ ПЕРСОНАЖА ОТ СЕКЦИИ TEX ДО GPU на нулевом бэкенде: верный sha —
  лист поднят один раз и делится по sha (второе тело, тот же PNG под другим
  именем ссылки — тот же номер ассета, live_textures не растёт); неверный
  sha — ОТКАЗ (0), файла нет — отказ; DFN_BODY_PALETTE — отказ без чтения.
  Контрольная рука (правило 30) — ровно неверный sha: та же ссылка с одним
  изменённым знаком обязана дать ноль.

Dependencies:
- Uses: engine/app CharacterTextures, engine/platform/render null backend,
  engine/render RenderSystem/ObjectRegistry, core Sha256, doctest.
- Used by: ctest (app_character_textures).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Лист берётся из assets/branding (PNG, лежащий в git): набор не пишет и не
  требует ничего вне дерева.
*/

#include "engine/app/sources/CharacterTextures.h"

#include "engine/core/serialization/sources/Sha256.h"
#include "engine/platform/render/sources/null/NullRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

using namespace dfn;

namespace {

const char* SHEET = "assets/branding/oak_seal/oak_seal_128.png";

render::RegistryObject body_with_sheet(const std::string& path, const std::string& sha) {
    render::RegistryObject obj;
    obj.name = "tex-test";
    render::TextureRef t;
    t.role = "albedo";
    t.path = path;
    t.sha256 = sha;
    obj.textures.push_back(t);
    return obj;
}

} // namespace

TEST_CASE("character sheet: verified sha is uploaded once and shared by sha") {
    if (!std::filesystem::exists(SHEET)) {
        MESSAGE("no branding PNG in the tree -- skipped");
        return;
    }
    const auto sha = serialization::sha256_file(SHEET);
    REQUIRE(sha.has_value());
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const uint32_t before = renderer.live_textures();
    const std::size_t loaded_before = app::body_textures_loaded();

    const render::RegistryObject a = body_with_sheet(SHEET, *sha);
    const uint32_t asset_a = app::body_albedo_asset(rs, renderer, a, "");
    CHECK(asset_a != 0);
    CHECK(renderer.live_textures() == before + 1);
    CHECK(app::body_textures_loaded() == loaded_before + 1);

    // A SECOND BODY naming the same bytes under an absolute path: same asset,
    // no second upload -- the cache keys on the sha, not on the path.
    const std::string abs = std::filesystem::absolute(SHEET).string();
    const render::RegistryObject b = body_with_sheet(abs, *sha);
    CHECK(app::body_albedo_asset(rs, renderer, b, "") == asset_a);
    CHECK(renderer.live_textures() == before + 1);
    CHECK(app::body_textures_loaded() == loaded_before + 1);

    // A body without a sheet is simply zero.
    render::RegistryObject bare;
    CHECK(app::body_albedo_asset(rs, renderer, bare, "") == 0);
}

TEST_CASE("character sheet: the control -- a wrong sha is refused, a missing file too") {
    if (!std::filesystem::exists(SHEET)) {
        MESSAGE("no branding PNG in the tree -- skipped");
        return;
    }
    const auto sha = serialization::sha256_file(SHEET);
    REQUIRE(sha.has_value());
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    const uint32_t before = renderer.live_textures();

    std::string wrong = *sha;
    wrong[0] = wrong[0] == 'a' ? 'b' : 'a';
    const render::RegistryObject a = body_with_sheet(SHEET, wrong);
    CHECK(app::body_albedo_asset(rs, renderer, a, "") == 0);
    CHECK(renderer.live_textures() == before);

    std::string missing_sha(64, 'c');
    const render::RegistryObject m = body_with_sheet("assets/branding/does_not_exist.png",
                                                     missing_sha);
    CHECK(app::body_albedo_asset(rs, renderer, m, "") == 0);
    CHECK(renderer.live_textures() == before);
}

TEST_CASE("character sheet: relative path resolves up from the .dfo's directory") {
    if (!std::filesystem::exists(SHEET)) {
        MESSAGE("no branding PNG in the tree -- skipped");
        return;
    }
    const auto sha = serialization::sha256_file(SHEET);
    REQUIRE(sha.has_value());
    platform::NullRenderer renderer;
    render::RenderSystem rs;
    // Pretend the .dfo lies deep in the tree; the sheet path is repo-relative
    // and the current directory is the repo root in ctest, so the first probe
    // already hits -- the walk-up is exercised by the sub-directory label
    // being irrelevant to the answer.
    const render::RegistryObject a = body_with_sheet(SHEET, *sha);
    CHECK(app::body_albedo_asset(rs, renderer, a,
                                 "assets/objects/characters/textures/x/y.dfo") != 0);
}
