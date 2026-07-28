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
#include "../parsing/paste_parser.hpp"
#include "../production/scene_numbering.hpp"
#include "../utf8_utils.hpp"
#include "autocomplete.hpp"
#include <QString>   // Unicode-aware toUpper for on_char
#include <functional>
#include <optional>
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
        record_structural([&] {
            strip_parenthetical_wrapping_if_leaving(t);
            current_block().type = t;
        });
        update_autocomplete();
    }

    // ── Alignment override ────────────────────────────────────────────────
    // Sets the current block's alignment. Toggling: re-applying the alignment
    // a block already has returns it to Default (the type's own alignment),
    // so the three toolbar buttons behave like a tri-state group rather than a
    // one-way door. Callers should gate on layout::supports_alignment() first;
    // this method is defensive but does not know about layout.
    void set_block_align(screenplay::BlockAlign a) {
        record_structural([&] {
            auto& b = current_block();
            b.align = (b.align == a) ? screenplay::BlockAlign::Default : a;
        });
    }


    // ── Production: revisions, locked scenes, omitted scenes ──────────────

    screenplay::Revision current_revision() const {
        return state_.script.current_revision;
    }
    bool scenes_locked() const { return state_.script.scenes_locked; }

    /// Opens a new revision pass. From here every edit stamps its block, so
    /// the margin marks build up on their own.
    void set_current_revision(screenplay::Revision r) {
        record_structural([&] { state_.script.current_revision = r; });
    }

    /// Clears every mark — what you do when the coloured pages have gone out
    /// and the next pass starts from a clean margin.
    void clear_revision_marks() {
        record_structural([&] {
            for (auto& b : state_.script.blocks)
                b.revision = screenplay::Revision::None;
        });
    }

    bool has_revision_marks() const {
        for (const auto& b : state_.script.blocks)
            if (b.revision != screenplay::Revision::None) return true;
        return false;
    }

    /// Freezes scene numbers so nothing already scheduled can shift.
    void lock_scenes() {
        record_structural([&] { production::lock_scenes(state_.script); });
    }
    void unlock_scenes() {
        record_structural([&] { production::unlock_scenes(state_.script); });
    }

    /// The scene heading governing the caret, if the caret is inside a scene.
    std::optional<size_t> current_scene_heading() const {
        for (int i = (int)state_.cursor.block_idx; i >= 0; --i)
            if (state_.script.blocks[(size_t)i].type
                    == screenplay::BlockType::SceneHeading)
                return (size_t)i;
        return std::nullopt;
    }

    /// Cuts the scene the caret is in: its body goes, its heading keeps the
    /// number and reads OMITTED. Undoable, so "restore" is Ctrl+Z — the number
    /// is what production cares about, and it never moves either way.
    void omit_current_scene() {
        const auto heading = current_scene_heading();
        if (!heading) return;
        record_structural([&] {
            const size_t at = *heading;
            size_t end = at + 1;
            while (end < state_.script.blocks.size() &&
                   state_.script.blocks[end].type
                       != screenplay::BlockType::SceneHeading) ++end;
            auto& head = state_.script.blocks[at];
            head.text    = production::kOmittedText;
            head.omitted = true;
            head.bold_runs.clear();
            head.italic_runs.clear();
            head.underline_runs.clear();
            state_.script.blocks.erase(
                state_.script.blocks.begin() + (long)(at + 1),
                state_.script.blocks.begin() + (long)end);
            state_.cursor = { at, head.text.size() };
        });
    }

    /// Forces the current block to open a page, or releases it if it already
    /// does — one action, so the same key both sets and clears the break.
    void toggle_page_break() {
        record_structural([&] {
            auto& b = current_block();
            b.page_break_before = !b.page_break_before;
        });
    }

    bool current_has_page_break() const {
        if (state_.cursor.block_idx >= state_.script.blocks.size()) return false;
        return state_.script.blocks[state_.cursor.block_idx].page_break_before;
    }

    bool current_scene_is_omitted() const {
        const auto heading = current_scene_heading();
        return heading && state_.script.blocks[*heading].omitted;
    }

    // ── Author notes ──────────────────────────────────────────────────────
    // Setting an empty string clears the note. Undoable like any other edit,
    // so an accidentally deleted note comes back with Ctrl+Z.
    void set_block_note(std::string note) {
        flush_typing_group();
        Command cmd;
        cmd.before        = state_.script;
        cmd.cursor_before = state_.cursor;
        current_block().note = std::move(note);
        cmd.after        = state_.script;
        cmd.cursor_after = state_.cursor;
        undo_.push(std::move(cmd));
        state_.dirty = true;
        pending_snapshot_ = state_.script;
    }

    std::string current_block_note() const {
        if (state_.script.blocks.empty() ||
                state_.cursor.block_idx >= state_.script.blocks.size())
            return {};
        return state_.script.blocks[state_.cursor.block_idx].note;
    }

    screenplay::BlockType current_block_type() const {
        if (state_.script.blocks.empty() ||
                state_.cursor.block_idx >= state_.script.blocks.size())
            return screenplay::BlockType::Action;
        return state_.script.blocks[state_.cursor.block_idx].type;
    }

    // Ctrl+4: turn the current block into a Parenthetical with structural
    // parentheses. An empty block becomes "()" with the caret between them;
    // a block with text is wrapped as "(text)" so nothing is lost. Atomic —
    // one undo step — so the parentheses always appear together.
    void make_parenthetical() {
        record_structural([this] {
            auto& b = current_block();
            const bool already_wrapped = is_paren_wrapped(b.text);
            std::string inner = already_wrapped
                ? b.text.substr(1, b.text.size() - 2)   // avoid double-wrapping
                : b.text;
            b.type = screenplay::BlockType::Parenthetical;
            b.text = "(" + inner + ")";
            // Wrapping fresh text prepends "(" — every existing run shifts by
            // one byte. Already-wrapped text is unchanged (identity), so runs
            // stay put.
            if (!already_wrapped) {
                screenplay::style_shift_insert(b.bold_runs,      0, 1);
                screenplay::style_shift_insert(b.italic_runs,    0, 1);
                screenplay::style_shift_insert(b.underline_runs, 0, 1);
            }
            state_.cursor.byte_offset = inner.empty() ? 1 : b.text.size() - 1;
        });
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

    // Ctrl+Shift+A: select only the current block (e.g. just the Dialogue
    // the caret is in), not the whole document.
    void select_current_block() {
        if (state_.script.blocks.empty()) return;
        const auto& blk = current_block();
        state_.sel_anchor         = { state_.cursor.block_idx, 0 };
        state_.cursor.byte_offset = blk.text.size();
        state_.has_selection      = !cp_equal(state_.sel_anchor, state_.cursor);
    }

    // Visual-line Up/Down: only the canvas knows the wrapped-line layout, so
    // it computes the target Cursor and hands it here. Mirrors
    // move_cursor_block()'s selection semantics (extend vs replace) so plain
    // and Shift+ Up/Down behave consistently whether moving by block or by
    // visual line.
    void move_cursor_to(screenplay::Cursor c, bool extend) {
        flush_typing_group();
        if (extend) {
            if (!state_.has_selection) state_.sel_anchor = state_.cursor;
            state_.cursor = c;
            state_.has_selection = !cp_equal(state_.cursor, state_.sel_anchor);
        } else {
            state_.has_selection = false;
            state_.cursor = c;
        }
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

    // The UI owns the suggestion popup; this is how it tells the controller
    // whether that popup is currently on screen. Keys behave differently
    // depending on whether the writer can actually SEE the list (see on_tab).
    void set_suggestions_visible(bool v) { suggestions_visible_ = v; }

    void dismiss_suggestions() {
        suggestions_visible_ = false;
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
            } else if (cb.type == screenplay::BlockType::Character &&
                       autocomplete_.is_character_extension(
                           cb.text, state_.cursor.byte_offset)) {
                // Completing a Character Extension: "JOÃO (V" → "JOÃO (V.O.)".
                cb.text = autocomplete_.build_character_extension(
                    cb.text, state_.cursor.byte_offset, sug);
            } else {
                cb.text = sug;
            }
            // The text was rewritten wholesale — any existing style ranges
            // would now point at meaningless positions. Scene Heading /
            // Character cues are structural, one-line labels that are never
            // meant to carry manual bold/italic/underline, so clearing is safe.
            cb.bold_runs.clear();
            cb.italic_runs.clear();
            cb.underline_runs.clear();
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
                // Find & Replace can shift/rewrite several occurrences at once
                // within a block; correctly re-deriving each surviving style
                // range through that isn't worth the complexity here, so the
                // block's styling is cleared rather than left pointing at the
                // wrong (now-meaningless) positions.
                if (all) {
                    count += (int)t.count(qneedle);
                    t.replace(qneedle, qrepl);
                    b.text = t.toStdString();
                } else {
                    t.replace(t.indexOf(qneedle), qneedle.size(), qrepl);
                    b.text = t.toStdString();
                    count = 1;
                }
                b.bold_runs.clear();
                b.italic_runs.clear();
                b.underline_runs.clear();
                if (!all) break;
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
                screenplay::style_shift_insert(cb.bold_runs,      state_.cursor.byte_offset, paras[0].size());
                screenplay::style_shift_insert(cb.italic_runs,    state_.cursor.byte_offset, paras[0].size());
                screenplay::style_shift_insert(cb.underline_runs, state_.cursor.byte_offset, paras[0].size());
                cb.text.insert(state_.cursor.byte_offset, paras[0]);
                state_.cursor.byte_offset += paras[0].size();
                return;
            }

            // Multi-paragraph: recognize each pasted paragraph as its own
            // screenplay element (Scene Heading, Character, Dialogue, …) rather
            // than forcing them all to the current block's type. The first part
            // joins the current block; the split-off tail of the current block
            // is appended to the last pasted paragraph.
            const auto types = parse::classify_paragraphs(paras);

            const size_t at   = state_.cursor.byte_offset;
            const size_t full = cb.text.size();
            const std::string head = cb.text.substr(0, at);
            const std::string tail = cb.text.substr(at);
            const bool cb_was_empty = head.empty() && tail.empty();

            // cb keeps only its head's runs (paras[0] appended after is new,
            // unstyled, plain-pasted text); its own erased tail's runs are
            // extracted now and carried over to the last new block below.
            screenplay::StyleRuns tail_bold      = screenplay::style_extract(cb.bold_runs,      at, full);
            screenplay::StyleRuns tail_italic    = screenplay::style_extract(cb.italic_runs,    at, full);
            screenplay::StyleRuns tail_underline = screenplay::style_extract(cb.underline_runs, at, full);
            screenplay::style_remove(cb.bold_runs,      at, full);
            screenplay::style_remove(cb.italic_runs,    at, full);
            screenplay::style_remove(cb.underline_runs, at, full);

            cb.text = head + paras[0];
            // Adopt the detected type only when pasting onto an empty line; when
            // pasting into existing text, the current element's type is kept.
            if (cb_was_empty) cb.type = types[0];

            const size_t base = state_.cursor.block_idx;
            for (size_t i = 1; i < paras.size(); ++i)
                blocks.insert(blocks.begin() + (ptrdiff_t)(base + i),
                    screenplay::Block{ types[i], paras[i],
                                       state_.script.next_id++ });

            auto& last = blocks[base + paras.size() - 1];
            const size_t cursor_off = last.text.size();
            screenplay::style_append_shifted(last.bold_runs,      tail_bold,      cursor_off);
            screenplay::style_append_shifted(last.italic_runs,    tail_italic,    cursor_off);
            screenplay::style_append_shifted(last.underline_runs, tail_underline, cursor_off);
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
                screenplay::style_shift_erase(cb.bold_runs,      pos, orig - pos);
                screenplay::style_shift_erase(cb.italic_runs,    pos, orig - pos);
                screenplay::style_shift_erase(cb.underline_runs, pos, orig - pos);
                state_.cursor.byte_offset = pos;
            } else {
                size_t pos = orig;
                while (pos < len && std::isspace((unsigned char)cb.text[pos])) ++pos;
                while (pos < len && !std::isspace((unsigned char)cb.text[pos])) ++pos;
                cb.text.erase(orig, pos - orig);
                screenplay::style_shift_erase(cb.bold_runs,      orig, pos - orig);
                screenplay::style_shift_erase(cb.italic_runs,    orig, pos - orig);
                screenplay::style_shift_erase(cb.underline_runs, orig, pos - orig);
            }
        });
    }

    // With an active selection, toggles the style over exactly the selected
    // range(s) — never the whole block. "Toggle" means: if the selection is
    // already uniformly styled, remove it; otherwise apply it to the whole
    // selection (matching Word/most editors' mixed-selection convention).
    // With no selection, falls back to whole-block toggle (the pre-existing
    // behaviour for "format this whole line").
    void toggle_bold()      { toggle_style(&screenplay::Block::bold_runs); }
    void toggle_italic()    { toggle_style(&screenplay::Block::italic_runs); }
    void toggle_underline() { toggle_style(&screenplay::Block::underline_runs); }

    // Read-only counterpart of toggle_style's "is this already fully styled?"
    // check — drives the Bold/Italic/Underline button checked-state so it
    // reflects the SELECTION (or, with none, the whole current block),
    // exactly matching what clicking the button would toggle off.
    bool style_is_active(screenplay::StyleRuns screenplay::Block::* member) const {
        const auto& blocks = state_.script.blocks;
        if (!state_.has_selection) {
            const auto& b = blocks[state_.cursor.block_idx];
            return !b.text.empty() && screenplay::style_covers(b.*member, 0, b.text.size());
        }
        auto [sel_s, sel_e] = normalized_selection();
        bool any = false;
        for (size_t bi = sel_s.block_idx; bi <= sel_e.block_idx; ++bi) {
            const size_t s = (bi == sel_s.block_idx) ? sel_s.byte_offset : 0;
            const size_t e = (bi == sel_e.block_idx) ? sel_e.byte_offset : blocks[bi].text.size();
            if (s >= e) continue;
            any = true;
            if (!screenplay::style_covers(blocks[bi].*member, s, e)) return false;
        }
        return any;
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
    // Mirrors whether the UI's suggestion popup is on screen. Not part of
    // EditorState because it is a property of the VIEW, not the document —
    // the controller only needs it to decide what Tab means.
    bool               suggestions_visible_ = false;
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
        stamp_revision();       // before the caller mutates: lands in `after`
    }

    // For structural commands (Enter, Backspace, block change)
    void record_structural(std::function<void()> mutation) {
        flush_typing_group();
        Command cmd;
        cmd.before        = state_.script;
        cmd.cursor_before = state_.cursor;
        mutation();
        stamp_revision();
        renumber_if_locked();
        cmd.after        = state_.script;
        cmd.cursor_after = state_.cursor;
        undo_.push(std::move(cmd));
        pending_snapshot_ = state_.script;
        pending_cursor_   = state_.cursor;
        state_.dirty = true;
    }

    // ── Production bookkeeping, folded into every edit ────────────────────
    // Both run INSIDE the command being recorded, so the mark and the new
    // scene number travel with the edit through undo and redo instead of
    // being applied afterwards and surviving a Ctrl+Z.

    /// Marks the block under the caret as belonging to the open pass.
    void stamp_revision() {
        if (state_.script.current_revision == screenplay::Revision::None) return;
        if (state_.cursor.block_idx >= state_.script.blocks.size()) return;
        state_.script.blocks[state_.cursor.block_idx].revision =
            state_.script.current_revision;
    }

    /// Gives a freshly created scene its letter-suffixed number. Existing
    /// numbers are never touched, so this is safe to run after every edit.
    void renumber_if_locked() {
        if (state_.script.scenes_locked)
            production::assign_missing_numbers(state_.script);
    }

    // ── Helpers ───────────────────────────────────────────────────────────
    screenplay::Block& current_block() {
        if (state_.cursor.block_idx >= state_.script.blocks.size())
            state_.cursor.block_idx = state_.script.blocks.size() - 1;
        return state_.script.blocks[state_.cursor.block_idx];
    }

    // Shared implementation for toggle_bold/italic/underline. `member` picks
    // which of the block's three StyleRuns lists to operate on, so the
    // selection-range-vs-whole-block logic is written exactly once.
    void toggle_style(screenplay::StyleRuns screenplay::Block::* member) {
        record_structural([&] {
            auto& blocks = state_.script.blocks;
            if (!state_.has_selection) {
                auto& cb = current_block();
                screenplay::StyleRuns& runs = cb.*member;
                const bool whole = !cb.text.empty() &&
                    screenplay::style_covers(runs, 0, cb.text.size());
                if (whole) screenplay::style_remove(runs, 0, cb.text.size());
                else       screenplay::style_add(runs, 0, cb.text.size());
                return;
            }

            auto [sel_s, sel_e] = normalized_selection();

            // First pass: is the whole selection already uniformly styled?
            bool all_styled = true;
            for (size_t bi = sel_s.block_idx; bi <= sel_e.block_idx && all_styled; ++bi) {
                const size_t s = (bi == sel_s.block_idx) ? sel_s.byte_offset : 0;
                const size_t e = (bi == sel_e.block_idx) ? sel_e.byte_offset
                                                          : blocks[bi].text.size();
                if (s >= e) continue;
                if (!screenplay::style_covers(blocks[bi].*member, s, e)) all_styled = false;
            }

            // Second pass: apply the opposite operation to exactly the
            // selected byte range in every affected block — never the parts
            // of a block outside the selection.
            for (size_t bi = sel_s.block_idx; bi <= sel_e.block_idx; ++bi) {
                const size_t s = (bi == sel_s.block_idx) ? sel_s.byte_offset : 0;
                const size_t e = (bi == sel_e.block_idx) ? sel_e.byte_offset
                                                          : blocks[bi].text.size();
                if (s >= e) continue;
                auto& runs = blocks[bi].*member;
                if (all_styled) screenplay::style_remove(runs, s, e);
                else            screenplay::style_add(runs, s, e);
            }
        });
    }

    // A Parenthetical whose text is wrapped in structural parentheses "(…)".
    static bool is_paren_wrapped(const std::string& text) {
        return text.size() >= 2 && text.front() == '(' && text.back() == ')';
    }

    // Called whenever the CURRENT block is about to leave Parenthetical for
    // any other type (Ctrl+1-6 / Format menu / Tab cycling). The "()" are
    // structural — Ctrl+4's own doing, never typed by the writer — so they
    // must not linger as literal text once the block stops being a
    // Parenthetical. No-op if the text isn't actually paren-wrapped (e.g. the
    // writer deleted a paren manually before switching type).
    void strip_parenthetical_wrapping_if_leaving(screenplay::BlockType next_type) {
        auto& cb = current_block();
        if (cb.type != screenplay::BlockType::Parenthetical ||
            next_type == screenplay::BlockType::Parenthetical ||
            !is_paren_wrapped(cb.text)) return;

        const size_t old_size = cb.text.size();
        // Erase the trailing ')' first, then the leading '(' — same order as
        // the actual text mutation below, so each shift lands on the string
        // shape it was computed against.
        auto strip = [&](screenplay::StyleRuns& runs) {
            screenplay::style_shift_erase(runs, old_size - 1, 1);
            screenplay::style_shift_erase(runs, 0, 1);
        };
        strip(cb.bold_runs);
        strip(cb.italic_runs);
        strip(cb.underline_runs);
        cb.text = cb.text.substr(1, old_size - 2);

        // Cursor was inside "(...)"; the leading '(' it may have been past
        // is gone, so shift left by one and clamp to the shorter text.
        size_t off = state_.cursor.byte_offset;
        off = (off == 0) ? 0 : off - 1;
        state_.cursor.byte_offset = std::min(off, cb.text.size());
    }

    // An element with no writer content. A Ctrl+4 Parenthetical still reduced to
    // its structural "()" counts as empty — its parentheses are not content.
    static bool is_empty_element(const screenplay::Block& b) {
        return b.text.empty() ||
            (b.type == screenplay::BlockType::Parenthetical && b.text == "()");
    }

    // Erase the current block, keeping the document non-empty. Caller must have
    // verified there is more than one block. `land_prev` puts the caret at the
    // end of the previous block (Backspace feel); otherwise at the start of the
    // block that slid into its place (Delete feel).
    void remove_current_block(bool land_prev) {
        auto& blocks = state_.script.blocks;
        size_t bi = state_.cursor.block_idx;
        blocks.erase(blocks.begin() + (ptrdiff_t)bi);
        if (land_prev && bi > 0) {
            state_.cursor.block_idx   = bi - 1;
            state_.cursor.byte_offset = blocks[bi - 1].text.size();
        } else {
            state_.cursor.block_idx   = std::min(bi, blocks.size() - 1);
            state_.cursor.byte_offset = 0;
        }
        state_.has_selection = false;
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

            // Empty Dialogue + Enter → convert this line to Action in place.
            // The writer asked for a new speech, then had nothing to say: rather
            // than leave an empty Character cue behind, the line drops out to
            // Action, which is where a scene continues.
            if (cur_type == BT::Dialogue && cb.text.empty()) {
                cb.type = BT::Action;
                update_autocomplete();
                return;
            }

            // Determine next block type. Enter is deterministic — it never uses
            // the length of the text to decide the next element.
            BT next;
            switch (cur_type) {
            case BT::SceneHeading: next = BT::Action;      break;
            case BT::Action:       next = BT::Action;      break;
            case BT::Character:    next = BT::Dialogue;    break;
            // A wrylie interrupts a speech, it does not end it: Enter returns
            // to the same character's dialogue.
            case BT::Parenthetical:next = BT::Dialogue;    break;
            // Enter at the end of a spoken line means the speech is over, so
            // the next thing to write is who speaks next. Continuing the same
            // speech is what the wrylie and plain wrapping are for.
            case BT::Dialogue:     next = BT::Character;   break;
            case BT::Transition:   next = BT::SceneHeading; break;
            default:               next = BT::Action;
            }

            // Split text at cursor — except a Parenthetical, whose parentheses
            // are structural and must never be split across two blocks. Enter in
            // a Parenthetical keeps "(…)" intact and starts an empty Dialogue.
            std::string tail;
            screenplay::StyleRuns tail_bold, tail_italic, tail_underline;
            if (cur_type == BT::Parenthetical) {
                tail = "";   // nothing moves — cb's own runs are untouched
            } else {
                const size_t at = state_.cursor.byte_offset;
                const size_t full = cb.text.size();
                tail_bold      = screenplay::style_extract(cb.bold_runs,      at, full);
                tail_italic    = screenplay::style_extract(cb.italic_runs,    at, full);
                tail_underline = screenplay::style_extract(cb.underline_runs, at, full);
                screenplay::style_remove(cb.bold_runs,      at, full);
                screenplay::style_remove(cb.italic_runs,    at, full);
                screenplay::style_remove(cb.underline_runs, at, full);
                tail    = cb.text.substr(at);
                cb.text = cb.text.substr(0, at);
            }

            size_t insert_at = state_.cursor.block_idx + 1;
            blocks.insert(blocks.begin() + (ptrdiff_t)insert_at,
                screenplay::Block{ next, tail, state_.script.next_id++ });
            blocks[insert_at].bold_runs      = std::move(tail_bold);
            blocks[insert_at].italic_runs    = std::move(tail_italic);
            blocks[insert_at].underline_runs = std::move(tail_underline);
            state_.cursor = { insert_at, 0 };
            update_autocomplete();
        });
    }

    void on_tab(bool reverse) {
        // Tab accepts a suggestion ONLY while the list is actually on screen.
        //
        // Testing `!suggestions.empty()` alone was a bug: a Transition (and a
        // Character) block is seeded with suggestions the instant the caret
        // arrives, so Tab in an empty Transition silently typed "CUT TO:"
        // instead of moving to the next element. The controller cannot see the
        // popup itself, so the UI reports it via set_suggestions_visible().
        if (!reverse && suggestions_visible_ && !state_.suggestions.empty()) {
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
            BT t = current_block().type;
            int idx = 0;
            for (int i = 0; i < N; ++i) if (cycle[i] == t) { idx = i; break; }
            BT next = cycle[(idx + (reverse ? N - 1 : 1)) % N];
            strip_parenthetical_wrapping_if_leaving(next);
            current_block().type = next;
        });
    }

    void on_backspace() {
        if (!state_.has_selection) {
            auto& cb = current_block();
            // Empty "()" parenthetical: Backspace removes the whole element, so
            // an unwanted Ctrl+4 is never a dead-end.
            if (cb.type == screenplay::BlockType::Parenthetical &&
                cb.text == "()" && state_.script.blocks.size() > 1) {
                record_structural([this] { remove_current_block(/*land_prev=*/true); });
                return;
            }
            // Protect a non-empty Parenthetical's opening "(": Backspace at or
            // before it is a no-op, so the parentheses can never be left
            // unbalanced.
            if (cb.type == screenplay::BlockType::Parenthetical &&
                is_paren_wrapped(cb.text) && state_.cursor.byte_offset <= 1)
                return;
        }
        record_structural([this] {
            if (state_.has_selection) { delete_selection_if_active(); return; }
            auto& cb = current_block();
            if (state_.cursor.byte_offset > 0) {
                // Walk back over continuation bytes to find the codepoint start
                size_t new_pos = screenplay::utf8::prev_cp(cb.text, state_.cursor.byte_offset);
                size_t del_len = state_.cursor.byte_offset - new_pos;
                cb.text.erase(new_pos, del_len);
                screenplay::style_shift_erase(cb.bold_runs,      new_pos, del_len);
                screenplay::style_shift_erase(cb.italic_runs,    new_pos, del_len);
                screenplay::style_shift_erase(cb.underline_runs, new_pos, del_len);
                state_.cursor.byte_offset = new_pos;
            } else if (state_.cursor.block_idx > 0) {
                auto& blocks = state_.script.blocks;
                auto& prev   = blocks[state_.cursor.block_idx - 1];
                size_t merge_pt = prev.text.size();
                screenplay::style_append_shifted(prev.bold_runs,      cb.bold_runs,      merge_pt);
                screenplay::style_append_shifted(prev.italic_runs,    cb.italic_runs,    merge_pt);
                screenplay::style_append_shifted(prev.underline_runs, cb.underline_runs, merge_pt);
                prev.text += cb.text;
                blocks.erase(blocks.begin() + (ptrdiff_t)state_.cursor.block_idx);
                state_.cursor = { state_.cursor.block_idx - 1, merge_pt };
            }
        });
    }

    void on_delete_fwd() {
        // Empty element + Delete → remove the whole element (this also removes
        // the structural "()" of an untouched Ctrl+4 Parenthetical).
        if (!state_.has_selection && state_.script.blocks.size() > 1 &&
            is_empty_element(current_block())) {
            record_structural([this] { remove_current_block(/*land_prev=*/false); });
            return;
        }
        // Protect a non-empty Parenthetical's closing ")": forward-delete at or
        // after it is a no-op, mirroring the Backspace guard above.
        if (!state_.has_selection) {
            auto& cb = current_block();
            if (cb.type == screenplay::BlockType::Parenthetical &&
                is_paren_wrapped(cb.text) &&
                state_.cursor.byte_offset >= cb.text.size() - 1)
                return;
        }
        record_structural([this] {
            if (state_.has_selection) { delete_selection_if_active(); return; }
            auto& cb = current_block();
            if (state_.cursor.byte_offset < cb.text.size()) {
                size_t del_len = screenplay::utf8::codepoint_len(
                    cb.text, state_.cursor.byte_offset);
                cb.text.erase(state_.cursor.byte_offset, del_len);
                screenplay::style_shift_erase(cb.bold_runs,      state_.cursor.byte_offset, del_len);
                screenplay::style_shift_erase(cb.italic_runs,    state_.cursor.byte_offset, del_len);
                screenplay::style_shift_erase(cb.underline_runs, state_.cursor.byte_offset, del_len);
            } else if (state_.cursor.block_idx + 1 < state_.script.blocks.size()) {
                auto& blocks = state_.script.blocks;
                auto& nxt    = blocks[state_.cursor.block_idx + 1];
                const size_t base = cb.text.size();
                screenplay::style_append_shifted(cb.bold_runs,      nxt.bold_runs,      base);
                screenplay::style_append_shifted(cb.italic_runs,    nxt.italic_runs,    base);
                screenplay::style_append_shifted(cb.underline_runs, nxt.underline_runs, base);
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
                // Character and Scene Heading are uppercase; Parentheticals
                // (wrylies) keep the writer's casing, e.g. "(smiling)".
                if (cb2.type == screenplay::BlockType::Character ||
                    cb2.type == screenplay::BlockType::SceneHeading)
                    ins = QString::fromStdString(ins).toUpper().toStdString();
                // New text is unstyled — shift any runs after the caret.
                screenplay::style_shift_insert(cb2.bold_runs,      state_.cursor.byte_offset, ins.size());
                screenplay::style_shift_insert(cb2.italic_runs,    state_.cursor.byte_offset, ins.size());
                screenplay::style_shift_insert(cb2.underline_runs, state_.cursor.byte_offset, ins.size());
                cb2.text.insert(state_.cursor.byte_offset, ins);
                state_.cursor.byte_offset += ins.size();
            });
            return;
        }

        // In an empty Dialogue, typing "(" starts a Parenthetical (wrylie): the
        // block converts and gains structural "()" with the caret between them.
        // The writer never types the closing ")" by hand.
        {
            auto& cur = current_block();
            if (utf8_char == "(" &&
                cur.type == screenplay::BlockType::Dialogue && cur.text.empty()) {
                make_parenthetical();   // empty block → "()" with caret inside
                return;
            }
        }

        auto& cb = current_block();
        // Unicode-aware uppercase for Character and SceneHeading blocks
        std::string to_insert = utf8_char;
        // Parentheticals keep the writer's casing (e.g. "(smiling)"); only
        // Character cues and Scene Headings are forced to uppercase.
        if (cb.type == screenplay::BlockType::Character ||
            cb.type == screenplay::BlockType::SceneHeading)
            to_insert = QString::fromStdString(utf8_char).toUpper().toStdString();

        record_char();   // open/continue typing group
        screenplay::style_shift_insert(cb.bold_runs,      state_.cursor.byte_offset, to_insert.size());
        screenplay::style_shift_insert(cb.italic_runs,    state_.cursor.byte_offset, to_insert.size());
        screenplay::style_shift_insert(cb.underline_runs, state_.cursor.byte_offset, to_insert.size());
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
            auto& b = blocks[sel_s.block_idx];
            const size_t len = sel_e.byte_offset - sel_s.byte_offset;
            b.text.erase(sel_s.byte_offset, len);
            screenplay::style_shift_erase(b.bold_runs,      sel_s.byte_offset, len);
            screenplay::style_shift_erase(b.italic_runs,    sel_s.byte_offset, len);
            screenplay::style_shift_erase(b.underline_runs, sel_s.byte_offset, len);
        } else {
            // Keep [0, sel_s.byte_offset) from start block and
            // [sel_e.byte_offset, end) from end block; merge them. Runs:
            // the start block keeps only its head's runs (its own erased
            // tail is dropped); the end block's surviving tail runs are
            // extracted, rebased, and appended after the head.
            auto& first = blocks[sel_s.block_idx];
            auto& last  = blocks[sel_e.block_idx];
            std::string head = first.text.substr(0, sel_s.byte_offset);
            std::string tail = last.text.substr(sel_e.byte_offset);

            auto merge_runs = [&](screenplay::StyleRuns& dst, const screenplay::StyleRuns& src_last) {
                screenplay::style_remove(dst, sel_s.byte_offset, first.text.size());
                auto tail_runs = screenplay::style_extract(src_last, sel_e.byte_offset, last.text.size());
                screenplay::style_append_shifted(dst, tail_runs, sel_s.byte_offset);
            };
            merge_runs(first.bold_runs,      last.bold_runs);
            merge_runs(first.italic_runs,    last.italic_runs);
            merge_runs(first.underline_runs, last.underline_runs);

            first.text = head + tail;
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
