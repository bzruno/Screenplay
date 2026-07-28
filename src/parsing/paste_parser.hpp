#pragma once
// parsing/paste_parser.hpp
// Recognizes screenplay elements in arbitrary pasted text (plain paragraphs,
// no Fountain forced-syntax markers). This is the single source of truth for
// Ctrl+V element detection — the editor delegates paste parsing here instead
// of guessing a single block type for every pasted paragraph.
//
// Pure C++ / std only (no Qt), reusing the shared recognition helpers in
// screenplay_parse.hpp so paste and scene analysis agree on what an element is.

#include "../model/model.hpp"
#include "screenplay_parse.hpp"
#include <string>
#include <vector>
#include <cctype>

namespace screenplay::parse {

// A paragraph is a Scene Heading when it carries a recognized INT./EXT. prefix.
inline bool looks_like_scene_heading(const std::string& paragraph) {
    return !parse_scene_heading(paragraph).int_ext.empty();
}

// A paragraph is a Transition when it ends with the classic "… TO:" / "… OUT:"
// cadence. (Matches the Fountain importer's heuristic for round-trip parity.)
inline bool looks_like_transition(const std::string& paragraph) {
    const std::string t = trim(paragraph);
    if (t.size() >= 3 && t.compare(t.size() - 3, 3, "TO:")  == 0) return true;
    if (t.size() >= 4 && t.compare(t.size() - 4, 4, "OUT:") == 0) return true;
    return false;
}

// A paragraph reads as a wrylie/parenthetical when wrapped in parentheses.
inline bool looks_like_parenthetical(const std::string& paragraph) {
    const std::string t = trim(paragraph);
    return t.size() >= 2 && t.front() == '(' && t.back() == ')';
}

// A Character cue is an uppercase line: it contains at least one letter and no
// lowercase letter. Non-ASCII bytes (accents) are neutral, so "JOÃO (V.O.)"
// still qualifies; the name/extension split happens later via
// parse_character_cue.
inline bool looks_like_character(const std::string& paragraph) {
    bool has_alpha = false;
    for (unsigned char c : paragraph) {
        if (std::isalpha(c)) {
            has_alpha = true;
            if (std::islower(c)) return false;
        }
    }
    return has_alpha;
}

// Classify each pasted paragraph into a BlockType. Detection order matters:
// the more specific patterns (scene heading, parenthetical, transition) are
// tested before the broad uppercase Character rule. A paragraph that follows a
// Character or its Parenthetical becomes Dialogue.
inline std::vector<screenplay::BlockType>
classify_paragraphs(const std::vector<std::string>& paragraphs) {
    using BT = screenplay::BlockType;
    std::vector<BT> types(paragraphs.size(), BT::Action);

    for (size_t i = 0; i < paragraphs.size(); ++i) {
        const std::string& p = paragraphs[i];
        if      (looks_like_scene_heading(p))  types[i] = BT::SceneHeading;
        else if (looks_like_parenthetical(p))  types[i] = BT::Parenthetical;
        else if (looks_like_transition(p) &&
                 looks_like_character(p))       types[i] = BT::Transition;
        else if (looks_like_character(p))       types[i] = BT::Character;
        else                                    types[i] = BT::Action;
    }

    // Promote the paragraph after a Character (or its Parenthetical) to Dialogue.
    for (size_t i = 1; i < types.size(); ++i) {
        if (types[i] != BT::Action) continue;
        if (types[i - 1] == BT::Character) {
            types[i] = BT::Dialogue;
        } else if (types[i - 1] == BT::Parenthetical && i >= 2 &&
                   (types[i - 2] == BT::Character || types[i - 2] == BT::Dialogue)) {
            types[i] = BT::Dialogue;
        }
    }

    return types;
}

} // namespace screenplay::parse
