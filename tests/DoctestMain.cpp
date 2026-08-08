/*
Created: 09:08:2026 - 00:35:09
Last updated: 09:08:2026 - 00:35:09
Module: tests
File: tests/DoctestMain.cpp

Responsibility:
- Single doctest main() translation unit shared by every test executable.

Dependencies:
- Uses: doctest.
- Used by: dfn_doctest_main static library (tests/CMakeLists.txt).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 09:08:2026 - 00:35:09: Created.
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
