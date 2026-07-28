#pragma once
// main_window.hpp
// The MainWindow class and the to_pdf_scene_num_mode helper, extracted
// verbatim from main.cpp (behaviour preserved by construction). The include
// prologue below is replicated from main.cpp so the header is self-contained
// when AUTOMOC compiles it on its own.


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

// Maps the canvas scene-number mode onto the PDF exporter's enum. The actual
// PDF/print rendering lives in screenplay::io::PdfExporter (io layer), not here.
static screenplay::io::PdfExporter::SceneNumberMode
to_pdf_scene_num_mode(ScreenplayCanvas::SceneNumMode m) {
    using S = ScreenplayCanvas::SceneNumMode;
    using D = screenplay::io::PdfExporter::SceneNumberMode;
    switch (m) {
    case S::Left:  return D::Left;
    case S::Right: return D::Right;
    case S::Both:  return D::Both;
    case S::None:  break;
    }
    return D::None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Window
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("Screenplay Editor " APP_VERSION);
        resize(1320, 920);

        // ── Own the whole window surface ─────────────────────────────────────
        // The OS title bar is a light strip the theme cannot reach: it stays
        // system-coloured whatever the palette says, cutting a foreign band
        // across the top of a Chamber/Dark window. Going frameless lets the
        // header BE the title bar, so the backdrop really is uniform edge to
        // edge — and gives us control buttons that match the rest of the app.
        //
        // Nothing native is lost: the move/resize/maximise gestures are handed
        // straight back to the window manager (see ui/window_frame.hpp), so
        // Aero Snap, snap layouts and drag-to-edge all behave as before.
        setWindowFlag(Qt::FramelessWindowHint, true);
        setAttribute(Qt::WA_Hover, true);
        setMouseTracking(true);
        // The header, canvas and status strip between them cover every pixel,
        // so a press near the window edge would land on a CHILD and the resize
        // hit-test below would never run. Reserving a grip-wide band as the
        // window's own contents margin is what makes the edges grabbable at
        // all. It is invisible — the band paints the same backdrop as
        // everything else. apply_frame_grip() drops it while maximised, where
        // there is nothing to resize.
        apply_frame_grip();

        try { canvas_ = new ScreenplayCanvas(this); }
        catch (const std::exception& e) {
            QMessageBox::critical(nullptr, "Error", e.what());
            std::exit(1);
        }

        // Restore persisted language before building menus
        {
            QSettings qs;
            using AL = screenplay::config::AppLanguage;
            int lv = qs.value("language", (int)AL::English).toInt();
            screenplay::config::LanguageConfig::set(
                lv == (int)AL::Portuguese ? AL::Portuguese : AL::English);
        }

        // The central column is [floating toolbar band | page | status strip].
        // The header goes to setMenuWidget() so it spans the full width above
        // everything, and the docks then sit BESIDE this column — which is
        // what puts the sidebar under the header and left of both the toolbar
        // and the page, as the reference layout does.
        central_ = new QWidget(this);
        central_col_ = new QVBoxLayout(central_);
        central_col_->setContentsMargins(0, 0, 0, 0);
        central_col_->setSpacing(0);
        setCentralWidget(central_);

        setup_toolbar();            // creates header_ + toolbar_
        central_col_->addWidget(canvas_, 1);
        setup_statusbar();          // creates status_ and appends it
        setup_scene_dock();
        setup_script_dock();
        setup_notes_dock();
        setup_shortcuts();

        auto& cfg = screenplay::config::AppConfig::instance();
        if (!cfg.load_geometry().isEmpty()) restoreGeometry(cfg.load_geometry());
        if (!cfg.load_state().isEmpty())    restoreState(cfg.load_state());

        panel_refresh_timer_.setSingleShot(true);
        panel_refresh_timer_.setInterval(250);
        connect(&panel_refresh_timer_, &QTimer::timeout, this, [this]{
            refresh_script_panel();
            refresh_notes();
        });

        connect(canvas_, &ScreenplayCanvas::script_changed,
                this,    &MainWindow::on_changed);
        connect(canvas_, &ScreenplayCanvas::autosave_requested,
                this,    &MainWindow::on_autosave);
        connect(canvas_, &ScreenplayCanvas::zoom_changed,
                this,    [this](float){ update_zoom(); });
        connect(canvas_, &ScreenplayCanvas::escape_pressed, this, [this]{
            // One state at a time, like the popup/highlight priority above:
            // leave Focus Mode first, then full screen on a second Escape.
            if (focus_mode_)       { set_focus_mode(false); return; }
            if (isFullScreen())    { on_fullscreen();        return; }
        });

        // Restore persisted zoom (saved on close, previously never re-applied)
        canvas_->set_zoom(cfg.zoom());

        // Seed the chrome readouts. Previously the status bar simply stayed
        // blank until the first edit fired on_changed(); with the strip now
        // carrying the document's identity line too, it has to be correct the
        // moment the window appears.
        update_status();
        update_save_indicator();
        update_zoom();
        // restoreGeometry() may have brought the window back maximised without
        // a state-change event reaching us, so seed the glyph explicitly.
        if (header_) header_->window_controls()->sync_state();
        apply_frame_grip();

        // Deferred post-init messages (shown after event loop starts)
        QTimer::singleShot(500, this, [this]{
            if (screenplay::ui::ScreenplayFont::using_fallback())
                show_status_message(
                    "Courier Prime not found \xe2\x80\x94 using Courier New", 8000);

            // Update spell check menu label based on runtime availability
            if (spell_status_act_) {
                spell_status_act_->setText(
                    QString("Spell Check (%1)")
                        .arg(canvas_->spell_available() ? "active" : "unavailable"));
            }
        });

        // First run: prompt the user to select the script language(s)
        {
            QSettings qs_fr;
            if (!qs_fr.contains("spell_languages")) {
                QTimer::singleShot(900, this, [this]{ on_script_language(); });
            }
        }

        // Crash recovery: offer to restore a leftover autosave
        QTimer::singleShot(700, this, [this]{ maybe_recover_autosave(); });

        // A scroll self-test, off unless SCREENPLAY_SCROLL_TEST names a file.
        // Reporting "it feels like 60 Hz" needs a number, and the only honest
        // number comes from the real window driving real repaints.
        if (const QByteArray script = qgetenv("SCREENPLAY_SCROLL_TEST");
                !script.isEmpty())
            QTimer::singleShot(1500, this, [this, script]{
                run_scroll_self_test(QString::fromLocal8Bit(script));
            });
    }

