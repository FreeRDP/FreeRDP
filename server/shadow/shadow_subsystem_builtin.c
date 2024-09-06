/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2016 Jiang Zihao <zihao.jiang@yahoo.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <freerdp/server/shadow.h>

typedef struct
{
	const char* (*name)(void);
	pfnShadowSubsystemEntry entry;
} RDP_SHADOW_SUBSYSTEM;

extern int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
extern const char* ShadowSubsystemName(void);

static RDP_SHADOW_SUBSYSTEM g_Subsystems[] = {

	{ ShadowSubsystemName, ShadowSubsystemEntry }
};

static size_t g_SubsystemCount = ARRAYSIZE(g_Subsystems);

static pfnShadowSubsystemEntry shadow_subsystem_load_static_entry(const char* name)
{
	if (!name)
	{
		if (g_SubsystemCount > 0)
		{
			const RDP_SHADOW_SUBSYSTEM* cur = &g_Subsystems[0];
			WINPR_ASSERT(cur->entry);

			return cur->entry;
		}

		return NULL;
	}

	for (size_t index = 0; index < g_SubsystemCount; index++)
	{
		const RDP_SHADOW_SUBSYSTEM* cur = &g_Subsystems[index];
		WINPR_ASSERT(cur->name);
		WINPR_ASSERT(cur->entry);

		if (strcmp(name, cur->name()) == 0)
			return cur->entry;
	}

	return NULL;
}

void shadow_subsystem_set_entry_builtin(const char* name)
{
	pfnShadowSubsystemEntry entry = shadow_subsystem_load_static_entry(name);

	if (entry)
		shadow_subsystem_set_entry(entry);
}
