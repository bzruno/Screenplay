#pragma once
// The cover (title page) as an editable form drawn inside the document.
//
// Owns everything about the cover: how it is painted, which rectangle each
// field occupies, and the inline editor that opens when one is clicked.
// The canvas hands it a painter and the page geometry; it hands back nothing
// but repaint requests through the write hook.

#include "app_palette.hpp"
#include "confirm_dialog.hpp"
#include "design_tokens.hpp"
#include "screenplay_font.hpp"
#include "../config/language.hpp"
#include "../config/ui_strings.hpp"
#include "../model/model.hpp"

#include <QCursor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QImageReader>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace screenplay::ui {

/// The cover's inline text field.
///
/// A plain QLineEdit is not enough here: Qt's own implementation *ignores*
/// Return and Tab so that dialogs can act on them, which means both keys
/// travel up to the canvas — Return split a screenplay block and opened
/// SmartType while the writer thought they were filling in a title. The cover
/// is a form laid over the page, not part of the script, so it swallows the
/// keys that navigate a form and never lets them reach the editor.
class CoverLineEdit : public QLineEdit {
public:
    using Action = std::function<void()>;

    explicit CoverLineEdit(QWidget* parent) : QLineEdit(parent) {}

    /// Return commits, Tab/Shift+Tab move between slots, Escape abandons.
    void on_commit(Action a) { commit_ = std::move(a); }
    void on_next(Action a)   { next_   = std::move(a); }
    void on_previous(Action a) { previous_ = std::move(a); }
    void on_cancel(Action a) { cancel_ = std::move(a); }

protected:
    /// Tab has to be caught in event(), not keyPressEvent().
    ///
    /// QWidget::event() consumes Tab and Backtab for focus navigation BEFORE
    /// it ever calls keyPressEvent, so an override there never sees them: focus
    /// jumped to the canvas, which then read the same Tab as a screenplay
    /// command and cycled the element type. Intercepting here is the only
    /// place early enough.
    bool event(QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(ev);
            switch (key->key()) {
            case Qt::Key_Tab:
                ev->accept();
                if (next_) next_();
                return true;
            case Qt::Key_Backtab:
                ev->accept();
                if (previous_) previous_();
                return true;
            default: break;
            }
        }
        return QLineEdit::event(ev);
    }

    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            event->accept();                    // never reaches the script
            if (commit_) commit_();
            return;
        case Qt::Key_Escape:
            event->accept();
            if (cancel_) cancel_();
            return;
        default:
            QLineEdit::keyPressEvent(event);
        }
    }

private:
    Action commit_, next_, previous_, cancel_;
};

class CoverEditor {
public:
    /// Where the cover page currently sits on screen, at the current zoom.
    struct Metrics {
        QRectF page;
        float  margin_left   = 0.f;
        float  margin_bottom = 0.f;
        float  body_px       = 12.f;   // body font pixel size
    };

    /// `read` returns the current title page; `write` commits a new one
    /// undoably and repaints. Supplied by the canvas so this class never
    /// reaches into the editor's state.
    using ReadTitlePage  = std::function<screenplay::TitlePage()>;
    using WriteTitlePage = std::function<void(screenplay::TitlePage)>;

    CoverEditor(QWidget* host, ReadTitlePage read, WriteTitlePage write)
        : host_(host), read_(std::move(read)), write_(std::move(write)) {}

    /// Paints the cover and rebuilds the click targets, so the hit areas stay
    /// correct through scrolling and zooming.
    void render(QPainter& painter, const screenplay::TitlePage& page,
                const Metrics& metrics) {
        metrics_ = metrics;
        fields_.clear();

        const QFont title_font = build_font(metrics.body_px * kTitleScale, true);
        const QFont body_font  = build_font(metrics.body_px, false);
        const float title_height = (float)QFontMetricsF(title_font).height();

        painter.setPen(MD3::PageText);
        draw_logo_slot(painter, page, title_height);
        draw_title(painter, page, title_font);
        draw_credits(painter, page, body_font, title_height);
        draw_contact(painter, page, body_font);
        draw_empty_field_guides(painter, page, body_font);
    }

