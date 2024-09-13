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
#include <winpr/json.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include "../log.h"

#include <string.h>

#define TAG WINPR_TAG("timezone.utils")

#if defined(WITH_TIMEZONE_ICU)
#include <unicode/ucal.h>
#else
#include "WindowsZones.h"
#endif

#include "TimeZoneNameMap.h"

#if defined(WITH_TIMEZONE_COMPILED)
#include "TimeZoneNameMap_static.h"
#endif

typedef struct
{
	size_t count;
	TimeZoneNameMapEntry* entries;
} TimeZoneNameMapContext;

static TimeZoneNameMapContext tz_context = { 0 };

static void tz_entry_free(TimeZoneNameMapEntry* entry)
{
	if (!entry)
		return;
	free(entry->DaylightName);
	free(entry->DisplayName);
	free(entry->Iana);
	free(entry->Id);
	free(entry->StandardName);

	const TimeZoneNameMapEntry empty = { 0 };
	*entry = empty;
}

static TimeZoneNameMapEntry tz_entry_clone(const TimeZoneNameMapEntry* entry)
{
	TimeZoneNameMapEntry clone = { 0 };
	if (!entry)
		return clone;

	if (entry->DaylightName)
		clone.DaylightName = _strdup(entry->DaylightName);
	if (entry->DisplayName)
		clone.DisplayName = _strdup(entry->DisplayName);
	if (entry->Iana)
		clone.Iana = _strdup(entry->Iana);
	if (entry->Id)
		clone.Id = _strdup(entry->Id);
	if (entry->StandardName)
		clone.StandardName = _strdup(entry->StandardName);
	return clone;
}

static void tz_context_free(void)
{
	for (size_t x = 0; x < tz_context.count; x++)
		tz_entry_free(&tz_context.entries[x]);
	free(tz_context.entries);
	tz_context.count = 0;
	tz_context.entries = NULL;
}

#if defined(WITH_TIMEZONE_FROM_FILE) && defined(WITH_WINPR_JSON)
static char* tz_get_object_str(WINPR_JSON* json, size_t pos, const char* name)
{
	WINPR_ASSERT(json);
	if (!WINPR_JSON_IsObject(json) || !WINPR_JSON_HasObjectItem(json, name))
	{
		WLog_WARN(TAG, "Invalid JSON entry at entry %" PRIuz ", missing an Object named '%s'", pos,
		          name);
		return NULL;
	}
	WINPR_JSON* obj = WINPR_JSON_GetObjectItem(json, name);
	WINPR_ASSERT(obj);
	if (!WINPR_JSON_IsString(obj))
	{
		WLog_WARN(TAG,
		          "Invalid JSON entry at entry %" PRIuz ", Object named '%s': Not of type string",
		          pos, name);
		return NULL;
	}

	const char* str = WINPR_JSON_GetStringValue(obj);
	if (!str)
	{
		WLog_WARN(TAG, "Invalid JSON entry at entry %" PRIuz ", Object named '%s': NULL string",
		          pos, name);
		return NULL;
	}

	return _strdup(str);
}

static BOOL tz_parse_json_entry(WINPR_JSON* json, size_t pos, TimeZoneNameMapEntry* entry)
{
	WINPR_ASSERT(entry);
	if (!json || !WINPR_JSON_IsObject(json))
	{
		WLog_WARN(TAG, "Invalid JSON entry at entry %" PRIuz ", expected an array", pos);
		return FALSE;
	}

	entry->Id = tz_get_object_str(json, pos, "Id");
	entry->StandardName = tz_get_object_str(json, pos, "StandardName");
	entry->DisplayName = tz_get_object_str(json, pos, "DisplayName");
	entry->DaylightName = tz_get_object_str(json, pos, "DaylightName");
	entry->Iana = tz_get_object_str(json, pos, "Iana");
	if (!entry->Id || !entry->StandardName || !entry->DisplayName || !entry->DaylightName ||
	    !entry->Iana)
	{
		tz_entry_free(entry);
		return FALSE;
	}
	return TRUE;
}

