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

#include "switcher.hpp"

#include <plugin-support.h>

#include <QStringList>

#include <cstring>
#include <string>
#include <utility>

/* Pause between the old stream stopping and the new one starting, so the ingest server closes the old session. */
static constexpr int kRestartDelayMs = 1000;
static constexpr int kStopTimeoutMs = 60000;
static constexpr int kStartTimeoutMs = 30000;

static constexpr const char *kNextHotkeyName = "KeyVault.Next";

static std::string SlotHotkeyName(int index)
{
	return "KeyVault.Slot." + std::to_string(index + 1);
}

static QString ServiceKey()
{
	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service)
		return QString();

	obs_data_t *settings = obs_service_get_settings(service);
	QString key = QString::fromUtf8(obs_data_get_string(settings, "key")).trimmed();
	obs_data_release(settings);
	return key;
}

Switcher::Switcher(QObject *parent) : QObject(parent)
{
	watchdog_.setSingleShot(true);
	connect(&watchdog_, &QTimer::timeout, this, [this]() {
		obs_log(LOG_WARNING, "switch timed out in state %d", static_cast<int>(state_));
		pending_ = -1;
		finishWithMessage(KvText("KeyVault.Error.Timeout"));
	});
}

Switcher::~Switcher()
{
	obs_data_release(hotkeyData_);
}

int Switcher::nextIndex() const
{
	int index = current_ < 0 ? 0 : current_ + 1;
	return index < static_cast<int>(entries_.size()) ? index : -1;
}

bool Switcher::isLive() const
{
	return obs_frontend_streaming_active();
}

QString Switcher::statusText() const
{
	QStringList lines;

	switch (state_) {
	case State::Stopping:
		lines << KvText("KeyVault.Status.Stopping").arg(entryName(pending_));
		break;
	case State::Starting:
		lines << KvText("KeyVault.Status.Starting").arg(entryName(pending_));
		break;
	case State::Idle:
		if (!isLive())
			lines << KvText("KeyVault.Status.NotLive");
		else if (current_ >= 0)
			lines << KvText("KeyVault.Status.Live").arg(entryName(current_));
		else
			lines << KvText("KeyVault.Status.LiveUnknown");
		break;
	}

	if (!message_.isEmpty())
		lines << message_;

	return lines.join('\n');
}

void Switcher::addEntry()
{
	if (state_ != State::Idle)
		return;

	int number = static_cast<int>(entries_.size()) + 1;
	entries_.push_back({KvText("KeyVault.DefaultName").arg(number), QString()});
	save();
	emit entriesChanged();
}

void Switcher::removeEntry(int index)
{
	if (state_ != State::Idle || index < 0 || index >= static_cast<int>(entries_.size()))
		return;

	entries_.erase(entries_.begin() + index);
	/* Keep "next" pointing at the row that took the removed one's place. */
	if (index <= current_)
		current_--;

	save();
	emit entriesChanged();
}

void Switcher::moveEntry(int from, int to)
{
	int count = static_cast<int>(entries_.size());
	if (state_ != State::Idle || from < 0 || from >= count || to < 0 || to >= count || from == to)
		return;

	std::swap(entries_[from], entries_[to]);
	if (current_ == from)
		current_ = to;
	else if (current_ == to)
		current_ = from;

	save();
	emit entriesChanged();
}

void Switcher::updateEntry(int index, const QString &name, const QString &key)
{
	if (index < 0 || index >= static_cast<int>(entries_.size()))
		return;

	entries_[index].name = name.trimmed();
	entries_[index].key = key.trimmed();
	save();
	emit stateChanged();
}

void Switcher::next()
{
	if (state_ != State::Idle) {
		message_ = KvText("KeyVault.Error.Busy");
		emit stateChanged();
		return;
	}

	if (entries_.empty()) {
		finishWithMessage(KvText("KeyVault.Error.NoEntries"));
		return;
	}

	int index = nextIndex();
	if (index < 0) {
		finishWithMessage(KvText("KeyVault.Last"));
		return;
	}

	goTo(index);
}

void Switcher::goTo(int index)
{
	if (state_ != State::Idle) {
		message_ = KvText("KeyVault.Error.Busy");
		emit stateChanged();
		return;
	}

	if (index < 0 || index >= static_cast<int>(entries_.size()))
		return;

	if (entries_[index].key.isEmpty()) {
		finishWithMessage(KvText("KeyVault.Error.EmptyKey").arg(entryName(index)));
		return;
	}

	pending_ = index;

	if (!isLive()) {
		startPending();
		return;
	}

	if (findEntryByServiceKey() == index) {
		current_ = index;
		pending_ = -1;
		finishWithMessage(KvText("KeyVault.AlreadyLive").arg(entryName(index)));
		return;
	}

	obs_log(LOG_INFO, "stopping stream to switch to '%s'", entryName(index).toUtf8().constData());
	message_.clear();
	setState(State::Stopping);
	watchdog_.start(kStopTimeoutMs);
	obs_frontend_streaming_stop();
}

void Switcher::startPending()
{
	if (pending_ < 0 || pending_ >= static_cast<int>(entries_.size())) {
		pending_ = -1;
		finishWithMessage(KvText("KeyVault.Error.EntryGone"));
		return;
	}

	if (!applyKey(pending_)) {
		pending_ = -1;
		setState(State::Idle);
		return;
	}

	obs_log(LOG_INFO, "starting stream with '%s'", entryName(pending_).toUtf8().constData());
	message_.clear();
	setState(State::Starting);
	watchdog_.start(kStartTimeoutMs);
	obs_frontend_streaming_start();
}