    bool editing() const { return editing_; }

    /// Opens the inline editor for the field under `pos`, if there is one.
    /// The logo slot opens a file picker instead. Returns false when the click
    /// was not on the cover, so the canvas can handle it as a normal click.
    bool begin_edit_at(const QPointF& pos) {
        if (!read_().enabled) return false;
        const Field* hit = field_at(pos);
        if (!hit) return false;

        // Moving between fields reuses the one QLineEdit, so nothing else
        // would tell the previous field its text was final.
        const Field field = *hit;
        commit();

        if (field.kind == Field::Logo) choose_logo();
        else                           open_inline_editor(field);
        return true;
    }

    /// Re-places the open editor over its field, and hides it while that field
    /// is scrolled out of view so it never floats over the workspace.
    void sync_geometry() {
        if (!editing_ || !inline_edit_) return;
        for (const auto& field : fields_) {
            if (field.kind != editing_field_.kind)  continue;
            if (field.index != editing_field_.index) continue;
            const QRect box = editor_box(field);
            inline_edit_->setGeometry(box);
            inline_edit_->setVisible(box.bottom() >= 0
                                  && box.top() <= host_->height());
            return;
        }
        inline_edit_->hide();      // the cover itself is no longer rendered
    }

    void commit() {
        if (!editing_) return;
        editing_ = false;
        screenplay::TitlePage page = read_();
        apply_edit(page, inline_edit_->text().trimmed().toStdString());
        inline_edit_->hide();
        host_->setFocus();
        write_(std::move(page));
    }

    /// Commits the open slot and opens its neighbour — Tab walks the cover as
    /// a form, in the order the fields are printed.
    void step_field(int direction) {
        const auto order = tab_order();
        if (order.empty()) return;

        int at = 0;
        for (size_t i = 0; i < order.size(); ++i)
            if (order[i].kind == editing_field_.kind &&
                order[i].index == editing_field_.index) { at = (int)i; break; }

        const int count = (int)order.size();
        const Field next = order[(size_t)(((at + direction) % count + count) % count)];
        commit();
        // commit() rewrites the title page, which repaints and rebuilds the
        // field rects; `next` is a copy, so it survives that.
        open_inline_editor(next);
    }

    void cancel() {
        if (!editing_) return;
        editing_ = false;
        inline_edit_->hide();
        host_->setFocus();
    }

private:
    /// A clickable region of the rendered cover, in host coordinates.
    struct Field {
        enum Kind { Title, Credit, Author, BasedOn,
                    Contact, Address2, Contact1, Contact2, Logo };
        Kind   kind  = Title;
        int    index = 0;      // author row; 0 for the single-value fields
        QRectF rect;
    };

    static constexpr float kTitleScale   = 1.25f;
    static constexpr float kLogoWidth    = 0.44f;   // fraction of the page
    static constexpr float kLogoHeight   = 0.16f;
    static constexpr float kMinFieldWide = 0.35f;
    static constexpr float kCenterFromTop = 0.40f;
    // How far the dashed guide sits outside its field's text box.
    static constexpr float kGuidePadX = 6.f;
    static constexpr float kGuidePadY = 3.f;
    // Breathing room between two stacked guides, so they read as separate
    // slots rather than one block with lines through it.
    static constexpr float kRowGap    = 5.f;

    // ── Painting ──────────────────────────────────────────────────────────

    static float snap(float v) { return std::round(v * 2.f) / 2.f; }

    static QFont build_font(float pixel_size, bool bold) {
        QFont font;
        font.setFamily(ScreenplayFont::family());
        font.setPixelSize(qRound(pixel_size));
        font.setStyleHint(QFont::TypeWriter);
        font.setBold(bold);
        ScreenplayFont::apply_render_quality(font);
        return font;
    }

