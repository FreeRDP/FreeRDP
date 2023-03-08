/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * TestJSON
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

#include <freerdp/utils/json.h>

const char* valid_json = " {\n\t\"string\" :\"two\\nlines \\\"with quotes\\\"\" , \"number\": "
                         "-12.3, \n \"true\" : true ,\"array\":[1,\"two\", {\"three\":3}]}";

int TestJSON(int argc, char* argv[])
{
	JSON* j = NULL;
	const char* s = NULL;
	double n = 0;
	JSON* a = NULL;
	JSON* json = json_parse(valid_json);
	if (!json)
		return -1;

	if (json_get_type(json) != JSON_TYPE_OBJECT)
		return -2;

	if (!json_object_get_prop(json, "string", &j))
		return -3;
	if (json_get_type(j) != JSON_TYPE_STRING)
		return -4;

	if (!json_string_get(j, &s))
		return -5;
	if (strcmp(s, "two\nlines \"with quotes\"") != 0)
		return -6;

	if (!json_object_get_prop(json, "number", &j))
		return -7;
	if (json_get_type(j) != JSON_TYPE_NUMBER)
		return -8;

	if (!json_number_get(j, &n))
		return -9;
	if (n != -12.3)
		return -10;

	if (!json_object_get_prop(json, "true", &j))
		return -11;
	if (json_get_type(j) != JSON_TYPE_TRUE)
		return -12;

	if (!json_object_get_prop(json, "array", &a))
		return -13;
	if (json_get_type(a) != JSON_TYPE_ARRAY)
		return -14;

	if (!json_array_get(a, 0, &j))
		return -15;
	if (json_get_type(j) != JSON_TYPE_NUMBER)
		return -16;

	if (json_array_get(a, 3, &j) != 0)
		return -17;

	if (json_object_get_prop(json, "notfound", &j) != 0)
		return -18;

	json_free(json);

	return 0;
}
