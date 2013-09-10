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

#ifndef WINPR_CMDLINE_H
#define WINPR_CMDLINE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

/* Command-Line Argument Flags */

#define COMMAND_LINE_INPUT_FLAG_MASK		0x0000FFFF
#define COMMAND_LINE_OUTPUT_FLAG_MASK		0xFFFF0000

/* Command-Line Argument Input Flags */

#define COMMAND_LINE_VALUE_FLAG			0x00000001
#define COMMAND_LINE_VALUE_REQUIRED		0x00000002
#define COMMAND_LINE_VALUE_OPTIONAL		0x00000004
#define COMMAND_LINE_VALUE_BOOL			0x00000008

#define COMMAND_LINE_ADVANCED			0x00000100
#define COMMAND_LINE_PRINT			0x00000200
#define COMMAND_LINE_PRINT_HELP			0x00000400
#define COMMAND_LINE_PRINT_VERSION		0x00000800

/* Command-Line Argument Output Flags */

#define COMMAND_LINE_ARGUMENT_PRESENT		0x80000000
#define COMMAND_LINE_VALUE_PRESENT		0x40000000

/* Command-Line Parsing Flags */

#define COMMAND_LINE_SIGIL_NONE			0x00000001
#define COMMAND_LINE_SIGIL_SLASH		0x00000002
#define COMMAND_LINE_SIGIL_DASH			0x00000004
#define COMMAND_LINE_SIGIL_DOUBLE_DASH		0x00000008
#define COMMAND_LINE_SIGIL_PLUS_MINUS		0x00000010
#define COMMAND_LINE_SIGIL_ENABLE_DISABLE	0x00000020
#define COMMAND_LINE_SIGIL_NOT_ESCAPED	0x00000040

#define COMMAND_LINE_SEPARATOR_COLON		0x00000100
#define COMMAND_LINE_SEPARATOR_EQUAL		0x00000200
#define COMMAND_LINE_SEPARATOR_SPACE		0x00000400

/* Command-Line Parsing Error Codes */

#define COMMAND_LINE_ERROR			-1000
#define COMMAND_LINE_ERROR_NO_KEYWORD		-1001
#define COMMAND_LINE_ERROR_UNEXPECTED_VALUE	-1002
#define COMMAND_LINE_ERROR_MISSING_VALUE	-1003
#define COMMAND_LINE_ERROR_MISSING_ARGUMENT	-1004
#define COMMAND_LINE_ERROR_UNEXPECTED_SIGIL	-1005
#define COMMAND_LINE_ERROR_LAST			-1006

/* Command-Line Parsing Status Codes */

#define COMMAND_LINE_STATUS_PRINT		-2001
#define COMMAND_LINE_STATUS_PRINT_HELP		-2002
#define COMMAND_LINE_STATUS_PRINT_VERSION	-2003

/* Command-Line Macros */

#define CommandLineSwitchStart(_arg) if (0) { }
#define CommandLineSwitchCase(_arg, _name) else if (strcmp(_arg->Name, _name) == 0)
#define CommandLineSwitchDefault(_arg) else
#define CommandLineSwitchEnd(_arg)

#define BoolValueTrue		((LPSTR) 1)
#define BoolValueFalse		((LPSTR) 0)

typedef struct _COMMAND_LINE_ARGUMENT_A COMMAND_LINE_ARGUMENT_A;
typedef struct _COMMAND_LINE_ARGUMENT_W COMMAND_LINE_ARGUMENT_W;

struct _COMMAND_LINE_ARGUMENT_A
{
	LPCSTR Name;
	DWORD Flags;
	LPSTR Format;
	LPSTR Default;
	LPSTR Value;
	LONG Index;
	LPCSTR Alias;
	LPCSTR Text;
};

struct _COMMAND_LINE_ARGUMENT_W
{
	LPCWSTR Name;
	DWORD Flags;
	LPSTR Format;
	LPWSTR Default;
	LPWSTR Value;
	LONG Index;
	LPCWSTR Alias;
	LPCWSTR Text;
};

#ifdef UNICODE
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_W
#else
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_A
#endif

typedef int (*COMMAND_LINE_PRE_FILTER_FN_A)(void* context, int index, int argc, LPCSTR* argv);
typedef int (*COMMAND_LINE_PRE_FILTER_FN_W)(void* context, int index, int argc, LPCWSTR* argv);

typedef int (*COMMAND_LINE_POST_FILTER_FN_A)(void* context, COMMAND_LINE_ARGUMENT_A* arg);
typedef int (*COMMAND_LINE_POST_FILTER_FN_W)(void* context, COMMAND_LINE_ARGUMENT_W* arg);

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API int CommandLineClearArgumentsA(COMMAND_LINE_ARGUMENT_A* options);
WINPR_API int CommandLineClearArgumentsW(COMMAND_LINE_ARGUMENT_W* options);

WINPR_API int CommandLineParseArgumentsA(int argc, LPCSTR* argv, COMMAND_LINE_ARGUMENT_A* options, DWORD flags,
		void* context, COMMAND_LINE_PRE_FILTER_FN_A preFilter, COMMAND_LINE_POST_FILTER_FN_A postFilter);
WINPR_API int CommandLineParseArgumentsW(int argc, LPCWSTR* argv, COMMAND_LINE_ARGUMENT_W* options, DWORD flags,
		void* context, COMMAND_LINE_PRE_FILTER_FN_W preFilter, COMMAND_LINE_POST_FILTER_FN_W postFilter);

WINPR_API COMMAND_LINE_ARGUMENT_A* CommandLineFindArgumentA(COMMAND_LINE_ARGUMENT_A* options, LPCSTR Name);
WINPR_API COMMAND_LINE_ARGUMENT_W* CommandLineFindArgumentW(COMMAND_LINE_ARGUMENT_W* options, LPCWSTR Name);

WINPR_API COMMAND_LINE_ARGUMENT_A* CommandLineFindNextArgumentA(COMMAND_LINE_ARGUMENT_A* argument);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CommandLineClearArguments	CommandLineClearArgumentsW
#define CommandLineParseArguments	CommandLineParseArgumentsW
#define CommandLineFindArgument		CommandLineFindArgumentW
#else
#define CommandLineClearArguments	CommandLineClearArgumentsA
#define CommandLineParseArguments	CommandLineParseArgumentsA
#define CommandLineFindArgument		CommandLineFindArgumentA
#endif

#endif /* WINPR_CMDLINE_H */

