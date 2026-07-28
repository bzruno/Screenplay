#pragma once
// database/script_index.hpp
// Structured index over screenplay content: scenes, characters, dialogue.
// Pure C++ std only — no Qt, no UI.

#include "../model/model.hpp"
#include "../layout/layout_engine.hpp"
#include "../parsing/screenplay_parse.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace screenplay::database {

// ─────────────────────────────────────────────────────────────────────────────
// Data structures
// ─────────────────────────────────────────────────────────────────────────────

struct DialogueLine {
    size_t block_idx;          // index of the Character block
    size_t dialogue_block_idx; // index of the Dialogue block
    std::string character;     // normalized uppercase name
    std::string text;          // dialogue text
    std::string parenthetical; // "(V.O.)" etc, empty if none
    int order_in_scene;        // 1-based position within scene
};

struct SceneContent {
    size_t block_idx;          // index of SceneHeading block
    int scene_number;          // 1-based
    std::string heading;       // full heading text e.g. "INT. HOUSE - DAY"
    std::string prefix;        // "INT." / "EXT." / "I/E."
    std::string location;      // e.g. "HOUSE"
    std::string time_of_day;   // e.g. "DAY"
    int page_estimate;         // approximate 1-based page number
    int line_count;            // total number of blocks in this scene
    std::vector<std::string>  characters;    // unique, normalized, in order of appearance
    std::vector<DialogueLine> dialogue;      // all dialogue in narrative order
    std::vector<size_t>       block_indices; // ALL block indices belonging to scene
};

struct CharacterRecord {
    std::string name;                       // normalized uppercase trimmed
    int total_dialogue_count  = 0;          // total DialogueLines across all scenes
    int total_scene_count     = 0;          // number of distinct scenes
    std::vector<int>    scene_numbers;      // 1-based, in narrative order
    std::vector<size_t> first_appearance;   // block_idx of first Character block per scene
    std::string most_common_parenthetical;  // most used extension (V.O., O.S., etc.)
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptIndex — lookup helpers
// ─────────────────────────────────────────────────────────────────────────────

struct ScriptIndex {
    std::vector<SceneContent>    scenes;
    std::vector<CharacterRecord> characters;

