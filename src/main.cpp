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
#include <QToolBar>
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
#include <QCursor>
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
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QPrinter>
#include <QPrintDialog>
#include <QInputDialog>
#include <QTextDocument>
#include <QPushButton>
#include <QFontDatabase>
#include <QMenuBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QSettings>

#include "layout/freetype_metrics.hpp"
#include "layout/layout_engine.hpp"
#include "editor/editor_controller.hpp"
#include "render/renderer.hpp"
#include "io/exporter.hpp"
#include "io/importer.hpp"
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
// MD3 Dark colour palette
// ─────────────────────────────────────────────────────────────────────────────
namespace MD3 {
    const QColor Surface       { 0x1C, 0x1B, 0x1F };  // dark background
    const QColor SurfaceVar    { 0x49, 0x45, 0x4F };
    const QColor OnSurface     { 0xE6, 0xE1, 0xE5 };
    const QColor Primary       { 0xD0, 0xBC, 0xFF };
    const QColor OnPrimary     { 0x38, 0x00, 0x6B };
    const QColor PrimaryContainer{ 0x4F, 0x37, 0x8A };
    const QColor Secondary     { 0xCC, 0xC2, 0xDC };
    const QColor Outline       { 0x93, 0x8F, 0x99 };
    const QColor Error         { 0xF2, 0xB8, 0xB8 };
    const QColor PageBg        { 0xFF, 0xFF, 0xFF };
    const QColor PageShadow    { 0x00, 0x00, 0x00 };
    const QColor Canvas        { 0x14, 0x13, 0x18 };  // slightly darker than Surface
}

// ─────────────────────────────────────────────────────────────────────────────
// Block colours (for the indicator strip)
// ─────────────────────────────────────────────────────────────────────────────
static QColor block_color(screenplay::BlockType t) {
    switch (t) {
    case screenplay::BlockType::SceneHeading:  return { 0xD0, 0xBC, 0xFF };
    case screenplay::BlockType::Action:        return { 0xA8, 0xD5, 0xA2 };
    case screenplay::BlockType::Character:     return { 0xFF, 0xB4, 0x6B };
    case screenplay::BlockType::Parenthetical: return { 0xFF, 0xE0, 0x6B };
    case screenplay::BlockType::Dialogue:      return { 0x6B, 0xC5, 0xFF };
    case screenplay::BlockType::Transition:    return { 0xFF, 0x6B, 0xB0 };
    case screenplay::BlockType::DualDialogue: return { 0x6B, 0xC5, 0xFF };
    default: return MD3::Outline;
    }
}

