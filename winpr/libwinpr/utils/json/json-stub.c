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

int WINPR_JSON_version(char* buffer, size_t len)
{
	(void)_snprintf(buffer, len, "JSON support not available");
	return -1;
}

WINPR_JSON* WINPR_JSON_Parse(const char* value)
{
	WINPR_UNUSED(value);
	return NULL;
}

WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length)
{
	WINPR_UNUSED(value);
	WINPR_UNUSED(buffer_length);
	return NULL;
}

void WINPR_JSON_Delete(WINPR_JSON* item)
{
	WINPR_UNUSED(item);
}

WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index)
{
	WINPR_UNUSED(array);
	WINPR_UNUSED(index);
	return NULL;
}

size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array)
{
	WINPR_UNUSED(array);
	return 0;
}

WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return NULL;
}

WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object, const char* string)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return NULL;
}

BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(string);
	return FALSE;
}

const char* WINPR_JSON_GetErrorPtr(void)
{
	return NULL;
}

const char* WINPR_JSON_GetStringValue(WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return NULL;
}

double WINPR_JSON_GetNumberValue(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return nan("");
}

BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return TRUE;
}

BOOL WINPR_JSON_IsFalse(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsBool(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsNull(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsString(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsArray(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

BOOL WINPR_JSON_IsObject(const WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return FALSE;
}

WINPR_JSON* WINPR_JSON_CreateNull(void)
{
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateTrue(void)
{
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateFalse(void)
{
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean)
{
	WINPR_UNUSED(boolean);
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateNumber(double num)
{
	WINPR_UNUSED(num);
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateString(const char* string)
{
	WINPR_UNUSED(string);
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateArray(void)
{
	return NULL;
}

WINPR_JSON* WINPR_JSON_CreateObject(void)
{
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name, BOOL boolean)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(boolean);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name, double number)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(number);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name, const char* string)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	WINPR_UNUSED(string);
	return NULL;
}

WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
}

BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item)
{
	WINPR_UNUSED(array);
	WINPR_UNUSED(item);
	return FALSE;
}

WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name)
{
	WINPR_UNUSED(object);
	WINPR_UNUSED(name);
	return NULL;
}

char* WINPR_JSON_Print(WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return NULL;
}

char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item)
{
	WINPR_UNUSED(item);
	return NULL;
}
