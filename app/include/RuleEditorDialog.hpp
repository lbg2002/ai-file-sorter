#pragma once

#include <QDialog>

class QTableWidget;
class QLabel;

class RuleEditorDialog : public QDialog {
public:
    explicit RuleEditorDialog(QWidget* parent = nullptr);

private:
    void add_rule();
    void remove_selected_rule();
    void save_rules();
    void load_rules();

    QTableWidget* table_{nullptr};
    QLabel* status_label_{nullptr};
};
