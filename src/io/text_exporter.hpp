#pragma once
// Plain text and RTF export.
//
// Interoperability, not archival: a producer asks for "the Word file", and
// .spl/.fdx are no answer. Both formats reproduce the page's indentation by
// laying elements out in real measurements — RTF in twips, text in Courier
// character columns — so a screenplay still reads as a screenplay after the
// round trip, even though neither format knows what a screenplay is.

#include "../layout/layout_engine.hpp"
#include "../model/model.hpp"
#include "../production/scene_numbering.hpp"

#include <QString>

#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace screenplay::io {

namespace detail {

/// Courier is monospaced at 10 characters per inch, so a point measurement
/// converts to a column count exactly — which is what makes plain text able to
/// hold screenplay indentation at all.
inline int columns_from_points(float points) {
    return (int)(points / 7.2f + 0.5f);
}

inline std::string spaces(int count) {
    return count > 0 ? std::string((size_t)count, ' ') : std::string();
}

/// Unicode-aware, and deliberately the same call the layout engine makes: a
/// byte-wise uppercase would leave "O Último Verão" as "O úLTIMO VERãO", so
/// the export would disagree with the page for any accented title.
inline std::string to_upper(const std::string& text) {
    return QString::fromStdString(text).toUpper().toStdString();
}

/// Scene labels keyed by block, so exports carry locked numbers like the page.
inline std::map<size_t, std::string> scene_labels(const Script& script) {
    std::map<size_t, std::string> out;
    for (const auto& scene : production::scene_numbers(script))
        out[scene.block_idx] = scene.label;
    return out;
}

} // namespace detail

// ── Plain text (.txt) ────────────────────────────────────────────────────────
class TextExporter {
public:
    static std::string to_string(const Script& script) {
        const auto geo    = layout::PageGeometry::us_letter();
        const auto labels = detail::scene_labels(script);

        std::string out;
        if (script.title_page.enabled) out += title_page_text(script.title_page);

        for (size_t i = 0; i < script.blocks.size(); ++i) {
            const auto& block = script.blocks[i];
            const auto  fmt   = layout::format_for(block.type);

            if (block.page_break_before && !out.empty()) out += "\f\n";

            std::string text = fmt.uppercase
                ? detail::to_upper(block.text) : block.text;

            if (block.type == BlockType::SceneHeading) {
                const auto label = labels.find(i);
                if (label != labels.end() && !label->second.empty())
                    text = label->second + ". " + text;
            }

            const int indent = detail::columns_from_points(fmt.left);
            if (fmt.space_before > 0.f && !out.empty()) out += "\n";
            out += detail::spaces(indent) + text + "\n";
        }
        return out;
    }

    static void write(const Script& script, const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open: " + path.string());
        file << to_string(script);
    }

private:
    static std::string title_page_text(const TitlePage& page) {
        std::string out;
        if (!page.title.empty())
            out += detail::to_upper(page.title) + "\n\n";
        out += (page.credit_type.empty() ? "Written by" : page.credit_type);
        out += "\n\n";
        for (const auto& author : page.authors) out += author + "\n";
        if (!page.based_on.empty()) out += "\n" + page.based_on + "\n";
        const auto contact = page.contact_block();
        if (!contact.empty()) {
            out += "\n";
            for (const auto& line : contact) out += line + "\n";
        }
        return out + "\f\n";
    }
};

