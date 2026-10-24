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

#pragma once

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class Switcher;

class KeyVaultDock : public QWidget {
	Q_OBJECT

public:
	explicit KeyVaultDock(Switcher *switcher, QWidget *parent = nullptr);

private:
	void rebuildTable();
	void refreshState();
	void onItemChanged(QTableWidgetItem *item);
	void removeSelected();
	void moveSelected(int delta);
	int selectedRow() const;

	QPointer<Switcher> switcher_;
	QTableWidget *table_;
	QPushButton *addButton_;
	QPushButton *removeButton_;
	QPushButton *upButton_;
	QPushButton *downButton_;
	QCheckBox *showKeys_;
	QPushButton *nextButton_;
	QLabel *statusLabel_;
	bool updating_ = false;
};
