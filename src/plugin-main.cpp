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

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include "key-vault-dock.hpp"
#include "switcher.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static constexpr const char *kDockId = "obs-key-vault-dock";

static Switcher *switcher = nullptr;

static void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	if (switcher)
		switcher->handleFrontendEvent(event);
}

static void OnSave(obs_data_t *, bool saving, void *)
{
	/* Fires after the user edits hotkeys in Settings, so their bindings are persisted too. */
	if (saving && switcher)
		switcher->save();
}

const char *obs_module_name(void)
{
	return "OBS Key Vault";
}

const char *obs_module_description(void)
{
	return obs_module_text("KeyVault.Description");
}

bool obs_module_load(void)
{
	switcher = new Switcher();
	switcher->registerHotkeys();
	switcher->load();

	auto *dock = new KeyVaultDock(switcher);
	if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("KeyVault.Title"), dock)) {
		obs_log(LOG_ERROR, "failed to add dock '%s'", kDockId);
		delete dock;
	}

	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
	obs_frontend_add_save_callback(OnSave, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	obs_frontend_remove_save_callback(OnSave, nullptr);

	delete switcher;
	switcher = nullptr;

	obs_log(LOG_INFO, "plugin unloaded");
}
