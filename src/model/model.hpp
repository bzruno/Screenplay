#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace screenplay {

// Forward declarations for Script::analyze_* return types.
// Include stats/scene_character_index.hpp to call these methods.
namespace stats {
    struct SceneInfo;
    struct CharacterInfo;
}


struct TitlePage {
    std::string              title;
    std::string              credit_type;   // "Written by" / "Screenplay by" / "Story by"
    std::vector<std::string> authors;       // one or more authors
    std::string              contact_left;  // bottom-left contact info
    bool                     enabled = false;
};

enum class BlockType : uint8_t {
    SceneHeading,
    Action,
    Character,
    Parenthetical,
    Dialogue,
    Transition,
    DualDialogue
};

struct Block {
    BlockType   type;
    std::string text;
    uint32_t    id;
    bool        is_bold_      = false;
    bool        is_italic_    = false;
    bool        is_underline_ = false;
};

struct Script {
    TitlePage          title_page;
    std::vector<Block> blocks;
    uint32_t           next_id = 1;

    Block& append(BlockType t, std::string text = {}) {
        blocks.push_back({ t, std::move(text), next_id++ });
        return blocks.back();
    }

    // Returns all scenes with their character lists.
    // Defined inline in stats/scene_character_index.hpp — include it to use.
    std::vector<stats::SceneInfo>     analyze_scenes()     const;
    std::vector<stats::CharacterInfo> analyze_characters() const;
};

struct Cursor {
    size_t block_idx   = 0;
    size_t byte_offset = 0;
};

} // namespace screenplay
