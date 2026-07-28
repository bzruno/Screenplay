#pragma once
// parsing/screenplay_parse.hpp
// Structural parsers for screenplay text — character cues and scene headings.
// Pure C++ / std only (no Qt), so every layer (layout, stats, database) shares
// one source of truth and produces identical results.

#include <string>
#include <cstdint>
#include <cctype>
#include <algorithm>

namespace screenplay::parse {

// ─────────────────────────────────────────────────────────────────────────────
// UTF-8 case/accent fold: lowercases ASCII and strips Latin-1 diacritics
// (Portuguese À-ÿ), leaving a comparable ASCII-ish key. Unknown multibyte
// sequences pass through unchanged. Used for accent-insensitive matching.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string fold(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        if (b < 0x80) {
            out += static_cast<char>(std::tolower(b));
            ++i;
        } else if (b == 0xC3 && i + 1 < s.size()) {
            // U+00C0..U+00FF encoded as 0xC3 0x80..0xBF.
            const unsigned int cp = 0xC0 + (static_cast<unsigned char>(s[i+1]) - 0x80);
            char base = 0;
            if      (cp==0xC0||cp==0xC1||cp==0xC2||cp==0xC3||cp==0xC4||cp==0xC5||
                     cp==0xE0||cp==0xE1||cp==0xE2||cp==0xE3||cp==0xE4||cp==0xE5) base='a';
            else if (cp==0xC7||cp==0xE7) base='c';
            else if (cp==0xC8||cp==0xC9||cp==0xCA||cp==0xCB||
                     cp==0xE8||cp==0xE9||cp==0xEA||cp==0xEB) base='e';
            else if (cp==0xCC||cp==0xCD||cp==0xCE||cp==0xCF||
                     cp==0xEC||cp==0xED||cp==0xEE||cp==0xEF) base='i';
            else if (cp==0xD1||cp==0xF1) base='n';
            else if (cp==0xD2||cp==0xD3||cp==0xD4||cp==0xD5||cp==0xD6||
                     cp==0xF2||cp==0xF3||cp==0xF4||cp==0xF5||cp==0xF6) base='o';
            else if (cp==0xD9||cp==0xDA||cp==0xDB||cp==0xDC||
                     cp==0xF9||cp==0xFA||cp==0xFB||cp==0xFC) base='u';
            else if (cp==0xDD||cp==0xFD||cp==0xFF) base='y';
            if (base) out += base;              // otherwise drop the odd glyph
            i += 2;
        } else {
            out += s[i];   // keep unknown bytes verbatim
            ++i;
        }
    }
    return out;
}

