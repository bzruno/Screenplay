#pragma once
// io/pdf_exporter.hpp
// WGA-accurate print/PDF rendering, extracted out of the UI layer. It reuses
// the editor's LayoutEngine PageList, so the exported sheet matches the screen
// exactly (breaks, MORE/CONT'D, margins, scene numbers). The MainWindow only
// configures a QPrinter and calls render(); all document-drawing logic lives
// here, in the io layer.
//
// Behaviour is identical to the previous in-UI implementation; the only change
// is efficiency — the per-line QFont/QFontMetricsF are rebuilt only when the
// run style actually changes instead of once per visual line.

#include "../model/model.hpp"
#include "../layout/layout_engine.hpp"

#include <QPrinter>
#include <QPainter>
#include <QFont>
#include <QFontMetricsF>
#include <QPageSize>
#include <QString>
#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <functional>
#include <vector>
#include <stdexcept>

namespace screenplay::io {

class PdfExporter {
public:
    enum class SceneNumberMode { None, Left, Right, Both };

    struct Options {
        float                       pt_size             = 12.f;
        QString                     font_family         = "Courier Prime";
        std::function<void(QFont&)> apply_quality;       // may be empty
        SceneNumberMode             scene_num_mode      = SceneNumberMode::None;
        bool                        bold_scene_headings = false;
    };

    // Paper size for a layout profile — Letter (US standard) or A4.
    static QPageSize page_size_for_profile(screenplay::layout::LayoutProfile p) {
        return p == screenplay::layout::LayoutProfile::InternationalA4
            ? QPageSize(QPageSize::A4)
            : QPageSize(QPageSize::Letter);
    }

