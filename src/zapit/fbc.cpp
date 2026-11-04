/*
 * Copyright (C) 2025 TangoCash
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "fbc.h"

#include <stdio.h>

#include "debug.h"

#define FBC_PROC_PATH "/proc/stb/frontend/%d/%s"

CFBCTunerManager *CFBCTunerManager::manager = NULL;

CFBCTunerManager::CFBCTunerManager()
{
	have_fbc = false;
}

CFBCTunerManager::~CFBCTunerManager()
{
	if (manager == this)
		manager = NULL;
}

CFBCTunerManager *CFBCTunerManager::getInstance()
{
	if (manager == NULL)
		manager = new CFBCTunerManager();
	return manager;
}

/* What the box says about a slot, or -1 where it says nothing, which is every
 * slot of a box without FBC tuners. */
int CFBCTunerManager::ReadProcInt(int slot, const char *entry) const
{
	char path[128];
	FILE *f;
	int value = -1;

	snprintf(path, sizeof(path), FBC_PROC_PATH, slot, entry);
	f = fopen(path, "r");
	if (f == NULL)
		return -1;
	if (fscanf(f, "%d", &value) != 1)
		value = -1;
	fclose(f);
	return value;
}

bool CFBCTunerManager::WriteProcInt(int slot, const char *entry, int value) const
{
	char path[128];
	FILE *f;

	snprintf(path, sizeof(path), FBC_PROC_PATH, slot, entry);
	f = fopen(path, "w");
	if (f == NULL)
		return false;
	fprintf(f, "%d", value);
	fclose(f);
	return true;
}

/* The positions of a set that may carry an input of their own, one bit each.
 * The box writes them together, as "12" or "1 2". */
int CFBCTunerManager::LoadConnectChoices(int slot) const
{
	char path[128];
	char line[128];
	FILE *f;
	int choices = 0;

	snprintf(path, sizeof(path), FBC_PROC_PATH, slot, "fbc_connect_choices");
	f = fopen(path, "r");
	if (f == NULL)
		return 0;
	line[0] = 0;
	if (fgets(line, sizeof(line), f) == NULL)
		line[0] = 0;
	fclose(f);

	for (char *p = line; *p != 0; p++)
		if (*p >= '0' && *p <= '9')
			choices |= (1 << (*p - '0'));
	return choices;
}

/* A slot is an FBC tuner where the box names the set it belongs to. The
 * position within that set is counted by the slots of the set already known,
 * which is the order the caller walks them in: enigma2 counts them the same
 * way, one set after another. */
bool CFBCTunerManager::Init(int slot, bool dvbs)
{
	fbc_tuner_t tuner;
	fbc_tuner_map_const_iterator_t it;
	int set_id, choices, position = 0;

	/* a DVB-C or DVB-T FBC tuner needs none of this */
	if (!dvbs)
		return false;

	set_id = ReadProcInt(slot, "fbc_set_id");
	if (set_id < 0)
		return false;

	for (it = tuners.begin(); it != tuners.end(); ++it)
		if (it->second.set_id == set_id)
			position++;

	/* the positions that may be the top of a set are the same for the whole
	 * set, and the box names them on each of its slots */
	choices = LoadConnectChoices(slot);

	tuner.set_id = set_id;
	tuner.position = position;
	tuner.is_root = ((choices & (1 << position)) != 0);
	/* a link starts tied to the first position of its set, which is what the
	 * box is told before anything is recording */
	tuner.connected_to = tuner.is_root ? position : 0;
	tuners[slot] = tuner;
	have_fbc = true;

	WriteProcInt(slot, "fbc_connect", tuner.connected_to);
	WriteProcInt(slot, "fbc_link", 0);

	INFO("FBC: slot %d in set %d, position %d, root %s, connected to %d",
			slot, set_id, position, tuner.is_root ? "yes" : "no",
			tuner.connected_to);
	return true;
}

bool CFBCTunerManager::IsFBCTuner(int slot) const
{
	return tuners.find(slot) != tuners.end();
}

bool CFBCTunerManager::IsRoot(int slot) const
{
	fbc_tuner_map_const_iterator_t it = tuners.find(slot);
	return (it != tuners.end()) && it->second.is_root;
}

bool CFBCTunerManager::IsFBCLink(int slot) const
{
	return IsFBCTuner(slot) && !IsRoot(slot);
}

bool CFBCTunerManager::IsSameSet(int slot_a, int slot_b) const
{
	fbc_tuner_map_const_iterator_t a = tuners.find(slot_a);
	fbc_tuner_map_const_iterator_t b = tuners.find(slot_b);

	if (a == tuners.end() || b == tuners.end())
		return false;
	return a->second.set_id == b->second.set_id;
}