    const SceneContent* find_scene_by_block(size_t block_idx) const {
        const SceneContent* result = nullptr;
        for (const auto& s : scenes)
            if (s.block_idx <= block_idx) result = &s;
        return result;
    }

private:
    static std::string norm(const std::string& s) {
        size_t f = s.find_first_not_of(" \t\r\n");
        if (f == std::string::npos) return {};
        size_t l = s.find_last_not_of(" \t\r\n");
        std::string r = s.substr(f, l - f + 1);
        for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return r;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ScriptIndexBuilder
// ─────────────────────────────────────────────────────────────────────────────

class ScriptIndexBuilder {
public:
    static ScriptIndex build(const screenplay::Script& script,
                             const screenplay::layout::PageList& pages)
    {
        ScriptIndex idx;
        SceneContent* current_scene    = nullptr;
        std::string   last_character;
        size_t        last_char_block  = 0;
        std::string   last_paren;
        int           scene_number     = 0;
        int           order_in_scene   = 0;

        // character name → { scene_number → first block_idx }
        std::unordered_map<std::string, std::unordered_map<int,size_t>> char_first;
        // character name → { parenthetical → count }
        std::unordered_map<std::string, std::unordered_map<std::string,int>> char_parens;

        // Page number for a given block_idx — precomputed ONCE in a single
        // pass over all pages/lines (O(total lines)), then looked up O(1) per
        // scene heading. The previous version re-scanned every page's every
        // line for EVERY scene heading (O(scenes * total_lines)), which on a
        // full-length script (hundreds of pages) took seconds — this was the
        // actual cause of the multi-second hang after importing a large FDX.
        std::unordered_map<size_t, int> page_of_block;
        page_of_block.reserve(script.blocks.size());
        for (const auto& page : pages)
            for (const auto& vl : page.lines)
                if (!vl.is_more && !vl.is_contd)
                    page_of_block.emplace(vl.block_idx, page.number);
        auto page_for_block = [&](size_t bi) -> int {
            auto it = page_of_block.find(bi);
            return it != page_of_block.end() ? it->second : 1;
        };

        // Find or create a CharacterRecord by normalized name
        auto get_or_create = [&](const std::string& norm_name) -> CharacterRecord& {
            for (auto& cr : idx.characters)
                if (cr.name == norm_name) return cr;
            idx.characters.push_back({norm_name});
            return idx.characters.back();
        };

        for (size_t i = 0; i < script.blocks.size(); ++i) {
            const auto& b = script.blocks[i];
            using BT = screenplay::BlockType;

            switch (b.type) {

            case BT::SceneHeading: {
                // Finalize previous scene
                if (current_scene)
                    current_scene->line_count = (int)current_scene->block_indices.size();

                ++scene_number;
                order_in_scene = 0;
                last_character.clear();
                last_paren.clear();

                SceneContent sc;
                sc.block_idx    = i;
                sc.scene_number = scene_number;
                sc.heading      = b.text;
                parse_heading(b.text, sc.prefix, sc.location, sc.time_of_day);
                sc.page_estimate = page_for_block(i);
                sc.line_count    = 0;
                sc.block_indices.push_back(i);
                idx.scenes.push_back(std::move(sc));
                current_scene = &idx.scenes.back();
                break;
            }

            case BT::Action:
            case BT::Transition:
                if (current_scene) current_scene->block_indices.push_back(i);
                // Action/Transition break the Character→Dialogue chain
                last_character.clear();
                last_paren.clear();
                break;

            case BT::Character: {
                if (!current_scene) break;
                current_scene->block_indices.push_back(i);

                // Split the Character Extension (V.O., O.S., CONT'D, …) from the
                // name; the name alone identifies the speaker.
                auto cue = parse::parse_character_cue(b.text);
                last_character   = normalize(cue.name);
                last_char_block  = i;
                last_paren.clear();
                if (!cue.extension.empty())
                    ++char_parens[last_character]["(" + cue.extension + ")"];

                if (!last_character.empty()) {
                    // Add to scene's unique character list (in appearance order)
                    if (std::find(current_scene->characters.begin(),
                                  current_scene->characters.end(),
                                  last_character) == current_scene->characters.end())
                        current_scene->characters.push_back(last_character);

                    // Ensure CharacterRecord exists
                    get_or_create(last_character);

                    // Track first block_idx per scene
                    auto& fmap = char_first[last_character];
                    if (fmap.find(scene_number) == fmap.end())
                        fmap[scene_number] = i;
                }
                break;
            }

            case BT::Parenthetical:
                if (current_scene) {
                    current_scene->block_indices.push_back(i);
                    last_paren = b.text;
                }
                break;

            case BT::Dialogue:
            case BT::DualDialogue: {
                if (!current_scene || last_character.empty()) break;
                current_scene->block_indices.push_back(i);

                DialogueLine dl;
                dl.block_idx          = last_char_block;
                dl.dialogue_block_idx = i;
                dl.character          = last_character;
                dl.text               = b.text;
                dl.parenthetical      = last_paren;
                dl.order_in_scene     = ++order_in_scene;
                current_scene->dialogue.push_back(dl);

                CharacterRecord& cr = get_or_create(last_character);
                ++cr.total_dialogue_count;

                if (!last_paren.empty())
                    ++char_parens[last_character][last_paren];

                last_paren.clear();
                break;
            }

            default:
                break;
            }
        }

        // Finalize last scene
        if (current_scene)
            current_scene->line_count = (int)current_scene->block_indices.size();

        // Build CharacterRecord scene lists, first_appearance, most_common_parenthetical
        for (auto& cr : idx.characters) {
            for (const auto& sc : idx.scenes) {
                bool present = std::find(sc.characters.begin(),
                                         sc.characters.end(),
                                         cr.name) != sc.characters.end();
                if (present) cr.scene_numbers.push_back(sc.scene_number);
            }
            cr.total_scene_count = (int)cr.scene_numbers.size();

            // first_appearance: one entry per scene, in scene order
            auto fit = char_first.find(cr.name);
            if (fit != char_first.end()) {
                for (int sn : cr.scene_numbers) {
                    auto sit = fit->second.find(sn);
                    if (sit != fit->second.end())
                        cr.first_appearance.push_back(sit->second);
                }
            }

            // most_common_parenthetical
            auto pit = char_parens.find(cr.name);
            if (pit != char_parens.end() && !pit->second.empty()) {
                auto best = std::max_element(pit->second.begin(), pit->second.end(),
                    [](const auto& a, const auto& b){ return a.second < b.second; });
                cr.most_common_parenthetical = best->first;
            }
        }

        return idx;
    }

private:
    static std::string normalize(const std::string& s) {
        size_t f = s.find_first_not_of(" \t\r\n");
        if (f == std::string::npos) return {};
        size_t l = s.find_last_not_of(" \t\r\n");
        std::string r = s.substr(f, l - f + 1);
        for (char& c : r)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return r;
    }

    static void parse_heading(const std::string& heading,
                               std::string& prefix,
                               std::string& location,
                               std::string& time_of_day)
    {
        // Delegate to the shared parser so layout, stats and database agree.
        auto p = parse::parse_scene_heading(heading);
        prefix      = p.int_ext;
        location    = p.location;
        time_of_day = p.time_of_day;
    }
};

} // namespace screenplay::database
