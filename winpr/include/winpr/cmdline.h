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
#define COMMAND_LINE_VALUE_BOOLEAN		0x00000008

#define COMMAND_LINE_ALIAS			0x00000100

/* Command-Line Argument Output Flags */

#define COMMAND_LINE_ARGUMENT_PRESENT		0x80000000
#define COMMAND_LINE_VALUE_PRESENT		0x40000000

/* Command-Line Parsing Flags */

#define COMMAND_LINE_SIGIL_SLASH		0x00000001
#define COMMAND_LINE_SIGIL_DASH			0x00000002
#define COMMAND_LINE_SIGIL_DOUBLE_DASH		0x00000004
#define COMMAND_LINE_SIGIL_PLUS_MINUS		0x00000008

#define COMMAND_LINE_SEPARATOR_COLON		0x00000100
#define COMMAND_LINE_SEPARATOR_EQUAL		0x00000200

/* Command-Line Parsing Error Codes */

#define COMMAND_LINE_ERROR_NO_KEYWORD		-1001
#define COMMAND_LINE_ERROR_UNEXPECTED_VALUE	-1002
#define COMMAND_LINE_ERROR_MISSING_VALUE	-1003
#define COMMAND_LINE_ERROR_4			-1004
#define COMMAND_LINE_ERROR_5			-1005
#define COMMAND_LINE_ERROR_6			-1006
#define COMMAND_LINE_ERROR_7			-1007
#define COMMAND_LINE_ERROR_8			-1008
#define COMMAND_LINE_ERROR_9			-1009
#define COMMAND_LINE_ERROR_10			-1010

typedef struct _COMMAND_LINE_ARGUMENT_A COMMAND_LINE_ARGUMENT_A;
typedef struct _COMMAND_LINE_ARGUMENT_W COMMAND_LINE_ARGUMENT_W;

struct _COMMAND_LINE_ARGUMENT_A
{
	LPCSTR Name;
	DWORD Flags;
	LPSTR Value;
	LPCSTR Alias;
};

struct _COMMAND_LINE_ARGUMENT_W
{
	LPCWSTR Name;
	DWORD Flags;
	LPWSTR Value;
	LPCWSTR Alias;
};

#ifdef UNICODE
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_W
#else
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_A
#endif

WINPR_API int CommandLineClearArgumentsA(COMMAND_LINE_ARGUMENT_A* options);
WINPR_API int CommandLineClearArgumentsW(COMMAND_LINE_ARGUMENT_W* options);

WINPR_API int CommandLineParseArgumentsA(int argc, LPCSTR* argv, COMMAND_LINE_ARGUMENT_A* options, DWORD flags);
WINPR_API int CommandLineParseArgumentsW(int argc, LPCWSTR* argv, COMMAND_LINE_ARGUMENT_W* options, DWORD flags);

WINPR_API COMMAND_LINE_ARGUMENT_A* CommandLineFindArgumentA(COMMAND_LINE_ARGUMENT_A* options, LPCSTR Name);
WINPR_API COMMAND_LINE_ARGUMENT_W* CommandLineFindArgumentW(COMMAND_LINE_ARGUMENT_W* options, LPCWSTR Name);

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

