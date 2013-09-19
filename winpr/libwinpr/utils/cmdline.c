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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/cmdline.h>

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

int CommandLineParseArgumentsA(int argc, LPCSTR* argv, COMMAND_LINE_ARGUMENT_A* options, DWORD flags,
		void* context, COMMAND_LINE_PRE_FILTER_FN_A preFilter, COMMAND_LINE_POST_FILTER_FN_A postFilter)
{
	int i, j;
	int status;
	int count;
	int length;
	int index;
	BOOL match;
	BOOL found;
	BOOL argument;
	BOOL escaped;
	BOOL notescaped;
	char* sigil;
	int sigil_length;
	int sigil_index;
	char* keyword;
	int keyword_length;
	int keyword_index;
	char* separator;
	int separator_length;
	int separator_index;
	char* value;
	int value_length;
	int value_index;
	int toggle;

	status = 0;

	match = FALSE;
	found = FALSE;
	argument = FALSE;
	escaped = TRUE;
	notescaped = FALSE;

	if (!argv)
		return status;

	if (argc == 1)
	{
		status = COMMAND_LINE_STATUS_PRINT_HELP;
		return status;
	}

	for (i = 1; i < argc; i++)
	{
		index = i;

		escaped = TRUE;

		if (preFilter)
		{
			count = preFilter(context, i, argc, argv);

			if (count < 0)
			{
				status = COMMAND_LINE_ERROR;
				return status;
			}

			if (count > 0)
			{
				i += (count - 1);
				continue;
			}
		}

		sigil_index = 0;
		sigil_length = 0;
		sigil = (char*) &argv[i][sigil_index];
		length = strlen(argv[i]);

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
				return COMMAND_LINE_ERROR; 

			sigil_length = 0;
			escaped = FALSE;
			notescaped = TRUE;
		}
		else
		{
			return COMMAND_LINE_ERROR;
		}

		if ((sigil_length > 0) || (flags & COMMAND_LINE_SIGIL_NONE) ||
				(flags & COMMAND_LINE_SIGIL_NOT_ESCAPED))
		{
			if (length < (sigil_length + 1))
				return COMMAND_LINE_ERROR_NO_KEYWORD;

			keyword_index = sigil_index + sigil_length;
			keyword = (char*) &argv[i][keyword_index];

			toggle = -1;

			if (flags & COMMAND_LINE_SIGIL_ENABLE_DISABLE)
			{
				if (strncmp(keyword, "enable-", 7) == 0)
				{
					toggle = TRUE;
					keyword_index += 7;
					keyword = (char*) &argv[i][keyword_index];
				}
				else if (strncmp(keyword, "disable-", 8) == 0)
				{
					toggle = FALSE;
					keyword_index += 8;
					keyword = (char*) &argv[i][keyword_index];
				}
			}

			separator = NULL;

			if ((flags & COMMAND_LINE_SEPARATOR_COLON) && (!separator))
				separator = strchr(keyword, ':');

			if ((flags & COMMAND_LINE_SEPARATOR_EQUAL) && (!separator))
				separator = strchr(keyword, '=');

			if (separator)
			{
				separator_length = 1;
				separator_index = (separator - argv[i]);

				keyword_length = (separator - keyword);

				value_index = separator_index + separator_length;
				value = (char*) &argv[i][value_index];
				value_length = (length - value_index);
			}
			else
			{
				separator_length = 0;
				separator_index = -1;
				keyword_length = (length - keyword_index);

				value_index = -1;
				value = NULL;
				value_length = 0;
			}

			if (!escaped)
				continue;

			found = FALSE;
			for (j = 0; options[j].Name != NULL; j++)
			{
				match = FALSE;

				if (strncmp(options[j].Name, keyword, keyword_length) == 0)
				{
					if (strlen(options[j].Name) == keyword_length)
						match = TRUE;
				}

				if ((!match) && (options[j].Alias != NULL))
				{
					if (strncmp(options[j].Alias, keyword, keyword_length) == 0)
					{
						if (strlen(options[j].Alias) == keyword_length)
							match = TRUE;
					}
				}

				if (!match)
					continue;

				found = match;
				options[j].Index = index;

				if ((flags & COMMAND_LINE_SEPARATOR_SPACE) && ((i + 1) < argc))
				{
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

					if ((options[j].Flags & COMMAND_LINE_VALUE_REQUIRED) ||
							(options[j].Flags & COMMAND_LINE_VALUE_OPTIONAL))
						argument = TRUE;
					else
						argument = FALSE;
					
					if (value_present && argument)
					{
						i++;
						value_index = 0;
						length = strlen(argv[i]);

						value = (char*) &argv[i][value_index];
						value_length = (length - value_index);
					}
					else if (!value_present && (options[j].Flags & COMMAND_LINE_VALUE_OPTIONAL))
					{
						value_index = 0;
						value = NULL;
						value_length = 0;
					}
					else if (!value_present && argument)
						return COMMAND_LINE_ERROR;
				}

				if (!(flags & COMMAND_LINE_SEPARATOR_SPACE))
				{
					if (value && (options[j].Flags & COMMAND_LINE_VALUE_FLAG))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				else
				{
					if (value && (options[j].Flags & COMMAND_LINE_VALUE_FLAG))
					{
						i--;
						value_index = -1;
						value = NULL;
						value_length = 0;
					}
				}

				if (!value && (options[j].Flags & COMMAND_LINE_VALUE_REQUIRED))
				{
					status = COMMAND_LINE_ERROR_MISSING_VALUE;
					return status;
				}

				options[j].Flags |= COMMAND_LINE_ARGUMENT_PRESENT;

				if (value)
				{
					options[j].Value = value;
					options[j].Flags |= COMMAND_LINE_VALUE_PRESENT;
				}
				else
				{
					if (options[j].Flags & COMMAND_LINE_VALUE_FLAG)
					{
						options[j].Value = (LPSTR) 1;
						options[j].Flags |= COMMAND_LINE_VALUE_PRESENT;
					}
					else if (options[j].Flags & COMMAND_LINE_VALUE_BOOL)
					{
						if (flags & COMMAND_LINE_SIGIL_ENABLE_DISABLE)
						{
							if (toggle == -1)
								options[j].Value = BoolValueTrue;
							else if (!toggle)
								options[j].Value = BoolValueFalse;
							else
								options[j].Value = BoolValueTrue;
						}
						else
						{
							if (sigil[0] == '+')
								options[j].Value = BoolValueTrue;
							else if (sigil[0] == '-')
								options[j].Value = BoolValueFalse;
							else
								options[j].Value = BoolValueTrue;
						}

						options[j].Flags |= COMMAND_LINE_VALUE_PRESENT;
					}
				}

				if (postFilter)
					postFilter(context, &options[j]);

				if (options[j].Flags & COMMAND_LINE_PRINT)
						return COMMAND_LINE_STATUS_PRINT;
				else if (options[j].Flags & COMMAND_LINE_PRINT_HELP)
						return COMMAND_LINE_STATUS_PRINT_HELP;
				else if (options[j].Flags & COMMAND_LINE_PRINT_VERSION)
						return COMMAND_LINE_STATUS_PRINT_VERSION;
			}
			
			if (!found)
				return COMMAND_LINE_ERROR_NO_KEYWORD;
		}
	}

	return status;
}

