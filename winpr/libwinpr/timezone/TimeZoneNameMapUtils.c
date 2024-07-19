/**
 * WinPR: Windows Portable Runtime
 * Time Zone Name Map Utils
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/synch.h>

#include <string.h>

#if defined(WITH_TIMEZONE_ICU)
#include <unicode/ucal.h>
#else
#include "WindowsZones.h"
#endif

#include "TimeZoneNameMap.h"

typedef struct
{
	size_t count;
	TimeZoneNameMapEntry* entries;
} TimeZoneNameMapContext;

static BOOL CALLBACK load_timezones(PINIT_ONCE once, PVOID param, PVOID* pvcontext)
{
	// Do not expose these, only used internally.
	extern const TimeZoneNameMapEntry TimeZoneNameMap[];
	extern const size_t TimeZoneNameMapSize;

	TimeZoneNameMapContext* context = pvcontext;
	WINPR_ASSERT(context);

	context->count = TimeZoneNameMapSize;
	context->entries = TimeZoneNameMap;
}

const TimeZoneNameMapEntry* TimeZoneGetAt(size_t index)
{
	static INIT_ONCE init_guard = INIT_ONCE_STATIC_INIT;
	static TimeZoneNameMapContext context = { 0 };

	InitOnceExecuteOnce(&init_guard, load_timezones, NULL, &context);
	if (index >= context.count)
		return NULL;
	return &context.entries[index];
}

static const char* return_type(const TimeZoneNameMapEntry* entry, TimeZoneNameType type)
{
	WINPR_ASSERT(entry);
	switch (type)
	{
		case TIME_ZONE_NAME_IANA:
			return entry->Iana;
		case TIME_ZONE_NAME_ID:
			return entry->Id;
		case TIME_ZONE_NAME_STANDARD:
			return entry->StandardName;
		case TIME_ZONE_NAME_DISPLAY:
			return entry->DisplayName;
		case TIME_ZONE_NAME_DAYLIGHT:
			return entry->DaylightName;
		default:
			return NULL;
	}
}

static BOOL iana_cmp(const TimeZoneNameMapEntry* entry, const char* iana)
{
	if (!entry || !iana || !entry->Iana)
		return FALSE;
	return strcmp(iana, entry->Iana) == 0;
}

static BOOL id_cmp(const TimeZoneNameMapEntry* entry, const char* id)
{
	if (!entry || !id || !entry->Id)
		return FALSE;
	return strcmp(id, entry->Id) == 0;
}

static const char* get_for_type(const char* val, TimeZoneNameType type,
                                BOOL (*cmp)(const TimeZoneNameMapEntry*, const char*))
{
	WINPR_ASSERT(val);
	WINPR_ASSERT(cmp);

	size_t index = 0;
	while (TRUE)
	{
		const TimeZoneNameMapEntry* entry = TimeZoneGetAt(index++);
		if (!entry)
			return NULL;
		if (cmp(entry, val))
			return return_type(entry, type);
	}
}

#if defined(WITH_TIMEZONE_ICU)
static char* get_wzid_icu(const UChar* utzid, size_t utzid_len)
{
	char* res = NULL;
	UErrorCode error = U_ZERO_ERROR;

	int32_t rc = ucal_getWindowsTimeZoneID(utzid, utzid_len, NULL, 0, &error);
	if ((error == U_BUFFER_OVERFLOW_ERROR) && (rc > 0))
	{
		rc++; // make space for '\0'
		UChar* wzid = calloc((size_t)rc + 1, sizeof(UChar));
		if (wzid)
		{
			UErrorCode error2 = U_ZERO_ERROR;
			int32_t rc2 = ucal_getWindowsTimeZoneID(utzid, utzid_len, wzid, rc, &error2);
			if (U_SUCCESS(error2) && (rc2 > 0))
				res = ConvertWCharNToUtf8Alloc(wzid, (size_t)rc, NULL);
			free(wzid);
		}
	}
	return res;
}

static char* get(const char* iana)
{
	size_t utzid_len = 0;
	UChar* utzid = ConvertUtf8ToWCharAlloc(iana, &utzid_len);
	if (!utzid)
		return NULL;

	char* wzid = get_wzid_icu(utzid, utzid_len);
	free(utzid);
	return wzid;
}

static const char* map_fallback(const char* iana, TimeZoneNameType type)
{
	char* wzid = get(iana);
	if (!wzid)
		return NULL;

	const char* res = get_for_type(wzid, type, id_cmp);
	free(wzid);
	return res;
}
#else
static const char* map_fallback(const char* iana, TimeZoneNameType type)
{
	if (!iana)
		return NULL;

	for (size_t x = 0; x < WindowsZonesNrElements; x++)
	{
		const WINDOWS_TZID_ENTRY* const entry = &WindowsZones[x];
		if (strchr(entry->tzid, ' '))
		{
			const char* res = NULL;
			char* tzid = _strdup(entry->tzid);
			char* ctzid = tzid;
			while (tzid)
			{
				char* space = strchr(tzid, ' ');
				if (space)
					*space++ = '\0';
				if (strcmp(tzid, iana) == 0)
				{
					res = entry->windows;
					break;
				}
				tzid = space;
			}
			free(ctzid);
			if (res)
				return res;
		}
		else if (strcmp(entry->tzid, iana) == 0)
			return entry->windows;
	}

	return NULL;
}
#endif

const char* TimeZoneIanaToWindows(const char* iana, TimeZoneNameType type)
{
	if (!iana)
		return NULL;

	const char* val = get_for_type(iana, type, iana_cmp);
	if (val)
		return val;

	const char* wzid = map_fallback(iana, type);
	if (!wzid)
		return NULL;

	return get_for_type(wzid, type, id_cmp);
}
