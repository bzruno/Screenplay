#pragma once
// Loads the monospace font the page is typeset in and exposes the family name
// every painter needs. Courier Prime ships with the app; Courier New is the
// fallback when it is missing.

#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <stdexcept>
#include <string>

namespace screenplay::ui {

class ScreenplayFont {
public:
    /// Qt family name of the loaded font. Valid after load().
    static const QString& family() { return family_; }

    /// True when Courier Prime was not found and Courier New is in use.
    static bool using_fallback() { return using_fallback_; }

    /// Renderer-quality settings for any QFont used in custom painting.
    /// Call right after setting family and pixel size.
    static void apply_render_quality(QFont& font) {
        font.setHintingPreference(QFont::PreferVerticalHinting);
        font.setStyleStrategy(QFont::StyleStrategy(
            QFont::PreferAntialias | QFont::PreferQuality
                                   | QFont::NoSubpixelAntialias));
    }

    /// Registers the font with Qt and returns the file actually loaded.
    /// Throws when no monospace font can be found at all.
    static std::string load() {
        if (const std::string bundled = load_courier_prime(); !bundled.empty())
            return bundled;
        if (const std::string system = load_courier_new(); !system.empty())
            return system;
        throw std::runtime_error("Courier font not found.");
    }

private:
    inline static QString family_         = "Courier Prime";
    inline static bool    using_fallback_ = false;

    static QStringList search_bases() {
        const QString app_dir = QCoreApplication::applicationDirPath();
        return { app_dir, app_dir + "/..", app_dir + "/../..", "." };
    }

    static std::string load_courier_prime() {
        for (const QString& base : search_bases()) {
            const QString dir = base + "/fonts/";
            const QString upright = first_existing({ dir + "CourierPrime-Medium.ttf",
                                                     dir + "CourierPrime-Regular.ttf" });
            if (upright.isEmpty()) continue;

            family_ = register_family(upright, "Courier Prime");

            // Real glyphs beat synthesized ones. Only ONE upright weight is
            // registered: adding both Medium and Regular under a single family
            // lets Regular claim weight 400 and override the Medium default.
            for (const char* variant : { "CourierPrime-Bold.ttf",
                                         "CourierPrime-Italic.ttf",
                                         "CourierPrime-BoldItalic.ttf" }) {
                const QString path = dir + variant;
                if (QFile::exists(path))
                    QFontDatabase::addApplicationFont(path);
            }
            return upright.toStdString();
        }
        return {};
    }

    static std::string load_courier_new() {
        const QString path = first_existing({ "C:\\Windows\\Fonts\\CourierNew.ttf",
                                              "C:\\Windows\\Fonts\\cour.ttf" });
        if (path.isEmpty()) return {};
        using_fallback_ = true;
        family_ = register_family(path, "Courier New");
        return path.toStdString();
    }

    static QString first_existing(const QStringList& candidates) {
        for (const QString& path : candidates)
            if (QFile::exists(path)) return path;
        return {};
    }

    static QString register_family(const QString& path, const QString& fallback_name) {
        const int id = QFontDatabase::addApplicationFont(path);
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? fallback_name : families.first();
    }
};

} // namespace screenplay::ui
