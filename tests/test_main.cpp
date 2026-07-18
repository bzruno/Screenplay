// tests/test_main.cpp
// Minimal self-contained unit tests — no external test framework needed.
// Build: cmake --build build --target ScreenplayTests
// Run:   ./build/ScreenplayTests

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// Pull in headers under test
#include "../src/model/model.hpp"
#include "../src/model/undo_stack.hpp"
#include "../src/editor/autocomplete.hpp"
#include "../src/io/exporter.hpp"
#include "../src/io/importer.hpp"

// ── Stub font metrics (no FreeType dependency in tests) ───────────────────────
#include "../src/layout/font_metrics.hpp"

class StubMetrics final : public screenplay::layout::IFontMetrics {
public:
    // Courier New: ~7.2 pts per character at 12pt
    static constexpr float kCharW = 7.2f;
    static constexpr float kLineH = 14.4f;

    screenplay::layout::LineMetrics measure(
        std::string_view text, float /*pt_size*/) const override
    {
        float w = static_cast<float>(text.size()) * kCharW;
        return { w, kLineH, 10.f, 4.f };
    }

    std::vector<size_t> word_wrap(
        std::string_view text, float /*pt_size*/, float max_width) const override
    {
        std::vector<size_t> breaks;
        size_t last_space = std::string_view::npos;
        float  line_w     = 0.f;

        for (size_t i = 0; i < text.size(); ++i) {
            line_w += kCharW;
            if (text[i] == ' ') last_space = i;
            if (line_w > max_width) {
                size_t at = (last_space != std::string_view::npos)
                    ? last_space + 1 : i;
                breaks.push_back(at);
                size_t from = at;
                line_w = static_cast<float>(i - from + 1) * kCharW;
                last_space = std::string_view::npos;
            }
        }
        return breaks;
    }

    float line_height(float /*pt_size*/) const override { return kLineH; }
};

// ── Test helpers ──────────────────────────────────────────────────────────────
static int passed = 0, failed = 0;

