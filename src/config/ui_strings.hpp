#pragma once
// config/ui_strings.hpp
// The application's UI translations.
//
// Keys are the ENGLISH text itself, not opaque identifiers ("action_new"):
// English is the default language, so a call site reads as the string it
// displays, an untranslated string still renders correctly instead of showing
// a key, and adding a new string is one table row rather than a key plus two
// values. Anything missing from the table falls through to its English text.
//
// Owned responsibility: the EN→PT string table and the lookup. Which language
// is active belongs to LanguageConfig (config/language.hpp).

#include "language.hpp"
#include "../model/model.hpp"

#include <QHash>
#include <QString>

namespace screenplay::config {

// Translate a user-visible string. Pass the English text verbatim.
inline QString tr_ui(const char* english) {
    const QString key = QString::fromUtf8(english);
    if (LanguageConfig::current() == AppLanguage::English) return key;

    static const QHash<QString, QString> pt = {
        // ── Menu titles ─────────────────────────────────────────────────
        { "&File",            QString::fromUtf8("&Arquivo")            },
        { "&Edit",            QString::fromUtf8("&Editar")             },
        { "&Document",        QString::fromUtf8("&Documento")          },
        { "&View",            QString::fromUtf8("&Exibir")             },
        { "F&ormat",          QString::fromUtf8("F&ormatar")           },
        { "&Tools",           QString::fromUtf8("&Ferramentas")        },
        { "&Notes",           QString::fromUtf8("&Notas")              },
        { "&Help",            QString::fromUtf8("&Ajuda")              },

        // ── File ────────────────────────────────────────────────────────
        { "New",              QString::fromUtf8("Novo")                },
        { "New document",     QString::fromUtf8("Novo documento")      },
        { "Open…",            QString::fromUtf8("Abrir…")              },
        { "Open",             QString::fromUtf8("Abrir")               },
        { "Open Recent",      QString::fromUtf8("Abrir recente")       },
        { "Save",             QString::fromUtf8("Salvar")              },
        { "Save As…",         QString::fromUtf8("Salvar como…")        },
        { "Import FDX…",      QString::fromUtf8("Importar FDX…")       },
        { "Import Fountain…", QString::fromUtf8("Importar Fountain…")  },
        { "Export PDF…",      QString::fromUtf8("Exportar PDF…")       },
        { "Export Fountain…", QString::fromUtf8("Exportar Fountain…")  },
        { "Export FDX…",      QString::fromUtf8("Exportar FDX…")       },
        { "Export",           QString::fromUtf8("Exportar")            },
        { "Print…",           QString::fromUtf8("Imprimir…")           },

        // ── Edit ────────────────────────────────────────────────────────
        { "Undo",             QString::fromUtf8("Desfazer")            },
        { "Redo",             QString::fromUtf8("Refazer")             },
        { "Cut",              QString::fromUtf8("Recortar")            },
        { "Copy",             QString::fromUtf8("Copiar")              },
        { "Paste",            QString::fromUtf8("Colar")               },
        { "Select All",       QString::fromUtf8("Selecionar tudo")     },
        { "Select Block",     QString::fromUtf8("Selecionar bloco")    },
        { "Find…",            QString::fromUtf8("Localizar…")          },
        { "Search",           QString::fromUtf8("Pesquisar")           },
        { "Replace…",         QString::fromUtf8("Substituir…")         },

        // ── Document ────────────────────────────────────────────────────
        // "Capa" throughout, at the user's request — not "página de rosto".
        { "Title Page",          QString::fromUtf8("Capa")                 },
        { "Title Page…",         QString::fromUtf8("Capa…")                },
        { "Edit Title Page…",    QString::fromUtf8("Editar capa…")         },
        { "Include Title Page",  QString::fromUtf8("Incluir capa")         },
        { "Cover",               QString::fromUtf8("Capa")                 },
        { "Scene Numbers",       QString::fromUtf8("Números de cena")      },
        { "Go to Scene…",        QString::fromUtf8("Ir para cena…")        },
        { "Go to Page…",         QString::fromUtf8("Ir para página…")      },
        { "Page Layout",         QString::fromUtf8("Layout da página")     },

        // ── Production ──────────────────────────────────────────────────
        // The colour ladder keeps its industry names: on a Brazilian set the
        // pages are still called "as páginas azuis", so translating the
        // colours is right, but the ORDER and the vocabulary are the same.
        { "Production",          QString::fromUtf8("Produção")             },
        { "Revision Pass: %1…",  QString::fromUtf8("Revisão: %1…")         },
        { "Revision pass",       QString::fromUtf8("Revisão")              },
        { "Clear Revision Marks",QString::fromUtf8("Limpar marcas de revisão") },
        { "Lock Scene Numbers",  QString::fromUtf8("Travar números de cena") },
        { "Omit Scene",          QString::fromUtf8("Omitir cena")          },
        { "White",               QString::fromUtf8("Branco")               },
        { "Blue",                QString::fromUtf8("Azul")                 },
        { "Pink",                QString::fromUtf8("Rosa")                 },
        { "Yellow",              QString::fromUtf8("Amarelo")              },
        { "Green",               QString::fromUtf8("Verde")                },
        { "Goldenrod",           QString::fromUtf8("Ouro")                 },
        { "Salmon",              QString::fromUtf8("Salmão")               },
        { "Cherry",              QString::fromUtf8("Cereja")               },
        { "Buff",                QString::fromUtf8("Palha")                },
        { "Tan",                 QString::fromUtf8("Bege")                 },
        { "Each pass is issued on its own colour of paper. While a pass is "
          "open, every edit marks its element with an asterisk in the margin.",
          QString::fromUtf8("Cada revisão sai numa cor de papel. Enquanto uma "
                            "revisão está aberta, toda edição marca o elemento "
                            "com um asterisco na margem.") },
        { "Revision marking off",
          QString::fromUtf8("Marcação de revisão desligada") },
        { "%1 revision — edits are now marked",
          QString::fromUtf8("Revisão %1 — as edições passam a ser marcadas") },
        { "Revision marks cleared",
          QString::fromUtf8("Marcas de revisão apagadas") },
        { "Scene numbers locked — new scenes get 1A, 1B…",
          QString::fromUtf8("Números de cena travados — cenas novas recebem 1A, 1B…") },
        { "Scene numbers unlocked — they follow position again",
          QString::fromUtf8("Números de cena destravados — voltam a seguir a posição") },
        { "Scene omitted — its number stays. Ctrl+Z restores it.",
          QString::fromUtf8("Cena omitida — o número permanece. Ctrl+Z restaura.") },
        { "Apply",               QString::fromUtf8("Aplicar")              },
        { "Reports",             QString::fromUtf8("Relatórios")           },
        { "Reports…",            QString::fromUtf8("Relatórios…")          },
        { "Scene Report",        QString::fromUtf8("Relatório de cenas")   },
        { "Character Report",    QString::fromUtf8("Relatório de personagens") },
        { "Location Report",     QString::fromUtf8("Relatório de locações") },
        { "Heading",             QString::fromUtf8("Cabeçalho")            },
        { "Cast",                QString::fromUtf8("Elenco")               },
        { "Elements",            QString::fromUtf8("Elementos")            },
        { "Lines",               QString::fromUtf8("Falas")                },
        { "First scene",         QString::fromUtf8("Primeira cena")        },
        { "Usual extension",     QString::fromUtf8("Extensão usual")       },
        { "Location",            QString::fromUtf8("Locação")              },
        { "INT/EXT",             QString::fromUtf8("INT/EXT")              },
        { "Time of day",         QString::fromUtf8("Hora do dia")          },
        { "Scene numbers",       QString::fromUtf8("Números das cenas")    },
        { "Export PDF…",         QString::fromUtf8("Exportar PDF…")        },
        { "PDF document",        QString::fromUtf8("Documento PDF")        },
        { "Nothing to report yet.",
          QString::fromUtf8("Ainda não há nada para relatar.")             },
        { "Scenes",              QString::fromUtf8("Cenas")                },
        { "Shot",                QString::fromUtf8("Plano")                },
        { "General",             QString::fromUtf8("Geral")                },
        { "Act Break",           QString::fromUtf8("Quebra de ato")        },
        { "Page Break Before",   QString::fromUtf8("Quebra de página antes") },
        { "This element now starts a page",
          QString::fromUtf8("Este elemento passa a iniciar uma página")    },
        { "Page break removed",  QString::fromUtf8("Quebra de página removida") },
        { "PAGE BREAK",          QString::fromUtf8("QUEBRA DE PÁGINA")     },
        { "Export RTF…",         QString::fromUtf8("Exportar RTF…")        },
        { "Export Text…",        QString::fromUtf8("Exportar texto…")      },
        { "Rich Text",           QString::fromUtf8("Rich Text")            },
        { "Plain text",          QString::fromUtf8("Texto simples")        },
        { "RTF exported",        QString::fromUtf8("RTF exportado")        },
        { "Text exported",       QString::fromUtf8("Texto exportado")      },
        { "Based on…",           QString::fromUtf8("Baseado em…")          },
        // Panel titles read as section headings, so they are set in caps at the
        // source rather than uppercased by a stylesheet — Portuguese accents
        // survive a translation table but not every text-transform.
        { "SCENE LIST",          QString::fromUtf8("LISTA DE CENAS")       },
        { "SCRIPT BREAKDOWN",    QString::fromUtf8("DECUPAGEM DO ROTEIRO") },
        { "AUTHOR NOTES",        QString::fromUtf8("NOTAS DO AUTOR")       },
        { "Script Breakdown",    QString::fromUtf8("Decupagem do roteiro") },
        { "Script Breakdown \xe2\x80\x94 totals, scenes, characters, dialogue (Ctrl+Shift+B)",
          QString::fromUtf8("Decupagem do roteiro — totais, cenas, "
                            "personagens, diálogo (Ctrl+Shift+B)") },
        { "Minutes",             QString::fromUtf8("Minutos")              },
        { "Dialogue",            QString::fromUtf8("Diálogo")              },
        { "Line",                QString::fromUtf8("Fala")                 },
        { "Filter…",             QString::fromUtf8("Filtrar…")             },
        { "Spell check dictionary missing",
          QString::fromUtf8("Falta o dicionário do corretor") },
        { "Open Windows settings",
          QString::fromUtf8("Abrir configurações do Windows") },
        { "Later",               QString::fromUtf8("Depois")                },
        { "Info 1",              QString::fromUtf8("Info 1")               },
        { "Info 2",              QString::fromUtf8("Info 2")               },
        { "Info 3",              QString::fromUtf8("Info 3")               },
        { "Info 4",              QString::fromUtf8("Info 4")               },
        { "Language changed — spell check now uses %1",
          QString::fromUtf8("Idioma alterado — o corretor passa a usar %1") },

        // ── View ────────────────────────────────────────────────────────
        { "Zoom In",          QString::fromUtf8("Ampliar")             },
        { "Zoom Out",         QString::fromUtf8("Reduzir")             },
        { "Full Screen",      QString::fromUtf8("Tela cheia")          },
        { "Focus Mode",       QString::fromUtf8("Modo foco")           },
        { "Theme",            QString::fromUtf8("Tema")                },
        { "Light",            QString::fromUtf8("Claro")               },
        { "Dark",             QString::fromUtf8("Escuro")              },
        { "Chamber",          QString::fromUtf8("Chamber")             },
        { "Scenes",           QString::fromUtf8("Cenas")               },
        { "Statistics",       QString::fromUtf8("Estatísticas")        },
        { "Stats",            QString::fromUtf8("Estatísticas")        },
        { "Database",         QString::fromUtf8("Banco de dados")      },
        { "Script Database",  QString::fromUtf8("Banco de dados")      },
        { "Characters",       QString::fromUtf8("Personagens")         },

        // ── Format / elements ───────────────────────────────────────────
        { "Scene Heading",    QString::fromUtf8("Cabeçalho de cena")   },
        { "Scene",            QString::fromUtf8("Cena")                },
        { "Action",           QString::fromUtf8("Ação")                },
        { "Character",        QString::fromUtf8("Personagem")          },
        { "Parenthetical",    QString::fromUtf8("Parêntese")           },
        { "Dialogue",         QString::fromUtf8("Diálogo")             },
        { "Transition",       QString::fromUtf8("Transição")           },
        { "Dual Dialogue",    QString::fromUtf8("Diálogo duplo")       },
        { "Bold",             QString::fromUtf8("Negrito")             },
        { "Italic",           QString::fromUtf8("Itálico")             },
        { "Underline",        QString::fromUtf8("Sublinhado")          },
        { "Align Left",       QString::fromUtf8("Alinhar à esquerda")  },
        { "Center",           QString::fromUtf8("Centralizar")         },
        { "Align Right",      QString::fromUtf8("Alinhar à direita")   },
        { "Bold Scene Headings",
          QString::fromUtf8("Cabeçalhos de cena em negrito")           },

        // ── Notes ───────────────────────────────────────────────────────
        { "Notes",            QString::fromUtf8("Notas")               },
        { "Note on This Block…", QString::fromUtf8("Nota neste bloco…") },
        { "Note on this block",  QString::fromUtf8("Nota neste bloco")  },
        { "All Notes",        QString::fromUtf8("Todas as notas")      },
        { "All notes",        QString::fromUtf8("Todas as notas")      },
        { "Write a note for this block…",
          QString::fromUtf8("Escreva uma nota para este bloco…")       },
        { "No notes yet — Ctrl+Alt+M adds one",
          QString::fromUtf8("Nenhuma nota ainda — Ctrl+Alt+M cria uma") },

        // ── Tools / Help ────────────────────────────────────────────────
        { "Language",         QString::fromUtf8("Idioma")              },
        { "English",          QString::fromUtf8("Inglês")              },
        { "Portuguese",       QString::fromUtf8("Português")           },
        { "Script Language…", QString::fromUtf8("Idioma do roteiro…")  },
        { "Spell Check",      QString::fromUtf8("Corretor ortográfico") },
        { "Keyboard Shortcuts", QString::fromUtf8("Atalhos de teclado") },
        { "About",            QString::fromUtf8("Sobre")               },
        { "Menu",             QString::fromUtf8("Menu")                },

        // ── Dialogs ─────────────────────────────────────────────────────
        { "Unsaved changes",  QString::fromUtf8("Alterações não salvas") },
        { "This screenplay has changes that have not been saved yet.",
          QString::fromUtf8("Este roteiro tem alterações que ainda não foram salvas.") },
        { "Discard",          QString::fromUtf8("Descartar")           },
        { "Cancel",           QString::fromUtf8("Cancelar")            },
        { "Clear",            QString::fromUtf8("Limpar")              },
        { "OK",               QString::fromUtf8("OK")                  },

        // ── Status strip / misc ─────────────────────────────────────────
        { "Words",            QString::fromUtf8("Palavras")            },
        { "Page",             QString::fromUtf8("Página")              },
        { "Untitled",         QString::fromUtf8("Sem título")          },
        { "Saved",            QString::fromUtf8("Salvo")               },
        { "Not saved yet",    QString::fromUtf8("Ainda não salvo")     },
        { "Unsaved changes ●", QString::fromUtf8("Alterações não salvas ●") },
        { "Filter scenes…",   QString::fromUtf8("Filtrar cenas…")      },
        // ── Keyboard-shortcuts dialog ───────────────────────────────────
        { "Keyboard Shortcuts", QString::fromUtf8("Atalhos de teclado")  },
        { "Filter shortcuts…",  QString::fromUtf8("Filtrar atalhos…")    },
        { "Shortcut",         QString::fromUtf8("Atalho")              },
        { "Context",          QString::fromUtf8("Contexto")            },
        { "Editor",           QString::fromUtf8("Editor")              },
        { "SmartType",        QString::fromUtf8("SmartType")           },
        { "New block",        QString::fromUtf8("Novo bloco")          },
        { "Accept suggestion / next type",
          QString::fromUtf8("Aceitar sugestão / próximo tipo")         },
        { "Previous block type",
          QString::fromUtf8("Tipo de bloco anterior")                  },
        { "Delete char / merge block",
          QString::fromUtf8("Apagar caractere / juntar bloco")         },
        { "Delete forward / merge block",
          QString::fromUtf8("Apagar à frente / juntar bloco")          },
        { "Delete previous word", QString::fromUtf8("Apagar palavra anterior") },
        { "Delete next word",     QString::fromUtf8("Apagar próxima palavra") },
        { "Line start / end",     QString::fromUtf8("Início / fim da linha") },
        { "Document start / end", QString::fromUtf8("Início / fim do documento") },
        { "Previous / next visual line",
          QString::fromUtf8("Linha visual anterior / seguinte")        },
        { "Previous / next block",
          QString::fromUtf8("Bloco anterior / seguinte")               },
        { "Previous / next character",
          QString::fromUtf8("Caractere anterior / seguinte")           },
        { "Previous / next word",
          QString::fromUtf8("Palavra anterior / seguinte")             },
        { "Extend selection",  QString::fromUtf8("Estender seleção")   },
        { "Select to line start / end",
          QString::fromUtf8("Selecionar até o início / fim da linha")  },
        { "Zoom in (alternate)", QString::fromUtf8("Ampliar (alternativo)") },
        { "Zoom",             QString::fromUtf8("Zoom")                },
        { "Select word",      QString::fromUtf8("Selecionar palavra")  },
        { "Select block",     QString::fromUtf8("Selecionar bloco")    },
        { "Accept suggestion", QString::fromUtf8("Aceitar sugestão")   },
        { "Previous / next suggestion",
          QString::fromUtf8("Sugestão anterior / seguinte")            },
        { "Accept suggestion N", QString::fromUtf8("Aceitar sugestão N") },
        { "Dismiss",          QString::fromUtf8("Dispensar")           },

        // ── Scene operations / database filters ─────────────────────────
        { "New Scene After",  QString::fromUtf8("Nova cena depois")    },
        { "Duplicate Scene",  QString::fromUtf8("Duplicar cena")       },
        { "Delete Scene…",    QString::fromUtf8("Excluir cena…")       },
        { "Delete scene",     QString::fromUtf8("Excluir cena")        },
        { "Delete",           QString::fromUtf8("Excluir")             },
        { "Move Scene Up",    QString::fromUtf8("Mover cena para cima") },
        { "Move Scene Down",  QString::fromUtf8("Mover cena para baixo") },
        { "Insert Scene After Current",
          QString::fromUtf8("Inserir cena após a atual")               },
        { "All",              QString::fromUtf8("Todos")               },
        { "Paren.",           QString::fromUtf8("Parênt.")             },
        { "Autosaved.",       QString::fromUtf8("Salvo automaticamente.") },
        { "Highlighting %1 — click again or press Esc to clear",
          QString::fromUtf8("Destacando %1 — clique de novo ou Esc para limpar") },

        { "Find:",            QString::fromUtf8("Localizar:")          },
        { "Replace:",         QString::fromUtf8("Substituir:")         },
        { "Replace",          QString::fromUtf8("Substituir")          },
        { "Replace All",      QString::fromUtf8("Substituir tudo")     },
        { "Export PDF (WGA layout)",
          QString::fromUtf8("Exportar PDF (layout WGA)")               },

        // ── Cover (title page) editing ──────────────────────────────────
        { "Title",            QString::fromUtf8("Título")              },
        { "Written by",       QString::fromUtf8("Escrito por")         },
        { "Author name",      QString::fromUtf8("Nome do autor")       },
        { "Contact",          QString::fromUtf8("Contato")             },
        { "Click to add a logo", QString::fromUtf8("Clique para adicionar um logo") },
        { "Choose a logo",    QString::fromUtf8("Escolher um logo")    },
        { "Replace logo…",    QString::fromUtf8("Substituir logo…")    },
        { "Remove logo",      QString::fromUtf8("Remover logo")        },
        { "Images",           QString::fromUtf8("Imagens")             },
        { "Could not load image",
          QString::fromUtf8("Não foi possível carregar a imagem")      },
        { "\"%1\" could not be read. Supported formats here are: %2.",
          QString::fromUtf8("Não foi possível ler \"%1\". Os formatos suportados aqui são: %2.") },
        { "Restore",          QString::fromUtf8("Restaurar")           },
        { "Recover unsaved work",
          QString::fromUtf8("Recuperar trabalho não salvo")            },
        { "Unsaved work from a previous session was found (autosaved %1). Restore it?",
          QString::fromUtf8("Foi encontrado trabalho não salvo de uma sessão anterior (salvo automaticamente %1). Restaurar?") },
        { "Delete \"%1\" and all its content? This can be undone with Ctrl+Z.",
          QString::fromUtf8("Excluir \"%1\" e todo o seu conteúdo? Isso pode ser desfeito com Ctrl+Z.") },
        { "Recover",          QString::fromUtf8("Recuperar")           },
        { "Language",         QString::fromUtf8("Idioma")              },
        { "WGA-standard screenplay editor.",
          QString::fromUtf8("Editor de roteiros no padrão WGA.")       },

        // ── Cover / script-language dialogs ─────────────────────────────
        { "Include title page", QString::fromUtf8("Incluir capa")      },
        { "Screenplay title", QString::fromUtf8("Título do roteiro")   },
        { "One author per line",
          QString::fromUtf8("Um autor por linha")                      },
        { "Email or phone",   QString::fromUtf8("E-mail ou telefone")  },
        { "Title:",           QString::fromUtf8("Título:")             },
        { "Credit:",          QString::fromUtf8("Crédito:")            },
        { "Author(s):",       QString::fromUtf8("Autor(es):")          },
        { "Contact:",         QString::fromUtf8("Contato:")            },
        { "All fields are optional. Do not add date, copyright or logline — not WGA standard.",
          QString::fromUtf8("Todos os campos são opcionais. Não inclua data, "
                            "copyright ou logline — não é padrão WGA.")  },
        { "Script Language",  QString::fromUtf8("Idioma do roteiro")   },
        { "What language is your screenplay written in?",
          QString::fromUtf8("Em qual idioma o seu roteiro está escrito?") },
        { "Select every language used in the screenplay. A word is only marked as misspelled when it is wrong in all of them.",
          QString::fromUtf8("Selecione todos os idiomas usados no roteiro. Uma palavra "
                            "só é marcada como errada quando está errada em todos eles.") },

        // ── Statistics panel ────────────────────────────────────────────
        { "Pages",            QString::fromUtf8("Páginas")             },
        { "Time",             QString::fromUtf8("Tempo")               },
        { "lines",            QString::fromUtf8("falas")               },
        { "scene",            QString::fromUtf8("cena")                },
        { "scenes",           QString::fromUtf8("cenas")               },
        { "Locations",        QString::fromUtf8("Locações")            },
        { "Time of Day",      QString::fromUtf8("Período do dia")      },
        { "Click a character to highlight their dialogue",
          QString::fromUtf8("Clique num personagem para destacar as falas dele") },
        { "Locations extracted from Scene Headings",
          QString::fromUtf8("Locações extraídas dos cabeçalhos de cena") },
        { "Scenes grouped by time of day",
          QString::fromUtf8("Cenas agrupadas por período do dia")      },
        { "Search only inside this element type",
          QString::fromUtf8("Buscar apenas dentro deste tipo de elemento") },
        { "Search… (Enter / Shift+Enter)",
          QString::fromUtf8("Buscar… (Enter / Shift+Enter)")           },
        { "Previous match (Shift+Enter)",
          QString::fromUtf8("Ocorrência anterior (Shift+Enter)")       },
        { "Next match (Enter)",
          QString::fromUtf8("Próxima ocorrência (Enter)")              },
        { "Close search (Esc)",
          QString::fromUtf8("Fechar busca (Esc)")                      },

        { "Minimize",         QString::fromUtf8("Minimizar")           },
        { "Maximize",         QString::fromUtf8("Maximizar")           },
        { "Restore",          QString::fromUtf8("Restaurar")           },
        { "Close",            QString::fromUtf8("Fechar")              },
    };

    const auto it = pt.constFind(key);
    return it == pt.constEnd() ? key : *it;
}

// The short badge shown beside the caret's line, and the status strip's
// element readout. Kept separate from tr_ui() because these are abbreviations,
// not the menu wording.
inline QString tr_block_label(screenplay::BlockType t) {
    const bool en = (LanguageConfig::current() == AppLanguage::English);
    switch (t) {
    case screenplay::BlockType::SceneHeading:
        return en ? "SCENE"      : QString::fromUtf8("CENA");
    case screenplay::BlockType::Action:
        return en ? "ACTION"     : QString::fromUtf8("AÇÃO");
    case screenplay::BlockType::Character:
        return en ? "CHARACTER"  : QString::fromUtf8("PERSONAGEM");
    case screenplay::BlockType::Parenthetical:
        return en ? "PAREN."     : QString::fromUtf8("PARÊNTESE");
    case screenplay::BlockType::Dialogue:
        return en ? "DIALOGUE"   : QString::fromUtf8("DIÁLOGO");
    case screenplay::BlockType::Transition:
        return en ? "TRANS."     : QString::fromUtf8("TRANSIÇÃO");
    case screenplay::BlockType::DualDialogue:
        return en ? "DUAL"       : QString::fromUtf8("DUPLO");
    case screenplay::BlockType::Shot:
        return en ? "SHOT"       : QString::fromUtf8("PLANO");
    case screenplay::BlockType::General:
        return en ? "GENERAL"    : QString::fromUtf8("GERAL");
    case screenplay::BlockType::ActBreak:
        return en ? "ACT"        : QString::fromUtf8("ATO");
    }
    return {};
}

} // namespace screenplay::config