inline std::string trim(const std::string& s) {
    size_t f = s.find_first_not_of(" \t\r\n");
    if (f == std::string::npos) return {};
    size_t l = s.find_last_not_of(" \t\r\n");
    return s.substr(f, l - f + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Character cue: "JOÃO (V.O.)" → name "JOÃO", extension "V.O."
// Any parenthetical text immediately after the name is a Character Extension
// (V.O., O.S., CONT'D, PELO TELEFONE, …); statistics must group by `name` only.
// ─────────────────────────────────────────────────────────────────────────────
struct CharacterCue {
    std::string name;
    std::string extension;   // without the surrounding parentheses; may be empty
};

inline CharacterCue parse_character_cue(const std::string& text) {
    std::string name = trim(text);
    std::string extension;

    // Strip every trailing "(…)" group; the last one removed is the one that
    // sits immediately after the name, so it becomes the reported extension.
    while (!name.empty() && name.back() == ')') {
        int depth = 0;
        size_t open = std::string::npos;
        for (size_t j = name.size(); j-- > 0; ) {
            if (name[j] == ')') ++depth;
            else if (name[j] == '(') {
                if (--depth == 0) { open = j; break; }
            }
        }
        if (open == std::string::npos) break;   // unbalanced — leave as-is
        extension = trim(name.substr(open + 1, name.size() - open - 2));
        name = trim(name.substr(0, open));
    }
    return { name, extension };
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene heading: "INT. CASA DE JOÃO - NOITE"
//   int_ext  = "INT."      location = "CASA DE JOÃO"      time_of_day = "NOITE"
// ─────────────────────────────────────────────────────────────────────────────
struct SceneHeadingParts {
    std::string int_ext;       // canonical "INT." / "EXT." / "INT./EXT." / "EST."
    std::string location;      // trimmed, original casing
    std::string time_of_day;   // raw text after the last dash, trimmed
};

inline SceneHeadingParts parse_scene_heading(const std::string& heading) {
    SceneHeadingParts p;
    std::string h = trim(heading);
    const std::string folded = fold(h);

    // Prefix table, longest first so "int./ext." wins over "int.".
    struct Pre { const char* folded; const char* canonical; };
    static const Pre kPre[] = {
        {"int./ext.", "INT./EXT."}, {"ext./int.", "EXT./INT."},
        {"int/ext",   "INT./EXT."}, {"ext/int",   "EXT./INT."},
        {"i/e.",      "INT./EXT."}, {"i/e",       "INT./EXT."},
        {"int.",      "INT."},      {"ext.",      "EXT."},
        {"est.",      "EST."},      {"int ",      "INT."},
        {"ext ",      "EXT."},
    };

    size_t body_start = 0;
    for (const auto& pre : kPre) {
        const size_t n = std::char_traits<char>::length(pre.folded);
        if (folded.compare(0, n, pre.folded) == 0) {
            p.int_ext   = pre.canonical;
            body_start  = n;
            break;
        }
    }

    std::string body = trim(h.substr(std::min(body_start, h.size())));
    // Drop a leading separator left over from the prefix (". " or "- ").
    while (!body.empty() && (body.front() == '.' || body.front() == '-' ||
                             body.front() == ' '))
        body.erase(body.begin());
    body = trim(body);

    // Split location / time on the LAST spaced dash (handles hyphenated names).
    size_t dash = body.rfind(" - ");
    size_t skip = 3;
    if (dash == std::string::npos) {
        // Fall back to an en/em dash surrounded by spaces.
        for (const char* d : { " \xe2\x80\x93 ", " \xe2\x80\x94 " }) {
            size_t pos = body.rfind(d);
            if (pos != std::string::npos) { dash = pos; skip = std::char_traits<char>::length(d); break; }
        }
    }
    if (dash != std::string::npos) {
        p.location    = trim(body.substr(0, dash));
        p.time_of_day = trim(body.substr(dash + skip));
    } else {
        p.location = body;
    }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Time-of-day classification → a canonical Portuguese label, or "Outro".
// Matches accent- and case-insensitively; more specific phrases are tested
// first so "MAIS TARDE" is not swallowed by "TARDE", etc.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string classify_time_of_day(const std::string& raw) {
    const std::string f = fold(raw);
    if (f.empty()) return "Outro";
    auto has = [&](const char* kw) { return f.find(kw) != std::string::npos; };

    if (has("momentos depois") || has("moments later")) return "MOMENTOS DEPOIS";
    if (has("mais tarde")      || has("later"))         return "MAIS TARDE";
    if (has("amanhecer")       || has("dawn")   || has("sunrise")) return "AMANHECER";
    if (has("entardecer")      || has("dusk")   || has("sunset"))  return "ENTARDECER";
    if (has("crepusculo")      || has("twilight"))      return "CREP\xc3\x9aSCULO";
    if (has("continuo")        || has("continuous") || has("contin")) return "CONT\xc3\x8dNUO";
    if (has("manha")           || has("morning"))       return "MANH\xc3\x83";
    if (has("tarde")           || has("afternoon"))     return "TARDE";
    if (has("noite")           || has("night"))         return "NOITE";
    if (has("dia")             || has("day"))           return "DIA";
    return "Outro";
}

} // namespace screenplay::parse
