#pragma once
// io/importer.hpp
// Reads .spl (JSON), .fountain, and .fdx back into a Script.
// Hand-rolled parsers — no external JSON/XML library needed.

#include "../model/model.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cctype>

namespace screenplay::io {

// ── Utilities ─────────────────────────────────────────────────────────────────
static inline std::string trim(std::string s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [](unsigned char c){ return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c){ return !std::isspace(c); }).base(),
            s.end());
    return s;
}

static inline std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + p.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ── JSON deserializer ─────────────────────────────────────────────────────────
// Matches exactly the schema written by JsonSerializer.
class JsonDeserializer {
public:
    static Script read(const std::filesystem::path& path) {
        return parse(read_file(path));
    }

    static Script parse(const std::string& json) {
        Script script;

        // Parse title_page object if present
        auto tp_pos = json.find("\"title_page\"");
        if (tp_pos != std::string::npos) {
            auto tp_ob = json.find('{', tp_pos + 12);
            auto tp_cb = json.find('}', tp_ob);
            if (tp_ob != std::string::npos && tp_cb != std::string::npos) {
                std::string tpobj = json.substr(tp_ob, tp_cb - tp_ob + 1);
                // enabled flag
                auto ep = tpobj.find("\"enabled\"");
                if (ep != std::string::npos) {
                    auto cp = tpobj.find(':', ep);
                    size_t vp = cp + 1;
                    while (vp < tpobj.size() && std::isspace((unsigned char)tpobj[vp])) ++vp;
                    script.title_page.enabled = (tpobj.substr(vp, 4) == "true");
                }
                script.title_page.title        = read_string(tpobj, "title");
                script.title_page.credit_type  = read_string(tpobj, "credit_type");
                script.title_page.contact_left = read_string(tpobj, "contact_left");
                // Parse authors array
                auto arr_p = tpobj.find("\"authors\"");
                if (arr_p != std::string::npos) {
                    auto ab = tpobj.find('[', arr_p);
                    auto ae = tpobj.find(']', ab == std::string::npos ? 0 : ab);
                    if (ab != std::string::npos && ae != std::string::npos) {
                        size_t ai = ab + 1;
                        while (ai < ae) {
                            auto q1 = tpobj.find('"', ai);
                            if (q1 == std::string::npos || q1 >= ae) break;
                            std::string author = read_string_from_quote(tpobj, q1);
                            if (!author.empty()) script.title_page.authors.push_back(author);
                            ai = tpobj.find(',', q1 + 1);
                            if (ai == std::string::npos) break;
                            ++ai;
                        }
                    }
                }
            }
        }

        // Find "blocks" array
        auto blocks_pos = json.find("\"blocks\"");
        if (blocks_pos == std::string::npos)
            throw std::runtime_error("JSON: missing 'blocks' field");

        auto arr_start = json.find('[', blocks_pos);
        auto arr_end   = json.rfind(']');
        if (arr_start == std::string::npos || arr_end == std::string::npos)
            throw std::runtime_error("JSON: malformed blocks array");

        // Iterate objects { ... }
        size_t pos = arr_start + 1;
        while (pos < arr_end) {
            auto ob = json.find('{', pos);
            if (ob == std::string::npos || ob >= arr_end) break;
            auto cb = json.find('}', ob);
            if (cb == std::string::npos) break;

            std::string obj = json.substr(ob, cb - ob + 1);

            Block block;
            block.type = static_cast<BlockType>(read_int(obj, "type"));
            block.id   = static_cast<uint32_t>(read_int(obj, "id"));
            block.text = read_string(obj, "text");
            // Bold / italic (omitted when false)
            {
                auto bp = obj.find("\"bold\"");
                if (bp != std::string::npos) {
                    size_t cp = obj.find(':', bp) + 1;
                    while (cp < obj.size() && std::isspace((unsigned char)obj[cp])) ++cp;
                    block.is_bold_ = (obj.substr(cp, 4) == "true");
                }
                auto ip = obj.find("\"italic\"");
                if (ip != std::string::npos) {
                    size_t cp = obj.find(':', ip) + 1;
                    while (cp < obj.size() && std::isspace((unsigned char)obj[cp])) ++cp;
                    block.is_italic_ = (obj.substr(cp, 4) == "true");
                }
                auto up = obj.find("\"underline\"");
                if (up != std::string::npos) {
                    size_t cp = obj.find(':', up) + 1;
                    while (cp < obj.size() && std::isspace((unsigned char)obj[cp])) ++cp;
                    block.is_underline_ = (obj.substr(cp, 4) == "true");
                }
            }

            script.blocks.push_back(std::move(block));
            if (block.id >= script.next_id)
                script.next_id = block.id + 1;

            pos = cb + 1;
        }

        if (script.blocks.empty())
            script.append(BlockType::SceneHeading);

        return script;
    }

private:
    // Read a JSON string starting AT the opening quote (pos points to '"')
    static std::string read_string_from_quote(const std::string& s, size_t q1) {
        std::string out;
        size_t i = q1 + 1;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') break;
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                default:   out += e;
                }
            } else { out += c; }
        }
        return out;
    }

    static long long read_int(const std::string& obj, const std::string& key) {
        auto kp = obj.find("\"" + key + "\"");
        if (kp == std::string::npos) return 0;
        auto cp = obj.find(':', kp);
        if (cp == std::string::npos) return 0;
        size_t vp = cp + 1;
        while (vp < obj.size() && std::isspace((unsigned char)obj[vp])) ++vp;
        return std::stoll(obj.substr(vp));
    }

    static std::string read_string(const std::string& obj,
                                   const std::string& key) {
        auto kp = obj.find("\"" + key + "\"");
        if (kp == std::string::npos) return {};
        auto q1 = obj.find('"', obj.find(':', kp) + 1);
        if (q1 == std::string::npos) return {};
        std::string out;
        size_t i = q1 + 1;
        while (i < obj.size()) {
            char c = obj[i++];
            if (c == '"') break;
            if (c == '\\' && i < obj.size()) {
                char e = obj[i++];
                switch (e) {
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                default:   out += e;
                }
            } else {
                out += c;
            }
        }
        return out;
    }
};