bool Switcher::applyKey(int index)
{
	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service) {
		message_ = KvText("KeyVault.Error.NoService");
		return false;
	}

	const char *id = obs_service_get_id(service);
	if (!id || (strcmp(id, "rtmp_custom") != 0 && strcmp(id, "rtmp_common") != 0)) {
		message_ = KvText("KeyVault.Error.Service").arg(QString::fromUtf8(id ? id : ""));
		return false;
	}

	obs_data_t *settings = obs_service_get_settings(service);
	obs_data_set_string(settings, "key", entries_[index].key.toUtf8().constData());
	obs_service_update(service, settings);
	obs_data_release(settings);
	obs_frontend_save_streaming_service();
	return true;
}

void Switcher::setState(State state)
{
	state_ = state;
	emit stateChanged();
}

void Switcher::finishWithMessage(const QString &message)
{
	watchdog_.stop();
	message_ = message;
	setState(State::Idle);
}

int Switcher::findEntryByServiceKey() const
{
	QString key = ServiceKey();
	if (key.isEmpty())
		return -1;

	if (current_ >= 0 && current_ < static_cast<int>(entries_.size()) && entries_[current_].key == key)
		return current_;

	for (size_t i = 0; i < entries_.size(); i++) {
		if (entries_[i].key == key)
			return static_cast<int>(i);
	}
	return -1;
}

QString Switcher::entryName(int index) const
{
	if (index < 0 || index >= static_cast<int>(entries_.size()))
		return QString();

	const QString &name = entries_[index].name;
	return name.isEmpty() ? QStringLiteral("#%1").arg(index + 1) : name;
}

void Switcher::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		watchdog_.stop();
		current_ = findEntryByServiceKey();
		pending_ = -1;
		setState(State::Idle);
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		if (state_ == State::Stopping) {
			watchdog_.stop();
			QTimer::singleShot(kRestartDelayMs, this, &Switcher::startPending);
		} else if (state_ == State::Starting) {
			QString name = entryName(pending_);
			pending_ = -1;
			finishWithMessage(KvText("KeyVault.Error.StartFailed").arg(name));
		} else {
			emit stateChanged();
		}
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		emit stateChanged();
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		save();
		unregisterHotkeys();
		break;

	default:
		break;
	}
}

void Switcher::load()
{
	obs_data_t *data = LoadVaultData();
	if (!data)
		return;

	entries_ = EntriesFromData(data);

	obs_data_release(hotkeyData_);
	hotkeyData_ = obs_data_get_obj(data, "hotkeys");
	loadHotkeys();

	obs_data_release(data);
	emit entriesChanged();
}

void Switcher::save()
{
	storeHotkeys();

	obs_data_t *data = obs_data_create();
	EntriesToData(entries_, data);
	if (hotkeyData_)
		obs_data_set_obj(data, "hotkeys", hotkeyData_);

	SaveVaultData(data);
	obs_data_release(data);
}

void Switcher::registerHotkeys()
{
	nextHotkey_ = obs_hotkey_register_frontend(kNextHotkeyName, obs_module_text("KeyVault.Hotkey.Next"),
						   HotkeyNext, this);

	for (int i = 0; i < kSlotHotkeys; i++) {
		SlotHotkey &slot = slotHotkeys_[i];
		slot.owner = this;
		slot.index = i;

		std::string name = SlotHotkeyName(i);
		QByteArray description = KvText("KeyVault.Hotkey.Slot").arg(i + 1).toUtf8();
		slot.id = obs_hotkey_register_frontend(name.c_str(), description.constData(), HotkeySlot, &slot);
	}

	loadHotkeys();
}

void Switcher::unregisterHotkeys()
{
	storeHotkeys();

	if (nextHotkey_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(nextHotkey_);
		nextHotkey_ = OBS_INVALID_HOTKEY_ID;
	}

	for (SlotHotkey &slot : slotHotkeys_) {
		if (slot.id != OBS_INVALID_HOTKEY_ID) {
			obs_hotkey_unregister(slot.id);
			slot.id = OBS_INVALID_HOTKEY_ID;
		}
	}
}

void Switcher::storeHotkeys()
{
	if (nextHotkey_ == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_t *hotkeys = obs_data_create();

	obs_data_array_t *array = obs_hotkey_save(nextHotkey_);
	obs_data_set_array(hotkeys, kNextHotkeyName, array);
	obs_data_array_release(array);

	for (int i = 0; i < kSlotHotkeys; i++) {
		array = obs_hotkey_save(slotHotkeys_[i].id);
		obs_data_set_array(hotkeys, SlotHotkeyName(i).c_str(), array);
		obs_data_array_release(array);
	}

	obs_data_release(hotkeyData_);
	hotkeyData_ = hotkeys;
}

void Switcher::loadHotkeys()
{
	if (!hotkeyData_ || nextHotkey_ == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_array_t *array = obs_data_get_array(hotkeyData_, kNextHotkeyName);
	if (array) {
		obs_hotkey_load(nextHotkey_, array);
		obs_data_array_release(array);
	}

	for (int i = 0; i < kSlotHotkeys; i++) {
		array = obs_data_get_array(hotkeyData_, SlotHotkeyName(i).c_str());
		if (array) {
			obs_hotkey_load(slotHotkeys_[i].id, array);
			obs_data_array_release(array);
		}
	}
}

/* Hotkeys fire on the OBS hotkey thread; hop to the UI thread before touching anything. */
void Switcher::HotkeyNext(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;

	auto *self = static_cast<Switcher *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->next(); }, Qt::QueuedConnection);
}

void Switcher::HotkeySlot(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;

	auto *slot = static_cast<SlotHotkey *>(data);
	Switcher *self = slot->owner;
	int index = slot->index;
	QMetaObject::invokeMethod(self, [self, index]() { self->goTo(index); }, Qt::QueuedConnection);
}
