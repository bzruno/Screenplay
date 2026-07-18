#pragma once
// config/language.hpp
// Application language configuration.
// English is the default; Portuguese is the alternative.
// UI strings are always in English — only SmartType suggestion strings change.

#include <cstring>
#include <functional>
#include <QSettings>

namespace screenplay::config {

enum class AppLanguage { English, Portuguese };

class LanguageConfig {
public:
    static AppLanguage current() { return current_; }

    static void set(AppLanguage lang) {
        current_ = lang;
        save();
        if (on_change_) on_change_();
    }

    // Register a callback invoked after every set() call.
    // Pass [&sys]{ sys.reseed(); } from wherever AutocompleteSystem lives.
    static void set_on_change(std::function<void()> cb) {
        on_change_ = std::move(cb);
    }

    static void save() {
        QSettings s;
        s.setValue("language", static_cast<int>(current_));
    }

    static void load() {
        QSettings s;
        current_ = static_cast<AppLanguage>(
            s.value("language", 0).toInt());
    }

    // Returns the localized const char* for key.
    // Caller may pass the result directly to Qt as an implicit Latin-1 or
    // via QString::fromUtf8() for strings containing non-ASCII.
    static const char* tr(const char* key) {
        struct Entry { const char* key; const char* en; const char* pt; };
        // clang-format off
        static const Entry kTable[] = {
            // ── Menu titles ─────────────────────────────────────────────
            {"menu_file",           "&File",              "&File"                           },
            {"menu_edit",           "&Edit",              "&Edit"                           },
            {"menu_document",       "&Document",          "&Document"                       },
            {"menu_view",           "&View",              "&View"                           },
            {"menu_format",         "F&ormat",            "F&ormat"                         },
            {"menu_help",           "&Help",              "&Help"                           },
            {"menu_tools",          "&Tools",             "&Tools"                          },
            // ── File menu ───────────────────────────────────────────────
            {"action_new",          "New",                "New"                             },
            {"action_open",         "Open\xe2\x80\xa6",  "Open\xe2\x80\xa6"                },
            {"action_save",         "Save",               "Save"                            },
            {"action_save_as",      "Save As\xe2\x80\xa6","Save As\xe2\x80\xa6"            },
            {"action_export_pdf",   "Export PDF\xe2\x80\xa6","Export PDF\xe2\x80\xa6"      },
            {"action_export_ftn",   "Export Fountain\xe2\x80\xa6","Export Fountain\xe2\x80\xa6"},
            {"action_export_fdx",   "Export FDX\xe2\x80\xa6","Export FDX\xe2\x80\xa6"     },
            // ── Edit menu ───────────────────────────────────────────────
            {"action_undo",         "Undo",               "Undo"                            },
            {"action_redo",         "Redo",               "Redo"                            },
            {"action_cut",          "Cut",                "Cut"                             },
            {"action_copy",         "Copy",               "Copy"                            },
            {"action_paste",        "Paste",              "Paste"                           },
            {"action_find",         "Find\xe2\x80\xa6",  "Find\xe2\x80\xa6"                },
            {"action_replace",      "Replace\xe2\x80\xa6","Replace\xe2\x80\xa6"            },
            {"action_spell_check",  "Spell Check",        "Spell Check"                     },
            // ── Document menu ───────────────────────────────────────────
            {"action_title_page",   "Edit Title Page\xe2\x80\xa6","Edit Title Page\xe2\x80\xa6"},
            {"action_incl_title",   "Include Title Page", "Include Title Page"              },
            {"action_scene_nums",   "Scene Numbers",      "Scene Numbers"                   },
            {"action_goto_scene",   "Go to Scene\xe2\x80\xa6","Go to Scene\xe2\x80\xa6"   },
            {"action_goto_page",    "Go to Page\xe2\x80\xa6","Go to Page\xe2\x80\xa6"     },
            // ── View menu ───────────────────────────────────────────────
            {"action_zoom_in",      "Zoom In",            "Zoom In"                         },
            {"action_zoom_out",     "Zoom Out",           "Zoom Out"                        },
            {"action_zoom_reset",   "1:1",                "1:1"                             },
            {"action_stats",        "Statistics",         "Statistics"                      },
            {"action_database",     "Database",           "Database"                        },
            {"action_fullscreen",   "Full Screen",        "Full Screen"                     },
            // ── Format menu ─────────────────────────────────────────────
            {"action_scene_heading","Scene Heading",      "Scene Heading"                   },
            {"action_action",       "Action",             "Action"                          },
            {"action_character",    "Character",          "Character"                       },
            {"action_paren",        "Parenthetical",      "Parenthetical"                   },
            {"action_dialogue",     "Dialogue",           "Dialogue"                        },
            {"action_transition",   "Transition",         "Transition"                      },
            {"action_dual_dial",    "Dual Dialogue",      "Dual Dialogue"                   },
            {"action_bold",         "Bold",               "Bold"                            },
            {"action_italic",       "Italic",             "Italic"                          },
            // ── Help menu ───────────────────────────────────────────────
            {"action_shortcuts",    "Keyboard Shortcuts", "Keyboard Shortcuts"              },
            {"action_about",        "About",              "About"                           },
            // ── Tools / Language ────────────────────────────────────────
            {"menu_language",       "Language",           "Language"                        },
            {"lang_english",        "English",            "English"                         },
            {"lang_portuguese",     "Portuguese",         "Portuguese"                      },
            // ── Database panel ──────────────────────────────────────────
            {"db_tab_scenes",       "Scenes",             "Scenes"                          },
            {"db_tab_chars",        "Characters",         "Characters"                      },
            {"db_tab_dialogue",     "Dialogue",           "Dialogue"                        },
            {"db_scenes_num",       "#",                  "#"                               },
            {"db_scenes_prefix",    "PREFIX",             "PREFIX"                          },
            {"db_scenes_loc",       "LOCATION",           "LOCATION"                        },
            {"db_scenes_time",      "TIME",               "TIME"                            },
            {"db_scenes_chars",     "CHARACTERS",         "CHARACTERS"                      },
            {"db_scenes_lines",     "LINES",              "LINES"                           },
            {"db_scenes_page",      "PAGE",               "PAGE"                            },
            {"db_chars_name",       "NAME",               "NAME"                            },
            {"db_chars_dlg",        "DIALOGUE COUNT",     "DIALOGUE COUNT"                  },
            {"db_chars_scn",        "SCENE COUNT",        "SCENE COUNT"                     },
            {"db_chars_scns",       "SCENES",             "SCENES"                          },
            {"db_chars_first",      "FIRST ENTRY",        "FIRST ENTRY"                     },
            {"db_dial_scene",       "SCENE #",            "SCENE #"                         },
            {"db_dial_char",        "CHARACTER",          "CHARACTER"                       },
            {"db_dial_paren",       "PARENTHETICAL",      "PARENTHETICAL"                   },
            {"db_dial_text",        "DIALOGUE",           "DIALOGUE"                        },
            {"db_ctx_view_scene",   "Go to scene",        "Go to scene"                     },
            {"db_ctx_chars_scene",  "Characters in this scene","Characters in this scene"   },
            {"db_ctx_goto_first",   "Go to first appearance","Go to first appearance"       },
            {"db_ctx_scenes_char",  "Scenes with this character","Scenes with this character"},
            {"db_ctx_view_dial",    "View dialogue",      "View dialogue"                   },
            {"db_filter_all_scenes","All scenes",         "All scenes"                      },
            {"db_filter_all_chars", "All characters",     "All characters"                  },
            {"db_filter_hint",      "Filter\xe2\x80\xa6", "Filter\xe2\x80\xa6"             },
            // ── Dock titles ─────────────────────────────────────────────
            {"dock_scenes",         "Scenes",             "Scenes"                          },
            {"dock_stats",          "Statistics",         "Statistics"                      },
            {"dock_database",       "Database",           "Database"                        },
            // ── Status bar ──────────────────────────────────────────────
            {"status_block",        "Block:",             "Block:"                          },
            {"status_page",         "Page:",              "Page:"                           },
            {"status_words",        "Words:",             "Words:"                          },
            // ── Misc ────────────────────────────────────────────────────
            {"untitled",            "Untitled",           "Untitled"                        },
            {"open_script_dlg",     "Open screenplay",    "Open screenplay"                 },
            {"save_error",          "Save error",         "Save error"                      },
            {nullptr, nullptr, nullptr}
        };
        // clang-format on
        for (int i = 0; kTable[i].key; ++i) {
            if (std::strcmp(kTable[i].key, key) == 0)
                return kTable[i].en; // UI always in English
        }
        return key; // fallback: return key itself
    }

private:
    inline static AppLanguage           current_   = AppLanguage::English;
    inline static std::function<void()> on_change_;
};

} // namespace screenplay::config
