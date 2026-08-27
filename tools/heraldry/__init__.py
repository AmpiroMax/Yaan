#!/usr/bin/env python3
#
# Created: 27:08:2026 - 13:52:00
# Last updated: 27:08:2026 - 13:52:00
# Module: tools
# File: tools/heraldry/__init__.py
#
# Responsibility:
# - Делает tools/heraldry пакетом. Содержимого нет и не должно быть: импорт
#   пакета не обязан ничего считать, а модули тут тяжёлые (numpy, чтение PNG).
#
# Dependencies:
# - Used by: tools/gen_heraldry.py.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly.
#
# UPD:
# - 27:08:2026 - 13:52:00: Создан вместе с пакетом генератора 3D-герба.
#
"""Offline heraldry baking: silhouette PNG to a relief 3D object."""
