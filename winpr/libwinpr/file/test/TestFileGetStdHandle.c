
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

static BOOL write_std_handle(DWORD nStdHandle, const char* name)
{
	const char buf[] = "happy happy\n";
	DWORD bytesWritten = 0;

	HANDLE h = GetStdHandle(nStdHandle);
	if (h == INVALID_HANDLE_VALUE)
	{
		(void)fprintf(stderr, "GetStdHandle(%s) failed ;(\n", name);
		return FALSE;
	}
	if (!WriteFile(h, buf, strnlen(buf, sizeof(buf)), &bytesWritten, nullptr))
	{
		(void)fprintf(stderr, "WriteFile(%s) failed\n", name);
		return FALSE;
	}
	if (bytesWritten != strnlen(buf, sizeof(buf)))
	{
		(void)fprintf(stderr, "write to %s failed\n", name);
		return FALSE;
	}
	return TRUE;
}

int TestFileGetStdHandle(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* STD_INPUT_HANDLE is requested first on purpose: every standard stream needs its own
	 * handle, regardless of which one a process asks for first. */
	const HANDLE si = GetStdHandle(STD_INPUT_HANDLE);
	const HANDLE so = GetStdHandle(STD_OUTPUT_HANDLE);
	const HANDLE se = GetStdHandle(STD_ERROR_HANDLE);
	if ((si == INVALID_HANDLE_VALUE) || (so == INVALID_HANDLE_VALUE) ||
	    (se == INVALID_HANDLE_VALUE))
	{
		(void)fprintf(stderr, "GetStdHandle failed ;(\n");
		return -1;
	}
	if ((si == so) || (si == se) || (so == se))
	{
		(void)fprintf(stderr, "GetStdHandle returned one handle for different streams\n");
		return -1;
	}
	if ((GetStdHandle(STD_INPUT_HANDLE) != si) || (GetStdHandle(STD_OUTPUT_HANDLE) != so) ||
	    (GetStdHandle(STD_ERROR_HANDLE) != se))
	{
		(void)fprintf(stderr, "GetStdHandle returned a new handle on a repeated call\n");
		return -1;
	}

	if (!write_std_handle(STD_OUTPUT_HANDLE, "STD_OUTPUT_HANDLE") ||
	    !write_std_handle(STD_ERROR_HANDLE, "STD_ERROR_HANDLE"))
		return -1;

	/* the standard handles are process wide, closing them must not invalidate them */
	(void)CloseHandle(si);
	(void)CloseHandle(so);
	(void)CloseHandle(se);
	if (!write_std_handle(STD_OUTPUT_HANDLE, "STD_OUTPUT_HANDLE") ||
	    !write_std_handle(STD_ERROR_HANDLE, "STD_ERROR_HANDLE"))
		return -1;

	return 0;
}
