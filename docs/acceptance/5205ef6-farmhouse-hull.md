<!--
Created: 16:08:2026 - 22:23:58
Last updated: 16:08:2026 - 22:23:58
-->
<!--
UPD:
- 16:08:2026 - 22:23:58: Кадры до/после запечатывания хутора (коммит 5205ef6).
-->

# 5205ef6 — дом как замкнутая оболочка (хутор houses/farmhouse)

Замечание пользователя: «там дырки в доме / у него стены несплошные / агент
домов должен сплошной закрытый дом делать, окна глухие, без сплошного
просвета, с имитацией вида насквозь».

- **before** (5205ef6-farmhouse-hull-before.png): снят лидом на бинарнике до
  правки. Пролёты — чёрные провалы, окно — открытая рама, щели досок насквозь.
- **after** (5205ef6-farmhouse-hull-after.png): тот же ракурс после правки
  кузницы и сцены.

Рецепт (оба кадра, стенд детерминирован, правило 13.1):
    DFN_EDITOR=1 DFN_OPEN_MAP=houses/farmhouse \
    DFN_EDITOR_CAM_REL=132.5,1.7,128,-1.5708,0.0 \
    DFN_CAPTURE_AFTER_FRAMES=120 DFN_CAPTURE_DIR=<куда> ./build_lead/engine/app/dfn_app

Контроль прибором (не кадром — кадр видит одну стену, дыр шесть сторон):
    ./build_lead/dfn_scene_check assets/scenes/house-farm.scene --stand Gallery \
        --objects assets/objects/parts --shell
    до: 216 из 18842 лучей наружу; после: 0 из 18842, hull sealed.
