// main.cpp — Screenplay Editor entry point.
//
// The application's two main widgets now live in their own headers:
//   screenplay_canvas.hpp — the editing surface (ScreenplayCanvas)
//   main_window.hpp       — the window shell (MainWindow)
// so this translation unit is just main(). main_window.hpp pulls in everything
// the app needs; only the handful of symbols main() itself touches are included
// explicitly below.

#include <QApplication>

#include "main_window.hpp"
#include "ui/theme_manager.hpp"
#include "ui/app_palette.hpp"

int main(int argc, char* argv[]) {
    // ── Rendering quality — must be set before QApplication ──────────────
    // FreeType backend for consistent high-quality rendering of custom-loaded
    // fonts (Courier Prime via addApplicationFont). DirectWrite mismatches
    // with the FreeType loader path, causing fallback quality.
    qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");

    // Note: Qt::AA_UseHighDpiPixmaps and Qt::AA_EnableHighDpiScaling are
    // deprecated in Qt6 (always enabled) — removed to avoid warnings.

    QApplication app(argc, argv);
    app.setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    app.setApplicationName("Screenplay Editor");
    app.setOrganizationName("IndieTools");
    app.setStyle("Fusion");

    // Theme: MD3::sync() keeps the compatibility globals in step with
    // whatever ThemeManager decides (Light/Dark/Chamber, restored or
    // migrated from the older bool preference), on load and on every
    // later change.
    screenplay::ui::ThemeManager::instance().set_on_change(&MD3::sync);
    screenplay::ui::ThemeManager::instance().load_and_apply();

    MainWindow w;
    w.show();
    return app.exec();
}
