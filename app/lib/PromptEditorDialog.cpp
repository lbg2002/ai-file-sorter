#include "PromptEditorDialog.hpp"

#include "PromptTemplateStore.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

PromptEditorDialog::PromptEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI Prompt Editor"));
    resize(760, 560);

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("Edit the system prompt used for AI categorization. The original project prompt remains active "
           "until the override is enabled. Supported variables: {{filename}}, {{path}}, {{item_type}}, "
           "{{context}}, {{output_format}}."), this);
    description->setWordWrap(true);
    layout->addWidget(description);

    enabled_checkbox_ = new QCheckBox(tr("Enable custom prompt override"), this);
    enabled_checkbox_->setChecked(PromptTemplateStore::enabled());
    layout->addWidget(enabled_checkbox_);

    editor_ = new QPlainTextEdit(this);
    editor_->setPlainText(QString::fromStdString(PromptTemplateStore::prompt_template()));
    editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(editor_, 1);

    status_label_ = new QLabel(this);
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto* restore = buttons->addButton(tr("Restore default"), QDialogButtonBox::ResetRole);
    auto* preview = buttons->addButton(tr("Preview rendered prompt"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save_prompt(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(restore, &QPushButton::clicked, this, [this]() { restore_default(); });
    connect(preview, &QPushButton::clicked, this, [this]() { show_preview(); });
}

void PromptEditorDialog::save_prompt()
{
    PromptTemplateStore::set_enabled(enabled_checkbox_->isChecked());
    PromptTemplateStore::set_prompt_template(editor_->toPlainText().toStdString());
    if (!PromptTemplateStore::save()) {
        status_label_->setText(tr("Could not save the prompt override."));
        return;
    }
    accept();
}

void PromptEditorDialog::restore_default()
{
    editor_->setPlainText(QString::fromStdString(PromptTemplateStore::default_template()));
}

void PromptEditorDialog::show_preview()
{
    const std::string current = editor_->toPlainText().toStdString();
    const bool was_enabled = PromptTemplateStore::enabled();
    const std::string previous = PromptTemplateStore::prompt_template();

    PromptTemplateStore::set_enabled(true);
    PromptTemplateStore::set_prompt_template(current);
    const std::string rendered = PromptTemplateStore::render_or_default(
        "", "example_report.docx", "/home/user/Desktop/example_report.docx",
        FileType::File, "Allowed main categories: Research, Meetings, Development");

    PromptTemplateStore::set_prompt_template(previous);
    PromptTemplateStore::set_enabled(was_enabled);

    QMessageBox::information(this, tr("Rendered prompt preview"), QString::fromStdString(rendered));
}
