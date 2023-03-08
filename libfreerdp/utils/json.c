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

#include <freerdp/utils/json.h>
#include <winpr/assert.h>
#include <winpr/string.h>
#include <ctype.h>

const char* whitespace = " \t\n\r";

struct JSON_OBJECT
{
	struct JSON_PROP** props;
	size_t count;
	size_t capacity;
};

struct JSON_PROP
{
	char* name;
	struct JSON* value;
};

struct JSON_ARRAY
{
	struct JSON** values;
	size_t count;
	size_t capacity;
};

struct JSON
{
	enum JSON_TYPE type;
	union
	{
		struct JSON_OBJECT object;
		struct JSON_ARRAY array;
		double number;
		char* string;
	} value;
};

static JSON* json_parse_value(const char* ptr, const char** tailptr);
static JSON* json_parse_object(const char* ptr, const char** tailptr);
static JSON* json_parse_array(const char* ptr, const char** tailptr);
static JSON* json_parse_number(const char* ptr, const char** tailptr);
static JSON* json_parse_string(const char* ptr, const char** tailptr);
static char* json_parse_raw_string(const char* str, const char** tailptr);

JSON* json_parse(const char* str)
{
	JSON* json = NULL;

	if (!str)
		return NULL;

	/* A JSON text is a single value surrounded by optional whitespace */
	str = str + strspn(str, whitespace);
	json = json_parse_value(str, &str);
	str = str + strspn(str, whitespace);

	/* Text must be fully consumed */
	if (*str != '\0')
	{
		json_free(json);
		return NULL;
	}

	return json;
}

JSON* json_new(enum JSON_TYPE type)
{
	struct JSON* json = calloc(1, sizeof(struct JSON));
	if (json)
		json->type = type;

	if (type == JSON_TYPE_OBJECT)
	{
		json->value.object.props = malloc(4 * sizeof(struct JSON_PROP*));
		if (!json->value.object.props)
		{
			free(json);
			return NULL;
		}
		json->value.object.capacity = 4;
	}
	else if (type == JSON_TYPE_ARRAY)
	{
		json->value.array.values = malloc(4 * sizeof(struct JSON_OBJECT*));
		if (!json->value.array.values)
		{
			free(json);
			return NULL;
		}
		json->value.array.capacity = 4;
	}
	else if (type == JSON_TYPE_STRING)
	{
		json->value.string = _strdup("");
		if (!json->value.string)
		{
			free(json);
			return NULL;
		}
	}

	return json;
}

void json_free(struct JSON* json)
{
	if (!json)
		return;

	if (json->type == JSON_TYPE_OBJECT)
	{
		for (size_t i = 0; i < json->value.object.count; i++)
		{
			free(json->value.object.props[i]->name);
			json_free(json->value.object.props[i]->value);
            free(json->value.object.props[i]);
		}
		free(json->value.object.props);
	}
	else if (json->type == JSON_TYPE_ARRAY)
	{
		for (size_t i = 0; i < json->value.array.count; i++)
			json_free(json->value.array.values[i]);
		free(json->value.array.values);
	}
	else if (json->type == JSON_TYPE_STRING)
	{
		free(json->value.string);
	}

	free(json);
}

enum JSON_TYPE json_get_type(struct JSON* json)
{
	if (!json)
		return JSON_TYPE_NULL;
	return json->type;
}

int json_object_set_prop(struct JSON* json, const char* name, JSON* value)
{
	struct JSON_OBJECT* object;
	struct JSON_PROP* new_prop;

	if (!json || !name || !value || json->type != JSON_TYPE_OBJECT)
		return 0;
	object = &json->value.object;

	/* If we reached capacity, double it */
	if (object->capacity <= object->count)
	{
		size_t new_size = object->capacity * 2;
		struct JSON_PROP** tmp = realloc(object->props, new_size * sizeof(struct JSON_PROP*));
		if (!tmp)
			return 0;
		object->capacity = new_size;
		object->props = tmp;
	}

	new_prop = calloc(1, sizeof(struct JSON_PROP));
	if (!new_prop)
		return 0;

	new_prop->name = _strdup(name);
	if (!new_prop->name)
	{
		free(new_prop);
		return 0;
	}
	new_prop->value = value;

	object->props[object->count++] = new_prop;
	return 1;
}

int json_array_add(JSON* json, JSON* value)
{
	struct JSON_ARRAY* array;

	if (!json || !value || json->type != JSON_TYPE_ARRAY)
		return 0;
	array = &json->value.array;

	/* If we reached capacity, double it */
	if (array->capacity <= array->count)
	{
		size_t new_size = array->capacity * 2;
		struct JSON** tmp = realloc(array->values, new_size * sizeof(struct JSON*));
		if (!tmp)
			return 0;
		array->capacity = new_size;
		array->values = tmp;
	}

	array->values[array->count++] = value;
	return 1;
}

int json_number_set(struct JSON* json, double value)
{
	if (!json || json->type != JSON_TYPE_NUMBER)
		return 0;
	json->value.number = value;
	return 1;
}

