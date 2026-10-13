/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Native Windows child used by ProxyCommand transport tests
 * SPDX-License-Identifier: Apache-2.0
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static BOOL helper_write(const void* data, DWORD length)
{
	const BYTE* buffer = data;
	DWORD offset = 0;
	while (offset < length)
	{
		DWORD count = 0;
		const DWORD remaining = length - offset;
		/* Fragment the output to exercise transport buffering. */
		const DWORD request = remaining > 777 ? 777 : remaining;
		if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer + offset, request, &count, NULL) ||
		    !count)
			return FALSE;
		offset += count;
	}
	return TRUE;
}

static int helper_echo(void)
{
	BYTE buffer[4096];
	/* Ensure a producer can fill the pipe before we start consuming. */
	Sleep(100);
	for (;;)
	{
		DWORD count = 0;
		if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, sizeof(buffer), &count, NULL))
			return GetLastError() == ERROR_BROKEN_PIPE ? 0 : 2;
		if (!count)
			return 0;
		if (!helper_write(buffer, count))
			return 3;
	}
}

static int helper_tree(void)
{
	WCHAR executable[32768];
	WCHAR command[32768];
	STARTUPINFOW startup = { 0 };
	PROCESS_INFORMATION child = { 0 };
	DWORD pids[2];
	const DWORD length =
	    GetModuleFileNameW(NULL, executable, sizeof(executable) / sizeof(executable[0]));
	if (!length || length >= sizeof(executable) / sizeof(executable[0]) ||
	    length + 10 >= sizeof(command) / sizeof(command[0]))
		return 4;
	command[0] = L'"';
	memcpy(command + 1, executable, length * sizeof(WCHAR));
	wcscpy(command + length + 1, L"\" sleep");
	startup.cb = sizeof(startup);
	/* The child automatically belongs to the parent's ProxyCommand Job Object. */
	if (!CreateProcessW(executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child))
		return 5;
	pids[0] = GetCurrentProcessId();
	pids[1] = child.dwProcessId;
	CloseHandle(child.hThread);
	CloseHandle(child.hProcess);
	if (!helper_write(pids, sizeof(pids)))
		return 6;
	Sleep(60000);
	return 7;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		return 1;
	if (strcmp(argv[1], "echo") == 0)
		return helper_echo();
	if (strcmp(argv[1], "sleep") == 0)
	{
		Sleep(60000);
		return 0;
	}
	if (strcmp(argv[1], "exit") == 0)
		return 7;
	if (strcmp(argv[1], "tokens") == 0 && argc == 5)
	{
		char output[2048];
		int length = snprintf(output, sizeof(output), "%s:%s:%s", argv[2], argv[3], argv[4]);
		return length > 0 && (size_t)length < sizeof(output) && helper_write(output, (DWORD)length)
		           ? 0
		           : 8;
	}
	if (strcmp(argv[1], "tree") == 0)
		return helper_tree();
	return 9;
}