// ── Fountain importer ─────────────────────────────────────────────────────────
// Implements a subset of the Fountain spec sufficient for round-trip.
// https://fountain.io/syntax
class FountainImporter {
public:
    static Script read(const std::filesystem::path& path) {
        return parse(read_file(path));
    }

    static Script parse(const std::string& src) {
        Script script;

        // Parse Fountain title page header (key: value pairs before first blank line)
        std::string body = src;
        {
            // Fountain spec: title block is a key:value section at start, ends at first blank line
            size_t first_blank = src.find("\n\n");
            if (first_blank != std::string::npos) {
                std::string header = src.substr(0, first_blank);
                bool is_header = true;
                // Header must contain at least one "Key:" pattern and no scene keywords
                for (const auto& line : split_lines(header)) {
                    auto colon = line.find(':');
                    if (colon == std::string::npos || colon == 0) { is_header = false; break; }
                }
                if (is_header && !header.empty()) {
                    auto parse_kv = [&](const std::string& key) -> std::string {
                        size_t p = header.find(key + ":");
                        if (p == std::string::npos) return {};
                        auto nl = header.find('\n', p);
                        std::string v = header.substr(p + key.size() + 1,
                            (nl == std::string::npos ? header.size() : nl) - p - key.size() - 1);
                        return trim(v);
                    };
                    script.title_page.title        = parse_kv("Title");
                    script.title_page.credit_type  = parse_kv("Credit");
                    script.title_page.contact_left = parse_kv("Contact");
                    // Collect all Author: lines
                    {
                        std::string remaining = header;
                        size_t p = 0;
                        while ((p = remaining.find("Author:", p)) != std::string::npos) {
                            auto nl = remaining.find('\n', p);
                            std::string v = remaining.substr(p + 7,
                                (nl == std::string::npos ? remaining.size() : nl) - p - 7);
                            std::string a = trim(v);
                            if (!a.empty()) script.title_page.authors.push_back(a);
                            p = (nl == std::string::npos ? remaining.size() : nl + 1);
                        }
                    }
                    script.title_page.enabled = !script.title_page.title.empty()
                                             || !script.title_page.authors.empty();
                    body = src.substr(first_blank + 2);
                }
            }
        }

        // Split into paragraphs (double newline separated)
        std::vector<std::string> paras;
        std::istringstream ss(body);
        std::string line, block_text;

        auto flush = [&] {
            auto t = trim(block_text);
            if (!t.empty()) paras.push_back(t);
            block_text.clear();
        };

        std::string prev_line;
        while (std::getline(ss, line)) {
            // Normalize CRLF
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty()) {
                flush();
            } else {
                if (!block_text.empty()) block_text += '\n';
                block_text += line;
            }
        }
        flush();