static const char* block_label(screenplay::BlockType t) {
    switch (t) {
    case screenplay::BlockType::SceneHeading:  return "SCENE [Ctrl+1]";
    case screenplay::BlockType::Action:        return "ACTION[Ctrl+2]";
    case screenplay::BlockType::Character:     return "CHAR. [Ctrl+3]";
    case screenplay::BlockType::Parenthetical: return "PAREN.[Ctrl+4]";
    case screenplay::BlockType::Dialogue:      return "DIAL. [Ctrl+5]";
    case screenplay::BlockType::Transition:    return "TRANS.[Ctrl+6]";
    case screenplay::BlockType::DualDialogue:  return "DUAL  [Ctrl+D]";
    default: return "?";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Font resolver + quality helpers
// ─────────────────────────────────────────────────────────────────────────────

// Qt family name for the loaded monospace font. Set once by resolve_font().
static QString g_courier_family = "Courier Prime";

// Apply renderer-quality settings to any QFont used for custom painting.
// Must be called right after setting the family / pixel-size.
static void apply_render_quality(QFont& f) {
    f.setHintingPreference(QFont::PreferVerticalHinting);
    f.setStyleStrategy(QFont::StyleStrategy(
        QFont::PreferAntialias    |
        QFont::PreferQuality      |
        QFont::NoSubpixelAntialias));
}

// Set to true when falling back to Courier New (checked after MainWindow init)
static bool g_courier_prime_missing = false;

static std::string resolve_font() {
    // ── Courier Prime (bundled) — search multiple base paths ─────────────
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QStringList search_bases = {
        app_dir,
        app_dir + "/..",
        app_dir + "/../..",
        "."   // fallback: working directory
    };

    for (const QString& base : search_bases) {
        QString regular = base + "/fonts/CourierPrime-Regular.ttf";
        if (!QFile::exists(regular)) continue;

        int id = QFontDatabase::addApplicationFont(regular);
        auto fams = QFontDatabase::applicationFontFamilies(id);
        g_courier_family = fams.isEmpty() ? "Courier Prime" : fams.first();

        // Load Bold, Italic, BoldItalic variants so Qt uses real glyphs
        // instead of synthesizing them (synthesized bold = thicker strokes).
        for (const char* suffix : {
            "CourierPrime-Bold.ttf",
            "CourierPrime-Italic.ttf",
            "CourierPrime-BoldItalic.ttf"
        }) {
            QString variant = base + "/fonts/" + suffix;
            if (QFile::exists(variant))
                QFontDatabase::addApplicationFont(variant);
        }
        return regular.toStdString();
    }

    // ── Courier New (system fallback — still a monospace serif, never sans) ─
    g_courier_prime_missing = true;
    for (const char* p : {
        "C:\\Windows\\Fonts\\CourierNew.ttf",
        "C:\\Windows\\Fonts\\cour.ttf"
    }) {
        if (QFile::exists(p)) {
            int id = QFontDatabase::addApplicationFont(p);
            auto fams = QFontDatabase::applicationFontFamilies(id);
            g_courier_family = fams.isEmpty() ? "Courier New" : fams.first();
            return p;
        }
    }
    throw std::runtime_error("Courier font not found.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Qt render target
// ─────────────────────────────────────────────────────────────────────────────
class QtRenderTarget final : public screenplay::render::IRenderTarget {
public:
    QtRenderTarget(QPainter* p, float dpi) : p_(p), dpi_(dpi) {}

    void clear(screenplay::render::Color c) override {
        p_->fillRect(p_->window(), qc(c));
    }
    void fill_rect(float x, float y, float w, float h,
                   screenplay::render::Color c) override {
        p_->fillRect(QRectF(x,y,w,h), qc(c));
    }
    void draw_text(float x, float y, std::string_view text,
                   float px_size, screenplay::render::Color c) override {
        p_->setPen(qc(c));
        QFont f; f.setFamily(g_courier_family);
        f.setPixelSize(qRound(px_size));
        f.setStyleHint(QFont::TypeWriter);
        apply_render_quality(f);
        p_->setFont(f);
        QFontMetricsF fm(f);
        p_->drawText(QPointF(x, y + fm.ascent()),
            QString::fromUtf8(text.data(), (int)text.size()));
    }
    void draw_line(float x1,float y1,float x2,float y2,
                   float w, screenplay::render::Color c) override {
        QPen pen(qc(c)); pen.setWidthF(w); p_->setPen(pen);
        p_->drawLine(QPointF(x1,y1), QPointF(x2,y2));
    }
    void push_clip(float x,float y,float w,float h) override {
        p_->save(); p_->setClipRect(QRectF(x,y,w,h));
    }
    void pop_clip() override { p_->restore(); }
    float pt_to_px(float pt) const override { return pt * dpi_; }

private:
    QPainter* p_; float dpi_;
    static QColor qc(screenplay::render::Color c) {
        return { c.r, c.g, c.b, c.a };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Title page editor dialog
// ─────────────────────────────────────────────────────────────────────────────
class TitlePageDialog : public QDialog {
    Q_OBJECT
public:
    explicit TitlePageDialog(QWidget* parent, const screenplay::TitlePage& tp)
        : QDialog(parent)
    {
        setWindowTitle("Title Page");
        setModal(true);
        setMinimumWidth(440);

        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(12);
        lay->setContentsMargins(16, 16, 16, 16);

        enabled_chk_ = new QCheckBox("Include title page");
        enabled_chk_->setChecked(tp.enabled);
        lay->addWidget(enabled_chk_);

        auto* form_box = new QGroupBox;
        auto* form = new QFormLayout(form_box);
        form->setSpacing(10);

        title_edit_ = new QLineEdit(QString::fromStdString(tp.title));
        title_edit_->setPlaceholderText("Screenplay title");
        form->addRow("Title:", title_edit_);

        credit_combo_ = new QComboBox;
        {
            using AL = screenplay::config::AppLanguage;
            bool is_pt = (screenplay::config::LanguageConfig::current() == AL::Portuguese);
            if (is_pt) {
                credit_combo_->addItems({
                    "Escrito por", "Roteiro de", "Hist\xc3\xb3ria de",
                    "Escrito e Dirigido por", "Roteiro Original de"
                });
            } else {
                credit_combo_->addItems({
                    "Written by", "Screenplay by", "Story by",
                    "Written and Directed by", "An Original Screenplay by"
                });
            }
        }
        {
            int idx = credit_combo_->findText(
                QString::fromStdString(tp.credit_type));
            credit_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
        }
        form->addRow("Credit:", credit_combo_);

        // Authors: one per line
        authors_edit_ = new QPlainTextEdit;
        authors_edit_->setFixedHeight(72);
        authors_edit_->setPlaceholderText("One author per line");
        {
            QStringList lines;
            for (const auto& a : tp.authors)
                lines << QString::fromStdString(a);
            authors_edit_->setPlainText(lines.join('\n'));
        }
        form->addRow("Author(s):", authors_edit_);

        contact_edit_ = new QLineEdit(QString::fromStdString(tp.contact_left));
        contact_edit_->setPlaceholderText("Email or phone");
        form->addRow("Contact:", contact_edit_);

        lay->addWidget(form_box);

        auto* note = new QLabel(
            "<small style='color:#888'>All fields optional. "
            "Do not include date, copyright or logline — not WGA standard.</small>");
        note->setWordWrap(true);
        lay->addWidget(note);

        auto* btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(btns);
    }

    screenplay::TitlePage result() const {
        screenplay::TitlePage tp;
        tp.enabled     = enabled_chk_->isChecked();
        tp.title       = title_edit_->text().toStdString();
        tp.credit_type = credit_combo_->currentText().toStdString();
        tp.contact_left = contact_edit_->text().toStdString();
        for (const auto& line : authors_edit_->toPlainText().split('\n')) {
            auto t = line.trimmed();
            if (!t.isEmpty()) tp.authors.push_back(t.toStdString());
        }
        return tp;
    }

private:
    QCheckBox*    enabled_chk_  = nullptr;
    QLineEdit*    title_edit_   = nullptr;
    QComboBox*    credit_combo_ = nullptr;
    QPlainTextEdit* authors_edit_ = nullptr;
    QLineEdit*    contact_edit_ = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Script language selection dialog
// ─────────────────────────────────────────────────────────────────────────────
class ScriptLanguageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScriptLanguageDialog(QWidget* parent, const QStringList& current_langs)
        : QDialog(parent)
    {
        setWindowTitle("Script Language");
        setModal(true);
        setMinimumWidth(320);

        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(10);
        lay->setContentsMargins(16, 16, 16, 16);

        auto* lbl = new QLabel("O seu roteiro está em qual idioma?\n(What language is your script written in?)");
        lbl->setWordWrap(true);
        lay->addWidget(lbl);

        lay->addSpacing(4);

        struct LangOpt { const char* label; const char* tag; };
        static const LangOpt kOpts[] = {
            { "English",            "en-US" },
            { "Espa\xc3\xb1ol",     "es"    },
            { "Fran\xc3\xa7\x61is", "fr"    },
            { "Portugu\xc3\xaas (Brasil)", "pt-BR" },
        };

        for (const auto& opt : kOpts) {
            auto* chk = new QCheckBox(QString::fromUtf8(opt.label));
            chk->setChecked(current_langs.contains(opt.tag));
            checks_.push_back({ chk, QString::fromUtf8(opt.tag) });
            lay->addWidget(chk);
        }

        lay->addSpacing(4);
        auto* note = new QLabel(
            "<small style='color:#888'>Selecione todos os idiomas usados no roteiro.<br>"
            "A correção ortográfica só marcará palavras erradas em <b>todos</b> os idiomas selecionados.</small>");
        note->setWordWrap(true);
        lay->addWidget(note);

        auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(btns);
    }

    QStringList selected_langs() const {
        QStringList result;
        for (const auto& [chk, tag] : checks_)
            if (chk->isChecked()) result << tag;
        return result;
    }

private:
    std::vector<std::pair<QCheckBox*, QString>> checks_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Autocomplete popup widget
// ─────────────────────────────────────────────────────────────────────────────
class AutocompletePopup : public QWidget {
    Q_OBJECT
public:
    explicit AutocompletePopup(QWidget* parent)
        : QWidget(parent, Qt::SubWindow | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        hide();
    }

    // anchor is in parent (MainWindow) coordinates, already pointing to caret bottom
    void show_suggestions(const std::vector<std::string>& suggs,
                          int selected, QPoint anchor) {
        if (suggs.empty()) { hide(); return; }

        const bool suggestions_changed = (suggs != suggs_);
        suggs_ = suggs;
        sel_   = selected;

        if (suggestions_changed) {
            scroll_offset_ = 0;   // reset scroll on new suggestion set
            QFont f; f.setFamily(g_courier_family); f.setPixelSize(13);
            f.setStyleHint(QFont::TypeWriter);
            apply_render_quality(f);
            QFontMetrics fm(f);

            constexpr int item_h    = 22;
            constexpr int pad_v     = 6;
            constexpr int pad_h     = 12;
            constexpr int max_w     = 300;

            int max_text_w = 0;
            for (const auto& s : suggs)
                max_text_w = std::max(max_text_w,
                    fm.horizontalAdvance(QString::fromStdString(s)));

            item_h_  = item_h;
            pad_v_   = pad_v;
            // Reserve 6px on the right for scroll indicator when list is long
            const bool need_scroll = (int)suggs.size() > kMaxVisible;
            const int  scroll_w    = need_scroll ? 6 : 0;
            cached_w_ = std::min(max_text_w + pad_h * 2 + 10 + scroll_w, max_w);
            cached_h_ = std::min((int)suggs.size(), kMaxVisible) * item_h
                        + pad_v * 2 + kHeaderH;
        }

        clamp_scroll();
        setGeometry(anchor.x(), anchor.y(), cached_w_, cached_h_);
        show();
        raise();
        update();
    }

    void update_selection(int selected) {
        if (sel_ == selected) return;
        sel_ = selected;
        // Scroll to keep selected item visible
        if (sel_ >= 0) {
            if (sel_ < scroll_offset_)
                scroll_offset_ = sel_;
            else if (sel_ >= scroll_offset_ + kMaxVisible)
                scroll_offset_ = sel_ - kMaxVisible + 1;
            clamp_scroll();
        }
        update();
    }

    void set_block_type(screenplay::BlockType t) {
        if (block_type_ == t) return;
        block_type_ = t;
        update();
    }

    void hide_popup() { hide(); }
    bool is_visible() const { return isVisible(); }

signals:
    void item_clicked(int index);

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        int row = (ev->pos().y() - pad_v_ - kHeaderH) / item_h_;
        int idx = row + scroll_offset_;
        if (idx >= 0 && idx < (int)suggs_.size())
            emit item_clicked(idx);
    }

    void wheelEvent(QWheelEvent* ev) override {
        if (suggs_.empty()) return;
        int delta = ev->angleDelta().y() > 0 ? -1 : 1;
        scroll_offset_ += delta;
        clamp_scroll();
        update();
        ev->accept();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Popup body background
        QPainterPath bg;
        bg.addRoundedRect(rect(), 8, 8);
        p.fillPath(bg, QColor(0x2D, 0x2C, 0x31, 245));
        p.setPen(QPen(MD3::Outline, 1));
        p.drawPath(bg);

        // ── SmartType header ──────────────────────────────────────────────
        {
            QPainterPath hdr_path;
            hdr_path.addRoundedRect(QRectF(0, 0, width(), kHeaderH + 8), 8, 8);
            p.fillPath(hdr_path, QColor(0x22, 0x21, 0x26, 245));
            p.fillRect(QRectF(0, kHeaderH / 2, width(), kHeaderH / 2 + 1),
                       QColor(0x22, 0x21, 0x26, 245));

            QFont hf; hf.setFamily("Segoe UI"); hf.setPixelSize(9); hf.setBold(true);
            apply_render_quality(hf);
            p.setFont(hf);
            p.setPen(MD3::Outline);
            p.drawText(QRect(8, 0, width() - 16, kHeaderH),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       "SmartType");
            p.setPen(QPen(MD3::Outline, 1));
            p.drawLine(QPointF(0, kHeaderH), QPointF(width(), kHeaderH));
        }

        // ── Suggestion items ──────────────────────────────────────────────
        QFont f; f.setFamily(g_courier_family); f.setPixelSize(13);
        f.setStyleHint(QFont::TypeWriter);
        apply_render_quality(f);
        p.setFont(f);

        const bool need_scroll = (int)suggs_.size() > kMaxVisible;
        const int  text_right  = need_scroll ? -12 : -6;
        const int  end_idx     = std::min(scroll_offset_ + kMaxVisible, (int)suggs_.size());

        for (int abs_i = scroll_offset_; abs_i < end_idx; ++abs_i) {
            int row = abs_i - scroll_offset_;
            QRect r(0, pad_v_ + kHeaderH + row * item_h_, width(), item_h_);
            if (abs_i == sel_) {
                QPainterPath sel_bg;
                sel_bg.addRoundedRect(r.adjusted(3, 1, -3, -1), 5, 5);
                QColor sel_col = block_color(block_type_);
                sel_col.setAlpha(180);
                p.fillPath(sel_bg, sel_col);
                p.setPen(QColor(0x1A, 0x1A, 0x1A));
            } else {
                p.setPen(MD3::OnSurface);
            }
            p.drawText(r.adjusted(10, 0, text_right, 0),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QString::fromStdString(suggs_[abs_i]));
        }

        // ── Scroll indicator (thin bar on right edge) ─────────────────────
        if (need_scroll) {
            const int total     = (int)suggs_.size();
            const int bar_x     = width() - 5;
            const int list_h    = kMaxVisible * item_h_;
            const int list_y    = kHeaderH + pad_v_;
            float bar_h  = (float)kMaxVisible / total * list_h;
            float bar_y  = list_y + (float)scroll_offset_ / total * list_h;
            p.fillRect(QRectF(bar_x, list_y, 3, list_h),
                       QColor(0x49, 0x45, 0x4F, 120));
            QPainterPath bp;
            bp.addRoundedRect(QRectF(bar_x, bar_y, 3, bar_h), 2, 2);
            p.fillPath(bp, QColor(MD3::Outline));
        }
    }

private:
    static constexpr int kHeaderH   = 18;
    static constexpr int kMaxVisible = 5;

    void clamp_scroll() {
        int max_off = std::max(0, (int)suggs_.size() - kMaxVisible);
        scroll_offset_ = std::clamp(scroll_offset_, 0, max_off);
    }

    std::vector<std::string>  suggs_;
    screenplay::BlockType     block_type_    = screenplay::BlockType::Action;
    int sel_           = -1;
    int scroll_offset_ = 0;
    int item_h_        = 22;
    int pad_v_         = 6;
    int cached_w_      = 0;
    int cached_h_      = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Search match
// ─────────────────────────────────────────────────────────────────────────────
struct SearchMatch {
    size_t block_idx;
    size_t start_offset;  // byte offset in block.text
    size_t end_offset;
};

// ─────────────────────────────────────────────────────────────────────────────
// Search bar widget (overlays the canvas, top-right corner)
// ─────────────────────────────────────────────────────────────────────────────
class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr) : QWidget(parent) {
        setAutoFillBackground(true);
        setFixedHeight(36);

        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(8, 4, 8, 4);
        lay->setSpacing(4);

        edit_ = new QLineEdit;
        edit_->setPlaceholderText("Search\xe2\x80\xa6 (Enter / Shift+Enter)");
        edit_->setFixedWidth(240);
        edit_->setClearButtonEnabled(true);
        lay->addWidget(edit_);

        match_lbl_ = new QLabel("—");
        match_lbl_->setFixedWidth(54);
        match_lbl_->setAlignment(Qt::AlignCenter);
        lay->addWidget(match_lbl_);

        auto* prev_btn = new QToolButton; prev_btn->setText("▲"); prev_btn->setFixedWidth(26);
        auto* next_btn = new QToolButton; next_btn->setText("▼"); next_btn->setFixedWidth(26);
        auto* cls_btn  = new QToolButton; cls_btn ->setText("✕"); cls_btn ->setFixedWidth(26);
        lay->addWidget(prev_btn);
        lay->addWidget(next_btn);
        lay->addWidget(cls_btn);

        adjustSize();

        connect(edit_,     &QLineEdit::textChanged,  this, &SearchBar::query_changed);
        connect(next_btn,  &QToolButton::clicked,    this, &SearchBar::next_requested);
        connect(prev_btn,  &QToolButton::clicked,    this, &SearchBar::prev_requested);
        connect(cls_btn,   &QToolButton::clicked,    this, &SearchBar::close_requested);
    }

    QLineEdit* edit() const { return edit_; }

    void focus_edit() { edit_->setFocus(); edit_->selectAll(); }

    void set_match_info(int current, int total) {
        if (total == 0) {
            match_lbl_->setText(edit_->text().isEmpty() ? "—" : "0");
            match_lbl_->setStyleSheet("color:#F28B82;");
        } else {
            match_lbl_->setText(QString("%1/%2").arg(current + 1).arg(total));
            match_lbl_->setStyleSheet("color:#A8D5A2;");
        }
    }

signals:
    void query_changed(const QString& text);
    void next_requested();
    void prev_requested();
    void close_requested();

private:
    QLineEdit* edit_      = nullptr;
    QLabel*    match_lbl_ = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Canvas
// ─────────────────────────────────────────────────────────────────────────────
class ScreenplayCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ScreenplayCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setMouseTracking(false);   // only need move events while button held

        popup_ = new AutocompletePopup(window());
        connect(popup_, &AutocompletePopup::item_clicked, this, [this](int index){
            ctrl_.set_suggestion_idx(index);
            ctrl_.accept_suggestion();
            ctrl_.dismiss_suggestions();
            just_accepted_ = true;
            popup_->hide_popup();
            request_relayout();
            emit script_changed();
        });

        // Search bar (hidden by default, shown on Ctrl+F)
        search_bar_ = new SearchBar(this);
        search_bar_->hide();
        search_bar_->edit()->installEventFilter(this);
        connect(search_bar_, &SearchBar::query_changed, this, [this](const QString& q){
            rebuild_search(q);
            update();
        });
        connect(search_bar_, &SearchBar::next_requested, this, [this]{ advance_match(+1); });
        connect(search_bar_, &SearchBar::prev_requested, this, [this]{ advance_match(-1); });
        connect(search_bar_, &SearchBar::close_requested, this, [this]{ close_search(); });

        relayout_timer_.setSingleShot(true);
        relayout_timer_.setInterval(16);
        connect(&relayout_timer_, &QTimer::timeout, this, [this]{
            pages_ = engine_.layout(ctrl_.state().script);
            update_popup();   // just_accepted_ guard is inside update_popup()
            spell_timer_.start(1200);
            update();
        });

        blink_timer_.setInterval(530);
        connect(&blink_timer_, &QTimer::timeout, this, [this]{
            blink_on_ = !blink_on_; update();
        });
        blink_timer_.start();

        autosave_timer_.setInterval(120000);
        connect(&autosave_timer_, &QTimer::timeout,
                this, &ScreenplayCanvas::autosave_requested);
        autosave_timer_.start();

        spell_timer_.setSingleShot(true);
        connect(&spell_timer_, &QTimer::timeout, this, [this]{
            update_spell_check();
            update();
        });

        click_timer_.setSingleShot(true);
        connect(&click_timer_, &QTimer::timeout, this, [this]{ click_count_ = 0; });

        // Initialize spell checker with stored language preferences.
        {
            QSettings qs;
            QStringList stored = qs.value("spell_languages").toStringList();
            if (!stored.isEmpty()) {
                std::vector<std::string> vtags;
                for (const auto& l : stored) vtags.push_back(l.toStdString());
                spell_checker_.reinit(vtags);
            }
            // If no preference stored yet the default (en-US) from the
            // SpellChecker constructor is used; the language dialog fires
            // from MainWindow on first run.
        }

        pages_ = engine_.layout(ctrl_.state().script);
    }

    void zoom_reset() { zoom_=1.f; emit zoom_changed(zoom_); relayout_timer_.start(); }
    void zoom_in()    { zoom_=std::clamp(zoom_+.1f,.3f,3.f); emit zoom_changed(zoom_); relayout_timer_.start(); }
    void zoom_out()   { zoom_=std::clamp(zoom_-.1f,.3f,3.f); emit zoom_changed(zoom_); relayout_timer_.start(); }
    float zoom() const { return zoom_; }

    void request_relayout() { relayout_timer_.start(); }
    const screenplay::layout::PageList& pages() const { return pages_; }

    void scroll_to_page(int page_num) {
        if (pages_.empty()) return;
        const auto& geo  = engine_.geometry();
        const float dpi  = (float)logicalDpiX() / 72.f;
        const float ph   = geo.page_h * dpi * zoom_;
        const float gap  = 40.f;
        float py = (float)(page_num - 1) * (ph + gap);
        if (ctrl_.state().script.title_page.enabled) py += ph + gap;
        scroll_y_ = std::clamp(py, 0.f, max_scroll_y());
        update_popup();
        update();
    }

    void scroll_to_block(size_t block_idx) {
        if (pages_.empty()) return;
        const auto& geo = engine_.geometry();
        const float dpi = (float)logicalDpiX() / 72.f;
        const float ph  = geo.page_h * dpi * zoom_;
        const float gap = 40.f;
        float py = gap;
        if (ctrl_.state().script.title_page.enabled) py += ph + gap;
        for (const auto& page : pages_) {
            for (const auto& vl : page.lines) {
                if (vl.block_idx == block_idx && !vl.is_more && !vl.is_contd) {
                    float line_y = py + vl.y * dpi * zoom_;
                    scroll_y_ = std::clamp(line_y - 60.f, 0.f, max_scroll_y());
                    update_popup();
                    return;
                }
            }
            py += ph + gap;
        }
    }

    enum class SceneNumMode { None, Left, Right, Both };
    void set_scene_num_mode(SceneNumMode m) { scene_num_mode_ = m; }
    screenplay::editor::EditorController& ctrl() { return ctrl_; }
    bool spell_available() const { return spell_checker_.available(); }

    void show_search() { open_search(); }

    // Reinitialize spell checker with a new set of language tags.
    // Called when the user changes language preferences.
    void reinit_spell(const std::vector<std::string>& lang_tags) {
        spell_checker_.reinit(lang_tags);
        spell_cache_.clear();
        spell_timer_.start(500);
    }

    void toggle_bold() {
        ctrl_.toggle_bold();
        request_relayout(); emit script_changed();
    }
    void toggle_italic() {
        ctrl_.toggle_italic();
        request_relayout(); emit script_changed();
    }
    void toggle_underline() {
        ctrl_.toggle_underline();
        request_relayout(); emit script_changed();
    }
    void set_bold_future_scenes(bool v) { bold_future_scenes_ = v; }

    void edit_title_page(QWidget* parent) {
        TitlePageDialog dlg(parent, ctrl_.state().script.title_page);
        if (dlg.exec() == QDialog::Accepted) {
            // Copy current script, update title page, reload
            auto script = ctrl_.state().script;
            script.title_page = dlg.result();
            ctrl_.load_script(std::move(script));
            update();
            emit script_changed();
        }
    }

signals:
    void script_changed();
    void autosave_requested();
    void zoom_changed(float z);

protected:
    bool focusNextPrevChild(bool) override { return false; }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing,          true);
        painter.setRenderHint(QPainter::TextAntialiasing,      true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Dark canvas background
        painter.fillRect(rect(), MD3::Canvas);

        const float dpi = (float)logicalDpiX() / 72.f;
        QtRenderTarget target(&painter, dpi);

        const auto& st = ctrl_.state();
        screenplay::render::RenderConfig cfg;
        cfg.zoom                = zoom_;
        cfg.scroll_y_px         = scroll_y_;
        cfg.canvas_w_px         = width();
        cfg.canvas_h_px         = height();
        cfg.cursor              = st.cursor;
        cfg.show_cursor         = blink_on_ && hasFocus();
        cfg.suggestions         = {};    // drawn by popup widget instead
        cfg.suggestion_selected = -1;
        cfg.page_gap_px         = 40.f;

        // Override renderer clear — canvas already filled
        // We'll call render but tell it not to clear
        render_pages(painter, dpi, cfg);

        // Block type indicator strip (left edge)
        draw_type_strip(painter, st);
    }

    void keyPressEvent(QKeyEvent* ev) override {
        using K  = screenplay::editor::Key;
        using BT = screenplay::BlockType;

        const bool ctrl  = ev->modifiers() & Qt::ControlModifier;
        const bool shift = ev->modifiers() & Qt::ShiftModifier;

        // Ctrl+1-6 — layout-independent block type switching.
        //
        // ev->key() returns the UNSHIFTED symbol for the physical key, which on
        // non-QWERTY layouts may not be Qt::Key_1..Key_6.  We therefore try two
        // methods and take the first that matches:
        //
        //   1. Virtual key:  ev->key() ∈ [Key_1, Key_6]  (works on QWERTY / ABNT2)
        //   2. Scan code:    Windows number-row scan codes 2-7 are layout-independent
        //                    (scan 2 = physical "1" key, … scan 7 = physical "6" key)
        //
        // The Shift guard is intentionally removed so that layouts where digits
        // require Shift (e.g. AZERTY Ctrl+Shift+6) still fire the shortcut.
        if (ctrl) {
            static const BT types[] = {
                BT::SceneHeading, BT::Action, BT::Character,
                BT::Parenthetical, BT::Dialogue, BT::Transition
            };

            // Method 1: virtual key (preferred, works on most layouts)
            int idx = ev->key() - Qt::Key_1;

            // Method 2: native scan code fallback (Windows, layout-independent)
            // Scan code 2 → "1" key, 3 → "2", …, 7 → "6"
            if (idx < 0 || idx > 5) {
                int sc = static_cast<int>(ev->nativeScanCode());
                if (sc >= 2 && sc <= 7) idx = sc - 2;
            }

            if (idx >= 0 && idx <= 5) {
                if (popup_->is_visible()) {
                    ctrl_.set_suggestion_idx(idx);
                    ctrl_.accept_suggestion();
                    request_relayout(); popup_->hide_popup(); emit script_changed();
                    return;
                }
                ctrl_.set_block_type(types[idx]);
                // Ctrl+4: clear block, insert () and place cursor inside (code-editor style)
                if (types[idx] == BT::Parenthetical) {
                    size_t bi = ctrl_.state().cursor.block_idx;
                    ctrl_.script_mut().blocks[bi].text.clear();
                    ctrl_.set_cursor_pos({ bi, 0 });
                    screenplay::editor::KeyEvent ke_open, ke_close, ke_left;
                    ke_open.key  = K::Char; ke_open.char_utf8  = "(";
                    ke_close.key = K::Char; ke_close.char_utf8 = ")";
                    ke_left.key  = K::Left;
                    ctrl_.handle_key(ke_open);
                    ctrl_.handle_key(ke_close);
                    ctrl_.handle_key(ke_left);
                }
                request_relayout(); emit script_changed(); return;
            }
        }

        // Ctrl+A
        if (ctrl && ev->key() == Qt::Key_A) {
            ctrl_.select_all(); popup_->hide_popup(); update(); return;
        }

        // Ctrl+F — open search
        if (ctrl && ev->key() == Qt::Key_F) {
            open_search(); return;
        }

        // Ctrl+C — copy
        if (ctrl && !shift && ev->key() == Qt::Key_C) {
            if (ctrl_.state().has_selection)
                QApplication::clipboard()->setText(
                    QString::fromStdString(ctrl_.copy_selection()));
            return;
        }

        // Ctrl+X — cut
        if (ctrl && !shift && ev->key() == Qt::Key_X) {
            if (ctrl_.state().has_selection) {
                QApplication::clipboard()->setText(
                    QString::fromStdString(ctrl_.copy_selection()));
                ctrl_.cut_selection();
                request_relayout(); emit script_changed();
            }
            return;
        }

        // Ctrl+V — paste
        if (ctrl && !shift && ev->key() == Qt::Key_V) {
            std::string txt = QApplication::clipboard()->text().toStdString();
            if (!txt.empty()) { ctrl_.paste(txt); request_relayout(); emit script_changed(); }
            return;
        }

        // Ctrl+B — bold toggle
        if (ctrl && !shift && ev->key() == Qt::Key_B) {
            ctrl_.toggle_bold();
            request_relayout(); emit script_changed(); return;
        }

        // Ctrl+I — italic toggle
        if (ctrl && !shift && ev->key() == Qt::Key_I) {
            ctrl_.toggle_italic();
            request_relayout(); emit script_changed(); return;
        }

        // Ctrl+U — underline toggle
        if (ctrl && !shift && ev->key() == Qt::Key_U) {
            ctrl_.toggle_underline();
            request_relayout(); emit script_changed(); return;
        }

        // Ctrl+D — dual dialogue
        if (ctrl && !shift && ev->key() == Qt::Key_D) {
            ctrl_.activate_dual_dialogue();
            request_relayout(); emit script_changed(); return;
        }

        // Ctrl+Left / Ctrl+Right (±word movement; Shift extends selection)
        if (ctrl && ev->key() == Qt::Key_Left) {
            if (shift) ctrl_.extend_selection_word(-1);
            else       ctrl_.move_word(-1);
            update(); return;
        }
        if (ctrl && ev->key() == Qt::Key_Right) {
            if (shift) ctrl_.extend_selection_word(+1);
            else       ctrl_.move_word(+1);
            update(); return;
        }

        // Ctrl+Backspace / Ctrl+Delete — delete word
        if (ctrl && ev->key() == Qt::Key_Backspace) {
            ctrl_.delete_word(-1); request_relayout(); emit script_changed(); return;
        }
        if (ctrl && ev->key() == Qt::Key_Delete) {
            ctrl_.delete_word(+1); request_relayout(); emit script_changed(); return;
        }

        // Ctrl+Home / Ctrl+End — document start/end
        if (ctrl && ev->key() == Qt::Key_Home) {
            ctrl_.cursor_to_start(); update(); return;
        }
        if (ctrl && ev->key() == Qt::Key_End) {
            ctrl_.cursor_to_end(); update(); return;
        }

        // ── Autocomplete navigation & acceptance ──────────────────────────
        const bool popup_open = popup_->is_visible();

        if (popup_open && !ctrl) {
            // Down/Up navigate suggestions
            if (ev->key() == Qt::Key_Down) {
                ctrl_.next_suggestion();
                popup_->update_selection(ctrl_.state().suggestion_idx);
                update();
                return;
            }
            if (ev->key() == Qt::Key_Up) {
                ctrl_.prev_suggestion();
                popup_->update_selection(ctrl_.state().suggestion_idx);
                update();
                return;
            }
            // Escape closes popup without accepting
            if (ev->key() == Qt::Key_Escape) {
                ctrl_.dismiss_suggestions();
                popup_->hide_popup();
                return;
            }
            // Enter accepts ONLY when popup is open
            if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
                ctrl_.accept_suggestion();
                ctrl_.dismiss_suggestions();
                just_accepted_ = true;
                popup_->hide_popup();
                request_relayout();
                emit script_changed();
                return;
            }
        }

        // Enter on suggestion-capable blocks (popup not yet open): show SmartType
        if (!ctrl && !shift
                && (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)
                && !popup_->is_visible()) {
            const auto& st2 = ctrl_.state();
            if (!st2.script.blocks.empty()
                    && st2.cursor.block_idx < st2.script.blocks.size()) {
                using BT2 = screenplay::BlockType;
                const auto& cb2 = st2.script.blocks[st2.cursor.block_idx];
                const bool has_smart = (cb2.type == BT2::SceneHeading
                                     || cb2.type == BT2::Character
                                     || cb2.type == BT2::Parenthetical);
                const bool at_start = (st2.cursor.byte_offset == 0);
                bool at_time_of_day = false;
                if (cb2.type == BT2::SceneHeading) {
                    std::string up = cb2.text.substr(0, st2.cursor.byte_offset);
                    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
                    at_time_of_day = (up.size() >= 3 &&
                                      up.rfind(" - ") == up.size() - 3);
                }
                if (has_smart && (at_start || at_time_of_day)) {
                    ctrl_.update_autocomplete_public();
                    update_popup();
                    update();
                    return;
                }
            }
        }

        // Tab always accepts if suggestions are active (popup open or not)
        if (!ctrl && !shift && ev->key() == Qt::Key_Tab
                && !ctrl_.state().suggestions.empty()) {
            ctrl_.accept_suggestion();
            ctrl_.dismiss_suggestions();
            just_accepted_ = true;
            popup_->hide_popup();
            setFocus();
            request_relayout();
            emit script_changed();
            return;
        }

        screenplay::editor::KeyEvent ke;
        ke.ctrl = ctrl; ke.shift = shift;

        if (ctrl) {
            switch (ev->key()) {
            case Qt::Key_Z: ke.key = shift ? K::Redo : K::Undo; break;
            case Qt::Key_Y: ke.key = K::Redo; break;
            // Ctrl+S is handled by the MainWindow QShortcut/menu — let it propagate.
            default: QWidget::keyPressEvent(ev); return;
            }
        } else {
            switch (ev->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:     ke.key = K::Enter;     break;
            case Qt::Key_Tab:       ke.key = shift ? K::BackTab : K::Tab; break;
            case Qt::Key_Backtab:   ke.key = K::BackTab;   break;
            case Qt::Key_Backspace: ke.key = K::Backspace; break;
            case Qt::Key_Delete:    ke.key = K::Delete;    break;
            case Qt::Key_Left:      ke.key = K::Left;      break;
            case Qt::Key_Right:     ke.key = K::Right;     break;
            case Qt::Key_Up:        ke.key = K::Up;        break;
            case Qt::Key_Down:      ke.key = K::Down;      break;
            case Qt::Key_Home:      ke.key = K::Home;      break;
            case Qt::Key_End:       ke.key = K::End;       break;
            case Qt::Key_Escape:
                // Escape with no popup: normal editor escape
                ke.key = K::Escape;
                break;
            default:
                if (!ev->text().isEmpty()) {
                    ke.key       = K::Char;
                    ke.char_utf8 = ev->text().toUtf8().toStdString();
                } else { QWidget::keyPressEvent(ev); return; }
            }
        }

        ctrl_.handle_key(ke);
        request_relayout();
        emit script_changed();
        // Show autocomplete immediately after Enter creates a new block
        if (ke.key == screenplay::editor::Key::Enter) {
            QTimer::singleShot(20, this, [this]{
                ctrl_.update_autocomplete_public();
                update_popup();
                update();
            });
        }
    }

    void wheelEvent(QWheelEvent* ev) override {
        if (ev->modifiers() & Qt::ControlModifier) {
            float d = ev->angleDelta().y() > 0 ? .1f : -.1f;
            zoom_ = std::clamp(zoom_ + d, .3f, 3.f);
            emit zoom_changed(zoom_);
            relayout_timer_.start();
        } else {
            scroll_y_ = std::clamp(
                scroll_y_ - ev->angleDelta().y() * .8f,
                0.f, max_scroll_y());
            update_popup();
            update();
        }
    }

    void contextMenuEvent(QContextMenuEvent* ev) override {
        setFocus();
        const screenplay::Cursor c = hit_test(QPointF(ev->pos()));

        const auto& blocks = ctrl_.state().script.blocks;
        if (blocks.empty() || c.block_idx >= blocks.size()) return;

        QMenu menu(this);

        // ── 1. Spell suggestions (if right-clicked word is misspelled) ───
        bool had_spell = false;
        if (spell_checker_.available() && c.block_idx < spell_cache_.size()) {
            for (const auto& ms : spell_cache_[c.block_idx].misspellings) {
                if (c.byte_offset >= ms.start &&
                    c.byte_offset <  ms.start + ms.length) {

                    // Show misspelled word at top (grayed out, bold)
                    {
                        const auto& bt2 = ctrl_.state().script.blocks[c.block_idx].text;
                        std::string word2 = (ms.start + ms.length <= bt2.size())
                            ? bt2.substr(ms.start, ms.length) : std::string{};
                        if (!word2.empty()) {
                            auto* word_act = menu.addAction("\"" + QString::fromStdString(word2) + "\"");
                            word_act->setEnabled(false);
                            QFont wf = word_act->font(); wf.setBold(true); word_act->setFont(wf);
                            menu.addSeparator();
                        }
                    }

                    if (ms.suggestions.empty()) {
                        menu.addAction("(No suggestions)")->setEnabled(false);
                    } else {
                        for (const auto& sug : ms.suggestions) {
                            auto* act = menu.addAction(QString::fromStdString(sug));
                            QFont bf = act->font(); bf.setBold(true); act->setFont(bf);
                            connect(act, &QAction::triggered, this,
                                [this, ms, sug, bi = c.block_idx] {
                                    auto& txt = ctrl_.script_mut().blocks[bi].text;
                                    if (ms.start + ms.length <= txt.size()) {
                                        txt.replace(ms.start, ms.length, sug);
                                        if (bi < spell_cache_.size())
                                            spell_cache_[bi].text_snapshot.clear();
                                        request_relayout();
                                        emit script_changed();
                                    }
                                });
                        }
                    }

                    menu.addSeparator();
                    {
                        const auto& bt = ctrl_.state().script.blocks[c.block_idx].text;
                        std::string word = (ms.start + ms.length <= bt.size())
                            ? bt.substr(ms.start, ms.length) : std::string{};
                        auto* act = menu.addAction("Add to dictionary");
                        connect(act, &QAction::triggered, this, [this, word] {
                            if (!word.empty()) {
                                spell_checker_.add_to_dictionary(word);
                                spell_cache_.clear();
                                request_relayout();
                            }
                        });
                    }

                    menu.addSeparator();
                    had_spell = true;
                    break;
                }
            }
        }
        (void)had_spell;

        // ── 2. Copiar / Recortar / Colar ─────────────────────────────────
        auto* act_copy = menu.addAction("Copy");
        act_copy->setShortcut(QKeySequence::Copy);
        act_copy->setEnabled(ctrl_.state().has_selection);
        connect(act_copy, &QAction::triggered, this, [this] {
            if (ctrl_.state().has_selection)
                QApplication::clipboard()->setText(
                    QString::fromStdString(ctrl_.copy_selection()));
        });

        auto* act_cut = menu.addAction("Cut");
        act_cut->setShortcut(QKeySequence::Cut);
        act_cut->setEnabled(ctrl_.state().has_selection);
        connect(act_cut, &QAction::triggered, this, [this] {
            if (ctrl_.state().has_selection) {
                QApplication::clipboard()->setText(
                    QString::fromStdString(ctrl_.copy_selection()));
                ctrl_.cut_selection();
                request_relayout();
                emit script_changed();
            }
        });

        auto* act_paste = menu.addAction("Paste");
        act_paste->setShortcut(QKeySequence::Paste);
        connect(act_paste, &QAction::triggered, this, [this] {
            std::string txt = QApplication::clipboard()->text().toStdString();
            if (!txt.empty()) {
                ctrl_.paste(txt);
                request_relayout();
                emit script_changed();
            }
        });

        menu.addSeparator();

        // ── 3. Selecionar Tudo ────────────────────────────────────────────
        auto* act_all = menu.addAction("Select All");
        act_all->setShortcut(QKeySequence::SelectAll);
        connect(act_all, &QAction::triggered, this, [this] {
            ctrl_.select_all();
            popup_->hide_popup();
            update();
        });

        menu.addSeparator();

        // ── 4. Submenu "Format as" ───────────────────────────────────────
        auto* fmt_sub = menu.addMenu("Format as");
        using BT = screenplay::BlockType;
        struct FEntry { const char* label; BT type; };
        static constexpr FEntry entries[] = {
            { "Scene Heading",  BT::SceneHeading  },
            { "Action",         BT::Action        },
            { "Character",      BT::Character     },
            { "Parenthetical",  BT::Parenthetical },
            { "Dialogue",       BT::Dialogue      },
            { "Transition",     BT::Transition    },
        };
        const BT cur_type = ctrl_.state().script.blocks[c.block_idx].type;
        for (const auto& e : entries) {
            auto* a = fmt_sub->addAction(QString::fromUtf8(e.label));
            a->setCheckable(true);
            a->setChecked(cur_type == e.type);
            BT t = e.type;
            connect(a, &QAction::triggered, this, [this, t] {
                ctrl_.set_block_type(t);
                request_relayout();
                emit script_changed();
            });
        }

        menu.exec(ev->globalPos());
    }

    void resizeEvent(QResizeEvent*) override {
        if (search_bar_ && search_bar_->isVisible()) {
            int sw = search_bar_->sizeHint().width();
            search_bar_->move(width() - sw - 10, 10);
        }
        update();
    }
    void mousePressEvent(QMouseEvent* ev) override {
        setFocus();
        if (ev->button() == Qt::LeftButton) {
            screenplay::Cursor c = hit_test(ev->position());

            if (!click_timer_.isActive()) click_count_ = 1;
            else                          ++click_count_;
            click_timer_.start(400);

            if (click_count_ == 2) {
                // Double-click: select word
                const auto& text = ctrl_.state().script.blocks[c.block_idx].text;
                const size_t len = text.size();
                size_t ws = c.byte_offset;
                while (ws > 0 && text[ws-1] != ' ' && text[ws-1] != '\n') --ws;
                size_t we = c.byte_offset;
                while (we < len && text[we] != ' ' && text[we] != '\n') ++we;
                if (ws < we)
                    ctrl_.set_selection({c.block_idx, ws}, {c.block_idx, we});
                popup_->hide_popup();
                update();
                return;
            } else if (click_count_ >= 3) {
                // Triple-click: select entire block (paragraph)
                click_count_ = 0;
                const auto& text = ctrl_.state().script.blocks[c.block_idx].text;
                ctrl_.set_selection(
                    {c.block_idx, 0},
                    {c.block_idx, text.size()});
                popup_->hide_popup();
                update();
                return;
            }

            ctrl_.set_cursor_pos(c);
            mouse_selecting_ = true;
            mouse_anchor_    = c;
            blink_on_        = true;
            popup_->hide_popup();
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (mouse_selecting_ && (ev->buttons() & Qt::LeftButton)) {
            screenplay::Cursor c = hit_test(ev->position());
            ctrl_.set_selection(mouse_anchor_, c);
            if (ctrl_.state().has_selection) popup_->hide_popup();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton)
            mouse_selecting_ = false;
    }

    void focusOutEvent(QFocusEvent*) override {
        ctrl_.dismiss_suggestions();
        popup_->hide_popup();
        update();
    }

    void changeEvent(QEvent* ev) override {
        if (ev->type() == QEvent::WindowStateChange) {
            if (window()->isMinimized())
                popup_->hide_popup();
        }
        QWidget::changeEvent(ev);
    }

private:
    // ── Max scroll: prevent scrolling past the last page ──────────────────
    float max_scroll_y() const {
        if (pages_.empty()) return 0.f;
        const float dpi  = (float)logicalDpiX() / 72.f;
        const float ph   = engine_.geometry().page_h * dpi * zoom_;
        const float gap  = 40.f;
        float total = gap + (float)pages_.size() * (ph + gap);
        if (ctrl_.state().script.title_page.enabled) total += ph + gap;
        return std::max(0.f, total - (float)height());
    }

    // ── Hit-test: screen pixel → document Cursor ──────────────────────────
    screenplay::Cursor hit_test(QPointF pos) const {
        const auto& geo  = engine_.geometry();
        const float dpi  = (float)logicalDpiX() / 72.f;
        const float zoom = zoom_;
        const float pw   = geo.page_w * dpi * zoom;
        const float ph   = geo.page_h * dpi * zoom;
        const float cx   = width()  * 0.5f;
        const float gap  = 40.f;

        QFont tf; tf.setFamily(g_courier_family);
        tf.setPixelSize(qRound(engine_.pt_size() * dpi * zoom));
        tf.setStyleHint(QFont::TypeWriter);
        apply_render_quality(tf);
        QFontMetricsF tfm(tf);

        float py = gap - scroll_y_;

        // Mirror title page offset from render_pages()
        if (ctrl_.state().script.title_page.enabled)
            py += ph + gap;

        for (const auto& page : pages_) {
            float page_bot = py + ph;
            if (page_bot < 0) { py += ph + gap; continue; }
            if (py > height()) break;

            float px_left = cx - pw * 0.5f;

            // Only process pages whose vertical span contains the click
            if (pos.y() >= py && pos.y() <= page_bot) {
                const screenplay::layout::VisualLine* best = nullptr;
                float best_dy = std::numeric_limits<float>::max();

                for (const auto& vl : page.lines) {
                    if (vl.is_more || vl.is_contd) continue;  // skip virtual lines
                    float ly  = py + vl.y * dpi * zoom;
                    float lh_px = vl.height * dpi * zoom;
                    // Vertical distance from click to this line
                    float dy = (pos.y() >= ly && pos.y() < ly + lh_px)
                        ? 0.f
                        : std::min(std::abs((float)pos.y() - ly),
                                   std::abs((float)pos.y() - (ly + lh_px)));
                    if (dy < best_dy) { best_dy = dy; best = &vl; }
                }

                if (!best) break;

                // Horizontal hit: find character boundary closest to click X
                float lx    = px_left + best->x * dpi * zoom;
                float rel_x = (float)pos.x() - lx;
                QString disp = QString::fromStdString(best->display_text);

                // Walk character boundaries (Qt char units), find nearest
                int    best_qi   = 0;
                float  best_dist = std::abs(rel_x);
                for (int qi = 1; qi <= disp.size(); ++qi) {
                    float w    = tfm.horizontalAdvance(disp.left(qi));
                    float dist = std::abs(rel_x - w);
                    if (dist < best_dist) { best_dist = dist; best_qi = qi; }
                    if (w > rel_x + 32.f) break;   // early exit: far past click
                }

                // Convert Qt char index → UTF-8 byte offset within this line
                size_t byte_off = best->start_offset
                    + (size_t)disp.left(best_qi).toUtf8().size();
                // Clamp to block text length
                const auto& blk_text =
                    ctrl_.state().script.blocks[best->block_idx].text;
                byte_off = std::min(byte_off, blk_text.size());
                return { best->block_idx, byte_off };
            }

            py += ph + gap;
        }

        // Click outside all pages — snap to document end
        const auto& blocks = ctrl_.state().script.blocks;
        if (blocks.empty()) return { 0, 0 };
        return { blocks.size() - 1, blocks.back().text.size() };
    }

    // ── Custom page rendering (bypass IRenderTarget for full MD3 look) ────
    void render_pages(QPainter& painter, float dpi,
                      const screenplay::render::RenderConfig& cfg) {
        // Snap to nearest 0.5 physical pixel to avoid sub-pixel text blur.
        auto snap = [](float v) { return std::round(v * 2.f) / 2.f; };

        const auto& geo = engine_.geometry();
        const float zoom = cfg.zoom;
        const float pw   = geo.page_w * dpi * zoom;
        const float ph   = geo.page_h * dpi * zoom;
        const float cx   = width() * .5f;
        const float gap  = cfg.page_gap_px;

        float py = gap - cfg.scroll_y_px;

        // ── Title page (before script pages, no page number) ──────────────
        const auto& tp = ctrl_.state().script.title_page;
        if (tp.enabled) {
            float tpx = cx - pw * .5f;

            // Shadow
            for (int s = 8; s >= 1; --s) {
                QColor sc(MD3::PageShadow); sc.setAlpha(6 * s);
                painter.fillRect(QRectF(tpx + s, py + s, pw, ph), sc);
            }
            QPainterPath tp_path;
            tp_path.addRoundedRect(QRectF(tpx, py, pw, ph), 4, 4);
            painter.fillPath(tp_path, MD3::PageBg);
            painter.save();
            painter.setClipPath(tp_path);

            const float ml = geo.margin_left * dpi * zoom;
            const float mb = geo.margin_bot  * dpi * zoom;

            // Title font (larger)
            QFont title_f; title_f.setFamily(g_courier_family);
            title_f.setPixelSize(qRound(engine_.pt_size() * dpi * zoom * 1.25f));
            title_f.setStyleHint(QFont::TypeWriter);
            title_f.setBold(true);
            apply_render_quality(title_f);

            // Body font
            QFont body_f; body_f.setFamily(g_courier_family);
            body_f.setPixelSize(qRound(engine_.pt_size() * dpi * zoom));
            body_f.setStyleHint(QFont::TypeWriter);
            apply_render_quality(body_f);

            painter.setPen(QColor(0x1A, 0x1A, 0x1A));

            // Centered block at ~40% from top
            float center_y = py + ph * 0.40f;

            // Title
            painter.setFont(title_f);
            QFontMetricsF tfm_t(title_f);
            float title_h = tfm_t.height();
            {
                QString q = QString::fromStdString(tp.title);
                float tw = tfm_t.horizontalAdvance(q);
                float tx = tpx + (pw - tw) * 0.5f;
                painter.drawText(QPointF(snap(tx), snap(center_y + tfm_t.ascent())), q);
            }

            // Credit line + authors (centered block below title)
            painter.setFont(body_f);
            QFontMetricsF tfm_b(body_f);
            float lh = tfm_b.height() * 1.5f;
            {
                float row_y = center_y + title_h + lh * 1.5f;
                auto draw_centered = [&](const QString& q, float y) {
                    float tw = tfm_b.horizontalAdvance(q);
                    float tx = tpx + (pw - tw) * 0.5f;
                    painter.drawText(QPointF(snap(tx), snap(y + tfm_b.ascent())), q);
                };

                // Credit (e.g. "Written by")
                const std::string credit = tp.credit_type.empty()
                    ? (screenplay::config::LanguageConfig::current() ==
                       screenplay::config::AppLanguage::Portuguese
                       ? "Escrito por" : "Written by")
                    : tp.credit_type;
                draw_centered(QString::fromStdString(credit), row_y);
                row_y += lh;

                // Authors — one per line
                for (const auto& author : tp.authors) {
                    draw_centered(QString::fromStdString(author), row_y);
                    row_y += lh;
                }
            }

            // Contact — bottom-left only
            if (!tp.contact_left.empty()) {
                float bx = tpx + ml;
                float by = py + ph - mb - tfm_b.height();
                painter.drawText(QPointF(snap(bx), snap(by + tfm_b.ascent())),
                    QString::fromStdString(tp.contact_left));
            }

            painter.restore();
            py += ph + gap;
        }

        const auto& st = ctrl_.state();

        // Precompute scene numbers (block_idx → 1-based, 0 = not a scene heading)
        std::vector<int> scene_num_by_block;
        if (scene_num_mode_ != SceneNumMode::None) {
            scene_num_by_block.resize(st.script.blocks.size(), 0);
            int sn = 0;
            for (size_t bi2 = 0; bi2 < st.script.blocks.size(); ++bi2)
                if (st.script.blocks[bi2].type == screenplay::BlockType::SceneHeading)
                    scene_num_by_block[bi2] = ++sn;
        }

        for (const auto& page : pages_) {
            if (py + ph < 0)  { py += ph + gap; continue; }
            if (py > height()) break;

            float px = cx - pw * .5f;

            // Elevation shadow (MD3 style)
            for (int s = 8; s >= 1; --s) {
                QColor sc(MD3::PageShadow);
                sc.setAlpha(6 * s);
                painter.fillRect(QRectF(px + s, py + s, pw, ph), sc);
            }

            // Page surface (white)
            QPainterPath page_path;
            page_path.addRoundedRect(QRectF(px, py, pw, ph), 4, 4);
            painter.fillPath(page_path, MD3::PageBg);

            // Page number — ONLY from page 2 onward, top-right
            if (page.number >= 2) {
                painter.setPen(QColor(0x99, 0x99, 0x99));
                QFont pnf; pnf.setFamily(g_courier_family); pnf.setPixelSize(11);
                apply_render_quality(pnf);
                painter.setFont(pnf);
                float nr_x = px + geo.page_w * dpi * zoom
                             - geo.margin_right * dpi * zoom - 30;
                float nr_y = py + (geo.margin_top * .55f) * dpi * zoom;
                painter.drawText(QPointF(snap(nr_x), snap(nr_y)),
                                 QString::number(page.number) + ".");
            }

            // Clip to page
            painter.save();
            painter.setClipPath(page_path);

            // Draw each visual line
            QFont tf; tf.setFamily(g_courier_family);
            tf.setPixelSize(qRound(engine_.pt_size() * dpi * zoom));
            tf.setStyleHint(QFont::TypeWriter);
            apply_render_quality(tf);
            painter.setFont(tf);
            QFontMetricsF tfm(tf);

            for (const auto& vl : page.lines) {
                float tx = px + vl.x * dpi * zoom;
                float ty = py + vl.y * dpi * zoom;
                float lh_px = std::round(vl.height * dpi * zoom);

                // ── Virtual MORE / CONT'D lines (gray italic, no editing) ─────
                if (vl.is_more || vl.is_contd) {
                    QFont ghost_f = tf;
                    ghost_f.setItalic(true);
                    painter.setFont(ghost_f);
                    QFontMetricsF ghost_fm(ghost_f);
                    painter.setPen(MD3::Outline);
                    painter.drawText(QPointF(snap(tx), snap(ty + ghost_fm.ascent())),
                        QString::fromStdString(vl.display_text));
                    painter.setFont(tf);
                    continue;
                }

                // Correct font + metrics for this line (bold/italic differ from base)
                const auto& blk_ref = st.script.blocks[vl.block_idx];
                QFont line_font = tf;
                bool eff_bold = blk_ref.is_bold_;
                if (bold_future_scenes_ && blk_ref.type == screenplay::BlockType::SceneHeading
                        && vl.block_idx > st.cursor.block_idx)
                    eff_bold = true;
                line_font.setBold(eff_bold);
                line_font.setItalic(blk_ref.is_italic_);
                line_font.setUnderline(blk_ref.is_underline_);
                const QFontMetricsF line_fm(line_font);

                // ── Active-block highlight: colored left border + faint tint ────
                bool active = (vl.block_idx == st.cursor.block_idx);
                if (active && vl.line_in_block == 0) {
                    float block_line_count = (float)std::max(size_t(1),
                        [&]{
                            size_t cnt = 0;
                            for (const auto& l2 : page.lines)
                                if (l2.block_idx == vl.block_idx) ++cnt;
                            return cnt;
                        }());
                    QColor bc = block_color(blk_ref.type);
                    float bh = lh_px * block_line_count + 4;
                    // Faint tinted background across the full page width
                    painter.fillRect(
                        QRectF(px, ty - 2, pw, bh),
                        QColor(bc.red(), bc.green(), bc.blue(), 14));
                    // 3px solid left border in block colour
                    painter.fillRect(
                        QRectF(px, ty - 2, 3, bh),
                        QColor(bc.red(), bc.green(), bc.blue(), 200));
                }

                // ── Search highlights (drawn BEHIND text) ─────────────────────
                if (search_state_.active) {
                    for (int mi = 0; mi < (int)search_state_.matches.size(); ++mi) {
                        const auto& m = search_state_.matches[mi];
                        if (m.block_idx != vl.block_idx) continue;
                        if (m.end_offset   <= vl.start_offset) continue;
                        if (m.start_offset >= vl.end_offset)   continue;

                        // Overlap of match with this visual line
                        size_t ov_s = std::max(m.start_offset, vl.start_offset) - vl.start_offset;
                        size_t ov_e = std::min(m.end_offset,   vl.end_offset)   - vl.start_offset;
                        ov_s = std::min(ov_s, vl.display_text.size());
                        ov_e = std::min(ov_e, vl.display_text.size());

                        float hx0 = tfm.horizontalAdvance(
                            QString::fromStdString(vl.display_text.substr(0, ov_s)));
                        float hx1 = tfm.horizontalAdvance(
                            QString::fromStdString(vl.display_text.substr(0, ov_e)));

                        bool is_cur = (mi == search_state_.current);
                        painter.fillRect(
                            QRectF(tx + hx0, ty, hx1 - hx0, lh_px),
                            is_cur ? QColor(255, 165,   0, 210)
                                   : QColor(255, 235,  59, 140));
                    }
                }

                // ── Selection highlight ───────────────────────────────────────
                if (st.has_selection) {
                    // Normalise anchor/cursor into [sel_s, sel_e]
                    auto cp_before = [](const screenplay::Cursor& a,
                                        const screenplay::Cursor& b) {
                        return (a.block_idx != b.block_idx)
                            ? a.block_idx  < b.block_idx
                            : a.byte_offset < b.byte_offset;
                    };
                    screenplay::Cursor sel_s = cp_before(st.cursor, st.sel_anchor)
                        ? st.cursor : st.sel_anchor;
                    screenplay::Cursor sel_e = cp_before(st.cursor, st.sel_anchor)
                        ? st.sel_anchor : st.cursor;

                    size_t bi = vl.block_idx;
                    if (bi >= sel_s.block_idx && bi <= sel_e.block_idx) {
                        // Effective byte range of selection within this block
                        size_t eff_s = (bi == sel_s.block_idx) ? sel_s.byte_offset : 0;
                        size_t eff_e = (bi == sel_e.block_idx)
                            ? sel_e.byte_offset
                            : std::numeric_limits<size_t>::max();

                        // Intersect with this visual line
                        if (eff_s < vl.end_offset && eff_e > vl.start_offset) {
                            size_t line_s = (eff_s > vl.start_offset)
                                ? eff_s - vl.start_offset : 0;
                            size_t line_e = std::min(eff_e, vl.end_offset)
                                - vl.start_offset;
                            line_s = std::min(line_s, vl.display_text.size());
                            line_e = std::min(line_e, vl.display_text.size());

                            float hx0 = line_fm.horizontalAdvance(
                                QString::fromStdString(vl.display_text.substr(0, line_s)));
                            float hx1 = line_fm.horizontalAdvance(
                                QString::fromStdString(vl.display_text.substr(0, line_e)));

                            // If selection continues past this line, extend
                            // highlight to cover trailing whitespace visually
                            if (eff_e >= vl.end_offset && !vl.display_text.empty())
                                hx1 = std::max(hx1, (float)line_fm.horizontalAdvance(
                                    QString::fromStdString(vl.display_text)) + 6.f);

                            painter.fillRect(
                                QRectF(tx + hx0, ty, hx1 - hx0, lh_px),
                                QColor(41, 121, 255, 90));
                        }
                    }
                }

                // ── Text ──────────────────────────────────────────────────────
                {
                    painter.setFont(line_font);
                    painter.setPen(QColor(0x1A, 0x1A, 0x1A));
                    painter.drawText(QPointF(snap(tx), snap(ty + line_fm.ascent())),
                        QString::fromStdString(vl.display_text));
                    if (blk_ref.is_bold_ || blk_ref.is_italic_ || blk_ref.is_underline_ || bold_future_scenes_)
                        painter.setFont(tf);  // restore base font
                }

                // ── Scene numbers (left / right / both) ──────────────────────
                if (!scene_num_by_block.empty() && vl.line_in_block == 0
                        && blk_ref.type == screenplay::BlockType::SceneHeading) {
                    int sn = scene_num_by_block[vl.block_idx];
                    if (sn > 0) {
                        QString sn_str = QString::number(sn) + ".";
                        painter.save();
                        painter.setFont(line_font);
                        painter.setPen(MD3::Outline);
                        QFontMetricsF sn_fm(line_font);
                        if (scene_num_mode_ == SceneNumMode::Left ||
                                scene_num_mode_ == SceneNumMode::Both) {
                            float left_x = px + (geo.margin_left - 36.f) * dpi * zoom;
                            painter.drawText(QPointF(snap(left_x), snap(ty + sn_fm.ascent())),
                                sn_str);
                        }
                        if (scene_num_mode_ == SceneNumMode::Right ||
                                scene_num_mode_ == SceneNumMode::Both) {
                            float right_x = px + (geo.page_w - geo.margin_right + 4.f) * dpi * zoom;
                            painter.drawText(QPointF(snap(right_x), snap(ty + sn_fm.ascent())),
                                sn_str);
                        }
                        painter.restore();
                    }
                }

                // ── Spell-check wavy underlines ───────────────────────────────
                if (spell_checker_.available() && vl.block_idx < spell_cache_.size()) {
                    const float uy = ty + line_fm.ascent() + 2.f;
                    for (const auto& ms : spell_cache_[vl.block_idx].misspellings) {
                        size_t ms_end = ms.start + ms.length;
                        if (ms_end <= vl.start_offset) continue;
                        if (ms.start >= vl.end_offset)  continue;

                        size_t ov_s = std::max(ms.start, vl.start_offset) - vl.start_offset;
                        size_t ov_e = std::min(ms_end,   vl.end_offset)   - vl.start_offset;
                        ov_s = std::min(ov_s, vl.display_text.size());
                        ov_e = std::min(ov_e, vl.display_text.size());

                        float wx0 = line_fm.horizontalAdvance(
                            QString::fromStdString(vl.display_text.substr(0, ov_s)));
                        float wx1 = line_fm.horizontalAdvance(
                            QString::fromStdString(vl.display_text.substr(0, ov_e)));
                        if (wx1 <= wx0) continue;

                        // Draw wavy underline
                        const float amp  = 1.5f;
                        const float step = 3.5f;
                        QPainterPath wave;
                        float wx = tx + wx0;
                        float wend = tx + wx1;
                        wave.moveTo(wx, uy);
                        bool up = true;
                        while (wx < wend) {
                            float nx = std::min(wx + step, wend);
                            wave.quadTo((wx + nx) * 0.5f, uy + (up ? -amp : amp), nx, uy);
                            wx = nx;
                            up = !up;
                        }
                        painter.save();
                        QPen wp(QColor(0xFF, 0x3A, 0x3A, 230));
                        wp.setWidthF(1.6f);
                        painter.setPen(wp);
                        painter.setBrush(Qt::NoBrush);
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.drawPath(wave);
                        painter.restore();
                    }
                }

                // ── Ghost text (inline suggestion preview) ────────────────────
                if (popup_->is_visible() && !st.suggestions.empty()
                        && vl.block_idx == st.cursor.block_idx) {
                    const std::string& block_text = st.script.blocks[vl.block_idx].text;
                    bool is_last_blk_line2 = (vl.end_offset >= block_text.size());
                    bool cursor_at_end     = (st.cursor.byte_offset == block_text.size());
                    bool cursor_on_line2   =
                        (st.cursor.byte_offset >= vl.start_offset) &&
                        (is_last_blk_line2 ? (st.cursor.byte_offset <= vl.end_offset)
                                           : (st.cursor.byte_offset <  vl.end_offset));

                    if (cursor_on_line2 && cursor_at_end) {
                        // Use the currently selected suggestion (tracks Up/Down nav)
                        int sidx = (st.suggestion_idx >= 0 &&
                                    st.suggestion_idx < (int)st.suggestions.size())
                            ? st.suggestion_idx : 0;
                        const std::string& sugg = st.suggestions[sidx];

                        // Only compare against the last word being typed, not the full block.
                        size_t last_space = block_text.rfind(' ');
                        std::string last_word = (last_space == std::string::npos)
                            ? block_text
                            : block_text.substr(last_space + 1);
                        std::string last_word_upper =
                            QString::fromStdString(last_word).toUpper().toStdString();

                        // Suffix = what remains of the suggestion after the typed last word.
                        std::string ghost;
                        if (sugg.size() >= last_word_upper.size() &&
                                sugg.substr(0, last_word_upper.size()) == last_word_upper) {
                            ghost = sugg.substr(last_word_upper.size());
                        } else {
                            ghost = "";
                        }

                        if (!ghost.empty()) {
                            float ghost_x = tx + line_fm.horizontalAdvance(
                                QString::fromStdString(vl.display_text));
                            painter.save();
                            painter.setFont(line_font);
                            painter.setPen(QColor(0xAA, 0xAA, 0xAA, 160));
                            painter.drawText(QPointF(snap(ghost_x), snap(ty + line_fm.ascent())),
                                QString::fromStdString(ghost));
                            painter.restore();
                        }
                    }
                }

                // ── Caret — correct on ALL wrapped lines ──────────────────────
                if (active && blink_on_ && hasFocus()) {
                    // Determine whether the cursor byte offset falls on this line.
                    // For non-final wrapped lines: [start_offset, end_offset)
                    // For the final wrapped line:  [start_offset, end_offset]
                    const size_t block_text_size =
                        st.script.blocks[vl.block_idx].text.size();
                    bool is_last_line = (vl.end_offset >= block_text_size);

                    bool cursor_here =
                        (st.cursor.byte_offset >= vl.start_offset) &&
                        (is_last_line
                            ? (st.cursor.byte_offset <= vl.end_offset)
                            : (st.cursor.byte_offset <  vl.end_offset));

                    if (cursor_here) {
                        const auto& blk_c = st.script.blocks[vl.block_idx];
                        size_t raw_len = (st.cursor.byte_offset > vl.start_offset)
                            ? st.cursor.byte_offset - vl.start_offset : 0;

                        // For uppercase blocks, display_text is toUpper(block.text).
                        // Remap raw_len through the same transform so the caret
                        // lands at the correct display-space byte position.
                        size_t cursor_in_line;
                        if (screenplay::layout::format_for(blk_c.type).uppercase
                                && vl.start_offset <= blk_c.text.size()) {
                            std::string raw_seg = blk_c.text.substr(
                                vl.start_offset,
                                std::min(raw_len,
                                         blk_c.text.size() - vl.start_offset));
                            std::string up_seg =
                                QString::fromStdString(raw_seg).toUpper().toStdString();
                            cursor_in_line = std::min(up_seg.size(),
                                                      vl.display_text.size());
                        } else {
                            cursor_in_line = std::min(raw_len,
                                                      vl.display_text.size());
                        }

                        float cw = line_fm.horizontalAdvance(
                            QString::fromStdString(
                                vl.display_text.substr(0, cursor_in_line)));
                        painter.setPen(QPen(block_color(blk_c.type), 2.f));
                        painter.drawLine(
                            QPointF(tx + cw, ty),
                            QPointF(tx + cw, ty + lh_px));
                    }
                }
            }

            painter.restore();
            py += ph + gap;
        }
    }

    void draw_type_strip(QPainter& painter, const screenplay::editor::EditorState& st) {
        if (st.script.blocks.empty() ||
            st.cursor.block_idx >= st.script.blocks.size()) return;
        const auto& cb  = st.script.blocks[st.cursor.block_idx];
        QColor col = block_color(cb.type);

        // 5px coloured strip on left
        painter.fillRect(0, 0, 5, height(), col);

        // Badge
        QFont f; f.setFamily("Segoe UI"); f.setPixelSize(10); f.setBold(true);
        apply_render_quality(f);
        painter.setFont(f);
        QFontMetrics fm(f);
        QString lbl = QString::fromUtf8(block_label(cb.type));
        int tw = fm.horizontalAdvance(lbl);

        QRect badge(8, 8, tw + 14, 20);
        QPainterPath bp; bp.addRoundedRect(badge, 6, 6);
        painter.fillPath(bp, QColor(col.red(), col.green(), col.blue(), 50));
        painter.setPen(col);
        painter.drawText(badge, Qt::AlignCenter, lbl);
    }

    void update_popup() {
        // If a suggestion was just accepted, block this one call and clear the flag.
        // This suppresses re-population from wheelEvent, scroll, mouse, and the
        // relayout timer — every path that reaches here.
        if (just_accepted_) { just_accepted_ = false; popup_->hide_popup(); return; }

        const auto& st = ctrl_.state();

        // Hide when no suggestions or selection is active
        if (st.suggestions.empty() || st.has_selection) {
            popup_->hide_popup();
            return;
        }

        // Compute the real caret pixel position by scanning the layout
        const auto& geo  = engine_.geometry();
        const float dpi  = (float)logicalDpiX() / 72.f;
        const float zoom = zoom_;
        const float pw   = geo.page_w * dpi * zoom;
        const float ph   = geo.page_h * dpi * zoom;
        const float cx   = width() * .5f;
        const float gap  = 40.f;

        QFont tf; tf.setFamily(g_courier_family);
        tf.setPixelSize(qRound(engine_.pt_size() * dpi * zoom));
        tf.setStyleHint(QFont::TypeWriter);
        apply_render_quality(tf);
        QFontMetricsF tfm(tf);

        float py = gap - scroll_y_;

        // Mirror title page offset from render_pages()
        if (st.script.title_page.enabled)
            py += ph + gap;

        for (const auto& page : pages_) {
            float page_bot = py + ph;
            if (page_bot < 0)  { py += ph + gap; continue; }
            if (py > height()) break;

            float px_left = cx - pw * .5f;

            for (const auto& vl : page.lines) {
                if (vl.block_idx != st.cursor.block_idx) continue;

                const size_t block_text_size =
                    st.script.blocks[vl.block_idx].text.size();
                bool is_last = (vl.end_offset >= block_text_size);
                bool cursor_here =
                    (st.cursor.byte_offset >= vl.start_offset) &&
                    (is_last ? st.cursor.byte_offset <= vl.end_offset
                             : st.cursor.byte_offset <  vl.end_offset);

                if (!cursor_here) continue;

                float tx = px_left + vl.x * dpi * zoom;
                float ty = py + vl.y * dpi * zoom;
                float lh_px = vl.height * dpi * zoom;

                size_t cursor_in_line = std::min(
                    st.cursor.byte_offset - vl.start_offset,
                    vl.display_text.size());
                float cw = tfm.horizontalAdvance(
                    QString::fromStdString(
                        vl.display_text.substr(0, cursor_in_line)));

                // Map canvas-local point to MainWindow coordinate space
                QPoint local_pt((int)(tx + cw), (int)(ty + lh_px + 2));
                QPoint win_pt = mapTo(window(), local_pt);

                popup_->set_block_type(st.script.blocks[st.cursor.block_idx].type);
                popup_->show_suggestions(st.suggestions, st.suggestion_idx,
                                         win_pt);
                return;
            }

            py += ph + gap;
        }

        popup_->hide_popup();
    }

    // ── Search ────────────────────────────────────────────────────────────

    struct SearchState {
        std::vector<SearchMatch> matches;
        int  current = -1;
        bool active  = false;
    };

    void rebuild_search(const QString& query) {
        search_state_.matches.clear();
        search_state_.current = -1;
        search_state_.active  = !query.isEmpty();

        if (!search_state_.active) {
            if (search_bar_) search_bar_->set_match_info(-1, 0);
            return;
        }

        const auto& blocks = ctrl_.state().script.blocks;
        for (size_t bi = 0; bi < blocks.size(); ++bi) {
            QString text = QString::fromStdString(blocks[bi].text);
            int pos = 0;
            while ((pos = text.indexOf(query, pos, Qt::CaseInsensitive)) != -1) {
                // Convert QString char index → UTF-8 byte offset
                size_t start_b = (size_t)text.left(pos).toUtf8().size();
                size_t end_b   = start_b + (size_t)text.mid(pos, query.size()).toUtf8().size();
                search_state_.matches.push_back({ bi, start_b, end_b });
                ++pos;
            }
        }

        if (!search_state_.matches.empty())
            search_state_.current = 0;

        update_search_bar_info();
    }

    void advance_match(int dir) {   // +1 = forward, -1 = backward
        if (search_state_.matches.empty()) return;
        int n = (int)search_state_.matches.size();
        search_state_.current = ((search_state_.current + dir) % n + n) % n;
        update_search_bar_info();
        update();
    }

    void update_search_bar_info() {
        if (search_bar_)
            search_bar_->set_match_info(search_state_.current,
                                        (int)search_state_.matches.size());
    }

    void open_search() {
        if (!search_bar_) return;
        int sw = search_bar_->sizeHint().width();
        search_bar_->move(width() - sw - 10, 10);
        search_bar_->show();
        search_bar_->raise();
        search_bar_->focus_edit();
    }

    void close_search() {
        if (search_bar_) search_bar_->hide();
        search_state_ = SearchState{};
        setFocus();
        update();
    }

    // ── eventFilter: intercept Enter / Escape inside the search QLineEdit ─

    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (search_bar_ && obj == search_bar_->edit()
            && ev->type() == QEvent::KeyPress)
        {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                if (ke->modifiers() & Qt::ShiftModifier) advance_match(-1);
                else                                      advance_match(+1);
                return true;
            }
            if (ke->key() == Qt::Key_Escape) { close_search(); return true; }
        }
        return QWidget::eventFilter(obj, ev);
    }

    // ── Font metrics helper ────────────────────────────────────────────────
    // Returns QFontMetricsF for the given block, applying bold/italic to base.
    static QFontMetricsF metrics_for(const screenplay::Block& blk, const QFont& base) {
        if (!blk.is_bold_ && !blk.is_italic_) return QFontMetricsF(base);
        QFont f = base;
        f.setBold(blk.is_bold_);
        f.setItalic(blk.is_italic_);
        return QFontMetricsF(f);
    }

    // ── Spell check ────────────────────────────────────────────────────────

    void update_spell_check() {
        if (!spell_checker_.available()) return;
        const auto& blocks = ctrl_.state().script.blocks;
        spell_cache_.resize(blocks.size());
        for (size_t i = 0; i < blocks.size(); ++i) {
            const std::string& txt = blocks[i].text;
            if (spell_cache_[i].text_snapshot == txt) continue; // unchanged
            spell_cache_[i].text_snapshot = txt;
            // Skip very long blocks to avoid latency in the spell API
            if (txt.size() > 500) {
                spell_cache_[i].misspellings.clear();
                continue;
            }
            // Only check complete words — skip last word if user is still typing it
            std::string txt_to_check = txt;
            size_t last_space = txt.find_last_of(" \t\n");
            if (last_space == std::string::npos) {
                // Entire block is one word being typed — skip entirely
                spell_cache_[i].misspellings = {};
                spell_cache_[i].text_snapshot = ""; // force recheck later
                continue;
            }
            txt_to_check = txt.substr(0, last_space + 1);
            spell_cache_[i].misspellings  = spell_checker_.check(txt_to_check);
        }
        // Shrink if blocks were deleted
        if (spell_cache_.size() > blocks.size())
            spell_cache_.resize(blocks.size());
    }

    void show_spell_menu(QPoint global_pos, size_t block_idx,
                         const screenplay::spellcheck::Misspelling& ms) {
        QMenu menu(this);

        if (ms.suggestions.empty()) {
            menu.addAction("(No suggestions)")->setEnabled(false);
        } else {
            for (const auto& sug : ms.suggestions) {
                QString qs = QString::fromStdString(sug);
                auto* act  = menu.addAction(qs);
                connect(act, &QAction::triggered, this,
                    [this, block_idx, ms, sug] {
                        auto& text = ctrl_.script_mut().blocks[block_idx].text;
                        if (ms.start + ms.length <= text.size()) {
                            text.replace(ms.start, ms.length, sug);
                            // Invalidate cache for this block so it re-checks
                            if (block_idx < spell_cache_.size())
                                spell_cache_[block_idx].text_snapshot.clear();
                            request_relayout();
                            emit script_changed();
                        }
                    });
            }
        }

        menu.addSeparator();
        {
            const auto& blktext = ctrl_.state().script.blocks[block_idx].text;
            std::string word = (ms.start + ms.length <= blktext.size())
                ? blktext.substr(ms.start, ms.length) : std::string{};
            auto* act = menu.addAction("Add to dictionary");
            connect(act, &QAction::triggered, this, [this, word] {
                if (!word.empty()) {
                    spell_checker_.add_to_dictionary(word);
                    spell_cache_.clear();   // force full re-check
                    request_relayout();
                }
            });
        }

        menu.exec(global_pos);
    }

    std::unique_ptr<screenplay::layout::FreeTypeMetrics> metrics_ =
        std::make_unique<screenplay::layout::FreeTypeMetrics>(
            resolve_font().c_str());

    screenplay::layout::LayoutEngine      engine_{ *metrics_ };
    screenplay::editor::EditorController  ctrl_;
    screenplay::layout::PageList          pages_;
    AutocompletePopup*                    popup_      = nullptr;
    SearchBar*                            search_bar_ = nullptr;
    SearchState                           search_state_;

    // Spell check
    struct SpellBlockCache {
        std::string text_snapshot;
        std::vector<screenplay::spellcheck::Misspelling> misspellings;
    };
    screenplay::spellcheck::SpellChecker  spell_checker_;
    std::vector<SpellBlockCache>          spell_cache_;

    float  zoom_         = 1.f;
    float  scroll_y_     = 0.f;
    bool   blink_on_     = true;
    SceneNumMode scene_num_mode_ = SceneNumMode::Both;
    bool   bold_future_scenes_ = false;
    bool   just_accepted_ = false;  // set true after accepting a suggestion;
                                    // consumed by the next update_popup() call
    QTimer relayout_timer_, blink_timer_, autosave_timer_, spell_timer_;

    // Mouse drag selection
    bool               mouse_selecting_ = false;
    screenplay::Cursor mouse_anchor_    = { 0, 0 };

    // Multi-click tracking
    int    click_count_ = 0;
    QTimer click_timer_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Stats panel
// ─────────────────────────────────────────────────────────────────────────────
class StatsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatsPanel(QWidget* p = nullptr) : QWidget(p) {
        setStyleSheet("background: transparent; color: #E6E1E5; font-size: 12px;");
        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(5); lay->setContentsMargins(10,10,10,10);
        auto mk = [&](const QString& t) {
            auto* l = new QLabel(t); lay->addWidget(l); return l;
        };
        lbl_pages_    = mk("Pages: —");
        lbl_scenes_   = mk("Scenes: —");
        lbl_words_    = mk("Words: —");
        lbl_time_     = mk("Time: —");
        lay->addSpacing(8);
        auto* ct = new QLabel("Characters:"); ct->setStyleSheet("font-weight:bold;");
        lay->addWidget(ct);
        char_list_ = new QListWidget;
        char_list_->setStyleSheet(
            "QListWidget { background:#2D2C31; border-radius:8px; color:#E6E1E5; font-size:11px; }");
        char_list_->setMaximumHeight(160);
        lay->addWidget(char_list_);
        lay->addStretch();
    }

    void refresh(const screenplay::stats::ScriptStats& s) {
        lbl_pages_ ->setText(QString("Pages: %1").arg(s.total_pages));
        lbl_scenes_->setText(QString("Scenes: %1").arg(s.total_scenes));
        lbl_words_ ->setText(QString("Words: %1").arg(s.total_words));
        lbl_time_  ->setText(QString("Time: ~%1 min").arg((int)s.screen_time_min));
        char_list_->clear();
        for (const auto& [name, cnt] :
             screenplay::stats::StatsEngine::top_characters(s))
            char_list_->addItem(
                QString("%1  (%2 lines)").arg(
                    QString::fromStdString(name)).arg(cnt));
    }
private:
    QLabel* lbl_pages_=nullptr, *lbl_scenes_=nullptr,
           *lbl_words_=nullptr, *lbl_time_=nullptr;
    QListWidget* char_list_=nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// Script Database panel — Scenes / Characters / Dialogue tabs
// ─────────────────────────────────────────────────────────────────────────────
class ScriptDatabasePanel : public QWidget {
    Q_OBJECT
public:
    explicit ScriptDatabasePanel(QWidget* p = nullptr) : QWidget(p) {
        using LC = screenplay::config::LanguageConfig;
        auto tr = [](const char* k){ return QString::fromUtf8(LC::tr(k)); };

        static const char* kTableStyle =
            "QTableWidget { background:#2D2C31; color:#E6E1E5; border:none; font-size:11px; }"
            "QHeaderView::section { background:#1C1B1F; color:#CCC2DC; border:none;"
            "                       padding:4px; font-size:10px; font-weight:bold; }"
            "QTableWidget::item:selected { background:#4F378A; }";

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);

        // ── Filter bar ──────────────────────────────────────────────────────
        auto* filter_row = new QWidget;
        auto* fr_lay = new QHBoxLayout(filter_row);
        fr_lay->setContentsMargins(4, 4, 4, 2);
        fr_lay->setSpacing(4);

        filter_edit_ = new QLineEdit;
        filter_edit_->setPlaceholderText(tr("db_filter_hint"));
        filter_edit_->setClearButtonEnabled(true);
        fr_lay->addWidget(filter_edit_, 1);

        scene_combo_ = new QComboBox;
        scene_combo_->addItem(tr("db_filter_all_scenes"), QVariant(0));
        scene_combo_->setMinimumWidth(140);
        fr_lay->addWidget(scene_combo_);

        char_combo_ = new QComboBox;
        char_combo_->addItem(tr("db_filter_all_chars"), QVariant(QString()));
        char_combo_->setMinimumWidth(120);
        fr_lay->addWidget(char_combo_);

        lay->addWidget(filter_row);

        // ── Tabs ────────────────────────────────────────────────────────────
        tabs_ = new QTabWidget;
        tabs_->setStyleSheet(
            "QTabWidget::pane { border:none; }"
            "QTabBar::tab { background:#2D2C31; color:#E6E1E5; padding:6px 10px;"
            "               border:none; font-size:11px; }"
            "QTabBar::tab:selected { background:#4F378A; border-radius:4px 4px 0 0; }");

        // ── Tab 1: Scenes ──────────────────────────────────────────────────
        scene_table_ = new QTableWidget;
        scene_table_->setColumnCount(7);
        scene_table_->setHorizontalHeaderLabels({
            tr("db_scenes_num"),   tr("db_scenes_prefix"), tr("db_scenes_loc"),
            tr("db_scenes_time"),  tr("db_scenes_chars"),  tr("db_scenes_lines"),
            tr("db_scenes_page")
        });
        scene_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        scene_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        scene_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        scene_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        scene_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        scene_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        scene_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
        scene_table_->verticalHeader()->setVisible(false);
        scene_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        scene_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        scene_table_->setAlternatingRowColors(false);
        scene_table_->setSortingEnabled(true);
        scene_table_->setContextMenuPolicy(Qt::CustomContextMenu);
        scene_table_->setStyleSheet(kTableStyle);
        tabs_->addTab(scene_table_, tr("db_tab_scenes"));

        // ── Tab 2: Characters ──────────────────────────────────────────────
        char_table_ = new QTableWidget;
        char_table_->setColumnCount(5);
        char_table_->setHorizontalHeaderLabels({
            tr("db_chars_name"), tr("db_chars_dlg"), tr("db_chars_scn"),
            tr("db_chars_scns"), tr("db_chars_first")
        });
        char_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        char_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        char_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        char_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        char_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        char_table_->verticalHeader()->setVisible(false);
        char_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        char_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        char_table_->setAlternatingRowColors(true);
        char_table_->setSortingEnabled(true);
        char_table_->setContextMenuPolicy(Qt::CustomContextMenu);
        char_table_->setStyleSheet(kTableStyle);
        tabs_->addTab(char_table_, tr("db_tab_chars"));

        // ── Tab 3: Dialogue ────────────────────────────────────────────────
        dial_table_ = new QTableWidget;
        dial_table_->setColumnCount(4);
        dial_table_->setHorizontalHeaderLabels({
            tr("db_dial_scene"), tr("db_dial_char"),
            tr("db_dial_paren"), tr("db_dial_text")
        });
        dial_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        dial_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        dial_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        dial_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        dial_table_->verticalHeader()->setVisible(false);
        dial_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        dial_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        dial_table_->setAlternatingRowColors(true);
        dial_table_->setSortingEnabled(false);
        dial_table_->setStyleSheet(kTableStyle);
        tabs_->addTab(dial_table_, tr("db_tab_dialogue"));

        lay->addWidget(tabs_, 1);

        // ── Stats footer ────────────────────────────────────────────────────
        stats_lbl_ = new QLabel;
        stats_lbl_->setStyleSheet("color:#938F99; font-size:10px; padding:2px 6px;");
        lay->addWidget(stats_lbl_);

        // ── Connections ────────────────────────────────────────────────────
        connect(filter_edit_, &QLineEdit::textChanged,
                this, [this](const QString&){ apply_filter(); });
        connect(scene_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ apply_filter(); });
        connect(char_combo_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ apply_filter(); });

        // Double-click: navigate to block
        connect(scene_table_, &QTableWidget::cellDoubleClicked, this,
                [this](int row, int) {
            auto* item = scene_table_->item(row, 0);
            if (item) emit navigate_to_block((size_t)item->data(Qt::UserRole).toULongLong());
        });
        connect(char_table_, &QTableWidget::cellDoubleClicked, this,
                [this](int row, int) {
            auto* item = char_table_->item(row, 0);
            if (item) emit navigate_to_block((size_t)item->data(Qt::UserRole).toULongLong());
        });
        connect(dial_table_, &QTableWidget::cellDoubleClicked, this,
                [this](int row, int) {
            auto* item = dial_table_->item(row, 0);
            if (item) emit navigate_to_block((size_t)item->data(Qt::UserRole).toULongLong());
        });

        // Right-click on scene table
        connect(scene_table_, &QWidget::customContextMenuRequested, this,
                [this](const QPoint& pos) {
            using LC2 = screenplay::config::LanguageConfig;
            auto tr2 = [](const char* k){ return QString::fromUtf8(LC2::tr(k)); };
            auto* item = scene_table_->itemAt(pos);
            if (!item) return;
            int row = scene_table_->row(item);
            auto* num_item = scene_table_->item(row, 0);
            if (!num_item) return;
            size_t block_idx   = (size_t)num_item->data(Qt::UserRole).toULongLong();
            int    scene_num   = num_item->data(Qt::UserRole + 1).toInt();

            QMenu menu(this);
            auto* act_nav   = menu.addAction(tr2("db_ctx_view_scene"));
            auto* act_chars = menu.addAction(tr2("db_ctx_chars_scene"));
            connect(act_nav,   &QAction::triggered, this, [this, block_idx]{ emit navigate_to_block(block_idx); });
            connect(act_chars, &QAction::triggered, this, [this, scene_num]{
                filter_by_scene(scene_num);
                tabs_->setCurrentWidget(char_table_);
            });
            menu.exec(scene_table_->viewport()->mapToGlobal(pos));
        });

        // Right-click on character table
        connect(char_table_, &QWidget::customContextMenuRequested, this,
                [this](const QPoint& pos) {
            using LC2 = screenplay::config::LanguageConfig;
            auto tr2 = [](const char* k){ return QString::fromUtf8(LC2::tr(k)); };
            auto* item = char_table_->itemAt(pos);
            if (!item) return;
            int row = char_table_->row(item);
            auto* name_item = char_table_->item(row, 0);
            if (!name_item) return;
            size_t  block_idx  = (size_t)name_item->data(Qt::UserRole).toULongLong();
            QString char_name  = name_item->text();

            QMenu menu(this);
            auto* act_goto   = menu.addAction(tr2("db_ctx_goto_first"));
            auto* act_scenes = menu.addAction(tr2("db_ctx_scenes_char"));
            auto* act_dial   = menu.addAction(tr2("db_ctx_view_dial"));
            connect(act_goto,   &QAction::triggered, this, [this, block_idx]{ emit navigate_to_block(block_idx); });
            connect(act_scenes, &QAction::triggered, this, [this, char_name]{
                filter_by_character(char_name);
                tabs_->setCurrentWidget(scene_table_);
            });
            connect(act_dial, &QAction::triggered, this, [this, char_name]{
                filter_by_character(char_name);
                tabs_->setCurrentWidget(dial_table_);
            });
            menu.exec(char_table_->viewport()->mapToGlobal(pos));
        });
    }

    void refresh(const screenplay::Script& script,
                 const screenplay::layout::PageList& pages)
    {
        index_ = screenplay::database::ScriptIndexBuilder::build(script, pages);
        rebuild_filter_combos();
        populate_all();
        apply_filter();
    }

