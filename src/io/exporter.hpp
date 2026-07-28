#pragma once
#include "../model/model.hpp"
#include "fdx_element_map.hpp"
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace screenplay::io {

// ── Fountain (.fountain) ──────────────────────────────────────────────────────
class FountainExporter {
public:
    static std::string to_string(const Script& script) {
        std::string out;

        // Title page as Fountain key:value header block
        const auto& tp = script.title_page;
        if (tp.enabled) {
            if (!tp.title.empty()) out += "Title: " + tp.title + "\n";
            if (!tp.credit_type.empty()) out += "Credit: " + tp.credit_type + "\n";
            for (size_t i = 0; i < tp.authors.size(); ++i) {
                out += (i == 0 ? "Author: " : "Author: ") + tp.authors[i] + "\n";
            }
            if (!tp.based_on.empty()) out += "Source: " + tp.based_on + "\n";
            for (const auto& line : tp.contact_block())
                out += "Contact: " + line + "\n";
            out += "\n";
        }

        for (const auto& b : script.blocks) {
            switch (b.type) {
            case BlockType::SceneHeading:
                // Fountain: scene headings prefixed with .  OR auto-detected INT./EXT.
                out += b.text + "\n\n";
                break;
            case BlockType::Action:
                out += b.text + "\n\n";
                break;
            case BlockType::Character:
                out += b.text + "\n";
                break;
            case BlockType::Parenthetical:
                out += "(" + b.text + ")\n";
                break;
            case BlockType::Dialogue:
                out += b.text + "\n\n";
                break;
            case BlockType::Transition:
                out += b.text + ":\n\n";
                break;
            case BlockType::DualDialogue:
                out += b.text + "\n\n";
                break;
            }
        }
        return out;
    }

    static void write(const Script& script, const std::filesystem::path& path) {
        std::ofstream f(path);
        if (!f) throw std::runtime_error("Cannot open: " + path.string());
        f << to_string(script);
    }
};

// ── Final Draft XML (.fdx) ────────────────────────────────────────────────────
class FDXExporter {
public:
    static std::string to_string(const Script& script) {
        std::string xml;
        xml += R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>)" "\n";
        xml += R"(<FinalDraft DocumentType="Script" Version="2">)" "\n";

        // Title page
        const auto& tp = script.title_page;
        if (tp.enabled) {
            xml += "<TitlePage>\n";
            xml += "  <Content>\n";
            auto tp_para = [&](const char* type, const std::string& text) {
                if (text.empty()) return;
                xml += "    <Paragraph Type=\""; xml += type; xml += "\">\n";
                xml += "      <Text>"; xml += xml_escape(text); xml += "</Text>\n";
                xml += "    </Paragraph>\n";
            };
            tp_para("Title",   tp.title);
            tp_para("Credit",  tp.credit_type);
            for (const auto& a : tp.authors) tp_para("Author", a);
            tp_para("Source", tp.based_on);
            for (const auto& line : tp.contact_block())
                tp_para("Contact", line);
            xml += "  </Content>\n";
            xml += "</TitlePage>\n";
        }

        xml += "<Content>\n";

        for (const auto& b : script.blocks) {
            xml += "  <Paragraph Type=\"";
            xml += fdx_element_name(b.type);
            xml += "\"";
            // Scene numbers travel on the Paragraph as Final Draft expects,
            // and only for the Scene Heading that owns them. Preserved verbatim.
            if (b.type == BlockType::SceneHeading && !b.scene_number.empty()) {
                xml += " Number=\"";
                xml += xml_escape(b.scene_number);
                xml += "\"";
            }
            // Author alignment overrides map straight onto Final Draft's own
            // Paragraph Alignment attribute, so they survive a round trip
            // through FD / Fade In / WriterDuet instead of being silently
            // flattened. Default is omitted (that IS "no override").
            if (b.align != BlockAlign::Default) {
                xml += " Alignment=\"";
                xml += b.align == BlockAlign::Left   ? "Left"
                     : b.align == BlockAlign::Center ? "Center"
                                                     : "Right";
                xml += "\"";
            }
            // Type stays "Dialogue" so any reader still gets valid dialogue;
            // this extra attribute is what lets us restore the simultaneous
            // pairing on import instead of silently flattening it.
            if (b.type == BlockType::DualDialogue)
                xml += " DualDialogue=\"Yes\"";
            xml += ">\n";
            // One <Text> run per style-homogeneous segment of the block's
            // text (Style="Bold+Italic" etc, the attribute Final Draft, Fade
            // In and WriterDuet all read) — so a bolded WORD inside an
            // otherwise-plain line round-trips as that same partial styling,
            // not as the whole paragraph.
            xml += text_runs_xml(b);
            // Author notes travel as Final Draft's own <ScriptNote>, nested in
            // the paragraph they annotate — the same element the importer
            // reads back (and which it used to discard outright).
            if (!b.note.empty()) {
                xml += "    <ScriptNote><Text>";
                xml += xml_escape(b.note);
                xml += "</Text></ScriptNote>\n";
            }
            xml += "  </Paragraph>\n";
        }

        xml += "</Content>\n</FinalDraft>\n";
        return xml;
    }

    static void write(const Script& script, const std::filesystem::path& path) {
        std::ofstream f(path);
        if (!f) throw std::runtime_error("Cannot open: " + path.string());
        f << to_string(script);
    }

private:
    // Splits `b.text` into style-homogeneous segments (cut at every
    // bold/italic/underline range boundary that falls inside it) and emits
    // one `<Text Style="…">…</Text>` per segment. Style is omitted when a
    // segment is unstyled. Always emits at least one <Text> (possibly empty).
    static std::string text_runs_xml(const Block& b) {
        std::vector<size_t> cuts = { 0, b.text.size() };
        auto add_cuts = [&](const StyleRuns& runs) {
            for (const auto& [rs, re] : runs) { cuts.push_back(rs); cuts.push_back(re); }
        };
        add_cuts(b.bold_runs);
        add_cuts(b.italic_runs);
        add_cuts(b.underline_runs);
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

        std::string out;
        for (size_t i = 0; i + 1 < cuts.size(); ++i) {
            const size_t s = cuts[i], e = cuts[i + 1];
            if (s >= e) continue;
            out += "    <Text";
            out += run_style_attr(style_covers(b.bold_runs,      s, e),
                                  style_covers(b.italic_runs,    s, e),
                                  style_covers(b.underline_runs, s, e));
            out += ">";
            out += xml_escape(b.text.substr(s, e - s));
            out += "</Text>\n";
        }
        if (out.empty()) out = "    <Text></Text>\n";   // empty block text
        return out;
    }

    // Builds the ` Style="…"` attribute, or "" when unstyled. Multiple styles
    // are joined with '+', matching Final Draft's AdornmentStyle string
    // (e.g. Style="Bold+Underline").
    static std::string run_style_attr(bool bold, bool italic, bool underline) {
        std::string style;
        auto add = [&](const char* name) {
            if (!style.empty()) style += '+';
            style += name;
        };
        if (bold)      add("Bold");
        if (italic)    add("Italic");
        if (underline) add("Underline");
        return style.empty() ? std::string{} : " Style=\"" + style + "\"";
    }

    static std::string xml_escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
            }
        }
        return out;
    }
};

