#pragma once
// ui/theme_manager.hpp
// ThemeManager — owns which ThemePalette is active and knows how to turn it
// into an application-wide QPalette + stylesheet. This is the single place
// that decides "what theme are we in"; every other class asks it, nothing
// else stores its own dark/light flag.
//
// Owned responsibility: current theme selection, persistence, and generating
// the global chrome stylesheet from DesignTokens + the active ThemePalette.
// It does NOT touch the screenplay canvas's own paintEvent — that code reads
// the palette values it needs directly (via the MD3 compatibility facade in
// main.cpp), same as before.

#include "theme_palette.hpp"
#include "design_tokens.hpp"
#include "typography.hpp"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <functional>
#include <vector>

namespace screenplay::ui {

class ThemeManager {
public:
    enum class Theme { Light, Dark, Chamber };

    static ThemeManager& instance() {
        static ThemeManager mgr;
        return mgr;
    }

    Theme                theme()   const { return theme_; }
    const ThemePalette&  palette() const { return palette_; }

    static const char* theme_name(Theme t) {
        switch (t) {
        case Theme::Light:   return "Light";
        case Theme::Dark:    return "Dark";
        case Theme::Chamber: return "Chamber";
        }
        return "Dark";
    }

    // Cycles Light -> Dark -> Chamber -> Light. Used by the existing
    // toolbar/menu theme-toggle affordance so no new button is needed to
    // reach all three themes.
    Theme next_theme() const {
        switch (theme_) {
        case Theme::Light:   return Theme::Dark;
        case Theme::Dark:    return Theme::Chamber;
        case Theme::Chamber: return Theme::Light;
        }
        return Theme::Dark;
    }

    void set_theme(Theme t) {
        theme_   = t;
        palette_ = palette_for(t);
        QSettings().setValue("view/theme", static_cast<int>(t));
        apply_to_app();
        if (on_change_) on_change_();
    }

    // Register a callback invoked after every set_theme() — mirrors the
    // existing LanguageConfig::set_on_change pattern so callers restyle their
    // own widgets (docks, panels) the same way they already do for language.
    void set_on_change(std::function<void()> cb) { on_change_ = std::move(cb); }

    // Loads the persisted theme (migrating the older bool "view/dark_theme"
    // key transparently) and applies it. Call once at startup.
    void load_and_apply() {
        QSettings s;
        if (s.contains("view/theme")) {
            theme_ = static_cast<Theme>(
                s.value("view/theme", (int)Theme::Dark).toInt());
        } else if (s.contains("view/dark_theme")) {
            // Migrate a pre-existing install's bool preference.
            theme_ = s.value("view/dark_theme", true).toBool()
                ? Theme::Dark : Theme::Light;
        } else {
            theme_ = Theme::Dark;
        }
        palette_ = palette_for(theme_);
        apply_to_app();
        if (on_change_) on_change_();   // let MD3::sync() (and similar) run once
    }