signals:
    void navigate_to_block(size_t block_idx);

private:
    // Populate filter combo boxes from the current index
    void rebuild_filter_combos() {
        using LC = screenplay::config::LanguageConfig;
        auto tr = [](const char* k){ return QString::fromUtf8(LC::tr(k)); };

        // Preserve current selections
        int old_scene = scene_combo_->currentData().toInt();
        QString old_char = char_combo_->currentData().toString();

        scene_combo_->blockSignals(true);
        scene_combo_->clear();
        scene_combo_->addItem(tr("db_filter_all_scenes"), QVariant(0));
        for (const auto& sc : index_.scenes)
            scene_combo_->addItem(
                QString("%1: %2").arg(sc.scene_number)
                    .arg(QString::fromStdString(sc.heading).left(40)),
                QVariant(sc.scene_number));
        // Try to restore previous selection
        for (int i = 0; i < scene_combo_->count(); ++i)
            if (scene_combo_->itemData(i).toInt() == old_scene) {
                scene_combo_->setCurrentIndex(i); break;
            }
        scene_combo_->blockSignals(false);

        char_combo_->blockSignals(true);
        char_combo_->clear();
        char_combo_->addItem(tr("db_filter_all_chars"), QVariant(QString()));
        for (const auto& cr : index_.characters)
            char_combo_->addItem(
                QString::fromStdString(cr.name),
                QVariant(QString::fromStdString(cr.name)));
        // Try to restore previous selection
        for (int i = 0; i < char_combo_->count(); ++i)
            if (char_combo_->itemData(i).toString() == old_char) {
                char_combo_->setCurrentIndex(i); break;
            }
        char_combo_->blockSignals(false);
    }

    // Fill all three tables from index_
    void populate_all() {
        populate_scenes();
        populate_chars();
        populate_dialogue();
    }

    void populate_scenes() {
        scene_table_->setSortingEnabled(false);
        scene_table_->setRowCount(0);
        scene_table_->setRowCount((int)index_.scenes.size());

        static const QColor kColorInt(0xE8, 0xF0, 0xFF);
        static const QColor kColorExt(0xE8, 0xFF, 0xE8);
        static const QColor kColorIE (0xFF, 0xFB, 0xE8);

        for (int r = 0; r < (int)index_.scenes.size(); ++r) {
            const auto& sc = index_.scenes[(size_t)r];

            QColor bg;
            if      (sc.prefix == "INT.") bg = kColorInt;
            else if (sc.prefix == "EXT.") bg = kColorExt;
            else if (sc.prefix == "I/E.") bg = kColorIE;

            // Build characters cell (max 3 then "+ N more")
            QString chars_text;
            int nc = (int)sc.characters.size();
            for (int j = 0; j < std::min(nc, 3); ++j) {
                if (j > 0) chars_text += ", ";
                chars_text += QString::fromStdString(sc.characters[(size_t)j]);
            }
            if (nc > 3) chars_text += QString(" +%1 more").arg(nc - 3);

            auto mki = [&](const QString& text, QVariant role = {}) -> QTableWidgetItem* {
                auto* item = new QTableWidgetItem(text);
                if (bg.isValid()) item->setBackground(bg);
                item->setForeground(QColor(0x1C, 0x1B, 0x1F));
                if (role.isValid()) item->setData(Qt::UserRole, role);
                return item;
            };
            auto mkn = [&](int n, QVariant role = {}) -> QTableWidgetItem* {
                auto* item = new QTableWidgetItem;
                item->setData(Qt::DisplayRole, n);
                if (bg.isValid()) item->setBackground(bg);
                item->setForeground(QColor(0x1C, 0x1B, 0x1F));
                if (role.isValid()) item->setData(Qt::UserRole, role);
                return item;
            };

            // Col 0: #  — stores block_idx in UserRole, scene_number in UserRole+1
            auto* num_item = mkn(sc.scene_number, QVariant((quint64)sc.block_idx));
            num_item->setData(Qt::UserRole + 1, sc.scene_number);
            scene_table_->setItem(r, 0, num_item);
            scene_table_->setItem(r, 1, mki(QString::fromStdString(sc.prefix)));
            scene_table_->setItem(r, 2, mki(QString::fromStdString(sc.location)));
            scene_table_->setItem(r, 3, mki(QString::fromStdString(sc.time_of_day)));
            scene_table_->setItem(r, 4, mki(chars_text));
            scene_table_->setItem(r, 5, mkn(sc.line_count));
            scene_table_->setItem(r, 6, mkn(sc.page_estimate));
        }
        scene_table_->setSortingEnabled(true);
    }

    void populate_chars() {
        char_table_->setSortingEnabled(false);
        char_table_->setRowCount(0);
        char_table_->setRowCount((int)index_.characters.size());

        for (int r = 0; r < (int)index_.characters.size(); ++r) {
            const auto& cr = index_.characters[(size_t)r];

            // Scene numbers as comma-separated list
            QString scn_text;
            for (int i = 0; i < (int)cr.scene_numbers.size(); ++i) {
                if (i > 0) scn_text += ", ";
                scn_text += QString::number(cr.scene_numbers[(size_t)i]);
            }

            // First appearance page (from first entry in first_appearance)
            int first_page = 0;
            // We stored the block_idx in first_appearance — find the page from scenes
            if (!cr.first_appearance.empty()) {
                size_t bi = cr.first_appearance[0];
                const auto* sc = index_.find_scene_by_block(bi);
                if (sc) first_page = sc->page_estimate;
            }

            // Navigation block_idx: first appearance block or 0
            size_t nav_bi = cr.first_appearance.empty() ? 0 : cr.first_appearance[0];

            auto* name_item = new QTableWidgetItem(QString::fromStdString(cr.name));
            name_item->setData(Qt::UserRole, QVariant((quint64)nav_bi));

            auto mkn2 = [](int n) {
                auto* item = new QTableWidgetItem;
                item->setData(Qt::DisplayRole, n);
                return item;
            };

            char_table_->setItem(r, 0, name_item);
            char_table_->setItem(r, 1, mkn2(cr.total_dialogue_count));
            char_table_->setItem(r, 2, mkn2(cr.total_scene_count));
            char_table_->setItem(r, 3, new QTableWidgetItem(scn_text));
            char_table_->setItem(r, 4, mkn2(first_page));
        }
        char_table_->setSortingEnabled(true);
    }

    void populate_dialogue() {
        dial_table_->setRowCount(0);

        int total = 0;
        for (const auto& sc : index_.scenes) total += (int)sc.dialogue.size();
        dial_table_->setRowCount(total);

        int r = 0;
        for (const auto& sc : index_.scenes) {
            for (const auto& dl : sc.dialogue) {
                // Truncate dialogue text to 60 chars
                QString dial_text = QString::fromStdString(dl.text);
                if (dial_text.length() > 60)
                    dial_text = dial_text.left(57) + "…";

                auto* sn_item = new QTableWidgetItem;
                sn_item->setData(Qt::DisplayRole, sc.scene_number);
                sn_item->setData(Qt::UserRole, QVariant((quint64)dl.dialogue_block_idx));

                dial_table_->setItem(r, 0, sn_item);
                dial_table_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(dl.character)));
                dial_table_->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(dl.parenthetical)));
                dial_table_->setItem(r, 3, new QTableWidgetItem(dial_text));
                ++r;
            }
        }
    }

    void apply_filter() {
        const QString text        = filter_edit_->text();
        const int     scene_sel   = scene_combo_->currentIndex();
        const int     scene_num   = scene_combo_->currentData().toInt();   // 0 = all
        const QString char_name   = char_combo_->currentData().toString(); // "" = all

        auto text_match = [&](QTableWidget* t, int row) -> bool {
            if (text.isEmpty()) return true;
            for (int c = 0; c < t->columnCount(); ++c) {
                auto* item = t->item(row, c);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) return true;
            }
            return false;
        };

        // ── Scenes table ────────────────────────────────────────────────────
        for (int r = 0; r < scene_table_->rowCount(); ++r) {
            bool show = text_match(scene_table_, r);
            // Filter by character
            if (show && !char_name.isEmpty()) {
                auto* chars_item = scene_table_->item(r, 4);
                if (!chars_item || !chars_item->text().contains(char_name, Qt::CaseInsensitive))
                    show = false;
            }
            scene_table_->setRowHidden(r, !show);
        }

        // ── Characters table ────────────────────────────────────────────────
        for (int r = 0; r < char_table_->rowCount(); ++r) {
            bool show = text_match(char_table_, r);
            // Filter by scene
            if (show && scene_sel > 0) {
                auto* scns_item = char_table_->item(r, 3); // SCENES column
                if (!scns_item || !scns_item->text().contains(
                        QString::number(scene_num), Qt::CaseInsensitive))
                    show = false;
            }
            char_table_->setRowHidden(r, !show);
        }

        // ── Dialogue table ──────────────────────────────────────────────────
        for (int r = 0; r < dial_table_->rowCount(); ++r) {
            bool show = text_match(dial_table_, r);
            // Filter by scene number
            if (show && scene_sel > 0) {
                auto* sn_item = dial_table_->item(r, 0);
                if (!sn_item || sn_item->data(Qt::DisplayRole).toInt() != scene_num)
                    show = false;
            }
            // Filter by character
            if (show && !char_name.isEmpty()) {
                auto* ch_item = dial_table_->item(r, 1);
                if (!ch_item || !ch_item->text().contains(char_name, Qt::CaseInsensitive))
                    show = false;
            }
            dial_table_->setRowHidden(r, !show);
        }

        update_stats_footer();
    }

    void update_stats_footer() {
        int vis_scenes = 0;
        for (int r = 0; r < scene_table_->rowCount(); ++r)
            if (!scene_table_->isRowHidden(r)) ++vis_scenes;
        int n_chars = (int)index_.characters.size();
        int n_dial  = 0;
        for (const auto& sc : index_.scenes) n_dial += (int)sc.dialogue.size();
        int n_pages = index_.scenes.empty() ? 0 : index_.scenes.back().page_estimate;
        stats_lbl_->setText(
            QString("%1 scenes · %2 characters · %3 lines · ~%4 min")
                .arg(vis_scenes).arg(n_chars).arg(n_dial).arg(n_pages));
    }

    void filter_by_scene(int scene_number) {
        for (int i = 0; i < scene_combo_->count(); ++i)
            if (scene_combo_->itemData(i).toInt() == scene_number) {
                scene_combo_->setCurrentIndex(i);
                return;
            }
    }

    void filter_by_character(const QString& name) {
        for (int i = 0; i < char_combo_->count(); ++i)
            if (char_combo_->itemData(i).toString().compare(name, Qt::CaseInsensitive) == 0) {
                char_combo_->setCurrentIndex(i);
                return;
            }
    }

    screenplay::database::ScriptIndex index_;

    QLineEdit*    filter_edit_  = nullptr;
    QComboBox*    scene_combo_  = nullptr;
    QComboBox*    char_combo_   = nullptr;
    QTabWidget*   tabs_         = nullptr;
    QTableWidget* scene_table_  = nullptr;
    QTableWidget* char_table_   = nullptr;
    QTableWidget* dial_table_   = nullptr;
    QLabel*       stats_lbl_    = nullptr;
};

