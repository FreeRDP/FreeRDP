/**
 * CTest for winpr's SetLastError/GetLastError
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#include <winpr/error.h>

static int status = 0;

static LONG* pLoopCount = NULL;
static BOOL bStopTest = FALSE;

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

static DWORD WINAPI test_error_thread(LPVOID arg)
{
	int id = 0;
	DWORD dwErrorSet = 0;
	DWORD dwErrorGet = 0;

	id = (int)(size_t)arg;

	do
	{
		dwErrorSet = prand(UINT32_MAX - 1) + 1;
		SetLastError(dwErrorSet);
		if ((dwErrorGet = GetLastError()) != dwErrorSet)
		{
			printf("GetLastError() failure (thread %d): Expected: 0x%08" PRIX32
			       ", Actual: 0x%08" PRIX32 "\n",
			       id, dwErrorSet, dwErrorGet);
			if (!status)
				status = -1;
			break;
		}
		InterlockedIncrement(pLoopCount);
	} while (!status && !bStopTest);

	return 0;
}

int TestErrorSetLastError(int argc, char* argv[])
{
	DWORD error = 0;
	HANDLE threads[4];

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* We must initialize WLog here. It will check for settings
	 * in the environment and if the variables are not set, the last
	 * error state is changed... */
	WLog_GetRoot();

	SetLastError(ERROR_ACCESS_DENIED);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%08X, Actual: 0x%08" PRIX32 "\n",
		       ERROR_ACCESS_DENIED, error);
		return -1;
	}

	pLoopCount = winpr_aligned_malloc(sizeof(LONG), sizeof(LONG));
	if (!pLoopCount)
	{
		printf("Unable to allocate memory\n");
		return -1;
	}
	*pLoopCount = 0;

	for (int i = 0; i < 4; i++)
	{
		if (!(threads[i] = CreateThread(NULL, 0, test_error_thread, (void*)(size_t)0, 0, NULL)))
		{
			printf("Failed to create thread #%d\n", i);
			return -1;
		}
	}

	// let the threads run for at least 0.2 seconds
	Sleep(200);
	bStopTest = TRUE;

	(void)WaitForSingleObject(threads[0], INFINITE);
	(void)WaitForSingleObject(threads[1], INFINITE);
	(void)WaitForSingleObject(threads[2], INFINITE);
	(void)WaitForSingleObject(threads[3], INFINITE);

	(void)CloseHandle(threads[0]);
	(void)CloseHandle(threads[1]);
	(void)CloseHandle(threads[2]);
	(void)CloseHandle(threads[3]);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%08X, Actual: 0x%08" PRIX32 "\n",
		       ERROR_ACCESS_DENIED, error);
		return -1;
	}

	if (*pLoopCount < 4)
	{
		printf("Error: unexpected loop count\n");
		return -1;
	}

	printf("Completed %" PRId32 " iterations.\n", *pLoopCount);
	winpr_aligned_free(pLoopCount);

	return status;
}
