#pragma once
// editor/autocomplete.hpp
// SmartType system — context-aware autocomplete for professional screenwriting.
// Handles scene heading structure (prefix / location / time-of-day),
// character names, dialogue extensions, and transitions.

#include "../model/model.hpp"
#include "../config/language.hpp"
#include "../parsing/screenplay_parse.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace screenplay::editor {

// ─────────────────────────────────────────────────────────────────────────────
// Trie — fast prefix-matching with frequency ranking
// ─────────────────────────────────────────────────────────────────────────────
struct TrieNode {
    std::unordered_map<char, TrieNode> children;
    std::string full_word;
    int         frequency = 0;
    bool        terminal  = false;
};

class AutocompleteIndex {
public:
    void learn(std::string_view word) {
        if (word.empty()) return;
        std::string up = to_upper(word);
        size_t s = up.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) return;
        up = up.substr(s);
        TrieNode* node = &root_;
        for (char c : up) node = &node->children[c];
        node->terminal  = true;
        node->full_word = up;
        node->frequency++;
    }

    void reset() { root_ = TrieNode{}; }

    // Returns completions sorted by frequency, up to max_results
    std::vector<std::string> suggest(std::string_view prefix,
                                     size_t max_results = 5) const
    {
        std::string up = to_upper(prefix);
        const TrieNode* node = &root_;
        for (char c : up) {
            auto it = node->children.find(c);
            if (it == node->children.end()) return {};
            node = &it->second;
        }
        std::vector<std::pair<int,std::string>> raw;
        collect(node, raw);
        // Never offer what has already been typed in full: completing "JOE"
        // with "JOE" (or "CUT TO:" with "CUT TO:") is a no-op that leaves the
        // popup open over finished text.
        raw.erase(std::remove_if(raw.begin(), raw.end(),
                                 [&up](const auto& e){ return e.second == up; }),
                  raw.end());
        std::sort(raw.begin(), raw.end(),
                  [](const auto& a, const auto& b){
                      if (a.first != b.first) return a.first > b.first;
                      return a.second < b.second;   // stable, alphabetical ties
                  });
        std::vector<std::string> out;
        out.reserve(std::min(raw.size(), max_results));
        for (size_t i = 0; i < std::min(raw.size(), max_results); ++i)
            out.push_back(raw[i].second);
        return out;
    }

    bool empty() const { return root_.children.empty(); }

