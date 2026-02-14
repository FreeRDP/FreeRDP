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

#if !defined(WITH_DEBUG_UTILS_CMDLINE_DUMP)
static const char censoredmessage[] =
    "<censored: build with -DWITH_DEBUG_UTILS_CMDLINE_DUMP=ON for details>";
#endif

#define log_error(flags, msg, index, arg) \
	log_error_((flags), (msg), (index), (arg), __FILE__, __func__, __LINE__)
static void log_error_(DWORD flags, LPCSTR message, int index, WINPR_ATTR_UNUSED LPCSTR argv,
                       const char* file, const char* fkt, size_t line)
{
	if ((flags & COMMAND_LINE_SILENCE_PARSER) == 0)
	{
		const DWORD level = WLOG_ERROR;
		static wLog* log = NULL;
		if (!log)
			log = WLog_Get(TAG);

		if (!WLog_IsLevelActive(log, level))
			return;

		WLog_PrintTextMessage(log, level, line, file, fkt, "Failed at index %d [%s]: %s", index,
#if defined(WITH_DEBUG_UTILS_CMDLINE_DUMP)
		                      argv
#else
		                      censoredmessage
#endif
		                      ,
		                      message);
	}
}

#define log_comma_error(msg, arg) log_comma_error_((msg), (arg), __FILE__, __func__, __LINE__)
static void log_comma_error_(const char* message, WINPR_ATTR_UNUSED const char* argument,
                             const char* file, const char* fkt, size_t line)
{
	const DWORD level = WLOG_ERROR;
	static wLog* log = NULL;
	if (!log)
		log = WLog_Get(TAG);

	if (!WLog_IsLevelActive(log, level))
		return;

	WLog_PrintTextMessage(log, level, line, file, fkt, "%s [%s]", message,
#if defined(WITH_DEBUG_UTILS_CMDLINE_DUMP)
	                      argument
#else
	                      censoredmessage
#endif
	);
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
				log_error(flags, "PreFilter rule could not be applied", i, argv[i]);
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
				log_error(flags, "Unescaped sigil", i, argv[i]);
				return COMMAND_LINE_ERROR;
			}

			sigil_length = 0;
			escaped = FALSE;
			notescaped = TRUE;
		}
		else
		{
			log_error(flags, "Invalid sigil", i, argv[i]);
			return COMMAND_LINE_ERROR;
		}

		if ((sigil_length > 0) || (flags & COMMAND_LINE_SIGIL_NONE) ||
		    (flags & COMMAND_LINE_SIGIL_NOT_ESCAPED))
		{
			if (length < (sigil_length + 1))
			{
				if ((flags & COMMAND_LINE_IGN_UNKNOWN_KEYWORD))
					continue;

				log_error(flags, "Unexpected keyword", i, argv[i]);
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
				keyword_length = WINPR_ASSERTING_INT_CAST(size_t, (separator - keyword));
				value = &argv[i][value_index];
			}
			else
			{
				if (length < keyword_index)
				{
					log_error(flags, "Argument required", i, argv[i]);
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
						log_error(flags, "Argument required", i, argv[i]);
						return COMMAND_LINE_ERROR;
					}
				}

				if (!(flags & COMMAND_LINE_SEPARATOR_SPACE))
				{
					if (value && (cur->Flags & COMMAND_LINE_VALUE_FLAG))
					{
						log_error(flags, "Unexpected value", i, argv[i]);
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
					log_error(flags, "Missing value", i, argv[i]);
					status = COMMAND_LINE_ERROR_MISSING_VALUE;
					return status;
				}

				cur->Flags |= COMMAND_LINE_ARGUMENT_PRESENT;

				if (value)
				{
					if (!(cur->Flags & (COMMAND_LINE_VALUE_OPTIONAL | COMMAND_LINE_VALUE_REQUIRED)))
					{
						log_error(flags, "Unexpected value", i, argv[i]);
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
						log_error(flags, "PostFilter rule could not be applied", i, argv[i]);
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
				log_error(flags, "Unexpected keyword", i, argv[i]);
				return COMMAND_LINE_ERROR_NO_KEYWORD;
			}
		}
	}

	return status;
}

