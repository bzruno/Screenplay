#pragma once
// ui/typography.hpp
// Typography — the UI chrome's font family and size scale. Courier Prime is
// deliberately NOT available here: it belongs exclusively to the screenplay
// page, sourced via g_courier_family in main.cpp. Every chrome widget (menu,
// toolbar, status bar, dialog) should build its QFont through this class
// instead of writing "Segoe UI" or a literal pixel size.
//
// Owned responsibility: chrome font family resolution (with fallback) and a
// named size/weight scale. Colour lives in ThemePalette; spacing/radius in
// DesignTokens.

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>

namespace screenplay::ui {

class Typography {
public:
    // Named type scale — every chrome font size in the app is one of these,
    // never a literal pixel value. See size_px() for the px mapping.
    enum class Size { Caption, BodySmall, Body, Label, Subtitle, Title };
    enum class Weight { Regular, Medium, Semibold };       // never heavy Bold

    // Resolves once per process: Inter, else Segoe UI Variable, else Segoe UI.
    // Never Courier Prime — that font is reserved for the screenplay page.
    static QString family() {
        static const QString resolved = resolve_family();
        return resolved;
    }

    static int size_px(Size s) {
        switch (s) {
        case Size::Caption:   return 10;   // de-emphasized (headers, page badge)
        case Size::BodySmall: return 11;   // secondary/status text, lists, tables
        case Size::Body:      return 12;   // default chrome text
        case Size::Label:     return 11;   // form labels — same size, Medium weight
        case Size::Subtitle:  return 13;   // slightly emphasized
        case Size::Title:     return 14;   // document name, largest chrome text
        }
        return 12;
    }

    static int weight(Weight w) {
        switch (w) {
        case Weight::Regular:  return QFont::Normal;
        case Weight::Medium:   return QFont::Medium;
        case Weight::Semibold: return QFont::DemiBold;
        }
        return QFont::Normal;
    }

    // Builds a ready-to-use chrome QFont. Callers still call
    // apply_render_quality() themselves (that hook also configures hinting
    // shared with the Courier Prime path and stays in main.cpp).
    static QFont ui_font(Size s = Size::Body, Weight w = Weight::Regular) {
        QFont f(family());
        f.setPixelSize(size_px(s));
        f.setWeight(static_cast<QFont::Weight>(weight(w)));
        return f;
    }

private:
    static QString resolve_family() {
        const QStringList families = QFontDatabase::families();
        if (families.contains("Inter", Qt::CaseInsensitive))
            return "Inter";
        if (families.contains("Segoe UI Variable", Qt::CaseInsensitive))
            return "Segoe UI Variable";
        return "Segoe UI";   // always present on Windows — final fallback
    }
};

} // namespace screenplay::ui
