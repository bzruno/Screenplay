#pragma once
// io/importer.hpp
// Reads .spl (JSON), .fountain, and .fdx back into a Script.
// Hand-rolled parsers — no external JSON/XML library needed.

#include "../model/model.hpp"
#include "fdx_element_map.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace screenplay::io {

// Collected while importing: Final Draft element types this model does not
// represent were kept as Action; the caller may surface them to the user.
struct ImportReport {
    std::vector<std::string> downgraded_types;
};

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
                script.title_page.based_on  = read_string(tpobj, "based_on");
                script.title_page.address_2 = read_string(tpobj, "address_2");
                script.title_page.contact_1 = read_string(tpobj, "contact_1");
                script.title_page.contact_2 = read_string(tpobj, "contact_2");
                script.title_page.logo_path    = read_string(tpobj, "logo_path");
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

        script.current_revision = to_revision(read_int(json, "current_revision"));
        script.scenes_locked    = read_bool(json, "scenes_locked");

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
            block.bold_runs      = read_runs(obj, "bold_runs");
            block.italic_runs    = read_runs(obj, "italic_runs");
            block.underline_runs = read_runs(obj, "underline_runs");
            // Scene number (omitted when empty) — preserved verbatim.
            block.scene_number = read_string(obj, "scene_number");
            // Alignment override (omitted when Default) — absent reads as 0,
            // which IS Default, so pre-alignment files load unchanged.
            block.align = to_block_align(read_int(obj, "align"));
            block.note  = read_string(obj, "note");
            // Production metadata. Absent reads as unrevised / not omitted,
            // so files written before production support load unchanged.
            block.revision = to_revision(read_int(obj, "revision"));
            block.omitted  = read_bool(obj, "omitted");
            block.page_break_before = read_bool(obj, "page_break");

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

    // Reads a `"key": [[s,e],[s,e],...]` StyleRuns array (absent -> empty).
    static StyleRuns read_runs(const std::string& obj, const std::string& key) {
        StyleRuns out;
        auto kp = obj.find("\"" + key + "\"");
        if (kp == std::string::npos) return out;
        auto ab = obj.find('[', obj.find(':', kp));
        if (ab == std::string::npos) return out;
        // Find the MATCHING outer ']': since entries are "[a, b]" pairs, the
        // outer array closes at the ']' that follows the LAST pair's ']'.
        // Scan forward counting bracket depth instead of assuming the first
        // ']' found is the outer one (it would be the first pair's).
        size_t depth = 0, i = ab, outer_end = ab;
        for (; i < obj.size(); ++i) {
            if (obj[i] == '[') ++depth;
            else if (obj[i] == ']') { --depth; if (depth == 0) { outer_end = i; break; } }
        }
        size_t p = ab + 1;
        while (p < outer_end) {
            auto pb = obj.find('[', p);
            if (pb == std::string::npos || pb >= outer_end) break;
            auto pe = obj.find(']', pb);
            if (pe == std::string::npos || pe >= outer_end) break;
            std::string pair = obj.substr(pb + 1, pe - pb - 1);
            auto comma = pair.find(',');
            if (comma != std::string::npos) {
                try {
                    size_t s = (size_t)std::stoull(pair.substr(0, comma));
                    size_t e = (size_t)std::stoull(pair.substr(comma + 1));
                    if (s < e) out.emplace_back(s, e);
                } catch (...) { /* malformed pair — skip */ }
            }
            p = pe + 1;
        }
        return out;
    }

    // Narrows an arbitrary on-disk integer to a valid BlockAlign. Anything
    // out of range (a corrupt or future-version file) degrades to Default
    // rather than producing an invalid enum value.
    static BlockAlign to_block_align(long long v) {
        switch (v) {
        case 1:  return BlockAlign::Left;
        case 2:  return BlockAlign::Center;
        case 3:  return BlockAlign::Right;
        default: return BlockAlign::Default;
        }
    }

    // Same narrowing for the revision ladder: a colour this build does not
    // know reads as unrevised rather than as an invalid enum.
    static Revision to_revision(long long v) {
        return (v > 0 && v < kRevisionCount) ? static_cast<Revision>(v)
                                             : Revision::None;
    }

    // `"key": true` — absent or anything else reads as false.
    static bool read_bool(const std::string& obj, const std::string& key) {
        const auto kp = obj.find("\"" + key + "\"");
        if (kp == std::string::npos) return false;
        const auto colon = obj.find(':', kp);
        if (colon == std::string::npos) return false;
        return obj.compare(obj.find_first_not_of(" \t\r\n", colon + 1), 4,
                           "true") == 0;
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
// Reads Final Draft .fdx. Hand-rolled, forward-only scanner: it always makes
// progress past each <Paragraph>, so a truncated or malformed file can never
// loop or hang — it stops early and returns whatever parsed cleanly.
class FDXImporter {
public:
    static Script read(const std::filesystem::path& path,
                       ImportReport* report = nullptr) {
        return parse(read_file(path), report);
    }

    static Script parse(const std::string& xml, ImportReport* report = nullptr) {
        Script script;

        // ── Title page ────────────────────────────────────────────────────
        auto tp_start = xml.find("<TitlePage>");
        auto tp_end   = xml.find("</TitlePage>");
        if (tp_start != std::string::npos && tp_end != std::string::npos) {
            std::string tp_xml = xml.substr(tp_start, tp_end - tp_start + 12);
            auto read_tp_para = [&](const char* type) -> std::string {
                std::string search = std::string("Type=\"") + type + "\"";
                size_t p = tp_xml.find(search);
                if (p == std::string::npos) return {};
                std::string text, style; size_t next;
                return next_text_run(tp_xml, p, tp_xml.size(), text, style, next)
                    ? text : std::string{};
            };
            script.title_page.title        = read_tp_para("Title");
            script.title_page.credit_type  = read_tp_para("Credit");
            script.title_page.contact_left = read_tp_para("Contact");
            {
                size_t p = 0;
                const std::string search = "Type=\"Author\"";
                while ((p = tp_xml.find(search, p)) != std::string::npos) {
                    std::string text, style; size_t next;
                    if (next_text_run(tp_xml, p, tp_xml.size(), text, style, next))
                        script.title_page.authors.push_back(text);
                    p += search.size();
                }
            }
            script.title_page.enabled = !script.title_page.title.empty()
                                     || !script.title_page.authors.empty();
        }

        // ── Content paragraphs ────────────────────────────────────────────
        size_t pos = (tp_end != std::string::npos) ? tp_end + 12 : 0;

        while (pos < xml.size()) {
            size_t pp = xml.find("<Paragraph", pos);
            if (pp == std::string::npos) break;

            size_t tag_end = xml.find('>', pp);
            if (tag_end == std::string::npos) break;   // truncated opening tag

            const std::string open_tag = xml.substr(pp, tag_end - pp);
            const std::string type_str  = attr_value(open_tag, "Type");
            const std::string number    = attr_value(open_tag, "Number");
            const std::string alignment = attr_value(open_tag, "Alignment");
            const std::string dual      = attr_value(open_tag, "DualDialogue");

            // Bound the run search at this paragraph's close (or EOF), and
            // before any nested <ScriptNote> so a production note's own <Text>
            // is not absorbed into the element's dialogue/action text. The
            // ScriptNote search is bounded to THIS paragraph — a whole-file
            // find here would be O(paragraphs × file size) and freeze on large
            // scripts.
            size_t para_close = xml.find("</Paragraph>", tag_end);
            size_t limit = (para_close == std::string::npos) ? xml.size()
                                                             : para_close;
            std::string_view region(xml.data() + tag_end, limit - tag_end);
            size_t note = region.find("<ScriptNote");
            const size_t note_start = (note == std::string_view::npos)
                                          ? std::string::npos : tag_end + note;
            if (note != std::string_view::npos) limit = note_start;

            // The note's own <Text> runs, read from the region the loop above
            // deliberately stops before. Previously this content was dropped
            // on the floor; now it round-trips into Block::note.
            std::string note_text;
            if (note_start != std::string::npos) {
                const size_t note_end = (para_close == std::string::npos)
                                            ? xml.size() : para_close;
                size_t ncur = note_start;
                std::string ntxt, nstyle; size_t nnext;
                while (next_text_run(xml, ncur, note_end, ntxt, nstyle, nnext)) {
                    note_text += ntxt;
                    ncur = nnext;
                }
            }

            // Concatenate EVERY <Text> run in order, tracking each run's own
            // byte range within the concatenated text so its Style (if any)
            // becomes a real per-character range — not a whole-block flag —
            // matching how the editor itself now stores formatting.
            std::string text;
            StyleRuns bold_runs, italic_runs, underline_runs;
            size_t cur = tag_end + 1;
            std::string run_text, run_style; size_t next;
            while (next_text_run(xml, cur, limit, run_text, run_style, next)) {
                const size_t rs = text.size();
                text += run_text;
                const size_t re = text.size();
                if (rs < re) {
                    if (run_style.find("Bold")      != std::string::npos) style_add(bold_runs,      rs, re);
                    if (run_style.find("Italic")    != std::string::npos) style_add(italic_runs,    rs, re);
                    if (run_style.find("Underline") != std::string::npos) style_add(underline_runs, rs, re);
                }
                cur = next;
            }

            bool known = false;
            BlockType bt = fdx_block_type(type_str, known);
            if (bt == BlockType::Dialogue && !dual.empty() && dual != "No")
                bt = BlockType::DualDialogue;
            if (!known && !type_str.empty() && report)
                report->downgraded_types.push_back(type_str);

            Block b{ bt, text, script.next_id++ };
            if (!number.empty())    b.scene_number = number;
            if (!note_text.empty()) b.note = note_text;
            // Final Draft's own Paragraph Alignment attribute. Only honoured
            // for the types that support an override, so an FDX that aligns a
            // Character cue can't smuggle in a broken layout.
            if (!alignment.empty() && supports_alignment(bt)) {
                if      (alignment == "Left")   b.align = BlockAlign::Left;
                else if (alignment == "Center") b.align = BlockAlign::Center;
                else if (alignment == "Right")  b.align = BlockAlign::Right;
            }
            b.bold_runs      = std::move(bold_runs);
            b.italic_runs    = std::move(italic_runs);
            b.underline_runs = std::move(underline_runs);
            script.blocks.push_back(std::move(b));

            // Guaranteed forward progress → never loops on malformed input.
            size_t advance = (para_close == std::string::npos)
                               ? limit : (para_close + 12);
            pos = (advance > pp) ? advance : pp + 1;
        }

        if (script.blocks.empty())
            script.append(BlockType::SceneHeading);

        return script;
    }

private:
    // Value of attribute `name` within a start-tag string, or "" if absent.
    static std::string attr_value(const std::string& tag, const char* name) {
        const std::string key = std::string(name) + "=\"";
        size_t p = tag.find(key);
        if (p == std::string::npos) return {};
        p += key.size();
        size_t q = tag.find('"', p);
        if (q == std::string::npos) return {};
        return tag.substr(p, q - p);
    }

    // Reads the next <Text …>…</Text> run in [from, limit). Accepts any
    // attributes on the tag (e.g. Style="Bold"). Returns false when none
    // remains. `out_style` receives the run's Style attribute (may be empty),
    // `next` the offset just past </Text>.
    static bool next_text_run(const std::string& xml, size_t from, size_t limit,
                              std::string& out_text, std::string& out_style,
                              size_t& next) {
        size_t ts = xml.find("<Text", from);
        if (ts == std::string::npos || ts >= limit) return false;
        size_t gt = xml.find('>', ts);
        if (gt == std::string::npos || gt >= limit) return false;

        out_style = attr_value(xml.substr(ts, gt - ts), "Style");

        if (gt > ts && xml[gt - 1] == '/') {   // self-closing <Text/>
            out_text.clear();
            next = gt + 1;
            return true;
        }
        size_t te = xml.find("</Text>", gt + 1);
        if (te == std::string::npos || te > limit) return false;
        out_text = xml_unescape(xml.substr(gt + 1, te - gt - 1));
        next = te + 7;   // strlen("</Text>")
        return true;
    }

    // Appends a Unicode code point to `out` as UTF-8.
    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFF) {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    static std::string xml_unescape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] != '&') { out += s[i++]; continue; }

            if      (s.compare(i, 5, "&amp;")  == 0) { out += '&';  i += 5; continue; }
            else if (s.compare(i, 4, "&lt;")   == 0) { out += '<';  i += 4; continue; }
            else if (s.compare(i, 4, "&gt;")   == 0) { out += '>';  i += 4; continue; }
            else if (s.compare(i, 6, "&quot;") == 0) { out += '"';  i += 6; continue; }
            else if (s.compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; continue; }

            // Numeric character references: &#233;  or  &#x1F600;
            if (s.compare(i, 2, "&#") == 0) {
                size_t semi = s.find(';', i + 2);
                if (semi != std::string::npos && semi - i <= 12) {
                    const bool hex = (i + 2 < s.size()) &&
                                     (s[i + 2] == 'x' || s[i + 2] == 'X');
                    const size_t start = i + (hex ? 3 : 2);
                    const std::string digits = s.substr(start, semi - start);
                    if (!digits.empty()) {
                        try {
                            unsigned long cp =
                                std::stoul(digits, nullptr, hex ? 16 : 10);
                            append_utf8(out, static_cast<uint32_t>(cp));
                            i = semi + 1;
                            continue;
                        } catch (...) { /* malformed → emit literally */ }
                    }
                }
            }
            out += s[i++];   // stray '&' — keep as-is
        }
        return out;
    }
};

} // namespace screenplay::io
