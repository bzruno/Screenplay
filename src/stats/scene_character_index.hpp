#pragma once
// stats/scene_character_index.hpp
// Scene and character analysis engine.
// Pure C++ / std only — zero Qt dependencies.

#include "../model/model.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace screenplay::stats {

// ─────────────────────────────────────────────────────────────────────────────
// Public data structures
// ─────────────────────────────────────────────────────────────────────────────

struct SceneInfo {
    size_t                   block_idx;   // index of the SceneHeading block
    std::string              title;       // raw heading text
    std::vector<std::string> characters; // unique normalized names in this scene
};

struct CharacterInfo {
    std::string            name;             // normalized (trimmed, uppercase)
    int                    appearance_count; // number of dialogue blocks spoken
    std::vector<size_t>    scene_indices;    // indexes into SceneInfo list
};

// ─────────────────────────────────────────────────────────────────────────────
// SceneCharacterIndex
// ─────────────────────────────────────────────────────────────────────────────

class SceneCharacterIndex {
public:
    // Full rebuild from scratch — O(n).
    void build(const screenplay::Script& script) {
        scenes_.clear();
        characters_.clear();
        pending_char_.clear();

        SceneInfo* cur = nullptr;
        for (size_t i = 0; i < script.blocks.size(); ++i) {
            const auto& b = script.blocks[i];
            if (b.type == screenplay::BlockType::SceneHeading) {
                scenes_.push_back({ i, b.text, {} });
                cur = &scenes_.back();
                pending_char_.clear();
            } else {
                process_block(b, i, cur);
            }
        }
    }

    // Partial rebuild — only processes blocks from the scene boundary at or
    // before changed_block_idx. Prefix scene objects are preserved;
    // prefix character data is rescanned to maintain correct counts.
    void update(const screenplay::Script& script, size_t changed_block_idx) {
        if (scenes_.empty()) { build(script); return; }

        // ── Find restart: last SceneHeading at or before changed_block_idx ──
        size_t restart_si    = 0;
        size_t restart_block = scenes_[0].block_idx;

        for (size_t si = 0; si < scenes_.size(); ++si) {
            if (scenes_[si].block_idx <= changed_block_idx) {
                restart_si    = si;
                restart_block = scenes_[si].block_idx;
            }
        }

        if (changed_block_idx < scenes_[0].block_idx) {
            // Change is before any scene — full rebuild
            build(script);
            return;
        }

        // ── Phase 1: build prefix character data (blocks 0 .. restart_block-1) ──
        // Reuses existing prefix SceneInfo objects (their .characters lists are
        // cleared and repopulated).
        characters_.clear();
        pending_char_.clear();

        // Clear prefix scene character lists so they're repopulated correctly
        for (size_t si = 0; si < restart_si; ++si)
            scenes_[si].characters.clear();

        {
            size_t si_cursor = 0;
            SceneInfo* cur = nullptr;
            for (size_t i = 0; i < restart_block && i < script.blocks.size(); ++i) {
                const auto& b = script.blocks[i];
                if (b.type == screenplay::BlockType::SceneHeading) {
                    pending_char_.clear();
                    // Advance to the matching prefix scene object
                    while (si_cursor < restart_si &&
                           scenes_[si_cursor].block_idx < i)
                        ++si_cursor;
                    if (si_cursor < restart_si &&
                        scenes_[si_cursor].block_idx == i) {
                        cur = &scenes_[si_cursor];
                        ++si_cursor;
                    }
                } else {
                    process_block(b, i, cur);
                }
            }
        }

        // Snapshot prefix character state before appending suffix
        std::vector<CharacterInfo> prefix_snap = characters_;

        // ── Phase 2: rebuild suffix (blocks restart_block .. end) ─────────────
        // Truncate to prefix scenes; suffix scenes will be appended fresh.
        scenes_.resize(restart_si);

        // Restore prefix character snapshot as the starting point
        characters_ = std::move(prefix_snap);
        pending_char_.clear();

        SceneInfo* cur = restart_si > 0 ? &scenes_[restart_si - 1] : nullptr;

        for (size_t i = restart_block; i < script.blocks.size(); ++i) {
            const auto& b = script.blocks[i];
            if (b.type == screenplay::BlockType::SceneHeading) {
                scenes_.push_back({ i, b.text, {} });
                cur = &scenes_.back();
                pending_char_.clear();
            } else {
                process_block(b, i, cur);
            }
        }
    }

