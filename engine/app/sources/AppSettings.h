/*
Module: engine/app
File: engine/app/sources/AppSettings.h

Responsibility:
- НАСТРОЙКИ ГРАФИКИ: разбор текста в AppConfig и печать AppConfig в текст.
  Вынесено из App.cpp по заказу пользователя 18.08: «настройки из app cpp надо
  вынести, их чтение их печать, оно не должно быть в этом файле».

Key items:
- settings_to_text / settings_from_text: чистые функции текст <-> настройки.
- load_or_create_settings / write_settings: те же функции плюс файл.

Dependencies:
- Uses: AppConfig (App.h).
- Used by: App при старте и при выходе со страницы калибровки.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- РАЗБОР ОТДЕЛЁН ОТ ФАЙЛА, и это единственная причина, по которой вынос имеет
  смысл. Пока чтение настроек сидело внутри App.cpp вперемешку с открытием
  файла, его нельзя было проверить: App.cpp владеет окном. Текст в структуру и
  структуру в текст — чистые функции, у них есть рукав, и именно поэтому
  ОТКАЗ ОТ НЕВЕРНОГО ЗНАЧЕНИЯ теперь доказуем, а не обещан.
- ОТВЕРГАЕМ ГРОМКО, НЕ ПОДГОНЯЕМ. Молча приведённое к ближайшему допустимому
  значение выглядит как работающая настройка и рисует другой мир.
*/

#pragma once

#include <string>

namespace dfn::app {

struct AppConfig;

/// Настройки в текст, с комментариями. Тот же текст, что ложится в файл.
[[nodiscard]] std::string settings_to_text(const AppConfig& cfg);

/// Текст в настройки. Неизвестные ключи и пустые строки пропускаются молча
/// (файл правит человек, и лишняя строка — не повод падать), а НЕВЕРНЫЕ
/// значения известных ключей отвергаются ГРОМКО и оставляют прежнее.
void settings_from_text(const std::string& text, AppConfig& cfg);

/// То же плюс файл: прочитать, а если файла нет — написать умолчания.
void load_or_create_settings(AppConfig& cfg);
void write_settings(const AppConfig& cfg);

} // namespace dfn::app