static WINPR_JSON* load_timezones_from_file(const char* filename)
{
	INT64 jstrlen = 0;
	char* jstr = NULL;
	WINPR_JSON* json = NULL;
	FILE* fp = winpr_fopen(filename, "r");
	if (!fp)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' does not exist or is not readable", filename);
		return NULL;
	}

	if (_fseeki64(fp, 0, SEEK_END) < 0)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' seek failed", filename);
		goto end;
	}
	jstrlen = _ftelli64(fp);
	if (jstrlen < 0)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' invalid length %" PRId64, filename, jstrlen);
		goto end;
	}
	if (_fseeki64(fp, 0, SEEK_SET) < 0)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' seek failed", filename);
		goto end;
	}

	jstr = calloc(jstrlen + 1, sizeof(char));
	if (!jstr)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' failed to allocate buffer of size %" PRId64,
		          filename, jstrlen);
		goto end;
	}

	if (fread(jstr, jstrlen, sizeof(char), fp) != 1)
	{
		WLog_WARN(TAG, "Timezone resource file '%s' failed to read buffer of size %" PRId64,
		          filename, jstrlen);
		goto end;
	}

	json = WINPR_JSON_ParseWithLength(jstr, jstrlen);
	if (!json)
		WLog_WARN(TAG, "Timezone resource file '%s' is not a valid JSON file", filename);
end:
	fclose(fp);
	free(jstr);
	return json;
}
#endif

static BOOL reallocate_context(TimeZoneNameMapContext* context, size_t size_to_add)
{
	{
		TimeZoneNameMapEntry* tmp = realloc(context->entries, (context->count + size_to_add) *
		                                                          sizeof(TimeZoneNameMapEntry));
		if (!tmp)
		{
			WLog_WARN(TAG,
			          "Failed to reallocate TimeZoneNameMapEntry::entries to %" PRIuz " elements",
			          context->count + size_to_add);
			return FALSE;
		}
		const size_t offset = context->count;
		context->entries = tmp;
		context->count += size_to_add;

		memset(&context->entries[offset], 0, size_to_add * sizeof(TimeZoneNameMapEntry));
	}
	return TRUE;
}

static BOOL CALLBACK load_timezones(PINIT_ONCE once, PVOID param, PVOID* pvcontext)
{
	TimeZoneNameMapContext* context = param;
	WINPR_ASSERT(context);
	WINPR_UNUSED(pvcontext);
	WINPR_UNUSED(once);

	const TimeZoneNameMapContext empty = { 0 };
	*context = empty;

#if defined(WITH_TIMEZONE_FROM_FILE) && defined(WITH_WINPR_JSON)
	{
		WINPR_JSON* json = NULL;
		char* filename = GetCombinedPath(WINPR_RESOURCE_ROOT, "TimeZoneNameMap.json");
		if (!filename)
		{
			WLog_WARN(TAG, "Could not create WinPR timezone resource filename");
			goto end;
		}

		json = load_timezones_from_file(filename);
		if (!json)
			goto end;

		if (!WINPR_JSON_IsObject(json))
		{
			WLog_WARN(TAG, "Invalid top level JSON type in file %s, expected an array", filename);
			goto end;
		}

		WINPR_JSON* obj = WINPR_JSON_GetObjectItem(json, "TimeZoneNameMap");
		if (!WINPR_JSON_IsArray(obj))
		{
			WLog_WARN(TAG, "Invalid top level JSON type in file %s, expected an array", filename);
			goto end;
		}
		const size_t count = WINPR_JSON_GetArraySize(obj);
		const size_t offset = context->count;
		if (!reallocate_context(context, count))
			goto end;
		for (size_t x = 0; x < count; x++)
		{
			WINPR_JSON* entry = WINPR_JSON_GetArrayItem(obj, x);
			if (!tz_parse_json_entry(entry, x, &context->entries[offset + x]))
				goto end;
		}

	end:
		free(filename);
		WINPR_JSON_Delete(json);
	}
#endif

#if defined(WITH_TIMEZONE_COMPILED)
	{
		const size_t offset = context->count;
		if (!reallocate_context(context, TimeZoneNameMapSize))
			return FALSE;
		for (size_t x = 0; x < TimeZoneNameMapSize; x++)
			context->entries[offset + x] = tz_entry_clone(&TimeZoneNameMap[x]);
	}
#endif

	(void)atexit(tz_context_free);
	return TRUE;
}

const TimeZoneNameMapEntry* TimeZoneGetAt(size_t index)
{
	static INIT_ONCE init_guard = INIT_ONCE_STATIC_INIT;

	InitOnceExecuteOnce(&init_guard, load_timezones, &tz_context, NULL);
	if (index >= tz_context.count)
		return NULL;
	return &tz_context.entries[index];
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
