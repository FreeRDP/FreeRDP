/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Simple JSON parser
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
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

#ifndef FREERDP_UTILS_JSON_H
#define FREERDP_UTILS_JSON_H

#include <freerdp/api.h>

enum JSON_TYPE
{
	JSON_TYPE_FALSE,
	JSON_TYPE_NULL,
	JSON_TYPE_TRUE,
	JSON_TYPE_OBJECT,
	JSON_TYPE_ARRAY,
	JSON_TYPE_NUMBER,
	JSON_TYPE_STRING
};

typedef struct JSON JSON;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API JSON* json_parse(const char* str);
	FREERDP_API JSON* json_new(enum JSON_TYPE type);
	FREERDP_API void json_free(JSON* json);

	FREERDP_API enum JSON_TYPE json_get_type(JSON* json);

	FREERDP_API int json_object_set_prop(JSON* object, const char* prop, JSON* value);
	FREERDP_API int json_array_add(JSON* array, JSON* value);
	FREERDP_API int json_number_set(JSON* json, double value);
	FREERDP_API int json_string_set(JSON* json, const char* value);

	FREERDP_API int json_object_get_prop(JSON* object, const char* prop, JSON** value);
	FREERDP_API int json_array_get(JSON* array, size_t index, JSON** value);
	FREERDP_API int json_number_get(JSON* json, double* value);
	FREERDP_API int json_string_get(JSON* json, const char** value);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_JSON_H */