// ── Internal JSON format ──────────────────────────────────────────────────────
class JsonSerializer {
public:
    static std::string serialize(const Script& script) {
        const auto& tp = script.title_page;
        std::string j = "{\n  \"version\": 1,\n";
        j += "  \"title_page\": {\n";
        j += "    \"enabled\": "; j += tp.enabled ? "true" : "false"; j += ",\n";
        j += "    \"title\": \""        + json_escape(tp.title)        + "\",\n";
        j += "    \"credit_type\": \"" + json_escape(tp.credit_type)  + "\",\n";
        j += "    \"authors\": [";
        for (size_t i = 0; i < tp.authors.size(); ++i) {
            if (i) j += ", ";
            j += "\"" + json_escape(tp.authors[i]) + "\"";
        }
        j += "],\n";
        j += "    \"contact_left\": \"" + json_escape(tp.contact_left) + "\",\n";
        j += "    \"based_on\": \""     + json_escape(tp.based_on)     + "\",\n";
        j += "    \"address_2\": \""    + json_escape(tp.address_2)    + "\",\n";
        j += "    \"contact_1\": \""    + json_escape(tp.contact_1)    + "\",\n";
        j += "    \"contact_2\": \""    + json_escape(tp.contact_2)    + "\",\n";
        // Cover artwork path (empty when the cover uses a typed title).
        j += "    \"logo_path\": \""    + json_escape(tp.logo_path)    + "\"\n";
        j += "  },\n";
        // Production state, both omitted at their defaults so a script that
        // never went into production stays byte-identical to before.
        if (script.current_revision != Revision::None)
            j += "  \"current_revision\": "
               + std::to_string(static_cast<int>(script.current_revision)) + ",\n";
        if (script.scenes_locked)
            j += "  \"scenes_locked\": true,\n";
        j += "  \"blocks\": [\n";
        for (size_t i = 0; i < script.blocks.size(); ++i) {
            const auto& b = script.blocks[i];
            j += "    { \"type\": ";
            j += std::to_string(static_cast<int>(b.type));
            j += ", \"id\": ";
            j += std::to_string(b.id);
            j += ", \"text\": \"";
            j += json_escape(b.text);
            j += "\"";
            if (!b.bold_runs.empty())      j += ", \"bold_runs\": "      + runs_json(b.bold_runs);
            if (!b.italic_runs.empty())    j += ", \"italic_runs\": "    + runs_json(b.italic_runs);
            if (!b.underline_runs.empty()) j += ", \"underline_runs\": " + runs_json(b.underline_runs);
            if (!b.scene_number.empty())
                j += ", \"scene_number\": \"" + json_escape(b.scene_number) + "\"";
            // Omitted when Default, so files written before alignment existed
            // and files with no overrides stay byte-identical.
            if (b.align != BlockAlign::Default)
                j += ", \"align\": " + std::to_string(static_cast<int>(b.align));
            if (!b.note.empty())
                j += ", \"note\": \"" + json_escape(b.note) + "\"";
            if (b.revision != Revision::None)
                j += ", \"revision\": "
                   + std::to_string(static_cast<int>(b.revision));
            if (b.omitted)
                j += ", \"omitted\": true";
            if (b.page_break_before)
                j += ", \"page_break\": true";
            j += " }";
            if (i + 1 < script.blocks.size()) j += ",";
            j += "\n";
        }
        j += "  ]\n}\n";
        return j;
    }

    static void write(const Script& script, const std::filesystem::path& path) {
        std::ofstream f(path);
        if (!f) throw std::runtime_error("Cannot open: " + path.string());
        f << serialize(script);
    }

private:
    // Serializes a StyleRuns list as a JSON array of [start, end] pairs, e.g.
    // "[[0,5],[12,15]]" — the internal .spl format, not FDX, so a plain
    // nested array is fine (no need to match any external schema).
    static std::string runs_json(const StyleRuns& runs) {
        std::string out = "[";
        for (size_t i = 0; i < runs.size(); ++i) {
            if (i) out += ", ";
            out += "[" + std::to_string(runs[i].first) + ", "
                       + std::to_string(runs[i].second) + "]";
        }
        out += "]";
        return out;
    }

    static std::string json_escape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"')       out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else                out += c;
        }
        return out;
    }
};

} // namespace screenplay::io
