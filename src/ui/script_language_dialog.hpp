#pragma once
// Which languages the SCREENPLAY is written in — independent of the interface
// language. Spell check only flags a word when every selected language
// rejects it, so picking extra languages loosens checking rather than
// tightening it.

#include "app_palette.hpp"
#include "design_tokens.hpp"
#include "../config/ui_strings.hpp"

#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QStringList>
#include <vector>
#include <utility>

namespace screenplay::ui {

class ScriptLanguageDialog : public QDialog {
    Q_OBJECT
public:
    ScriptLanguageDialog(QWidget* parent, const QStringList& selected)
        : QDialog(parent) {
        using config::tr_ui;
        setWindowTitle(tr_ui("Script Language"));
        setModal(true);
        setMinimumWidth(kWidth);

        auto* column = new QVBoxLayout(this);
        column->setSpacing(Spacing::S + 2);
        column->setContentsMargins(Spacing::L, Spacing::L, Spacing::L, Spacing::L);

        auto* question = new QLabel(
            tr_ui("What language is your screenplay written in?"), this);
        question->setWordWrap(true);
        column->addWidget(question);
        column->addSpacing(Spacing::XS);

        for (const auto& [label, tag] : available_languages()) {
            auto* check = new QCheckBox(label, this);
            check->setChecked(selected.contains(tag));
            options_.emplace_back(check, tag);
            column->addWidget(check);
        }

        column->addSpacing(Spacing::XS);
        column->addWidget(build_note());
        column->addWidget(build_buttons());
    }

    QStringList selected_langs() const {
        QStringList chosen;
        for (const auto& [check, tag] : options_)
            if (check->isChecked()) chosen << tag;
        return chosen;
    }

private:
    static constexpr int kWidth = 320;

    static std::vector<std::pair<QString, QString>> available_languages() {
        return { { QString::fromUtf8("English"),            "en-US" },
                 { QString::fromUtf8("Español"),            "es"    },
                 { QString::fromUtf8("Français"),           "fr"    },
                 { QString::fromUtf8("Português (Brasil)"), "pt-BR" } };
    }

    QLabel* build_note() {
        auto* note = new QLabel(
            QString("<small style='color:%1'>%2</small>")
                .arg(MD3::hx(MD3::TextDim),
                     config::tr_ui("Select every language used in the screenplay. "
                                   "A word is only marked as misspelled when it is "
                                   "wrong in all of them.")),
            this);
        note->setWordWrap(true);
        return note;
    }

    QDialogButtonBox* build_buttons() {
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        return buttons;
    }

    std::vector<std::pair<QCheckBox*, QString>> options_;
};

} // namespace screenplay::ui
