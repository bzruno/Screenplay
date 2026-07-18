#pragma once
// config/app_config.hpp
// Persists user preferences via QSettings (Windows Registry / INI on Win11).

#include <QSettings>
#include <QString>
#include <QByteArray>

namespace screenplay::config {

class AppConfig {
public:
    // Singleton
    static AppConfig& instance() {
        static AppConfig cfg;
        return cfg;
    }

    // ── Window geometry ───────────────────────────────────────────────────
    void save_geometry(const QByteArray& geom) {
        s_.setValue("window/geometry", geom);
    }
    QByteArray load_geometry() const {
        return s_.value("window/geometry").toByteArray();
    }

    void save_state(const QByteArray& state) {
        s_.setValue("window/state", state);
    }
    QByteArray load_state() const {
        return s_.value("window/state").toByteArray();
    }

    // ── Editor prefs ──────────────────────────────────────────────────────
    float zoom() const {
        return s_.value("editor/zoom", 1.0f).toFloat();
    }
    void set_zoom(float z) {
        s_.setValue("editor/zoom", z);
    }

    QString font_path() const {
        return s_.value("editor/font_path", "").toString();
    }
    void set_font_path(const QString& p) {
        s_.setValue("editor/font_path", p);
    }

    // ── Recent files ──────────────────────────────────────────────────────
    QStringList recent_files() const {
        return s_.value("files/recent").toStringList();
    }
    void add_recent_file(const QString& path) {
        auto list = recent_files();
        list.removeAll(path);
        list.prepend(path);
        while (list.size() > 10) list.removeLast();
        s_.setValue("files/recent", list);
    }

    // ── Page format ───────────────────────────────────────────────────────
    bool us_letter() const {
        return s_.value("page/us_letter", true).toBool();
    }
    void set_us_letter(bool v) { s_.setValue("page/us_letter", v); }

    void sync() { s_.sync(); }

private:
    AppConfig()
        : s_("IndieTools", "ScreenplayEditor") {}

    QSettings s_;
};

} // namespace screenplay::config
