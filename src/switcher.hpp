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

#include "storage.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <vector>

inline QString KvText(const char *lookup)
{
	return QString::fromUtf8(obs_module_text(lookup));
}

/*
 * Owns the list of stream keys and switches the stream between them:
 * stop the stream -> put the next key into the stream service -> start again.
 */
class Switcher : public QObject {
	Q_OBJECT

public:
	enum class State { Idle, Stopping, Starting };

	static constexpr int kSlotHotkeys = 5;

	explicit Switcher(QObject *parent = nullptr);
	~Switcher() override;

	const std::vector<KeyEntry> &entries() const { return entries_; }
	int current() const { return current_; }
	int nextIndex() const;
	State state() const { return state_; }
	bool isLive() const;
	QString statusText() const;
	QString entryName(int index) const;

	void addEntry();
	void removeEntry(int index);
	void moveEntry(int from, int to);
	void updateEntry(int index, const QString &name, const QString &key);

	void goTo(int index);
	void next();

	void load();
	void save();
	void registerHotkeys();
	void unregisterHotkeys();
	void handleFrontendEvent(enum obs_frontend_event event);

signals:
	/* The list itself changed (rows added, removed or moved). */
	void entriesChanged();
	/* Live/next row, status text or busy state changed. */
	void stateChanged();

private:
	struct SlotHotkey {
		Switcher *owner = nullptr;
		int index = 0;
		obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
	};

	bool applyKey(int index);
	void startPending();
	void setState(State state);
	void finishWithMessage(const QString &message);
	int findEntryByServiceKey() const;
	void storeHotkeys();
	void loadHotkeys();

	static void HotkeyNext(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void HotkeySlot(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	std::vector<KeyEntry> entries_;
	int current_ = -1;
	int pending_ = -1;
	State state_ = State::Idle;
	QString message_;
	QTimer watchdog_;

	obs_hotkey_id nextHotkey_ = OBS_INVALID_HOTKEY_ID;
	std::array<SlotHotkey, kSlotHotkeys> slotHotkeys_;
	/* Last known hotkey bindings, kept so they survive saves after the hotkeys are unregistered. */
	obs_data_t *hotkeyData_ = nullptr;
};
