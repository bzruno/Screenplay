#pragma once
// stats/script_stats.hpp
// Computes real-time statistics from a Script.

#include "../model/model.hpp"
#include "../parsing/screenplay_parse.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
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

    // Location → scene count and time-of-day → scene count, both extracted from
    // Scene Headings and sorted by count (descending, ties keep first-seen order).
    std::vector<std::pair<std::string,int>> locations;
    std::vector<std::pair<std::string,int>> time_of_day;

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

        // Insertion-ordered accumulators for locations and time-of-day; keyed by
        // an accent/case-folded string so "Casa" and "CASA" collapse together.
        std::vector<std::pair<std::string,int>> loc_acc, tod_acc;
        std::unordered_map<std::string,size_t> loc_idx, tod_idx;
        auto bump = [](std::vector<std::pair<std::string,int>>& acc,
                       std::unordered_map<std::string,size_t>& idx,
                       const std::string& key, const std::string& display) {
            auto it = idx.find(key);
            if (it == idx.end()) { idx[key] = acc.size(); acc.push_back({ display, 1 }); }
            else                 { ++acc[it->second].second; }
        };

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
            case BlockType::SceneHeading: {
                ++s.total_scenes;
                auto parts = parse::parse_scene_heading(b.text);
                if (!parts.location.empty())
                    bump(loc_acc, loc_idx, parse::fold(parts.location), parts.location);
                bump(tod_acc, tod_idx,
                     parse::classify_time_of_day(parts.time_of_day),
                     parse::classify_time_of_day(parts.time_of_day));
                break;
            }
            case BlockType::Action:
                ++s.action_blocks;
                break;
            case BlockType::Character: {
                // Character Extension (V.O., O.S., CONT'D …) is NOT part of the
                // name — statistics group speakers by name only.
                std::string ch = parse::parse_character_cue(b.text).name;
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

        // Sort both breakdowns by descending count; std::stable_sort keeps the
        // first-seen order for equal counts (matches narrative appearance).
        auto by_count = [](const auto& a, const auto& b){ return a.second > b.second; };
        std::stable_sort(loc_acc.begin(), loc_acc.end(), by_count);
        std::stable_sort(tod_acc.begin(), tod_acc.end(), by_count);
        s.locations   = std::move(loc_acc);
        s.time_of_day = std::move(tod_acc);
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
