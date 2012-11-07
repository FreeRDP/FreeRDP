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
 * Remote Desktop Connection Usage
 *
 * mstsc [<connection file>] [/v:<server[:port]>] [/admin] [/f[ullscreen]]
 * [/w:<width>] [/h:<height>] [/public] | [/span] [/multimon] [/migrate]
 * [/edit "connection file"] [/?]
 *
 * "connection file" -- Specifies the name of an .RDP for the connection.
 *
 * /v:<server[:port]> -- Specifies the remote computer to which you want
 * to connect.
 *
 * /admin -- Connects you to the session for administering a server.
 *
 * /f -- Starts Remote Desktop in full-screen mode.
 *
 * /w:<width> -- Specifies the width of the Remote Desktop window.
 *
 * /h:<height> -- Specifies the height of the Remote Desktop window.
 *
 * /public -- Runs Remote Desktop in public mode.
 *
 * /span -- Matches the remote desktop width and height with the local
 * virtual desktop, spanning across multiple monitors if necessary. To
 * span across monitors, the monitors must be arranged to form a
 * rectangle.
 *
 * /multimon -- Configures the remote desktop session layout to
 * be identical to the current client-side configuration.
 *
 * /edit -- Opens the specified .RDP connection file for editing.
 *
 * /migrate -- Migrates legacy connection files that were created with
 * Client Connection Manager to new .RDP connection files.
 *
 * /? -- Lists these parameters.
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

int CommandLineParseArgumentsA(int argc, LPCSTR* argv, COMMAND_LINE_ARGUMENT_A* options,
		DWORD flags, COMMAND_LINE_FILTER_FN_A filter, void* context)
{
	int i, j;
	int length;
	BOOL match;
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

	for (i = 1; i < argc; i++)
	{
		if (filter)
			filter(i, argv[i], context);

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
		}
		else if ((sigil[0] == '+') && (flags & COMMAND_LINE_SIGIL_PLUS_MINUS))
		{
			sigil_length = 1;
		}
		else if ((sigil[0] == '-') && (flags & COMMAND_LINE_SIGIL_PLUS_MINUS))
		{
			sigil_length = 1;
		}
		else
		{
			continue;
		}

		if (sigil_length > 0)
		{
			if (length < (sigil_length + 1))
				return COMMAND_LINE_ERROR_NO_KEYWORD;

			keyword_index = sigil_index + sigil_length;
			keyword = (char*) &argv[i][keyword_index];

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

				options[j].Index = i;

				if (value && (options[j].Flags & COMMAND_LINE_VALUE_FLAG))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (!value && (options[j].Flags & COMMAND_LINE_VALUE_REQUIRED))
					return COMMAND_LINE_ERROR_MISSING_VALUE;

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
						if (sigil[0] == '+')
							options[j].Value = BoolValueTrue;
						else if (sigil[0] == '-')
							options[j].Value = BoolValueFalse;
						else
							options[j].Value = BoolValueTrue;

						options[j].Flags |= COMMAND_LINE_VALUE_PRESENT;
					}
				}

				if (options[j].Flags & COMMAND_LINE_PRINT)
						return COMMAND_LINE_STATUS_PRINT;
				else if (options[j].Flags & COMMAND_LINE_PRINT_HELP)
						return COMMAND_LINE_STATUS_PRINT_HELP;
				else if (options[j].Flags & COMMAND_LINE_PRINT_VERSION)
						return COMMAND_LINE_STATUS_PRINT_VERSION;
			}
		}
	}

	return 0;
}

int CommandLineParseArgumentsW(int argc, LPCWSTR* argv, COMMAND_LINE_ARGUMENT_W* options,
		DWORD flags, COMMAND_LINE_FILTER_FN_W filter, void* context)
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
