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

#include <winpr/json.h>
#include <winpr/assert.h>

#if defined(WITH_CJSON)
#include <cjson/cJSON.h>
#endif
#if defined(WITH_JSONC)
#include <json-c/json.h>
#endif

#if defined(WITH_CJSON)
#if CJSON_VERSION_MAJOR == 1
#if CJSON_VERSION_MINOR <= 7
#if CJSON_VERSION_PATCH < 13
#define USE_CJSON_COMPAT
#endif
#endif
#endif
#endif

#if defined(WITH_JSONC)
#if JSON_C_MAJOR_VERSION == 0
#if JSON_C_MINOR_VERSION < 14
static struct json_object* json_object_new_null(void)
{
	return NULL;
}
#endif
#endif
#endif

#if defined(USE_CJSON_COMPAT)
static double cJSON_GetNumberValue(const cJSON* prop)
{
#ifndef NAN
#ifdef _WIN32
#define NAN sqrt(-1.0)
#define COMPAT_NAN_UNDEF
#else
#define NAN 0.0 / 0.0
#define COMPAT_NAN_UNDEF
#endif
#endif

	if (!cJSON_IsNumber(prop))
		return NAN;
	char* val = cJSON_GetStringValue(prop);
	if (!val)
		return NAN;

	errno = 0;
	char* endptr = NULL;
	double dval = strtod(val, &endptr);
	if (val == endptr)
		return NAN;
	if (endptr != NULL)
		return NAN;
	if (errno != 0)
		return NAN;
	return dval;

#ifdef COMPAT_NAN_UNDEF
#undef NAN
#endif
}

static cJSON* cJSON_ParseWithLength(const char* value, size_t buffer_length)
{
	// Check for string '\0' termination.
	const size_t slen = strnlen(value, buffer_length);
	if (slen >= buffer_length)
	{
		if (value[buffer_length] != '\0')
			return NULL;
	}
	return cJSON_Parse(value);
}
#endif

int WINPR_JSON_version(char* buffer, size_t len)
{
#if defined(WITH_JSONC)
	return _snprintf(buffer, len, "json-c %s", json_c_version());
#elif defined(WITH_CJSON)
	return _snprintf(buffer, len, "cJSON %s", cJSON_Version());
#else
	return _snprintf(buffer, len, "JSON support not available");
#endif
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
#if defined(WITH_JSONC)
	return json_tokener_parse(value);
#elif defined(WITH_CJSON)
	return cJSON_Parse(value);
#else
	WINPR_UNUSED(value);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
#if defined(WITH_JSONC)
	WINPR_ASSERT(buffer_length <= INT_MAX);
	json_tokener* tok = json_tokener_new();
	if (!tok)
		return NULL;
	json_object* obj = json_tokener_parse_ex(tok, value, (int)buffer_length);
	json_tokener_free(tok);
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_ParseWithLength(value, buffer_length);
#else
	WINPR_UNUSED(value);
	WINPR_UNUSED(buffer_length);
	return NULL;
#endif
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	json_object_put((json_object*)item);
#elif defined(WITH_CJSON)
	cJSON_Delete((cJSON*)item);
#else
	WINPR_UNUSED(item);
#endif
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
#if defined(WITH_JSONC)
	return json_object_array_get_idx((const json_object*)array, index);
#elif defined(WITH_CJSON)
	WINPR_ASSERT(index <= INT_MAX);
	return cJSON_GetArrayItem((const cJSON*)array, (INT)index);
#else
	WINPR_UNUSED(array);
	WINPR_UNUSED(index);
	return NULL;
#endif
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
#if defined(WITH_JSONC)
	return json_object_array_length((const json_object*)array);
#elif defined(WITH_CJSON)
	const int rc = cJSON_GetArraySize((const cJSON*)array);
	if (rc <= 0)
		return 0;
	return (size_t)rc;
#else
	WINPR_UNUSED(array);
	return 0;
#endif
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
#if defined(WITH_JSONC)
	return json_object_object_get((const json_object*)object, string);
#elif defined(WITH_CJSON)
	return cJSON_GetObjectItem((const cJSON*)object, string);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
#if defined(WITH_JSONC)
	return json_object_object_get((const json_object*)object, string);
#elif defined(WITH_CJSON)
	return cJSON_GetObjectItemCaseSensitive((const cJSON*)object, string);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return NULL;
#endif
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
#if defined(WITH_JSONC)
	return json_object_object_get_ex((const json_object*)object, string, NULL);
#elif defined(WITH_CJSON)
	return cJSON_HasObjectItem((const cJSON*)object, string);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return FALSE;
#endif
}