#define CHECK(cond) do { \
    if (cond) { ++passed; std::cout << "  PASS  " #cond "\n"; } \
    else      { ++failed; std::cout << "  FAIL  " #cond "  [" __FILE__ ":" << __LINE__ << "]\n"; } \
} while(0)

#define SECTION(name) std::cout << "\n── " name " ──\n";

// ── Tests ─────────────────────────────────────────────────────────────────────

void test_script_model() {
    SECTION("Script model")
    using namespace screenplay;

    Script s;
    CHECK(s.blocks.empty());

    auto& b = s.append(BlockType::SceneHeading, "INT. OFFICE - DAY");
    CHECK(b.type == BlockType::SceneHeading);
    CHECK(b.text == "INT. OFFICE - DAY");
    CHECK(b.id   == 1);
    CHECK(s.next_id == 2);

    s.append(BlockType::Action, "Bob enters.");
    CHECK(s.blocks.size() == 2);
}

void test_undo_stack() {
    SECTION("Undo / Redo")
    using namespace screenplay;

    Script s1, s2, s3;
    s1.append(BlockType::Action, "A");
    s2.append(BlockType::Action, "B");
    s3.append(BlockType::Action, "C");

    UndoStack stack;
    Cursor c0{0,0}, c1{0,1};

    stack.push({ s1, s2, c0, c1 });
    stack.push({ s2, s3, c1, c0 });

    CHECK(stack.can_undo());
    CHECK(!stack.can_redo());

    auto u = stack.undo();
    CHECK(u.has_value());
    CHECK(u->before.blocks[0].text == "B");
    CHECK(stack.can_redo());

    auto u2 = stack.undo();
    CHECK(u2.has_value());
    CHECK(u2->before.blocks[0].text == "A");

    CHECK(!stack.can_undo());

    auto r = stack.redo();
    CHECK(r.has_value());
    CHECK(r->after.blocks[0].text == "B");
}

void test_autocomplete() {
    SECTION("Autocomplete")
    using namespace screenplay::editor;

    AutocompleteIndex idx;
    idx.learn("JOHN");
    idx.learn("JOHN");
    idx.learn("JANE");
    idx.learn("JAMES");

    auto s = idx.suggest("J");
    CHECK(!s.empty());
    // JOHN should be first (frequency 2)
    CHECK(s[0] == "JOHN");

    auto s2 = idx.suggest("JA");
    CHECK(s2.size() == 2);   // JANE, JAMES

    auto s3 = idx.suggest("XYZ");
    CHECK(s3.empty());

    // Case-insensitive input
    auto s4 = idx.suggest("jo");
    CHECK(!s4.empty());
    CHECK(s4[0] == "JOHN");
}

void test_layout_engine() {
    SECTION("Layout engine — pagination")
    using namespace screenplay;
    using namespace screenplay::layout;

    StubMetrics metrics;
    // Tiny page: 200pt wide, 100pt tall, 10pt margins → 80pt printable height
    PageGeometry geo;
    geo.page_w       = 200.f;
    geo.page_h       = 100.f;
    geo.margin_top   = 10.f;
    geo.margin_bot   = 10.f;
    geo.margin_left  = 10.f;
    geo.margin_right = 10.f;

    LayoutEngine engine(metrics, geo, 12.f);

    Script s;
    // Add enough action blocks to force pagination
    for (int i = 0; i < 10; ++i)
        s.append(BlockType::Action, "Line of action text.");

    auto pages = engine.layout(s);
    CHECK(pages.size() >= 2);
    CHECK(pages[0].number == 1);
    CHECK(pages[1].number == 2);

    // All lines must have valid block_idx
    for (const auto& page : pages)
        for (const auto& vl : page.lines)
            CHECK(vl.block_idx < s.blocks.size());
}

void test_layout_character_grouping() {
    SECTION("Layout — Character+Dialogue grouping")
    using namespace screenplay;
    using namespace screenplay::layout;

    StubMetrics metrics;
    PageGeometry geo;
    geo.page_w = 300.f; geo.page_h = 80.f;
    geo.margin_top = 5.f; geo.margin_bot = 5.f;
    geo.margin_left = 5.f; geo.margin_right = 5.f;

    LayoutEngine engine(metrics, geo, 12.f);
    Script s;
    // Fill almost a page with action
    s.append(BlockType::Action, "Some action to fill the page up almost completely.");
    s.append(BlockType::Character, "JOHN");
    s.append(BlockType::Dialogue,  "Hello.");

    auto pages = engine.layout(s);
    // Character and Dialogue must be on the same page
    if (pages.size() >= 2) {
        // Find character block page and dialogue block page
        int char_page = -1, dial_page = -1;
        for (int pi = 0; pi < (int)pages.size(); ++pi) {
            for (const auto& vl : pages[pi].lines) {
                if (vl.block_idx == 1) char_page = pi;
                if (vl.block_idx == 2) dial_page = pi;
            }
        }
        if (char_page >= 0 && dial_page >= 0)
            CHECK(char_page == dial_page);
    }
}

void test_fountain_roundtrip() {
    SECTION("Fountain export + import roundtrip")
    using namespace screenplay;
    using namespace screenplay::io;

    Script original;
    original.append(BlockType::SceneHeading, "INT. OFFICE - DAY");
    original.append(BlockType::Action,       "Bob enters the office.");
    original.append(BlockType::Character,    "BOB");
    original.append(BlockType::Dialogue,     "Hello, world.");
    original.append(BlockType::Transition,   "CUT TO");

    std::string fountain = FountainExporter::to_string(original);
    CHECK(!fountain.empty());

    Script loaded = FountainImporter::parse(fountain);
    CHECK(!loaded.blocks.empty());

    // Scene heading must survive
    bool found_scene = false;
    for (const auto& b : loaded.blocks)
        if (b.type == BlockType::SceneHeading) found_scene = true;
    CHECK(found_scene);
}

void test_fdx_roundtrip() {
    SECTION("FDX export + import roundtrip")
    using namespace screenplay;
    using namespace screenplay::io;

    Script original;
    original.append(BlockType::SceneHeading, "EXT. BEACH - SUNSET");
    original.append(BlockType::Character,    "ALICE");
    original.append(BlockType::Dialogue,     "It's beautiful.");

    std::string fdx = FDXExporter::to_string(original);
    CHECK(fdx.find("<FinalDraft") != std::string::npos);

    Script loaded = FDXImporter::parse(fdx);
    CHECK(loaded.blocks.size() == 3);
    CHECK(loaded.blocks[0].type == BlockType::SceneHeading);
    CHECK(loaded.blocks[1].type == BlockType::Character);
    CHECK(loaded.blocks[2].type == BlockType::Dialogue);
    CHECK(loaded.blocks[0].text == "EXT. BEACH - SUNSET");
}

void test_json_roundtrip() {
    SECTION("JSON serialize + deserialize roundtrip")
    using namespace screenplay;
    using namespace screenplay::io;

    Script original;
    original.append(BlockType::SceneHeading, "INT. LAB - NIGHT");
    original.append(BlockType::Action,       "Scientist types furiously.");
    original.append(BlockType::Character,    "SCIENTIST");
    original.append(BlockType::Dialogue,     "Eureka!");

    std::string json = JsonSerializer::serialize(original);
    CHECK(!json.empty());
    CHECK(json.find("\"blocks\"") != std::string::npos);

    Script loaded = JsonDeserializer::parse(json);
    CHECK(loaded.blocks.size() == 4);
    CHECK(loaded.blocks[2].type == BlockType::Character);
    CHECK(loaded.blocks[2].text == "SCIENTIST");
    CHECK(loaded.blocks[3].text == "Eureka!");
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    std::cout << "Screenplay Editor — Unit Tests\n";
    std::cout << std::string(40, '=') << "\n";

    test_script_model();
    test_undo_stack();
    test_autocomplete();
    test_layout_engine();
    test_layout_character_grouping();
    test_fountain_roundtrip();
    test_fdx_roundtrip();
    test_json_roundtrip();

    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
