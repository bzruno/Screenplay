#pragma once
// utf8_utils.hpp — Portable UTF-8 helpers used throughout the editor.
// No Qt dependency; works on raw std::string / std::string_view.

#include <string>
#include <string_view>
#include <cstdint>

namespace screenplay::utf8 {

// ─────────────────────────────────────────────────────────────────────────────
// decode — read one codepoint from `s` at byte position `pos`.
// Sets `len` to the number of bytes consumed (1-4).
// Returns U+FFFD for invalid / truncated sequences and consumes 1 byte.
// Returns 0 and sets len=0 when pos >= s.size().
// ─────────────────────────────────────────────────────────────────────────────
inline uint32_t decode(std::string_view s, size_t pos, size_t& len) noexcept {
    if (pos >= s.size()) { len = 0; return 0; }

    const auto* u = reinterpret_cast<const uint8_t*>(s.data() + pos);
    const size_t avail = s.size() - pos;

    if (u[0] < 0x80) {                          // 1-byte (ASCII)
        len = 1; return u[0];
    }
    if (u[0] < 0xC0) {                          // Stray continuation byte
        len = 1; return 0xFFFD;
    }
    if (u[0] < 0xE0) {                          // 2-byte sequence
        if (avail < 2 || (u[1] & 0xC0) != 0x80) { len = 1; return 0xFFFD; }
        len = 2;
        return static_cast<uint32_t>((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu);
    }
    if (u[0] < 0xF0) {                          // 3-byte sequence
        if (avail < 3 || (u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80) {
            len = 1; return 0xFFFD;
        }
        len = 3;
        return static_cast<uint32_t>((u[0] & 0x0Fu) << 12)
             | static_cast<uint32_t>((u[1] & 0x3Fu) << 6)
             | (u[2] & 0x3Fu);
    }
    // 4-byte sequence
    if (avail < 4 || (u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80 || (u[3] & 0xC0) != 0x80) {
        len = 1; return 0xFFFD;
    }
    len = 4;
    return static_cast<uint32_t>((u[0] & 0x07u) << 18)
         | static_cast<uint32_t>((u[1] & 0x3Fu) << 12)
         | static_cast<uint32_t>((u[2] & 0x3Fu) << 6)
         | (u[3] & 0x3Fu);
}

// ─────────────────────────────────────────────────────────────────────────────
// codepoint_len — byte length of the codepoint that starts at s[pos].
// Safe: always returns at least 1 (never 0) so callers can advance.
// ─────────────────────────────────────────────────────────────────────────────
inline size_t codepoint_len(const std::string& s, size_t pos) noexcept {
    size_t len;
    decode(std::string_view(s), pos, len);
    return (len > 0) ? len : 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// next_cp — advance pos by exactly one codepoint.
// ─────────────────────────────────────────────────────────────────────────────
inline size_t next_cp(const std::string& s, size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    return pos + codepoint_len(s, pos);
}

// ─────────────────────────────────────────────────────────────────────────────
// prev_cp — move pos back to the start of the preceding codepoint.
// Skips over UTF-8 continuation bytes (0x80–0xBF).
// ─────────────────────────────────────────────────────────────────────────────
inline size_t prev_cp(const std::string& s, size_t pos) noexcept {
    if (pos == 0) return 0;
    size_t p = pos - 1;
    while (p > 0 && (static_cast<uint8_t>(s[p]) & 0xC0u) == 0x80u)
        --p;
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// align_to_cp_start — if pos is in the middle of a multibyte sequence,
// walk back to the start of that codepoint.
// ─────────────────────────────────────────────────────────────────────────────
inline size_t align_to_cp_start(const std::string& s, size_t pos) noexcept {
    if (pos >= s.size()) return s.size();
    size_t p = pos;
    while (p > 0 && (static_cast<uint8_t>(s[p]) & 0xC0u) == 0x80u)
        --p;
    return p;
}

} // namespace screenplay::utf8