private:
    TrieNode root_;

    static std::string to_upper(std::string_view s) {
        std::string r(s);
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return r;
    }

    void collect(const TrieNode* n,
                 std::vector<std::pair<int,std::string>>& out) const {
        if (n->terminal && !n->full_word.empty() &&
                n->full_word.find_first_not_of(" \t\r\n") != std::string::npos)
            out.emplace_back(n->frequency, n->full_word);
        for (const auto& [c, child] : n->children)
            collect(&child, out);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Scene Heading SmartType
// A scene heading has the structure:  PREFIX LOCATION - TIME
//   e.g.  "INT. ESCRITÓRIO - DIA"
// We parse the current text and suggest the relevant part.
// ─────────────────────────────────────────────────────────────────────────────
class SceneHeadingSmartType {
public:
    // Fixed prefix suggestions (shown immediately on empty / partial match)
    static const std::vector<std::string>& prefixes() {
        static const std::vector<std::string> p = {
            "INT.", "EXT.", "I/E."
        };
        return p;
    }

    // Time-of-day suggestions — language-aware
    static const std::vector<std::string>& times_of_day() {
        static const std::vector<std::string> t_en = {
            "DAY", "NIGHT", "MORNING", "AFTERNOON", "EVENING",
            "DAWN", "DUSK", "LATER", "CONTINUOUS", "MOMENTS LATER"
        };
        static const std::vector<std::string> t_pt = {
            "DIA", "NOITE", "MANHÃ", "TARDE",
            "ENTARDECER", "AMANHECER", "MADRUGADA", "MAIS TARDE", "CONTÍNUO"
        };
        return (screenplay::config::LanguageConfig::current() ==
                screenplay::config::AppLanguage::English) ? t_en : t_pt;
    }

    // Learn location from a committed scene heading
    void learn_heading(std::string_view heading) {
        // Parse: PREFIX LOCATION - TIME
        std::string h(heading);
        std::transform(h.begin(), h.end(), h.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        // Strip prefix
        for (const auto& pfx : prefixes()) {
            if (h.rfind(pfx, 0) == 0) {
                h = h.substr(pfx.size());
                break;
            }
        }
        // Strip time-of-day (after " - ")
        auto dash = h.rfind(" - ");
        if (dash != std::string::npos)
            h = h.substr(0, dash);
        // Trim
        size_t s = h.find_first_not_of(' ');
        if (s != std::string::npos) h = h.substr(s);
        size_t e = h.find_last_not_of(' ');
        if (e != std::string::npos) h = h.substr(0, e + 1);
        if (!h.empty()) locations_.learn(h);
    }

    // Main suggest function: analyses current partial text up to cursor_offset
    // Returns a list of suggestions appropriate for current parse position
    std::vector<std::string> suggest(const std::string& text,
                                     size_t cursor_offset) const {
        std::string partial = text.substr(0, std::min(cursor_offset, text.size()));
        std::string up(partial);
        std::transform(up.begin(), up.end(), up.begin(),
                       [](unsigned char c){ return std::toupper(c); });

        // ── Part 1: prefix not yet typed ─────────────────────────────────
        if (up.empty()) {
            return prefixes();
        }

        // ── Which stage of "PREFIX LOCATION - TIME" are we in? ───────────
        //
        // Order matters here. A COMPLETED prefix has to be recognised before a
        // partial one, because "EXT." is simultaneously a complete prefix and
        // a leading substring of itself — testing "partial" first meant a
        // finished prefix kept re-suggesting itself forever and never advanced
        // to suggesting a location.
        bool has_prefix = false;
        std::string after_prefix;
        for (const auto& pfx : prefixes()) {
            if (up.rfind(pfx, 0) == 0) {          // starts with the FULL prefix
                has_prefix   = true;
                after_prefix = up.substr(pfx.size());
                break;
            }
        }

        if (!has_prefix) {
            // Still mid-prefix: offer only the prefixes this text could still
            // become. `p.size() > up.size()` keeps a completed prefix out (it
            // is handled above), and an empty result is the correct answer for
            // text that matches nothing — typing "A" opens a scene heading
            // that is simply not INT./EXT./I/E., so SmartType must stay shut
            // rather than offer all three regardless of what was typed.
            std::vector<std::string> res;
            for (const auto& p : prefixes())
                if (p.size() > up.size() && p.rfind(up, 0) == 0)
                    res.push_back(p);
            return res;
        }

        // Trim leading space after prefix
        size_t sp = after_prefix.find_first_not_of(' ');
        if (sp == std::string::npos) {
            // Just typed "INT. " — suggest locations
            return locations_.empty()
                ? std::vector<std::string>{}
                : locations_.suggest("");
        }
        after_prefix = after_prefix.substr(sp);

        // ── Part 2: check if " - " separator is present ──────────────────
        auto dash = after_prefix.rfind(" - ");
        if (dash != std::string::npos) {
            // ── Part 3: typing time-of-day ────────────────────────────────
            std::string time_part = after_prefix.substr(dash + 3);
            std::vector<std::string> matches;
            for (const auto& t : times_of_day())
                if (t.rfind(time_part, 0) == 0) matches.push_back(t);
            return matches;
        }

        // ── Part 2: typing location ───────────────────────────────────────
        return locations_.suggest(after_prefix);
    }

    // Build full suggestion string to insert
    // Given the current text and a chosen suggestion,
    // return the COMPLETE heading text (not just the suffix)
    static std::string build_completion(const std::string& /*current*/,
                                        const std::string& suggestion,
                                        const std::string& text_up)
    {
        // Determine which part we're completing
        std::string up = text_up;
        // Find active prefix
        std::string prefix_used;
        for (const auto& pfx : prefixes()) {
            if (up.rfind(pfx, 0) == 0) { prefix_used = pfx; break; }
        }

        if (prefix_used.empty()) {
            // Completing the prefix itself
            return suggestion + " ";
        }

        std::string after = up.substr(prefix_used.size());
        size_t sp = after.find_first_not_of(' ');

        if (sp == std::string::npos) {
            // After prefix, just typed space — completing location
            return prefix_used + " " + suggestion + " - ";
        }

        after = after.substr(sp);
        auto dash = after.rfind(" - ");
        if (dash != std::string::npos) {
            // Completing time-of-day
            std::string loc = after.substr(0, dash);
            return prefix_used + " " + loc + " - " + suggestion;
        }

        // Completing location
        return prefix_used + " " + suggestion + " - ";
    }

private:
    AutocompleteIndex locations_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Full SmartType system
// ─────────────────────────────────────────────────────────────────────────────
class AutocompleteSystem {
public:
    AutocompleteSystem() {
        seed_transitions();
    }

    // Clear all learned data and re-seed built-in entries
    void reset() {
        characters_.reset();
        transitions_.reset();
        scene_type_ = SceneHeadingSmartType{};
        seed_transitions();
    }

    // Re-seed built-in transitions with current language.
    // Call after LanguageConfig::set() to pick up the new language.
    void reseed() {
        transitions_.reset();
        seed_transitions();
    }

    // Called when a block is committed (Enter pressed)
    void train(screenplay::BlockType type, std::string_view text) {
        if (text.empty()) return;
        switch (type) {
        case screenplay::BlockType::Character:
            // Learn the NAME only — never the extension — so name suggestions
            // stay clean ("JOÃO", not "JOÃO (V.O.)").
            characters_.learn(parse::parse_character_cue(std::string(text)).name);
            break;
        case screenplay::BlockType::SceneHeading:
            scene_type_.learn_heading(text);
            break;
        case screenplay::BlockType::Transition:
            transitions_.learn(text);
            break;
        default:
            break;
        }
    }

    // Returns suggestions for the current block context
    std::vector<std::string> query(screenplay::BlockType ctx,
                                   const std::string&    text,
                                   size_t                cursor_offset) const
    {
        switch (ctx) {
        case screenplay::BlockType::SceneHeading:
            return scene_type_.suggest(text, cursor_offset);

        case screenplay::BlockType::Character: {
            // While typing a Character Extension — an unclosed "(" after the
            // name, e.g. "JOÃO (V" — suggest extensions (V.O., O.S., …).
            // Otherwise suggest learned character names.
            CharExtContext ec = char_ext_context(text, cursor_offset);
            if (ec.active) return suggest_extensions(ec.partial);
            return characters_.suggest(text);
        }

        case screenplay::BlockType::Transition:
            return transitions_.suggest(text);

        case screenplay::BlockType::Parenthetical:
            return {};   // No SmartType inside Parenthetical (wrylie) blocks.

        default:
            return {};
        }
    }

    // True when the caret sits inside an open Character Extension parenthesis.
    bool is_character_extension(const std::string& text, size_t cursor) const {
        return char_ext_context(text, cursor).active;
    }

    // Build the full Character text when accepting an extension suggestion:
    // keeps everything up to and including "(", appends the suggestion + ")".
    std::string build_character_extension(const std::string& text, size_t cursor,
                                          const std::string& suggestion) const {
        CharExtContext ec = char_ext_context(text, cursor);
        if (!ec.active) return text;
        return text.substr(0, ec.open_pos + 1) + suggestion + ")";
    }

    // The curated Character Extension list, in industry-usage order. Any other
    // text the writer types between the parentheses is accepted verbatim as a
    // custom extension — the list is a convenience, not a constraint.
    static const std::vector<std::string>& character_extensions() {
        static const std::vector<std::string> ext = {
            "V.O.", "O.S.", "O.C.", "CONT'D", "FILTERED", "PRE-LAP"
        };
        return ext;
    }

    // Extensions whose text starts with `partial` (case-insensitive), preserving
    // the curated order. An empty partial returns the full list.
    static std::vector<std::string> suggest_extensions(const std::string& partial) {
        std::string up = partial;
        std::transform(up.begin(), up.end(), up.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        std::vector<std::string> out;
        for (const auto& e : character_extensions())
            if (e.rfind(up, 0) == 0) out.push_back(e);
        return out;
    }

    // For scene headings: build the full replacement string
    std::string build_scene_completion(const std::string& current_text,
                                       const std::string& suggestion) const
    {
        std::string up = current_text;
        std::transform(up.begin(), up.end(), up.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return SceneHeadingSmartType::build_completion(current_text, suggestion, up);
    }

    bool is_scene_completion(screenplay::BlockType ctx) const {
        return ctx == screenplay::BlockType::SceneHeading;
    }

private:
    SceneHeadingSmartType scene_type_;
    AutocompleteIndex     characters_;
    AutocompleteIndex     transitions_;

    // Locates an open Character Extension parenthesis at/before the caret.
    struct CharExtContext {
        bool        active   = false;
        size_t      open_pos = 0;       // index of the '(' being completed
        std::string partial;            // text typed between '(' and the caret
    };
    static CharExtContext char_ext_context(const std::string& text, size_t cursor) {
        CharExtContext c;
        const size_t cur = std::min(cursor, text.size());
        for (size_t i = cur; i-- > 0; ) {
            if (text[i] == ')') break;              // parenthesis already closed
            if (text[i] == '(') { c.active = true; c.open_pos = i; break; }
        }
        if (c.active) c.partial = text.substr(c.open_pos + 1, cur - c.open_pos - 1);
        return c;
    }

    void seed_transitions() {
        using LC = screenplay::config::LanguageConfig;
        using AL = screenplay::config::AppLanguage;
        if (LC::current() == AL::English) {
            for (const auto& t : {
                "CUT TO:", "FADE IN:", "FADE OUT:", "DISSOLVE TO:",
                "SMASH CUT TO:", "MATCH CUT TO:", "JUMP CUT TO:",
                "IRIS OUT:", "WIPE TO:", "FLASH CUT TO:"
            }) transitions_.learn(t);
        } else {
            for (const auto& t : {
                "CORTE PARA:", "FADE IN:", "FADE OUT:", "DISSOLVER PARA:",
                "CORTE SECO:", "FUSÃO:", "ÍRIS OUT:"
            }) transitions_.learn(t);
        }
    }

};

} // namespace screenplay::editor