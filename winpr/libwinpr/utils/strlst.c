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

	for (i = 0; string_list[i]; i++)
	{
		free(string_list[i]);
	}

	free(string_list);
}

int string_list_length(const char* const* string_list)
{
	int i;

	for (i = 0; string_list[i]; i++)
		;

	return i;
}

char** string_list_copy(const char* const* string_list)
{
	int i;
	int length = string_list_length(string_list);
	char** copy = calloc(length + 1, sizeof(char*));

	if (!copy)
	{
		return 0;
	}

	for (i = 0; i < length; i++)
	{
		copy[i] = _strdup(string_list[i]);
	}

	copy[length] = 0;
	return copy;
}

void string_list_print(FILE* out, const char* const* string_list)
{
	int j;

	for (j = 0; string_list[j]; j++)
	{
		fprintf(out, "[%2d]: %s\n", j, string_list[j]);
	}

	fflush(out);
}
