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
#include "../src/parsing/paste_parser.hpp"
#include "../src/io/exporter.hpp"
#include "../src/io/importer.hpp"
#include "../src/layout/layout_engine.hpp"

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

// A size or emptiness assertion that guards an indexed access must stop the
// test when it fails. Falling through indexes out of range, and the runner
// dies on the access violation without reporting a single result.
#define REQUIRE(cond) do { \
    if (cond) { ++passed; std::cout << "  PASS  " #cond "\n"; } \
    else      { ++failed; std::cout << "  FAIL  " #cond "  [" __FILE__ ":" << __LINE__ \
                                    << "]  - aborting test\n"; return; } \
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
    REQUIRE(u.has_value());
    CHECK(u->before.blocks[0].text == "B");
    CHECK(stack.can_redo());

    auto u2 = stack.undo();
    REQUIRE(u2.has_value());
    CHECK(u2->before.blocks[0].text == "A");

    CHECK(!stack.can_undo());

    auto r = stack.redo();
    REQUIRE(r.has_value());
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
    REQUIRE(!s.empty());
    // JOHN should be first (frequency 2)
    CHECK(s[0] == "JOHN");

    auto s2 = idx.suggest("JA");
    CHECK(s2.size() == 2);   // JANE, JAMES

    auto s3 = idx.suggest("XYZ");
    CHECK(s3.empty());

    // Case-insensitive input
    auto s4 = idx.suggest("jo");
    REQUIRE(!s4.empty());
    CHECK(s4[0] == "JOHN");
}

void test_character_smarttype() {
    SECTION("Character SmartType — names vs. extensions")
    using namespace screenplay;
    using namespace screenplay::editor;

    AutocompleteSystem sys;

    // A committed cue with an extension must train the bare NAME only, so the
    // name dropdown never offers a "JOHN (V.O.)" duplicate of "JOHN".
    sys.train(BlockType::Character, "JOHN");
    sys.train(BlockType::Character, "JOHN (V.O.)");

    // A name already typed in full is deliberately never offered as its own
    // completion, so a strict prefix is what reveals what was learned: one
    // entry, the bare name.
    const std::vector<std::string> kJohnOnly = { "JOHN" };
    CHECK(sys.query(BlockType::Character, "JOH", std::strlen("JOH")) == kJohnOnly);
    CHECK(sys.query(BlockType::Character, "JOHN", std::strlen("JOHN")).empty());

    // Typing "(" after the name switches to extension suggestions: the curated
    // list, in industry-usage order.
    const std::vector<std::string> kAllExtensions = {
        "V.O.", "O.S.", "O.C.", "CONT'D", "FILTERED", "PRE-LAP"
    };
    std::string typing = "JOHN (";
    CHECK(sys.query(BlockType::Character, typing, typing.size()) == kAllExtensions);

    // Prefix-filtered inside the parentheses, preserving order.
    const std::vector<std::string> kOExtensions = { "O.S.", "O.C." };
    std::string typingO = "JOHN (O";
    CHECK(sys.query(BlockType::Character, typingO, typingO.size()) == kOExtensions);

    // Mid-prefix scene heading text offers only the prefixes it could still
    // become, and a finished prefix is not re-suggested: "INT" completes to
    // "INT." and nothing else.
    const std::vector<std::string> kIntPrefix = { "INT." };
    CHECK(sys.query(BlockType::SceneHeading, "INT", std::strlen("INT")) == kIntPrefix);
}

void test_paste_parser() {
    SECTION("Paste parser — element recognition")
    using namespace screenplay;

    std::vector<std::string> paras = {
        "INT. HOUSE - DAY",       // Scene Heading
        "John walks in slowly.",  // Action
        "JOHN",                   // Character
        "(smiling)",              // Parenthetical
        "Hello there.",           // Dialogue (after parenthetical)
        "JOHN (V.O.)",            // Character (extension kept in text)
        "That was the start.",    // Dialogue (after character)
        "CUT TO:"                 // Transition
    };

    auto types = parse::classify_paragraphs(paras);
    REQUIRE(types.size() == 8);
    CHECK(types[0] == BlockType::SceneHeading);
    CHECK(types[1] == BlockType::Action);
    CHECK(types[2] == BlockType::Character);
    CHECK(types[3] == BlockType::Parenthetical);
    CHECK(types[4] == BlockType::Dialogue);
    CHECK(types[5] == BlockType::Character);
    CHECK(types[6] == BlockType::Dialogue);
    CHECK(types[7] == BlockType::Transition);
}

