/*
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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