const char* WINPR_JSON_GetErrorPtr(void)
{
#if defined(WITH_JSONC)
	return json_util_get_last_err();
#elif defined(WITH_CJSON)
	return cJSON_GetErrorPtr();
#else
	return NULL;
#endif
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_get_string((json_object*)item);
#elif defined(WITH_CJSON)
	return cJSON_GetStringValue((cJSON*)item);
#else
	WINPR_UNUSED(item);
	return NULL;
#endif
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_get_double((const json_object*)item);
#elif defined(WITH_CJSON)
	return cJSON_GetNumberValue((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return nan("");
#endif
}

BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
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
#elif defined(WITH_CJSON)
	return cJSON_IsInvalid((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return TRUE;
#endif
}

BOOL WINPR_JSON_IsFalse(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	if (!json_object_is_type((const json_object*)item, json_type_boolean))
		return FALSE;
	json_bool val = json_object_get_boolean((const json_object*)item);
	return val == 0;
#elif defined(WITH_CJSON)
	return cJSON_IsFalse((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	if (!json_object_is_type((const json_object*)item, json_type_boolean))
		return FALSE;
	json_bool val = json_object_get_boolean((const json_object*)item);
	return val != 0;
#elif defined(WITH_CJSON)
	return cJSON_IsTrue((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_boolean);
#elif defined(WITH_CJSON)
	return cJSON_IsBool((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_null);
#elif defined(WITH_CJSON)
	return cJSON_IsNull((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_int) ||
	       json_object_is_type((const json_object*)item, json_type_double);
#elif defined(WITH_CJSON)
	return cJSON_IsNumber((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_string);
#elif defined(WITH_CJSON)
	return cJSON_IsString((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_array);
#elif defined(WITH_CJSON)
	return cJSON_IsArray((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	return json_object_is_type((const json_object*)item, json_type_object);
#elif defined(WITH_CJSON)
	return cJSON_IsObject((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
#if defined(WITH_JSONC)
	return json_object_new_null();
#elif defined(WITH_CJSON)
	return cJSON_CreateNull();
#else
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
#if defined(WITH_JSONC)
	return json_object_new_boolean(TRUE);
#elif defined(WITH_CJSON)
	return cJSON_CreateTrue();
#else
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
#if defined(WITH_JSONC)
	return json_object_new_boolean(FALSE);
#elif defined(WITH_CJSON)
	return cJSON_CreateFalse();
#else
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
#if defined(WITH_JSONC)
	return json_object_new_boolean(boolean);
#elif defined(WITH_CJSON)
	return cJSON_CreateBool(boolean);
#else
	WINPR_UNUSED(boolean);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
#if defined(WITH_JSONC)
	return json_object_new_double(num);
#elif defined(WITH_CJSON)
	return cJSON_CreateNumber(num);
#else
	WINPR_UNUSED(num);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
#if defined(WITH_JSONC)
	return json_object_new_string(string);
#elif defined(WITH_CJSON)
	return cJSON_CreateString(string);
#else
	WINPR_UNUSED(string);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
#if defined(WITH_JSONC)
	return json_object_new_array();
#elif defined(WITH_CJSON)
	return cJSON_CreateArray();
#else
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
#if defined(WITH_JSONC)
	return json_object_new_object();
#elif defined(WITH_CJSON)
	return cJSON_CreateObject();
#else
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_null();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddNullToObject((cJSON*)object, name);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_boolean(TRUE);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddTrueToObject((cJSON*)object, name);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_boolean(FALSE);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddFalseToObject((cJSON*)object, name);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name, BOOL boolean)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_boolean(boolean);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddBoolToObject((cJSON*)object, name, boolean);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(boolean);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name, double number)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_double(number);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddNumberToObject((cJSON*)object, name, number);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(number);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name, const char* string)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_string(string);
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddStringToObject((cJSON*)object, name, string);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(string);
	return NULL;
#endif
}

WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_object();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddObjectToObject((cJSON*)object, name);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
#endif
}

BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	const int rc = json_object_array_add((json_object*)array, (json_object*)item);
	if (rc != 0)
		return FALSE;
	return TRUE;
#elif defined(WITH_CJSON)
	return cJSON_AddItemToArray((cJSON*)array, (cJSON*)item);
#else
	WINPR_UNUSED(array);
	WINPR_UNUSED(item);
	return FALSE;
#endif
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
#if defined(WITH_JSONC)
	struct json_object* obj = json_object_new_array();
	if (json_object_object_add((json_object*)object, name, obj) != 0)
	{
		json_object_put(obj);
		return NULL;
	}
	return obj;
#elif defined(WITH_CJSON)
	return cJSON_AddArrayToObject((cJSON*)object, name);
#else
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
#endif
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	const char* str = json_object_to_json_string_ext((json_object*)item, JSON_C_TO_STRING_PRETTY);
	if (!str)
		return NULL;
	return _strdup(str);
#elif defined(WITH_CJSON)
	return cJSON_Print((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return NULL;
#endif
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
#if defined(WITH_JSONC)
	const char* str = json_object_to_json_string_ext((json_object*)item, JSON_C_TO_STRING_PLAIN);
	if (!str)
		return NULL;
	return _strdup(str);
#elif defined(WITH_CJSON)
	return cJSON_PrintUnformatted((const cJSON*)item);
#else
	WINPR_UNUSED(item);
	return NULL;
#endif
}