    static void render(QPrinter&                                printer,
                       const screenplay::Script&                script,
                       const screenplay::layout::PageList&      pages,
                       const screenplay::layout::PageGeometry&  geo,
                       const Options&                           opt)
    {
        QPainter painter(&printer);
        if (!painter.isActive())
            throw std::runtime_error("Cannot open printer / PDF output.");

        // Uniform scale: layout points (1/72 in) → printer device pixels.
        const QRectF dev = printer.pageRect(QPrinter::DevicePixel);
        const qreal  s   = dev.width() / geo.page_w;
        painter.scale(s, s);

        QFont base(opt.font_family);
        base.setPixelSize(qRound(opt.pt_size));   // 1 px == 1 pt in scaled space
        base.setStyleHint(QFont::TypeWriter);
        if (opt.apply_quality) opt.apply_quality(base);
        painter.setPen(Qt::black);

        const QFontMetricsF base_fm(base);

        bool first_sheet = true;
        auto next_sheet = [&] {
            if (!first_sheet) printer.newPage();
            first_sheet = false;
        };

        // ── Title page ──────────────────────────────────────────────────────
        const auto& tp = script.title_page;
        if (tp.enabled) {
            next_sheet();
            QFont title_f = base;
            title_f.setPixelSize(qRound(opt.pt_size * 1.25f));
            title_f.setBold(true);
            QFontMetricsF tfm_t(title_f);

            float y = geo.page_h * 0.40f;
            painter.setFont(title_f);
            {
                QString q = QString::fromStdString(tp.title);
                painter.drawText(
                    QPointF((geo.page_w - tfm_t.horizontalAdvance(q)) / 2.0,
                            y + tfm_t.ascent()), q);
            }
            painter.setFont(base);
            const float lh = (float)base_fm.height() * 1.5f;
            float row_y = y + (float)tfm_t.height() + lh * 1.5f;
            auto draw_centered = [&](const QString& q) {
                painter.drawText(
                    QPointF((geo.page_w - base_fm.horizontalAdvance(q)) / 2.0,
                            row_y + base_fm.ascent()), q);
                row_y += lh;
            };
            draw_centered(QString::fromStdString(
                tp.credit_type.empty() ? "Written by" : tp.credit_type));
            for (const auto& author : tp.authors)
                draw_centered(QString::fromStdString(author));
            if (!tp.contact_left.empty())
                painter.drawText(
                    QPointF(geo.margin_left,
                            geo.page_h - geo.margin_bot - base_fm.height()
                                + base_fm.ascent()),
                    QString::fromStdString(tp.contact_left));
        }

        // Scene numbers (block_idx → 1-based) when enabled
        std::vector<int> scene_num_by_block;
        if (opt.scene_num_mode != SceneNumberMode::None) {
            scene_num_by_block.resize(script.blocks.size(), 0);
            int sn = 0;
            for (size_t bi = 0; bi < script.blocks.size(); ++bi)
                if (script.blocks[bi].type == screenplay::BlockType::SceneHeading)
                    scene_num_by_block[bi] = ++sn;
        }

        // Per-line font/metrics cache — rebuilt only when the run style changes.
        int           style_key = -1;
        QFont         line_font = base;
        QFontMetricsF line_fm   = base_fm;

        // ── Script pages ────────────────────────────────────────────────────
        for (const auto& page : pages) {
            next_sheet();

            // Numbered from page one, matching what the canvas shows.
            {
                painter.setFont(base);
                QString pnum = QString::number(page.number) + ".";
                // 0.5 in (36 pt) from the top, flush with the text's right margin.
                painter.drawText(
                    QPointF(geo.page_w - geo.margin_right
                                - base_fm.horizontalAdvance(pnum),
                            36.f + base_fm.ascent()), pnum);
            }

            for (const auto& vl : page.lines) {
                if (vl.is_more || vl.is_contd) {
                    const int key = 2;   // italic only
                    if (key != style_key) {
                        line_font = base;
                        line_font.setBold(false);
                        line_font.setItalic(true);
                        line_font.setUnderline(false);
                        line_fm   = QFontMetricsF(line_font);
                        style_key = key;
                    }
                    painter.setFont(line_font);
                    painter.drawText(QPointF(vl.x, vl.y + line_fm.ascent()),
                                     QString::fromStdString(vl.display_text));
                } else {
                    // Split into style-homogeneous spans by the block's real
                    // bold/italic/underline ranges (same technique as the
                    // on-screen renderer) — a partially-styled line must not
                    // apply one style to the whole line in the exported PDF.
                    const auto& blk = script.blocks[vl.block_idx];
                    std::vector<size_t> cuts = { 0, vl.display_text.size() };
                    auto add_cuts = [&](const screenplay::StyleRuns& runs) {
                        for (auto [rs, re] : runs) {
                            if (re <= vl.start_offset || rs >= vl.end_offset) continue;
                            size_t ls = (rs > vl.start_offset) ? rs - vl.start_offset : 0;
                            size_t le = std::min(re, vl.end_offset) - vl.start_offset;
                            cuts.push_back(std::min(ls, vl.display_text.size()));
                            cuts.push_back(std::min(le, vl.display_text.size()));
                        }
                    };
                    add_cuts(blk.bold_runs);
                    add_cuts(blk.italic_runs);
                    add_cuts(blk.underline_runs);
                    std::sort(cuts.begin(), cuts.end());
                    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

                    float run_x = vl.x;
                    for (size_t ci = 0; ci + 1 < cuts.size(); ++ci) {
                        const size_t span_s = cuts[ci], span_e = cuts[ci + 1];
                        if (span_s >= span_e) continue;
                        const size_t abs_s = vl.start_offset + span_s, abs_e = vl.start_offset + span_e;

                        const bool sp_bold =
                            screenplay::style_covers(blk.bold_runs, abs_s, abs_e) ||
                            (opt.bold_scene_headings &&
                             blk.type == screenplay::BlockType::SceneHeading);
                        const bool sp_ital = screenplay::style_covers(blk.italic_runs, abs_s, abs_e);
                        const bool sp_und  = screenplay::style_covers(blk.underline_runs, abs_s, abs_e);
                        const int key = (sp_bold ? 1 : 0) | (sp_ital ? 2 : 0) | (sp_und ? 4 : 0);
                        if (key != style_key) {
                            line_font = base;
                            line_font.setBold(sp_bold);
                            line_font.setItalic(sp_ital);
                            line_font.setUnderline(sp_und);
                            line_fm   = QFontMetricsF(line_font);
                            style_key = key;
                        }
                        painter.setFont(line_font);
                        const QString span_text = QString::fromStdString(
                            vl.display_text.substr(span_s, span_e - span_s));
                        painter.drawText(QPointF(run_x, vl.y + line_fm.ascent()), span_text);
                        run_x += line_fm.horizontalAdvance(span_text);
                    }
                }

                // Scene numbers in the margins, mirroring the on-screen modes
                if (!vl.is_more && !vl.is_contd && !scene_num_by_block.empty()
                        && vl.line_in_block == 0
                        && script.blocks[vl.block_idx].type
                               == screenplay::BlockType::SceneHeading) {
                    int sn = scene_num_by_block[vl.block_idx];
                    if (sn > 0) {
                        QString sn_str = QString::number(sn) + ".";
                        if (opt.scene_num_mode == SceneNumberMode::Left ||
                            opt.scene_num_mode == SceneNumberMode::Both)
                            painter.drawText(
                                QPointF(geo.margin_left - 36.f,
                                        vl.y + line_fm.ascent()), sn_str);
                        if (opt.scene_num_mode == SceneNumberMode::Right ||
                            opt.scene_num_mode == SceneNumberMode::Both)
                            painter.drawText(
                                QPointF(geo.page_w - geo.margin_right + 4.f,
                                        vl.y + line_fm.ascent()), sn_str);
                    }
                }
            }
        }
    }
};

} // namespace screenplay::io
