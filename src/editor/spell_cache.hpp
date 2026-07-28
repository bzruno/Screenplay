#pragma once
// Per-block spell-check results for a Script.
//
// Decides WHAT is worth checking and in WHICH ORDER; the checking itself
// happens on SpellWorker's thread (see spell_worker.hpp). Nothing here blocks:
// update() hands the worker a priority list and collects whatever came back
// since last time.

#include "../model/model.hpp"
#include "../parsing/screenplay_parse.hpp"
#include "spell_worker.hpp"

#include <QString>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace screenplay::editor {

using Misspelling = screenplay::spellcheck::Misspelling;

class SpellCache {
public:
    /// Blocks either side of the visible range that are checked too, so a
    /// short scroll finds its text already done.
    static constexpr size_t kLookAhead = 40;

    /// Longer blocks are skipped: they are pasted prose or stage directions,
    /// and one costs many times an ordinary line.
    static constexpr size_t kMaxBlockBytes = 500;

    bool available() const { return worker_.available(); }

    void set_languages(const std::vector<std::string>& lang_tags) {
        worker_.set_languages(lang_tags);
        on_demand_.reinit(lang_tags);
        invalidate_all();
    }

    /// Replacements for one word, for the context menu.
    ///
    /// Served by a checker owned by the CALLING thread: a COM object belongs
    /// to the apartment that created it, so the worker's cannot be borrowed.
    /// One right-click is one round-trip, which is affordable — collecting
    /// them for a whole document was not.
    std::vector<std::string> suggest(const std::string& word) const {
        return on_demand_.suggest(word);
    }

    void invalidate_all() { entries_.clear(); }

    void invalidate_block(size_t block_idx) {
        entries_.erase(block_idx);
    }

    void add_to_dictionary(const std::string& word) {
        worker_.add_to_dictionary(word);
        invalidate_all();
    }

    /// Collects finished work and queues what the reader can see.
    ///
    /// `first_visible`/`last_visible` are the blocks currently on screen. They
    /// are checked before anything else, because a squiggle the writer cannot
    /// see is worth nothing — and on a long script checking everything eagerly
    /// is thousands of COM calls for text nobody is looking at.
    ///
    /// Returns true while results are still expected, so the caller knows to
    /// come back and repaint.
    bool update(const screenplay::Script& script,
                size_t first_visible, size_t last_visible) {
        // The cast has to be known BEFORE collecting: a reply is matched by
        // re-preparing the block, and preparing needs the cast. Computed
        // afterwards, the first pass would compare a result against an empty
        // cast and discard perfectly good work.
        last_cast_ = cast_names(script);
        collect(script);
        if (!available()) return false;

        const auto& cast = last_cast_;
        std::vector<SpellWorker::Job> jobs;
        for (size_t i : priority_order(script, first_visible, last_visible)) {
            const auto& block = script.blocks[i];
            if (block.text.size() > kMaxBlockBytes) continue;
            jobs.push_back({ i, prepared(block.text, block.type, cast) });
        }
        if (!jobs.empty()) worker_.submit(std::move(jobs));
        return !jobs.empty() || worker_.busy();
    }

    /// Empty when the block has nothing flagged or has not been checked yet.
    const std::vector<Misspelling>& misspellings(size_t block_idx) const {
        static const std::vector<Misspelling> none;
        const auto found = entries_.find(block_idx);
        return found == entries_.end() ? none : found->second.misspellings;
    }

private:
    struct Entry {
        std::string              snapshot;   // text the results belong to
        std::vector<Misspelling> misspellings;
    };

    /// Takes in whatever the worker finished, discarding replies for text the
    /// writer has changed since the job was queued.
    void collect(const screenplay::Script& script) {
        for (auto& result : worker_.take_results()) {
            if (result.block_idx >= script.blocks.size()) continue;
            const auto& block = script.blocks[result.block_idx];
            if (prepared_snapshot_differs(block, result.text)) continue;
            entries_[result.block_idx] =
                Entry{ block.text, std::move(result.misspellings) };
        }
    }

