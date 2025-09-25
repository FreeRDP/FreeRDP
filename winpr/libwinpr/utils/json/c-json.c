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

#if !defined(WITH_CJSON)
#error "This file must only be compiled when cJSON is enabled"
#endif
#include <cjson/cJSON.h>

#if defined(WITH_CJSON)
#if CJSON_VERSION_MAJOR == 1
#if (CJSON_VERSION_MINOR < 7) || ((CJSON_VERSION_MINOR == 7) && (CJSON_VERSION_PATCH < 13))
#define USE_CJSON_COMPAT
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
	return _snprintf(buffer, len, "cJSON %s", cJSON_Version());
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
	return cJSON_Parse(value);
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
	return cJSON_ParseWithLength(value, buffer_length);
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
	cJSON_Delete((cJSON*)item);
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
	WINPR_ASSERT(index <= INT_MAX);
	return cJSON_GetArrayItem((const cJSON*)array, (INT)index);
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
	const int rc = cJSON_GetArraySize((const cJSON*)array);
	if (rc <= 0)
		return 0;
	return (size_t)rc;
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
	return cJSON_GetObjectItem((const cJSON*)object, string);
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
	return cJSON_GetObjectItemCaseSensitive((const cJSON*)object, string);
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
	return cJSON_HasObjectItem((const cJSON*)object, string);
}

const char* WINPR_JSON_GetErrorPtr(void)
{
	return cJSON_GetErrorPtr();
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
	return cJSON_GetStringValue((cJSON*)item);
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
	return cJSON_GetNumberValue((const cJSON*)item);
}

BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* item)
{
	return cJSON_IsInvalid((const cJSON*)item);
}

BOOL WINPR_JSON_IsFalse(const WINPR_JSON* item)
{
	return cJSON_IsFalse((const cJSON*)item);
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
	return cJSON_IsTrue((const cJSON*)item);
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
	return cJSON_IsBool((const cJSON*)item);
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
	return cJSON_IsNull((const cJSON*)item);
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
	return cJSON_IsNumber((const cJSON*)item);
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
	return cJSON_IsString((const cJSON*)item);
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
	return cJSON_IsArray((const cJSON*)item);
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
	return cJSON_IsObject((const cJSON*)item);
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
	return cJSON_CreateNull();
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
	return cJSON_CreateTrue();
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
	return cJSON_CreateFalse();
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
	return cJSON_CreateBool(boolean);
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
	return cJSON_CreateNumber(num);
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
	return cJSON_CreateString(string);
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
	return cJSON_CreateArray();
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
	return cJSON_CreateObject();
}

WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name)
{
	return cJSON_AddNullToObject((cJSON*)object, name);
}

WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name)
{
	return cJSON_AddTrueToObject((cJSON*)object, name);
}

WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name)
{
	return cJSON_AddFalseToObject((cJSON*)object, name);
}

WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name, BOOL boolean)
{
	return cJSON_AddBoolToObject((cJSON*)object, name, boolean);
}

WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name, double number)
{
	return cJSON_AddNumberToObject((cJSON*)object, name, number);
}

WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name, const char* string)
{
	return cJSON_AddStringToObject((cJSON*)object, name, string);
}

WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name)
{
	return cJSON_AddObjectToObject((cJSON*)object, name);
}

BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item)
{
#if defined(USE_CJSON_COMPAT)
	if ((array == NULL) || (item == NULL))
		return FALSE;
	cJSON_AddItemToArray((cJSON*)array, (cJSON*)item);
	return TRUE;
#else
	return cJSON_AddItemToArray((cJSON*)array, (cJSON*)item);
#endif
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
	return cJSON_AddArrayToObject((cJSON*)object, name);
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
	return cJSON_Print((const cJSON*)item);
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
	return cJSON_PrintUnformatted((const cJSON*)item);
}