    // Builds the QApplication-wide QPalette + stylesheet from the active
    // ThemePalette and DesignTokens. Equivalent to the previous free function
    // apply_app_theme(), just sourced from the palette object instead of
    // loose globals, and reading spacing/radius from DesignTokens instead of
    // literal numbers.
    void apply_to_app() const {
        const ThemePalette& p = palette_;

        QPalette pal;
        pal.setColor(QPalette::Window,          p.Surface());
        pal.setColor(QPalette::WindowText,      p.OnSurface());
        pal.setColor(QPalette::Base,            p.Bg1);
        pal.setColor(QPalette::AlternateBase,   p.SurfaceVar());
        pal.setColor(QPalette::Text,            p.OnSurface());
        pal.setColor(QPalette::Button,          p.SurfaceVar());
        pal.setColor(QPalette::ButtonText,      p.OnSurface());
        pal.setColor(QPalette::Highlight,       p.PrimaryContainer());
        pal.setColor(QPalette::HighlightedText, p.Text);
        pal.setColor(QPalette::ToolTipBase,     p.Bg1);
        pal.setColor(QPalette::ToolTipText,     p.Text);
        pal.setColor(QPalette::PlaceholderText, p.TextDim);
        qApp->setPalette(pal);

        using S = Spacing;
        using R = Radius;

        // Every literal below is a token — a 4/8/12/16/24/32/48 grid value, a
        // named Radius level, or a ControlHeight. No arbitrary numbers.
        //
        // The rules encode the redesign's structural decisions:
        //   · the menu bar is deliberately near-invisible (no background of
        //     its own, no bottom rule) so it dissolves into the header;
        //   · transient surfaces that are their own OS WINDOW (QMenu, the
        //     QComboBox popup, QToolTip) are square — see the note above the
        //     QMenu rule; everything that is an in-window widget is rounded;
        //   · no widget carries a heavy border.
        QString ss = QString::fromUtf8(
            // ── Base ─────────────────────────────────────────────────────────
            "QWidget { font-family:{font}; }"
            "QMainWindow, QDialog { background:{bg0}; }"

            // ── Menu bar: present, but visually dissolved into the header ────
            "QMenuBar { background:transparent; color:{dim}; border:none;"
            "           font-size:{fs_body}px; padding:0px {s_s}px; }"
            "QMenuBar::item { background:transparent; padding:{s_xs}px {s_m}px;"
            "                 margin:0px 1px; border-radius:{r_chip}px; }"
            "QMenuBar::item:selected { background:{hoverSoft}; color:{text}; }"
            "QMenuBar::item:pressed  { background:{pressedSoft}; color:{text}; }"

            // ── Menus / dropdowns: floating cards ────────────────────────────
            // Popups are their own top-level OS windows, and those windows are
            // rectangular. A border-radius here rounds only what gets PAINTED —
            // the real corner pixels stay, showing as light (or, with a
            // half-working translucency hack, black) squares behind the menu.
            // Square popups have no corners to leak, which is why every popup
            // surface below is deliberately radius-free. Rounded menus would
            // need per-window translucency set up before the native handle
            // exists — not worth the fragility for a dropdown.
            "QMenu { background:{card}; color:{text};"
            "        border:{hw}px solid {border}; border-radius:0px;"
            "        padding:{s_s}px; font-size:{fs_body}px; }"
            "QMenu::item { padding:{s_s}px {s_l}px; margin:1px {s_xs}px;"
            "              border-radius:{r_chip}px; }"
            "QMenu::item:selected { background:{hover}; }"
            "QMenu::item:disabled { color:{dim}; }"
            "QMenu::separator { height:{hw}px; background:{divider};"
            "                   margin:{s_s}px {s_m}px; }"
            "QMenu::indicator { width:{ic_sm}px; height:{ic_sm}px;"
            "                   margin-left:{s_s}px; }"

            // ── Dialogs ──────────────────────────────────────────────────────
            "QMessageBox { background:{card}; color:{text}; }"
            "QDialog QLabel { color:{text}; font-size:{fs_body}px; }"
            "QDialogButtonBox QPushButton { min-width:88px; }"

            // ── Docks / panels ───────────────────────────────────────────────
            "QDockWidget { color:{text}; font-size:{fs_body}px;"
            "              titlebar-close-icon:none; titlebar-normal-icon:none; }"
            // The title is the panel's name, so it is set like a label
            // rather than left at the default: uppercased, tracked out and
            // quiet, so it reads as a section heading instead of a window
            // caption competing with the content below it.
            "QDockWidget::title { background:transparent; border:none;"
            "                     padding:{s_m}px {s_l}px {s_s}px {s_l}px;"
            "                     text-align:left; }"
            // Side panels state their own bounds. Since the window is one
            // uniform colour, a dock without an outline is indistinguishable
            // from the page area beside it — this hairline is what separates
            // them. Set on the panel body (objectName below), not on
            // QDockWidget itself, whose own frame styling is unreliable.
            "QWidget#dockPanel { border:{hw}px solid {border};"
            "                    border-radius:{r_card}px; }"

            // ── Scrollbars: thin, fully rounded, no arrow buttons ───────────
            "QScrollBar:vertical { background:transparent; width:{sb_w}px;"
            "                      margin:{s_xs}px; }"
            "QScrollBar::handle:vertical { background:{scrollbar};"
            "        border-radius:{sb_r}px; min-height:{sb_min}px; }"
            "QScrollBar::handle:vertical:hover { background:{scrollbarHover}; }"
            "QScrollBar:horizontal { background:transparent; height:{sb_w}px;"
            "                        margin:{s_xs}px; }"
            "QScrollBar::handle:horizontal { background:{scrollbar};"
            "        border-radius:{sb_r}px; min-width:{sb_min}px; }"
            "QScrollBar::handle:horizontal:hover { background:{scrollbarHover}; }"
            "QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }"
            "QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }"

            // ── Buttons ──────────────────────────────────────────────────────
            // (The chrome's own buttons are custom-painted SoftButtons; these
            //  rules cover the ones Qt creates for us inside dialogs.)
            "QToolButton { border:none; border-radius:{r_btn}px;"
            "              padding:{s_xs}px; color:{text}; }"
            "QToolButton:hover { background:{hoverSoft}; }"
            "QToolButton:pressed { background:{pressedSoft}; }"
            "QToolButton:checked { background:{hover}; }"
            "QToolButton:disabled { color:{dim}; }"
            "QToolButton::menu-indicator { image:none; }"
            "QPushButton { background:{card}; color:{text};"
            "              border:{hw}px solid {border}; border-radius:{r_btn}px;"
            "              padding:{s_m}px {s_xl}px; font-size:{fs_body}px; }"
            "QPushButton:hover { background:{hover}; }"
            "QPushButton:pressed { background:{pressed}; }"
            "QPushButton:disabled { background:{bg1}; color:{dim}; }"
            "QPushButton:default { background:{primary}; color:{onPrimary};"
            "                      border:{hw}px solid {primary}; }"

            // ── Inputs ───────────────────────────────────────────────────────
            "QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit {"
            "            background:{bg1}; color:{text};"
            "            border:{hw}px solid {border}; border-radius:{r_btn}px;"
            "            padding:{s_s}px {s_m}px; font-size:{fs_body}px;"
            "            selection-background-color:{selection};"
            "            selection-color:{text}; }"
            "QLineEdit:focus, QComboBox:focus, QSpinBox:focus,"
            "QPlainTextEdit:focus, QTextEdit:focus {"
            "            border:{hw}px solid {primary}; background:{card}; }"
            "QComboBox::drop-down { border:none; width:{s_xl}px; }"
            "QComboBox QAbstractItemView { background:{card}; color:{text};"
            "            border:{hw}px solid {border}; border-radius:0px;"
            "            padding:{s_xs}px; outline:none;"
            "            selection-background-color:{hover};"
            "            selection-color:{text}; }"
            "QCheckBox, QRadioButton { color:{text}; font-size:{fs_body}px;"
            "                          spacing:{s_s}px; }"

            // ── Lists / tables ───────────────────────────────────────────────
            "QListWidget, QTreeWidget, QTableWidget {"
            "            background:transparent; border:none; outline:none;"
            "            font-size:{fs_body}px; }"
            "QListWidget::item, QTreeWidget::item {"
            "            padding:{s_s}px {s_m}px; border-radius:{r_chip}px;"
            "            margin:1px {s_xs}px; }"
            "QListWidget::item:hover, QTreeWidget::item:hover {"
            "            background:{hoverSoft}; }"
            "QListWidget::item:selected, QTreeWidget::item:selected {"
            "            background:{selection}; color:{text}; }"
            "QTableWidget::item { padding:{s_xs}px {s_s}px; }"
            "QTableWidget::item:selected { background:{selection}; color:{text}; }"
            "QHeaderView::section { background:transparent; color:{dim};"
            "            border:none; border-bottom:{hw}px solid {divider};"
            "            padding:{s_s}px; font-size:{fs_small}px; }"

            // ── Tabs ─────────────────────────────────────────────────────────
            "QTabWidget::pane { border:{hw}px solid {border};"
            "                   border-radius:{r_card}px; }"
            "QTabBar::tab { background:transparent; color:{dim};"
            "               padding:{s_s}px {s_l}px; margin-right:{s_xs}px;"
            "               border-radius:{r_chip}px; font-size:{fs_body}px; }"
            "QTabBar::tab:selected { background:{hover}; color:{text}; }"

            // ── Tooltips ─────────────────────────────────────────────────────
            // Tooltips are a whisper, not a panel: minimal padding and the
            // smallest type in the scale. They appear under the pointer and
            // must never obscure what is being pointed at.
            "QToolTip { background:{card}; color:{text};"
            "           border:{hw}px solid {border}; border-radius:0px;"
            "           padding:2px {s_s}px; font-size:{fs_caption}px; }"

            // ── Misc ─────────────────────────────────────────────────────────
            "QSplitter::handle { background:{divider}; }"
            "QProgressBar { background:{bg1}; border:none;"
            "               border-radius:{r_chip}px; height:{s_s}px;"
            "               text-align:center; }"
            "QProgressBar::chunk { background:{primary};"
            "                      border-radius:{r_chip}px; }");

        ss.replace("{bg0}",        ThemePalette::hex(p.Bg0));
        ss.replace("{bg1}",        ThemePalette::hex(p.Bg1));
        ss.replace("{card}",       ThemePalette::hex(p.Card));
        ss.replace("{border}",     ThemePalette::hex(p.Border));
        ss.replace("{divider}",    ThemePalette::hex(p.Divider));
        ss.replace("{text}",       ThemePalette::hex(p.Text));
        ss.replace("{dim}",        ThemePalette::hex(p.TextDim));
        ss.replace("{hover}",      ThemePalette::hex(p.HoverBg));
        ss.replace("{hoverSoft}",  p.hoverSoft());
        ss.replace("{pressed}",    ThemePalette::hex(p.PressedBg));
        ss.replace("{pressedSoft}",p.pressedSoft());
        ss.replace("{selection}",  ThemePalette::hex(p.SelectionBg));
        ss.replace("{scrollbar}",      ThemePalette::hex(p.ScrollbarBg));
        ss.replace("{scrollbarHover}", ThemePalette::hex(p.ScrollbarHoverBg));
        ss.replace("{primary}",    ThemePalette::hex(p.Primary));
        ss.replace("{onPrimary}",  ThemePalette::hex(p.OnPrimary()));
        ss.replace("{font}",       "'" + Typography::family() + "'");
        ss.replace("{hw}",         QString::number(BorderWidth::Hairline));
        ss.replace("{s_xs}",       QString::number(S::XS));
        ss.replace("{s_s}",        QString::number(S::S));
        ss.replace("{s_m}",        QString::number(S::M));
        ss.replace("{s_l}",        QString::number(S::L));
        ss.replace("{s_xl}",       QString::number(S::XL));
        ss.replace("{r_chip}",     QString::number(R::Chip));
        ss.replace("{r_btn}",      QString::number(R::Button));
        ss.replace("{r_menu}",     QString::number(R::Menu));
        ss.replace("{r_card}",     QString::number(R::Card));
        ss.replace("{ic_sm}",      QString::number(IconSize::Small));
        ss.replace("{fs_body}",    QString::number(
                                       Typography::size_px(Typography::Size::Body)));
        ss.replace("{fs_small}",   QString::number(
                                       Typography::size_px(Typography::Size::BodySmall)));
        ss.replace("{fs_caption}", QString::number(
                                       Typography::size_px(Typography::Size::Caption)));
        ss.replace("{sb_w}",       QString::number(ScrollbarMetrics::Width));
        ss.replace("{sb_r}",       QString::number(ScrollbarMetrics::HandleRadius));
        ss.replace("{sb_min}",     QString::number(ScrollbarMetrics::MinHandleLength));
        qApp->setStyleSheet(ss);
    }

private:
    ThemeManager() : palette_(ThemePalette::dark()) {}

    static ThemePalette palette_for(Theme t) {
        switch (t) {
        case Theme::Light:   return ThemePalette::light();
        case Theme::Dark:    return ThemePalette::dark();
        case Theme::Chamber: return ThemePalette::chamber();
        }
        return ThemePalette::dark();
    }

    Theme                  theme_   = Theme::Dark;
    ThemePalette           palette_;
    std::function<void()>  on_change_;
};

} // namespace screenplay::ui