    float center_y() const {
        return (float)metrics_.page.top()
             + (float)metrics_.page.height() * kCenterFromTop;
    }

    /// An empty field still needs a target wide enough to click into.
    void record(Field::Kind kind, int index, float x, float y,
                float w, float h) {
        const float minimum = (float)metrics_.page.width() * kMinFieldWide;
        if (w < minimum) {
            x = (float)metrics_.page.left()
              + ((float)metrics_.page.width() - minimum) * .5f;
            w = minimum;
        }
        fields_.push_back({ kind, index, QRectF(x, y, w, h) });
    }

    void draw_centered(QPainter& painter, const QString& text, float y,
                       const QFontMetricsF& fm, Field::Kind kind, int index) {
        const float w = (float)fm.horizontalAdvance(text);
        const float x = (float)metrics_.page.left()
                      + ((float)metrics_.page.width() - w) * .5f;
        painter.drawText(QPointF(snap(x), snap(y + (float)fm.ascent())), text);
        record(kind, index, x, y, w, (float)fm.height());
    }

    /// The cover can carry the film's artwork instead of (or above) a typed
    /// title. The slot is always present so it can be clicked while empty.
    void draw_logo_slot(QPainter& painter, const screenplay::TitlePage& page,
                        float title_height) {
        const float w = (float)metrics_.page.width()  * kLogoWidth;
        const float h = (float)metrics_.page.height() * kLogoHeight;
        const QRectF slot((float)metrics_.page.left()
                              + ((float)metrics_.page.width() - w) * .5f,
                          center_y() - h - title_height * .6f, w, h);

        const QPixmap& art = logo_pixmap(page.logo_path);
        if (!art.isNull()) {
            const QPixmap scaled = art.scaled(slot.size().toSize(),
                                              Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation);
            painter.drawPixmap(
                QPointF(slot.center().x() - scaled.width()  * .5,
                        slot.center().y() - scaled.height() * .5), scaled);
        }
        fields_.push_back({ Field::Logo, 0, slot });
    }

    void draw_title(QPainter& painter, const screenplay::TitlePage& page,
                    const QFont& title_font) {
        painter.setFont(title_font);
        const QFontMetricsF fm(title_font);
        draw_centered(painter, QString::fromStdString(page.title), center_y(),
                      fm, Field::Title, 0);
    }

    /// Credit line plus one row per author. An empty author list still gets a
    /// clickable row, so a blank cover can be filled in from here.
    void draw_credits(QPainter& painter, const screenplay::TitlePage& page,
                      const QFont& body_font, float title_height) {
        painter.setFont(body_font);
        const QFontMetricsF fm(body_font);
        const float line = (float)fm.height() * 1.5f;
        float y = center_y() + title_height + line * 1.5f;

        const std::string credit = page.credit_type.empty() ? default_credit()
                                                            : page.credit_type;
        draw_centered(painter, QString::fromStdString(credit), y, fm,
                      Field::Credit, 0);
        y += line;

        if (page.authors.empty()) {
            draw_centered(painter, QString(), y, fm, Field::Author, 0);
            y += line;
        } else {
            for (size_t i = 0; i < page.authors.size(); ++i) {
                draw_centered(painter, QString::fromStdString(page.authors[i]),
                              y, fm, Field::Author, (int)i);
                y += line;
            }
        }

        // The source credit sits below the authors, set apart: it credits a
        // different work, not another writer of this one.
        draw_centered(painter, QString::fromStdString(page.based_on),
                      y + line * .5f, fm, Field::BasedOn, 0);
    }

