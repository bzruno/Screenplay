#pragma once
// stats/script_stats.hpp
// Computes real-time statistics from a Script.

#include "../model/model.hpp"
#include <string>
#include <unordered_map>
#include <algorithm>

namespace screenplay::stats {

struct ScriptStats {
    int total_pages        = 0;
    int total_scenes       = 0;
    int total_blocks       = 0;
    int total_words        = 0;
    int total_chars        = 0;
    int dialogue_blocks    = 0;
    int action_blocks      = 0;
    std::unordered_map<std::string, int> character_lines; // name → line count

    // Estimated screen time in minutes (1 page ≈ 1 min)
    float screen_time_min = 0.f;
};

class StatsEngine {
public:
    static ScriptStats compute(const Script& script, int page_count) {
        ScriptStats s;
        s.total_pages  = page_count;
        s.total_blocks = static_cast<int>(script.blocks.size());
        s.screen_time_min = static_cast<float>(page_count);

        std::string last_character;

        for (const auto& b : script.blocks) {
            // Word count
            bool in_word = false;
            for (char c : b.text) {
                if (std::isspace((unsigned char)c)) {
                    in_word = false;
                } else {
                    if (!in_word) { ++s.total_words; in_word = true; }
                }
            }
            s.total_chars += static_cast<int>(b.text.size());

            switch (b.type) {
            case BlockType::SceneHeading:
                ++s.total_scenes;
                break;
            case BlockType::Action:
                ++s.action_blocks;
                break;
            case BlockType::Character: {
                std::string ch = b.text;
                // trim leading/trailing whitespace
                size_t f = ch.find_first_not_of(" \t\r\n");
                size_t l = ch.find_last_not_of(" \t\r\n");
                ch = (f == std::string::npos) ? "" : ch.substr(f, l - f + 1);
                // uppercase
                std::transform(ch.begin(), ch.end(), ch.begin(),
                    [](unsigned char c){ return std::toupper(c); });
                last_character = ch;
                break;
            }
            case BlockType::Dialogue:
            case BlockType::DualDialogue:
                ++s.dialogue_blocks;
                if (!last_character.empty())
                    s.character_lines[last_character]++;
                break;
            default:
                break;
            }
        }
        return s;
    }

    // Top N characters by dialogue count
    static std::vector<std::pair<std::string,int>> top_characters(
        const ScriptStats& s, int n = 10)
    {
        std::vector<std::pair<std::string,int>> v(
            s.character_lines.begin(), s.character_lines.end());
        std::sort(v.begin(), v.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });
        if ((int)v.size() > n) v.resize(n);
        return v;
    }
};

} // namespace screenplay::stats
