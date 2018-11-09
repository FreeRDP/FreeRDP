/*
 * String List Utils
 *
 * Copyright 2018 Pascal Bourguignon <pjb@informatimago.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/strlst.h>
#include <winpr/string.h>


void string_list_free(char** string_list)
{
	int i;

	for (i = 0; string_list[i]; i ++)
	{
		free(string_list[i]);
	}

	free(string_list);
}

int string_list_length(const char* const* string_list)
{
	int i;

	for (i = 0; string_list[i]; i ++);

	return i;
}

char**  string_list_copy(const char* const* string_list)
{
	int i;
	int length = string_list_length(string_list);
	char** copy = malloc((length + 1) * sizeof(char*));

	if (copy == NULL)
	{
		return NULL;
	}

	for (i = 0; i < length; i ++)
	{
		copy[i] = _strdup(string_list[i]);

		if (copy[i] == NULL)
		{
			string_list_free(copy);
			return NULL;
		}
	}

	copy[length] = NULL;
	return copy;
}

void string_list_print(FILE* out, const char* const* string_list)
{
	int j;

	for (j = 0; string_list[j]; j ++)
	{
		fprintf(out, "[%2d]: %s\n", j, string_list[j]);
	}

	fflush(out);
}

char* string_list_join(const char* const* string_list, const char* separator)
{
	char* result;
	char* current;
	size_t maximum_size;
	size_t i;
	size_t count = string_list_length(string_list);
	size_t separator_length = strlen(separator);
	size_t* string_lengths = malloc(sizeof(*string_lengths) * count);
	size_t total_length = 0;

	if (string_lengths == NULL)
	{
		return NULL;
	}

	for (i = 0; i < count; i ++)
	{
		string_lengths[i] = strlen(string_list[i]);
		total_length += string_lengths[i];
	}

	maximum_size = (((count == 0) ? 0 : (count - 1) * separator_length)
	                + total_length
	                + 1);
	result = malloc(maximum_size);

	if (result == NULL)
	{
		goto done;
	}

	strcpy(result, "");
	current = result;

	for (i = 0; i < count; i ++)
	{
		strcpy(current, string_list[i]);
		current += string_lengths[i];

		if (i < count - 1)
		{
			strcpy(current, separator);
			current += separator_length;
		}
	}

done:
	free(string_lengths);
	return result;
}
