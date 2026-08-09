/*
Created: 09:08:2026 - 00:50:00
Last updated: 09:08:2026 - 00:50:00
Module: engine/app
File: engine/app/sources/Main.cpp

Responsibility:
- Entry point. Rule 22: parse config, construct App, run. Nothing else.

Dependencies:
- Uses: App.
- Used by: dfn_app executable target.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 09:08:2026 - 00:50:00: Created.
*/

#include "engine/app/sources/App.h"

#include <cstdio>

int main() {
    dfn::app::App app;
    const dfn::app::AppConfig config = dfn::app::AppConfig::from_env();
    if (!app.init(config)) {
        std::fprintf(stderr, "dfn: init failed\n");
        app.shutdown();
        return 1;
    }
    const int rc = app.run();
    app.shutdown();
    return rc;
}
