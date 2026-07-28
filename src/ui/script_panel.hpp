#pragma once
// The script's own panel: what this screenplay is made of.
//
// Replaces the two panels that used to sit in separate docks — Statistics and
// Script Database. They answered one question between them ("what is in this
// script?") and answered it twice, in two different visual languages, each
// with its own footer. Merged, the totals become the panel's header and the
// lists become its tabs.
//
// The tables are rendered from screenplay::reports, the same models the
// Reports dialog and its PDF are built from, so the panel and a printed report
// can never disagree about a scene count or who speaks most.

#include "app_palette.hpp"
#include "controls.hpp"
#include "design_tokens.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"
#include "../config/ui_strings.hpp"
#include "../database/script_index.hpp"
#include "../reports/script_reports.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <map>
#include <vector>

namespace screenplay::ui {

/// One headline number: the figure large, its name small and quiet beneath.
/// Painted rather than styled so the tile reads identically in all three
/// themes without a stylesheet per theme.
class StatTile : public QWidget {
public:
    StatTile(QWidget* parent, const QString& caption)
        : QWidget(parent), caption_(caption) {
        setMinimumHeight(kHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void set_value(const QString& v) { value_ = v; update(); }
    void set_caption(const QString& c) { caption_ = c; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        const ThemePalette& p = ThemeManager::instance().palette();
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);

        QPainterPath card;
        card.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            Radius::Chip, Radius::Chip);
        g.fillPath(card, p.Bg0);
        g.setPen(QPen(p.Border, BorderWidth::Hairline));
        g.drawPath(card);

        g.setPen(p.Text);
        g.setFont(Typography::ui_font(Typography::Size::Title,
                                      Typography::Weight::Semibold));
        g.drawText(QRect(0, Spacing::XS, width(), kValueH),
                   Qt::AlignHCenter | Qt::AlignVCenter, value_);

        g.setPen(p.TextDim);
        g.setFont(Typography::ui_font(Typography::Size::Caption));
        g.drawText(QRect(0, kValueH, width(), height() - kValueH - Spacing::XS),
                   Qt::AlignHCenter | Qt::AlignTop, caption_);
    }

private:
    static constexpr int kHeight = 58;
    static constexpr int kValueH = 32;
    QString caption_, value_ = "0";
};

class ScriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit ScriptPanel(QWidget* parent = nullptr) : QWidget(parent) {
        auto* column = new QVBoxLayout(this);
        column->setContentsMargins(Spacing::M, Spacing::M, Spacing::M, Spacing::M);
        column->setSpacing(Spacing::M);

        build_totals(column);
        build_tab_row(column);
        build_filter(column);
        build_tables(column);

        show_tab(0);
        restyle();
    }

    void refresh(const screenplay::Script& script,
                 const screenplay::layout::PageList& pages) {
        const auto index = database::ScriptIndexBuilder::build(script, pages);

        int spoken = 0;
        for (const auto& scene : index.scenes) spoken += (int)scene.dialogue.size();
        scenes_->set_value(QString::number(index.scenes.size()));
        characters_->set_value(QString::number(index.characters.size()));
        dialogue_->set_value(QString::number(spoken));
        fill(scene_table_,     reports::scene_report(script, index));
        // The block index rides on the row's own item rather than in a
        // side vector: the table is sortable, so a row's POSITION stops
        // identifying it the moment the reader clicks a column header.
        for (int r = 0; r < scene_table_->rowCount()
                     && (size_t)r < index.scenes.size(); ++r)
            if (auto* item = scene_table_->item(r, 0))
                item->setData(Qt::UserRole,
                              (qulonglong)index.scenes[(size_t)r].block_idx);
        fill(character_table_, reports::character_report(script, index));
        const auto places = reports::location_report(script, index);
        locations_->set_value(QString::number(places.rows.size()));
        fill(location_table_,  places);
        fill_dialogue(script, index);
        apply_filter();
    }

    void restyle() {
        const ThemePalette& p = ThemeManager::instance().palette();
        const QString table_style =
            QString("QTableWidget { background:%1; color:%2;"
                    "               border:1px solid %8; gridline-color:%8;"
                    "               font-size:%3px; }"
                    "QTableWidget::item { padding:%4px %5px; }"
                    "QTableWidget::item:selected { background:%6; color:%2; }"
                    "QHeaderView::section { background:%1; color:%7;"
                    "                       border:none; border-bottom:1px solid %8;"
                    "                       padding:%4px %5px; font-weight:600;"
                    "                       font-size:%9px; }")
                .arg(ThemePalette::hex(p.Bg1), ThemePalette::hex(p.Text))
                .arg(Typography::size_px(Typography::Size::BodySmall))
                .arg(Spacing::XS).arg(Spacing::S)
                .arg(ThemePalette::hex(p.SelectionBg),
                     ThemePalette::hex(p.TextDim),
                     ThemePalette::hex(p.Divider))
                .arg(Typography::size_px(Typography::Size::Caption));

        for (auto* table : tables()) table->setStyleSheet(table_style);
        filter_->setStyleSheet(
            QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                    "            border-radius:%4px; padding:%5px %6px; }"
                    "QLineEdit:focus { border:1px solid %7; }")
                .arg(ThemePalette::hex(p.Bg1), ThemePalette::hex(p.Text),
                     ThemePalette::hex(p.Border))
                .arg(Radius::Button).arg(Spacing::XS).arg(Spacing::S)
                .arg(ThemePalette::hex(p.Primary)));
        retranslate();
        for (auto* tile : tiles()) tile->update();
        update();
    }

