#pragma once
// config/language.hpp
// Application language configuration.
// English is the default; Portuguese is the alternative.
// The entire UI follows the selected language. Strings keyed by their
// English text live in config/ui_strings.hpp; this table is keyed by id.

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
            {"menu_file",           "&File",              "&Arquivo"                           },
            {"menu_edit",           "&Edit",              "&Editar"                           },
            {"menu_document",       "&Document",          "&Documento"                       },
            {"menu_view",           "&View",              "&Exibir"                           },
            {"menu_format",         "F&ormat",            "F&ormatar"                         },
            {"menu_help",           "&Help",              "&Ajuda"                           },
            {"menu_tools",          "&Tools",             "&Ferramentas"                          },
            // ── File menu ───────────────────────────────────────────────
            {"action_new",          "New",                "Novo"                             },
            {"action_open",         "Open\xe2\x80\xa6",  "Abrir\xe2\x80\xa6"                },
            {"action_save",         "Save",               "Salvar"                            },
            {"action_save_as",      "Save As\xe2\x80\xa6","Salvar como\xe2\x80\xa6"            },
            {"action_export_pdf",   "Export PDF\xe2\x80\xa6","Exportar PDF\xe2\x80\xa6"      },
            {"action_export_ftn",   "Export Fountain\xe2\x80\xa6","Exportar Fountain\xe2\x80\xa6"},
            {"action_export_fdx",   "Export FDX\xe2\x80\xa6","Exportar FDX\xe2\x80\xa6"     },
            // ── Edit menu ───────────────────────────────────────────────
            {"action_undo",         "Undo",               "Desfazer"                            },
            {"action_redo",         "Redo",               "Refazer"                            },
            {"action_cut",          "Cut",                "Recortar"                             },
            {"action_copy",         "Copy",               "Copiar"                            },
            {"action_paste",        "Paste",              "Colar"                           },
            {"action_find",         "Find\xe2\x80\xa6",  "Localizar\xe2\x80\xa6"                },
            {"action_replace",      "Replace\xe2\x80\xa6","Substituir\xe2\x80\xa6"            },
            {"action_spell_check",  "Spell Check",        "Corretor ortogr\xc3\xa1" "fico"                     },
            // ── Document menu ───────────────────────────────────────────
            {"action_incl_title",   "Include Title Page", "Incluir capa"              },
            {"action_scene_nums",   "Scene Numbers",      "N\xc3\xbameros de cena"                   },
            {"action_goto_scene",   "Go to Scene\xe2\x80\xa6","Ir para cena\xe2\x80\xa6"   },
            {"action_goto_page",    "Go to Page\xe2\x80\xa6","Ir para p\xc3\xa1gina\xe2\x80\xa6"     },
            // ── View menu ───────────────────────────────────────────────
            {"action_zoom_in",      "Zoom In",            "Ampliar"                         },
            {"action_zoom_out",     "Zoom Out",           "Reduzir"                        },
            {"action_zoom_reset",   "1:1",                "1:1"                             },
            {"action_stats",        "Statistics",         "Estat\xc3\xadsticas"                      },
            {"action_database",     "Database",           "Banco de dados"                        },
            {"action_fullscreen",   "Full Screen",        "Tela cheia"                     },
            // ── Format menu ─────────────────────────────────────────────
            {"action_scene_heading","Scene Heading",      "Cabe\xc3\xa7" "alho de cena"                   },
            {"action_action",       "Action",             "A\xc3\xa7\xc3\xa3o"                          },
            {"action_character",    "Character",          "Personagem"                       },
            {"action_paren",        "Parenthetical",      "Par\xc3\xaantese"                   },
            {"action_dialogue",     "Dialogue",           "Di\xc3\xa1logo"                        },
            {"action_transition",   "Transition",         "Transi\xc3\xa7\xc3\xa3o"                      },
            {"action_dual_dial",    "Dual Dialogue",      "Di\xc3\xa1logo duplo"                   },
            {"action_bold",         "Bold",               "Negrito"                            },
            {"action_italic",       "Italic",             "It\xc3\xa1lico"                          },
            // ── Help menu ───────────────────────────────────────────────
            {"action_shortcuts",    "Keyboard Shortcuts", "Atalhos de teclado"              },
            {"action_about",        "About",              "Sobre"                           },
            // ── Tools / Language ────────────────────────────────────────
            {"menu_language",       "Language",           "Idioma"                        },
            {"lang_english",        "English",            "Ingl\xc3\xaas"                         },
            {"lang_portuguese",     "Portuguese",         "Portugu\xc3\xaas"                      },
            // ── Database panel ──────────────────────────────────────────
            {"db_tab_scenes",       "Scenes",             "Cenas"                          },
            {"db_tab_chars",        "Characters",         "Personagens"                      },
            {"db_tab_dialogue",     "Dialogue",           "Di\xc3\xa1logos"                        },
            {"db_scenes_num",       "#",                  "#"                               },
            {"db_scenes_prefix",    "PREFIX",             "PREFIXO"                          },
            {"db_scenes_loc",       "LOCATION",           "LOCAL"                        },
            {"db_scenes_time",      "TIME",               "TEMPO"                            },
            {"db_scenes_chars",     "CHARACTERS",         "PERSONAGENS"                      },
            {"db_scenes_lines",     "LINES",              "FALAS"                           },
            {"db_scenes_page",      "PAGE",               "P\xc3\x81GINA"                            },
            {"db_chars_name",       "NAME",               "NOME"                            },
            {"db_chars_dlg",        "DIALOGUE COUNT",     "N\xc2\xba DE FALAS"                  },
            {"db_chars_scn",        "SCENE COUNT",        "N\xc2\xba DE CENAS"                     },
            {"db_chars_scns",       "SCENES",             "CENAS"                          },
            {"db_chars_first",      "FIRST ENTRY",        "PRIMEIRA APARI\xc3\x87\xc3\x83O"                     },
            {"db_dial_scene",       "SCENE #",            "CENA #"                         },
            {"db_dial_char",        "CHARACTER",          "PERSONAGEM"                       },
            {"db_dial_paren",       "PARENTHETICAL",      "PAR\xc3\x8aNTESE"                   },
            {"db_dial_text",        "DIALOGUE",           "DI\xc3\x81LOGO"                        },
            {"db_ctx_view_scene",   "Go to scene",        "Ir para a cena"                     },
            {"db_ctx_chars_scene",  "Characters in this scene","Personagens desta cena"   },
            {"db_ctx_goto_first",   "Go to first appearance","Ir para a primeira apari\xc3\xa7\xc3\xa3o"       },
            {"db_ctx_scenes_char",  "Scenes with this character","Cenas com este personagem"},
            {"db_ctx_view_dial",    "View dialogue",      "Ver di\xc3\xa1logo"                   },
            {"db_filter_all_scenes","All scenes",         "Todas as cenas"                      },
            {"db_filter_all_chars", "All characters",     "Todos os personagens"                  },
            {"db_filter_hint",      "Filter\xe2\x80\xa6", "Filtrar\xe2\x80\xa6"             },
            // ── Dock titles ─────────────────────────────────────────────
            {"dock_scenes",         "Scenes",             "Cenas"                          },
            {"dock_stats",          "Statistics",         "Estat\xc3\xadsticas"                      },
            {"dock_database",       "Database",           "Banco de dados"                        },
            // ── Status bar ──────────────────────────────────────────────
            {"status_block",        "Block:",             "Bloco:"                          },
            {"status_page",         "Page:",              "P\xc3\xa1gina:"                           },
            {"status_words",        "Words:",             "Palavras:"                          },
            // ── Misc ────────────────────────────────────────────────────
            {"untitled",            "Untitled",           "Sem t\xc3\xadtulo"                        },
            {"open_script_dlg",     "Open screenplay",    "Abrir roteiro"                 },
            {"save_error",          "Save error",         "Erro ao salvar"                      },
            {nullptr, nullptr, nullptr}
        };
        // clang-format on
        for (int i = 0; kTable[i].key; ++i) {
            if (std::strcmp(kTable[i].key, key) == 0)
                return current_ == AppLanguage::Portuguese
                           ? kTable[i].pt : kTable[i].en;
        }
        return key; // fallback: return key itself
    }

private:
    inline static AppLanguage           current_   = AppLanguage::English;
    inline static std::function<void()> on_change_;
};

} // namespace screenplay::config
