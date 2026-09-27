#pragma once

#include <QDialog>

class QCheckBox;
class QPlainTextEdit;
class QLabel;

class PromptEditorDialog : public QDialog {
public:
    explicit PromptEditorDialog(QWidget* parent = nullptr);

private:
    void save_prompt();
    void restore_default();
    void show_preview();

    QCheckBox* enabled_checkbox_{nullptr};
    QPlainTextEdit* editor_{nullptr};
    QLabel* status_label_{nullptr};
};
