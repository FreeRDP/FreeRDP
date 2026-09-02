/**
 * WinPR: Windows Portable Runtime
 * Test for winpr_exportHandleToString / winpr_importHandleFromString
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/handle.h>
#include <winpr/pipe.h>

/* exports hWrite, imports it back from the resulting string, then checks that writing through
 * the imported HANDLE is visible on hRead - i.e. that the import genuinely reconstructs a
 * working HANDLE, not just a value that happens to parse back. */
static BOOL test_roundtrip(HANDLE hRead, HANDLE hWrite)
{
	char formatted[256] = WINPR_C_ARRAY_INIT;
	if (!winpr_exportHandleToString(hWrite, "--fdIn={}", formatted, sizeof(formatted)))
	{
		printf("[roundtrip] winpr_exportHandleToString failed\n");
		return FALSE;
	}
	printf("[roundtrip] exported as '%s'\n", formatted);

	HANDLE imported = winpr_importHandleFromString(formatted, "--fdIn={}");
	if (!imported || (imported == INVALID_HANDLE_VALUE))
	{
		printf("[roundtrip] winpr_importHandleFromString failed\n");
		return FALSE;
	}

	const char payload[] = "PING";
	DWORD written = 0;
	if (!WriteFile(imported, payload, sizeof(payload) - 1, &written, nullptr) ||
	    (written != sizeof(payload) - 1))
	{
		printf("[roundtrip] WriteFile through the imported handle failed\n");
		(void)CloseHandle(imported);
		return FALSE;
	}

	char readBuf[16] = WINPR_C_ARRAY_INIT;
	DWORD nread = 0;
	if (!ReadFile(hRead, readBuf, sizeof(payload) - 1, &nread, nullptr) ||
	    (nread != sizeof(payload) - 1))
	{
		printf("[roundtrip] ReadFile on the original read end failed\n");
		(void)CloseHandle(imported);
		return FALSE;
	}

	if (memcmp(readBuf, payload, sizeof(payload) - 1) != 0)
	{
		printf("[roundtrip] data read back does not match what was written\n");
		(void)CloseHandle(imported);
		return FALSE;
	}

	(void)CloseHandle(imported);
	return TRUE;
}

/* a format string without a "{}" placeholder is invalid input - must fail rather than silently
 * dropping the handle value somewhere. */
static BOOL test_export_rejects_missing_placeholder(HANDLE hWrite)
{
	char buf[64] = WINPR_C_ARRAY_INIT;
	if (winpr_exportHandleToString(hWrite, "no-placeholder-here", buf, sizeof(buf)))
	{
		printf("[missing-placeholder] expected export to fail, but it succeeded\n");
		return FALSE;
	}
	return TRUE;
}

/* an output buffer too small to hold "prefix + value + suffix + NUL" must fail cleanly instead
 * of truncating or overflowing. */
static BOOL test_export_rejects_short_buffer(HANDLE hWrite)
{
	char tiny[2] = WINPR_C_ARRAY_INIT;
	if (winpr_exportHandleToString(hWrite, "--fdIn={}", tiny, sizeof(tiny)))
	{
		printf("[short-buffer] expected export to fail, but it succeeded\n");
		return FALSE;
	}
	return TRUE;
}

/* an input string that does not match the given format's literal prefix/suffix must be rejected
 * rather than mis-parsed. */
static BOOL test_import_rejects_format_mismatch(void)
{
	HANDLE h = winpr_importHandleFromString("--other=5", "--fdIn={}");
	if (h != INVALID_HANDLE_VALUE)
	{
		printf("[format-mismatch] expected INVALID_HANDLE_VALUE, got a handle\n");
		(void)CloseHandle(h);
		return FALSE;
	}
	return TRUE;
}

/* a non-numeric handle value between the matched prefix/suffix must be rejected. */
static BOOL test_import_rejects_garbage_value(void)
{
	HANDLE h = winpr_importHandleFromString("--fdIn=notanumber", "--fdIn={}");
	if (h != INVALID_HANDLE_VALUE)
	{
		printf("[garbage-value] expected INVALID_HANDLE_VALUE, got a handle\n");
		(void)CloseHandle(h);
		return FALSE;
	}
	return TRUE;
}

int TestHandleExportImportString(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	HANDLE hRead = nullptr;
	HANDLE hWrite = nullptr;
	if (!CreatePipe(&hRead, &hWrite, nullptr, 0))
	{
		printf("CreatePipe failed\n");
		return -1;
	}

	int rc = 0;
	if (!test_roundtrip(hRead, hWrite))
		rc = -1;
	if (!test_export_rejects_missing_placeholder(hWrite))
		rc = -1;
	if (!test_export_rejects_short_buffer(hWrite))
		rc = -1;
	if (!test_import_rejects_format_mismatch())
		rc = -1;
	if (!test_import_rejects_garbage_value())
		rc = -1;

	(void)CloseHandle(hRead);
	(void)CloseHandle(hWrite);
	return rc;
}
