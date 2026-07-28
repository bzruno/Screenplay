// ui/main_menu.cpp
// Definition of MainWindow::setup_toolbar(), moved out of main_window.hpp
// to shrink that header. Verbatim move â behaviour unchanged. Everything the
// method needs (Qt, icons, AppHeader, FloatingToolbar, ThemeManager, the
// action members it fills in) comes through main_window.hpp.

#include "main_window.hpp"

void MainWindow::setup_toolbar() {
        // ═══════════════════════════════════════════════════════════════════
        // Chrome assembly. Two pieces, deliberately separate surfaces:
        //   header_  — one generous row (identity · mode switch · actions)
        //              plus the near-invisible menu bar, via setMenuWidget()
        //   toolbar_ — the floating, centred tool card, first row of the
        //              central column so the sidebar sits beside it
        // Everything below the two `setup_*` calls is unchanged action
        // construction: the actions are the app's behaviour and this redesign
        // only changes how they are presented.
        // ═══════════════════════════════════════════════════════════════════
        header_ = new screenplay::ui::AppHeader(this);
        auto* header_vbox = new QVBoxLayout;
        header_vbox->setContentsMargins(0, 0, 0, 0);
        header_vbox->setSpacing(0);

        auto* header_host = new QWidget(this);
        auto* host_col = new QVBoxLayout(header_host);
        host_col->setContentsMargins(0, 0, 0, 0);
        host_col->setSpacing(0);
        host_col->addWidget(header_);
        host_col->addLayout(header_vbox);
        header_host->setAutoFillBackground(true);
        {
            QPalette hp = header_host->palette();
            hp.setColor(QPalette::Window,
                        screenplay::ui::ThemeManager::instance().palette().Bg0);
            header_host->setPalette(hp);
        }

        // The ☰ button's menu is populated at the end of setup_toolbar(), once
        // every menu exists — see the "main menu" block below.

        // Editable document name — same rename semantics as before, now
        // owned by the header widget instead of a bare row.
        doc_name_edit_ = header_->name_edit();
        connect(doc_name_edit_, &QLineEdit::editingFinished, this, [this]{
            QString new_name = doc_name_edit_->text().trimmed();
            if (new_name.isEmpty()) {
                update_doc_name_display();
                return;
            }
            if (current_path_.isEmpty()) {
                // No file yet — just update the in-memory display name
                doc_custom_name_ = new_name;
                update_title();
                return;
            }
            // Rename file on disk
            QFileInfo fi(current_path_);
            QString new_path = fi.absolutePath() + "/" + new_name;
            if (!new_name.contains('.'))
                new_path += "." + fi.suffix();
            if (QFile::rename(current_path_, new_path)) {
                current_path_ = new_path;
            } else {
                update_doc_name_display();  // revert on failure
            }
            update_title();
        });

        // ── Menu bar — now entirely inside the hamburger ─────────────────
        // Every menu is still built exactly as before (so no command is lost),
        // but the bar itself is never shown: it is parented off-screen and its
        // menus are re-hung under the header's ☰ button. That is what removes
        // the last visible trace of a classic desktop menu row.
        auto* mb = new QMenuBar(header_host);
        mb->hide();
        header_vbox->addWidget(mb);
        mb->setMaximumHeight(0);

        // File
        auto* mFile = mb->addMenu(tr_ui("&File"));
        auto* act_new = mFile->addAction(tr_ui("New"), this, &MainWindow::on_new);
        act_new->setShortcut(QKeySequence::New);
        act_new->setIcon(icons::make(icons::Id::New));
        act_new->setToolTip(tr_ui("New script (Ctrl+N)"));
        auto* act_open = mFile->addAction(tr_ui("Open\xe2\x80\xa6"), this, &MainWindow::on_open);
        act_open->setShortcut(QKeySequence::Open);
        act_open->setIcon(icons::make(icons::Id::Open));
        act_open->setToolTip(tr_ui("Open script (Ctrl+O)"));
        {
            recent_menu_ = mFile->addMenu(tr_ui("Open Recent"));
            connect(recent_menu_, &QMenu::aboutToShow,
                    this, &MainWindow::populate_recent_menu);
            populate_recent_menu();   // set initial enabled state
        }
        auto* act_save = mFile->addAction(tr_ui("Save"), this, &MainWindow::on_save);
        act_save->setShortcut(QKeySequence::Save);
        act_save->setIcon(icons::make(icons::Id::Save));
        act_save->setToolTip(tr_ui("Save (Ctrl+S)"));
        mFile->addAction(tr_ui("Save As\xe2\x80\xa6"), this, &MainWindow::on_save_as)->setShortcut(QKeySequence("Ctrl+Shift+S"));
        mFile->addSeparator();
        mFile->addAction(tr_ui("Import FDX\xe2\x80\xa6"),      this, &MainWindow::on_import_fdx);
        mFile->addAction(tr_ui("Import Fountain\xe2\x80\xa6"), this, &MainWindow::on_import_fountain);
        mFile->addSeparator();
        auto* act_pdf = mFile->addAction(tr_ui("Export PDF\xe2\x80\xa6"),
                                         this, &MainWindow::on_export_pdf);
        act_pdf->setIcon(icons::make(icons::Id::Pdf));
        act_pdf->setToolTip(tr_ui("Export PDF (WGA layout)"));
        mFile->addAction(tr_ui("Export Fountain\xe2\x80\xa6"), this, &MainWindow::on_export_fountain);
        mFile->addAction(tr_ui("Export RTF\xe2\x80\xa6"),      this, &MainWindow::on_export_rtf);
        mFile->addAction(tr_ui("Export Text\xe2\x80\xa6"),     this, &MainWindow::on_export_text);
        mFile->addAction(tr_ui("Export FDX\xe2\x80\xa6"),      this, &MainWindow::on_export_fdx);
        mFile->addSeparator();
        auto* act_print = mFile->addAction(tr_ui("Print\xe2\x80\xa6"),
                                           this, &MainWindow::on_print);
        act_print->setShortcut(QKeySequence::Print);
        act_print->setIcon(icons::make(icons::Id::Print));
        act_print->setToolTip(tr_ui("Print (Ctrl+P)"));
        // Edit
        auto* mEdit = mb->addMenu(tr_ui("&Edit"));
        auto* act_undo = mEdit->addAction(tr_ui("Undo"), canvas_, [this]{ canvas_->ctrl().handle_key({screenplay::editor::Key::Undo}); canvas_->request_relayout(true); emit canvas_->script_changed(); });
        act_undo->setShortcut(QKeySequence::Undo);
        act_undo->setIcon(icons::make(icons::Id::Undo));
        act_undo->setToolTip(tr_ui("Undo (Ctrl+Z)"));
        auto* act_redo = mEdit->addAction(tr_ui("Redo"), canvas_, [this]{ canvas_->ctrl().handle_key({screenplay::editor::Key::Redo}); canvas_->request_relayout(true); emit canvas_->script_changed(); });
        act_redo->setShortcut(QKeySequence("Ctrl+Shift+Z"));
        act_redo->setIcon(icons::make(icons::Id::Redo));
        act_redo->setToolTip(tr_ui("Redo (Ctrl+Shift+Z)"));
        mEdit->addSeparator();
        mEdit->addAction(tr_ui("Cut"), canvas_, [this]{
            if (canvas_->ctrl().state().has_selection) {
                QApplication::clipboard()->setText(QString::fromStdString(canvas_->ctrl().copy_selection()));
                canvas_->ctrl().cut_selection();
                canvas_->request_relayout(); emit canvas_->script_changed();
            }
        })->setShortcut(QKeySequence::Cut);
        mEdit->addAction(tr_ui("Copy"), canvas_, [this]{
            if (canvas_->ctrl().state().has_selection)
                QApplication::clipboard()->setText(QString::fromStdString(canvas_->ctrl().copy_selection()));
        })->setShortcut(QKeySequence::Copy);
        mEdit->addAction(tr_ui("Paste"), canvas_, [this]{
            std::string txt = QApplication::clipboard()->text().toStdString();
            if (!txt.empty()) { canvas_->ctrl().paste(txt); canvas_->request_relayout(); emit canvas_->script_changed(); }
        })->setShortcut(QKeySequence::Paste);
        mEdit->addSeparator();
        mEdit->addAction(tr_ui("Select All"), canvas_, [this]{
            canvas_->ctrl().select_all(); canvas_->update();
        })->setShortcut(QKeySequence::SelectAll);
        auto* act_sel_block = mEdit->addAction(tr_ui("Select Block"), canvas_, [this]{
            canvas_->ctrl().select_current_block(); canvas_->update();
        });
        act_sel_block->setShortcut(QKeySequence("Ctrl+Shift+A"));
        act_sel_block->setToolTip(
            "Select only the current block, e.g. just the Dialogue the caret is in (Ctrl+Shift+A)");
        mEdit->addSeparator();
        auto* act_find = mEdit->addAction(tr_ui("Find\xe2\x80\xa6"),
                                          this, [this]{ canvas_->show_search(); });
        act_find->setShortcut(QKeySequence::Find);
        act_find->setIcon(icons::make(icons::Id::Search));
        act_find->setToolTip(tr_ui("Find (Ctrl+F)"));
        mEdit->addAction(tr_ui("Replace\xe2\x80\xa6"), this, &MainWindow::on_find_replace)->setShortcut(QKeySequence("Ctrl+H"));
        mEdit->addSeparator();
        {
            // Spell check status indicator (read-only; check runs automatically)
            spell_status_act_ = mEdit->addAction(tr_ui("Spell Check"));
            spell_status_act_->setEnabled(false);
            // Text updated after canvas is ready (post-init timer)
        }

        // Document
        auto* mDoc = mb->addMenu(tr_ui("&Document"));
        {
            capa_toggle_act_ = mDoc->addAction(tr_ui("Include Title Page"));
            capa_toggle_act_->setCheckable(true);
            capa_toggle_act_->setChecked(
                canvas_->ctrl().state().script.title_page.enabled);
            connect(capa_toggle_act_, &QAction::triggered, this, [this](bool checked){
                auto tp = canvas_->ctrl().state().script.title_page;
                tp.enabled = checked;
                canvas_->ctrl().set_title_page(std::move(tp));  // undoable
                canvas_->request_relayout();
                emit canvas_->script_changed();
                // The cover is filled in on the page itself, so switching it on
                // takes the writer there instead of opening a dialog.
                if (checked) canvas_->scroll_to_page(1);
                update_capa_badge();
            });
        }
        mDoc->addSeparator();
        {
            using SNM = ScreenplayCanvas::SceneNumMode;
            auto* mSN  = mDoc->addMenu(tr_ui("Scene Numbers"));
            auto* grp  = new QActionGroup(mSN);
            grp->setExclusive(true);
            struct SNOpt { const char* label; SNM mode; };
            static const SNOpt opts[] = {
                {"None",        SNM::None},
                {"Left only",   SNM::Left},
                {"Right only",  SNM::Right},
                {"Both",        SNM::Both},
            };
            for (const auto& opt : opts) {
                auto* a = mSN->addAction(opt.label);
                a->setCheckable(true);
                a->setChecked(opt.mode == canvas_->scene_num_mode());
                grp->addAction(a);
                SNM m = opt.mode;
                connect(a, &QAction::triggered, this, [this, m]{
                    canvas_->set_scene_num_mode(m);
                    canvas_->update();
                });
            }
        }
        {
            using Prof = screenplay::layout::LayoutProfile;
            auto* mLP  = mDoc->addMenu(tr_ui("Layout Profile"));
            auto* grp  = new QActionGroup(mLP);
            grp->setExclusive(true);
            struct LPOpt { const char* label; Prof profile; };
            static const LPOpt opts[] = {
                {"US Industry Standard (Letter)", Prof::USLetter},
                {"International (A4)",            Prof::InternationalA4},
            };
            for (const auto& opt : opts) {
                auto* a = mLP->addAction(opt.label);
                a->setCheckable(true);
                a->setChecked(opt.profile == canvas_->profile());
                grp->addAction(a);
                Prof p = opt.profile;
                connect(a, &QAction::triggered, this, [this, p]{
                    if (canvas_->profile() == p) return;
                    canvas_->set_profile(p);
                    if (p == screenplay::layout::LayoutProfile::InternationalA4)
                        show_status_message(
                            "A4: mais linhas por p\xc3\xa1gina que Letter \xe2\x80\x94 "
                            "a contagem de p\xc3\xa1ginas muda e n\xc3\xa3o serve para "
                            "estimar dura\xc3\xa7\xc3\xa3o. Use Letter para \xe2\x80\x9c"
                            "1 p\xc3\xa1gina \xe2\x89\x88 1 minuto\xe2\x80\x9d.", 9000);
                    update_status();
                });
            }
        }
        mDoc->addSeparator();
        setup_production_menu(mDoc);
        mDoc->addSeparator();
        mDoc->addAction(tr_ui("Go to Scene\xe2\x80\xa6"), this, &MainWindow::on_goto_scene)->setShortcut(QKeySequence("Ctrl+G"));
        mDoc->addAction(tr_ui("Go to Page\xe2\x80\xa6"),  this, &MainWindow::on_goto_page)->setShortcut(QKeySequence("Ctrl+Shift+G"));
        mDoc->addSeparator();
        // Same operations as the Scenes panel's right-click menu, acting on
        // the scene the caret is currently in — so they're reachable without
        // opening that panel.
        mDoc->addAction(tr_ui("Insert Scene After Current"), this, &MainWindow::on_scene_insert_after_current);
        mDoc->addAction(tr_ui("Duplicate Current Scene"),    this, &MainWindow::on_scene_duplicate_current);
        mDoc->addAction(tr_ui("Delete Current Scene\xe2\x80\xa6"), this, &MainWindow::on_scene_delete_current);
        mDoc->addAction(tr_ui("Move Scene Up"),              this, &MainWindow::on_scene_move_up);
        mDoc->addAction(tr_ui("Move Scene Down"),            this, &MainWindow::on_scene_move_down);

        // View
        auto* mView = mb->addMenu(tr_ui("&View"));
        mView->addAction(tr_ui("Zoom In"),  this, &MainWindow::on_zoom_in)->setShortcut(QKeySequence("Ctrl++"));
        mView->addAction(tr_ui("Zoom Out"), this, &MainWindow::on_zoom_out)->setShortcut(QKeySequence("Ctrl+-"));
        mView->addAction(tr_ui("1:1"),      this, &MainWindow::on_zoom_reset)->setShortcut(QKeySequence("Ctrl+0"));
        mView->addSeparator();
        {
            act_view_scenes_ = mView->addAction(tr_ui("Scenes"));
            act_view_scenes_->setCheckable(true);
            act_view_scenes_->setChecked(false);
            act_view_scenes_->setIcon(icons::make(icons::Id::Scenes));
            act_view_scenes_->setToolTip(tr_ui("Scenes panel"));
            connect(act_view_scenes_, &QAction::triggered, this, [this](bool checked){
                scene_dock_->setVisible(checked);
            });
        }
        {
            act_view_script_ = mView->addAction(tr_ui("Script Breakdown"));
            act_view_script_->setCheckable(true);
            act_view_script_->setChecked(false);
            act_view_script_->setShortcut(QKeySequence("Ctrl+Shift+B"));
            act_view_script_->setIcon(icons::make(icons::Id::Characters));
            act_view_script_->setToolTip(tr_ui("Script Breakdown \xe2\x80\x94 totals, scenes, characters, dialogue (Ctrl+Shift+B)"));
            connect(act_view_script_, &QAction::triggered, this, [this](bool checked){
                script_dock_->setVisible(checked);
                if (checked) refresh_script_panel();
            });
        }
        mView->addSeparator();
        {
            using TM = screenplay::ui::ThemeManager;
            auto& mgr = TM::instance();

            auto icon_for = [](TM::Theme t) {
                switch (t) {
                case TM::Theme::Light:   return icons::Id::Sun;
                case TM::Theme::Dark:    return icons::Id::Moon;
                case TM::Theme::Chamber: return icons::Id::Candle;
                }
                return icons::Id::Sun;
            };

            // Single-click cycle (Light -> Dark -> Chamber -> Light) — the
            // exact same affordance as the previous two-theme toggle, kept on
            // both this menu item and the toolbar button that shares it.
            act_theme_ = mView->addAction(
                QString("Next Theme (%1)").arg(TM::theme_name(mgr.next_theme())));
            act_theme_->setIcon(icons::make(icon_for(mgr.next_theme())));
            act_theme_->setToolTip(tr_ui("Cycle Light / Dark / Chamber theme"));
            connect(act_theme_, &QAction::triggered,
                    this, &MainWindow::on_toggle_theme);

            // Direct 3-way picker submenu.
            auto* mTheme = mView->addMenu(tr_ui("Theme"));
            auto* grp = new QActionGroup(mTheme);
            grp->setExclusive(true);

            auto add_theme_pick = [&](const QString& label, TM::Theme t) -> QAction* {
                auto* a = mTheme->addAction(icons::make(icon_for(t)), label);
                a->setCheckable(true);
                a->setChecked(mgr.theme() == t);
                grp->addAction(a);
                connect(a, &QAction::triggered, this, [this, t]{
                    screenplay::ui::ThemeManager::instance().set_theme(t);
                    apply_theme_everywhere();
                });
                return a;
            };
            act_theme_light_   = add_theme_pick(tr_ui("Light"),   TM::Theme::Light);
            act_theme_dark_    = add_theme_pick(tr_ui("Dark"),    TM::Theme::Dark);
            act_theme_chamber_ = add_theme_pick(tr_ui("Chamber"), TM::Theme::Chamber);
        }
        {
            act_focus_mode_ = mView->addAction(tr_ui("Focus Mode"));
            act_focus_mode_->setCheckable(true);
            act_focus_mode_->setChecked(focus_mode_);
            act_focus_mode_->setShortcut(QKeySequence("Ctrl+Shift+F"));
            act_focus_mode_->setIcon(icons::make(icons::Id::Focus));
            act_focus_mode_->setToolTip(
                "Focus mode \xe2\x80\x94 just you and the page (Ctrl+Shift+F)");
            connect(act_focus_mode_, &QAction::triggered,
                    this, &MainWindow::set_focus_mode);
        }
        mView->addAction(tr_ui("Full Screen"), this, &MainWindow::on_fullscreen)->setShortcut(Qt::Key_F11);

        // Format
        using BT = screenplay::BlockType;
        auto* mFmt = mb->addMenu(tr_ui("F&ormat"));
        struct FDef { const char* name; BT t; const char* sc; };
        static const FDef fdefs[] = {
            {"Scene Heading", BT::SceneHeading,  "Ctrl+1"},
            {"Action",        BT::Action,         "Ctrl+2"},
            {"Character",     BT::Character,      "Ctrl+3"},
            {"Parenthetical", BT::Parenthetical,  "Ctrl+4"},
            {"Dialogue",      BT::Dialogue,       "Ctrl+5"},
            {"Transition",    BT::Transition,     "Ctrl+6"},
            // Shot, General and Act Break complete the element set: without an
            // Act Break there is no way to write television, and without Shot
            // a camera instruction has to masquerade as Action.
            {"Shot",          BT::Shot,           "Ctrl+7"},
            {"General",       BT::General,        "Ctrl+8"},
            {"Act Break",     BT::ActBreak,       "Ctrl+9"},
        };
        QAction** blk_slots[] = { &act_blk_scene_, &act_blk_action_, &act_blk_character_,
                                  &act_blk_parenthetical_, &act_blk_dialogue_, &act_blk_transition_,
                                  &act_blk_shot_, &act_blk_general_, &act_blk_act_break_ };
        auto* blk_group = new QActionGroup(mFmt);
        blk_group->setExclusive(true);
        for (size_t i = 0; i < std::size(fdefs); ++i) {
            const auto& fd = fdefs[i];
            auto* a = mFmt->addAction(tr_ui(fd.name));
            *blk_slots[i] = a;
            auto   t = fd.t;
            a->setShortcut(QKeySequence(fd.sc));
            a->setCheckable(true);
            blk_group->addAction(a);
            // Icons only for the four types that also live in the Row 3
            // toolbar; Parenthetical/Transition stay text-only in the menu,
            // same appearance as before.
            if      (t == BT::SceneHeading) a->setIcon(icons::make(icons::Id::BlockScene));
            else if (t == BT::Action)       a->setIcon(icons::make(icons::Id::BlockAction));
            else if (t == BT::Character)    a->setIcon(icons::make(icons::Id::BlockCharacter));
            else if (t == BT::Dialogue)     a->setIcon(icons::make(icons::Id::BlockDialogue));
            connect(a, &QAction::triggered, this, [this, t]{
                // Parenthetical builds its structural "()" atomically; the other
                // types just switch the current block.
                if (t == BT::Parenthetical)
                    canvas_->ctrl().make_parenthetical();
                else
                    canvas_->ctrl().set_block_type(t);
                canvas_->request_relayout();
                emit canvas_->script_changed();
            });
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction(tr_ui("Page Break Before"));
            a->setShortcut(QKeySequence("Ctrl+Return"));
            a->setCheckable(true);
            act_page_break_ = a;
            connect(a, &QAction::triggered, this, [this]{
                canvas_->ctrl().toggle_page_break();
                canvas_->request_relayout();
                emit canvas_->script_changed();
                show_status_message(
                    canvas_->ctrl().current_has_page_break()
                        ? tr_ui("This element now starts a page")
                        : tr_ui("Page break removed"), 4000);
            });
        }
        {
            auto* a = mFmt->addAction(tr_ui("Dual Dialogue"));
            a->setShortcut(QKeySequence("Ctrl+D"));
            connect(a, &QAction::triggered, this, [this]{
                canvas_->ctrl().activate_dual_dialogue();
                canvas_->request_relayout();
                emit canvas_->script_changed();
            });
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction(icons::make(icons::Id::Bold), tr_ui("Bold"));
            a->setShortcut(QKeySequence("Ctrl+B"));
            a->setCheckable(true);
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_bold(); });
            act_bold_ = a;
        }
        {
            auto* a = mFmt->addAction(icons::make(icons::Id::Italic), tr_ui("Italic"));
            a->setShortcut(QKeySequence("Ctrl+I"));
            a->setCheckable(true);
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_italic(); });
            act_italic_ = a;
        }
        {
            auto* a = mFmt->addAction(icons::make(icons::Id::Underline), tr_ui("Underline"));
            a->setShortcut(QKeySequence("Ctrl+U"));
            a->setCheckable(true);
            connect(a, &QAction::triggered, this, [this]{ canvas_->toggle_underline(); });
            act_underline_ = a;
        }
        mFmt->addSeparator();
        // ── Alignment ─────────────────────────────────────────────────────
        // Only Action and Transition accept an override (see
        // screenplay::supports_alignment) — update_align_buttons() disables
        // all three on every other element type, so the control can never
        // promise something the layout engine will refuse to honour.
        {
            using BA = screenplay::BlockAlign;
            struct Spec { const char* label; const char* keys;
                          icons::Id icon; BA align; QAction** slot; };
            const Spec specs[] = {
                { "Align Left",   "Ctrl+Shift+L", icons::Id::AlignLeft,
                  BA::Left,   &act_align_left_   },
                { "Center",       "Ctrl+Shift+E", icons::Id::AlignCenter,
                  BA::Center, &act_align_center_ },
                { "Align Right",  "Ctrl+Shift+R", icons::Id::AlignRight,
                  BA::Right,  &act_align_right_  },
            };
            for (const auto& s : specs) {
                auto* a = mFmt->addAction(icons::make(s.icon), tr_ui(s.label));
                a->setShortcut(QKeySequence(s.keys));
                a->setCheckable(true);
                a->setToolTip(QString("%1 (%2)").arg(tr_ui(s.label), s.keys));
                const BA target = s.align;
                connect(a, &QAction::triggered, this, [this, target]{
                    canvas_->set_block_align(target);
                });
                *s.slot = a;
            }
        }
        mFmt->addSeparator();
        {
            auto* a = mFmt->addAction(tr_ui("Bold Scene Headings"));
            a->setCheckable(true);
            a->setChecked(canvas_->bold_scene_headings());
            a->setToolTip(tr_ui("Show every scene heading in bold"));
            connect(a, &QAction::toggled, this, [this](bool checked){
                canvas_->set_bold_scene_headings(checked);
            });
        }

        // Tools — Language submenu
        {
            using LC = screenplay::config::LanguageConfig;
            using AL = screenplay::config::AppLanguage;
            auto* mTools = mb->addMenu(QString::fromUtf8(LC::tr("menu_tools")));
            auto* mLang  = mTools->addMenu(QString::fromUtf8(LC::tr("menu_language")));
            auto* lang_en = mLang->addAction(QString::fromUtf8(LC::tr("lang_english")));
            auto* lang_pt = mLang->addAction(QString::fromUtf8(LC::tr("lang_portuguese")));
            lang_en->setCheckable(true);
            lang_pt->setCheckable(true);
            lang_en->setChecked(LC::current() == AL::English);
            lang_pt->setChecked(LC::current() == AL::Portuguese);
            connect(lang_en, &QAction::triggered, this, [this]{
                apply_language(screenplay::config::AppLanguage::English);
            });
            connect(lang_pt, &QAction::triggered, this, [this]{
                apply_language(screenplay::config::AppLanguage::Portuguese);
            });
            mTools->addSeparator();
            mTools->addAction(tr_ui("Script Language\xe2\x80\xa6"), this, &MainWindow::on_script_language);
        }

        // ── Notes: the author's margin comments ───────────────────────────
        // These two back the capsule's last two buttons. A note is metadata —
        // it never renders into the page or the PDF.
        {
            auto* mNotes = mb->addMenu(tr_ui("&Notes"));
            act_note_ = mNotes->addAction(icons::make(icons::Id::Comment),
                                          tr_ui("Note on This Block\xe2\x80\xa6"));
            act_note_->setShortcut(QKeySequence("Ctrl+Alt+M"));
            act_note_->setToolTip(
                tr_ui("Note on this block") + " (Ctrl+Alt+M)");
            connect(act_note_, &QAction::triggered, this, &MainWindow::on_edit_note);

            act_view_notes_ = mNotes->addAction(icons::make(icons::Id::Notes),
                                                tr_ui("All Notes"));
            act_view_notes_->setCheckable(true);
            act_view_notes_->setShortcut(QKeySequence("Ctrl+Alt+N"));
            act_view_notes_->setToolTip(tr_ui("All notes") + " (Ctrl+Alt+N)");
            connect(act_view_notes_, &QAction::triggered, this, [this](bool on){
                if (notes_dock_) notes_dock_->setVisible(on);
            });
        }

        // Help
        auto* mHelp = mb->addMenu(tr_ui("&Help"));
        mHelp->addAction(tr_ui("Keyboard Shortcuts"), this, &MainWindow::on_shortcuts_dialog);
        mHelp->addSeparator();
        mHelp->addAction(tr_ui("About"), this, [this]{
            screenplay::ui::ConfirmDialog::ask(this, {
                tr_ui("About"),
                QString("<b>Screenplay Editor</b><br>"
                        "Version: " APP_VERSION_FULL "<br><br>")
                    + tr_ui("WGA-standard screenplay editor.") + "<br>"
                    + "Qt6 + C++.",
                tr_ui("OK"), QString(), QString(), false });
        });

        // ═══════════════════════════════════════════════════════════════════
        // Header actions — Export (primary) and Theme, both as card menus.
        // ═══════════════════════════════════════════════════════════════════
        {
            // No separate Export button: PDF and Print already sit in the
            // header's document group, and every export format lives in
            // File > Export. A second control for the same commands was
            // duplication, not convenience.
            using TM = screenplay::ui::ThemeManager;
            auto* th = new QMenu(header_);
            th->addAction(act_theme_light_);
            th->addAction(act_theme_dark_);
            th->addAction(act_theme_chamber_);
            header_->theme_button()->setMenu(th);
            header_->set_theme_glyph(
                TM::instance().theme() == TM::Theme::Light  ? icons::Id::Sun
              : TM::instance().theme() == TM::Theme::Dark   ? icons::Id::Moon
                                                            : icons::Id::Candle);
        }

        // ═══════════════════════════════════════════════════════════════════
        // Header, left: document + history commands.
        // ═══════════════════════════════════════════════════════════════════
        header_->add_document_action(act_new,  icons::Id::New);
        header_->add_document_action(act_open, icons::Id::Open);
        header_->add_document_action(act_save, icons::Id::Save);
        header_->add_document_action(act_pdf,   icons::Id::Pdf);
        header_->add_document_action(act_print, icons::Id::Print);
        header_->add_document_action(act_undo, icons::Id::Undo);
        header_->add_document_action(act_redo, icons::Id::Redo);

        // ═══════════════════════════════════════════════════════════════════
        // Header, right: search, the three panels, and Focus Mode.
        // ═══════════════════════════════════════════════════════════════════
        header_->add_aux_action(act_find,           icons::Id::Search);
        header_->add_aux_action(act_view_scenes_,   icons::Id::Scenes);
        header_->add_aux_action(act_view_script_, icons::Id::Stats);
        header_->add_aux_action(act_focus_mode_,    icons::Id::Focus);

        // ═══════════════════════════════════════════════════════════════════
        // The writing capsule. Every writing tool the app has, icon-only with
        // the name and shortcut in the tooltip: element types, character
        // styles, alignment, then notes.
        // ═══════════════════════════════════════════════════════════════════
        toolbar_ = new screenplay::ui::FloatingToolbar(central_);
        auto* card = toolbar_->card();

        card->add_group();
        card->add(act_blk_scene_,         icons::Id::BlockScene);
        card->add(act_blk_action_,        icons::Id::BlockAction);
        card->add(act_blk_character_,     icons::Id::BlockCharacter);
        card->add(act_blk_dialogue_,      icons::Id::BlockDialogue);
        card->add(act_blk_parenthetical_, icons::Id::BlockParenthetical);
        card->add(act_blk_transition_,    icons::Id::BlockTransition);

        card->add_group();
        card->add(act_bold_,      icons::Id::Bold);
        card->add(act_italic_,    icons::Id::Italic);
        card->add(act_underline_, icons::Id::Underline);

        card->add_group();
        card->add(act_align_left_,   icons::Id::AlignLeft);
        card->add(act_align_center_, icons::Id::AlignCenter);
        card->add(act_align_right_,  icons::Id::AlignRight);

        card->add_group();
        card->add(act_note_,       icons::Id::Comment);
        card->add(act_view_notes_, icons::Id::Notes);

        toolbar_->refresh_theme();
        central_col_->insertWidget(0, toolbar_);

        // ═══════════════════════════════════════════════════════════════════
        // The ☰ main menu. Every menu built above is re-hung here, so the
        // full command set stays reachable with no visible menu bar at all.
        // ═══════════════════════════════════════════════════════════════════
        {
            auto* main_menu = new QMenu(header_);
            for (QAction* a : mb->actions())
                main_menu->addAction(a);
            header_->menu_button()->setMenu(main_menu);

            // A QAction's shortcut only resolves while one of the widgets it
            // was added to is visible — and the menu bar is now permanently
            // hidden. Re-register every command on the window itself so
            // Ctrl+N/O/S/Z/F/… keep working exactly as before. (Adding one
            // QAction to several widgets is normal Qt and still fires once.)
            register_shortcuts_on_window(main_menu);
        }

        setMenuWidget(header_host);   // carries the (hidden) menu bar
    }
