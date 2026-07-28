#pragma once
// Production reports.
//
// The three lists a production office actually asks for before a shoot: what
// scenes exist, who is in them, and where they happen. All of it is already
// computed by ScriptIndex — this only shapes it into printable tables, so the
// numbers on a report can never disagree with the numbers in the panels.
//
// Pure logic: builds rows of strings. Nothing here knows about fonts, PDF or
// widgets; ui/report_dialog.hpp decides how a Report looks.

#include "../database/script_index.hpp"
#include "../model/model.hpp"
#include "../production/scene_numbering.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace screenplay::reports {

struct Column {
    std::string title;
    bool        numeric = false;   // right-aligned when printed
};

struct Report {
    std::string                            title;
    std::string                            summary;
    std::vector<Column>                    columns;
    std::vector<std::vector<std::string>>  rows;

    bool empty() const { return rows.empty(); }
};

namespace detail {

inline std::string join(const std::vector<std::string>& parts,
                        const std::string& separator) {
    std::string out;
    for (const auto& part : parts) {
        if (!out.empty()) out += separator;
        out += part;
    }
    return out;
}

/// Scene labels keyed by the heading's block index, so every report shows the
/// same number the page does — including the locked "1A" suffixes.
inline std::map<size_t, std::string> labels_by_block(const Script& script) {
    std::map<size_t, std::string> out;
    for (const auto& scene : production::scene_numbers(script))
        out[scene.block_idx] = scene.label;
    return out;
}

inline std::string plural(int count, const char* one, const char* many) {
    return std::to_string(count) + " " + (count == 1 ? one : many);
}

} // namespace detail

/// Every scene: its number, heading, page, and who appears in it. The list an
/// AD builds a shooting schedule from.
inline Report scene_report(const Script& script,
                           const database::ScriptIndex& index) {
    Report report;
    report.title   = "Scene Report";
    report.columns = { {"Scene", true}, {"Heading"}, {"Page", true},
                       {"Cast"}, {"Elements", true} };

    const auto labels = detail::labels_by_block(script);
    int omitted = 0;

    for (const auto& scene : index.scenes) {
        const auto label = labels.find(scene.block_idx);
        const bool is_omitted = scene.block_idx < script.blocks.size()
                             && script.blocks[scene.block_idx].omitted;
        if (is_omitted) ++omitted;

        report.rows.push_back({
            label != labels.end() ? label->second
                                  : std::to_string(scene.scene_number),
            is_omitted ? production::kOmittedText : scene.heading,
            std::to_string(scene.page_estimate),
            is_omitted ? std::string() : detail::join(scene.characters, ", "),
            std::to_string(scene.line_count),
        });
    }

    report.summary = detail::plural((int)index.scenes.size(), "scene", "scenes");
    if (omitted) report.summary += ", " + std::to_string(omitted) + " omitted";
    return report;
}

/// Every speaking part, heaviest first — the list a casting office reads.
inline Report character_report(const Script& script,
                               const database::ScriptIndex& index) {
    Report report;
    report.title   = "Character Report";
    report.columns = { {"Character"}, {"Scenes", true}, {"Lines", true},
                       {"First scene", true}, {"Usual extension"} };

    const auto labels = detail::labels_by_block(script);

    // Sorting by line count puts the leads at the top, which is the order the
    // list is read in; alphabetical would bury them.
    auto ranked = index.characters;
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) {
                  if (a.total_dialogue_count != b.total_dialogue_count)
                      return a.total_dialogue_count > b.total_dialogue_count;
                  return a.name < b.name;
              });

    for (const auto& character : ranked) {
        // The character's first cue sits inside a scene; report the scene that
        // owns it, not the raw block index.
        std::string first = "\xe2\x80\x94";
        if (!character.first_appearance.empty()) {
            const size_t block = character.first_appearance.front();
            for (auto it = labels.rbegin(); it != labels.rend(); ++it)
                if (it->first <= block) { first = it->second; break; }
        }
        report.rows.push_back({
            character.name,
            std::to_string(character.total_scene_count),
            std::to_string(character.total_dialogue_count),
            first,
            character.most_common_parenthetical.empty()
                ? std::string("\xe2\x80\x94")
                : character.most_common_parenthetical,
        });
    }

    report.summary =
        detail::plural((int)index.characters.size(), "character", "characters");
    return report;
}

/// Every location with the scenes shot there — the list a location manager and
/// a scheduler both work from, because scenes sharing a location get grouped.
inline Report location_report(const Script& script,
                              const database::ScriptIndex& index) {
    Report report;
    report.title   = "Location Report";
    report.columns = { {"Location"}, {"INT/EXT"}, {"Time of day"},
                       {"Scenes", true}, {"Scene numbers"} };

    const auto labels = detail::labels_by_block(script);

    struct Place {
        std::vector<std::string> prefixes;
        std::vector<std::string> times;
        std::vector<std::string> scene_labels;
    };
    std::map<std::string, Place> places;   // sorted by name, which is how it reads

    auto remember = [](std::vector<std::string>& list, const std::string& value) {
        if (value.empty()) return;
        if (std::find(list.begin(), list.end(), value) == list.end())
            list.push_back(value);
    };

    for (const auto& scene : index.scenes) {
        if (scene.block_idx < script.blocks.size()
                && script.blocks[scene.block_idx].omitted) continue;
        auto& place = places[scene.location.empty() ? scene.heading
                                                    : scene.location];
        remember(place.prefixes, scene.prefix);
        remember(place.times,    scene.time_of_day);
        const auto label = labels.find(scene.block_idx);
        place.scene_labels.push_back(label != labels.end()
                                         ? label->second
                                         : std::to_string(scene.scene_number));
    }

    for (const auto& [name, place] : places) {
        report.rows.push_back({
            name,
            detail::join(place.prefixes, " / "),
            detail::join(place.times, " / "),
            std::to_string(place.scene_labels.size()),
            detail::join(place.scene_labels, ", "),
        });
    }

    report.summary = detail::plural((int)places.size(), "location", "locations");
    return report;
}

} // namespace screenplay::reports