void test_layout_engine() {
    SECTION("Layout engine — pagination")
    using namespace screenplay;
    using namespace screenplay::layout;

    StubMetrics metrics;
    // Real page WIDTH, short page height. Block indents are absolute insets
    // from the page edges, so shrinking the width would give the text columns
    // a negative size; only the height may be cut to force pagination.
    PageGeometry geo = PageGeometry::us_letter();
    geo.page_h     = 100.f;
    geo.margin_top = 5.f;
    geo.margin_bot = 5.f;

    LayoutEngine engine(metrics, geo, 12.f);

    Script s;
    // Add enough action blocks to force pagination
    for (int i = 0; i < 10; ++i)
        s.append(BlockType::Action, "Line of action text.");

    auto pages = engine.layout(s);
    REQUIRE(pages.size() >= 2);
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
    PageGeometry geo = PageGeometry::us_letter();
    geo.page_h     = 100.f;
    geo.margin_top = 5.f;
    geo.margin_bot = 5.f;

    LayoutEngine engine(metrics, geo, 12.f);

    // Sweeping the amount of preceding action lands the cue at every possible
    // distance from the page bottom, so the boundary where it would be split
    // from its dialogue is actually exercised instead of guessed at.
    for (int filler = 0; filler <= 12; ++filler) {
        Script s;
        for (int i = 0; i < filler; ++i)
            s.append(BlockType::Action, "Action line.");

        const size_t cue_idx = s.blocks.size();
        s.append(BlockType::Character, "JOHN");
        s.append(BlockType::Dialogue,  "Hello.");

        int cue_page = -1, dialogue_page = -1;
        auto pages = engine.layout(s);
        for (size_t pi = 0; pi < pages.size(); ++pi)
            for (const auto& vl : pages[pi].lines) {
                if (vl.block_idx == cue_idx) cue_page = (int)pi;
                if (vl.block_idx == cue_idx + 1 && dialogue_page < 0)
                    dialogue_page = (int)pi;
            }

        CHECK(cue_page >= 0 && cue_page == dialogue_page);
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
    REQUIRE(loaded.blocks.size() == 3);
    CHECK(loaded.blocks[0].type == BlockType::SceneHeading);
    CHECK(loaded.blocks[1].type == BlockType::Character);
    CHECK(loaded.blocks[2].type == BlockType::Dialogue);
    CHECK(loaded.blocks[0].text == "EXT. BEACH - SUNSET");
}

void test_fdx_fidelity() {
    SECTION("FDX fidelity — styles, scene numbers, entities, robustness")
    using namespace screenplay;
    using namespace screenplay::io;

    // ── Styles + scene numbers survive a round trip ──────────────────────
    Script s;
    { Block b{BlockType::SceneHeading, "INT. HOUSE - DAY", 1}; b.scene_number = "1A";
      s.blocks.push_back(b); }
    { Block b{BlockType::Dialogue, "Hi.", 2};
      b.bold_runs = {{0, b.text.size()}}; b.underline_runs = {{0, b.text.size()}};
      s.blocks.push_back(b); }
    s.next_id = 3;

    std::string xml = FDXExporter::to_string(s);
    CHECK(xml.find("Number=\"1A\"")             != std::string::npos);
    CHECK(xml.find("Style=\"Bold+Underline\"")  != std::string::npos);
    CHECK(xml.find("<Bold>")                     == std::string::npos); // old form gone

    Script r = FDXImporter::parse(xml);
    REQUIRE(r.blocks.size() == 2);
    CHECK(r.blocks[0].scene_number == "1A");
    CHECK(r.blocks[1].is_bold_whole() && r.blocks[1].is_underline_whole()
          && !r.blocks[1].is_italic_whole());

    // ── Multiple <Text> runs (with attributes) + numeric entities ────────
    const std::string multi =
        "<Content><Paragraph Type=\"Action\">"
        "<Text>He </Text><Text Style=\"Italic\">runs</Text>"
        "<Text> &#233;&#x21;</Text></Paragraph></Content>";
    ImportReport rep;
    Script m = FDXImporter::parse(multi, &rep);
    REQUIRE(m.blocks.size() == 1);
    CHECK(m.blocks[0].text == "He runs \xC3\xA9!");   // concatenated + decoded
    // Only "runs" (bytes [3,7)) is italic — not the whole block.
    CHECK(!m.blocks[0].is_italic_whole());
    CHECK(style_covers(m.blocks[0].italic_runs, 3, 7));
    CHECK(!style_covers(m.blocks[0].italic_runs, 0, 3));

    // ── "Shot" is an element this model represents, so it imports as itself
    //    and nothing is reported as converted ───────────────────────────────
    const std::string shot =
        "<Content><Paragraph Type=\"Shot\"><Text>ON THE DOOR</Text></Paragraph></Content>";
    ImportReport rep2;
    Script sh = FDXImporter::parse(shot, &rep2);
    REQUIRE(!sh.blocks.empty());
    CHECK(sh.blocks[0].type == BlockType::Shot);
    CHECK(sh.blocks[0].text == "ON THE DOOR");
    CHECK(rep2.downgraded_types.empty());

    // ── An element with no equivalent here keeps its text as Action, and the
    //    conversion is reported so the user can be told ─────────────────────
    const std::string lyrics =
        "<Content><Paragraph Type=\"Lyrics\"><Text>La la la</Text></Paragraph></Content>";
    ImportReport rep3;
    Script ly = FDXImporter::parse(lyrics, &rep3);
    REQUIRE(!ly.blocks.empty());
    CHECK(ly.blocks[0].type == BlockType::Action);
    CHECK(ly.blocks[0].text == "La la la");
    CHECK(rep3.downgraded_types.size() == 1 && rep3.downgraded_types[0] == "Lyrics");

    // ── Robustness: truncated / empty / garbage never loop or crash ──────
    Script trunc = FDXImporter::parse(
        "<Content><Paragraph Type=\"Action\"><Text>oops");   // no closings
    CHECK(trunc.blocks.size() >= 1);
    CHECK(FDXImporter::parse("").blocks.size() == 1);
    CHECK(FDXImporter::parse("garbage <<< &&&").blocks.size() == 1);
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
    REQUIRE(loaded.blocks.size() == 4);
    CHECK(loaded.blocks[2].type == BlockType::Character);
    CHECK(loaded.blocks[2].text == "SCIENTIST");
    CHECK(loaded.blocks[3].text == "Eureka!");
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    // Unbuffered: if a test crashes, the output up to the failure survives.
    std::cout << std::unitbuf;
    std::cout << "Screenplay Editor — Unit Tests\n";
    std::cout << std::string(40, '=') << "\n";

    test_script_model();
    test_undo_stack();
    test_autocomplete();
    test_character_smarttype();
    test_paste_parser();
    test_layout_engine();
    test_layout_character_grouping();
    test_fountain_roundtrip();
    test_fdx_roundtrip();
    test_fdx_fidelity();
    test_json_roundtrip();

    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