signals:
    /// A character row was chosen — the canvas highlights their dialogue.
    void character_clicked(const QString& name);
    /// A scene row was chosen; carries the heading's block index.
    void scene_activated(int block_idx);

private:
    /// Tab order, and the caption of the tile that counts each one.
    static constexpr const char* kTabs[4] =
        { "Scenes", "Characters", "Locations", "Dialogue" };

    // ── Construction ──────────────────────────────────────────────────────

    void build_totals(QVBoxLayout* column) {
        auto* row = new QHBoxLayout;
        row->setSpacing(Spacing::S);
        // Pages, words and running time already sit in the footer; repeating
        // them here spent the panel's most prominent row on numbers the writer
        // can already see. These four count what the tabs below hold.
        scenes_     = new StatTile(this, config::tr_ui("Scenes"));
        characters_ = new StatTile(this, config::tr_ui("Characters"));
        locations_  = new StatTile(this, config::tr_ui("Locations"));
        dialogue_   = new StatTile(this, config::tr_ui("Dialogue"));
        for (auto* tile : tiles()) row->addWidget(tile);
        column->addLayout(row);
    }

    void build_tab_row(QVBoxLayout* column) {
        auto* row = new QHBoxLayout;
        row->setSpacing(ButtonRhythm::WithinGroup);
        for (int i = 0; i < 4; ++i) {
            auto* button = new SoftButton(this);
            button->label(config::tr_ui(kTabs[i]))
                  ->layout_mode(SoftButton::Layout::TextOnly)
                  ->variant(SoftButton::Variant::Ghost)
                  ->height_token(ControlHeight::Compact)
                  ->text_size(Typography::Size::BodySmall);
            QObject::connect(button, &QToolButton::clicked, this,
                             [this, i]{ show_tab(i); });
            tab_buttons_.push_back(button);
            row->addWidget(button);
        }
        row->addStretch(1);
        column->addLayout(row);
    }

    /// Re-applies every string from the translation table.
    ///
    /// Tab labels and tile captions are set once at construction, so switching
    /// language left them in the language the app started in — while the table
    /// headers, rebuilt on every refresh, had already changed. One panel in two
    /// languages at once.
    void retranslate() {
        for (size_t i = 0; i < tab_buttons_.size(); ++i)
            tab_buttons_[i]->label(config::tr_ui(kTabs[i]));
        const auto all = tiles();
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i]) all[i]->set_caption(config::tr_ui(kTabs[i]));
        if (filter_)
            filter_->setPlaceholderText(config::tr_ui("Filter\xe2\x80\xa6"));
    }

    void build_filter(QVBoxLayout* column) {
        filter_ = new QLineEdit(this);
        filter_->setPlaceholderText(config::tr_ui("Filter\xe2\x80\xa6"));
        filter_->setClearButtonEnabled(true);
        QObject::connect(filter_, &QLineEdit::textChanged,
                         this, [this]{ apply_filter(); });
        column->addWidget(filter_);
    }

    void build_tables(QVBoxLayout* column) {
        stack_ = new QStackedWidget(this);
        scene_table_     = make_table();
        character_table_ = make_table();
        location_table_  = make_table();
        dialogue_table_  = make_table();
        for (auto* table : tables()) stack_->addWidget(table);
        column->addWidget(stack_, 1);

        QObject::connect(scene_table_, &QTableWidget::cellActivated, this,
                         [this](int row, int) { activate_scene(row); });
        QObject::connect(scene_table_, &QTableWidget::cellClicked, this,
                         [this](int row, int) { activate_scene(row); });
        QObject::connect(character_table_, &QTableWidget::cellClicked, this,
                         [this](int row, int) {
                             if (auto* item = character_table_->item(row, 0))
                                 emit character_clicked(item->text());
                         });
    }

    QTableWidget* make_table() {
        auto* table = new QTableWidget(this);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setShowGrid(true);
        table->setAlternatingRowColors(false);
        table->setSortingEnabled(true);
        table->setWordWrap(false);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setStretchLastSection(true);
        return table;
    }

    std::vector<QTableWidget*> tables() const {
        return { scene_table_, character_table_, location_table_, dialogue_table_ };
    }

    // ── Filling ───────────────────────────────────────────────────────────

    /// Renders a report model. The panel therefore shows exactly the numbers a
    /// printed report would, because it reads the same builder.
    static void fill(QTableWidget* table, const reports::Report& report) {
        const bool was_sorting = table->isSortingEnabled();
        table->setSortingEnabled(false);
        table->clear();

        QStringList headers;
        for (const auto& c : report.columns)
            headers << config::tr_ui(c.title.c_str());
        table->setColumnCount((int)report.columns.size());
        table->setHorizontalHeaderLabels(headers);
        table->setRowCount((int)report.rows.size());

        for (int r = 0; r < (int)report.rows.size(); ++r)
            for (int c = 0; c < (int)report.rows[(size_t)r].size(); ++c) {
                // Everything flush left: a column of right-aligned numbers
                // beside left-aligned text makes the eye jump across the row.
                auto* item = new QTableWidgetItem(
                    QString::fromStdString(report.rows[(size_t)r][(size_t)c]));
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                table->setItem(r, c, item);
            }
        table->setSortingEnabled(was_sorting);
    }

    void fill_dialogue(const screenplay::Script& script,
                       const database::ScriptIndex& index) {
        dialogue_table_->setSortingEnabled(false);
        dialogue_table_->clear();
        dialogue_table_->setColumnCount(3);
        dialogue_table_->setHorizontalHeaderLabels(
            { config::tr_ui("Character"), config::tr_ui("Line"),
              config::tr_ui("Scene") });

        const auto labels = scene_labels(script);
        int rows = 0;
        for (const auto& scene : index.scenes) rows += (int)scene.dialogue.size();
        dialogue_table_->setRowCount(rows);

        int r = 0;
        for (const auto& scene : index.scenes) {
            const auto label = labels.find(scene.block_idx);
            const QString scene_name = label != labels.end()
                ? QString::fromStdString(label->second)
                : QString::number(scene.scene_number);
            for (const auto& line : scene.dialogue) {
                dialogue_table_->setItem(r, 0, new QTableWidgetItem(
                    QString::fromStdString(line.character)));
                dialogue_table_->setItem(r, 1, new QTableWidgetItem(
                    QString::fromStdString(line.text)));
                dialogue_table_->setItem(r, 2, new QTableWidgetItem(scene_name));
                ++r;
            }
        }
        dialogue_table_->setSortingEnabled(true);
    }

    static std::map<size_t, std::string>
    scene_labels(const screenplay::Script& script) {
        std::map<size_t, std::string> out;
        for (const auto& scene : production::scene_numbers(script))
            out[scene.block_idx] = scene.label;
        return out;
    }

    // ── Behaviour ─────────────────────────────────────────────────────────

    void show_tab(int index) {
        current_ = index;
        stack_->setCurrentIndex(index);
        for (size_t i = 0; i < tab_buttons_.size(); ++i)
            tab_buttons_[i]->variant((int)i == index ? SoftButton::Variant::Soft
                                                    : SoftButton::Variant::Ghost);
        apply_filter();
    }

    /// Hides rows that match nothing, across every column — one field filters
    /// whichever tab is open, instead of a combo box per list.
    void apply_filter() {
        auto* table = tables()[(size_t)current_];
        const QString needle = filter_->text().trimmed();
        for (int r = 0; r < table->rowCount(); ++r) {
            bool visible = needle.isEmpty();
            for (int c = 0; !visible && c < table->columnCount(); ++c)
                if (auto* item = table->item(r, c))
                    visible = item->text().contains(needle, Qt::CaseInsensitive);
            table->setRowHidden(r, !visible);
        }
    }

    void activate_scene(int row) {
        auto* item = scene_table_->item(row, 0);
        if (!item) return;
        const QVariant block = item->data(Qt::UserRole);
        if (block.isValid()) emit scene_activated((int)block.toULongLong());
    }

    StatTile* scenes_     = nullptr;
    StatTile* characters_ = nullptr;
    StatTile* locations_  = nullptr;
    StatTile* dialogue_   = nullptr;

    std::vector<StatTile*> tiles() const {
        return { scenes_, characters_, locations_, dialogue_ };
    }

    std::vector<SoftButton*> tab_buttons_;
    QLineEdit*      filter_          = nullptr;
    QStackedWidget* stack_           = nullptr;
    QTableWidget*   scene_table_     = nullptr;
    QTableWidget*   character_table_ = nullptr;
    QTableWidget*   location_table_  = nullptr;
    QTableWidget*   dialogue_table_  = nullptr;
    int current_ = 0;
};

} // namespace screenplay::ui