// ── Rich Text Format (.rtf) ──────────────────────────────────────────────────
// Written by hand rather than via QTextDocument because the indentation has to
// be the screenplay's own measurements: RTF's \li is in twips (1/1440 inch),
// and the layout's insets are in points, so one multiplication carries the
// exact page geometry into Word.
class RtfExporter {
public:
    static std::string to_string(const Script& script) {
        const auto geo    = layout::PageGeometry::us_letter();
        const auto labels = detail::scene_labels(script);

        std::string out =
            "{\\rtf1\\ansi\\ansicpg1252\\deff0"
            "{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier Prime;}"
            "{\\f1\\fmodern\\fcharset0 Courier New;}}\n"
            "\\paperw12240\\paperh15840"        // US Letter, in twips
            "\\margl" + twips(geo.margin_left) +
            "\\margr" + twips(geo.margin_right) +
            "\\margt" + twips(geo.margin_top) +
            "\\margb" + twips(geo.margin_bot) + "\n"
            "\\f0\\fs24\n";                     // 12 pt (RTF counts half-points)

        if (script.title_page.enabled) out += title_page_rtf(script.title_page);

        for (size_t i = 0; i < script.blocks.size(); ++i) {
            const auto& block = script.blocks[i];
            const auto  fmt   = layout::format_for(block.type);

            out += "\\pard";
            if (block.page_break_before && i > 0) out += "\\page";

            // Insets are measured from the page edge; RTF measures from the
            // margin, so the margin has to come back out of both sides.
            out += "\\li" + twips(fmt.left  - geo.margin_left);
            out += "\\ri" + twips(fmt.right - geo.margin_right);
            if (fmt.right_align)       out += "\\qr";
            else if (fmt.center_align) out += "\\qc";
            if (fmt.space_before > 0.f) out += "\\sb" + twips(fmt.space_before);

            std::string text = fmt.uppercase
                ? detail::to_upper(block.text) : block.text;
            if (block.type == BlockType::SceneHeading) {
                const auto label = labels.find(i);
                if (label != labels.end() && !label->second.empty())
                    text = label->second + ". " + text;
            }
            out += " " + escape(text) + "\\par\n";
        }
        return out + "}\n";
    }

    static void write(const Script& script, const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open: " + path.string());
        file << to_string(script);
    }

private:
    static std::string twips(float points) {
        return std::to_string((int)(points * 20.f + 0.5f));
    }

    /// RTF is ASCII: braces and backslashes are syntax, and anything above
    /// 0x7F has to be re-encoded, or an accented Portuguese name arrives in
    /// Word as mojibake.
    static std::string escape(const std::string& text) {
        std::string out;
        for (size_t i = 0; i < text.size(); ++i) {
            const unsigned char c = (unsigned char)text[i];
            if (c == '\\' || c == '{' || c == '}') { out += '\\'; out += (char)c; }
            else if (c < 0x80)                     { out += (char)c; }
            else {
                // Decode the UTF-8 sequence to a code point, then emit RTF's
                // \uN escape with a '?' fallback for readers that ignore it.
                unsigned cp = 0; int extra = 0;
                if      ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; extra = 1; }
                else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; extra = 2; }
                else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; extra = 3; }
                else                         { continue; }
                for (int k = 0; k < extra && i + 1 < text.size(); ++k)
                    cp = (cp << 6) | ((unsigned char)text[++i] & 0x3Fu);
                out += "\\u" + std::to_string((int)(short)cp) + "?";
            }
        }
        return out;
    }

    static std::string title_page_rtf(const TitlePage& page) {
        std::string out = "\\pard\\qc\\sb2880 ";
        if (!page.title.empty())
            out += "\\b " + escape(detail::to_upper(page.title))
                 + "\\b0\\par\n";
        out += "\\pard\\qc\\sb480 "
             + escape(page.credit_type.empty() ? "Written by" : page.credit_type)
             + "\\par\n";
        for (const auto& author : page.authors)
            out += "\\pard\\qc " + escape(author) + "\\par\n";
        if (!page.based_on.empty())
            out += "\\pard\\qc\\sb480 " + escape(page.based_on) + "\\par\n";
        bool first_contact = true;
        for (const auto& line : page.contact_block()) {
            out += first_contact ? "\\pard\\ql\\sb960 " : "\\pard\\ql ";
            out += escape(line) + "\\par\n";
            first_contact = false;
        }
        return out + "\\page\n";
    }
};

} // namespace screenplay::io