    /// The bottom-left block: an address over the ways to reach the writer.
    /// Four fixed rows rather than a growing list, because the block has to
    /// stay inside the bottom margin — a cover that scrolls is not a cover.
    void draw_contact(QPainter& painter, const screenplay::TitlePage& page,
                      const QFont& body_font) {
        painter.setFont(body_font);
        const QFontMetricsF fm(body_font);
        const float x = (float)metrics_.page.left() + metrics_.margin_left;
        const float row_h = (float)fm.height();
        const float width = (float)metrics_.page.width() * 0.45f;

        struct Row { Field::Kind kind; const std::string& text; };
        const Row rows[] = {
            { Field::Contact,  page.contact_left },
            { Field::Address2, page.address_2    },
            { Field::Contact1, page.contact_1    },
            { Field::Contact2, page.contact_2    },
        };

        // Each row's dashed guide is drawn 3 px above and below its text box
        // (see outline_box), so stepping by the text height alone made
        // consecutive boxes overlap and read as one smeared block. The step
        // has to clear the guide, not just the glyphs.
        const float step = row_h + kGuidePadY * 2.f + kRowGap;

        // Laid out upwards from the bottom margin so the last row always rests
        // on it, however many rows are filled in.
        float y = (float)metrics_.page.bottom() - metrics_.margin_bottom
                - step * (float)std::size(rows);

        for (const auto& row : rows) {
            const QString text = QString::fromStdString(row.text);
            if (!text.isEmpty())
                painter.drawText(QPointF(snap(x), snap(y + (float)fm.ascent())),
                                 text);
            fields_.push_back({ row.kind, 0, QRectF(x, y, width, row_h) });
            y += step;
        }
    }

    static bool is_contact(Field::Kind kind) {
        return kind == Field::Contact  || kind == Field::Address2
            || kind == Field::Contact1 || kind == Field::Contact2;
    }

    /// Every empty slot gets a dashed box so the cover reads as a form to fill
    /// in rather than a blank page that happens to be clickable. Once a field
    /// has content its box and placeholder disappear — the outline exists to
    /// say "something goes here", which stops being true once something does.
    /// Editing scaffolding only: PdfExporter renders the cover itself and
    /// knows nothing about these.
    void draw_empty_field_guides(QPainter& painter,
                                 const screenplay::TitlePage& page,
                                 const QFont& body_font) {
        painter.save();
        QColor guide = MD3::PageTextDim;
        guide.setAlpha(110);
        QPen pen(guide, 1, Qt::DashLine);
        pen.setDashPattern({ 3, 3 });
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.setFont(body_font);

        for (const auto& field : fields_) {
            if (!is_empty(page, field)) continue;
            const QRectF box = outline_box(field);
            painter.drawRoundedRect(box, Radius::Chip, Radius::Chip);
            painter.save();
            painter.setPen(guide);
            painter.drawText(box, Qt::AlignCenter, placeholder(field.kind));
            painter.restore();
        }
        painter.restore();
    }

    // ── Fields ────────────────────────────────────────────────────────────

    static QRectF outline_box(const Field& field) {
        return field.rect.adjusted(-kGuidePadX, -kGuidePadY,
                                    kGuidePadX,  kGuidePadY);
    }
    static QRect editor_box(const Field& field) {
        return outline_box(field).toRect();
    }

    /// The text slots, in reading order. The logo is left out: Tab should not
    /// drop the writer into a file dialog mid-form.
    std::vector<Field> tab_order() const {
        std::vector<Field> order;
        for (const auto& field : fields_)
            if (field.kind != Field::Logo) order.push_back(field);
        std::stable_sort(order.begin(), order.end(),
                         [](const Field& a, const Field& b) {
                             return a.rect.top() < b.rect.top();
                         });
        return order;
    }

    const Field* field_at(const QPointF& pos) const {
        for (const auto& field : fields_)
            if (field.rect.adjusted(-4, -4, 4, 4).contains(pos)) return &field;
        return nullptr;
    }

    static QString placeholder(Field::Kind kind) {
        switch (kind) {
        case Field::Title:    return config::tr_ui("Title");
        case Field::Credit:   return config::tr_ui("Written by");
        case Field::Author:   return config::tr_ui("Author name");
        case Field::BasedOn:  return config::tr_ui("Based on\xe2\x80\xa6");
        case Field::Contact:  return config::tr_ui("Info 1");
        case Field::Address2: return config::tr_ui("Info 2");
        case Field::Contact1: return config::tr_ui("Info 3");
        case Field::Contact2: return config::tr_ui("Info 4");
        case Field::Logo:     return config::tr_ui("Click to add a logo");
        }
        return {};
    }

