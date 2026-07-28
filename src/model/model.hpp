#pragma once
#include "revision.hpp"
#include "style_runs.hpp"
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
    // Source credit under the authors: "Based on the novel by …", "Based on a
    // true story". Empty for an original screenplay, which is why it is a
    // separate field rather than another author line.
    std::string              based_on;
    // The bottom-left block: an address over the ways to reach the writer.
    // `contact_left` is the first line and keeps its name because that is what
    // every .spl file already written calls it.
    std::string              contact_left;  // address, line 1
    std::string              address_2;
    std::string              contact_1;
    std::string              contact_2;
    // Absolute path to an image used INSTEAD of the typed title — a film's
    // logo or treatment artwork. Empty means "use `title` as text". Stored as
    // a path rather than embedded bytes so the .spl stays a small text file;
    // a missing file simply falls back to the text title.
    std::string              logo_path;
    bool                     enabled = false;

    bool uses_logo() const { return !logo_path.empty(); }

    /// The bottom-left lines that actually have content, in printing order —
    /// so an empty address line never leaves a hole on the page.
    std::vector<std::string> contact_block() const {
        std::vector<std::string> lines;
        for (const std::string* line : { &contact_left, &address_2,
                                         &contact_1, &contact_2 })
            if (!line->empty()) lines.push_back(*line);
        return lines;
    }
};

// Serialized by ordinal, so new members go at the END — inserting one in the
// middle would silently reinterpret every block in every file already saved.
enum class BlockType : uint8_t {
    SceneHeading,
    Action,
    Character,
    Parenthetical,
    Dialogue,
    Transition,
    DualDialogue,
    // A camera instruction inside a scene: "ANGLE ON THE DOOR", "CLOSE ON ANA".
    // Reads as Action but uppercase, because it addresses the camera and not
    // the reader.
    Shot,
    // Free text at the full margin width, for the things a screenplay
    // occasionally needs and no element covers.
    General,
    // Television's act structure: "ACT ONE", "END OF ACT ONE". Centred,
    // because it is a divider between acts rather than a line of the story.
    ActBreak,
};

// Optional per-block horizontal alignment override.
//
// In a screenplay the element TYPE normally dictates alignment, and for the
// cue/dialogue family it defines the element outright (Character, Dialogue and
// Parenthetical ARE their indent columns — realigning them would stop them
// being those elements). Only Action and Transition have a real tradition of
// author-chosen alignment: a centred "THE END" / "INTERCUT", a flush-left
// "FADE IN:" against a flush-right "CUT TO:".
//
// `Default` means "whatever the type's own format says" — the value every
// block starts at, and the reason older files without this field load
// unchanged. See layout::supports_alignment().
enum class BlockAlign : uint8_t {
    Default,
    Left,
    Center,
    Right
};

// Whether an author may override this block type's alignment.
//
// Action and Transition only, for the reasons above. This lives on the MODEL,
// not in the layout engine or the toolbar, because it is a rule about what a
// screenplay document permits — so the layout engine, the FDX importer and the
// UI all answer the question the same way and can never drift apart.
inline bool supports_alignment(BlockType t) {
    return t == BlockType::Action || t == BlockType::Transition
        || t == BlockType::General;
}

struct Block {
    BlockType   type;
    std::string text;
    uint32_t    id;
    // Per-character formatting: each is a sorted, disjoint set of [start,end)
    // byte ranges within `text` where that style is active — NOT a whole-
    // block flag, so selecting one word and toggling Bold styles only that
    // word. See model/style_runs.hpp for the range operations, and keep
    // these in sync on every text mutation (insert/erase/split/merge).
    StyleRuns   bold_runs;
    StyleRuns   italic_runs;
    StyleRuns   underline_runs;
    // Scene number as authored in an imported file (e.g. "1", "1A", "A5").
    // Empty means "unnumbered"; it is preserved verbatim across FDX/JSON round
    // trips and never auto-renumbered. On-screen numbering is computed
    // separately and does not depend on this field.
    std::string scene_number;
    // Author's alignment override for this block, or Default to follow the
    // block type's own format. Only meaningful for the types
    // layout::supports_alignment() accepts.
    BlockAlign  align = BlockAlign::Default;
    // Author's note on this block — the screenplay equivalent of a margin
    // comment. Never rendered into the page or exported to PDF: it is
    // production/annotation metadata, not screenplay content. Empty means
    // "no note", which is what every block starts as.
    std::string note;
    // The revision pass that last changed this block. Drives the asterisk in
    // the margin — the mark an assistant director scans for to find what moved
    // since the last coloured pages went out. `None` means "unchanged since
    // the original draft", which is where every block starts.
    Revision    revision = Revision::None;
    // A scene the production cut. The heading keeps its locked number and
    // reads "OMITTED" so every later number stays put — renumbering a script
    // mid-shoot invalidates every schedule and call sheet built from it.
    // Meaningful on SceneHeading only.
    bool        omitted  = false;
    // Forces this block to open a page. The writer's override for the one
    // thing automatic pagination cannot know — that a beat has to land at the
    // top of a page. Ignored when the block already starts one.
    bool        page_break_before = false;

    bool is_bold_whole()      const { return style_covers(bold_runs,      0, text.size()) && !text.empty(); }
    bool is_italic_whole()    const { return style_covers(italic_runs,    0, text.size()) && !text.empty(); }
    bool is_underline_whole() const { return style_covers(underline_runs, 0, text.size()) && !text.empty(); }
};

struct Script {
    TitlePage          title_page;
    std::vector<Block> blocks;
    uint32_t           next_id = 1;
    // The pass being written right now. While it is not `None`, every edit
    // stamps its block, so the margin marks accumulate on their own instead of
    // relying on the writer to remember to flag each change.
    Revision           current_revision = Revision::None;
    // Locked scene numbers stop following the scene's position: they are
    // frozen into Block::scene_number, and scenes inserted afterwards get
    // letter suffixes (1, 1A, 1B, 2) so nothing already scheduled shifts.
    bool               scenes_locked    = false;

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
