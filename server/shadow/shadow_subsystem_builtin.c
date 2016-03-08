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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/server/shadow.h>

struct _RDP_SHADOW_SUBSYSTEM
{
	const char* name;
	pfnShadowSubsystemEntry entry;
};
typedef struct _RDP_SHADOW_SUBSYSTEM RDP_SHADOW_SUBSYSTEM;

#ifdef WITH_SHADOW_X11
extern int X11_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif

#ifdef WITH_SHADOW_MAC
extern int Mac_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif

#ifdef WITH_SHADOW_WIN
extern int Win_ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
#endif

static RDP_SHADOW_SUBSYSTEM g_Subsystems[] =
{

#ifdef WITH_SHADOW_X11
	{ "X11", X11_ShadowSubsystemEntry },
#endif

#ifdef WITH_SHADOW_MAC
	{ "Mac", Mac_ShadowSubsystemEntry },
#endif

#ifdef WITH_SHADOW_WIN
	{ "Win", Win_ShadowSubsystemEntry },
#endif

	{ "", NULL }
};

static int g_SubsystemCount = (sizeof(g_Subsystems) / sizeof(g_Subsystems[0]));

static pfnShadowSubsystemEntry shadow_subsystem_load_static_entry(const char* name)
{
	int index;

	if (!name)
	{
		for (index = 0; index < g_SubsystemCount; index++)
		{
			if (g_Subsystems[index].name)
				return g_Subsystems[index].entry;
		}
	}

	for (index = 0; index < g_SubsystemCount; index++)
	{
		if (strcmp(name, g_Subsystems[index].name) == 0)
			return g_Subsystems[index].entry;
	}

	return NULL;
}

void shadow_subsystem_set_entry_builtin(const char* name)
{
	pfnShadowSubsystemEntry entry;

	entry = shadow_subsystem_load_static_entry(name);

	if (entry)
		shadow_subsystem_set_entry(entry);

	return;
}
