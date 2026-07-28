// main.cpp — Screenplay Editor  (Windows 11 · Qt6 · Material Design 3 Dark)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QTimer>
#include <QAction>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QDockWidget>
#include <QListWidget>
#include <QMessageBox>
#include <QShortcut>
#include <QDir>
#include <QStandardPaths>
#include <QMenu>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QToolButton>
#include <QActionGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QRadioButton>
#include <QGroupBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QDesktopServices>
#include <QPrinter>
#include <QPrintDialog>
#include <QInputDialog>
#include <QPushButton>
#include <QMenuBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QSettings>
#include <QElapsedTimer>

#include "layout/freetype_metrics.hpp"
#include "layout/layout_engine.hpp"
#include "editor/editor_controller.hpp"
#include "render/renderer.hpp"
#include "io/exporter.hpp"
#include "io/text_exporter.hpp"
#include "io/importer.hpp"
#include "io/pdf_exporter.hpp"
#include "ui/design_tokens.hpp"
#include "ui/theme_palette.hpp"
#include "ui/theme_manager.hpp"
#include "ui/app_palette.hpp"
#include "ui/screenplay_font.hpp"
#include "ui/toast.hpp"
#include "ui/script_language_dialog.hpp"
#include "ui/autocomplete_popup.hpp"
#include "ui/search_bar.hpp"
#include "editor/script_search.hpp"
#include "editor/spell_cache.hpp"
#include "production/scene_numbering.hpp"
#include "reports/script_reports.hpp"
#include "ui/cover_editor.hpp"
#include "ui/report_dialog.hpp"
#include "ui/revision_dialog.hpp"
#include "ui/scroll_animator.hpp"
#include "ui/page_metrics.hpp"
#include "ui/panel_frame.hpp"
#include "ui/script_panel.hpp"
#include "ui/typography.hpp"
#include "ui/icon_manager.hpp"
#include "ui/elevation.hpp"
#include "ui/controls.hpp"
#include "ui/floating_toolbar.hpp"
#include "ui/app_header.hpp"
#include "ui/status_strip.hpp"
#include "ui/window_frame.hpp"
#include "ui/confirm_dialog.hpp"
#include "ui/app_dialog.hpp"
#include "config/ui_strings.hpp"

// Shorthand for the UI translation lookup. Declared at file scope because
// every class in this translation unit (canvas, panels, dialogs, MainWindow)
// shows user-visible text and must translate it the same way.
using screenplay::config::tr_ui;
using screenplay::config::tr_block_label;
using screenplay::ui::block_color;
using screenplay::ui::contrast_text;
using screenplay::ui::Toast;
using screenplay::ui::ScriptLanguageDialog;
using screenplay::ui::AutocompletePopup;
using screenplay::ui::SearchBar;
using screenplay::ui::ScriptPanel;

#ifdef Q_OS_WIN
// Only for the WS_THICKFRAME restoration in MainWindow::showEvent() — see the
// comment there. NOMINMAX/WIN32_LEAN_AND_MEAN keep windows.h from colliding
// with std::min/std::max and from dragging in the whole Win32 surface.
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif
#include "config/app_config.hpp"
#include "stats/script_stats.hpp"
#include "stats/scene_character_index.hpp"
#include "spellcheck/spell_checker.hpp"
#include "database/script_index.hpp"
#include "config/language.hpp"
#include "version.hpp"

#include <memory>
#include <algorithm>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Colour tokens — a flat, neutral greyscale system (Word / Pages / Final Draft
// feel), NOT Material Design. Mutable so the whole app flips dark ↔ light;
// every widget reads these (directly or via generated stylesheets), so the
// palette is the single source of truth. Kept in namespace MD3 for call-site
// compatibility — the values are pure neutral greys, no lavender/blue/warm.
//
// The only non-grey tokens are: Accent (a restrained neutral graphite used
// for focus rings), and WarnAccent (a muted, desaturated red reserved for
// genuine caution semantics — unsaved state, no matches, errors). Everything
// else is white → grey → graphite.
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// Page separation — flat, vector look (Word / Pages). The page is a plain
// 2D rectangle; the 1px border does the real separating. We add only a
// whisper of a shadow (a few px, single-digit alpha, no offset) so the page
// reads as distinct from the canvas without ever looking like it floats.
// Deliberately no corner radius: the page is a sharp rectangle.
// ─────────────────────────────────────────────────────────────────────────────
// A screenplay font plus its metrics, kept together because every caller that
// wants one wants the other.
#include "screenplay_canvas.hpp"

#include "main_window.hpp"


// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────
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
