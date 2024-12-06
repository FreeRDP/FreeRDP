/**
 * WinPR: Windows Portable Runtime
 * Command-Line Utils
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/cmdline.h>

#include "../log.h"

#define TAG WINPR_TAG("commandline")

/**
 * Command-line syntax: some basic concepts:
 * https://pythonconquerstheuniverse.wordpress.com/2010/07/25/command-line-syntax-some-basic-concepts/
 */

/**
 * Command-Line Syntax:
 *
 * <sigil><keyword><separator><value>
 *
 * <sigil>: '/' or '-' or ('+' | '-')
 *
 * <keyword>: option, named argument, flag
 *
 * <separator>: ':' or '='
 *
 * <value>: argument value
 *
 */

static void log_error(DWORD flags, LPCSTR message, int index, LPCSTR argv)
{
	if ((flags & COMMAND_LINE_SILENCE_PARSER) == 0)
		WLog_ERR(TAG, message, index, argv);
}

int CommandLineParseArgumentsA(int argc, LPSTR* argv, COMMAND_LINE_ARGUMENT_A* options, DWORD flags,
                               void* context, COMMAND_LINE_PRE_FILTER_FN_A preFilter,
                               COMMAND_LINE_POST_FILTER_FN_A postFilter)
{
	int status = 0;
	int count = 0;
	BOOL notescaped = FALSE;
	const char* sigil = NULL;
	size_t sigil_length = 0;
	char* keyword = NULL;
	size_t keyword_index = 0;
	char* separator = NULL;
	char* value = NULL;
	int toggle = 0;

	if (!argv)
		return status;

	if (argc == 1)
	{
		if (flags & COMMAND_LINE_IGN_UNKNOWN_KEYWORD)
			status = 0;
		else
			status = COMMAND_LINE_STATUS_PRINT_HELP;

		return status;
	}

	for (int i = 1; i < argc; i++)
	{
		size_t keyword_length = 0;
		BOOL found = FALSE;
		BOOL escaped = TRUE;

		if (preFilter)
		{
			count = preFilter(context, i, argc, argv);

			if (count < 0)
			{
				log_error(flags, "Failed for index %d [%s]: PreFilter rule could not be applied", i,
				          argv[i]);
				status = COMMAND_LINE_ERROR;
				return status;
			}

			if (count > 0)
			{
				i += (count - 1);
				continue;
			}
		}

		sigil = argv[i];
		size_t length = strlen(argv[i]);

		if ((sigil[0] == '/') && (flags & COMMAND_LINE_SIGIL_SLASH))
		{
			sigil_length = 1;
		}
		else if ((sigil[0] == '-') && (flags & COMMAND_LINE_SIGIL_DASH))
		{
			sigil_length = 1;

			if (length > 2)
			{
				if ((sigil[1] == '-') && (flags & COMMAND_LINE_SIGIL_DOUBLE_DASH))
					sigil_length = 2;
			}
		}
		else if ((sigil[0] == '+') && (flags & COMMAND_LINE_SIGIL_PLUS_MINUS))
		{
			sigil_length = 1;
		}
		else if ((sigil[0] == '-') && (flags & COMMAND_LINE_SIGIL_PLUS_MINUS))
		{
			sigil_length = 1;
		}
		else if (flags & COMMAND_LINE_SIGIL_NONE)
		{
			sigil_length = 0;
		}
		else if (flags & COMMAND_LINE_SIGIL_NOT_ESCAPED)
		{
			if (notescaped)
			{
				log_error(flags, "Failed at index %d [%s]: Unescaped sigil", i, argv[i]);
				return COMMAND_LINE_ERROR;
			}

			sigil_length = 0;
			escaped = FALSE;
			notescaped = TRUE;
		}
		else
		{
			log_error(flags, "Failed at index %d [%s]: Invalid sigil", i, argv[i]);
			return COMMAND_LINE_ERROR;
		}

		if ((sigil_length > 0) || (flags & COMMAND_LINE_SIGIL_NONE) ||
		    (flags & COMMAND_LINE_SIGIL_NOT_ESCAPED))
		{
			if (length < (sigil_length + 1))
			{
				if ((flags & COMMAND_LINE_IGN_UNKNOWN_KEYWORD))
					continue;

				return COMMAND_LINE_ERROR_NO_KEYWORD;
			}

			keyword_index = sigil_length;
			keyword = &argv[i][keyword_index];
			toggle = -1;

			if (flags & COMMAND_LINE_SIGIL_ENABLE_DISABLE)
			{
				if (strncmp(keyword, "enable-", 7) == 0)
				{
					toggle = TRUE;
					keyword_index += 7;
					keyword = &argv[i][keyword_index];
				}
				else if (strncmp(keyword, "disable-", 8) == 0)
				{
					toggle = FALSE;
					keyword_index += 8;
					keyword = &argv[i][keyword_index];
				}
			}

			separator = NULL;

			if ((flags & COMMAND_LINE_SEPARATOR_COLON) && (!separator))
				separator = strchr(keyword, ':');

			if ((flags & COMMAND_LINE_SEPARATOR_EQUAL) && (!separator))
				separator = strchr(keyword, '=');

			if (separator)
			{
				SSIZE_T separator_index = (separator - argv[i]);
				SSIZE_T value_index = separator_index + 1;
				keyword_length = (separator - keyword);
				value = &argv[i][value_index];
			}
			else
			{
				if (length < keyword_index)
				{
					log_error(flags, "Failed at index %d [%s]: Argument required", i, argv[i]);
					return COMMAND_LINE_ERROR;
				}

				keyword_length = length - keyword_index;
				value = NULL;
			}

			if (!escaped)
				continue;

			for (size_t j = 0; options[j].Name != NULL; j++)
			{
				COMMAND_LINE_ARGUMENT_A* cur = &options[j];
				BOOL match = FALSE;

				if (strncmp(cur->Name, keyword, keyword_length) == 0)
				{
					if (strlen(cur->Name) == keyword_length)
						match = TRUE;
				}

				if ((!match) && (cur->Alias != NULL))
				{
					if (strncmp(cur->Alias, keyword, keyword_length) == 0)
					{
						if (strlen(cur->Alias) == keyword_length)
							match = TRUE;
					}
				}

				if (!match)
					continue;

				found = match;
				cur->Index = i;

				if ((flags & COMMAND_LINE_SEPARATOR_SPACE) && ((i + 1) < argc))
				{
					BOOL argument = 0;
					int value_present = 1;

					if (flags & COMMAND_LINE_SIGIL_DASH)
					{
						if (strncmp(argv[i + 1], "-", 1) == 0)
							value_present = 0;
					}

					if (flags & COMMAND_LINE_SIGIL_DOUBLE_DASH)
					{
						if (strncmp(argv[i + 1], "--", 2) == 0)
							value_present = 0;
					}

					if (flags & COMMAND_LINE_SIGIL_SLASH)
					{
						if (strncmp(argv[i + 1], "/", 1) == 0)
							value_present = 0;
					}

					if ((cur->Flags & COMMAND_LINE_VALUE_REQUIRED) ||
					    (cur->Flags & COMMAND_LINE_VALUE_OPTIONAL))
						argument = TRUE;
					else
						argument = FALSE;

					if (value_present && argument)
					{
						i++;
						value = argv[i];
					}
					else if (!value_present && (cur->Flags & COMMAND_LINE_VALUE_OPTIONAL))
					{
						value = NULL;
					}
					else if (!value_present && argument)
					{
						log_error(flags, "Failed at index %d [%s]: Argument required", i, argv[i]);
						return COMMAND_LINE_ERROR;
					}
				}

				if (!(flags & COMMAND_LINE_SEPARATOR_SPACE))
				{
					if (value && (cur->Flags & COMMAND_LINE_VALUE_FLAG))
					{
						log_error(flags, "Failed at index %d [%s]: Unexpected value", i, argv[i]);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
				}
				else
				{
					if (value && (cur->Flags & COMMAND_LINE_VALUE_FLAG))
					{
						i--;
						value = NULL;
					}
				}

				if (!value && (cur->Flags & COMMAND_LINE_VALUE_REQUIRED))
				{
					log_error(flags, "Failed at index %d [%s]: Missing value", i, argv[i]);
					status = COMMAND_LINE_ERROR_MISSING_VALUE;
					return status;
				}

				cur->Flags |= COMMAND_LINE_ARGUMENT_PRESENT;

				if (value)
				{
					if (!(cur->Flags & (COMMAND_LINE_VALUE_OPTIONAL | COMMAND_LINE_VALUE_REQUIRED)))
					{
						log_error(flags, "Failed at index %d [%s]: Unexpected value", i, argv[i]);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					cur->Value = value;
					cur->Flags |= COMMAND_LINE_VALUE_PRESENT;
				}
				else
				{
					if (cur->Flags & COMMAND_LINE_VALUE_FLAG)
					{
						cur->Value = (LPSTR)1;
						cur->Flags |= COMMAND_LINE_VALUE_PRESENT;
					}
					else if (cur->Flags & COMMAND_LINE_VALUE_BOOL)
					{
						if (flags & COMMAND_LINE_SIGIL_ENABLE_DISABLE)
						{
							if (toggle == -1)
								cur->Value = BoolValueTrue;
							else if (!toggle)
								cur->Value = BoolValueFalse;
							else
								cur->Value = BoolValueTrue;
						}
						else
						{
							if (sigil[0] == '+')
								cur->Value = BoolValueTrue;
							else if (sigil[0] == '-')
								cur->Value = BoolValueFalse;
							else
								cur->Value = BoolValueTrue;
						}

						cur->Flags |= COMMAND_LINE_VALUE_PRESENT;
					}
				}

				if (postFilter)
				{
					count = postFilter(context, &options[j]);

					if (count < 0)
					{
						log_error(flags,
						          "Failed at index %d [%s]: PostFilter rule could not be applied",
						          i, argv[i]);
						status = COMMAND_LINE_ERROR;
						return status;
					}
				}

				if (cur->Flags & COMMAND_LINE_PRINT)
					return COMMAND_LINE_STATUS_PRINT;
				else if (cur->Flags & COMMAND_LINE_PRINT_HELP)
					return COMMAND_LINE_STATUS_PRINT_HELP;
				else if (cur->Flags & COMMAND_LINE_PRINT_VERSION)
					return COMMAND_LINE_STATUS_PRINT_VERSION;
				else if (cur->Flags & COMMAND_LINE_PRINT_BUILDCONFIG)
					return COMMAND_LINE_STATUS_PRINT_BUILDCONFIG;
			}

			if (!found && (flags & COMMAND_LINE_IGN_UNKNOWN_KEYWORD) == 0)
			{
				log_error(flags, "Failed at index %d [%s]: Unexpected keyword", i, argv[i]);
				return COMMAND_LINE_ERROR_NO_KEYWORD;
			}
		}
	}

	return status;
}

int CommandLineParseArgumentsW(int argc, LPWSTR* argv, COMMAND_LINE_ARGUMENT_W* options,
                               DWORD flags, void* context, COMMAND_LINE_PRE_FILTER_FN_W preFilter,
                               COMMAND_LINE_POST_FILTER_FN_W postFilter)
{
	return 0;
}

int CommandLineClearArgumentsA(COMMAND_LINE_ARGUMENT_A* options)
{
	for (size_t i = 0; options[i].Name != NULL; i++)
	{
		options[i].Flags &= COMMAND_LINE_INPUT_FLAG_MASK;
		options[i].Value = NULL;
	}

	return 0;
}

int CommandLineClearArgumentsW(COMMAND_LINE_ARGUMENT_W* options)
{
	for (int i = 0; options[i].Name != NULL; i++)
	{
		options[i].Flags &= COMMAND_LINE_INPUT_FLAG_MASK;
		options[i].Value = NULL;
	}

	return 0;
}

const COMMAND_LINE_ARGUMENT_A* CommandLineFindArgumentA(const COMMAND_LINE_ARGUMENT_A* options,
                                                        LPCSTR Name)
{
	WINPR_ASSERT(options);
	WINPR_ASSERT(Name);

	for (size_t i = 0; options[i].Name != NULL; i++)
	{
		if (strcmp(options[i].Name, Name) == 0)
			return &options[i];

		if (options[i].Alias != NULL)
		{
			if (strcmp(options[i].Alias, Name) == 0)
				return &options[i];
		}
	}

	return NULL;
}

const COMMAND_LINE_ARGUMENT_W* CommandLineFindArgumentW(const COMMAND_LINE_ARGUMENT_W* options,
                                                        LPCWSTR Name)
{
	WINPR_ASSERT(options);
	WINPR_ASSERT(Name);

	for (size_t i = 0; options[i].Name != NULL; i++)
	{
		if (_wcscmp(options[i].Name, Name) == 0)
			return &options[i];

		if (options[i].Alias != NULL)
		{
			if (_wcscmp(options[i].Alias, Name) == 0)
				return &options[i];
		}
	}

	return NULL;
}

const COMMAND_LINE_ARGUMENT_A* CommandLineFindNextArgumentA(const COMMAND_LINE_ARGUMENT_A* argument)
{
	const COMMAND_LINE_ARGUMENT_A* nextArgument = NULL;

	if (!argument || !argument->Name)
		return NULL;

	nextArgument = &argument[1];

	if (nextArgument->Name == NULL)
		return NULL;

	return nextArgument;
}

static int is_quoted(char c)
{
	switch (c)
	{
		case '"':
			return 1;
		case '\'':
			return -1;
		default:
			return 0;
	}
}

static size_t get_element_count(const char* list, BOOL* failed, BOOL fullquoted)
{
	size_t count = 0;
	int quoted = 0;
	BOOL finished = FALSE;
	BOOL first = TRUE;
	const char* it = list;

	if (!list)
		return 0;
	if (strlen(list) == 0)
		return 0;

	while (!finished)
	{
		BOOL nextFirst = FALSE;
		switch (*it)
		{
			case '\0':
				if (quoted != 0)
				{
					WLog_ERR(TAG, "Invalid argument (missing closing quote) '%s'", list);
					*failed = TRUE;
					return 0;
				}
				finished = TRUE;
				break;
			case '\'':
			case '"':
				if (!fullquoted)
				{
					int now = is_quoted(*it);
					if (now == quoted)
						quoted = 0;
					else if (quoted == 0)
						quoted = now;
				}
				break;
			case ',':
				if (first)
				{
					WLog_ERR(TAG, "Invalid argument (empty list elements) '%s'", list);
					*failed = TRUE;
					return 0;
				}
				if (quoted == 0)
				{
					nextFirst = TRUE;
					count++;
				}
				break;
			default:
				break;
		}

		first = nextFirst;
		it++;
	}
	return count + 1;
}

static char* get_next_comma(char* string, BOOL fullquoted)
{
	const char* log = string;
	int quoted = 0;
	BOOL first = TRUE;

	WINPR_ASSERT(string);

	while (TRUE)
	{
		switch (*string)
		{
			case '\0':
				if (quoted != 0)
					WLog_ERR(TAG, "Invalid quoted argument '%s'", log);
				return NULL;

			case '\'':
			case '"':
				if (!fullquoted)
				{
					int now = is_quoted(*string);
					if ((quoted == 0) && !first)
					{
						WLog_ERR(TAG, "Invalid quoted argument '%s'", log);
						return NULL;
					}
					if (now == quoted)
						quoted = 0;
					else if (quoted == 0)
						quoted = now;
				}
				break;

			case ',':
				if (first)
				{
					WLog_ERR(TAG, "Invalid argument (empty list elements) '%s'", log);
					return NULL;
				}
				if (quoted == 0)
					return string;
				break;

			default:
				break;
		}
		first = FALSE;
		string++;
	}

	return NULL;
}

static BOOL is_valid_fullquoted(const char* string)
{
	char cur = '\0';
	char last = '\0';
	const char quote = *string++;

	/* We did not start with a quote. */
	if (is_quoted(quote) == 0)
		return FALSE;

	while ((cur = *string++) != '\0')
	{
		/* A quote is found. */
		if (cur == quote)
		{
			/* If the quote was escaped, it is valid. */
			if (last != '\\')
			{
				/* Only allow unescaped quote as last character in string. */
				if (*string != '\0')
					return FALSE;
			}
			/* If the last quote in the string is escaped, it is wrong. */
			else if (*string != '\0')
				return FALSE;
		}
		last = cur;
	}

	/* The string did not terminate with the same quote as it started. */
	if (last != quote)
		return FALSE;
	return TRUE;
}

char** CommandLineParseCommaSeparatedValuesEx(const char* name, const char* list, size_t* count)
{
	char** p = NULL;
	char* str = NULL;
	size_t nArgs = 0;
	size_t prefix = 0;
	size_t len = 0;
	size_t namelen = 0;
	BOOL failed = FALSE;
	char* copy = NULL;
	char* unquoted = NULL;
	BOOL fullquoted = FALSE;

	BOOL success = FALSE;
	if (count == NULL)
		goto fail;

	*count = 0;
	if (list)
	{
		int start = 0;
		int end = 0;
		unquoted = copy = _strdup(list);
		if (!copy)
			goto fail;

		len = strlen(unquoted);
		if (len > 0)
		{
			start = is_quoted(unquoted[0]);
			end = is_quoted(unquoted[len - 1]);

			if ((start != 0) && (end != 0))
			{
				if (start != end)
				{
					WLog_ERR(TAG, "invalid argument (quote mismatch) '%s'", list);
					goto fail;
				}
				if (!is_valid_fullquoted(unquoted))
					goto fail;
				unquoted[len - 1] = '\0';
				unquoted++;
				len -= 2;
				fullquoted = TRUE;
			}
		}
	}

	*count = get_element_count(unquoted, &failed, fullquoted);
	if (failed)
		goto fail;

	if (*count == 0)
	{
		if (!name)
			goto fail;
		else
		{
			size_t clen = strlen(name);
			p = (char**)calloc(2UL + clen, sizeof(char*));

			if (p)
			{
				char* dst = (char*)&p[1];
				p[0] = dst;
				(void)sprintf_s(dst, clen + 1, "%s", name);
				*count = 1;
				success = TRUE;
				goto fail;
			}
		}
	}

	nArgs = *count;

	if (name)
		nArgs++;

	prefix = (nArgs + 1UL) * sizeof(char*);
	if (name)
		namelen = strlen(name);
	p = (char**)calloc(len + prefix + 1 + namelen + 1, sizeof(char*));

	if (!p)
		goto fail;

	str = &((char*)p)[prefix];
	memcpy(str, unquoted, len);

	if (name)
	{
		char* namestr = &((char*)p)[prefix + len + 1];
		memcpy(namestr, name, namelen);

		p[0] = namestr;
	}

	for (size_t index = name ? 1 : 0; index < nArgs; index++)
	{
		char* ptr = str;
		const int quote = is_quoted(*ptr);
		char* comma = get_next_comma(str, fullquoted);

		if ((quote != 0) && !fullquoted)
			ptr++;

		p[index] = ptr;

		if (comma)
		{
			char* last = comma - 1;
			const int lastQuote = is_quoted(*last);

			if (!fullquoted)
			{
				if (lastQuote != quote)
				{
					WLog_ERR(TAG, "invalid argument (quote mismatch) '%s'", list);
					goto fail;
				}
				else if (lastQuote != 0)
					*last = '\0';
			}
			*comma = '\0';

			str = comma + 1;
		}
		else if (quote)
		{
			char* end = strrchr(ptr, '"');
			if (!end)
				goto fail;
			*end = '\0';
		}
	}

	*count = nArgs;
	success = TRUE;
fail:
	free(copy);
	if (!success)
	{
		if (count)
			*count = 0;
		free((void*)p);
		return NULL;
	}
	return p;
}

char** CommandLineParseCommaSeparatedValues(const char* list, size_t* count)
{
	return CommandLineParseCommaSeparatedValuesEx(NULL, list, count);
}

char* CommandLineToCommaSeparatedValues(int argc, char* argv[])
{
	return CommandLineToCommaSeparatedValuesEx(argc, argv, NULL, 0);
}

static const char* filtered(const char* arg, const char* filters[], size_t number)
{
	if (number == 0)
		return arg;
	for (size_t x = 0; x < number; x++)
	{
		const char* filter = filters[x];
		size_t len = strlen(filter);
		if (_strnicmp(arg, filter, len) == 0)
			return &arg[len];
	}
	return NULL;
}

char* CommandLineToCommaSeparatedValuesEx(int argc, char* argv[], const char* filters[],
                                          size_t number)
{
	char* str = NULL;
	size_t offset = 0;
	size_t size = argc + 1;
	if ((argc <= 0) || !argv)
		return NULL;

	for (int x = 0; x < argc; x++)
		size += strlen(argv[x]);

	str = calloc(size, sizeof(char));
	if (!str)
		return NULL;
	for (int x = 0; x < argc; x++)
	{
		int rc = 0;
		const char* arg = filtered(argv[x], filters, number);
		if (!arg)
			continue;
		rc = _snprintf(&str[offset], size - offset, "%s,", arg);
		if (rc <= 0)
		{
			free(str);
			return NULL;
		}
		offset += (size_t)rc;
	}
	if (offset > 0)
		str[offset - 1] = '\0';
	return str;
}

void CommandLineParserFree(char** ptr)
{
	union
	{
		char* p;
		char** pp;
	} uptr;
	uptr.pp = ptr;
	free(uptr.p);
}