int json_string_set(struct JSON* json, const char* value)
{
	char* tmp;

	if (!json || json->type != JSON_TYPE_STRING)
		return 0;

	tmp = _strdup(value);
	if (!tmp)
		return 0;
	free(json->value.string);
	json->value.string = tmp;
	return 1;
}

int json_object_get_prop(struct JSON* json, const char* name, JSON** value)
{
	struct JSON_OBJECT* object;

	if (!json || !name || !value || json->type != JSON_TYPE_OBJECT)
		return 0;
	object = &json->value.object;

	for (size_t i = 0; i < object->count; i++)
	{
		if (strcmp(object->props[i]->name, name) == 0)
		{
			*value = object->props[i]->value;
			return 1;
		}
	}

	return 0;
}

int json_array_get(struct JSON* json, size_t index, JSON** value)
{
	struct JSON_ARRAY* array;

	if (!json || !value)
		return 0;
	array = &json->value.array;

	if (index >= array->count)
		return 0;

	*value = array->values[index];
	return 1;
}

int json_number_get(struct JSON* json, double* value)
{
	if (!json || !value)
		return 0;
	*value = json->value.number;
	return 1;
}

int json_string_get(struct JSON* json, const char** value)
{
	if (!json || !value)
		return 0;
	*value = json->value.string;
	return 1;
}

static JSON* json_parse_value(const char* ptr, const char** tailptr)
{
	struct JSON* json = NULL;

	if (!ptr)
		return NULL;

	/* Check what type of value to expect */
	if (*ptr == '{')
	{
		json = json_parse_object(ptr, &ptr);
	}
	else if (*ptr == '[')
	{
		json = json_parse_array(ptr, &ptr);
	}
	else if (*ptr == '"')
	{
		json = json_parse_string(ptr, &ptr);
	}
	else if (isdigit(*ptr) || *ptr == '-')
	{
		json = json_parse_number(ptr, &ptr);
	}
	else if (strncmp(ptr, "false", 5) == 0)
	{
		json = json_new(JSON_TYPE_FALSE);
		ptr += 5;
	}
	else if (strncmp(ptr, "null", 4) == 0)
	{
		json = json_new(JSON_TYPE_NULL);
		ptr += 4;
	}
	else if (strncmp(ptr, "true", 4) == 0)
	{
		json = json_new(JSON_TYPE_TRUE);
		ptr += 4;
	}
	/* If all these checks fail json remains null */

	if (tailptr)
		*tailptr = ptr;

	return json;
}

static JSON* json_parse_object(const char* ptr, const char** tailptr)
{
	struct JSON* json;
	char* prop_name = NULL;

	if (!ptr)
		return NULL;

	/* Objects begin with a '{' character surrounded by optional whitespace */
	ptr += strspn(ptr, whitespace);
	if (*ptr++ != '{')
		return NULL;
	ptr += strspn(ptr, whitespace);

	json = json_new(JSON_TYPE_OBJECT);
	if (!json)
		return NULL;

	/* Objects can be empty */
	/* Any leading whitespace has been consumed together with trailing whitespace of '{' above */
	if (*ptr == '}')
	{
		/* Consume '}' and any trailing whitespace */
		ptr++;
		if (tailptr)
			*tailptr = ptr + strspn(ptr, whitespace);
		return json;
	}

	while (*ptr)
	{
		const char* cur;

		prop_name = json_parse_raw_string(ptr, &ptr);
		if (!prop_name)
			break;

		/* Property names must be followd by a ':' character with optional surrounding whitespace */
		ptr += strspn(ptr, whitespace);
		if (*ptr++ != ':')
			break;
		ptr += strspn(ptr, whitespace);

		if (!json_object_set_prop(json, prop_name, json_parse_value(ptr, &ptr)))
			break;

		free(prop_name);
		prop_name = NULL;

		/* Consume any whitespace before either a '}' or ',' character */
		ptr += strspn(ptr, whitespace);

		cur = ptr++;
		if (*cur == '}')
		{
			/* Consume any space following '}' and parsing is complete */
			if (tailptr)
				*tailptr = ptr + strspn(ptr, whitespace);
			return json;
		}
		else if (*cur == ',')
		{
			/* Consume any space following ',' and continue to the next property */
			ptr += strspn(ptr, whitespace);
			continue;
		}
		else
			break;
	}

	/* If we got here, something failed */
	free(prop_name);
	json_free(json);
	return NULL;
}

static JSON* json_parse_array(const char* ptr, const char** tailptr)
{
	struct JSON* json;

	if (!ptr)
		return NULL;

	/* Arrays start with '[', with optional surrounding whitespace */
	ptr += strspn(ptr, whitespace);
	if (*ptr++ != '[')
		return NULL;
	ptr += strspn(ptr, whitespace);

	json = json_new(JSON_TYPE_ARRAY);
	if (!json)
		return NULL;

	/* Arrays can be empty */
	/* Any leading whitespace has been consumed together with trailing whitespace of '[' above */
	if (*ptr == ']')
	{
		/* Consume ']' and any trailing whitespace */
		ptr++;
		if (tailptr)
			*tailptr = ptr + strspn(ptr, whitespace);
		return json;
	}

	while (*ptr)
	{
		const char* cur;

		if (!json_array_add(json, json_parse_value(ptr, &ptr)))
			break;

		/* Consume any whitespace leading ']' or ',' */
		ptr += strspn(ptr, whitespace);

		cur = ptr++;
		if (*cur == ']')
		{
			/* The array is completely parsed */
			if (tailptr)
				*tailptr = ptr + strspn(ptr, whitespace);
			return json;
		}
		else if (*cur == ',')
		{
			ptr += strspn(ptr, whitespace);
			continue;
		}
		else
			break;
	}

	json_free(json);
	return NULL;
}

