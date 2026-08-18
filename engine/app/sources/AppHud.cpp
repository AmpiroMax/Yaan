/*
Created: 18:08:2026 - 17:35:04
Last updated: 18:08:2026 - 17:35:04
Module: engine/app
File: engine/app/sources/AppHud.cpp

Responsibility:
- The assembly itself, and the badge's decision. See AppHud.h for why the
  assembly is the part that was worth moving.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 17:35:04: Создан вместе с заголовком — 238 строк сборки кадра из
  App::run().
*/

#include "engine/app/sources/AppHud.h"

#include "engine/app/sources/Localization.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/PixelCanvas.h"

namespace dfn::app {
namespace {

std::string_view say(const char* key) {
    return localized(serialization::fnv1a64(key));
}

// The prompt and the probe share this ink and this baseline. Two copies would
// be two chances for the verification hook to stop looking like the thing it
// verifies, which would make it useless in the one way that is hard to notice.
constexpr render::Color PROMPT_INK{232, 228, 214};
constexpr int PROMPT_BOTTOM_GAP = 40;

void draw_centred(render::PixelCanvas& hud, std::string_view text, int y,
                  bool plate) {
    const int w = static_cast<int>(hud.width());
    const int tw = render::text_width_px(text);
    const int tx = (w - tw) / 2;
    if (plate) {
        draw_text_plate(hud, tx, y, tw, render::FONT_INK_H);
    }
    render::draw_text(hud, tx, y, text, PROMPT_INK, /*shadow=*/true);
}

} // namespace

ToolBadge tool_badge(const ToolBadgeFacts& f) {
    ToolBadge b;
    if (!f.editor) {
        // NOT IN THE EDITOR THERE IS NO BADGE. The free camera has no reach and
        // does not interact; a verb under the crosshair of a body that is not
        // there is a ghost of the possessed player, and the user flagged
        // exactly that on the first cut.
        return b;
    }
    b.shown = true;
    if (!f.have_tool) {
        // РУКА ПУСТА — И ЭТО СОСТОЯНИЕ, А НЕ ОТСУТСТВИЕ СОСТОЯНИЯ (заказ 18.08:
        // «выбор сбросится... я буду просто бегать по игре»). Подпись говорит
        // именно это, а не молчит.
        b.name = say("editor.tool.none");
        b.action = say("tool.hint.empty");
        b.ready = false;
    } else {
        b.name = f.title;
        b.ready = f.ready;
        // ЧТО БУДЕТ ПО ЩЕЛЧКУ — спрашивается У ИНСТРУМЕНТА. Здесь стоял switch
        // из пяти веток, и одна из них сама читала призрак и приговор судьи.
        // Теперь отвечает тот, кто это знает: статус несёт либо ключ, либо
        // готовую фразу судьи, и готовая фраза сильнее ключа.
        b.action = f.status_text.empty()
                       ? std::string(f.status_key != nullptr ? say(f.status_key)
                                                             : std::string_view{})
                       : std::string(f.status_text);
        if (f.ready && f.wants_rotation && !f.group.empty()) {
            b.action += "  ";
            b.action += say("tool.group");
            b.action += f.group;
        }
    }
    // ПОКА УКАЗАТЕЛЬ НА ПАНЕЛИ, МИР НЕ ТРОГАЕТСЯ, и подпись говорит ровно это:
    // иначе щелчок по ползунку выглядит как проглоченный щелчок по земле.
    // ПОСЛЕДНИМ, поверх всего сказанного выше, потому что это правда о СЛЕДУЮЩЕМ
    // щелчке, а она сильнее правды о том, что в руке.
    if (f.ui_wants_mouse) {
        b.action = say("tool.hint.blocked");
        b.ready = false;
    }
    return b;
}

bool compose_hud(render::PixelCanvas& hud, const HudFrame& f) {
    if (f.hud_off) {
        // НИ ОДНОГО ПИКСЕЛЯ, а не «невидимый слой»: холст, на котором что-то
        // нарисовано и не показано, отличается от чистого ровно тем, чего
        // проверить нельзя.
        return false;
    }
    bool any = false;

    // ПРИЦЕЛ И ЛЕНТА. Подсказка взаимодействия рисуется по центру экрана, у
    // которого центр ничем не отмечен, — это и была жалоба на кадре ui-ingame.
    // Здоровье/силы/магия остаются единицами: тратить их пока нечем, и полоса,
    // которая ползёт для вида, учит читать пустое число.
    HudFacts facts = f.facts;
    any = draw_compass_ribbon(hud, facts) || any;
    any = draw_condition_bars(hud, facts) || any;
    any = draw_crosshair(hud, facts) || any;

    // ЧТО Я СЕЙЧАС ДЕЛАЮ И ЧТО БУДЕТ ПО ЩЕЛЧКУ — у прицела, в мире. Вопрос
    // задают, глядя на землю, которую сейчас изменят, поэтому ответ стоит там же.
    // The strings are held HERE for the length of the draw; in run() they were
    // a local kept alive by hand next to a string_view pointing into it.
    const ToolBadge badge = tool_badge(f.tool);
    if (badge.shown) {
        facts.tool_name = badge.name;
        facts.tool_action = badge.action;
        facts.tool_ready = badge.ready;
        any = draw_tool_badge(hud, facts) || any;
    }

    // THE INTERACTION VERB. The prompt stands on the same ground as the
    // readout: same ink, same font, same 5 px letters, so ui's measurement
    // applies to it word for word -- 56.1% of that ink fails the two-step
    // separation rule wherever the background is bright, and this line is drawn
    // over whatever the player happens to be facing. It was the only text left
    // without a plate.
    if (!f.prompt.empty()) {
        draw_centred(hud, f.prompt,
                     static_cast<int>(hud.height()) - PROMPT_BOTTOM_GAP,
                     /*plate=*/true);
        any = true;
    }

    // VERIFICATION HOOK (Rule 27, gated by DFN_HUD_PROBE): draws a real prompt
    // and a deliberate MISS side by side, so the placeholder is proved to be
    // unmistakable rather than assumed to be.
    if (f.probe) {
        const int h = static_cast<int>(hud.height());
        draw_centred(hud, say("prompt.take"), h - PROMPT_BOTTOM_GAP, false);
        draw_centred(hud, say("prompt.nonexistent"), h - 24, false);
        any = true;
    }

    // THE READOUT DRAWS AFTER THE PROMPT so it is never occluded by one: a
    // debug view that can be hidden by whatever else is on screen is not a
    // debug view.
    //
    // AND IT PUBLISHES WHERE IT ENDED. The editor's block stacks UNDER it
    // rather than beside it, so the two are laid out by ONE arithmetic instead
    // of being pinned to the same corner by two -- which is what they were, at
    // (3,3) and (4,4), printing through each other for anyone running with both
    // on. That pair is the reason this function exists.
    int overlay_bottom = 0;
    if (f.readout != nullptr) {
        // ОТСТУП БЕРЁТСЯ, А НЕ ПОДБИРАЕТСЯ. Полосу поставил интерфейс редактора,
        // он же её и посчитал; вывод начинается в остатке.
        draw_debug_overlay(hud, *f.readout, facts.world_x, facts.world_y);
        overlay_bottom = debug_overlay_bottom_y(*f.readout, facts.world_y);
        any = true;
    }
    if (f.editor_block != nullptr) {
        // Under the readout when it is up, at the top of the frame when it is
        // not -- so the block does not sit in the middle of an empty corner
        // just because the other panel is switched off.
        (void)draw_editor_hud(hud, *f.editor_block,
                              f.readout != nullptr ? editor_hud_top_y(overlay_bottom)
                                                   : editor_hud_top_y(0));
        any = true;
    }

    // THE CHAT WINDOW draws last so it sits over everything else on the HUD (it
    // is the thing the player is interacting with when it is up).
    if (f.chat != nullptr) {
        any = f.chat->draw(hud) || any;
    }
    return any;
}

} // namespace dfn::app