using ScriptDatabase = ScriptDatabasePanel;

// ─────────────────────────────────────────────────────────────────────────────
// Main Window
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle("Screenplay Editor " APP_VERSION);
        resize(1320, 920);

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

        // Build the toolbar rows (title bar + menus + icon toolbar)
        // then set canvas as central widget below the toolbar area.
        setup_toolbar();
        setCentralWidget(canvas_);
        setup_statusbar();
        setup_scene_dock();
        setup_stats_dock();
        setup_database_dock();
        setup_shortcuts();

        auto& cfg = screenplay::config::AppConfig::instance();
        if (!cfg.load_geometry().isEmpty()) restoreGeometry(cfg.load_geometry());
        if (!cfg.load_state().isEmpty())    restoreState(cfg.load_state());

        connect(canvas_, &ScreenplayCanvas::script_changed,
                this,    &MainWindow::on_changed);
        connect(canvas_, &ScreenplayCanvas::autosave_requested,
                this,    &MainWindow::on_autosave);
        connect(canvas_, &ScreenplayCanvas::zoom_changed,
                this,    [this](float){ update_zoom(); });

        // Deferred post-init messages (shown after event loop starts)
        QTimer::singleShot(500, this, [this]{
            if (g_courier_prime_missing)
                statusBar()->showMessage(
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
    }

private slots:
    void on_changed() {
        refresh_scenes();
        refresh_stats();
        refresh_database();
        update_title();
        update_status();
    }

    void on_new() {
        if (dirty_confirm()) return;
        canvas_->ctrl() = screenplay::editor::EditorController{};
        current_path_.clear();
        doc_custom_name_ = "Untitled";
        canvas_->request_relayout();
        emit canvas_->script_changed();
    }

    void on_open() {
        auto path = QFileDialog::getOpenFileName(
            this, "Open screenplay",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            "Todos (*.spl *.fountain *.fdx);;JSON (*.spl);;Fountain (*.fountain);;FDX (*.fdx)");
        if (path.isEmpty()) return;
        try {
            screenplay::Script s;
            QString ext = QFileInfo(path).suffix().toLower();
            if      (ext=="fountain") s=screenplay::io::FountainImporter::read(path.toStdString());
            else if (ext=="fdx")      s=screenplay::io::FDXImporter::read(path.toStdString());
            else                      s=screenplay::io::JsonDeserializer::read(path.toStdString());
            canvas_->ctrl().load_script(std::move(s));
            current_path_ = path;
            canvas_->request_relayout();
            emit canvas_->script_changed();

            // TASK 3d: hint if title page has content but is not enabled
            const auto& tp = canvas_->ctrl().state().script.title_page;
            if (!tp.enabled && (!tp.title.empty() || !tp.authors.empty()))
                statusBar()->showMessage(
                    "Title page configured but not shown \xe2\x80\x94"
                    " enable in Document > Title Page\xe2\x80\xa6",
                    8000);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Open error", e.what());
        }
    }

    void on_save()    { current_path_.isEmpty() ? on_save_as() : do_save(current_path_); }
    void on_save_as() {
        auto p = QFileDialog::getSaveFileName(
            this, "Save screenplay", {}, "JSON Script (*.spl)");
        if (!p.isEmpty()) do_save(p);
    }

    void on_autosave() {
        if (!canvas_->ctrl().state().dirty) return;
        QString p = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    + "/autosave.spl";
        QDir().mkpath(QFileInfo(p).absolutePath());
        try { screenplay::io::JsonSerializer::write(
                  canvas_->ctrl().state().script, p.toStdString());
              statusBar()->showMessage("Autosaved.", 2000);
        } catch (...) {}
    }

    void on_export_fountain() {
        auto p = QFileDialog::getSaveFileName(
            this, "Export Fountain", {}, "Fountain (*.fountain)");
        if (p.isEmpty()) return;
        try { screenplay::io::FountainExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              statusBar()->showMessage("Exported.", 3000);
        } catch (const std::exception& e) { QMessageBox::critical(this,"Error",e.what()); }
    }

    void on_export_fdx() {
        auto p = QFileDialog::getSaveFileName(
            this, "Export FDX", {}, "Final Draft (*.fdx)");
        if (p.isEmpty()) return;
        try { screenplay::io::FDXExporter::write(
                  canvas_->ctrl().state().script, p.toStdString());
              statusBar()->showMessage("Exported.", 3000);
        } catch (const std::exception& e) { QMessageBox::critical(this,"Error",e.what()); }
    }

    void on_zoom_in()    { canvas_->zoom_in();    update_zoom(); }
    void on_zoom_out()   { canvas_->zoom_out();   update_zoom(); }
    void on_zoom_reset() { canvas_->zoom_reset(); update_zoom(); }

    void on_print() {
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dlg(&printer, this);
        if (dlg.exec() != QDialog::Accepted) return;
        QTextDocument doc;
        QString text;
        for (const auto& b : canvas_->ctrl().state().script.blocks) {
            if (!text.isEmpty()) text += "\n";
            text += QString::fromStdString(b.text);
        }
        doc.setPlainText(text);
        doc.print(&printer);
    }

    void on_find_replace() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Find and Replace");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        auto* form   = new QFormLayout(dlg);
        auto* find_e = new QLineEdit(dlg);
        auto* repl_e = new QLineEdit(dlg);
        form->addRow("Find:",    find_e);
        form->addRow("Replace:", repl_e);
        auto* btns = new QHBoxLayout;
        auto* btn_repl    = new QPushButton("Replace",     dlg);
        auto* btn_repl_all= new QPushButton("Replace All", dlg);
        auto* btn_close   = new QPushButton("Close",       dlg);
        btns->addWidget(btn_repl); btns->addWidget(btn_repl_all); btns->addWidget(btn_close);
        form->addRow(btns);
        connect(btn_close, &QPushButton::clicked, dlg, &QDialog::close);
        connect(btn_repl_all, &QPushButton::clicked, this, [this, find_e, repl_e, dlg]{
            const QString needle = find_e->text();
            const QString rep    = repl_e->text();
            if (needle.isEmpty()) return;
            bool changed = false;
            for (auto& b : canvas_->ctrl().script_mut().blocks) {
                QString t = QString::fromStdString(b.text);
                if (t.contains(needle)) {
                    b.text = t.replace(needle, rep).toStdString();
                    changed = true;
                }
            }
            if (changed) { canvas_->request_relayout(); emit canvas_->script_changed(); }
            dlg->close();
        });
        connect(btn_repl, &QPushButton::clicked, this, [this, find_e, repl_e]{
            const QString needle = find_e->text();
            const QString rep    = repl_e->text();
            if (needle.isEmpty()) return;
            for (auto& b : canvas_->ctrl().script_mut().blocks) {
                QString t = QString::fromStdString(b.text);
                int idx = t.indexOf(needle);
                if (idx != -1) {
                    b.text = t.replace(idx, needle.size(), rep).toStdString();
                    canvas_->request_relayout(); emit canvas_->script_changed();
                    return;
                }
            }
        });
        dlg->show();
    }

    void on_goto_page() {
        bool ok = false;
        int page = QInputDialog::getInt(this, "Go to page", "Page:",
                                        1, 1, (int)canvas_->pages().size(), 1, &ok);
        if (ok) canvas_->scroll_to_page(page);
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
                    canvas_->ctrl().set_cursor_pos({ bi, 0 });
                    canvas_->scroll_to_block(bi);
                    canvas_->update();
                    emit canvas_->script_changed();
                    return;
                }
            }
        }
    }
    void on_fullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }
    void on_stats()      { stats_dock_->setVisible(!stats_dock_->isVisible()); }
    void on_database()   { db_dock_->setVisible(!db_dock_->isVisible()); if (db_dock_->isVisible()) refresh_database(); }

    void on_script_language() {
        QSettings qs;
        QStringList curr = qs.value("spell_languages", QStringList{"en-US"}).toStringList();
        ScriptLanguageDialog dlg(this, curr);
        if (dlg.exec() != QDialog::Accepted) return;
        QStringList langs = dlg.selected_langs();
        if (langs.isEmpty()) {
            QMessageBox::warning(this, "Language",
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
        statusBar()->showMessage("Spell check language updated.", 3000);
    }
    void on_title_page() {
        canvas_->edit_title_page(this);
        update_capa_badge();
    }

    void on_shortcuts_dialog() {
        // Non-modal, resizable dialog with searchable shortcut table
        auto* dlg = new QDialog(this, Qt::Window);
        dlg->setWindowTitle("Keyboard Shortcuts");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(620, 520);

        auto* lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(8);

        auto* filter = new QLineEdit;
        filter->setPlaceholderText("Filter shortcuts\xe2\x80\xa6");
        filter->setClearButtonEnabled(true);
        lay->addWidget(filter);

        auto* table = new QTableWidget;
        table->setColumnCount(3);
        table->setHorizontalHeaderLabels({"Shortcut", "Action", "Context"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);
        lay->addWidget(table);

        // Populate shortcut data
        struct SC { const char* key; const char* action; const char* ctx; };
        static const SC shortcuts[] = {
            // File
            {"Ctrl+N",          "New document",                "Global"},
            {"Ctrl+O",          "Open document",               "Global"},
            {"Ctrl+S",          "Save",                        "Global"},
            {"Ctrl+Shift+S",    "Save as",                     "Global"},
            {"Ctrl+P",          "Export PDF",                  "Global"},
            // Edit
            {"Ctrl+Z",          "Undo",                        "Global"},
            {"Ctrl+Shift+Z",    "Redo",                        "Global"},
            {"Ctrl+Y",          "Redo (alternative)",          "Editor"},
            {"Ctrl+A",          "Select all",                  "Editor"},
            {"Ctrl+C",          "Copy",                        "Editor"},
            {"Ctrl+X",          "Cut",                         "Editor"},
            {"Ctrl+V",          "Paste",                       "Editor"},
            {"Ctrl+F",          "Find",                        "Editor"},
            {"Ctrl+H",          "Replace",                     "Global"},
            // Navigation
            {"Ctrl+Home",       "Beginning of document",       "Editor"},
            {"Ctrl+End",        "End of document",             "Editor"},
            {"Ctrl+Left",       "Previous word",               "Editor"},
            {"Ctrl+Right",      "Next word",                   "Editor"},
            {"Ctrl+Shift+Left", "Select previous word",        "Editor"},
            {"Ctrl+Shift+Right","Select next word",            "Editor"},
            {"Shift+Left",      "Extend selection left",       "Editor"},
            {"Shift+Right",     "Extend selection right",      "Editor"},
            {"Shift+Up",        "Select block above",          "Editor"},
            {"Shift+Down",      "Select block below",          "Editor"},
            {"Shift+Home",      "Select to line start",        "Editor"},
            {"Shift+End",       "Select to line end",          "Editor"},
            {"Home",            "Line start",                  "Editor"},
            {"End",             "Line end",                    "Editor"},
            {"Up",              "Block above",                 "Editor"},
            {"Down",            "Block below",                 "Editor"},
            {"Left",            "Previous character",          "Editor"},
            {"Right",           "Next character",              "Editor"},
            {"Ctrl+G",          "Go to scene",                 "Global"},
            {"Ctrl+Shift+G",    "Go to page",                  "Global"},
            // Words
            {"Ctrl+Backspace",  "Delete previous word",        "Editor"},
            {"Ctrl+Delete",     "Delete next word",            "Editor"},
            // Blocks
            {"Enter",           "New block",                   "Editor"},
            {"Tab",             "Accept suggestion / Next type","Editor"},
            {"Shift+Tab",       "Previous block type",         "Editor"},
            {"Backspace",       "Delete char / Merge block",   "Editor"},
            {"Delete",          "Delete forward / Merge block","Editor"},
            {"Ctrl+1",          "Scene heading",               "Editor"},
            {"Ctrl+2",          "Action",                      "Editor"},
            {"Ctrl+3",          "Character",                   "Editor"},
            {"Ctrl+4",          "Parenthetical",               "Editor"},
            {"Ctrl+5",          "Dialogue",                    "Editor"},
            {"Ctrl+6",          "Transition",                  "Editor"},
            // Autocomplete
            {"Tab",             "Accept suggestion",           "Popup"},
            {"Enter",           "Accept suggestion",           "Popup"},
            {"Escape",          "Close popup",                 "Popup"},
            {"Up",              "Previous suggestion",         "Popup"},
            {"Down",            "Next suggestion",             "Popup"},
            {"Ctrl+1-6",        "Accept suggestion N",         "Popup"},
            // Zoom
            {"Ctrl++",          "Zoom in",                     "Global"},
            {"Ctrl+=",          "Zoom in (alternative)",       "Global"},
            {"Ctrl+-",          "Zoom out",                    "Global"},
            {"Ctrl+0",          "Zoom 1:1",                    "Global"},
            {"Ctrl+Scroll",     "Zoom with mouse",             "Editor"},
            // View
            {"F11",             "Full screen",                 "Global"},
            {"Ctrl+Shift+I",    "Statistics",                  "Global"},
            {"Ctrl+Shift+B",    "Script Database",             "Global"},
            // Format
            {"Ctrl+B",          "Bold",                        "Editor"},
            {"Ctrl+I",          "Italic",                      "Editor"},
            {"Ctrl+U",          "Underline",                   "Editor"},
            {"Ctrl+D",          "Dual dialogue",               "Editor"},
            // Search
            {"Enter",           "Next result",                 "Search"},
            {"Shift+Enter",     "Previous result",             "Search"},
            {"Escape",          "Close search",                "Search"},
        };

        const int N = (int)std::size(shortcuts);
        table->setRowCount(N);
        for (int i = 0; i < N; ++i) {
            table->setItem(i, 0, new QTableWidgetItem(shortcuts[i].key));
            table->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(shortcuts[i].action)));
            table->setItem(i, 2, new QTableWidgetItem(shortcuts[i].ctx));
        }
        table->resizeColumnsToContents();

        // Filter logic
        connect(filter, &QLineEdit::textChanged, table, [table](const QString& text){
            for (int r = 0; r < table->rowCount(); ++r) {
                bool match = text.isEmpty();
                if (!match) {
                    for (int c = 0; c < 3 && !match; ++c) {
                        auto* item = table->item(r, c);
                        if (item && item->text().contains(text, Qt::CaseInsensitive))
                            match = true;
                    }
                }
                table->setRowHidden(r, !match);
            }
        });

        dlg->show();
    }

private:
    // Tear down the current header widget and rebuild it with the active language.
    // Called after LanguageConfig::set() so all menu strings are re-evaluated.
    void rebuild_menus() {
        QWidget* old_header = menuWidget();
        setup_toolbar();                    // creates new header, calls setMenuWidget()
        if (old_header) old_header->deleteLater();

        // Sync dock-visibility checkmarks to actual dock state
        if (act_view_scenes_   && scene_dock_)
            act_view_scenes_->setChecked(scene_dock_->isVisible());
        if (act_view_stats_    && stats_dock_)
            act_view_stats_->setChecked(stats_dock_->isVisible());
        if (act_view_database_ && db_dock_)
            act_view_database_->setChecked(db_dock_->isVisible());

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

    void setup_toolbar() {
        // ═══════════════════════════════════════════════════════════════════
        // Header widget: Row 1 (logo + doc name) ABOVE Row 2 (menu bar).
        // Using setMenuWidget() so QMainWindow treats this composite as
        // the menu area, guaranteeing Row 1 is visually on top.
        // ═══════════════════════════════════════════════════════════════════
        auto* header = new QWidget(this);
        auto* header_vbox = new QVBoxLayout(header);
        header_vbox->setContentsMargins(0, 0, 0, 0);
        header_vbox->setSpacing(0);

        // ── ROW 1 — Title bar area: logo + editable document name ────────
        auto* row1 = new QWidget;
        row1->setFixedHeight(40);
        row1->setStyleSheet(
            "background:#2D2C31; border-bottom:1px solid #49454F;");
        auto* row1_lay = new QHBoxLayout(row1);
        row1_lay->setContentsMargins(8, 4, 8, 4);
        row1_lay->setSpacing(6);

        // App logo (32x32 placeholder if resource missing)
        auto* logo_lbl = new QLabel;
        QPixmap logo_pm(":/icons/logo.png");
        if (logo_pm.isNull()) {
            logo_pm = QPixmap(32, 32);
            logo_pm.fill(QColor(0xD0, 0xBC, 0xFF));  // MD3 Primary as placeholder
        }
        logo_lbl->setPixmap(logo_pm.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo_lbl->setFixedSize(32, 32);
        row1_lay->addWidget(logo_lbl);

        // Editable document name
        doc_name_edit_ = new QLineEdit("Untitled");
        doc_name_edit_->setFrame(false);
        doc_name_edit_->setStyleSheet(
            "QLineEdit { background:transparent; color:#E6E1E5;"
            "            font-size:14px; font-weight:bold; border:none;"
            "            font-family:'Segoe UI'; padding:2px 4px; }");
        doc_name_edit_->setMinimumWidth(160);
        doc_name_edit_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        row1_lay->addWidget(doc_name_edit_);
        connect(doc_name_edit_, &QLineEdit::editingFinished, this, [this]{
            QString new_name = doc_name_edit_->text().trimmed();
            if (new_name.isEmpty()) {
                update_doc_name_display();
                return;
            }
            if (current_path_.isEmpty()) {
                // No file yet — just update the in-memory display name
                doc_custom_name_ = new_name;
                update_title();
                return;
            }
            // Rename file on disk
            QFileInfo fi(current_path_);
            QString new_path = fi.absolutePath() + "/" + new_name;
            if (!new_name.contains('.'))
                new_path += "." + fi.suffix();
            if (QFile::rename(current_path_, new_path)) {
                current_path_ = new_path;
            } else {
                update_doc_name_display();  // revert on failure
            }
            update_title();
        });

        // Right stretch
        row1_lay->addStretch();

        header_vbox->addWidget(row1);   // Row 1 added FIRST → on top

        // ── ROW 2 — Menu bar ────────────────────────────────────────────
        auto* mb = new QMenuBar(header); // parented to header, not QMainWindow
        header_vbox->addWidget(mb);      // Row 2 added SECOND → below Row 1

        // File
        auto* mFile = mb->addMenu("&File");
        mFile->addAction("New",         this, &MainWindow::on_new)->setShortcut(QKeySequence::New);
        mFile->addAction("Open\xe2\x80\xa6",  this, &MainWindow::on_open)->setShortcut(QKeySequence::Open);
        mFile->addAction("Save",        this, &MainWindow::on_save)->setShortcut(QKeySequence::Save);
        mFile->addAction("Save As\xe2\x80\xa6", this, &MainWindow::on_save_as)->setShortcut(QKeySequence("Ctrl+Shift+S"));
        mFile->addSeparator();
        mFile->addAction("Export PDF\xe2\x80\xa6",      this, &MainWindow::on_print);
        mFile->addAction("Export Fountain\xe2\x80\xa6", this, &MainWindow::on_export_fountain);
        mFile->addAction("Export FDX\xe2\x80\xa6",      this, &MainWindow::on_export_fdx);
        // Edit
        auto* mEdit = mb->addMenu("&Edit");
        mEdit->addAction("Undo", canvas_, [this]{ canvas_->ctrl().handle_key({screenplay::editor::Key::Undo}); canvas_->request_relayout(); emit canvas_->script_changed(); })->setShortcut(QKeySequence::Undo);
        mEdit->addAction("Redo", canvas_, [this]{ canvas_->ctrl().handle_key({screenplay::editor::Key::Redo}); canvas_->request_relayout(); emit canvas_->script_changed(); })->setShortcut(QKeySequence("Ctrl+Shift+Z"));
        mEdit->addSeparator();
        mEdit->addAction("Cut", canvas_, [this]{
            if (canvas_->ctrl().state().has_selection) {
                QApplication::clipboard()->setText(QString::fromStdString(canvas_->ctrl().copy_selection()));
                canvas_->ctrl().cut_selection();
                canvas_->request_relayout(); emit canvas_->script_changed();
            }
        })->setShortcut(QKeySequence::Cut);
        mEdit->addAction("Copy", canvas_, [this]{
            if (canvas_->ctrl().state().has_selection)
                QApplication::clipboard()->setText(QString::fromStdString(canvas_->ctrl().copy_selection()));
        })->setShortcut(QKeySequence::Copy);
        mEdit->addAction("Paste", canvas_, [this]{
            std::string txt = QApplication::clipboard()->text().toStdString();
            if (!txt.empty()) { canvas_->ctrl().paste(txt); canvas_->request_relayout(); emit canvas_->script_changed(); }
        })->setShortcut(QKeySequence::Paste);
        mEdit->addSeparator();
        mEdit->addAction("Find\xe2\x80\xa6",    this, [this]{ canvas_->show_search(); })->setShortcut(QKeySequence::Find);
        mEdit->addAction("Replace\xe2\x80\xa6", this, &MainWindow::on_find_replace)->setShortcut(QKeySequence("Ctrl+H"));
        mEdit->addSeparator();
        {
            // Spell check status indicator (read-only; check runs automatically)
            spell_status_act_ = mEdit->addAction("Spell Check");
            spell_status_act_->setEnabled(false);
            // Text updated after canvas is ready (post-init timer)
        }

        // Document
        auto* mDoc = mb->addMenu("&Document");
        mDoc->addAction("Edit Title Page\xe2\x80\xa6", this, &MainWindow::on_title_page);
        {
            capa_toggle_act_ = mDoc->addAction("Include Title Page");
            capa_toggle_act_->setCheckable(true);
            capa_toggle_act_->setChecked(
                canvas_->ctrl().state().script.title_page.enabled);
            connect(capa_toggle_act_, &QAction::triggered, this, [this](bool checked){
                auto script = canvas_->ctrl().state().script;
                script.title_page.enabled = checked;
                // If toggling ON and no content yet, open the editor immediately
                if (checked && script.title_page.title.empty()
                    && script.title_page.authors.empty()) {
                    canvas_->ctrl().load_script(std::move(script));
                    canvas_->request_relayout();
                    emit canvas_->script_changed();
                    on_title_page();
                } else {
                    canvas_->ctrl().load_script(std::move(script));
                    canvas_->request_relayout();
                    emit canvas_->script_changed();
                }
                update_capa_badge();
            });
        }
        mDoc->addSeparator();
        {
            using SNM = ScreenplayCanvas::SceneNumMode;
            auto* mSN  = mDoc->addMenu("Scene Numbers");
            auto* grp  = new QActionGroup(mSN);
            grp->setExclusive(true);
            struct SNOpt { const char* label; SNM mode; };
            static const SNOpt opts[] = {
                {"None",        SNM::None},
                {"Left only",   SNM::Left},
                {"Right only",  SNM::Right},
                {"Both",        SNM::Both},
            };
            for (const auto& opt : opts) {
                auto* a = mSN->addAction(opt.label);
                a->setCheckable(true);
                a->setChecked(opt.mode == SNM::Both);
                grp->addAction(a);
                SNM m = opt.mode;
                connect(a, &QAction::triggered, this, [this, m]{
                    canvas_->set_scene_num_mode(m);
                    canvas_->update();
                });
            }
        }
        mDoc->addSeparator();
        mDoc->addAction("Go to Scene\xe2\x80\xa6", this, &MainWindow::on_goto_scene)->setShortcut(QKeySequence("Ctrl+G"));
        mDoc->addAction("Go to Page\xe2\x80\xa6",  this, &MainWindow::on_goto_page)->setShortcut(QKeySequence("Ctrl+Shift+G"));

        // View
        auto* mView = mb->addMenu("&View");
        mView->addAction("Zoom In",  this, &MainWindow::on_zoom_in)->setShortcut(QKeySequence("Ctrl++"));
        mView->addAction("Zoom Out", this, &MainWindow::on_zoom_out)->setShortcut(QKeySequence("Ctrl+-"));
        mView->addAction("1:1",      this, &MainWindow::on_zoom_reset)->setShortcut(QKeySequence("Ctrl+0"));
        mView->addSeparator();
        {
            act_view_scenes_ = mView->addAction("Scenes");
            act_view_scenes_->setCheckable(true);
            act_view_scenes_->setChecked(false);
            connect(act_view_scenes_, &QAction::triggered, this, [this](bool checked){
                scene_dock_->setVisible(checked);
            });
        }
        {
            act_view_stats_ = mView->addAction("Statistics");
            act_view_stats_->setCheckable(true);
            act_view_stats_->setChecked(false);
            act_view_stats_->setShortcut(QKeySequence("Ctrl+Shift+I"));
            connect(act_view_stats_, &QAction::triggered, this, [this](bool checked){
                stats_dock_->setVisible(checked);
                if (checked) refresh_stats();
            });
        }
        {
            act_view_database_ = mView->addAction("Script Database");
            act_view_database_->setCheckable(true);
            act_view_database_->setChecked(false);
            act_view_database_->setShortcut(QKeySequence("Ctrl+Shift+B"));
            connect(act_view_database_, &QAction::triggered, this, [this](bool checked){
                db_dock_->setVisible(checked);
                if (checked) refresh_database();
            });
        }
        mView->addSeparator();
        mView->addAction("Full Screen", this, &MainWindow::on_fullscreen)->setShortcut(Qt::Key_F11);

        // Format
        using BT = screenplay::BlockType;
        auto* mFmt = mb->addMenu("F&ormat");
        struct FDef { const char* name; BT t; const char* sc; };
        static const FDef fdefs[] = {
            {"Scene Heading", BT::SceneHeading,  "Ctrl+1"},
            {"Action",        BT::Action,         "Ctrl+2"},
            {"Character",     BT::Character,      "Ctrl+3"},
            {"Parenthetical", BT::Parenthetical,  "Ctrl+4"},
            {"Dialogue",      BT::Dialogue,       "Ctrl+5"},
            {"Transition",    BT::Transition,     "Ctrl+6"},
        };
        for (const auto& fd : fdefs) {
            auto* a = mFmt->addAction(QString::fromUtf8(fd.name));
            auto   t = fd.t;
            a->setShortcut(QKeySequence(fd.sc));
            connect(a, &QAction::triggered, this, [this, t]{
                canvas_->ctrl().set_block_type(t);
                canvas_->request_relayout();
                emit canvas_->script_changed();
            });
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction("Dual Dialogue");
            a->setShortcut(QKeySequence("Ctrl+D"));
            connect(a, &QAction::triggered, this, [this]{
                canvas_->ctrl().activate_dual_dialogue();
                canvas_->request_relayout();
                emit canvas_->script_changed();
            });
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction("Bold");
            a->setShortcut(QKeySequence("Ctrl+B"));
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_bold(); });
        }
        {
            auto* a = mFmt->addAction("Italic");
            a->setShortcut(QKeySequence("Ctrl+I"));
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_italic(); });
        }
        {
            auto* a = mFmt->addAction("Underline");
            a->setShortcut(QKeySequence("Ctrl+U"));
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_underline(); });
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction("Bold Future Scene Headings");
            a->setCheckable(true);
            connect(a, &QAction::toggled, this, [this](bool checked){
                canvas_->set_bold_future_scenes(checked);
                canvas_->update();
            });
        }

        // Tools — Language submenu
        {
            using LC = screenplay::config::LanguageConfig;
            using AL = screenplay::config::AppLanguage;
            auto* mTools = mb->addMenu(QString::fromUtf8(LC::tr("menu_tools")));
            auto* mLang  = mTools->addMenu(QString::fromUtf8(LC::tr("menu_language")));
            auto* lang_en = mLang->addAction(QString::fromUtf8(LC::tr("lang_english")));
            auto* lang_pt = mLang->addAction(QString::fromUtf8(LC::tr("lang_portuguese")));
            lang_en->setCheckable(true);
            lang_pt->setCheckable(true);
            lang_en->setChecked(LC::current() == AL::English);
            lang_pt->setChecked(LC::current() == AL::Portuguese);
            connect(lang_en, &QAction::triggered, this, [this]{
                using AL = screenplay::config::AppLanguage;
                screenplay::config::LanguageConfig::set(AL::English);
                QSettings().setValue("language", (int)AL::English);
                canvas_->ctrl().reseed_autocomplete();
                rebuild_menus();
            });
            connect(lang_pt, &QAction::triggered, this, [this]{
                using AL = screenplay::config::AppLanguage;
                screenplay::config::LanguageConfig::set(AL::Portuguese);
                QSettings().setValue("language", (int)AL::Portuguese);
                canvas_->ctrl().reseed_autocomplete();
                rebuild_menus();
            });
            mTools->addSeparator();
            mTools->addAction("Script Language\xe2\x80\xa6", this, &MainWindow::on_script_language);
        }

        // Help
        auto* mHelp = mb->addMenu("&Help");
        mHelp->addAction("Keyboard Shortcuts", this, &MainWindow::on_shortcuts_dialog);
        mHelp->addSeparator();
        mHelp->addAction("About", this, [this]{
            QMessageBox::about(this, "About",
                "<b>Screenplay Editor</b><br>"
                "Version: " APP_VERSION_FULL "<br><br>"
                "WGA-standard screenplay editor.<br>"
                "Built with Qt6 + C++.");
        });

        // ═══════════════════════════════════════════════════════════════════
        // ROW 3 — Icon toolbar (flat, 28px, 10 placeholder slots)
        // All 3 rows live inside the header widget via setMenuWidget().
        // ═══════════════════════════════════════════════════════════════════
        auto* row3 = new QWidget;
        row3->setFixedHeight(28);
        row3->setStyleSheet(
            "background:#2D2C31; border-bottom:1px solid #49454F;");
        auto* row3_lay = new QHBoxLayout(row3);
        row3_lay->setContentsMargins(4, 0, 4, 0);
        row3_lay->setSpacing(2);

        // Slots 1-5 (left group)
        for (int i = 0; i < 5; ++i) {
            auto* btn = new QToolButton;
            btn->setText("");
            btn->setIcon(QIcon());
            btn->setFixedSize(24, 24);
            btn->setStyleSheet(
                "QToolButton { border:none; padding:2px; border-radius:4px; }"
                "QToolButton:hover { background:#4F378A; }");
            btn->setEnabled(false);
            btn->setToolTip("Reserved \xe2\x80\x94 under development");
            row3_lay->addWidget(btn);
        }

        // Spacer between slot 5 and 6
        row3_lay->addStretch();

        // Slots 6-10 (right group)
        for (int i = 0; i < 5; ++i) {
            auto* btn = new QToolButton;
            btn->setText("");
            btn->setIcon(QIcon());
            btn->setFixedSize(24, 24);
            btn->setStyleSheet(
                "QToolButton { border:none; padding:2px; border-radius:4px; }"
                "QToolButton:hover { background:#4F378A; }");
            btn->setEnabled(false);
            btn->setToolTip("Reserved \xe2\x80\x94 under development");
            row3_lay->addWidget(btn);
        }

        header_vbox->addWidget(row3);   // Row 3 added THIRD → below menu bar

        setMenuWidget(header);           // All 3 rows inside one widget
    }

    void setup_statusbar() {
        word_lbl_ = new QLabel("Words: 0");
        word_lbl_->setStyleSheet("color:#A8D5A2; font-size:11px;");
        statusBar()->addPermanentWidget(word_lbl_);

        runtime_lbl_ = new QLabel("~0min");
        runtime_lbl_->setStyleSheet("color:#CCC2DC; font-size:11px;");
        statusBar()->addPermanentWidget(runtime_lbl_);

        pg_lbl_ = new QLabel("Page: 0");
        pg_lbl_->setStyleSheet("color:#CCC2DC; font-size:11px;");
        statusBar()->addPermanentWidget(pg_lbl_);

        // Zoom controls on the right of the status bar
        auto* zm_out = new QToolButton;
        zm_out->setText("−");
        zm_out->setStyleSheet("color:#E6E1E5; font-size:12px;");
        connect(zm_out, &QToolButton::clicked, this, &MainWindow::on_zoom_out);
        statusBar()->addPermanentWidget(zm_out);

        zoom_lbl_ = new QLabel("100%");
        zoom_lbl_->setFixedWidth(44);
        zoom_lbl_->setAlignment(Qt::AlignCenter);
        zoom_lbl_->setStyleSheet("color:#E6E1E5; font-size:11px;");
        statusBar()->addPermanentWidget(zoom_lbl_);

        auto* zm_in = new QToolButton;
        zm_in->setText("+");
        zm_in->setStyleSheet("color:#E6E1E5; font-size:12px;");
        connect(zm_in, &QToolButton::clicked, this, &MainWindow::on_zoom_in);
        statusBar()->addPermanentWidget(zm_in);
    }

    void setup_scene_dock() {
        auto* dock_container = new QWidget;
        auto* dc_lay = new QVBoxLayout(dock_container);
        dc_lay->setContentsMargins(0, 0, 0, 0);
        dc_lay->setSpacing(0);

        scene_list_ = new QListWidget;
        scene_list_->setStyleSheet(
            "QListWidget { background:#2D2C31; border:none; color:#E6E1E5; font-size:11px; }"
            "QListWidget::item:selected { background:#4F378A; border-radius:6px; }");
        dc_lay->addWidget(scene_list_);

        scene_dock_ = new QDockWidget("Scenes", this);
        scene_dock_->setWidget(dock_container);

        scene_dock_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
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

    void setup_stats_dock() {
        stats_panel_ = new StatsPanel;
        stats_dock_  = new QDockWidget("Statistics", this);
        stats_dock_->setWidget(stats_panel_);
        stats_dock_->setAllowedAreas(Qt::RightDockWidgetArea|Qt::LeftDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, stats_dock_);
        stats_dock_->hide();
        connect(stats_dock_, &QDockWidget::visibilityChanged, this, [this](bool v){
            if (act_view_stats_) act_view_stats_->setChecked(v);
        });
    }

    void setup_database_dock() {
        db_panel_ = new ScriptDatabase;
        db_dock_  = new QDockWidget("Script Database", this);
        db_dock_->setWidget(db_panel_);
        db_dock_->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, db_dock_);
        db_dock_->hide();
        connect(db_dock_, &QDockWidget::visibilityChanged, this, [this](bool v){
            if (act_view_database_) act_view_database_->setChecked(v);
        });

        connect(db_panel_, &ScriptDatabase::navigate_to_block, this,
                [this](size_t block_idx) {
            canvas_->ctrl().set_cursor_pos({ block_idx, 0 });
            canvas_->scroll_to_block(block_idx);
            canvas_->update();
            emit canvas_->script_changed();
        });
    }

    void setup_shortcuts() {
        auto sc = [&](const char* seq, std::function<void()> fn){
            auto* s = new QShortcut(QKeySequence(seq), this);
            connect(s, &QShortcut::activated, this, fn);
        };
        // Ctrl+= is an alternate zoom-in (menu uses Ctrl++, a different key sequence)
        sc("Ctrl+=", [this]{ on_zoom_in(); });
        // Ctrl+P for print/export PDF — menu action has no shortcut set
        sc("Ctrl+P", [this]{ on_print(); });
        // All other shortcuts (Ctrl+S, F11, Ctrl+N, Ctrl+O, Ctrl+H, etc.)
        // are handled exclusively by their menu action setShortcut() calls.
        // Duplicate QShortcuts were removed to eliminate Qt ambiguous-shortcut
        // conflicts that prevented either handler from firing.
    }

    void do_save(const QString& path) {
        try {
            screenplay::io::JsonSerializer::write(
                canvas_->ctrl().state().script, path.toStdString());
            current_path_ = path;
            canvas_->ctrl().mark_clean();
            statusBar()->showMessage("Saved: " + QFileInfo(path).fileName(), 3000);
            update_title();
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Save error", e.what());
        }
    }

    bool dirty_confirm() {
        if (!canvas_->ctrl().state().dirty) return false;
        return QMessageBox::question(this, "Warning",
            "There are unsaved changes. Continue?") != QMessageBox::Yes;
    }

    void refresh_scenes() {
        scene_list_->clear();
        int n = 0;
        for (const auto& b : canvas_->ctrl().state().script.blocks)
            if (b.type == screenplay::BlockType::SceneHeading)
                scene_list_->addItem(
                    QString("%1. %2").arg(++n).arg(QString::fromStdString(b.text)));

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
        pg_lbl_->setText(QString("Page: %1").arg(pages));
        runtime_lbl_->setText(QString("~%1min").arg(pages));

        int words = 0;
        for (const auto& b : canvas_->ctrl().state().script.blocks) {
            bool in_word = false;
            for (unsigned char c : b.text) {
                if (std::isspace(c)) in_word = false;
                else if (!in_word) { ++words; in_word = true; }
            }
        }
        word_lbl_->setText(QString("Words: %1").arg(words));
    }

    void refresh_database() {
        if (!db_dock_ || !db_dock_->isVisible()) return;
        db_panel_->refresh(canvas_->ctrl().state().script, canvas_->pages());
    }

    void refresh_stats() {
        if (!stats_dock_->isVisible()) return;
        auto s = screenplay::stats::StatsEngine::compute(
            canvas_->ctrl().state().script,
            (int)canvas_->pages().size());
        stats_panel_->refresh(s);
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
        const auto   btype = st.script.blocks[st.cursor.block_idx].type;
        const QString lbl  = QString::fromUtf8(block_label(btype));
        if (blk_type_lbl_) blk_type_lbl_->setText(lbl);
    }

    void update_zoom() {
        zoom_lbl_->setText(
            QString("%1%").arg((int)(canvas_->zoom() * 100)));
    }

    void closeEvent(QCloseEvent* ev) override {
        if (dirty_confirm()) { ev->ignore(); return; }
        auto& cfg = screenplay::config::AppConfig::instance();
        cfg.save_geometry(saveGeometry());
        cfg.save_state(saveState());
        cfg.set_zoom(canvas_->zoom());
        cfg.sync();
        ev->accept();
    }

    void update_doc_name_display() {
        if (!doc_name_edit_) return;
        QString name = current_path_.isEmpty()
            ? doc_custom_name_ : QFileInfo(current_path_).baseName();
        doc_name_edit_->setText(name);
    }

    void update_capa_badge() {
        bool enabled = canvas_->ctrl().state().script.title_page.enabled;
        if (capa_badge_) capa_badge_->setVisible(enabled);
        if (capa_toggle_act_) capa_toggle_act_->setChecked(enabled);
    }

    ScreenplayCanvas* canvas_      = nullptr;
    QListWidget*      scene_list_  = nullptr;
    StatsPanel*       stats_panel_ = nullptr;
    QDockWidget*      stats_dock_  = nullptr;
    QDockWidget*      scene_dock_  = nullptr;
    ScriptDatabase*   db_panel_    = nullptr;
    QDockWidget*      db_dock_     = nullptr;
    QLabel*           zoom_lbl_     = nullptr;
    QLabel*           pg_lbl_       = nullptr;
    QLabel*           word_lbl_     = nullptr;
    QLabel*           runtime_lbl_  = nullptr;
    QLabel*           blk_type_lbl_ = nullptr;
    QLineEdit*        doc_name_edit_ = nullptr;
    QLabel*           capa_badge_    = nullptr;
    QAction*          capa_toggle_act_   = nullptr;
    QAction*          spell_status_act_  = nullptr;
    QAction*          act_view_scenes_   = nullptr;
    QAction*          act_view_stats_    = nullptr;
    QAction*          act_view_database_ = nullptr;
    QString           current_path_;
    QString           doc_custom_name_   = "Untitled";
};

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

    // Material Design 3 Dark palette
    QPalette pal;
    pal.setColor(QPalette::Window,          MD3::Surface);
    pal.setColor(QPalette::WindowText,      MD3::OnSurface);
    pal.setColor(QPalette::Base,            QColor(0x2D, 0x2C, 0x31));
    pal.setColor(QPalette::AlternateBase,   MD3::SurfaceVar);
    pal.setColor(QPalette::Text,            MD3::OnSurface);
    pal.setColor(QPalette::Button,          MD3::SurfaceVar);
    pal.setColor(QPalette::ButtonText,      MD3::OnSurface);
    pal.setColor(QPalette::Highlight,       MD3::PrimaryContainer);
    pal.setColor(QPalette::HighlightedText, MD3::Primary);
    pal.setColor(QPalette::ToolTipBase,     QColor(0x2D,0x2C,0x31));
    pal.setColor(QPalette::ToolTipText,     MD3::OnSurface);
    pal.setColor(QPalette::PlaceholderText, MD3::Outline);
    app.setPalette(pal);

    app.setStyleSheet(
        // Toolbar
        "QToolBar { background:#2D2C31; border-bottom:1px solid #49454F;"
        "           spacing:3px; padding:4px; }"
        "QToolBar QToolButton { color:#E6E1E5; padding:4px 8px;"
        "           border-radius:8px; font-size:11px; font-family:'Segoe UI'; }"
        "QToolBar QToolButton:hover { background:#4F378A; }"
        "QToolBar QToolButton:pressed { background:#6650A4; }"
        "QToolBar::separator { background:#49454F; width:1px; margin:4px 2px; }"
        // Dock
        "QDockWidget { color:#E6E1E5; font-family:'Segoe UI'; font-size:12px; }"
        "QDockWidget::title { background:#2D2C31; padding:6px;"
        "           border-bottom:1px solid #49454F; }"
        // Status bar
        "QStatusBar { background:#1C1B1F; border-top:1px solid #49454F; }"
        // Menu bar
        "QMenuBar { background:#2D2C31; color:#E6E1E5; border-bottom:1px solid #49454F;"
        "           font-family:'Segoe UI'; font-size:12px; padding:2px 4px; }"
        "QMenuBar::item { padding:4px 10px; border-radius:4px; }"
        "QMenuBar::item:selected { background:#4F378A; }"
        // Menus
        "QMenu { background:#2D2C31; color:#E6E1E5; border:1px solid #49454F;"
        "        border-radius:8px; padding:4px; }"
        "QMenu::item { padding:6px 24px; border-radius:6px; }"
        "QMenu::item:selected { background:#4F378A; }"
        // Message boxes
        "QMessageBox { background:#2D2C31; color:#E6E1E5; }"
        // Scroll bar
        "QScrollBar:vertical { background:#1C1B1F; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:#49454F; border-radius:4px; }"
    );

    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"