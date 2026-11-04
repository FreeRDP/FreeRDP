/**
 * WinPR: Windows Portable Runtime
 * Renamed IANA time zones
 *
 * Copyright 2026 Island Technology, Inc.
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
#include <winpr/config.h>

#include <winpr/atexit.h>
#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/json.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include "../log.h"

#include <string.h>

#define TAG WINPR_TAG("timezone.aliases")

#include "TimeZoneNameMap.h"
#include "TimeZoneIanaAliases.h"

#if defined(WITH_TIMEZONE_COMPILED)
#include "TimeZoneIanaAliases_static.h"
#endif

typedef struct
{
	size_t count;
	size_t size;
	TimeZoneIanaAliasEntry* entries;
} TimeZoneIanaAliasContext;

static TimeZoneIanaAliasContext alias_context = WINPR_C_ARRAY_INIT;

static void alias_context_free(void)
{
	for (size_t x = 0; x < alias_context.count; x++)
	{
		free(alias_context.entries[x].Current);
		free(alias_context.entries[x].Legacy);
	}
	free(alias_context.entries);

	const TimeZoneIanaAliasContext empty = WINPR_C_ARRAY_INIT;
	alias_context = empty;
}

WINPR_ATTR_NODISCARD
static BOOL alias_reserve(TimeZoneIanaAliasContext* context, size_t size_to_add)
{
	WINPR_ASSERT(context);

	TimeZoneIanaAliasEntry* tmp =
	    realloc(context->entries, (context->size + size_to_add) * sizeof(TimeZoneIanaAliasEntry));
	if (!tmp)
	{
		WLog_WARN(TAG,
		          "Failed to reallocate TimeZoneIanaAliasEntry::entries to %" PRIuz " elements",
		          context->size + size_to_add);
		return FALSE;
	}
	context->entries = tmp;
	context->size += size_to_add;
	return TRUE;
}

/* takes ownership of current and legacy, source and pos name the entry in log messages */
WINPR_ATTR_NODISCARD
static BOOL alias_add(TimeZoneIanaAliasContext* context, const char* source, size_t pos,
                      char* current, char* legacy)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(source);
	WINPR_ASSERT(context->count < context->size);

	if (!current || !legacy)
	{
		WLog_WARN(TAG, "Invalid timezone alias at entry %" PRIuz " of %s", pos, source);
		free(current);
		free(legacy);
		return FALSE;
	}

	context->entries[context->count].Current = current;
	context->entries[context->count].Legacy = legacy;
	context->count++;
	return TRUE;
}

#if defined(WITH_TIMEZONE_FROM_FILE) && defined(WITH_WINPR_JSON)
static void load_aliases_from_file(TimeZoneIanaAliasContext* context)
{
	WINPR_JSON* json = nullptr;
	char* filename = GetCombinedPath(WINPR_RESOURCE_ROOT, "TimeZoneIanaAliases.json");
	if (!filename)
	{
		WLog_WARN(TAG, "Could not create WinPR timezone alias resource filename");
		goto end;
	}

	json = TimeZoneJsonParseFile(filename);
	if (!json)
		goto end;

	WINPR_JSON* obj = WINPR_JSON_IsObject(json)
	                      ? WINPR_JSON_GetObjectItemCaseSensitive(json, "TimeZoneIanaAliases")
	                      : nullptr;
	if (!WINPR_JSON_IsArray(obj))
	{
		WLog_WARN(TAG, "Invalid top level JSON type in file %s, expected an array", filename);
		goto end;
	}

	const size_t count = WINPR_JSON_GetArraySize(obj);
	if (!alias_reserve(context, count))
		goto end;
	for (size_t x = 0; x < count; x++)
	{
		WINPR_JSON* entry = WINPR_JSON_GetArrayItem(obj, x);
		if (!alias_add(context, filename, x, TimeZoneJsonGetString(entry, x, "Current"),
		               TimeZoneJsonGetString(entry, x, "Legacy")))
			goto end;
	}

end:
	free(filename);
	WINPR_JSON_Delete(json);
}
#endif

static BOOL CALLBACK load_aliases(PINIT_ONCE once, PVOID param, PVOID* pvcontext)
{
	TimeZoneIanaAliasContext* context = param;
	WINPR_ASSERT(context);
	WINPR_UNUSED(pvcontext);
	WINPR_UNUSED(once);

#if defined(WITH_TIMEZONE_FROM_FILE) && defined(WITH_WINPR_JSON)
	load_aliases_from_file(context);
#endif

#if defined(WITH_TIMEZONE_COMPILED)
	if (!alias_reserve(context, TimeZoneIanaAliasesSize))
	{
		alias_context_free();
		return FALSE;
	}
	for (size_t x = 0; x < TimeZoneIanaAliasesSize; x++)
	{
		if (!alias_add(context, "the compiled in table", x, _strdup(TimeZoneIanaAliases[x].Current),
		               _strdup(TimeZoneIanaAliases[x].Legacy)))
		{
			alias_context_free();
			return FALSE;
		}
	}
#endif

	(void)winpr_atexit(alias_context_free);
	return TRUE;
}

const char* TimeZoneIanaAlias(const char* iana)
{
	static INIT_ONCE init_guard = INIT_ONCE_STATIC_INIT;

	if (!iana)
		return nullptr;
	if (!InitOnceExecuteOnce(&init_guard, load_aliases, &alias_context, nullptr))
		return nullptr;

	for (size_t x = 0; x < alias_context.count; x++)
	{
		const TimeZoneIanaAliasEntry* alias = &alias_context.entries[x];
		if (strcmp(iana, alias->Current) == 0)
			return alias->Legacy;
		if (strcmp(iana, alias->Legacy) == 0)
			return alias->Current;
	}
	return nullptr;
}