int CommandLineParseArgumentsW(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED LPWSTR* argv,
                               WINPR_ATTR_UNUSED COMMAND_LINE_ARGUMENT_W* options,
                               WINPR_ATTR_UNUSED DWORD flags, WINPR_ATTR_UNUSED void* context,
                               WINPR_ATTR_UNUSED COMMAND_LINE_PRE_FILTER_FN_W preFilter,
                               WINPR_ATTR_UNUSED COMMAND_LINE_POST_FILTER_FN_W postFilter)
{
	WLog_ERR("TODO", "TODO: implement");
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

static char* getNextComma(char* ptr)
{
	if (!ptr)
		return NULL;

	const size_t len = strlen(ptr);
	bool escaped = false;
	bool quoted = false;
	for (size_t x = 0; x < len; x++)
	{
		const char cur = ptr[x];
		if (escaped)
		{
			escaped = false;
			continue;
		}

		if (!quoted && (cur == '\\'))
		{
			escaped = true;
			continue;
		}

		if (!escaped && (cur == '"'))
		{
			quoted = !quoted;
			continue;
		}

		if (!quoted && !escaped && (cur == ','))
			return &ptr[x];
	}

	return &ptr[len];
}

static SSIZE_T countElements(const char* list)
{
	if (!list)
	{
		log_comma_error("Invalid argument (null)", list);
		return 0;
	}

	const size_t len = strlen(list);
	if (len == 0)
		return 0;

	SSIZE_T elements = 0;
	bool escaped = false;
	bool quoted = false;

	/* Parsing ruled:
	 * 1. \ is the escape character, the following one is then ignored from other rules.
	 * 2. "<string>" quotes build an escaped block that is ignored during parsing.
	 * 2. , is the separation character. Ignored if escaped or in a quote block.
	 */
	for (size_t x = 0; x < len; x++)
	{
		const char cur = list[x];

		if ((cur == '\\') && !escaped && !quoted)
		{
			escaped = true;
			continue;
		}

		if (!escaped && (cur == '"'))
		{
			quoted = !quoted;
			continue;
		}

		if (!quoted && !escaped && (cur == ','))
			elements++;

		if (escaped)
		{
			switch (cur)
			{
				case '"':
				case '\\':
				case ',':
					break;
				default:
					log_comma_error("Invalid argument (invalid escape sequence)", list);
					return -1;
			}
		}
		escaped = false;
	}

	if (quoted)
	{
		log_comma_error("Invalid argument (missing closing quote)", list);
		return -1;
	}
	if (escaped)
	{
		log_comma_error("Invalid argument (missing escaped char)", list);
		return -1;
	}

	return elements + 1;
}

static bool unescape(char* list)
{
	if (!list)
		return false;

	size_t len = strlen(list);
	bool escaped = false;
	bool quoted = false;
	size_t pos = 0;
	for (size_t x = 0; x < len; x++)
	{
		const char cur = list[x];
		if (escaped)
		{
			list[pos++] = cur;
			escaped = false;
			continue;
		}
		else if (!escaped && (cur == '"'))
		{
			quoted = !quoted;
		}

		if (!quoted && (cur == '\\'))
		{
			escaped = true;
			continue;
		}
		list[pos++] = cur;
	}
	list[pos++] = '\0';

	if (quoted)
	{
		log_comma_error("Invalid argument (unterminated quote sequence)", list);
		return false;
	}

	if (escaped)
	{
		log_comma_error("Invalid argument (unterminated escape sequence)", list);
		return false;
	}

	return true;
}

static bool unquote(char* list)
{
	if (!list)
		return false;

	size_t len = strlen(list);
	if (len < 2)
		return true;
	if ((list[0] != '"') || (list[len - 1] != '"'))
		return true;

	for (size_t x = 1; x < len - 1; x++)
	{
		char cur = list[x];
		if (cur == '\\')
		{
			x++;
			continue;
		}
		if (cur == '"')
			return true;
	}

	memmove(list, &list[1], len - 2);
	list[len - 2] = '\0';

	return true;
}

char** CommandLineParseCommaSeparatedValuesEx(const char* name, const char* clist, size_t* count)
{
	union
	{
		char** ppc;
		char* pc;
		void* pv;
	} ptr;

	ptr.pv = NULL;
	size_t namelen = 0;
	BOOL success = FALSE;

	if (count == NULL)
		return NULL;
	*count = 0;

	SSIZE_T rc = 0;
	size_t len = 0;
	char* arg = NULL;
	if (clist)
	{
		arg = _strdup(clist);
		if (!arg)
			return NULL;

		if (!unquote(arg))
			goto fail;

		// Get the number of segments separated by ','
		len = strlen(arg);
		rc = countElements(arg);
		if (rc < 0)
			goto fail;
	}

	size_t nArgs = WINPR_ASSERTING_INT_CAST(size_t, rc);

	/* allocate buffer */
	if (name)
	{
		namelen = strlen(name);
		nArgs++;
	}

	const size_t prefix = (nArgs + 1UL) * sizeof(char*);
	ptr.pv = calloc(prefix + len + 1 + namelen + 1 + nArgs, sizeof(char));

	if (!ptr.pv)
		goto fail;

	// No elements, just return empty
	if (rc == 0)
	{
		if (!name)
			goto fail;
	}

	size_t offset = prefix;
	if (name)
	{
		char* dst = &ptr.pc[offset];
		offset += namelen + 1;
		ptr.ppc[0] = dst;
		strncpy(dst, name, namelen);
	}

	{
		char* str = &ptr.pc[offset];
		strncpy(str, arg, len);

		for (size_t index = name ? 1 : 0; (index < nArgs) && str; index++)
		{
			char* cptr = str;
			str = getNextComma(cptr);
			if (str)
				*str++ = '\0';

			if (!unescape(cptr))
				goto fail;
			if (!unquote(cptr))
				goto fail;

			ptr.ppc[index] = cptr;
		}
	}

	*count = nArgs;
	success = TRUE;

fail:
	free(arg);
	if (!success)
	{
		if (count)
			*count = 0;
		free(ptr.pv);
		return NULL;
	}
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): this is an allocator function
	return ptr.ppc;
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

WINPR_ATTR_MALLOC(free, 1)
static char* escape_comma(const char* str)
{
	WINPR_ASSERT(str);
	const size_t len = strlen(str);
	size_t toEscape = 0;

	/* Get all symbols to escape.
	 * might catch a few too many as we don't check for escaped ones.
	 */
	for (size_t x = 0; x < len; x++)
	{
		switch (str[x])
		{
			case ',':
			case '\\':
				toEscape++;
				break;
			default:
				break;
		}
	}

	char* copy = calloc(len + toEscape + 1, sizeof(char));
	if (!copy)
		return NULL;

	bool escaped = false;
	bool quoted = false;
	for (size_t pos = 0, x = 0; x < len; x++)
	{
		const bool thisescaped = escaped;
		escaped = false;

		const char cur = str[x];
		switch (cur)
		{
			case '"':
				if (!thisescaped)
					quoted = !quoted;
				break;
			case ',':
			case '\\':
				escaped = true;
				if (!quoted)
					copy[pos++] = '\\';
				break;
			default:
				break;
		}
		copy[pos++] = cur;
	}

	if (quoted)
	{
		free(copy);
		return NULL;
	}
	return copy;
}

char* CommandLineToCommaSeparatedValuesEx(int argc, char* argv[], const char* filters[],
                                          size_t number)
{
	if ((argc <= 0) || !argv)
		return NULL;

	size_t size = WINPR_ASSERTING_INT_CAST(size_t, argc) + 1ull;
	for (int x = 0; x < argc; x++)
		size += strlen(argv[x]) *
		        2; /* Overallocate to allow (worst case) escaping every single character */

	char* str = calloc(size, sizeof(char));
	if (!str)
		return NULL;

	size_t offset = 0;
	for (int x = 0; x < argc; x++)
	{
		int rc = 0;

		const char* farg = filtered(argv[x], filters, number);
		if (!farg)
			continue;

		char* arg = escape_comma(farg);
		if (!arg)
		{
			free(str);
			return NULL;
		}

		rc = _snprintf(&str[offset], size - offset, "%s,", arg);
		free(arg);

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