        for (const auto& para : paras) {
            Block b;
            b.id = script.next_id++;

            // Scene heading: starts with INT. EXT. INT/EXT etc., or forced with .
            if (is_scene_heading(para)) {
                b.type = BlockType::SceneHeading;
                b.text = para.front() == '.' ? para.substr(1) : para;
            }
            // Transition: ends with TO: or is forced with >
            else if (is_transition(para)) {
                b.type = BlockType::Transition;
                b.text = (para.front() == '>') ? trim(para.substr(1)) : para;
            }
            // Character: all uppercase, optionally ending with ()
            else if (is_character(para)) {
                b.type = BlockType::Character;
                std::string name = para;
                if (name.front() == '@') name = name.substr(1);
                b.text = trim(name);
            }
            // Parenthetical
            else if (para.front() == '(' && para.back() == ')') {
                b.type = BlockType::Parenthetical;
                b.text = para.substr(1, para.size() - 2);
            }
            // Centered action (ignored — treat as action)
            else {
                b.type = BlockType::Action;
                b.text = para;
            }

            script.blocks.push_back(std::move(b));
        }

        // Post-process: detect dialogue blocks (block following a Character)
        for (size_t i = 1; i < script.blocks.size(); ++i) {
            if (script.blocks[i - 1].type == BlockType::Character &&
                script.blocks[i].type     == BlockType::Action) {
                script.blocks[i].type = BlockType::Dialogue;
            }
            if (script.blocks[i - 1].type == BlockType::Parenthetical &&
                script.blocks[i].type     == BlockType::Action) {
                // Check if there's a character before the parenthetical
                if (i >= 2 && (script.blocks[i - 2].type == BlockType::Character
                             || script.blocks[i - 2].type == BlockType::Dialogue))
                    script.blocks[i].type = BlockType::Dialogue;
            }
        }

        if (script.blocks.empty())
            script.append(BlockType::SceneHeading);

        return script;
    }

private:
    static std::vector<std::string> split_lines(const std::string& s) {
        std::vector<std::string> lines;
        std::istringstream ss(s);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return lines;
    }

    static bool is_scene_heading(const std::string& s) {
        if (s.empty()) return false;
        if (s.front() == '.') return true;
        auto up = s.substr(0, std::min(s.size(), size_t(10)));
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        return up.rfind("INT.", 0) == 0
            || up.rfind("EXT.", 0) == 0
            || up.rfind("INT/EXT", 0) == 0
            || up.rfind("I/E ", 0) == 0
            || up.rfind("EST.", 0) == 0;
    }

    static bool is_transition(const std::string& s) {
        if (s.front() == '>') return true;
        if (s.size() > 3 && s.substr(s.size() - 3) == "TO:") return true;
        if (s.size() > 4 && s.substr(s.size() - 4) == "OUT:") return true;
        return false;
    }

    static bool is_character(const std::string& s) {
        if (s.front() == '@') return true;
        // All uppercase and no lowercase letters
        bool has_alpha = false;
        for (unsigned char c : s) {
            if (std::isalpha(c)) {
                has_alpha = true;
                if (std::islower(c)) return false;
            }
        }
        return has_alpha;
    }
};

