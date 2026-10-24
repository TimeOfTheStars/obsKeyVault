/*
OBS Key Vault
Copyright (C) 2026 Alexey <arcadnick228@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "key-vault-dock.hpp"
#include "switcher.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

enum Column { ColGo = 0, ColName, ColKey, ColCount };

/* Shows stream keys as "••••••••ab12" unless "Show keys" is checked. Editing always shows the real key. */
class KeyDelegate : public QStyledItemDelegate {
public:
	KeyDelegate(QCheckBox *showKeys, QObject *parent) : QStyledItemDelegate(parent), showKeys_(showKeys) {}

	QString displayText(const QVariant &value, const QLocale &locale) const override
	{
		QString key = value.toString();
		if (key.isEmpty())
			return KvText("KeyVault.KeyPlaceholder");
		if (showKeys_->isChecked())
			return QStyledItemDelegate::displayText(value, locale);

		QString bullets(8, QChar(0x2022));
		return key.size() > 4 ? bullets + key.right(4) : bullets;
	}

private:
	QCheckBox *showKeys_;
};

KeyVaultDock::KeyVaultDock(Switcher *switcher, QWidget *parent) : QWidget(parent), switcher_(switcher)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	nextButton_ = new QPushButton(this);
	nextButton_->setMinimumHeight(40);
	QFont nextFont = nextButton_->font();
	nextFont.setBold(true);
	nextButton_->setFont(nextFont);
	nextButton_->setToolTip(KvText("KeyVault.Next.Tooltip"));

	statusLabel_ = new QLabel(this);
	statusLabel_->setWordWrap(true);
	statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

	showKeys_ = new QCheckBox(KvText("KeyVault.ShowKeys"), this);

	table_ = new QTableWidget(0, ColCount, this);
	table_->setHorizontalHeaderLabels({QString(), KvText("KeyVault.Column.Name"), KvText("KeyVault.Column.Key")});
	table_->horizontalHeader()->setSectionResizeMode(ColGo, QHeaderView::ResizeToContents);
	table_->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(ColKey, QHeaderView::Stretch);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
				QAbstractItemView::SelectedClicked);
	table_->setItemDelegateForColumn(ColKey, new KeyDelegate(showKeys_, table_));

	addButton_ = new QPushButton(QStringLiteral("+"), this);
	addButton_->setToolTip(KvText("KeyVault.Add"));
	removeButton_ = new QPushButton(QString(QChar(0x2212)), this);
	removeButton_->setToolTip(KvText("KeyVault.Remove"));
	upButton_ = new QPushButton(QString(QChar(0x2191)), this);
	upButton_->setToolTip(KvText("KeyVault.MoveUp"));
	downButton_ = new QPushButton(QString(QChar(0x2193)), this);
	downButton_->setToolTip(KvText("KeyVault.MoveDown"));

	auto *toolbar = new QHBoxLayout();
	for (QPushButton *button : {addButton_, removeButton_, upButton_, downButton_}) {
		button->setFixedWidth(32);
		toolbar->addWidget(button);
	}
	toolbar->addStretch();
	toolbar->addWidget(showKeys_);

	layout->addWidget(nextButton_);
	layout->addWidget(statusLabel_);
	layout->addWidget(table_, 1);
	layout->addLayout(toolbar);

	connect(nextButton_, &QPushButton::clicked, switcher, &Switcher::next);
	connect(table_, &QTableWidget::itemChanged, this, &KeyVaultDock::onItemChanged);
	connect(table_, &QTableWidget::itemSelectionChanged, this, &KeyVaultDock::refreshState);
	connect(showKeys_, &QCheckBox::toggled, this, [this]() { table_->viewport()->update(); });
	connect(addButton_, &QPushButton::clicked, this, [this]() {
		switcher_->addEntry();
		int row = table_->rowCount() - 1;
		if (row >= 0) {
			table_->selectRow(row);
			table_->editItem(table_->item(row, ColName));
		}
	});
	connect(removeButton_, &QPushButton::clicked, this, &KeyVaultDock::removeSelected);
	connect(upButton_, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
	connect(downButton_, &QPushButton::clicked, this, [this]() { moveSelected(1); });
	connect(switcher, &Switcher::entriesChanged, this, &KeyVaultDock::rebuildTable);
	connect(switcher, &Switcher::stateChanged, this, &KeyVaultDock::refreshState);

	rebuildTable();
}