int CFBCTunerManager::GetSetID(int slot) const
{
	fbc_tuner_map_const_iterator_t it = tuners.find(slot);
	return (it == tuners.end()) ? -1 : it->second.set_id;
}

int CFBCTunerManager::GetPosition(int slot) const
{
	fbc_tuner_map_const_iterator_t it = tuners.find(slot);
	return (it == tuners.end()) ? -1 : it->second.position;
}

int CFBCTunerManager::GetConnectedTo(int slot) const
{
	fbc_tuner_map_const_iterator_t it = tuners.find(slot);
	return (it == tuners.end()) ? -1 : it->second.connected_to;
}

/* The root of the set a slot is in, which is the slot its links are tied to.
 * Where a set has more than one root the first one is taken: the map is
 * ordered by slot and the positions follow that order, so the first root met
 * is the lowest position of the set. */
int CFBCTunerManager::GetRootOfSet(int slot) const
{
	fbc_tuner_map_const_iterator_t it;
	int set_id = GetSetID(slot);

	if (set_id < 0)
		return -1;

	for (it = tuners.begin(); it != tuners.end(); ++it) {
		if (it->second.set_id != set_id || !it->second.is_root)
			continue;
		return it->first;
	}
	return -1;
}

/* Ties a link tuner to a root of its own set, so that it tunes from the input
 * that root has. A root is tied to itself, and a slot that is not an FBC
 * tuner is left alone. */
bool CFBCTunerManager::Connect(int slot, int root_slot)
{
	fbc_tuner_map_iterator_t it = tuners.find(slot);
	fbc_tuner_map_const_iterator_t root;

	if (it == tuners.end())
		return false;

	if (it->second.is_root)
		return WriteProcInt(slot, "fbc_connect", it->second.position);

	root = tuners.find(root_slot);
	if (root == tuners.end() || !root->second.is_root ||
			root->second.set_id != it->second.set_id)
		return false;

	it->second.connected_to = root->second.position;
	WriteProcInt(slot, "fbc_connect", it->second.connected_to);
	WriteProcInt(slot, "fbc_link", 1);

	INFO("FBC: slot %d tied to root slot %d, position %d", slot, root_slot,
			root->second.position);
	return true;
}

/* Unties a link tuner, tying it back to the root of its set: it is still
 * listening to that input, and is not using it for anything. */
bool CFBCTunerManager::Disconnect(int slot)
{
	fbc_tuner_map_iterator_t it = tuners.find(slot);
	int root;

	if (it == tuners.end() || it->second.is_root)
		return false;

	root = GetRootOfSet(slot);
	if (root >= 0)
		it->second.connected_to = tuners.find(root)->second.position;

	WriteProcInt(slot, "fbc_connect", it->second.connected_to);
	WriteProcInt(slot, "fbc_link", 0);

	INFO("FBC: slot %d untied, back to %d", slot, it->second.connected_to);
	return true;
}

/* Ties a link to the root it tunes from. The master it is looped from is the
 * one to use where it is a root of the same set; where it is not - which is
 * what a box configured without the loop says - the first root of the set is
 * the input it tunes from. Answers the slot it was tied to. */
int CFBCTunerManager::ConnectToMaster(int slot, int master_slot)
{
	if (!IsFBCLink(slot))
		return -1;

	if (Connect(slot, master_slot))
		return master_slot;

	master_slot = GetRootOfSet(slot);
	if (master_slot >= 0 && Connect(slot, master_slot))
		return master_slot;

	INFO("FBC: no root to tie slot %d to", slot);
	return -1;
}

/* Every root tied to itself and every link tied back to the root of its set,
 * with none of them marked as linked: where the box is left when nothing is
 * recording, which is where enigma2 leaves it at start. */
void CFBCTunerManager::ConnectAll()
{
	fbc_tuner_map_const_iterator_t it;

	for (it = tuners.begin(); it != tuners.end(); ++it) {
		if (it->second.is_root) {
			WriteProcInt(it->first, "fbc_connect", it->second.position);
			WriteProcInt(it->first, "fbc_link", 0);
		} else {
			Disconnect(it->first);
		}
	}
}

void CFBCTunerManager::PrintLinks() const
{
	fbc_tuner_map_const_iterator_t it;

	for (it = tuners.begin(); it != tuners.end(); ++it)
		INFO("FBC: slot %d set %d position %d root %s connected to %d",
				it->first, it->second.set_id, it->second.position,
				it->second.is_root ? "yes" : "no",
				it->second.connected_to);
}

