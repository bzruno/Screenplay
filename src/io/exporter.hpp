#pragma once
#include "../model/model.hpp"
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>

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
            if (!tp.contact_left.empty()) out += "Contact: " + tp.contact_left + "\n";
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
            tp_para("Contact", tp.contact_left);
            xml += "  </Content>\n";
            xml += "</TitlePage>\n";
        }

        xml += "<Content>\n";

        for (const auto& b : script.blocks) {
            const char* elem = "Action";
            switch (b.type) {
            case BlockType::SceneHeading:  elem = "Scene Heading"; break;
            case BlockType::Character:     elem = "Character";     break;
            case BlockType::Parenthetical: elem = "Parenthetical"; break;
            case BlockType::Dialogue:      elem = "Dialogue";      break;
            case BlockType::Transition:    elem = "Transition";    break;
            case BlockType::DualDialogue:  elem = "Dialogue";      break;
            default: break;
            }
            xml += "  <Paragraph Type=\"";
            xml += elem;
            xml += "\">\n    <Text>";
            xml += xml_escape(b.text);
            xml += "</Text>\n";
            if (b.is_bold_)      xml += "    <Bold>YES</Bold>\n";
            if (b.is_italic_)    xml += "    <Italic>YES</Italic>\n";
            if (b.is_underline_) xml += "    <Underline>YES</Underline>\n";
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
        j += "    \"contact_left\": \"" + json_escape(tp.contact_left) + "\"\n";
        j += "  },\n";
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
            if (b.is_bold_)      j += ", \"bold\": true";
            if (b.is_italic_)    j += ", \"italic\": true";
            if (b.is_underline_) j += ", \"underline\": true";
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