void KeyVaultDock::rebuildTable()
{
	if (!switcher_)
		return;

	bool wasUpdating = updating_;
	updating_ = true;

	const std::vector<KeyEntry> &entries = switcher_->entries();
	int count = static_cast<int>(entries.size());
	table_->setRowCount(count);

	for (int row = 0; row < count; row++) {
		auto *go = new QPushButton(QString(QChar(0x25B6)));
		go->setToolTip(KvText("KeyVault.GoLive"));
		connect(go, &QPushButton::clicked, this, [this, row]() {
			if (switcher_)
				switcher_->goTo(row);
		});
		table_->setCellWidget(row, ColGo, go);
		table_->setItem(row, ColName, new QTableWidgetItem(entries[row].name));
		table_->setItem(row, ColKey, new QTableWidgetItem(entries[row].key));
	}

	updating_ = wasUpdating;
	refreshState();
}

void KeyVaultDock::refreshState()
{
	if (!switcher_)
		return;

	bool wasUpdating = updating_;
	updating_ = true;

	bool idle = switcher_->state() == Switcher::State::Idle;
	bool live = switcher_->isLive();
	int current = switcher_->current();
	int next = switcher_->nextIndex();
	int count = table_->rowCount();

	/* Highlight the row that is (or was last) on air. Only touch items that change, so an open editor is not reset. */
	QVariant liveBackground =
		QVariant::fromValue(QBrush(live ? QColor(198, 40, 40, 110) : QColor(128, 128, 128, 70)));
	for (int row = 0; row < count; row++) {
		bool isCurrent = row == current;
		for (int col : {ColName, ColKey}) {
			QTableWidgetItem *item = table_->item(row, col);
			if (!item)
				continue;

			QVariant background = isCurrent ? liveBackground : QVariant();
			if (item->data(Qt::BackgroundRole) != background)
				item->setData(Qt::BackgroundRole, background);

			if (item->font().bold() != isCurrent) {
				QFont font = item->font();
				font.setBold(isCurrent);
				item->setFont(font);
			}
		}

		if (QWidget *go = table_->cellWidget(row, ColGo))
			go->setEnabled(idle);
	}

	if (next >= 0) {
		const char *label = live ? "KeyVault.NextWithName" : "KeyVault.StartWithName";
		nextButton_->setText(KvText(label).arg(switcher_->entryName(next)));
	} else {
		nextButton_->setText(KvText("KeyVault.Next"));
	}
	nextButton_->setEnabled(idle && next >= 0);

	statusLabel_->setText(switcher_->statusText());

	int selected = selectedRow();
	addButton_->setEnabled(idle);
	removeButton_->setEnabled(idle && selected >= 0);
	upButton_->setEnabled(idle && selected > 0);
	downButton_->setEnabled(idle && selected >= 0 && selected < count - 1);

	updating_ = wasUpdating;
}

void KeyVaultDock::onItemChanged(QTableWidgetItem *item)
{
	if (updating_ || !switcher_)
		return;

	int row = item->row();
	QTableWidgetItem *name = table_->item(row, ColName);
	QTableWidgetItem *key = table_->item(row, ColKey);
	if (!name || !key)
		return;

	switcher_->updateEntry(row, name->text(), key->text());

	/* Show the trimmed values that were actually stored. */
	const KeyEntry &entry = switcher_->entries()[row];
	updating_ = true;
	if (name->text() != entry.name)
		name->setText(entry.name);
	if (key->text() != entry.key)
		key->setText(entry.key);
	updating_ = false;
}

void KeyVaultDock::removeSelected()
{
	int row = selectedRow();
	if (row < 0 || !switcher_)
		return;

	auto answer = QMessageBox::question(this, KvText("KeyVault.ConfirmRemove.Title"),
					    KvText("KeyVault.ConfirmRemove.Text").arg(switcher_->entryName(row)));
	if (answer != QMessageBox::Yes)
		return;

	switcher_->removeEntry(row);
	int count = table_->rowCount();
	if (count > 0)
		table_->selectRow(row < count ? row : count - 1);
}

void KeyVaultDock::moveSelected(int delta)
{
	int row = selectedRow();
	int target = row + delta;
	if (row < 0 || target < 0 || target >= table_->rowCount() || !switcher_)
		return;

	switcher_->moveEntry(row, target);
	table_->selectRow(target);
}

int KeyVaultDock::selectedRow() const
{
	QModelIndexList rows = table_->selectionModel()->selectedRows();
	return rows.isEmpty() ? -1 : rows.first().row();
}