// ── FDX importer ──────────────────────────────────────────────────────────────
// Reads the XML written by FDXExporter. Minimal parser.
class FDXImporter {
public:
    static Script read(const std::filesystem::path& path) {
        return parse(read_file(path));
    }

    static Script parse(const std::string& xml) {
        Script script;
        size_t pos = 0;

        // Parse TitlePage block if present
        auto tp_start = xml.find("<TitlePage>");
        auto tp_end   = xml.find("</TitlePage>");
        if (tp_start != std::string::npos && tp_end != std::string::npos) {
            std::string tp_xml = xml.substr(tp_start, tp_end - tp_start + 12);
            auto read_tp_para = [&](const char* type) -> std::string {
                std::string search = std::string("Type=\"") + type + "\"";
                auto p = tp_xml.find(search);
                if (p == std::string::npos) return {};
                auto tx = tp_xml.find("<Text>", p);
                auto te = tp_xml.find("</Text>", p);
                if (tx == std::string::npos || te == std::string::npos) return {};
                return xml_unescape(tp_xml.substr(tx + 6, te - tx - 6));
            };
            script.title_page.title        = read_tp_para("Title");
            script.title_page.credit_type  = read_tp_para("Credit");
            script.title_page.contact_left = read_tp_para("Contact");
            // Collect all Author paragraphs
            {
                size_t p = 0;
                std::string search = "Type=\"Author\"";
                while ((p = tp_xml.find(search, p)) != std::string::npos) {
                    auto tx = tp_xml.find("<Text>", p);
                    auto te = tp_xml.find("</Text>", p);
                    if (tx != std::string::npos && te != std::string::npos)
                        script.title_page.authors.push_back(
                            xml_unescape(tp_xml.substr(tx + 6, te - tx - 6)));
                    p += search.size();
                }
            }
            script.title_page.enabled = !script.title_page.title.empty()
                                     || !script.title_page.authors.empty();
        }

        // Start parsing after </TitlePage> to avoid duplicating title content
        if (tp_end != std::string::npos) pos = tp_end + 12;

        while (pos < xml.size()) {
            // Find next <Paragraph
            auto pp = xml.find("<Paragraph", pos);
            if (pp == std::string::npos) break;

            // Read Type attribute
            auto tp = xml.find("Type=\"", pp);
            if (tp == std::string::npos) { pos = pp + 1; continue; }
            auto tq = xml.find('"', tp + 6);
            std::string type_str = xml.substr(tp + 6, tq - tp - 6);

            // Read <Text>...</Text>
            auto txp = xml.find("<Text>", pp);
            auto txe = xml.find("</Text>", pp);
            std::string text_raw = (txp != std::string::npos && txe != std::string::npos)
                ? xml.substr(txp + 6, txe - txp - 6) : "";

            // Unescape XML entities
            std::string text = xml_unescape(text_raw);

            BlockType bt = BlockType::Action;
            if (type_str == "Scene Heading")  bt = BlockType::SceneHeading;
            else if (type_str == "Character")     bt = BlockType::Character;
            else if (type_str == "Parenthetical") bt = BlockType::Parenthetical;
            else if (type_str == "Dialogue")      bt = BlockType::Dialogue;
            else if (type_str == "Transition")    bt = BlockType::Transition;

            script.blocks.push_back({ bt, text, script.next_id++ });
            pos = txe + 1;
        }

        if (script.blocks.empty())
            script.append(BlockType::SceneHeading);

        return script;
    }

private:
    static std::string xml_unescape(const std::string& s) {
        std::string out;
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '&') {
                if (s.substr(i, 5) == "&amp;")  { out += '&';  i += 5; }
                else if (s.substr(i, 4) == "&lt;")   { out += '<';  i += 4; }
                else if (s.substr(i, 4) == "&gt;")   { out += '>';  i += 4; }
                else if (s.substr(i, 6) == "&quot;") { out += '"';  i += 6; }
                else if (s.substr(i, 6) == "&apos;") { out += '\''; i += 6; }
                else { out += s[i++]; }
            } else {
                out += s[i++];
            }
        }
        return out;
    }
};

} // namespace screenplay::io
