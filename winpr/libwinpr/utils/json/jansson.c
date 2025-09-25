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

int WINPR_JSON_version(char* buffer, size_t len)
{
	return _snprintf(buffer, len, "jansson %s", jansson_version_str());
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
	json_error_t error = { 0 };
	return json_loads(value, JSON_DECODE_ANY, &error);
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
	json_error_t error = { 0 };
	return json_loadb(value, buffer_length, JSON_DECODE_ANY, &error);
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
	json_delete((json_t*)item);
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
	return json_array_get(array, index);
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
	return json_array_size(array);
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
	void* it = json_object_iter((json_t*)object);
	while (it)
	{
		const char* name = json_object_iter_key(it);
		if (_stricmp(name, string) == 0)
			return json_object_iter_value(it);
		it = json_object_iter_next((json_t*)object, it);
	}
	return NULL;
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
	return json_object_get(object, string);
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
	return json_object_get(object, string) != NULL;
}

const char* WINPR_JSON_GetErrorPtr(void)
{
	return NULL;
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
	return json_string_value(item);
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
	return json_real_value((const json_t*)item);
}

BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* item)
{
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
	return json_is_false((const json_t*)item);
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
	return json_is_true((const json_t*)item);
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
	return json_is_boolean((const json_t*)item);
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
	return json_is_null((const json_t*)item);
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
	return json_is_number((const json_t*)item);
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
	return json_is_string((const json_t*)item);
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
	return json_is_array((const json_t*)item);
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
	return json_is_array((const json_t*)item);
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
	return json_null();
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
	return json_true();
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
	return json_false();
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
	return json_boolean(boolean);
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
	return json_real(num);
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
	return json_string(string);
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
	return json_array();
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
	return json_object();
}

static WINPR_JSON* add_to_object(WINPR_JSON* object, const char* name, json_t* obj)
{
	if (!obj)
		return NULL;
	if (json_object_set_new(object, name, obj) != 0)
		return NULL;
	return obj;
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
	return json_array_append(array, item) == 0;
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
	json_t* obj = json_array();
	return add_to_object(object, name, obj);
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
	return json_dumps(item, JSON_INDENT(2) | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
	return json_dumps(item, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
}
