
/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include <winpr/file.h>
#include <winpr/handle.h>
#include <string.h>
#include <stdio.h>

int TestFileGetStdHandle(int argc, char* argv[])
{
	HANDLE stdout;
	char *buf = "happy happy";
	DWORD bytesWritten;

	stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdout == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "GetStdHandle failed ;(\n");
		return -1;
	}
	WriteFile(stdout, buf, strlen(buf), &bytesWritten, FALSE);
	if (bytesWritten != strlen(buf))
	{
		fprintf(stderr, "write failed\n");
		return -1;
	}
	CloseHandle(stdout);

	return 0;
}
