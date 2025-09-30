/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * JSON parser wrapper
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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
#include <winpr/file.h>
#include <winpr/json.h>
#include <winpr/assert.h>

#if !defined(WITH_JANSSON)
#error "This file must only be compiled if jansson library is linked in"
#endif
#include <jansson.h>

#if defined(WITH_DEBUG_JANSSON)
#include "../log.h"
#define TAG WINPR_TAG("jansson")

#define ccast(json) ccast_((json), __func__)
static const json_t* ccast_(const WINPR_JSON* json, const char* fkt)
{
	const json_t* jansson = (const json_t*)json;
	if (!jansson)
		WLog_DBG(TAG, "%s: NULL", fkt);
	else
	{
		WLog_DBG(TAG, "%s: %" PRIuz, fkt, jansson->refcount);
	}
	return jansson;
}

#define cast(json) cast_((json), __func__)
static json_t* cast_(WINPR_JSON* json, const char* fkt)
{
	json_t* jansson = (json_t*)json;
	if (!jansson)
		WLog_DBG(TAG, "%s: NULL", fkt);
	else
	{
		WLog_DBG(TAG, "%s: %" PRIuz, fkt, jansson->refcount);
	}
	return jansson;
}

#define revcast(json) revcast_((json), __func__)
static WINPR_JSON* revcast_(json_t* json, const char* fkt)
{
	json_t* jansson = (json_t*)json;
	if (!jansson)
		WLog_DBG(TAG, "%s: NULL", fkt);
	else
	{
		WLog_DBG(TAG, "%s: %" PRIuz, fkt, jansson->refcount);
	}
	return jansson;
}
#else
static inline const json_t* ccast(const WINPR_JSON* json)
{
	return WINPR_CXX_COMPAT_CAST(const json_t*, json);
}

static inline json_t* cast(WINPR_JSON* json)
{
	return WINPR_CXX_COMPAT_CAST(json_t*, json);
}

static inline WINPR_JSON* revcast(json_t* json)
{
	return WINPR_CXX_COMPAT_CAST(WINPR_JSON*, json);
}
#endif

int WINPR_JSON_version(char* buffer, size_t len)
{
	return _snprintf(buffer, len, "jansson %s", jansson_version_str());
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
	json_error_t error = { 0 };
	return revcast(json_loads(value, JSON_DECODE_ANY, &error));
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
	json_error_t error = { 0 };
	return revcast(json_loadb(value, buffer_length, JSON_DECODE_ANY, &error));
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
	json_delete(cast(item));
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
	return revcast(json_array_get(ccast(array), index));
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
	return json_array_size(ccast(array));
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
	json_t* json = cast(WINPR_CAST_CONST_PTR_AWAY(object, WINPR_JSON*));
	void* it = json_object_iter(json);
	while (it)
	{
		const char* name = json_object_iter_key(it);
		if (_stricmp(name, string) == 0)
			return revcast(json_object_iter_value(it));
		it = json_object_iter_next(json, it);
	}
	return NULL;
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
	return revcast(json_object_get(ccast(object), string));
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
	return json_object_get(ccast(object), string) != NULL;
}

const char* WINPR_JSON_GetErrorPtr(void)
{
	return NULL;
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
	return json_string_value(cast(item));
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
	return json_real_value(ccast(item));
}

BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* json)
{
	const json_t* item = ccast(json);
	if (WINPR_JSON_IsArray(item))
		return FALSE;
	if (WINPR_JSON_IsObject(item))
		return FALSE;
	if (WINPR_JSON_IsNull(item))
		return FALSE;
	if (WINPR_JSON_IsNumber(item))
		return FALSE;
	if (WINPR_JSON_IsBool(item))
		return FALSE;
	if (WINPR_JSON_IsString(item))
		return FALSE;
	return TRUE;
}

BOOL WINPR_JSON_IsFalse(const WINPR_JSON* item)
{
	return json_is_false(ccast(item));
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
	return json_is_true(ccast(item));
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
	return json_is_boolean(ccast(item));
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
	return json_is_null(ccast(item));
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
	return json_is_number(ccast(item));
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
	return json_is_string(ccast(item));
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
	return json_is_array(ccast(item));
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
	return json_is_object(ccast(item));
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
	return revcast(json_null());
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
	return revcast(json_true());
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
	return revcast(json_false());
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
	return revcast(json_boolean(boolean));
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
	return revcast(json_real(num));
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
	return revcast(json_string(string));
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
	return revcast(json_array());
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
	return revcast(json_object());
}

static WINPR_JSON* add_to_object(WINPR_JSON* object, const char* name, json_t* obj)
{
	if (!obj)
		return NULL;
	const BOOL rc = (json_object_set_new(cast(object), name, obj) != 0);
	json_decref(obj);
	if (!rc)
		return NULL;
	return revcast(obj);
}

WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_null();
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_true();
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_false();
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name, BOOL boolean)
{
	json_t* obj = json_boolean(boolean);
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name, double number)
{
	json_t* obj = json_real(number);
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name, const char* string)
{
	json_t* obj = json_string(string);
	return add_to_object(object, name, obj);
}

WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_object();
	return add_to_object(object, name, obj);
}

BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item)
{
	return json_array_append(cast(array), item) == 0;
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_array();
	return add_to_object(object, name, obj);
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
	return json_dumps(cast(item), JSON_INDENT(2) | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
	return json_dumps(cast(item), JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
}