    static bool is_empty(const screenplay::TitlePage& page, const Field& f) {
        switch (f.kind) {
        case Field::Title:    return page.title.empty();
        case Field::Credit:   return false;   // always shows a default
        case Field::Author:   return (size_t)f.index >= page.authors.size()
                                  || page.authors[(size_t)f.index].empty();
        case Field::BasedOn:  return page.based_on.empty();
        case Field::Contact:  return page.contact_left.empty();
        case Field::Address2: return page.address_2.empty();
        case Field::Contact1: return page.contact_1.empty();
        case Field::Contact2: return page.contact_2.empty();
        case Field::Logo:     return page.logo_path.empty();
        }
        return true;
    }

    static std::string default_credit() {
        return screenplay::config::LanguageConfig::current() ==
               screenplay::config::AppLanguage::Portuguese
                   ? "Escrito por" : "Written by";
    }

    /// Decoded artwork, cached by path so a repaint doesn't re-read the file
    /// every frame. A path that fails to load caches as a null pixmap, which
    /// the renderer treats as "no logo".
    const QPixmap& logo_pixmap(const std::string& path) {
        if (path != logo_path_) {
            logo_path_ = path;
            logo_ = path.empty() ? QPixmap()
                                 : QPixmap(QString::fromStdString(path));
        }
        return logo_;
    }

    // ── Inline text editor ────────────────────────────────────────────────

    QString initial_text(const Field& field) const {
        const screenplay::TitlePage page = read_();
        switch (field.kind) {
        case Field::Title:    return QString::fromStdString(page.title);
        case Field::Credit:   return QString::fromStdString(
                                     page.credit_type.empty()
                                         ? default_credit() : page.credit_type);
        case Field::Author:   return (size_t)field.index < page.authors.size()
                                  ? QString::fromStdString(
                                        page.authors[(size_t)field.index])
                                  : QString();
        case Field::BasedOn:  return QString::fromStdString(page.based_on);
        case Field::Contact:  return QString::fromStdString(page.contact_left);
        case Field::Address2: return QString::fromStdString(page.address_2);
        case Field::Contact1: return QString::fromStdString(page.contact_1);
        case Field::Contact2: return QString::fromStdString(page.contact_2);
        case Field::Logo:     break;
        }
        return {};
    }