    /// The worker was given the PREPARED text, so a reply is matched by
    /// re-preparing the block rather than comparing raw text.
    bool prepared_snapshot_differs(const screenplay::Block& block,
                                   const std::string& checked) const {
        return prepared(block.text, block.type, last_cast_) != checked;
    }

    /// Visible blocks first, then the look-ahead either side. Blocks whose
    /// cached result still matches their text are left out entirely.
    std::vector<size_t> priority_order(const screenplay::Script& script,
                                       size_t first_visible,
                                       size_t last_visible) const {
        const size_t count = script.blocks.size();
        if (count == 0) return {};

        last_visible  = std::min(last_visible, count - 1);
        first_visible = std::min(first_visible, last_visible);
        const size_t from = first_visible > kLookAhead ? first_visible - kLookAhead : 0;
        const size_t to   = std::min(count - 1, last_visible + kLookAhead);

        std::vector<size_t> order;
        for (size_t i = first_visible; i <= last_visible; ++i)
            if (is_stale(script, i)) order.push_back(i);
        for (size_t i = from; i < first_visible; ++i)
            if (is_stale(script, i)) order.push_back(i);
        for (size_t i = last_visible + 1; i <= to; ++i)
            if (is_stale(script, i)) order.push_back(i);
        return order;
    }

    bool is_stale(const screenplay::Script& script, size_t i) const {
        const auto found = entries_.find(i);
        return found == entries_.end()
            || found->second.snapshot != script.blocks[i].text;
    }

    /// Prose only. A Scene Heading, Character cue or Transition is uppercase by
    /// convention and made of names and slugs, so checking it produces nothing
    /// but noise.
    static bool is_prose(screenplay::BlockType type) {
        using BT = screenplay::BlockType;
        return type == BT::Action || type == BT::Dialogue
            || type == BT::DualDialogue || type == BT::General;
    }

    /// The cast, uppercased. A writer capitalises a character's name on their
    /// first appearance in an Action line; without this, every one of those
    /// would be underlined as a misspelling.
    static std::set<std::string> cast_names(const screenplay::Script& script) {
        std::set<std::string> names;
        for (const auto& block : script.blocks)
            if (block.type == screenplay::BlockType::Character)
                names.insert(QString::fromStdString(
                                 screenplay::parse::parse_character_cue(block.text).name)
                                 .toUpper().toStdString());
        return names;
    }

    /// Text as the checker should see it, byte for byte the same length as the
    /// original so every reported offset still lands on the real word.
    ///
    /// Windows' spell checker skips words in ALL CAPS, treating them as
    /// acronyms. In a screenplay that silences exactly the words a writer
    /// capitalises for emphasis — "He wears a PYRPLE CLOARK" was never checked
    /// at all. Those runs are lower-cased for the checker's benefit; cast
    /// names and short acronyms (V.O., SFX) are left capitalised so they stay
    /// skipped.
    static std::string prepared(const std::string& text,
                                screenplay::BlockType type,
                                const std::set<std::string>& cast) {
        if (!is_prose(type)) return text;

        std::string out = text;
        size_t i = 0;
        while (i < out.size()) {
            if (!is_upper_ascii(out[i])) { ++i; continue; }
            size_t end = i;
            while (end < out.size() && is_upper_ascii(out[end])) ++end;

            const std::string word = out.substr(i, end - i);
            const bool worth_checking = word.size() >= kMinEmphasisWord
                                     && cast.find(word) == cast.end();
            if (worth_checking)
                for (size_t k = i; k < end; ++k)
                    out[k] = (char)(out[k] - 'A' + 'a');
            i = end;
        }
        return out;
    }

    static bool is_upper_ascii(char c) { return c >= 'A' && c <= 'Z'; }

    /// Below this, an all-caps run is far more likely an acronym than an
    /// emphasised word.
    static constexpr size_t kMinEmphasisWord = 4;

    SpellWorker                            worker_;
    /// Suggestions only — see suggest(). Never used for checking, which is
    /// the worker's job.
    screenplay::spellcheck::SpellChecker   on_demand_;
    std::unordered_map<size_t, Entry>      entries_;
    std::set<std::string>                  last_cast_;
};

} // namespace screenplay::editor
