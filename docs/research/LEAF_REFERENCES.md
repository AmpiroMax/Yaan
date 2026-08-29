<!--
Module: docs
File: docs/research/LEAF_REFERENCES.md

Responsibility:
- Реестр фото-эталонов флоры (решение пользователя 16.08.2026: «листочки должны
  быть скачены и быть примерами как должно выглядеть; для сверки, не для
  источника правды... не выдумывать просто так, а сверяться с истиной, тоже
  самое для коры»). Файлы лежат в artifacts/reference/<порода>/, движок их НЕ
  читает (Q13 живо, конвейер процедурный) — они для ГЛАЗ при каждой итерации
  штампов листьев и коры.

Key items:
- Таблица «порода → файл → источник → лицензия → что берём»; правило сверки.

Dependencies:
- Uses: Wikimedia Commons, ambientCG (ссылки в таблице).
- Used by: FloraCards.cpp (калибровка штампов/коры), TREE_PASSPORTS.md.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ПРАВИЛО СВЕРКИ (пользователь, дословно передано лидом): каждая правка штампа
  листа или пластины коры кладётся РЯДОМ с эталоном и смотрится вместе. Замер
  впечатлением не является; A/B-картинка — является.
- Лицензии соблюдены (CC0 / CC BY / CC BY-SA, атрибуция ниже). Пользователь
  планирует заменить набор своими фото ближе к релизу.
-->

# Фото-эталоны флоры (artifacts/reference/)

## Правило

Эталон — то, «что надо почаще смотреть и брать в пример». Меняешь штамп листа
или кору → построй A/B (эталон слева, наш тайл справа), посмотри глазами,
только потом коммить. Частоты/цвета, снятые с эталонов, документируются в
UPD файла-потребителя.

## Листья и ветви

| Порода | Файл | Источник | Лицензия | Что берём |
|---|---|---|---|---|
| oak | oak_0_Quercus_robur_leaf.jpg | [commons](https://commons.wikimedia.org/wiki/File:Quercus_robur_leaf.jpg) | CC BY-SA 4.0, Knopik-som | силуэт лопастей одного листа |
| oak | oak_1_...Herbarium._Oak._img-062.jpg | [commons](https://commons.wikimedia.org/wiki/File:2020_year._Herbarium._Oak._img-062.jpg) | CC BY-SA 4.0, Knopik-som | ветка с листьями, расположение |
| oak | oak_2_Neuchâtel...Quercus_pubescens.jpg | [commons](https://commons.wikimedia.org/wiki/File:Neuch%C3%A2tel_Herbarium_-_Quercus_pubescens_-_NEU000046168.jpg) | CC BY-SA 3.0, Neuchâtel Herbarium | гербарный лист целиком |
| oak | ambientcg_LeafSet012_color.png | [ambientCG LeafSet012](https://ambientcg.com/view?id=LeafSet012) | CC0 | осенние дубовые листья, атлас с альфой |
| birch | birch_0/1_...Herbarium._Betula.jpg | [commons](https://commons.wikimedia.org/wiki/File:2020_year._Herbarium._Betula._img-070.jpg) | CC BY-SA 4.0, Knopik-som | ромбовидный лист, зубцы, веточки |
| birch | ambientcg_LeafSet005_color.png | [ambientCG LeafSet005](https://ambientcg.com/view?id=LeafSet005) | CC0 | ветка с листьями — образец «пачки» |
| aspen | aspen_0/1_...Populus_tremula.jpg | [commons](https://commons.wikimedia.org/wiki/File:2020_year._Herbarium._Populus_tremula._img-026.jpg) | CC BY-SA 4.0, Knopik-som | круглый «монетный» лист, черешок |
| spruce | spruce_0/1_...Picea_abies.jpg | [commons](https://commons.wikimedia.org/wiki/File:2020_year._Herbarium._Picea_abies._img-001.jpg) | CC BY-SA 4.0, Knopik-som | хвоя вдоль веточки, строение лапы |
| spruce | ambientcg_LeafSet019_color.png | [ambientCG LeafSet019](https://ambientcg.com/view?id=LeafSet019) | CC0 | атлас хвойных веточек с альфой |
| pine | pine_0/1_Pinus_sylvestris_2026_G1/G2.jpg | [commons](https://commons.wikimedia.org/wiki/File:Pinus_sylvestris_2026_G1.jpg) | CC BY 4.0, George Chernilevsky | пары длинных игл, пучки |
| larch | larch_0_Neuchâtel...Larix_decidua.jpg | [commons](https://commons.wikimedia.org/wiki/File:Neuch%C3%A2tel_Herbarium_-_Larix_decidua_-_NEU000003686.tif) | CC BY-SA 3.0, Neuchâtel Herbarium | кисточки хвои на веточке |
| larch | larch_1_Larch_branch-tree...jpg | [commons](https://commons.wikimedia.org/wiki/File:Larch_branch-tree_-_geograph.org.uk_-_484651.jpg) | CC BY-SA 2.0, geograph.org.uk | живая ветвь, посадка кисточек |

## Кора

| Порода | Файл | Источник | Лицензия | Что берём |
|---|---|---|---|---|
| дуб | bark/ambientcg_Bark012_oak_color.jpg | [ambientCG Bark012](https://ambientcg.com/view?id=Bark012) | CC0 | шаг борозд 4-6/8-10 см, макро 20 см, светлоты p5-p95 0.41-0.65 — снято в FloraCards v3 |
| хвоя | bark/ambientcg_Bark014_fir_color.jpg | [ambientCG Bark014](https://ambientcg.com/view?id=Bark014) | CC0 | пластины пихты/ели |

Полные наборы (нормали/дисплейсмент ambientCG, фотосканы Polyhaven pine_forest)
лежат в скретчпаде сессии и скачиваются заново по ссылкам — в репо только то,
на что смотрим глазами.
