/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * JSON parser wrapper
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
#include <math.h>
#include <errno.h>

#include <winpr/file.h>
#include <winpr/json.h>
#include <winpr/assert.h>

#if !defined(WITH_JSONC)
#error "This file must only be compiled when json-c is enabled"
#endif
#include <json.h>

#if JSON_C_MAJOR_VERSION == 0
#if JSON_C_MINOR_VERSION < 14
static struct json_object* json_object_new_null(void)
{
	return NULL;
}
#endif
#endif

int WINPR_JSON_version(char* buffer, size_t len)
{
	return _snprintf(buffer, len, "json-c %s", json_c_version());
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
	return json_tokener_parse(value);
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
	WINPR_ASSERT(buffer_length <= INT_MAX);
	json_tokener* tok = json_tokener_new();
	if (!tok)
		return NULL;
	json_object* obj = json_tokener_parse_ex(tok, value, (int)buffer_length);
	json_tokener_free(tok);
	return obj;
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
	json_object_put((json_object*)item);
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
	return json_object_array_get_idx((const json_object*)array, index);
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
	return json_object_array_length((const json_object*)array);
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
	struct json_object_iterator it = json_object_iter_begin((const json_object*)object);
	struct json_object_iterator itEnd = json_object_iter_end((const json_object*)object);
	while (!json_object_iter_equal(&it, &itEnd))
	{
		const char* key = json_object_iter_peek_name(&it);
		if (_stricmp(key, string) == 0)
		{
			return json_object_iter_peek_value(&it);
		}
		json_object_iter_next(&it);
	}
	return NULL;
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
	return json_object_object_get((const json_object*)object, string);
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
	return json_object_object_get_ex((const json_object*)object, string, NULL);
}

const char* WINPR_JSON_GetErrorPtr(void)
{
	return json_util_get_last_err();
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
	return json_object_get_string((json_object*)item);
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
	return json_object_get_double((const json_object*)item);
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
	if (!json_object_is_type((const json_object*)item, json_type_boolean))
		return FALSE;
	json_bool val = json_object_get_boolean((const json_object*)item);
	return val == 0;
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
	if (!json_object_is_type((const json_object*)item, json_type_boolean))
		return FALSE;
	json_bool val = json_object_get_boolean((const json_object*)item);
	return val != 0;
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_boolean);
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_null);
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_int) ||
	       json_object_is_type((const json_object*)item, json_type_double);
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_string);
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_array);
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
	return json_object_is_type((const json_object*)item, json_type_object);
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
	return json_object_new_null();
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
	return json_object_new_boolean(TRUE);
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
	return json_object_new_boolean(FALSE);
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
	return json_object_new_boolean(boolean);
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
	return json_object_new_double(num);
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
	return json_object_new_string(string);
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
	return json_object_new_array();
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
	return json_object_new_object();
}

WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name)
{
	struct json_object* obj = json_object_new_null();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name)
{
	struct json_object* obj = json_object_new_boolean(TRUE);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name)
{
	struct json_object* obj = json_object_new_boolean(FALSE);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name, BOOL boolean)
{
	struct json_object* obj = json_object_new_boolean(boolean);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name, double number)
{
	struct json_object* obj = json_object_new_double(number);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name, const char* string)
{
	struct json_object* obj = json_object_new_string(string);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name)
{
	struct json_object* obj = json_object_new_object();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item)
{
	const int rc = json_object_array_add((json_object*)array, (json_object*)item);
	if (rc != 0)
		return FALSE;
	return TRUE;
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
	struct json_object* obj = json_object_new_array();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
	const char* str = json_object_to_json_string_ext((json_object*)item, JSON_C_TO_STRING_PRETTY);
	if (!str)
		return NULL;
	return _strdup(str);
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
	const char* str = json_object_to_json_string_ext((json_object*)item, JSON_C_TO_STRING_PLAIN);
	if (!str)
		return NULL;
	return _strdup(str);
}
