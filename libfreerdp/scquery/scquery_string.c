#include <stdarg.h>
#include <stdio.h>

#include "scquery_string.h"
#include "scquery_error.h"

#if _POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE)
/* strndup and strnlen are defined in string.h */
void string_dummy(void)
{
} /* some compilers complain on empty sources. */
#else

size_t strnlen(const char* string, size_t length)
{
	size_t i = 0;

	if (string == NULL)
	{
		return i;
	}

	while ((i < length) && (string[i] != '\0'))
	{
		i++;
	}

	return i;
}

char* strndup(const char* string, size_t length)
{
	if (string == NULL)
	{
		return NULL;
	}
	else
	{
		size_t i;
		size_t size = 1 + strnlen(string, length);
		char* result = checked_malloc(size);

		if (result == NULL)
		{
			errno = ENOMEM;
			return NULL;
		}

		for (i = 0; i < size - 1; i++)
		{
			result[i] = string[i];
		}

		result[size - 1] = '\0';
		return result;
	}
}

#endif

#if (_XOPEN_SOURCE >= 500) || (_POSIX_C_SOURCE >= 200809L) || _BSD_SOURCE || _SVID_SOURCE
/* strdup is defined in string.h */
#else

char* strdup(const char* string)
{
	if (string == NULL)
	{
		return NULL;
	}
	else
	{
		char* result = checked_malloc(1 + strlen(string));

		if (result == NULL)
		{
			errno = ENOMEM;
			return NULL;
		}

		strcpy(result, string);
		return result;
	}
}

#endif

/* typedef char* (* string_preprocess_pr)(const char *); */
/* typedef void (* string_postprocess_pr)(char *); */

char* string_mapconcat(string_preprocess_pr preprocess, string_postprocess_pr postprocess,
                       unsigned count, const char** strings, const char* separator)
{
	if ((count == 0) || (strings == 0))
	{
		goto failure;
	}

	char* item;
	char* current;
	size_t seplen = strlen(separator);
	size_t size = 1 + seplen * (count - 1);
	unsigned i;

	for (i = 0; i < count; i++)
	{
		size += strlen(strings[i]);
	}

	char* result = checked_malloc(size);

	if (result == NULL)
	{
		goto failure;
	}

	current = result;
	item = preprocess(strings[0]);
	strcpy(current, item);
	current = strchr(current, '\0');
	postprocess(item);

	for (i = 1; i < count; i++)
	{
		item = preprocess(strings[i]);
		strcat(current, separator);
		current = strchr(current, '\0');
		strcat(current, item);
		current = strchr(current, '\0');
		postprocess(item);
	}

	return result;
failure:
	return check_memory(strdup(""), 1);
}

size_t string_count(const char* string, char character)
{
	size_t count = 0;

	while ((string = strchr(string, character)) != NULL)
	{
		count++;
	}

	return count;
}

size_t padded_string_length(const char* padded_string, size_t max_size, char pad)
{
	size_t len = strnlen(padded_string, max_size);

	while ((len > 0) && (padded_string[len - 1] == pad))
	{
		len--;
	}

	return len;
}

char* string_from_padded_string(const char* padded_string, size_t max_size, char pad)
{
	size_t length = padded_string_length(padded_string, max_size, pad);
	return check_memory(strndup(padded_string, length), length);
}

char* string_format(const char* format_string, ...)
{
	char* result = NULL;
	va_list args;
	int length;
	va_start(args, format_string);
	length = vsnprintf(NULL, 0, format_string, args);
	va_end(args);
	result = checked_malloc(1 + length);

	if (result != NULL)
	{
		va_start(args, format_string);
		vsnprintf(result, 1 + length, format_string, args);
		va_end(args);
	}

	return result;
}
