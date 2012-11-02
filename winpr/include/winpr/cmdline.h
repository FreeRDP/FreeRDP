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

#define COMMAND_LINE_VALUE_FLAG			0x00000001
#define COMMAND_LINE_VALUE_REQUIRED		0x00000002
#define COMMAND_LINE_VALUE_OPTIONAL		0x00000004
#define COMMAND_LINE_VALUE_BOOLEAN		0x00000008

#define COMMAND_LINE_VALUE_PRESENT		0x80000001

struct _COMMAND_LINE_ARGUMENT_A
{
	LPCSTR Name;
	DWORD Flags;
	LPSTR Value;
};
typedef struct _COMMAND_LINE_ARGUMENT_A COMMAND_LINE_ARGUMENT_A;

struct _COMMAND_LINE_ARGUMENT_W
{
	LPCWSTR Name;
	DWORD Flags;
	LPWSTR Value;
};
typedef struct _COMMAND_LINE_ARGUMENT_W COMMAND_LINE_ARGUMENT_W;

#ifdef UNICODE
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_W
#else
#define COMMAND_LINE_ARGUMENT	COMMAND_LINE_ARGUMENT_A
#endif

WINPR_API int CommandLineParseArgumentsA(int argc, LPCSTR* argv, COMMAND_LINE_ARGUMENT_A* options, DWORD flags);
WINPR_API int CommandLineParseArgumentsW(int argc, LPCWSTR* argv, COMMAND_LINE_ARGUMENT_W* options, DWORD flags);

#ifdef UNICODE
#define CommandLineParseArguments	CommandLineParseArgumentsW
#else
#define CommandLineParseArguments	CommandLineParseArgumentsA
#endif

#endif /* WINPR_CMDLINE_H */

