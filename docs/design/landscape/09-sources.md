<!--
Created: 12:08:2026 - 22:57:02
Last updated: 12:08:2026 - 22:57:02
-->
<!--
UPD:
- 12:08:2026 - 22:57:02: Выделен из docs/design/LANDSCAPE.md (9786 строк против FILE_HARD_LIMIT 800): §9. Чистый перенос — ни одна строка тела не изменена, ни один номер секции не изменён; адреса вида «LANDSCAPE.md §X» продолжают действовать, таблица § → файл живёт в docs/design/LANDSCAPE.md.
-->

## 9. Sources

Consulted 09:08:2026. Engine-internal grounding: NUMBERS.md, DECISIONS.md
(Q12/Q41/Q45/Q46), `engine/world/sources/Worldgen.cpp` (octaves, quantization
contract), devlog sync №2; installed skills `level-design` (pacing, critical
path, readability, blockout checklist) and `procedural-gen` (fBm,
redistribution, biome lookup, blue-noise scatter, determinism checklist).

- Breath of the Wild triangle rule (Fujibayashi/Yonezu/Dohta, GDC/CEDEC 2017):
  [GamingBolt summary](https://gamingbolt.com/the-legend-of-zelda-breath-of-the-wilds-ingenious-world-design-owes-itself-to-triangles),
  [Nintendo Life report](https://www.nintendolife.com/news/2017/10/zelda_breath_of_the_wilds_ingenious_design_is_all_about_triangles_apparently)
- Robert Yang — [Open world level design: spatial composition and flow in
  Breath of the Wild](https://www.blog.radiator.debacle.us/2017/10/open-world-level-design-spatial.html)
  (scale hierarchy, occlusion/reveal, orbiting, curved paths)
- GMTK — [How Nintendo Solved Zelda's Open World](https://gmtk.substack.com/p/how-nintendo-solved-zeldas-open-world)
  (attraction distribution, progressive revelation, motivation-based pull)
- Joel Burgess — [Skyrim's Modular Level Design, GDC 2013 transcript](https://level-design.org/?p=1643)
  and [Motivating Players in Open World Games, GDC 2011](http://blog.joelburgess.com/2011/03/gdc-2011-transcript-motivating-players.html)
  (landmark-driven motivation, modular kits; the "weenie" lineage in Bethesda
  worlds)
- The "weenie" concept — [Theory of Theme Parks: Wayfinding in Themed Design](http://theoryofthemeparks.blogspot.com/2015/08/wayfinding-in-themed-design-weenie.html)
- Amit Patel / Red Blob Games — [Polygonal Map Generation for Games](http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/)
  (rivers downhill via descent, pond-and-spill, elevation redistribution,
  elevation+moisture biome lookup)
- Jaap van Muijden (Guerrilla Games) — [GPU-Based Run-Time Procedural
  Placement in Horizon Zero Dawn, GDC 2017](https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn)
  (layered density/exclusion maps, ecotope-driven scatter, water-proximity
  density)
- The Level Design Book — [Landscape](https://book.leveldesignbook.com/process/blockout/massing/landscape)
  (walkable slopes, bowls/ridges vocabulary, water curves, trees as walls,
  rain-shadow reasoning)


