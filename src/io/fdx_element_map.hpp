#pragma once
// io/fdx_element_map.hpp
// Single source of truth for the BlockType <-> Final Draft (.fdx) element-name
// mapping. Both FDXExporter and FDXImporter use this table so the two sides can
// never drift out of sync (DRY).

#include "../model/model.hpp"
#include <string>

namespace screenplay::io {

// BlockType -> FDX <Paragraph Type="…"> name (export direction).
//
// DualDialogue maps to a plain "Dialogue" paragraph: the internal model pairs
// two dialogue columns for on-screen rendering but does not carry the speaker
// association FDX's <DualDialogue> grouping requires, so a flat — but valid —
// Dialogue paragraph is the correct conservative representation.
inline const char* fdx_element_name(screenplay::BlockType t) {
    switch (t) {
    case screenplay::BlockType::SceneHeading:  return "Scene Heading";
    case screenplay::BlockType::Action:        return "Action";
    case screenplay::BlockType::Character:     return "Character";
    case screenplay::BlockType::Parenthetical: return "Parenthetical";
    case screenplay::BlockType::Dialogue:      return "Dialogue";
    case screenplay::BlockType::Transition:    return "Transition";
    case screenplay::BlockType::DualDialogue:  return "Dialogue";
    case screenplay::BlockType::Shot:          return "Shot";
    case screenplay::BlockType::General:       return "General";
    case screenplay::BlockType::ActBreak:      return "New Act";
    }
    return "Action";
}

// FDX <Paragraph Type="…"> name -> BlockType (import direction).
// `known` reports whether the name is a screenplay element this model
// represents; the Final Draft types this model has no element for (Lyrics,
// Cast List, …) are down-graded to Action with their text preserved, and the
// caller is told via `known == false` so it can surface the conversion.
inline screenplay::BlockType
fdx_block_type(const std::string& type_str, bool& known) {
    known = true;
    if (type_str == "Scene Heading")  return screenplay::BlockType::SceneHeading;
    if (type_str == "Action")         return screenplay::BlockType::Action;
    if (type_str == "Character")      return screenplay::BlockType::Character;
    if (type_str == "Parenthetical")  return screenplay::BlockType::Parenthetical;
    if (type_str == "Dialogue")       return screenplay::BlockType::Dialogue;
    if (type_str == "Transition")     return screenplay::BlockType::Transition;
    if (type_str == "Shot")           return screenplay::BlockType::Shot;
    if (type_str == "General")        return screenplay::BlockType::General;
    // Final Draft writes both ends of an act as their own element; the model
    // has one act divider, and the text ("ACT ONE" / "END OF ACT ONE") is what
    // distinguishes them on the page.
    if (type_str == "New Act")        return screenplay::BlockType::ActBreak;
    if (type_str == "End of Act")     return screenplay::BlockType::ActBreak;
    known = false;
    return screenplay::BlockType::Action;
}

} // namespace screenplay::io
