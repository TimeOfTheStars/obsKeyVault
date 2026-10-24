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

#include <obs.h>

#include <QString>

#include <vector>

struct KeyEntry {
	QString name;
	QString key;
};

/* Reads keys.json from the plugin config directory. Returns nullptr if the file does not exist yet. */
obs_data_t *LoadVaultData();

/* Writes keys.json atomically (keeps a .bak copy). */
bool SaveVaultData(obs_data_t *data);

std::vector<KeyEntry> EntriesFromData(obs_data_t *data);
void EntriesToData(const std::vector<KeyEntry> &entries, obs_data_t *data);