    void open_inline_editor(const Field& field) {
        if (!inline_edit_) {
            inline_edit_ = new CoverLineEdit(host_);
            inline_edit_->hide();
            inline_edit_->on_commit ([this] { commit(); });
            inline_edit_->on_cancel ([this] { cancel(); });
            inline_edit_->on_next   ([this] { step_field(+1); });
            inline_edit_->on_previous([this] { step_field(-1); });
            // Clicking away is still a commit; only the KEYS are ours.
            QObject::connect(inline_edit_, &QLineEdit::editingFinished,
                             inline_edit_, [this] { commit(); });
        }
        editing_field_ = field;
        editing_       = true;

        const bool is_title = (field.kind == Field::Title);
        inline_edit_->setFont(build_font(
            metrics_.body_px * (is_title ? kTitleScale : 1.f), is_title));
        // The contact block is flush left in the bottom corner; everything
        // else is centred, so the editor must match or the text jumps when it
        // opens over the field.
        inline_edit_->setAlignment(is_contact(field.kind) ? Qt::AlignLeft
                                                          : Qt::AlignHCenter);
        inline_edit_->setStyleSheet(
            QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                    "            border-radius:%4px; padding:0px 4px; }")
                .arg(MD3::hx(MD3::PageBg), MD3::hx(MD3::PageText),
                     MD3::hx(MD3::Primary))
                .arg(Radius::Chip));
        inline_edit_->setText(initial_text(field));
        inline_edit_->setGeometry(editor_box(field));
        inline_edit_->show();
        inline_edit_->setFocus();
        inline_edit_->selectAll();
    }

    void apply_edit(screenplay::TitlePage& page, const std::string& value) const {
        switch (editing_field_.kind) {
        case Field::Title:    page.title        = value; return;
        case Field::Credit:   page.credit_type  = value; return;
        case Field::BasedOn:  page.based_on     = value; return;
        case Field::Contact:  page.contact_left = value; return;
        case Field::Address2: page.address_2    = value; return;
        case Field::Contact1: page.contact_1    = value; return;
        case Field::Contact2: page.contact_2    = value; return;
        case Field::Logo:     return;   // never a text edit
        case Field::Author:   break;
        }
        const size_t row = (size_t)editing_field_.index;
        if (value.empty()) {
            if (row < page.authors.size())
                page.authors.erase(page.authors.begin() + (long)row);
        } else if (row < page.authors.size()) {
            page.authors[row] = value;
        } else {
            page.authors.push_back(value);
        }
    }

    // ── Logo slot ─────────────────────────────────────────────────────────

    /// Clicking an empty slot picks an image; clicking one that already has
    /// artwork offers replace/remove first.
    void choose_logo() {
        screenplay::TitlePage page = read_();
        if (!page.logo_path.empty() && !confirm_logo_replacement(page)) return;

        const QStringList globs = decodable_globs();
        const QString file = QFileDialog::getOpenFileName(
            host_, config::tr_ui("Choose a logo"), QString(),
            config::tr_ui("Images") + " (" + globs.join(' ') + ")");
        if (file.isEmpty()) return;

        // Load before committing, so a file that cannot be decoded reports
        // itself instead of leaving an empty slot and no explanation.
        if (QPixmap(file).isNull()) {
            report_unreadable(file, globs);
            return;
        }
        page.logo_path = file.toStdString();
        write_(std::move(page));
    }

    /// True when the caller should go on to pick a new file. Removing the
    /// existing logo is handled here and reported as "nothing more to do".
    bool confirm_logo_replacement(screenplay::TitlePage& page) {
        QMenu menu(host_);
        QAction* replace = menu.addAction(config::tr_ui("Replace logo\xe2\x80\xa6"));
        QAction* remove  = menu.addAction(config::tr_ui("Remove logo"));
        QAction* chosen  = menu.exec(QCursor::pos());
        if (chosen == remove) {
            page.logo_path.clear();
            write_(std::move(page));
        }
        return chosen == replace;
    }

    /// Exactly the formats this build can decode. A hardcoded list (it once
    /// advertised .webp and .svg) lets the user pick a file the app then
    /// silently fails to draw, because those plugins are not deployed.
    static QStringList decodable_globs() {
        QStringList globs;
        for (const QByteArray& format : QImageReader::supportedImageFormats())
            globs << ("*." + QString::fromLatin1(format).toLower());
        globs.removeDuplicates();
        return globs;
    }

    void report_unreadable(const QString& file, const QStringList& globs) const {
        ConfirmDialog::ask(host_, {
            config::tr_ui("Could not load image"),
            QString(config::tr_ui("\"%1\" could not be read. Supported formats "
                                  "here are: %2."))
                .arg(QFileInfo(file).fileName(),
                     globs.join(", ").remove('*').remove('.')),
            config::tr_ui("OK"), QString(), QString(), false });
    }

    QWidget*       host_ = nullptr;
    ReadTitlePage  read_;
    WriteTitlePage write_;

    Metrics            metrics_;
    std::vector<Field> fields_;

    CoverLineEdit* inline_edit_ = nullptr;
    Field      editing_field_ {};
    bool       editing_       = false;

    std::string logo_path_;
    QPixmap     logo_;
};

} // namespace screenplay::ui
