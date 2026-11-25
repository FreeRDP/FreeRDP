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
#include <freerdp/log.h>

#define TAG SERVER_TAG("shadow")

typedef struct
{
	const char* (*name)(void);
	pfnShadowSubsystemEntry entry;
} RDP_SHADOW_SUBSYSTEM;

#if defined(WITH_SHADOW_MUTTER)
extern int MutterShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
extern const char* MutterShadowSubsystemName(void);
#endif

#if defined(WITH_SHADOW_PLATFORM)
/* The platform's own: Win, Mac, X11 or Sample */
extern int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints);
extern const char* ShadowSubsystemName(void);
#endif

/* In the order they are tried. Mutter comes first: inside a GNOME session on
 * Wayland, X11 would only see Xwayland, and outside GNOME Mutter finds nothing. */
static const RDP_SHADOW_SUBSYSTEM g_Subsystems[] = {
#if defined(WITH_SHADOW_MUTTER)
	{ MutterShadowSubsystemName, MutterShadowSubsystemEntry },
#endif
#if defined(WITH_SHADOW_PLATFORM)
	{ ShadowSubsystemName, ShadowSubsystemEntry },
#endif
};

static const size_t g_SubsystemCount = ARRAYSIZE(g_Subsystems);

WINPR_ATTR_NODISCARD
static pfnShadowSubsystemEntry shadow_subsystem_load_static_entry(const char* name)
{
	WINPR_ASSERT(name);

	for (size_t index = 0; index < g_SubsystemCount; index++)
	{
		const RDP_SHADOW_SUBSYSTEM* cur = &g_Subsystems[index];
		WINPR_ASSERT(cur->name);
		WINPR_ASSERT(cur->entry);

		if (strcmp(name, cur->name()) == 0)
			return cur->entry;
	}

	return nullptr;
}

void shadow_subsystem_set_entry_builtin(const char* name)
{
	if (!name)
	{
		/* all of them: the server picks one when it starts (see /subsystem) */
		for (size_t index = 0; index < g_SubsystemCount; index++)
		{
			const RDP_SHADOW_SUBSYSTEM* cur = &g_Subsystems[index];
			if (!shadow_subsystem_add_entry(cur->name(), cur->entry))
				WLog_ERR(TAG, "cannot add the %s subsystem", cur->name());
		}
		return;
	}

	pfnShadowSubsystemEntry entry = shadow_subsystem_load_static_entry(name);

	if (entry)
		shadow_subsystem_set_entry(entry);
}