static JSON* json_parse_number(const char* ptr, const char** tailptr)
{
	struct JSON* json;
	char* tail;
	double value;

	if (!ptr)
		return NULL;

	value = strtod(ptr, &tail);
	if (tail == ptr)
		return NULL;

	json = json_new(JSON_TYPE_NUMBER);
	if (!json)
		return NULL;

	json_number_set(json, value);

	if (tailptr)
		*tailptr = tail;

	return json;
}

static JSON* json_parse_string(const char* ptr, const char** tailptr)
{
	struct JSON* json;
	char *string = NULL;

	if (!ptr)
		return NULL;

	json = json_new(JSON_TYPE_STRING);
	if (!json)
		return NULL;
	
	string = json_parse_raw_string(ptr, &ptr);

	if (!json_string_set(json, string))
	{
		json_free(json);
		json = NULL;
	}

	free(string);

	if (tailptr)
		*tailptr = ptr;

	return json;
}

static uint16_t get_utf16_code_point(const char* str)
{
	char hexdig[5] = { 0 };
	char* tailptr;
	uint16_t cp;

	for (int i = 0; i < 4 && isxdigit(str[i]); i++)
		hexdig[i] = str[i];
	if (strlen(hexdig) != 4)
		return 0;

	cp = (uint16_t)strtoul(hexdig, &tailptr, 16);
	if (tailptr != hexdig + 4)
		return 0;

	return cp;
}

static char* json_parse_raw_string(const char* str, const char** tailptr)
{
	char* buffer;
	char* ptr;
	size_t str_len = strlen(str);
	int escaped = 0;

	/* Strings must begin with '"' */
	if (!str || *str++ != '"')
		return NULL;

	/* The string cannot be longer than the rest of the JSON text */
	if (!(buffer = malloc(str_len)))
		return NULL;
	ptr = buffer;

	/* Read until we encounter an unescaped end-quote or we reach the end of the text */
	while (*str && (escaped || *str != '"'))
	{
		if (escaped)
		{
			char cur = *str++;

			if (cur == 'u')
			{
				uint32_t cp;
				uint16_t surrogate;

				cp = get_utf16_code_point(str);
				if (cp == 0)
					break;
				str += 4;

				/* Check if this might be the high surrogate of a pair */
				if (cp >= 0xD800 && cp < 0xDC00 && str[0] == '\\' && str[1] == 'u')
				{
					surrogate = get_utf16_code_point(str + 2);
					if (surrogate >= 0xDC00 && surrogate < 0xE000)
					{
						cp = (((cp & 0x3FF) << 10) & (surrogate & 0x3FF)) + 0x10000;
						str += 6;
					}
				}

				/* Encode the character as utf-8 */
				if (cp < 0x80)
					*ptr++ = cp;
				else if (cp < 0x800)
				{
					*ptr++ = 0xC0 | (cp >> 6);
					*ptr++ = 0x80 | (cp & 0x3F);
				}
				else if (cp < 0x10000)
				{
					*ptr++ = 0xE0 | (cp >> 12);
					*ptr++ = 0x80 | ((cp >> 6) & 0x3F);
					*ptr++ = 0x80 | (cp & 0x3F);
				}
				else
				{
					*ptr++ = 0xF0 | (cp >> 18);
					*ptr++ = 0x80 | ((cp >> 12) & 0x3F);
					*ptr++ = 0x80 | ((cp >> 6) & 0x3F);
					*ptr++ = 0x80 | (cp & 0x3F);
				}
			}
			else if (cur == '"')
				*ptr++ = '"';
			else if (cur == '\\')
				*ptr++ = '\\';
			else if (cur == '/')
				*ptr++ = '/';
			else if (cur == 'b')
				*ptr++ = '\b';
			else if (cur == 'f')
				*ptr++ = '\f';
			else if (cur == 'n')
				*ptr++ = '\n';
			else if (cur == 'r')
				*ptr++ = '\r';
			else if (cur == 't')
				*ptr++ = '\t';
			else
				break;
			escaped = 0;
		}
		else if (*str == '\\')
		{
			str++;
			escaped = 1;
		}
		else
			*ptr++ = *str++;
	}
    *ptr++ = '\0';

	if (escaped || *str++ != '"')
	{
		free(buffer);
		return NULL;
	}
	else
	{
		/* Try to not tie up a potentially large amount of unused memory */
		char* tmp = realloc(buffer, ptr - buffer);
		if (tmp)
			buffer = tmp;
	}

	if (tailptr)
		*tailptr = str;

	return buffer;
}
