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
#include <stdarg.h>
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

char* string_concatenate(const char* string, ...)
{
	char* result;
	char* current;
	/* sum the lengths of the strings */
	const char* arg = string;
	int total_length = 0;
	va_list strings;
	va_start(strings, string);

	while (arg)
	{
		total_length += strlen(arg);
		arg = va_arg(strings, const char*);
	}

	va_end(strings);
	total_length += 1; /*  null byte */

	if (NULL == (result = malloc(total_length)))
	{
		return NULL;
	}

	/* start copying */
	current = result;
	strcpy(current, string);
	current += strlen(string);
	va_start(strings, string);
	arg = va_arg(strings, const char*);

	while (arg)
	{
		strcpy(current, arg);
		current += strlen(arg);
		arg = va_arg(strings, const char*);
	}

	va_end(strings);
	/* strcpy copied the terminating null byte */
	return result;
}

static int extract_separated_substrings(const char* string, const char* separator,
                                        int remove_empty_substring, char** result)
{
	/*
	PRECONDITION: (string != NULL) && (strlen(string) > 0) && (separator != NULL) &&
	(strlen(separator) > 0)
	*/
	size_t seplen = strlen(separator);
	int i = 0;
	int done = 0;

	do
	{
		char* next = strstr(string, separator);
		size_t sublen = 0;
		/*
		 * When there are no remaining separator,
		 * we still need to add the rest of the string
		 * so we find the end-of-string
		 */
		done = (next == NULL);

		if (done)
		{
			next = strchr(string, '\0');
		}

		sublen = next - string;

		if (!remove_empty_substring || (sublen > 0))
		{
			if (result != NULL)
			{
				result[i] = strndup(string, sublen);
			}

			i++;
		}

		if (!done)
		{
			string = next + seplen;
		}
	} while (!done);

	return i;
}
char** string_list_split_string(const char* string, const char* separator,
                                int remove_empty_substring)
{
	char** result = NULL;
	size_t seplen = ((separator == NULL) ? 0 : strlen(separator));
	size_t count = 0;

	if (string == NULL)
	{
		goto empty_result;
	}

	if (seplen == 0)
	{
		if (remove_empty_substring && (strlen(string) == 0))
		{
			goto empty_result;
		}

		result = calloc(2, sizeof(*result));

		if (result == NULL)
		{
			return NULL;
		}

		result[0] = strdup(string);

		if (result[0] == NULL)
		{
			free(result);
			return NULL;
		}

		return result;
	}

	count = extract_separated_substrings(string, separator, remove_empty_substring, NULL);
	result = calloc(count + 1, sizeof(*result));

	if (result == NULL)
	{
		return NULL;
	}

	extract_separated_substrings(string, separator, remove_empty_substring, result);

	if (count != string_list_length((char**)result))
	{
		/* at least one of the strdup couldn't allocate */
		string_list_free(result);
		return NULL;
	}

	return result;
empty_result:
	return calloc(1, sizeof(*result));
}
