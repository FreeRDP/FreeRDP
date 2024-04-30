/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Interface
 *
 * Copyright 2024 Ondrej Holy <oholy@redhat.com>
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

#ifndef FREERDP_JSON_H
#define FREERDP_JSON_H

#if defined(WITH_CJSON)

#include <cjson/cJSON.h>

#elif defined(WITH_JSONC)

#include <json-c/json.h>
#include <winpr/string.h>

#define cJSON json_object
#define cJSON_Delete json_object_put
#define cJSON_GetArrayItem json_object_array_get_idx
#define cJSON_GetArraySize json_object_array_length
#define cJSON_GetNumberValue json_object_get_double
#define cJSON_GetObjectItemCaseSensitive json_object_object_get
#define cJSON_GetStringValue(item) (char *)json_object_get_string((json_object *)item)
#define cJSON_IsArray(item) json_object_is_type(item, json_type_array)
#define cJSON_IsBool(item) json_object_is_type(item, json_type_boolean)
#define cJSON_IsNumber(item) \
	(json_object_is_type(item, json_type_double) || json_object_is_type(item, json_type_int))
#define cJSON_IsString(item) json_object_is_type(item, json_type_string)
#define cJSON_IsTrue json_object_get_boolean
#define cJSON_Parse json_tokener_parse

#endif

#endif /* FREERDP_JSON_H */
