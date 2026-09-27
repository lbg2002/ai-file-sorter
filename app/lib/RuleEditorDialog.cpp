#include "RuleEditorDialog.hpp"

#include "RuleEngine.hpp"
#include "AppTheme.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

RuleEditorDialog::RuleEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("fileSortGuardUtilityDialog"));
    setStyleSheet(AppTheme::utility_dialog_style_sheet(palette()));
    setWindowTitle(tr("File Rules"));
    resize(880, 520);

    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(
        rule_editor_tr("Rules run top-to-bottom; the first match wins. Fields: extension, filename, path, size. "
           "Operators: equals, contains, regex, startswith, endswith, >, <. Example: extension / equals / .pdf / Documents / PDF."),
        this);
    help->setWordWrap(true);
    layout->addWidget(help);

    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({
        rule_editor_tr("Enabled"), rule_editor_tr("Field"), rule_editor_tr("Operator"), rule_editor_tr("Value"), rule_editor_tr("Category"), rule_editor_tr("Subcategory")
    });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_, 1);

    auto* row_actions = new QHBoxLayout();
    auto* add = new QPushButton(rule_editor_tr("Add rule"), this);
    auto* remove = new QPushButton(rule_editor_tr("Remove selected"), this);
    row_actions->addWidget(add);
    row_actions->addWidget(remove);
    row_actions->addStretch(1);
    layout->addLayout(row_actions);

    status_label_ = new QLabel(this);
    layout->addWidget(status_label_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(add, &QPushButton::clicked, this, [this]() { add_rule(); });
    connect(remove, &QPushButton::clicked, this, [this]() { remove_selected_rule(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save_rules(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    load_rules();
}

void RuleEditorDialog::add_rule()
{
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* enabled = new QTableWidgetItem;
    enabled->setFlags(enabled->flags() | Qt::ItemIsUserCheckable);
    enabled->setCheckState(Qt::Checked);
    table_->setItem(row, 0, enabled);
    table_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("extension")));
    table_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("equals")));
    table_->setItem(row, 3, new QTableWidgetItem(QStringLiteral(".pdf")));
    table_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("Documents")));
    table_->setItem(row, 5, new QTableWidgetItem(QStringLiteral("PDF")));
}

void RuleEditorDialog::remove_selected_rule()
{
    const int row = table_->currentRow();
    if (row >= 0) {
        table_->removeRow(row);
    }
}

void RuleEditorDialog::load_rules()
{
    const auto rules = RuleStore::load();
    for (const auto& rule : rules) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* enabled = new QTableWidgetItem;
        enabled->setFlags(enabled->flags() | Qt::ItemIsUserCheckable);
        enabled->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        table_->setItem(row, 0, enabled);
        table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(rule.field)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(rule.op)));
        table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(rule.value)));
        table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(rule.category)));
        table_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(rule.subcategory)));
    }
}

void RuleEditorDialog::save_rules()
{
    std::vector<FileRule> rules;
    for (int row = 0; row < table_->rowCount(); ++row) {
        FileRule rule;
        const auto text = [this, row](int column) {
            auto* item = table_->item(row, column);
            return item ? item->text().trimmed().toStdString() : std::string();
        };
        auto* enabled = table_->item(row, 0);
        rule.enabled = !enabled || enabled->checkState() == Qt::Checked;
        rule.field = text(1);
        rule.op = text(2);
        rule.value = text(3);
        rule.category = text(4);
        rule.subcategory = text(5);
        if (!rule.field.empty() && !rule.value.empty() && !rule.category.empty()) {
            rules.push_back(std::move(rule));
        }
    }

    if (!RuleStore::save(rules)) {
        status_label_->setText(rule_editor_tr("Could not save rules."));
        return;
    }
    accept();
}