private slots:
    void on_changed() {
        refresh_scenes();          // cheap: already skips rebuild if titles didn't change
        panel_refresh_timer_.start();   // debounced: refresh_script_panel()
        update_title();
        update_status();
        update_save_indicator();
    }

    // Save state now reads in TWO places: quietly in the status strip, and as
    // the header's subtitle right under the document name — which is where
    // the reference layout puts it, and where the eye already is.
    void update_save_indicator() {
        using H = screenplay::ui::AppHeader;
        const bool dirty = canvas_->ctrl().state().dirty;

        QString text;
        H::Tone tone = H::Tone::Neutral;
        if (dirty) {
            text = "\xe2\x97\x8f Unsaved changes";
            tone = H::Tone::Warn;
        } else if (!last_save_time_.isEmpty()) {
            text = "Saved " + last_save_time_;
            tone = H::Tone::Good;
        }

        if (status_) status_->set_saved(text);
        if (header_) header_->set_subtitle(text.isEmpty() ? "Not saved yet" : text,
                                           tone);
    }

    void on_new() {
        if (dirty_confirm()) return;

        using Prof = screenplay::layout::LayoutProfile;

        // ── Layout profile selection ──────────────────────────────────────
        QDialog dlg(this);
        dlg.setWindowTitle(tr_ui("New Screenplay"));
        dlg.setModal(true);
        dlg.setMinimumWidth(400);

        auto* lay = new QVBoxLayout(&dlg);
        lay->setSpacing(12);
        lay->setContentsMargins(16, 16, 16, 16);

        auto* box = new QGroupBox("Layout");
        auto* box_lay = new QVBoxLayout(box);
        auto* rb_letter = new QRadioButton("US Industry Standard (Letter)");
        auto* rb_a4     = new QRadioButton("International (A4)");
        rb_letter->setToolTip("8.5 \xc3\x97 11 in \xe2\x80\x94 Final Draft / Fade In / "
                              "Movie Magic. Reference for 1 page \xe2\x89\x88 1 min.");
        rb_a4->setToolTip(tr_ui("210 \xc3\x97 297 mm \xe2\x80\x94 same layout adapted to A4 paper."));
        (canvas_->profile() == Prof::InternationalA4 ? rb_a4 : rb_letter)
            ->setChecked(true);
        box_lay->addWidget(rb_letter);
        box_lay->addWidget(rb_a4);
        lay->addWidget(box);

        auto* note = new QLabel(QString(
            "<small style='color:%1'>A4 fits more lines per page than Letter, "
            "so the same script shows a different page count. Use the Letter "
            "profile for the \xe2\x80\x9c" "1 page \xe2\x89\x88 1 minute" "\xe2\x80\x9d "
            "screen-time estimate.</small>").arg(MD3::hx(MD3::TextDim)));
        note->setWordWrap(true);
        lay->addWidget(note);

        auto* btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(btns);

        if (dlg.exec() != QDialog::Accepted) return;

        const Prof chosen = rb_a4->isChecked() ? Prof::InternationalA4
                                               : Prof::USLetter;

        canvas_->ctrl() = screenplay::editor::EditorController{};
        current_path_.clear();
        doc_custom_name_ = "Untitled";
        canvas_->set_profile(chosen);   // page size, margins, indents, pagination
        canvas_->request_relayout();
        emit canvas_->script_changed();
        update_status();
    }

    void on_open() {
        auto path = QFileDialog::getOpenFileName(
            this, "Open screenplay",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            "Todos (*.spl *.fountain *.fdx);;JSON (*.spl);;Fountain (*.fountain);;FDX (*.fdx)");
        if (!path.isEmpty()) open_path(path);
    }

    // Dedicated importers — file dialogs filtered to one format. Both delegate
    // to open_path(), which dispatches by extension (so FDX still reports any
    // down-graded elements) — no import logic is duplicated here.
    void on_import_fdx() {
        auto path = QFileDialog::getOpenFileName(
            this, "Import FDX",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            "Final Draft (*.fdx)");
        if (!path.isEmpty()) open_path(path);
    }

    void on_import_fountain() {
        auto path = QFileDialog::getOpenFileName(
            this, "Import Fountain",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            "Fountain (*.fountain)");
        if (!path.isEmpty()) open_path(path);
    }

    // Shared open path: used by Open…, Open Recent and drag & drop.
    /// Opens a script and scrolls it the way a reader would, reporting the
    /// frame rate actually achieved. Diagnostic only — see the call site.
    void run_scroll_self_test(const QString& path) {
        open_path(path);

        auto* clock  = new QElapsedTimer;
        auto* frames = new int(0);
        auto* ticker = new QTimer(this);
        clock->start();

        ticker->setTimerType(Qt::PreciseTimer);
        ticker->setInterval(50);           // a wheel notch every 50 ms
        connect(ticker, &QTimer::timeout, this, [=]{
            if (clock->elapsed() > 6000) {
                ticker->stop();
                const auto& scroll = canvas_->scroll_animator();
                std::fprintf(stderr,
                    "\n--- scroll self-test ---\n"
                    "monitor            : %.0f Hz\n"
                    "frame interval     : %d ms  (%.0f Hz paced)\n"
                    "smoothed paint cost: %.1f ms\n"
                    "frames per notch   : %d delivered / %d aimed for\n"
                    "notches driven     : %d over %lld ms\n",
                    screen() ? screen()->refreshRate() : 0.0,
                    scroll.frame_interval_ms(),
                    1000.0 / std::max(1, scroll.frame_interval_ms()),
                    (double)scroll.paint_cost_ms(),
                    scroll.frames_last_run(), scroll.frames_expected(),
                    *frames, clock->elapsed());
                std::fflush(stderr);
                delete clock; delete frames;
                QTimer::singleShot(200, qApp, &QCoreApplication::quit);
                return;
            }
            ++*frames;
            canvas_->scroll_by_notch(-1);
        });
        ticker->start();
    }

    void open_path(const QString& path) {
        if (dirty_confirm()) return;
        try {
            screenplay::Script s;
            screenplay::io::ImportReport fdx_report;
            QString ext = QFileInfo(path).suffix().toLower();
            if      (ext=="fountain") s=screenplay::io::FountainImporter::read(path.toStdString());
            else if (ext=="fdx")      s=screenplay::io::FDXImporter::read(path.toStdString(), &fdx_report);
            else                      s=screenplay::io::JsonDeserializer::read(path.toStdString());
            canvas_->ctrl().load_script(std::move(s));

            // Tell the writer if any unsupported Final Draft elements were kept
            // as Action rather than dropped, so nothing disappears silently.
            if (!fdx_report.downgraded_types.empty()) {
                std::vector<std::string> uniq;
                for (const auto& t : fdx_report.downgraded_types)
                    if (std::find(uniq.begin(), uniq.end(), t) == uniq.end())
                        uniq.push_back(t);
                QStringList names;
                for (const auto& t : uniq) names << QString::fromStdString(t);
                Toast::show_toast(this,
                    "Imported as Action (unsupported in this editor): "
                        + names.join(", "),
                    Toast::Kind::Info);
            }
            current_path_ = path;
            screenplay::config::AppConfig::instance().add_recent_file(path);
            canvas_->request_relayout();
            emit canvas_->script_changed();

            // TASK 3d: hint if title page has content but is not enabled
            const auto& tp = canvas_->ctrl().state().script.title_page;
            if (!tp.enabled && (!tp.title.empty() || !tp.authors.empty()))
                show_status_message(
                    "Title page configured but not shown \xe2\x80\x94"
                    " enable in Document > Title Page\xe2\x80\xa6",
                    8000);
        } catch (const std::exception& e) {
            show_error("Open error", e.what());
        }
    }

    void on_save()    { current_path_.isEmpty() ? on_save_as() : do_save(current_path_); }
    void on_save_as() {
        auto p = QFileDialog::getSaveFileName(
            this, "Save screenplay", {}, "JSON Script (*.spl)");
        if (!p.isEmpty()) do_save(p);
    }

    static QString autosave_path() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + "/autosave.spl";
    }

    void on_autosave() {
        if (!canvas_->ctrl().state().dirty) return;
        QString p = autosave_path();
        QDir().mkpath(QFileInfo(p).absolutePath());
        try { screenplay::io::JsonSerializer::write(
                  canvas_->ctrl().state().script, p.toStdString());
              show_status_message(tr_ui("Autosaved."), 2000);
        } catch (...) {}
    }

    // Offer to restore work left behind by a crash (autosave file present —
    // it is deleted on every clean exit and successful manual save).
    void maybe_recover_autosave() {
        const QString p = autosave_path();
        if (!QFile::exists(p)) return;
        const auto when = QFileInfo(p).lastModified().toString("dd/MM HH:mm");
        using CD = screenplay::ui::ConfirmDialog;
        if (CD::ask(this, {
                tr_ui("Recover unsaved work"),
                QString(tr_ui("Unsaved work from a previous session was found "
                              "(autosaved %1). Restore it?")).arg(when),
                tr_ui("Restore"), QString(), tr_ui("Discard"), false })
                == CD::Confirmed) {
            try {
                canvas_->ctrl().load_script(
                    screenplay::io::JsonDeserializer::read(p.toStdString()));
                doc_custom_name_ = "Recovered";
                canvas_->request_relayout();
                emit canvas_->script_changed();
                Toast::show_toast(this,
                    "Recovered \xe2\x80\x94 use Save As to keep this script",
                    Toast::Kind::Info);
            } catch (const std::exception& e) {
                show_error(tr_ui("Recover"), e.what());
            }
        }
        QFile::remove(p);
    }

    void on_export_fountain() {
        auto p = QFileDialog::getSaveFileName(
            this, "Export Fountain", {}, "Fountain (*.fountain)");
        if (p.isEmpty()) return;
        try { screenplay::io::FountainExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              Toast::show_toast(this, "Fountain exported \xe2\x80\x94 "
                                          + QFileInfo(p).fileName());
        } catch (const std::exception& e) { show_error("Error",e.what()); }
    }

    void on_export_rtf() {
        auto p = QFileDialog::getSaveFileName(
            this, tr_ui("Export RTF\xe2\x80\xa6"), {},
            tr_ui("Rich Text") + " (*.rtf)");
        if (p.isEmpty()) return;
        try { screenplay::io::RtfExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              Toast::show_toast(this, tr_ui("RTF exported") + " \xe2\x80\x94 "
                                          + QFileInfo(p).fileName());
        } catch (const std::exception& e) { show_error("Error", e.what()); }
    }

    void on_export_text() {
        auto p = QFileDialog::getSaveFileName(
            this, tr_ui("Export Text\xe2\x80\xa6"), {},
            tr_ui("Plain text") + " (*.txt)");
        if (p.isEmpty()) return;
        try { screenplay::io::TextExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              Toast::show_toast(this, tr_ui("Text exported") + " \xe2\x80\x94 "
                                          + QFileInfo(p).fileName());
        } catch (const std::exception& e) { show_error("Error", e.what()); }
    }

    void on_export_fdx() {
        auto p = QFileDialog::getSaveFileName(
            this, "Export FDX", {}, "Final Draft (*.fdx)");
        if (p.isEmpty()) return;
        try { screenplay::io::FDXExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              Toast::show_toast(this, "FDX exported \xe2\x80\x94 "
                                          + QFileInfo(p).fileName());
        } catch (const std::exception& e) { show_error("Error",e.what()); }
    }

    void on_zoom_in()    { canvas_->zoom_in();    update_zoom(); }
    void on_zoom_out()   { canvas_->zoom_out();   update_zoom(); }
    void on_zoom_reset() { canvas_->zoom_reset(); update_zoom(); }

    // Builds the PDF exporter options from the current canvas state. Shared by
    // Export PDF and Print so both produce byte-identical output.
    screenplay::io::PdfExporter::Options pdf_options() const {
        screenplay::io::PdfExporter::Options o;
        o.pt_size             = canvas_->pt_size();
        o.font_family         = screenplay::ui::ScreenplayFont::family();
        o.apply_quality       = [](QFont& f) { screenplay::ui::ScreenplayFont::apply_render_quality(f); };
        o.scene_num_mode      = to_pdf_scene_num_mode(canvas_->scene_num_mode());
        o.bold_scene_headings = canvas_->bold_scene_headings();
        return o;
    }

    void on_export_pdf() {
        auto p = QFileDialog::getSaveFileName(
            this, "Export PDF", {}, "PDF (*.pdf)");
        if (p.isEmpty()) return;
        try {
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(p);
            printer.setPageSize(screenplay::io::PdfExporter::page_size_for_profile(
                canvas_->profile()));
            printer.setPageMargins(QMarginsF(0, 0, 0, 0));
            screenplay::io::PdfExporter::render(
                printer, canvas_->ctrl().state().script, canvas_->pages(),
                canvas_->page_geometry(), pdf_options());
            Toast::show_toast(this, "PDF exported \xe2\x80\x94 "
                                        + QFileInfo(p).fileName());
        } catch (const std::exception& e) {
            show_error("Export error", e.what());
        }
    }

    void on_print() {
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageSize(screenplay::io::PdfExporter::page_size_for_profile(
            canvas_->profile()));
        printer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPrintDialog dlg(&printer, this);
        if (dlg.exec() != QDialog::Accepted) return;
        try {
            screenplay::io::PdfExporter::render(
                printer, canvas_->ctrl().state().script, canvas_->pages(),
                canvas_->page_geometry(), pdf_options());
        } catch (const std::exception& e) {
            show_error("Print error", e.what());
        }
    }

    void on_find_replace() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(tr_ui("Find and Replace"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        auto* form   = new QFormLayout(dlg);
        auto* find_e = new QLineEdit(dlg);
        auto* repl_e = new QLineEdit(dlg);
        form->addRow(tr_ui("Find:"),    find_e);
        form->addRow(tr_ui("Replace:"), repl_e);
        auto* btns = new QHBoxLayout;
        auto* btn_repl    = new QPushButton(tr_ui("Replace"),     dlg);
        auto* btn_repl_all= new QPushButton(tr_ui("Replace All"), dlg);
        auto* btn_close   = new QPushButton(tr_ui("Close"),       dlg);
        btns->addWidget(btn_repl); btns->addWidget(btn_repl_all); btns->addWidget(btn_close);
        form->addRow(btns);
        connect(btn_close, &QPushButton::clicked, dlg, &QDialog::close);
        connect(btn_repl_all, &QPushButton::clicked, this, [this, find_e, repl_e, dlg]{
            int n = canvas_->ctrl().replace_text(
                find_e->text().toStdString(), repl_e->text().toStdString(), true);
            if (n > 0) {
                canvas_->request_relayout();
                emit canvas_->script_changed();
                Toast::show_toast(this,
                    QString("Replaced %1 occurrence%2")
                        .arg(n).arg(n == 1 ? "" : "s"));
            } else {
                Toast::show_toast(this, "No matches found", Toast::Kind::Info);
            }
            dlg->close();
        });
        connect(btn_repl, &QPushButton::clicked, this, [this, find_e, repl_e]{
            int n = canvas_->ctrl().replace_text(
                find_e->text().toStdString(), repl_e->text().toStdString(), false);
            if (n > 0) { canvas_->request_relayout(); emit canvas_->script_changed(); }
            else       { Toast::show_toast(this, "No matches found", Toast::Kind::Info); }
        });
        dlg->show();
    }

    void on_goto_page() {
        bool ok = false;
        int page = QInputDialog::getInt(this, "Go to page", "Page:",
                                        1, 1, (int)canvas_->pages().size(), 1, &ok);
        if (ok) canvas_->scroll_to_page(page);
    }

    // Places the caret at the start of block `bi` and scrolls to it — used by
    // every programmatic "jump" (scene list, Go to Scene, database panel).
    // Explicitly dismisses SmartType suggestions: set_cursor_pos() populates
    // them for the new position (correct for an actual in-editor click), but
    // a navigation jump isn't the writer typing, so nothing should pop up
    // while the scroll-to animation runs (each tick repaints the popup).
    void navigate_to_block(size_t bi) {
        canvas_->ctrl().set_cursor_pos({ bi, 0 });
        canvas_->ctrl().dismiss_suggestions();
        canvas_->scroll_to_block(bi);
        canvas_->update();
        emit canvas_->script_changed();
    }

    void on_goto_scene() {
        bool ok = false;
        int n = QInputDialog::getInt(this, "Go to scene", "Scene number:",
                                     1, 1, 9999, 1, &ok);
        if (!ok) return;
        int count = 0;
        const auto& blocks = canvas_->ctrl().state().script.blocks;
        for (size_t bi = 0; bi < blocks.size(); ++bi) {
            if (blocks[bi].type == screenplay::BlockType::SceneHeading) {
                ++count;
                if (count == n) {
                    navigate_to_block(bi);
                    return;
                }
            }
        }
    }
    void on_fullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }

    // Focus mode: hide every panel/bar, leaving only the page. Dock
    // visibility is remembered and restored when leaving the mode.
    void set_focus_mode(bool on) {
        if (on == focus_mode_) return;
        focus_mode_ = on;
        if (on) {
            focus_prev_scenes_ = scene_dock_->isVisible();
            focus_prev_script_ = script_dock_->isVisible();
            scene_dock_->hide();
            script_dock_->hide();
            if (status_)   status_->hide();
            if (toolbar_)  toolbar_->hide();
            if (menuWidget()) menuWidget()->hide();
        } else {
            if (menuWidget()) menuWidget()->show();
            if (toolbar_)  toolbar_->show();
            if (status_)   status_->show();
            scene_dock_->setVisible(focus_prev_scenes_);
            script_dock_->setVisible(focus_prev_script_);
        }
        if (act_focus_mode_) act_focus_mode_->setChecked(on);
        canvas_->setFocus();
    }
    /// Switches the interface language — and the dictionary with it.
    ///
    /// These used to be separate: the menu changed only the UI strings, while
    /// the spell checker kept whatever "Script Language…" had last stored. A
    /// writer who set the program to English then saw every English word
    /// underlined, because a Portuguese dictionary was still the only one
    /// running. Choosing a language now means choosing it for the whole app;
    /// "Script Language…" remains for a script that mixes languages or is
    /// written in one the interface is not.
    void apply_language(screenplay::config::AppLanguage language) {
        screenplay::config::LanguageConfig::set(language);
        QSettings().setValue("language", (int)language);
        canvas_->ctrl().reseed_autocomplete();
        rebuild_menus();

        const QString tag =
            (language == screenplay::config::AppLanguage::Portuguese) ? "pt-BR"
                                                                      : "en-US";
        // Dictionaries come from Windows, and the one for this language may
        // simply not be installed. Switching to it blindly turned spell check
        // OFF — silently, which is worse than checking in the wrong language.
        canvas_->reinit_spell({ tag.toStdString() });
        if (canvas_->spell_available()) {
            QSettings().setValue("spell_languages", QStringList{ tag });
            show_status_message(
                QString(tr_ui("Language changed \xe2\x80\x94 spell check now uses %1"))
                    .arg(tag), 6000);
            return;
        }

        // Put back whatever did work, then offer the one action that fixes it.
        // Dictionaries are a Windows feature, not something the app can ship or
        // install — but it can take the writer straight to the page that adds
        // one, which is more use than a message telling them to go looking.
        const QStringList previous =
            QSettings().value("spell_languages", QStringList{ "pt-BR" }).toStringList();
        std::vector<std::string> tags;
        for (const auto& l : previous) tags.push_back(l.toStdString());
        canvas_->reinit_spell(tags);

        const auto choice = screenplay::ui::ConfirmDialog::ask(this, {
            tr_ui("Spell check dictionary missing"),
            // The quotes are split literals on purpose: "\x9cB" would be read
            // as ONE hex escape and overflow. See the note in CLAUDE.md.
            QString(tr_ui("Windows has no %1 dictionary installed, so spell "
                          "check would go silent. Add the language in Windows "
                          "settings \xe2\x80\x94 tick " "\xe2\x80\x9c" "Basic typing"
                          "\xe2\x80\x9d" " for it \xe2\x80\x94 then choose it here "
                          "again.\n\nStill checking with: %2."))
                .arg(tag, previous.join(", ")),
            tr_ui("Open Windows settings"), QString(), tr_ui("Later"), false });
        if (choice == screenplay::ui::ConfirmDialog::Confirmed)
            QDesktopServices::openUrl(QUrl("ms-settings:regionlanguage"));
    }

    void on_script_language() {
        QSettings qs;
        QStringList curr = qs.value("spell_languages", QStringList{"en-US"}).toStringList();
        ScriptLanguageDialog dlg(this, curr);
        if (dlg.exec() != QDialog::Accepted) return;
        QStringList langs = dlg.selected_langs();
        if (langs.isEmpty()) {
            show_error(tr_ui("Language"),
                "Please select at least one language for spell checking.");
            return;
        }
        qs.setValue("spell_languages", langs);
        std::vector<std::string> vtags;
        for (const auto& l : langs) vtags.push_back(l.toStdString());
        canvas_->reinit_spell(vtags);
        // Update spell-check availability label
        QTimer::singleShot(300, this, [this]{
            if (spell_status_act_)
                spell_status_act_->setText(
                    QString("Spell Check (%1)")
                        .arg(canvas_->spell_available() ? "active" : "unavailable"));
        });
        Toast::show_toast(this, "Spell check language updated",
                          Toast::Kind::Info);
    }
    void on_shortcuts_dialog() {
        // The command half of this table is GENERATED from the live QActions
        // rather than hand-maintained. A hardcoded list silently goes stale the
        // moment a shortcut is added or changed — which is exactly what had
        // happened: alignment, notes, formatting and zoom were all missing.
        // Walking the real menus means the dialog cannot lie.
        using CD = screenplay::ui::AppDialog;
        auto* dlg = new CD(this, tr_ui("Keyboard Shortcuts"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(660, 560);

        auto* filter = new QLineEdit(dlg);
        filter->setPlaceholderText(tr_ui("Filter shortcuts\xe2\x80\xa6"));
        filter->setClearButtonEnabled(true);
        dlg->content()->addWidget(filter);

        auto* table = new QTableWidget(dlg);
        table->setColumnCount(3);
        table->setHorizontalHeaderLabels({ tr_ui("Shortcut"), tr_ui("Action"),
                                           tr_ui("Context") });
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setShowGrid(false);
        dlg->content()->addWidget(table, 1);

        struct Row { QString key, action, ctx; };
        std::vector<Row> rows;

        // 1. Every menu command that carries a shortcut, with its menu as the
        //    context — so "where do I find this?" is answered too.
        std::function<void(QMenu*, const QString&)> walk =
            [&](QMenu* menu, const QString& path) {
                for (QAction* act : menu->actions()) {
                    if (act->isSeparator()) continue;
                    const QString label =
                        act->text().remove('&');
                    if (act->menu()) {
                        walk(act->menu(), path.isEmpty()
                                              ? label : path + " \xe2\x80\xba " + label);
                        continue;
                    }
                    if (act->shortcut().isEmpty()) continue;
                    rows.push_back({ act->shortcut().toString(QKeySequence::NativeText),
                                     label, path });
                }
            };
        if (auto* mb = findChild<QMenuBar*>()) {
            for (QAction* top : mb->actions())
                if (top->menu()) walk(top->menu(), top->text().remove('&'));
        }

        // 2. Keys the editor handles itself. These are NOT QActions — they are
        //    interpreted in ScreenplayCanvas::keyPressEvent — so they have to
        //    be listed by hand. Keep this in step with that handler.
        const QString ed  = tr_ui("Editor");
        const QString pop = tr_ui("SmartType");
        const struct { const char* key; const char* action; const QString& ctx; } manual[] = {
            {"Enter",            "New block",                        ed},
            {"Tab",              "Accept suggestion / next type",    ed},
            {"Shift+Tab",        "Previous block type",              ed},
            {"Backspace",        "Delete char / merge block",        ed},
            {"Delete",           "Delete forward / merge block",     ed},
            {"Ctrl+Backspace",   "Delete previous word",             ed},
            {"Ctrl+Delete",      "Delete next word",                 ed},
            {"Home / End",       "Line start / end",                 ed},
            {"Ctrl+Home / End",  "Document start / end",             ed},
            {"Up / Down",        "Previous / next visual line",      ed},
            {"Ctrl+Up / Down",   "Previous / next block",            ed},
            {"Left / Right",     "Previous / next character",        ed},
            {"Ctrl+Left / Right","Previous / next word",             ed},
            {"Shift+arrows",     "Extend selection",                 ed},
            {"Shift+Home / End", "Select to line start / end",       ed},
            {"Ctrl+=",           "Zoom in (alternate)",              ed},
            {"Ctrl+scroll",      "Zoom",                             ed},
            {"Double-click",     "Select word",                      ed},
            {"Triple-click",     "Select block",                     ed},
            {"Tab / Enter",      "Accept suggestion",                pop},
            {"Up / Down",        "Previous / next suggestion",       pop},
            // Split literal: \x93 followed by '6' would be read as \x936.
            {"Ctrl+1\xe2\x80\x93" "6", "Accept suggestion N",        pop},
            {"Escape",           "Dismiss",                          pop},
        };
        for (const auto& m : manual)
            rows.push_back({ m.key, tr_ui(m.action), m.ctx });

        table->setRowCount((int)rows.size());
        for (int i = 0; i < (int)rows.size(); ++i) {
            table->setItem(i, 0, new QTableWidgetItem(rows[(size_t)i].key));
            table->setItem(i, 1, new QTableWidgetItem(rows[(size_t)i].action));
            table->setItem(i, 2, new QTableWidgetItem(rows[(size_t)i].ctx));
        }
        table->resizeColumnsToContents();

        connect(filter, &QLineEdit::textChanged, table, [table](const QString& text){
            for (int r = 0; r < table->rowCount(); ++r) {
                bool match = text.isEmpty();
                for (int c = 0; c < 3 && !match; ++c) {
                    auto* item = table->item(r, c);
                    if (item && item->text().contains(text, Qt::CaseInsensitive))
                        match = true;
                }
                table->setRowHidden(r, !match);
            }
        });

        auto* close = dlg->add_button(tr_ui("Close"),
                                      screenplay::ui::SoftButton::Variant::Primary);
        connect(close, &QToolButton::clicked, dlg, &QDialog::accept);
        dlg->show();
    }

private:
    // Tear down the current header widget and rebuild it with the active language.
    // Called after LanguageConfig::set() so all menu strings are re-evaluated.
    void rebuild_menus() {
        // setup_toolbar() rebuilds BOTH chrome pieces from scratch, so both
        // old ones have to go: the header host (via setMenuWidget) and the
        // toolbar card, which lives in the central column instead.
        QWidget* old_header  = menuWidget();
        QWidget* old_toolbar = toolbar_;
        if (old_toolbar) central_col_->removeWidget(old_toolbar);

        setup_toolbar();                    // creates new header_ + toolbar_

        if (old_header)  old_header->deleteLater();
        if (old_toolbar) old_toolbar->deleteLater();
        if (focus_mode_) {
            if (menuWidget()) menuWidget()->hide();
            if (toolbar_)     toolbar_->hide();
        }

        // Sync dock-visibility checkmarks to actual dock state
        if (act_view_scenes_   && scene_dock_)
            act_view_scenes_->setChecked(scene_dock_->isVisible());
        if (act_view_script_ && script_dock_)
            act_view_script_->setChecked(script_dock_->isVisible());

        // Restore dynamic labels that setup_toolbar() resets to defaults
        update_doc_name_display();
        update_capa_badge();
        update_status();
        refresh_scenes();

        // Restore spell-check label (post-init timer already fired)
        if (spell_status_act_)
            spell_status_act_->setText(
                QString("Spell Check (%1)")
                    .arg(canvas_->spell_available() ? "active" : "unavailable"));
    }

    void setup_toolbar();   // defined in ui/main_menu.cpp

    // Walks a menu tree and adds every shortcut-bearing action to the window,
    // so shortcuts survive the menu bar being hidden behind the ☰ button.
    // Actions already registered are skipped (QWidget::addAction is a no-op
    // for duplicates, but checking keeps the intent explicit).
    void register_shortcuts_on_window(QMenu* menu) {
        for (QAction* a : menu->actions()) {
            if (a->menu()) { register_shortcuts_on_window(a->menu()); continue; }
            if (a->shortcut().isEmpty()) continue;
            a->setShortcutContext(Qt::WindowShortcut);
            if (!actions().contains(a)) addAction(a);
        }
    }

    void setup_statusbar() {
        status_ = new screenplay::ui::StatusStrip(this);

        // The strip lives in the window's status-bar slot, which is the one
        // area that spans the FULL width beneath the docks. It used to be the
        // last row of the central column, and a dock — being a sibling of that
        // column — ran the whole height beside it: a panel's own bottom edge
        // ended up level with the footer, the two reading as one broken row.
        //
        // QStatusBar is only the container. Its sunken frame, per-item borders
        // and size grip are all switched off, so nothing of Qt's own status bar
        // language shows through.
        auto* slot = new QStatusBar(this);
        slot->setSizeGripEnabled(false);
        slot->setContentsMargins(0, 0, 0, 0);
        slot->setStyleSheet("QStatusBar { border:none; background:transparent; }"
                            "QStatusBar::item { border:none; }");
        slot->addPermanentWidget(status_, 1);
        setStatusBar(slot);

        connect(status_->zoom_in(),  &QToolButton::clicked,
                this, &MainWindow::on_zoom_in);
        connect(status_->zoom_out(), &QToolButton::clicked,
                this, &MainWindow::on_zoom_out);

        status_->set_words(tr_ui("Words") + " 0");
        status_->set_scenes(tr_ui("Scenes") + " 0");
        status_->set_runtime("~0 min");
        status_->set_page(tr_ui("Page") + " 0/0");
        status_->set_zoom("100%");

        // Transient messages used to rely on QStatusBar::showMessage's own
        // timeout; the strip has no such machinery, so one shared timer
        // clears them instead.
        status_msg_timer_.setSingleShot(true);
        connect(&status_msg_timer_, &QTimer::timeout, this, [this]{
            if (status_) status_->set_message(QString());
        });
    }

    // Drop-in replacement for the old statusBar()->showMessage(text, ms).
    void show_status_message(const QString& text, int ms = 0) {
        if (!status_) return;
        status_->set_message(text);
        status_msg_timer_.stop();
        if (ms > 0) status_msg_timer_.start(ms);
    }

    void restyle_statusbar() {
        if (status_) status_->restyle();
    }

    void setup_scene_dock() {
        using Sp = screenplay::ui::Spacing;
        auto* dock_container = new QWidget;
        dock_container->setObjectName("dockPanel");   // picks up the panel border
        // Real padding on all four sides — the panel's content must not touch
        // its edges.
        auto* dc_lay = new QVBoxLayout(dock_container);
        dc_lay->setContentsMargins(Sp::M, Sp::S, Sp::M, Sp::M);
        dc_lay->setSpacing(Sp::S);

        // Quick scene filter (Final Draft navigator-style)
        scene_filter_edit_ = new QLineEdit;
        scene_filter_edit_->setPlaceholderText(tr_ui("Filter scenes\xe2\x80\xa6"));
        scene_filter_edit_->setClearButtonEnabled(true);
        dc_lay->addWidget(scene_filter_edit_);
        connect(scene_filter_edit_, &QLineEdit::textChanged,
                this, [this](const QString&){ apply_scene_filter(); });

        scene_list_ = new QListWidget;
        scene_list_->setContextMenuPolicy(Qt::CustomContextMenu);
        restyle_scene_dock();
        connect(scene_list_, &QWidget::customContextMenuRequested,
                this, &MainWindow::scene_context_menu);
        dc_lay->addWidget(scene_list_);

        dock_container->setMinimumWidth(screenplay::ui::SidebarWidth::Default);
        scene_dock_ = screenplay::ui::make_panel_dock(
            this, tr_ui("SCENE LIST"), dock_container, &scene_frame_);
        scene_dock_->setMinimumWidth(screenplay::ui::SidebarWidth::Minimum);
        addDockWidget(Qt::LeftDockWidgetArea, scene_dock_);
        scene_dock_->hide();
        connect(scene_dock_, &QDockWidget::visibilityChanged, this, [this](bool v){
            if (act_view_scenes_) act_view_scenes_->setChecked(v);
        });

        connect(scene_list_, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item) {
            int row = scene_list_->row(item);
            int count = -1;
            const auto& blocks = canvas_->ctrl().state().script.blocks;
            for (size_t bi = 0; bi < blocks.size(); ++bi) {
                if (blocks[bi].type == screenplay::BlockType::SceneHeading) {
                    ++count;
                    if (count == row) {
                        canvas_->ctrl().set_cursor_pos({ bi, 0 });
                        canvas_->scroll_to_block(bi);
                        canvas_->update();
                        emit canvas_->script_changed();
                        return;
                    }
                }
            }
        });
    }

    // Flip dark ↔ light and restyle every themed surface.
    // Cycles Light → Dark → Chamber. ThemeManager owns the palette and
    // persistence; everything here just re-styles what it already built.
    void on_toggle_theme() {
        using TM = screenplay::ui::ThemeManager;
        TM::instance().set_theme(TM::instance().next_theme());
        apply_theme_everywhere();
        Toast::show_toast(this, tr_ui(TM::theme_name(TM::instance().theme())),
                          Toast::Kind::Info);
    }

    // One place that re-styles every surface after a theme change, so a new
    // panel only has to be added here once.
    void apply_theme_everywhere() {
        rebuild_menus();            // header + action icons re-created
        restyle_statusbar();
        restyle_scene_dock();
        if (script_panel_) script_panel_->restyle();
        for (auto* frame : { scene_frame_, script_frame_, notes_frame_ })
            if (frame) frame->restyle();
        canvas_->apply_theme_refresh();
        update_save_indicator();
    }

    // Shared by the Stats and Database panels.
    void on_character_highlight(const QString& name) {
        canvas_->set_character_highlight(name.toStdString());
        if (!canvas_->highlighted_character().empty())
            show_status_message(
                QString(tr_ui("Highlighting %1 \xe2\x80\x94 click again or press Esc to clear"))
                    .arg(name), 5000);
        else
            show_status_message(QString());
    }

    // ── One dock for "what is in this script" ─────────────────────────────
    // Statistics and Script Database used to be two docks answering the same
    // question in two visual languages. One panel, one dock, one toggle.
    void setup_script_dock() {
        script_panel_ = new ScriptPanel;
        connect(script_panel_, &ScriptPanel::character_clicked,
                this, &MainWindow::on_character_highlight);
        connect(script_panel_, &ScriptPanel::scene_activated, this,
                [this](int block_idx) {
            const size_t at = (size_t)block_idx;
            canvas_->ctrl().set_cursor_pos({ at, 0 });
            canvas_->scroll_to_block(at);
            canvas_->setFocus();
            canvas_->update();
        });

        script_dock_ = screenplay::ui::make_panel_dock(
            this, tr_ui("SCRIPT BREAKDOWN"), script_panel_, &script_frame_);
        script_dock_->setMinimumWidth(screenplay::ui::SidebarWidth::Minimum);
        addDockWidget(Qt::RightDockWidgetArea, script_dock_);
        script_dock_->hide();
        connect(script_dock_, &QDockWidget::visibilityChanged, this, [this](bool v){
            if (act_view_script_) act_view_script_->setChecked(v);
            if (v) refresh_script_panel();
        });
    }

    void setup_shortcuts() {
        auto sc = [&](const char* seq, std::function<void()> fn){
            auto* s = new QShortcut(QKeySequence(seq), this);
            connect(s, &QShortcut::activated, this, fn);
        };
        // Ctrl+= is an alternate zoom-in (menu uses Ctrl++, a different key sequence)
        sc("Ctrl+=", [this]{ on_zoom_in(); });
        // Ctrl+P now belongs to the File > Print… menu action.
        // All other shortcuts (Ctrl+S, F11, Ctrl+N, Ctrl+O, Ctrl+H, etc.)
        // are handled exclusively by their menu action setShortcut() calls.
        // Duplicate QShortcuts were removed to eliminate Qt ambiguous-shortcut
        // conflicts that prevented either handler from firing.
    }

    // Rebuild the Open Recent submenu from AppConfig (files pruned if gone).
    void populate_recent_menu() {
        if (!recent_menu_) return;
        recent_menu_->clear();
        int shown = 0;
        const QStringList files =
            screenplay::config::AppConfig::instance().recent_files();
        for (const QString& f : files) {
            if (!QFile::exists(f)) continue;
            QFileInfo fi(f);
            auto* act = recent_menu_->addAction(
                QString("%1   \xe2\x80\x94   %2")
                    .arg(fi.fileName(), fi.absolutePath()));
            connect(act, &QAction::triggered, this, [this, f]{ open_path(f); });
            ++shown;
        }
        recent_menu_->setEnabled(shown > 0);
    }

    void do_save(const QString& path) {
        try {
            screenplay::io::JsonSerializer::write(
                canvas_->ctrl().state().script, path.toStdString());
            current_path_ = path;
            screenplay::config::AppConfig::instance().add_recent_file(path);
            canvas_->ctrl().mark_clean();
            last_save_time_ = QTime::currentTime().toString("HH:mm");
            QFile::remove(autosave_path());   // work is safe on disk now
            Toast::show_toast(this, "Saved " + QFileInfo(path).fileName());
            update_title();
            update_save_indicator();
        } catch (const std::exception& e) {
            show_error("Save error", e.what());
        }
    }

    // Returns true if the caller should ABORT the pending action (new/open/
    // close). False means it's safe to proceed — either nothing was dirty,
    // or the user explicitly chose to save or discard.
    // Every error the user sees goes through here, so failures wear the same
    // chrome as everything else instead of a platform message box.
    void show_error(const QString& title, const QString& body) {
        screenplay::ui::ConfirmDialog::ask(this, {
            title, body, tr_ui("OK"), QString(), QString(), false });
    }

    // Returns true when the caller should ABORT whatever it was about to do.
    bool dirty_confirm() {
        if (!canvas_->ctrl().state().dirty) return false;
        using CD = screenplay::ui::ConfirmDialog;
        const auto reply = CD::ask(this, {
            tr_ui("Unsaved changes"),
            tr_ui("This screenplay has changes that have not been saved yet."),
            tr_ui("Save"),
            tr_ui("Discard"),
            tr_ui("Cancel"),
            true,          // discarding loses work — mark it destructive
        });
        if (reply == CD::Cancelled) return true;
        if (reply == CD::Confirmed) {
            on_save();
            // Save As can itself be cancelled, or writing can fail — in
            // either case the document is still dirty, so abort too.
            if (canvas_->ctrl().state().dirty) return true;
        }
        return false;   // Discard, or Save succeeded
    }

    // Inclusive block range [heading, last] of the row-th scene in the list.
    struct SceneRange { size_t heading; size_t last; };
    bool scene_range_for_row(int row, SceneRange& out) const {
        const auto& blocks = canvas_->ctrl().state().script.blocks;
        int  n = -1;
        bool found = false;
        for (size_t bi = 0; bi < blocks.size(); ++bi) {
            if (blocks[bi].type != screenplay::BlockType::SceneHeading) continue;
            if (found) { out.last = bi - 1; return true; }
            if (++n == row) { out.heading = bi; found = true; }
        }
        if (found) { out.last = blocks.size() - 1; return true; }
        return false;
    }

    void after_scene_op() {
        canvas_->request_relayout();
        emit canvas_->script_changed();
        canvas_->scroll_to_block(canvas_->ctrl().state().cursor.block_idx);
        canvas_->update();
    }

    // Finds the scene (and its 0-based row in the Scenes list) that owns
    // block_idx — the last Scene Heading at or before it. Backs the
    // Document-menu scene actions, which act on "the scene the caret is
    // currently in" (scene_range_for_row acts on a right-clicked list row —
    // same underlying scene, reached a different way).
    bool scene_range_for_block(size_t block_idx, int& row_out, SceneRange& out) const {
        const auto& blocks = canvas_->ctrl().state().script.blocks;
        int row = -1, found_row = -1;
        size_t found_heading = 0;
        for (size_t bi = 0; bi < blocks.size(); ++bi) {
            if (blocks[bi].type != screenplay::BlockType::SceneHeading) continue;
            ++row;
            if (bi <= block_idx) { found_heading = bi; found_row = row; }
            else break;   // scene headings are strictly increasing in bi
        }
        if (found_row < 0) return false;
        row_out     = found_row;
        out.heading = found_heading;
        out.last    = blocks.size() - 1;
        for (size_t bi = found_heading + 1; bi < blocks.size(); ++bi)
            if (blocks[bi].type == screenplay::BlockType::SceneHeading) { out.last = bi - 1; break; }
        return true;
    }

    // ── Document-menu equivalents of the scene-list context menu ─────────
    // Same underlying operations (EditorController::insert_block_at /
    // duplicate_block_range / delete_block_range / rotate_blocks), just
    // targeting the scene the caret is currently in instead of a
    // right-clicked row — so these are reachable without the Scenes panel.
    void on_scene_insert_after_current() {
        auto& ctrl = canvas_->ctrl();
        int row; SceneRange range;
        size_t at = ctrl.state().script.blocks.size();
        if (scene_range_for_block(ctrl.state().cursor.block_idx, row, range))
            at = range.last + 1;
        ctrl.insert_block_at(at, screenplay::BlockType::SceneHeading);
        after_scene_op();
    }

    void on_scene_duplicate_current() {
        auto& ctrl = canvas_->ctrl();
        int row; SceneRange range;
        if (!scene_range_for_block(ctrl.state().cursor.block_idx, row, range)) return;
        ctrl.duplicate_block_range(range.heading, range.last);
        after_scene_op();
    }

    void on_scene_delete_current() {
        auto& ctrl = canvas_->ctrl();
        int row; SceneRange range;
        if (!scene_range_for_block(ctrl.state().cursor.block_idx, row, range)) return;
        const QString title = QString::fromStdString(
            ctrl.state().script.blocks[range.heading].text);
        using CD = screenplay::ui::ConfirmDialog;
        if (CD::ask(this, { tr_ui("Delete scene"),
                            QString(tr_ui("Delete \"%1\" and all its content? "
                                          "This can be undone with Ctrl+Z.")).arg(title),
                            tr_ui("Delete"), QString(), tr_ui("Cancel"), false })
                != CD::Confirmed) return;
        ctrl.delete_block_range(range.heading, range.last);
        after_scene_op();
    }

    void on_scene_move_up() {
        auto& ctrl = canvas_->ctrl();
        int row; SceneRange range;
        if (!scene_range_for_block(ctrl.state().cursor.block_idx, row, range)) return;
        if (row <= 0) return;
        SceneRange prev{};
        if (!scene_range_for_row(row - 1, prev)) return;
        ctrl.rotate_blocks(prev.heading, range.heading, range.last + 1, prev.heading);
        after_scene_op();
    }

    void on_scene_move_down() {
        auto& ctrl = canvas_->ctrl();
        int row; SceneRange range;
        if (!scene_range_for_block(ctrl.state().cursor.block_idx, row, range)) return;
        SceneRange next{};
        if (!scene_range_for_row(row + 1, next)) return;
        ctrl.rotate_blocks(range.heading, next.heading, next.last + 1,
                           range.heading + (next.last - next.heading + 1));
        after_scene_op();
    }

    void scene_context_menu(const QPoint& pos) {
        auto* item = scene_list_->itemAt(pos);
        const int row = item ? scene_list_->row(item) : -1;
        const int scene_count = scene_list_->count();

        SceneRange range{};
        const bool has_scene = (row >= 0) && scene_range_for_row(row, range);

        QMenu menu(this);
        auto* act_new  = menu.addAction(tr_ui("New Scene After"));
        menu.addSeparator();
        auto* act_dup  = menu.addAction(tr_ui("Duplicate Scene"));
        auto* act_del  = menu.addAction(tr_ui("Delete Scene\xe2\x80\xa6"));
        menu.addSeparator();
        auto* act_up   = menu.addAction(tr_ui("Move Scene Up"));
        auto* act_down = menu.addAction(tr_ui("Move Scene Down"));

        act_dup ->setEnabled(has_scene);
        act_del ->setEnabled(has_scene);
        act_up  ->setEnabled(has_scene && row > 0);
        act_down->setEnabled(has_scene && row + 1 < scene_count);

        auto* chosen = menu.exec(scene_list_->viewport()->mapToGlobal(pos));
        if (!chosen) return;
        auto& ctrl = canvas_->ctrl();

        if (chosen == act_new) {
            const size_t at = has_scene
                ? range.last + 1
                : ctrl.state().script.blocks.size();
            ctrl.insert_block_at(at, screenplay::BlockType::SceneHeading);
            after_scene_op();
        } else if (chosen == act_dup) {
            ctrl.duplicate_block_range(range.heading, range.last);
            after_scene_op();
        } else if (chosen == act_del) {
            const QString title = item ? item->text() : QString();
            using CD = screenplay::ui::ConfirmDialog;
            if (CD::ask(this, { tr_ui("Delete scene"),
                    QString(tr_ui("Delete \"%1\" and all its content? "
                                  "This can be undone with Ctrl+Z.")).arg(title),
                    tr_ui("Delete"), QString(), tr_ui("Cancel"), false })
                    != CD::Confirmed) return;
            ctrl.delete_block_range(range.heading, range.last);
            after_scene_op();
        } else if (chosen == act_up) {
            SceneRange prev{};
            if (!scene_range_for_row(row - 1, prev)) return;
            ctrl.rotate_blocks(prev.heading, range.heading, range.last + 1,
                               prev.heading);
            after_scene_op();
        } else if (chosen == act_down) {
            SceneRange next{};
            if (!scene_range_for_row(row + 1, next)) return;
            // Moved scene ends up after the next scene's block span
            ctrl.rotate_blocks(range.heading, next.heading, next.last + 1,
                               range.heading + (next.last - next.heading + 1));
            after_scene_op();
        }
    }

    // Sidebar in the redesign's language: a rounded filter field that floats
    // in the panel instead of a full-width input with a hard bottom border,
    // and list rows as rounded chips with real breathing room.
    void restyle_scene_dock() {
        using Ty  = screenplay::ui::Typography;
        using Sp  = screenplay::ui::Spacing;
        using Rad = screenplay::ui::Radius;
        scene_filter_edit_->setStyleSheet(QString(
            "QLineEdit { background:%1; color:%2; border:%7px solid %3;"
            "            border-radius:%8px; padding:%4px %5px;"
            "            font-size:%6px; }"
            "QLineEdit:focus { border:%7px solid %9; background:%10; }")
            .arg(MD3::hx(MD3::Bg1), MD3::hx(MD3::Text), MD3::hx(MD3::Border))
            .arg(Sp::S).arg(Sp::M).arg(Ty::size_px(Ty::Size::Body))
            .arg(screenplay::ui::BorderWidth::Hairline).arg(Rad::Button)
            .arg(MD3::hx(MD3::Primary), MD3::hx(MD3::Card)));
        scene_list_->setStyleSheet(QString(
            "QListWidget { background:transparent; border:none; outline:none;"
            "              color:%1; font-size:%2px; }"
            "QListWidget::item { padding:%3px %4px; margin:1px 0px;"
            "                    border-radius:%5px; }"
            "QListWidget::item:hover { background:%6; }"
            "QListWidget::item:selected { background:%7; color:%1; }")
            .arg(MD3::hx(MD3::Text))
            .arg(Ty::size_px(Ty::Size::Body))
            .arg(Sp::S).arg(Sp::M).arg(Rad::Chip)
            .arg(MD3::hoverSoft(), MD3::hx(MD3::SelectionBg)));
    }

    void apply_scene_filter() {
        if (!scene_filter_edit_) return;
        const QString q = scene_filter_edit_->text();
        for (int r = 0; r < scene_list_->count(); ++r) {
            auto* it = scene_list_->item(r);
            it->setHidden(!q.isEmpty() &&
                          !it->text().contains(q, Qt::CaseInsensitive));
        }
    }

    void refresh_scenes() {
        // Rebuild the list widget only when scene titles actually changed —
        // this runs on every keystroke and item churn was the costly part.
        QStringList titles;
        int n = 0;
        for (const auto& b : canvas_->ctrl().state().script.blocks)
            if (b.type == screenplay::BlockType::SceneHeading)
                titles << QString("%1. %2").arg(++n)
                              .arg(QString::fromStdString(b.text));
        if (titles != scene_titles_cache_) {
            scene_titles_cache_ = titles;
            scene_list_->clear();
            scene_list_->addItems(titles);
            apply_scene_filter();
        }

        // Auto-scroll scene panel to the scene containing the cursor
        {
            const auto& cursor = canvas_->ctrl().state().cursor;
            const auto& blocks = canvas_->ctrl().state().script.blocks;
            int matching_row = -1, scene_row = -1;
            for (size_t bi = 0; bi < blocks.size(); ++bi) {
                if (blocks[bi].type == screenplay::BlockType::SceneHeading)
                    ++scene_row;
                if (bi == cursor.block_idx) { matching_row = scene_row; break; }
            }
            if (matching_row >= 0) {
                scene_list_->setCurrentRow(matching_row);
                if (scene_list_->currentItem())
                    scene_list_->scrollToItem(scene_list_->currentItem());
            }
        }

        int pages = (int)canvas_->pages().size();
        if (status_) {
            status_->set_page(QString("%1 %2/%3").arg(tr_ui("Page"))
                                  .arg(canvas_->cursor_page()).arg(pages));
            status_->set_runtime(QString("~%1 min").arg(pages));
            status_->set_scenes(QString("%1 %2").arg(tr_ui("Scenes")).arg(n));
        }

        int words = 0;
        for (const auto& b : canvas_->ctrl().state().script.blocks) {
            bool in_word = false;
            for (unsigned char c : b.text) {
                if (std::isspace(c)) in_word = false;
                else if (!in_word) { ++words; in_word = true; }
            }
        }
        if (status_) status_->set_words(QString("%1 %2").arg(tr_ui("Words")).arg(words));
    }

    void refresh_script_panel() {
        if (!script_dock_ || !script_dock_->isVisible()) return;
        const auto& script = canvas_->ctrl().state().script;
        script_panel_->refresh(script, canvas_->pages());
    }

    void update_title() {
        QString name = current_path_.isEmpty()
            ? doc_custom_name_ : QFileInfo(current_path_).baseName();
        setWindowTitle(QString("Screenplay Editor " APP_VERSION " \xe2\x80\x94 ") + name +
                       (canvas_->ctrl().state().dirty ? " *" : ""));
        update_doc_name_display();
        update_capa_badge();
    }

    void update_status() {
        const auto& st = canvas_->ctrl().state();
        if (st.script.blocks.empty() ||
            st.cursor.block_idx >= st.script.blocks.size()) return;
        const auto& blk = st.script.blocks[st.cursor.block_idx];
        update_format_buttons(blk);
    }

    // Element-type, formatting and alignment buttons all reflect the caret's
    // actual block — read from the document, never from "which control fired",
    // so every input path (shortcut, menu, toolbar) stays in sync.
    void update_format_buttons(const screenplay::Block& blk) {
        using BT = screenplay::BlockType;
        QAction* match = nullptr;
        switch (blk.type) {
        case BT::SceneHeading:  match = act_blk_scene_;         break;
        case BT::Action:        match = act_blk_action_;        break;
        case BT::Character:     match = act_blk_character_;     break;
        case BT::Parenthetical: match = act_blk_parenthetical_; break;
        case BT::Dialogue:
        case BT::DualDialogue:  match = act_blk_dialogue_;      break;
        case BT::Transition:    match = act_blk_transition_;    break;
        case BT::Shot:          match = act_blk_shot_;          break;
        case BT::General:       match = act_blk_general_;       break;
        case BT::ActBreak:      match = act_blk_act_break_;     break;
        }
        if (match) match->setChecked(true);
        if (act_page_break_)
            act_page_break_->setChecked(blk.page_break_before);

        const auto& ctrl = canvas_->ctrl();
        if (act_bold_)      act_bold_->setChecked(ctrl.style_is_active(&screenplay::Block::bold_runs));
        if (act_italic_)    act_italic_->setChecked(ctrl.style_is_active(&screenplay::Block::italic_runs));
        if (act_underline_) act_underline_->setChecked(ctrl.style_is_active(&screenplay::Block::underline_runs));

        update_align_buttons(blk);
    }

    // Alignment is enabled only where the document model permits an override,
    // and checked to whatever the block actually carries. A block on Default
    // shows none of the three lit — that state means "the element type's own
    // alignment", which is not the same as any explicit choice.
    void update_align_buttons(const screenplay::Block& blk) {
        using BA = screenplay::BlockAlign;
        const bool can = screenplay::supports_alignment(blk.type);
        const struct { QAction* act; BA align; } items[] = {
            { act_align_left_,   BA::Left   },
            { act_align_center_, BA::Center },
            { act_align_right_,  BA::Right  },
        };
        for (const auto& it : items) {
            if (!it.act) continue;
            it.act->setEnabled(can);
            it.act->setChecked(can && blk.align == it.align);
        }
    }

    void update_zoom() {
        if (status_)
            status_->set_zoom(QString("%1%").arg((int)(canvas_->zoom() * 100)));
    }

    // ── Author notes ──────────────────────────────────────────────────────
    // A note belongs to the block the caret is in. The dialog is deliberately
    // plain: one multi-line field, OK/Clear/Cancel.
    void on_edit_note() {
        auto& ctrl = canvas_->ctrl();
        QDialog dlg(this);
        dlg.setWindowTitle(tr_ui("Note on this block"));
        dlg.setMinimumWidth(420);
        using Sp = screenplay::ui::Spacing;
        auto* lay = new QVBoxLayout(&dlg);
        lay->setContentsMargins(Sp::XL, Sp::XL, Sp::XL, Sp::XL);
        lay->setSpacing(Sp::L);

        auto* caption = new QLabel(
            QString::fromStdString(preview_of_current_block()), &dlg);
        caption->setWordWrap(true);
        caption->setStyleSheet("color:" + MD3::hx(MD3::TextDim) + ";");
        lay->addWidget(caption);

        auto* edit = new QPlainTextEdit(
            QString::fromStdString(ctrl.current_block_note()), &dlg);
        edit->setPlaceholderText(tr_ui("Write a note for this block\xe2\x80\xa6"));
        edit->setMinimumHeight(120);
        lay->addWidget(edit);

        auto* bb = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        auto* clear = bb->addButton(tr_ui("Clear"), QDialogButtonBox::DestructiveRole);
        lay->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        connect(clear, &QPushButton::clicked, &dlg, [&]{ edit->clear(); dlg.accept(); });

        if (dlg.exec() != QDialog::Accepted) return;
        ctrl.set_block_note(edit->toPlainText().trimmed().toStdString());
        canvas_->request_relayout();
        on_changed();
        refresh_notes();
        canvas_->setFocus();
    }

    // A short, single-line excerpt of the current block, so the note dialog
    // and the notes list both say WHAT is being annotated.
    std::string preview_of_current_block() const {
        const auto& st = canvas_->ctrl().state();
        if (st.script.blocks.empty() ||
                st.cursor.block_idx >= st.script.blocks.size()) return {};
        const auto& b = st.script.blocks[st.cursor.block_idx];
        std::string t = b.text.empty() ? std::string("(empty)") : b.text;
        if (t.size() > 70) t = t.substr(0, 67) + "...";
        return tr_block_label(b.type).toStdString() + " \xe2\x80\x94 " + t;
    }

    void setup_notes_dock() {
        notes_list_ = new QListWidget;
        notes_list_->setWordWrap(true);
        connect(notes_list_, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item){
            const QVariant v = item->data(Qt::UserRole);
            if (!v.isValid()) return;          // the "no notes yet" placeholder
            canvas_->scroll_to_block((size_t)v.toInt());
        });

        auto* container = new QWidget;
        auto* lay = new QVBoxLayout(container);
        using Sp = screenplay::ui::Spacing;
        lay->setContentsMargins(Sp::M, Sp::S, Sp::M, Sp::M);
        lay->setSpacing(Sp::S);
        lay->addWidget(notes_list_);

        notes_dock_ = screenplay::ui::make_panel_dock(
            this, tr_ui("AUTHOR NOTES"), container, &notes_frame_);
        notes_dock_->setMinimumWidth(screenplay::ui::SidebarWidth::Minimum);
        addDockWidget(Qt::RightDockWidgetArea, notes_dock_);
        notes_dock_->hide();
        connect(notes_dock_, &QDockWidget::visibilityChanged, this, [this](bool v){
            if (act_view_notes_) act_view_notes_->setChecked(v);
            if (v) refresh_notes();
        });
    }

    void refresh_notes() {
        if (!notes_dock_ || !notes_dock_->isVisible()) return;
        notes_list_->clear();
        const auto& blocks = canvas_->ctrl().state().script.blocks;
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].note.empty()) continue;
            std::string head = blocks[i].text;
            if (head.size() > 48) head = head.substr(0, 45) + "...";
            auto* item = new QListWidgetItem(
                QString::fromStdString(head.empty() ? "(empty)" : head)
                    + "\n" + QString::fromStdString(blocks[i].note));
            item->setData(Qt::UserRole, (int)i);
            notes_list_->addItem(item);
        }
        if (notes_list_->count() == 0)
            notes_list_->addItem(tr_ui("No notes yet \xe2\x80\x94 Ctrl+Alt+M adds one"));
    }

    // ── Frameless-window plumbing ─────────────────────────────────────────
    // Qt implements startSystemResize() on Windows by posting
    // WM_SYSCOMMAND/SC_SIZE, and DefWindowProc ignores that unless the window
    // carries WS_THICKFRAME — which Qt::FramelessWindowHint strips. Putting
    // the bit back (the client area still covers everything, because Qt keeps
    // handling WM_NCCALCSIZE) restores native edge resizing, Aero Snap and the
    // DWM drop shadow, with no title bar coming back.
    void showEvent(QShowEvent* ev) override {
        QMainWindow::showEvent(ev);
#ifdef Q_OS_WIN
        if (!frame_style_patched_) {
            frame_style_patched_ = true;
            auto hwnd = reinterpret_cast<HWND>(winId());
            const LONG_PTR s = GetWindowLongPtr(hwnd, GWL_STYLE);
            SetWindowLongPtr(hwnd, GWL_STYLE,
                             s | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        }
#endif
    }

    // With no OS frame there is nothing for the user to grab, so the window
    // does its own edge hit-testing and hands the gesture to the window
    // manager via startSystemResize (see ui/window_frame.hpp).
    void mousePressEvent(QMouseEvent* ev) override {
        using FG = screenplay::ui::FrameGeometry;
        if (ev->button() == Qt::LeftButton &&
                FG::begin_resize(this, ev->position().toPoint()))
            return;
        QMainWindow::mousePressEvent(ev);
    }

    // Cursor feedback is the only cue that an invisible edge is grabbable.
    void mouseMoveEvent(QMouseEvent* ev) override {
        using FG = screenplay::ui::FrameGeometry;
        if (!isMaximized() && !isFullScreen())
            setCursor(FG::cursor_for(FG::edges_at(size(), ev->position().toPoint())));
        else
            unsetCursor();
        QMainWindow::mouseMoveEvent(ev);
    }

    void leaveEvent(QEvent* ev) override {
        unsetCursor();
        QMainWindow::leaveEvent(ev);
    }

    void changeEvent(QEvent* ev) override {
        if (ev->type() == QEvent::WindowStateChange) {
            if (header_) header_->window_controls()->sync_state();
            apply_frame_grip();
        }
        QMainWindow::changeEvent(ev);
    }

    // A grip-wide margin belongs to the window itself so its edges can be
    // grabbed; maximised there is nothing to grab, so the space goes back to
    // the page.
    void apply_frame_grip() {
        const int g = (isMaximized() || isFullScreen())
                          ? 0 : screenplay::ui::FrameGeometry::kGrip;
        setContentsMargins(g, g, g, g);
    }

    void closeEvent(QCloseEvent* ev) override {
        if (dirty_confirm()) { ev->ignore(); return; }
        auto& cfg = screenplay::config::AppConfig::instance();
        cfg.save_geometry(saveGeometry());
        cfg.save_state(saveState());
        cfg.set_zoom(canvas_->zoom());
        cfg.sync();
        QFile::remove(autosave_path());   // clean exit — no recovery needed
        ev->accept();
    }

    void update_doc_name_display() {
        if (!doc_name_edit_) return;
        QString name = current_path_.isEmpty()
            ? doc_custom_name_ : QFileInfo(current_path_).baseName();
        doc_name_edit_->setText(name);
    }

    void update_capa_badge() {
        if (capa_toggle_act_)
            capa_toggle_act_->setChecked(
                canvas_->ctrl().state().script.title_page.enabled);
    }

    // ── Production ────────────────────────────────────────────────────────
    // Everything a script needs once it leaves the writer and goes to a crew:
    // which coloured pass is open, frozen scene numbers, and cut scenes that
    // keep their number so nothing already scheduled moves.

    void setup_production_menu(QMenu* parent) {
        auto* menu = parent->addMenu(tr_ui("Production"));

        act_revision_ = menu->addAction(
            QString(), this, &MainWindow::on_choose_revision);
        act_clear_revisions_ = menu->addAction(
            tr_ui("Clear Revision Marks"), this,
            &MainWindow::on_clear_revision_marks);

        menu->addSeparator();
        act_lock_scenes_ = menu->addAction(tr_ui("Lock Scene Numbers"));
        act_lock_scenes_->setCheckable(true);
        connect(act_lock_scenes_, &QAction::triggered,
                this, &MainWindow::on_toggle_scene_lock);

        menu->addSeparator();
        act_omit_scene_ = menu->addAction(
            tr_ui("Omit Scene"), this, &MainWindow::on_omit_scene);
        act_omit_scene_->setShortcut(QKeySequence("Ctrl+Shift+X"));

        menu->addSeparator();
        menu->addAction(tr_ui("Reports\xe2\x80\xa6"), this, &MainWindow::on_reports)
            ->setShortcut(QKeySequence("Ctrl+Shift+R"));

        connect(menu, &QMenu::aboutToShow, this,
                &MainWindow::update_production_menu);
        update_production_menu();
    }

    void update_production_menu() {
        auto& ctrl = canvas_->ctrl();
        if (act_revision_) {
            // The pass is named in the item itself, so the open colour is
            // visible without drilling into the dialog.
            act_revision_->setText(
                QString(tr_ui("Revision Pass: %1\xe2\x80\xa6"))
                    .arg(tr_ui(screenplay::revision_name(ctrl.current_revision()))));
        }
        if (act_clear_revisions_)
            act_clear_revisions_->setEnabled(ctrl.has_revision_marks());
        if (act_lock_scenes_)
            act_lock_scenes_->setChecked(ctrl.scenes_locked());
        if (act_omit_scene_)
            act_omit_scene_->setEnabled(ctrl.current_scene_heading().has_value()
                                     && !ctrl.current_scene_is_omitted());
    }

    void on_choose_revision() {
        screenplay::ui::RevisionDialog dlg(this, canvas_->ctrl().current_revision());
        if (dlg.exec() != QDialog::Accepted) return;
        canvas_->ctrl().set_current_revision(dlg.chosen());
        after_production_change(
            dlg.chosen() == screenplay::Revision::None
                ? tr_ui("Revision marking off")
                : QString(tr_ui("%1 revision — edits are now marked"))
                      .arg(tr_ui(screenplay::revision_name(dlg.chosen()))));
    }

    void on_clear_revision_marks() {
        canvas_->ctrl().clear_revision_marks();
        after_production_change(tr_ui("Revision marks cleared"));
    }

    void on_toggle_scene_lock() {
        auto& ctrl = canvas_->ctrl();
        const bool locking = !ctrl.scenes_locked();
        if (locking) ctrl.lock_scenes();
        else         ctrl.unlock_scenes();
        after_production_change(
            locking ? tr_ui("Scene numbers locked — new scenes get 1A, 1B\xe2\x80\xa6")
                    : tr_ui("Scene numbers unlocked — they follow position again"));
    }

    void on_omit_scene() {
        auto& ctrl = canvas_->ctrl();
        if (!ctrl.current_scene_heading() || ctrl.current_scene_is_omitted()) return;
        ctrl.omit_current_scene();
        after_production_change(
            tr_ui("Scene omitted — its number stays. Ctrl+Z restores it."));
    }

    void on_reports() {
        // Each tab rebuilds from the live script when it is shown, so a report
        // is never a stale snapshot of the document behind the dialog.
        auto index_now = [this] {
            return screenplay::database::ScriptIndexBuilder::build(
                canvas_->ctrl().state().script, canvas_->pages());
        };
        auto script_now = [this]() -> const screenplay::Script& {
            return canvas_->ctrl().state().script;
        };

        std::vector<screenplay::ui::ReportTab> tabs = {
            { tr_ui("Scenes"),    [=]{ return screenplay::reports::scene_report(
                                          script_now(), index_now()); } },
            { tr_ui("Characters"),[=]{ return screenplay::reports::character_report(
                                          script_now(), index_now()); } },
            { tr_ui("Locations"), [=]{ return screenplay::reports::location_report(
                                          script_now(), index_now()); } },
        };

        screenplay::ui::ReportDialog dlg(this, std::move(tabs),
                                         current_document_name());
        dlg.exec();
    }

    QString current_document_name() const {
        return current_path_.isEmpty() ? doc_custom_name_
                                       : QFileInfo(current_path_).baseName();
    }

    void after_production_change(const QString& message) {
        canvas_->request_relayout();
        canvas_->setFocus();
        emit canvas_->script_changed();
        update_production_menu();
        show_status_message(message, 6000);
    }

    ScreenplayCanvas* canvas_      = nullptr;
    QListWidget*      scene_list_  = nullptr;
    QLineEdit*        scene_filter_edit_ = nullptr;
    QStringList       scene_titles_cache_;
    // Debounces refresh_script_panel(): it does a full Qt widget
    // rebuild (QTableWidget rows, QComboBox items) over the WHOLE script —
    // real widget-churn cost that a raw computation benchmark doesn't show,
    // and (unlike refresh_scenes(), which already skips rebuilding when
    // titles haven't changed) neither had any guard, so with the Stats or
    // Database dock open every single keystroke rebuilt every row of every
    // table. Same idea as ScreenplayCanvas::relayout_timer_, just for the
    // side-panel widgets instead of the page layout.
    QTimer            panel_refresh_timer_;
    ScriptPanel*      script_panel_ = nullptr;
    QDockWidget*      script_dock_  = nullptr;
    screenplay::ui::PanelFrame* scene_frame_  = nullptr;
    screenplay::ui::PanelFrame* script_frame_ = nullptr;
    screenplay::ui::PanelFrame* notes_frame_  = nullptr;
    QDockWidget*      scene_dock_  = nullptr;
    // ── New chrome (2026 redesign) ────────────────────────────────────────
    // The header spans the window via setMenuWidget(); the toolbar card and
    // status strip are the first and last rows of the central column, so the
    // docks sit beside them exactly as the reference layout shows.
    QWidget*                          central_     = nullptr;
    QVBoxLayout*                      central_col_ = nullptr;
    screenplay::ui::AppHeader*        header_      = nullptr;
    screenplay::ui::FloatingToolbar*  toolbar_     = nullptr;
    screenplay::ui::StatusStrip*      status_      = nullptr;
    QTimer                            status_msg_timer_;

    QLineEdit*        doc_name_edit_ = nullptr;   // owned by header_
    QMenu*            recent_menu_   = nullptr;
    QAction*          capa_toggle_act_   = nullptr;
    QAction*          spell_status_act_  = nullptr;
    QAction*          act_revision_        = nullptr;
    QAction*          act_clear_revisions_ = nullptr;
    QAction*          act_lock_scenes_     = nullptr;
    QAction*          act_omit_scene_      = nullptr;
    QAction*          act_view_scenes_   = nullptr;
    QAction*          act_view_script_   = nullptr;
    QAction*          act_focus_mode_    = nullptr;
    QAction*          act_theme_         = nullptr;
    QAction*          act_theme_light_   = nullptr;
    QAction*          act_theme_dark_    = nullptr;
    QAction*          act_theme_chamber_ = nullptr;
    // Element-type actions — shared by the Format menu AND the Row 3 toolbar
    // (same QAction, so both stay in sync automatically). Checkable +
    // mutually exclusive; update_status() checks the one matching the
    // current block's type after every edit/cursor move.
    QAction*          act_blk_scene_        = nullptr;
    QAction*          act_blk_action_       = nullptr;
    QAction*          act_blk_character_    = nullptr;
    QAction*          act_blk_parenthetical_= nullptr;
    QAction*          act_blk_dialogue_     = nullptr;
    QAction*          act_blk_transition_   = nullptr;
    QAction*          act_blk_shot_         = nullptr;
    QAction*          act_page_break_       = nullptr;
    QAction*          act_blk_general_      = nullptr;
    QAction*          act_blk_act_break_    = nullptr;
    // Formatting actions — shared by the Format menu AND the Row 3 toolbar.
    // Checkable (independently, not exclusive); update_status() reflects the
    // current block's is_bold_/is_italic_/is_underline_ flags.
    QAction*          act_bold_      = nullptr;
    QAction*          act_italic_    = nullptr;
    QAction*          act_underline_ = nullptr;
    // Alignment override actions — shared by the Format menu and the writing
    // toolbar. Checkable but NOT an exclusive QActionGroup: "no override"
    // (BlockAlign::Default) is a real fourth state in which none is checked,
    // and an exclusive group cannot express that.
    QAction*          act_align_left_   = nullptr;
    QAction*          act_align_center_ = nullptr;
    QAction*          act_align_right_  = nullptr;
    // Author notes — the capsule's last two buttons.
    QAction*          act_note_        = nullptr;
    QAction*          act_view_notes_  = nullptr;
    QDockWidget*      notes_dock_      = nullptr;
    QListWidget*      notes_list_      = nullptr;
    bool              frame_style_patched_ = false;  // see showEvent()
    bool              focus_mode_        = false;
    bool              focus_prev_scenes_ = false;
    bool              focus_prev_script_ = false;
    QString           last_save_time_;
    QString           current_path_;
    QString           doc_custom_name_   = "Untitled";
};