    const std::vector<SceneInfo>&     scenes()     const { return scenes_; }
    const std::vector<CharacterInfo>& characters() const { return characters_; }

    const CharacterInfo* find_character(const std::string& name) const {
        const std::string n = normalize(name);
        for (const auto& c : characters_)
            if (c.name == n) return &c;
        return nullptr;
    }

    // Returns the last scene whose block_idx <= block_idx (i.e., the scene
    // that contains the given block), or nullptr if before any scene.
    const SceneInfo* find_scene(size_t block_idx) const {
        const SceneInfo* result = nullptr;
        for (const auto& s : scenes_)
            if (s.block_idx <= block_idx) result = &s;
        return result;
    }

private:
    std::vector<SceneInfo>     scenes_;
    std::vector<CharacterInfo> characters_;
    std::string                pending_char_; // last seen Character name (state during scan)

    // ── Helpers ───────────────────────────────────────────────────────────────

    static std::string normalize(const std::string& s) {
        size_t f = s.find_first_not_of(" \t\r\n");
        if (f == std::string::npos) return {};
        size_t l = s.find_last_not_of(" \t\r\n");
        std::string r = s.substr(f, l - f + 1);
        for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return r;
    }

    CharacterInfo* find_character_mut(const std::string& norm_name) {
        for (auto& c : characters_)
            if (c.name == norm_name) return &c;
        return nullptr;
    }

    // Process one non-SceneHeading block, updating characters_ and current_scene.
    void process_block(const screenplay::Block& b, size_t /*idx*/,
                       SceneInfo* current_scene)
    {
        using BT = screenplay::BlockType;
        switch (b.type) {

        case BT::Character:
            pending_char_ = normalize(b.text);
            break;

        case BT::Parenthetical:
            // Parenthetical sits between Character and Dialogue — keep pending_char_
            break;

        case BT::Dialogue:
        case BT::DualDialogue: {
            if (pending_char_.empty()) break;

            // Find or create CharacterInfo
            CharacterInfo* ci = find_character_mut(pending_char_);
            if (!ci) {
                characters_.push_back({ pending_char_, 0, {} });
                ci = &characters_.back();
            }
            ++ci->appearance_count;

            // Register in current scene (unique membership)
            if (current_scene) {
                size_t scene_idx = static_cast<size_t>(current_scene - scenes_.data());

                if (std::find(ci->scene_indices.begin(),
                              ci->scene_indices.end(),
                              scene_idx) == ci->scene_indices.end())
                    ci->scene_indices.push_back(scene_idx);

                if (std::find(current_scene->characters.begin(),
                              current_scene->characters.end(),
                              pending_char_) == current_scene->characters.end())
                    current_scene->characters.push_back(pending_char_);
            }
            break;
        }

        default:
            // Action, Transition, etc. break the Character→Dialogue chain
            pending_char_.clear();
            break;
        }
    }
};

} // namespace screenplay::stats

// ─────────────────────────────────────────────────────────────────────────────
// Inline implementations of Script::analyze_* (declared in model.hpp).
// Any file calling these methods must include this header.
// ─────────────────────────────────────────────────────────────────────────────
namespace screenplay {

inline std::vector<stats::SceneInfo> Script::analyze_scenes() const {
    stats::SceneCharacterIndex idx;
    idx.build(*this);
    return idx.scenes();          // returns by value (copy)
}

inline std::vector<stats::CharacterInfo> Script::analyze_characters() const {
    stats::SceneCharacterIndex idx;
    idx.build(*this);
    return idx.characters();      // returns by value (copy)
}

} // namespace screenplay
