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

#include "storage.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

static constexpr const char *kVaultFile = "keys.json";

obs_data_t *LoadVaultData()
{
	char *path = obs_module_config_path(kVaultFile);
	if (!path)
		return nullptr;

	obs_data_t *data = nullptr;
	if (os_file_exists(path))
		data = obs_data_create_from_json_file_safe(path, "bak");

	bfree(path);
	return data;
}

bool SaveVaultData(obs_data_t *data)
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(kVaultFile);
	if (!path)
		return false;

	bool ok = obs_data_save_json_safe(data, path, "tmp", "bak");
	if (!ok)
		obs_log(LOG_WARNING, "failed to save %s", path);

	bfree(path);
	return ok;
}

std::vector<KeyEntry> EntriesFromData(obs_data_t *data)
{
	std::vector<KeyEntry> entries;
	obs_data_array_t *array = obs_data_get_array(data, "entries");
	if (!array)
		return entries;

	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		entries.push_back({QString::fromUtf8(obs_data_get_string(item, "name")),
				   QString::fromUtf8(obs_data_get_string(item, "key"))});
		obs_data_release(item);
	}

	obs_data_array_release(array);
	return entries;
}

void EntriesToData(const std::vector<KeyEntry> &entries, obs_data_t *data)
{
	obs_data_array_t *array = obs_data_array_create();
	for (const KeyEntry &entry : entries) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "name", entry.name.toUtf8().constData());
		obs_data_set_string(item, "key", entry.key.toUtf8().constData());
		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}

	obs_data_set_array(data, "entries", array);
	obs_data_array_release(array);
}
