#pragma once
// Previews a production report and prints or exports it.
//
// One renderer for all three reports: they differ only in their columns and
// rows, so a fourth report is a data change here, never a new dialog.

#include "app_dialog.hpp"
#include "design_tokens.hpp"
#include "theme_manager.hpp"
#include "typography.hpp"
#include "../config/ui_strings.hpp"
#include "../reports/script_reports.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextBrowser>
#include <QTextDocument>

#include <functional>
#include <vector>

namespace screenplay::ui {

/// Builds a report. Deferred so switching tabs recomputes from the live
/// script rather than from a snapshot taken when the dialog opened.
using ReportSource = std::function<reports::Report()>;

struct ReportTab {
    QString      label;
    ReportSource build;
};

class ReportDialog : public AppDialog {
public:
    ReportDialog(QWidget* parent, std::vector<ReportTab> tabs,
                 const QString& document_name)
        : AppDialog(parent, config::tr_ui("Reports")),
          tabs_(std::move(tabs)), document_name_(document_name) {
        resize(kWidth, kHeight);

        auto* chooser = new QHBoxLayout;
        chooser->setSpacing(Spacing::S);
        for (size_t i = 0; i < tabs_.size(); ++i) {
            auto* button = new SoftButton(this);
            button->label(tabs_[i].label)
                  ->layout_mode(SoftButton::Layout::TextOnly)
                  ->variant(SoftButton::Variant::Ghost)
                  ->height_token(ControlHeight::Button)
                  ->text_size(Typography::Size::Body);
            QObject::connect(button, &QToolButton::clicked, this,
                             [this, i]{ show_report(i); });
            chooser_buttons_.push_back(button);
            chooser->addWidget(button);
        }
        chooser->addStretch(1);
        content()->addLayout(chooser);

        preview_ = new QTextBrowser(this);
        preview_->setFrameShape(QFrame::NoFrame);
        content()->addWidget(preview_, 1);

        auto* close = add_button(config::tr_ui("Close"), SoftButton::Variant::Ghost);
        QObject::connect(close, &QToolButton::clicked, this, [this]{ reject(); });
        auto* pdf = add_button(config::tr_ui("Export PDF\xe2\x80\xa6"),
                               SoftButton::Variant::Primary);
        QObject::connect(pdf, &QToolButton::clicked, this, [this]{ export_pdf(); });

        show_report(0);
    }

private:
    static constexpr int kWidth  = 860;
    static constexpr int kHeight = 620;

    void show_report(size_t index) {
        if (index >= tabs_.size()) return;
        current_ = index;
        for (size_t i = 0; i < chooser_buttons_.size(); ++i)
            chooser_buttons_[i]->variant(i == index ? SoftButton::Variant::Soft
                                                    : SoftButton::Variant::Ghost);
        report_ = tabs_[index].build();
        preview_->setHtml(as_html(report_, /*for_print=*/false));
    }

    /// Rendered as HTML because the report IS a table, and QTextDocument gives
    /// the same markup to the screen and to the PDF — so what is previewed is
    /// exactly what is exported.
    QString as_html(const reports::Report& report, bool for_print) const {
        const ThemePalette& p = ThemeManager::instance().palette();
        const QString text  = for_print ? "#111111" : ThemePalette::hex(p.Text);
        const QString dim   = for_print ? "#666666" : ThemePalette::hex(p.TextDim);
        const QString rule  = for_print ? "#cccccc" : ThemePalette::hex(p.Border);
        const QString band  = for_print ? "#f4f4f4" : ThemePalette::hex(p.Bg0);

        QString html =
            "<html><body style='font-family:" + Typography::family() + ";"
            "color:" + text + ";'>";
        html += "<h2 style='margin:0 0 2px 0;'>"
              + config::tr_ui(report.title.c_str()) + "</h2>";
        html += "<div style='color:" + dim + ";font-size:small;margin-bottom:14px;'>"
              + document_name_.toHtmlEscaped() + " \xe2\x80\x94 "
              + QString::fromStdString(report.summary).toHtmlEscaped()
              + "</div>";

        if (report.empty()) {
            html += "<p style='color:" + dim + ";'>"
                  + config::tr_ui("Nothing to report yet.") + "</p></body></html>";
            return html;
        }

        html += "<table width='100%' cellspacing='0' cellpadding='6'>";
        html += "<tr>";
        for (const auto& column : report.columns)
            html += "<th align='" + QString(column.numeric ? "right" : "left")
                  + "' style='border-bottom:1px solid " + rule + ";color:" + dim
                  + ";font-weight:600;'>"
                  + config::tr_ui(column.title.c_str()) + "</th>";
        html += "</tr>";

        for (size_t r = 0; r < report.rows.size(); ++r) {
            // Banded rows: a wide table with five columns is hard to track
            // across on paper without them.
            const QString bg = (r % 2) ? (" bgcolor='" + band + "'") : QString();
            html += "<tr" + bg + ">";
            for (size_t c = 0; c < report.rows[r].size(); ++c) {
                const bool numeric = c < report.columns.size()
                                  && report.columns[c].numeric;
                html += "<td align='" + QString(numeric ? "right" : "left")
                      + "' style='border-bottom:1px solid " + rule + ";'>"
                      + QString::fromStdString(report.rows[r][c]).toHtmlEscaped()
                      + "</td>";
            }
            html += "</tr>";
        }
        html += "</table></body></html>";
        return html;
    }

    void export_pdf() {
        const QString suggested = document_name_ + " \xe2\x80\x94 "
                                + config::tr_ui(report_.title.c_str()) + ".pdf";
        const QString path = QFileDialog::getSaveFileName(
            this, config::tr_ui("Export PDF\xe2\x80\xa6"), suggested,
            config::tr_ui("PDF document") + " (*.pdf)");
        if (path.isEmpty()) return;

        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setTitle(config::tr_ui(report_.title.c_str()));

        QTextDocument doc;
        doc.setHtml(as_html(report_, /*for_print=*/true));
        doc.setPageSize(QSizeF(writer.width(), writer.height()));
        doc.print(&writer);
    }

    std::vector<ReportTab>   tabs_;
    QString                  document_name_;
    QTextBrowser*            preview_ = nullptr;
    std::vector<SoftButton*> chooser_buttons_;
    reports::Report          report_;
    size_t                   current_ = 0;
};

} // namespace screenplay::ui