int CommandLineParseArgumentsW(int argc, LPCWSTR* argv, COMMAND_LINE_ARGUMENT_W* options, DWORD flags,
		void* context, COMMAND_LINE_PRE_FILTER_FN_W preFilter, COMMAND_LINE_POST_FILTER_FN_W postFilter)
{
	return 0;
}

int CommandLineClearArgumentsA(COMMAND_LINE_ARGUMENT_A* options)
{
	int i;

	for (i = 0; options[i].Name != NULL; i++)
	{
		options[i].Flags &= COMMAND_LINE_INPUT_FLAG_MASK;
		options[i].Value = NULL;
	}

	return 0;
}

int CommandLineClearArgumentsW(COMMAND_LINE_ARGUMENT_W* options)
{
	int i;

	for (i = 0; options[i].Name != NULL; i++)
	{
		options[i].Flags &= COMMAND_LINE_INPUT_FLAG_MASK;
		options[i].Value = NULL;
	}

	return 0;
}

COMMAND_LINE_ARGUMENT_A* CommandLineFindArgumentA(COMMAND_LINE_ARGUMENT_A* options, LPCSTR Name)
{
	int i;

	for (i = 0; options[i].Name != NULL; i++)
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

COMMAND_LINE_ARGUMENT_W* CommandLineFindArgumentW(COMMAND_LINE_ARGUMENT_W* options, LPCWSTR Name)
{
	int i;

	for (i = 0; options[i].Name != NULL; i++)
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

COMMAND_LINE_ARGUMENT_A* CommandLineFindNextArgumentA(COMMAND_LINE_ARGUMENT_A* argument)
{
	COMMAND_LINE_ARGUMENT_A* nextArgument;

	nextArgument = &argument[1];

	if (nextArgument->Name == NULL)
		return NULL;

	return nextArgument;
}
