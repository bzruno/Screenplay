#pragma once
// editor/editor_controller.hpp
// Input FSM for the screenplay editor.
// Features:
//   - Smart undo grouping (typing = one step, pause/block change = new step)
//   - Full SmartType (scene headings, characters, transitions, extensions)
//   - Ctrl+1-6 block type switching
//   - Ctrl+A, Escape, Tab autocomplete acceptance

#include "../model/model.hpp"
#include "../model/undo_stack.hpp"
#include "../utf8_utils.hpp"
#include "autocomplete.hpp"
#include <QString>   // Unicode-aware toUpper for on_char
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace screenplay::editor {

// ─────────────────────────────────────────────────────────────────────────────
// Input abstraction
// ─────────────────────────────────────────────────────────────────────────────
enum class Key {
    Enter, Tab, BackTab, Backspace, Delete,
    Left, Right, Up, Down,
    Home, End, Escape,
    Char,
    Undo, Redo, Save
};

struct KeyEvent {
    Key         key;
    std::string char_utf8;   // UTF-8 encoded character for Key::Char (replaces char)
    bool        ctrl  = false;
    bool        shift = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Editor state (read-only snapshot for renderer)
// ─────────────────────────────────────────────────────────────────────────────
struct EditorState {
    screenplay::Script       script;
    screenplay::Cursor       cursor;
    // Selection: when has_selection is true, the selected range is
    // [min(sel_anchor,cursor), max(sel_anchor,cursor)] in document order.
    screenplay::Cursor       sel_anchor;
    bool                     has_selection  = false;
    std::vector<std::string> suggestions;
    int                      suggestion_idx = -1;
    bool                     dirty          = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// EditorController
// ─────────────────────────────────────────────────────────────────────────────
class EditorController {
public:
    EditorController() {
        state_.script.append(screenplay::BlockType::SceneHeading);
        state_.cursor = { 0, 0 };
        pending_snapshot_ = state_.script;
    }

    // ── Main entry point ──────────────────────────────────────────────────
    void handle_key(const KeyEvent& ev) {
        switch (ev.key) {
        case Key::Enter:     on_enter();              break;
        case Key::Tab:       on_tab(false);           break;
        case Key::BackTab:   on_tab(true);            break;
        case Key::Backspace: on_backspace();          break;
        case Key::Delete:    on_delete_fwd();         break;
        case Key::Left:
            if (ev.shift) extend_selection(-1);
            else { clear_selection(); move_cursor(-1); }
            return;
        case Key::Right:
            if (ev.shift) extend_selection(+1);
            else { clear_selection(); move_cursor(+1); }
            return;
        case Key::Up:
            if (ev.shift) extend_selection_block(-1);
            else { clear_selection(); move_cursor_block(-1); }
            return;
        case Key::Down:
            if (ev.shift) extend_selection_block(+1);
            else { clear_selection(); move_cursor_block(+1); }
            return;
        case Key::Home:
            if (ev.shift) {
                if (!state_.has_selection) state_.sel_anchor = state_.cursor;
                state_.cursor.byte_offset = 0;
                state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
            } else {
                clear_selection();
                state_.cursor.byte_offset = 0;
            }
            return;
        case Key::End:
            if (ev.shift) {
                if (!state_.has_selection) state_.sel_anchor = state_.cursor;
                state_.cursor.byte_offset = current_block().text.size();
                state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
            } else {
                clear_selection();
                state_.cursor.byte_offset = current_block().text.size();
            }
            return;
        case Key::Escape:    dismiss_suggestions(); return;
        case Key::Undo:      on_undo(); return;
        case Key::Redo:      on_redo(); return;
        case Key::Char:      on_char(ev.char_utf8);   break;
        default: return;
        }
        update_autocomplete();
    }

    // ── Public actions ────────────────────────────────────────────────────

    // Ctrl+1-6: force current block to a specific type
    void set_block_type(screenplay::BlockType t) {
        flush_typing_group();
        Command cmd;
        cmd.before        = state_.script;
        cmd.cursor_before = state_.cursor;
        current_block().type = t;
        cmd.after        = state_.script;
        cmd.cursor_after = state_.cursor;
        undo_.push(std::move(cmd));
        state_.dirty = true;
        pending_snapshot_ = state_.script;
        update_autocomplete();
    }

    // Ctrl+A: select entire document
    void select_all() {
        if (state_.script.blocks.empty()) return;
        state_.sel_anchor         = { 0, 0 };
        state_.cursor.block_idx   = state_.script.blocks.size() - 1;
        state_.cursor.byte_offset = state_.script.blocks.back().text.size();
        state_.has_selection      = !cp_equal(state_.sel_anchor, state_.cursor);
    }

    // Mouse press: place cursor, clear selection
    void set_cursor_pos(screenplay::Cursor c) {
        flush_typing_group();
        state_.cursor        = c;
        state_.has_selection = false;
        dismiss_suggestions();
        update_autocomplete();
    }

    // Mouse drag: set anchor + active end (may be equal → no selection)
    void set_selection(screenplay::Cursor anchor, screenplay::Cursor active) {
        flush_typing_group();
        state_.sel_anchor    = anchor;
        state_.cursor        = active;
        state_.has_selection = !cp_equal(anchor, active);
    }

    void set_suggestion_idx(int idx) {
        if (!state_.suggestions.empty())
            state_.suggestion_idx = std::clamp(idx, 0, (int)state_.suggestions.size() - 1);
    }

    void next_suggestion() {
        if (state_.suggestions.empty()) return;
        state_.suggestion_idx =
            (state_.suggestion_idx + 1) % (int)state_.suggestions.size();
    }

    void prev_suggestion() {
        if (state_.suggestions.empty()) return;
        int n = (int)state_.suggestions.size();
        state_.suggestion_idx = (state_.suggestion_idx - 1 + n) % n;
    }

    void dismiss_suggestions() {
        state_.suggestions.clear();
        state_.suggestion_idx = -1;
    }

    void update_autocomplete_public() { update_autocomplete(); }

    // Re-seed built-in autocomplete terms after a language change.
    void reseed_autocomplete() { autocomplete_.reseed(); update_autocomplete(); }

    // Ctrl+D: convert current Dialogue block into a dual-dialogue left column
    // and insert an empty right-column DualDialogue block next to it.
    void activate_dual_dialogue() {
        using BT = screenplay::BlockType;
        if (current_block().type != BT::Dialogue) return;
        record_structural([&] {
            auto& blocks = state_.script.blocks;
            size_t ci = state_.cursor.block_idx;
            blocks[ci].type = BT::DualDialogue;
            blocks.insert(blocks.begin() + (ptrdiff_t)ci + 1,
                screenplay::Block{ BT::DualDialogue, "", state_.script.next_id++ });
            state_.cursor = { ci + 1, 0 };
        });
    }

    void accept_suggestion() {
        if (state_.suggestion_idx < 0 ||
            state_.suggestion_idx >= (int)state_.suggestions.size()) return;

        const std::string& sug = state_.suggestions[state_.suggestion_idx];
        record_structural([&] {
            auto& cb = current_block();
            if (autocomplete_.is_scene_completion(cb.type)) {
                cb.text = autocomplete_.build_scene_completion(cb.text, sug);
            } else if (cb.type == screenplay::BlockType::Parenthetical) {
                std::string inner = (!sug.empty() && sug.front() == '(')
                    ? sug.substr(1, sug.size() > 1 && sug.back() == ')' ? sug.size()-2 : sug.size()-1)
                    : sug;
                inner = QString::fromStdString(inner).toUpper().toStdString();
                cb.text = "(" + inner + ")";
            } else {
                cb.text = sug;
            }
            state_.cursor.byte_offset = cb.text.size();
            autocomplete_.train(cb.type, cb.text);
        });
        dismiss_suggestions();
    }

    // ── Block-range operations (scene organisation — all undoable) ───────
    // Ranges are inclusive [first, last] indices into script.blocks.

    // Insert a fresh, empty block of the given type at `at` and move the
    // cursor to it. `at` is clamped to [0, blocks.size()].
    void insert_block_at(size_t at, screenplay::BlockType t) {
        record_structural([&] {
            auto& blocks = state_.script.blocks;
            at = std::min(at, blocks.size());
            blocks.insert(blocks.begin() + (ptrdiff_t)at,
                screenplay::Block{ t, "", state_.script.next_id++ });
            state_.cursor = { at, 0 };
            state_.has_selection = false;
        });
    }

    // Duplicate [first, last] right after `last`; cursor lands on the copy.
    void duplicate_block_range(size_t first, size_t last) {
        auto& blocks = state_.script.blocks;
        if (first > last || last >= blocks.size()) return;
        record_structural([&] {
            std::vector<screenplay::Block> copy(
                blocks.begin() + (ptrdiff_t)first,
                blocks.begin() + (ptrdiff_t)last + 1);
            for (auto& b : copy) b.id = state_.script.next_id++;
            blocks.insert(blocks.begin() + (ptrdiff_t)last + 1,
                          copy.begin(), copy.end());
            state_.cursor = { last + 1, 0 };
            state_.has_selection = false;
        });
    }

    // Delete [first, last]; keeps the document non-empty.
    void delete_block_range(size_t first, size_t last) {
        auto& blocks = state_.script.blocks;
        if (first > last || last >= blocks.size()) return;
        record_structural([&] {
            blocks.erase(blocks.begin() + (ptrdiff_t)first,
                         blocks.begin() + (ptrdiff_t)last + 1);
            if (blocks.empty())
                state_.script.append(screenplay::BlockType::SceneHeading);
            state_.cursor = { std::min(first, blocks.size() - 1), 0 };
            state_.has_selection = false;
        });
    }

    // std::rotate over [a, c): the sub-range [b, c) ends up before [a, b).
    // Used to swap two adjacent scenes. cursor_block: where to land after.
    void rotate_blocks(size_t a, size_t b, size_t c, size_t cursor_block) {
        auto& blocks = state_.script.blocks;
        if (!(a < b && b < c) || c > blocks.size()) return;
        record_structural([&] {
            std::rotate(blocks.begin() + (ptrdiff_t)a,
                        blocks.begin() + (ptrdiff_t)b,
                        blocks.begin() + (ptrdiff_t)c);
            state_.cursor = { std::min(cursor_block, blocks.size() - 1), 0 };
            state_.has_selection = false;
        });
    }

    // ── Find & Replace (undoable) ─────────────────────────────────────────

    // Replaces the first (or all) occurrences of needle across the script.
    // Returns the number of replacements; 0 leaves the undo stack untouched.
    int replace_text(const std::string& needle, const std::string& repl,
                     bool all) {
        if (needle.empty()) return 0;
        const QString qneedle = QString::fromStdString(needle);
        const QString qrepl   = QString::fromStdString(repl);

        bool any = false;
        for (const auto& b : state_.script.blocks)
            if (QString::fromStdString(b.text).contains(qneedle)) { any = true; break; }
        if (!any) return 0;

        int count = 0;
        record_structural([&] {
            for (auto& b : state_.script.blocks) {
                QString t = QString::fromStdString(b.text);
                if (!t.contains(qneedle)) continue;
                if (all) {
                    count += (int)t.count(qneedle);
                    t.replace(qneedle, qrepl);
                    b.text = t.toStdString();
                } else {
                    t.replace(t.indexOf(qneedle), qneedle.size(), qrepl);
                    b.text = t.toStdString();
                    count = 1;
                    break;
                }
            }
            // Replacements may have shortened the cursor block
            state_.has_selection = false;
            auto& cb = current_block();
            state_.cursor.byte_offset = screenplay::utf8::align_to_cp_start(
                cb.text, std::min(state_.cursor.byte_offset, cb.text.size()));
        });
        return count;
    }

    // Update the title page as a single undoable step
    // (previous implementation reloaded the script, wiping undo history).
    void set_title_page(screenplay::TitlePage tp) {
        record_structural([&] {
            state_.script.title_page = std::move(tp);
        });
    }

    // ── Clipboard helpers ─────────────────────────────────────────────────

    std::string copy_selection() const {
        if (!state_.has_selection) return {};
        auto [s, e] = normalized_selection();
        const auto& blocks = state_.script.blocks;
        if (s.block_idx == e.block_idx)
            return blocks[s.block_idx].text.substr(s.byte_offset,
                                                   e.byte_offset - s.byte_offset);
        std::string r = blocks[s.block_idx].text.substr(s.byte_offset);
        for (size_t i = s.block_idx + 1; i < e.block_idx; ++i)
            r += "\n" + blocks[i].text;
        r += "\n" + blocks[e.block_idx].text.substr(0, e.byte_offset);
        return r;
    }

    void cut_selection() {
        if (!state_.has_selection) return;
        record_structural([this]{ delete_selection_if_active(); });
    }

    void paste(const std::string& utf8) {
        if (utf8.empty()) return;

        // Split into paragraphs: \r stripped, blank lines collapse
        // (screenplay blocks never contain embedded newlines).
        std::vector<std::string> paras;
        std::string cur;
        for (char c : utf8) {
            if (c == '\r') continue;
            if (c == '\n') {
                if (!cur.empty()) paras.push_back(std::move(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) paras.push_back(std::move(cur));
        if (paras.empty()) return;

        record_structural([&]{
            delete_selection_if_active();
            auto& blocks = state_.script.blocks;
            auto& cb     = current_block();

            if (paras.size() == 1) {
                cb.text.insert(state_.cursor.byte_offset, paras[0]);
                state_.cursor.byte_offset += paras[0].size();
                return;
            }

            // Multi-paragraph: first part joins the current block, the rest
            // become new blocks of the same type; the split-off tail of the
            // current block is appended to the last pasted paragraph.
            const std::string tail = cb.text.substr(state_.cursor.byte_offset);
            cb.text = cb.text.substr(0, state_.cursor.byte_offset) + paras[0];

            const auto   type = cb.type;
            const size_t base = state_.cursor.block_idx;
            for (size_t i = 1; i < paras.size(); ++i)
                blocks.insert(blocks.begin() + (ptrdiff_t)(base + i),
                    screenplay::Block{ type, paras[i],
                                       state_.script.next_id++ });

            auto& last = blocks[base + paras.size() - 1];
            const size_t cursor_off = last.text.size();
            last.text += tail;
            state_.cursor = { base + paras.size() - 1, cursor_off };
        });
    }

    // ── Word movement ─────────────────────────────────────────────────────

    void move_word(int dir) {
        flush_typing_group();
        const auto& text = current_block().text;
        size_t pos = state_.cursor.byte_offset;
        const size_t len = text.size();
        if (dir < 0) {
            while (pos > 0 && std::isspace((unsigned char)text[pos-1])) --pos;
            while (pos > 0 && !std::isspace((unsigned char)text[pos-1])) --pos;
        } else {
            while (pos < len && std::isspace((unsigned char)text[pos])) ++pos;
            while (pos < len && !std::isspace((unsigned char)text[pos])) ++pos;
        }
        state_.cursor.byte_offset = pos;
        state_.has_selection = false;
    }

    void extend_selection_word(int dir) {
        if (!state_.has_selection) state_.sel_anchor = state_.cursor;
        const auto& text = current_block().text;
        size_t pos = state_.cursor.byte_offset;
        const size_t len = text.size();
        if (dir < 0) {
            while (pos > 0 && std::isspace((unsigned char)text[pos-1])) --pos;
            while (pos > 0 && !std::isspace((unsigned char)text[pos-1])) --pos;
        } else {
            while (pos < len && std::isspace((unsigned char)text[pos])) ++pos;
            while (pos < len && !std::isspace((unsigned char)text[pos])) ++pos;
        }
        state_.cursor.byte_offset = pos;
        state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
    }

    void delete_word(int dir) {
        record_structural([&]{
            if (state_.has_selection) { delete_selection_if_active(); return; }
            auto& cb  = current_block();
            size_t orig = state_.cursor.byte_offset;
            const size_t len = cb.text.size();
            if (dir < 0) {
                size_t pos = orig;
                while (pos > 0 && std::isspace((unsigned char)cb.text[pos-1])) --pos;
                while (pos > 0 && !std::isspace((unsigned char)cb.text[pos-1])) --pos;
                cb.text.erase(pos, orig - pos);
                state_.cursor.byte_offset = pos;
            } else {
                size_t pos = orig;
                while (pos < len && std::isspace((unsigned char)cb.text[pos])) ++pos;
                while (pos < len && !std::isspace((unsigned char)cb.text[pos])) ++pos;
                cb.text.erase(orig, pos - orig);
            }
        });
    }

    void toggle_bold() {
        record_structural([this]{
            current_block().is_bold_ = !current_block().is_bold_;
        });
    }

    void toggle_italic() {
        record_structural([this]{
            current_block().is_italic_ = !current_block().is_italic_;
        });
    }

    void toggle_underline() {
        record_structural([this]{
            current_block().is_underline_ = !current_block().is_underline_;
        });
    }

    void cursor_to_start() {
        flush_typing_group();
        state_.cursor = { 0, 0 };
        state_.has_selection = false;
    }

    void cursor_to_end() {
        flush_typing_group();
        if (state_.script.blocks.empty()) return;
        state_.cursor.block_idx   = state_.script.blocks.size() - 1;
        state_.cursor.byte_offset = state_.script.blocks.back().text.size();
        state_.has_selection = false;
    }

    void mark_clean()  { state_.dirty = false; }

    void load_script(screenplay::Script s) {
        flush_typing_group();
        state_.script  = std::move(s);
        if (state_.script.blocks.empty())
            state_.script.append(screenplay::BlockType::SceneHeading);
        state_.cursor  = { 0, 0 };
        state_.dirty   = false;
        state_.suggestions.clear();
        state_.suggestion_idx = -1;
        undo_ = UndoStack{};
        pending_snapshot_ = state_.script;
        // Re-train autocomplete from loaded script
        for (const auto& b : state_.script.blocks)
            autocomplete_.train(b.type, b.text);
    }

    const EditorState& state() const { return state_; }

    // Direct mutable script access — use sparingly (bypasses undo)
    screenplay::Script& script_mut() { return state_.script; }

    // Access to autocomplete system (for scene heading completion building)
    const AutocompleteSystem& autocomplete() const { return autocomplete_; }

private:
    EditorState        state_;
    UndoStack          undo_;
    AutocompleteSystem autocomplete_;
    // ── Smart undo grouping ───────────────────────────────────────────────
    // We accumulate typing into a "pending" snapshot.
    // A new undo group starts when:
    //   1. User pauses typing (>800ms)
    //   2. A structural action happens (Enter, Backspace, block type change)
    //   3. The cursor moves to a different block
    screenplay::Script pending_snapshot_;   // state at start of current group
    screenplay::Cursor pending_cursor_;     // cursor at start of current group
    bool               group_open_ = false;

    using Clock = std::chrono::steady_clock;
    Clock::time_point last_char_time_;

    // Call before any structural mutation to close the typing group
    void flush_typing_group() {
        if (!group_open_) return;
        // Push the snapshot captured at group start → current state as "after"
        Command cmd;
        cmd.before        = pending_snapshot_;
        cmd.cursor_before = pending_cursor_;
        cmd.after         = state_.script;
        cmd.cursor_after  = state_.cursor;
        undo_.push(std::move(cmd));
        group_open_       = false;
        pending_snapshot_ = state_.script;
        pending_cursor_   = state_.cursor;
    }

    // Call on every character typed
    void record_char() {
        auto now = Clock::now();
        bool timed_out = group_open_ &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_char_time_).count() > 800;

        if (timed_out) {
            // Pause detected — flush current group and start fresh
            flush_typing_group();
        }

        if (!group_open_) {
            // Start a new group
            pending_snapshot_ = state_.script;
            pending_cursor_   = state_.cursor;
            group_open_       = true;
        }
        last_char_time_ = now;
        state_.dirty = true;
    }

    // For structural commands (Enter, Backspace, block change)
    void record_structural(std::function<void()> mutation) {
        flush_typing_group();
        Command cmd;
        cmd.before        = state_.script;
        cmd.cursor_before = state_.cursor;
        mutation();
        cmd.after        = state_.script;
        cmd.cursor_after = state_.cursor;
        undo_.push(std::move(cmd));
        pending_snapshot_ = state_.script;
        pending_cursor_   = state_.cursor;
        state_.dirty = true;
    }

    // ── Helpers ───────────────────────────────────────────────────────────
    screenplay::Block& current_block() {
        if (state_.cursor.block_idx >= state_.script.blocks.size())
            state_.cursor.block_idx = state_.script.blocks.size() - 1;
        return state_.script.blocks[state_.cursor.block_idx];
    }

    // ── Key handlers ──────────────────────────────────────────────────────
    void on_enter() {
        // NOTE: suggestion acceptance via Enter is handled exclusively by the
        // UI layer (main.cpp popup handler, which guards on popup_->is_visible()).
        // Accepting here would fire when the popup is closed but suggestions are
        // still live in state_ (e.g. after focusOutEvent hides the popup without
        // calling dismiss_suggestions), causing Enter to silently accept instead
        // of splitting the block.

        record_structural([this] {
            delete_selection_if_active();   // collapse selection before split

            using BT = screenplay::BlockType;
            auto& blocks   = state_.script.blocks;
            auto& cb       = current_block();
            auto  cur_type = cb.type;

            // Train autocomplete before splitting
            autocomplete_.train(cb.type, cb.text);

            // Determine next block type
            BT next;
            switch (cur_type) {
            case BT::SceneHeading: next = BT::Action;      break;
            case BT::Action:       next = BT::Action;      break;
            case BT::Character:    next = BT::Dialogue;    break;
            case BT::Parenthetical:next = BT::Dialogue;    break;
            case BT::Dialogue:
                // Short dialogue → likely another character; long → action
                next = (cb.text.size() < 80) ? BT::Character : BT::Action;
                break;
            case BT::Transition:   next = BT::SceneHeading; break;
            default:               next = BT::Action;
            }

            // Split text at cursor
            std::string tail = cb.text.substr(state_.cursor.byte_offset);
            cb.text          = cb.text.substr(0, state_.cursor.byte_offset);

            size_t insert_at = state_.cursor.block_idx + 1;
            blocks.insert(blocks.begin() + (ptrdiff_t)insert_at,
                screenplay::Block{ next, tail, state_.script.next_id++ });
            state_.cursor = { insert_at, 0 };
            update_autocomplete();
        });
    }

    void on_tab(bool reverse) {
        // Accept suggestion on Tab
        if (!reverse && !state_.suggestions.empty()) {
            accept_suggestion();
            return;
        }

        using BT = screenplay::BlockType;
        static constexpr BT cycle[] = {
            BT::SceneHeading, BT::Action, BT::Character,
            BT::Parenthetical, BT::Dialogue, BT::Transition
        };
        constexpr int N = (int)std::size(cycle);

        record_structural([&] {
            auto& t = current_block().type;
            int idx = 0;
            for (int i = 0; i < N; ++i) if (cycle[i] == t) { idx = i; break; }
            t = cycle[(idx + (reverse ? N - 1 : 1)) % N];
        });
    }

    void on_backspace() {
        record_structural([this] {
            if (state_.has_selection) { delete_selection_if_active(); return; }
            auto& cb = current_block();
            if (state_.cursor.byte_offset > 0) {
                // Walk back over continuation bytes to find the codepoint start
                size_t new_pos = screenplay::utf8::prev_cp(cb.text, state_.cursor.byte_offset);
                size_t del_len = state_.cursor.byte_offset - new_pos;
                cb.text.erase(new_pos, del_len);
                state_.cursor.byte_offset = new_pos;
            } else if (state_.cursor.block_idx > 0) {
                auto& blocks = state_.script.blocks;
                auto& prev   = blocks[state_.cursor.block_idx - 1];
                size_t merge_pt = prev.text.size();
                prev.text += cb.text;
                blocks.erase(blocks.begin() + (ptrdiff_t)state_.cursor.block_idx);
                state_.cursor = { state_.cursor.block_idx - 1, merge_pt };
            }
        });
    }

    void on_delete_fwd() {
        record_structural([this] {
            if (state_.has_selection) { delete_selection_if_active(); return; }
            auto& cb = current_block();
            if (state_.cursor.byte_offset < cb.text.size()) {
                size_t del_len = screenplay::utf8::codepoint_len(
                    cb.text, state_.cursor.byte_offset);
                cb.text.erase(state_.cursor.byte_offset, del_len);
            } else if (state_.cursor.block_idx + 1 < state_.script.blocks.size()) {
                auto& blocks = state_.script.blocks;
                auto& nxt    = blocks[state_.cursor.block_idx + 1];
                cb.text += nxt.text;
                blocks.erase(blocks.begin() + (ptrdiff_t)(state_.cursor.block_idx + 1));
            }
        });
    }

    void on_char(const std::string& utf8_char) {
        if (utf8_char.empty()) return;

        if (state_.has_selection) {
            // Replace selection — treat as a structural change for undo
            record_structural([&] {
                delete_selection_if_active();
                auto& cb2 = current_block();
                std::string ins = utf8_char;
                if (cb2.type == screenplay::BlockType::Character ||
                    cb2.type == screenplay::BlockType::SceneHeading ||
                    cb2.type == screenplay::BlockType::Parenthetical)
                    ins = QString::fromStdString(ins).toUpper().toStdString();
                cb2.text.insert(state_.cursor.byte_offset, ins);
                state_.cursor.byte_offset += ins.size();
            });
            return;
        }

        auto& cb = current_block();
        // Unicode-aware uppercase for Character and SceneHeading blocks
        std::string to_insert = utf8_char;
        if (cb.type == screenplay::BlockType::Character ||
            cb.type == screenplay::BlockType::SceneHeading ||
            cb.type == screenplay::BlockType::Parenthetical)
            to_insert = QString::fromStdString(utf8_char).toUpper().toStdString();

        record_char();   // open/continue typing group
        cb.text.insert(state_.cursor.byte_offset, to_insert);
        state_.cursor.byte_offset += to_insert.size();
    }

    void on_undo() {
        flush_typing_group();
        if (auto cmd = undo_.undo()) {
            state_.script = cmd->before;
            state_.cursor = cmd->cursor_before;
            state_.dirty  = true;
            pending_snapshot_ = state_.script;
            pending_cursor_   = state_.cursor;
            rebuild_autocomplete_from_script();
        }
    }

    void on_redo() {
        if (auto cmd = undo_.redo()) {
            state_.script = cmd->after;
            state_.cursor = cmd->cursor_after;
            state_.dirty  = true;
            pending_snapshot_ = state_.script;
            pending_cursor_   = state_.cursor;
            rebuild_autocomplete_from_script();
        }
    }

    void move_cursor(int delta) {
        auto& blocks = state_.script.blocks;
        auto& cb     = blocks[state_.cursor.block_idx];

        if (delta < 0) {
            if (state_.cursor.byte_offset > 0) {
                state_.cursor.byte_offset =
                    screenplay::utf8::prev_cp(cb.text, state_.cursor.byte_offset);
            } else if (state_.cursor.block_idx > 0) {
                flush_typing_group();
                --state_.cursor.block_idx;
                state_.cursor.byte_offset = blocks[state_.cursor.block_idx].text.size();
            }
        } else {
            if (state_.cursor.byte_offset < cb.text.size()) {
                state_.cursor.byte_offset =
                    screenplay::utf8::next_cp(cb.text, state_.cursor.byte_offset);
            } else if (state_.cursor.block_idx + 1 < blocks.size()) {
                flush_typing_group();
                ++state_.cursor.block_idx;
                state_.cursor.byte_offset = 0;
            }
        }
    }

    void move_cursor_block(int delta) {
        flush_typing_group();
        auto& blocks = state_.script.blocks;
        int bi = std::clamp((int)state_.cursor.block_idx + delta,
                            0, (int)blocks.size() - 1);
        state_.cursor.block_idx = (size_t)bi;

        // Clamp to text length and align to codepoint boundary
        const auto& txt = blocks[bi].text;
        size_t off = std::min(state_.cursor.byte_offset, txt.size());
        state_.cursor.byte_offset = screenplay::utf8::align_to_cp_start(txt, off);
    }

    // ── Selection helpers ─────────────────────────────────────────────────

    // Returns true if cursor a is before b in document order.
    static bool cp_before(const screenplay::Cursor& a, const screenplay::Cursor& b) {
        if (a.block_idx != b.block_idx) return a.block_idx < b.block_idx;
        return a.byte_offset < b.byte_offset;
    }
    static bool cp_equal(const screenplay::Cursor& a, const screenplay::Cursor& b) {
        return a.block_idx == b.block_idx && a.byte_offset == b.byte_offset;
    }

    // Returns {start, end} so that start <= end in document order.
    std::pair<screenplay::Cursor, screenplay::Cursor> normalized_selection() const {
        if (cp_before(state_.cursor, state_.sel_anchor))
            return { state_.cursor, state_.sel_anchor };
        return { state_.sel_anchor, state_.cursor };
    }

    void clear_selection() {
        state_.has_selection = false;
    }

    // Extend selection by moving cursor one step (±1 codepoint).
    void extend_selection(int delta) {
        if (!state_.has_selection) state_.sel_anchor = state_.cursor;
        move_cursor(delta);
        state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
    }

    // Extend selection by moving cursor one block.
    void extend_selection_block(int delta) {
        if (!state_.has_selection) state_.sel_anchor = state_.cursor;
        move_cursor_block(delta);
        state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
    }

    // Delete the selected range. Leaves cursor at selection start.
    // Must be called INSIDE a record_structural() lambda.
    void delete_selection_if_active() {
        if (!state_.has_selection) return;
        auto [sel_s, sel_e] = normalized_selection();
        auto& blocks = state_.script.blocks;

        if (sel_s.block_idx == sel_e.block_idx) {
            auto& text = blocks[sel_s.block_idx].text;
            text.erase(sel_s.byte_offset, sel_e.byte_offset - sel_s.byte_offset);
        } else {
            // Keep [0, sel_s.byte_offset) from start block and
            // [sel_e.byte_offset, end) from end block; merge them.
            std::string head = blocks[sel_s.block_idx].text.substr(0, sel_s.byte_offset);
            std::string tail = blocks[sel_e.block_idx].text.substr(sel_e.byte_offset);
            blocks[sel_s.block_idx].text = head + tail;
            blocks.erase(blocks.begin() + (ptrdiff_t)sel_s.block_idx + 1,
                         blocks.begin() + (ptrdiff_t)sel_e.block_idx + 1);
        }
        state_.cursor       = sel_s;
        state_.has_selection = false;
    }

    // ── Autocomplete ──────────────────────────────────────────────────────

    // Discard all learned data and re-train from the current script state.
    // Call after undo/redo so stale completions don't survive content deletion.
    void rebuild_autocomplete_from_script() {
        autocomplete_.reset();
        for (const auto& b : state_.script.blocks)
            autocomplete_.train(b.type, b.text);
        update_autocomplete();
    }

    void update_autocomplete() {
        auto& cb = current_block();
        state_.suggestions = autocomplete_.query(cb.type, cb.text, state_.cursor.byte_offset);
        state_.suggestion_idx = state_.suggestions.empty() ? -1 : 0;
    }
};

} // namespace screenplay::editor
