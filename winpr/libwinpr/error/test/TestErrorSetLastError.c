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
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#include <winpr/error.h>

static int status = 0;

LONG *pLoopCount = NULL;
BOOL bStopTest = FALSE;

static DWORD WINAPI test_error_thread(LPVOID arg)
{
	int id;
	DWORD dwErrorSet;
	DWORD dwErrorGet;

	id = (int) (size_t) arg;

	do {
		dwErrorSet = (DWORD)rand();
		SetLastError(dwErrorSet);
		if ((dwErrorGet = GetLastError()) != dwErrorSet)
		{
			printf("GetLastError() failure (thread %d): Expected: 0x%08"PRIX32", Actual: 0x%08"PRIX32"\n",
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
	DWORD error;
	HANDLE threads[4];
	int i;

	/* We must initialize WLog here. It will check for settings
	 * in the environment and if the variables are not set, the last
	 * error state is changed... */
	WLog_GetRoot();

	SetLastError(ERROR_ACCESS_DENIED);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%08X, Actual: 0x%08"PRIX32"\n",
				ERROR_ACCESS_DENIED, error);
		return -1;
	}

	pLoopCount = _aligned_malloc(sizeof(LONG), sizeof(LONG));
	if (!pLoopCount)
	{
		printf("Unable to allocate memory\n");
		return -1;
	}
	*pLoopCount = 0;

	for (i = 0; i < 4; i++)
	{
		if (!(threads[i] = CreateThread(NULL, 0, test_error_thread, (void*) (size_t) 0, 0, NULL)))
		{
			printf("Failed to create thread #%d\n", i);
			return -1;
		}
	}

	// let the threads run for at least 2 seconds
	Sleep(2000);
	bStopTest = TRUE;

	WaitForSingleObject(threads[0], INFINITE);
	WaitForSingleObject(threads[1], INFINITE);
	WaitForSingleObject(threads[2], INFINITE);
	WaitForSingleObject(threads[3], INFINITE);

	CloseHandle(threads[0]);
	CloseHandle(threads[1]);
	CloseHandle(threads[2]);
	CloseHandle(threads[3]);

	error = GetLastError();

	if (error != ERROR_ACCESS_DENIED)
	{
		printf("GetLastError() failure: Expected: 0x%08X, Actual: 0x%08"PRIX32"\n",
				ERROR_ACCESS_DENIED, error);
		return -1;
	}

	if (*pLoopCount < 4)
	{
		printf("Error: unexpected loop count\n");
		return -1;
	}

	printf("Completed %"PRId32" iterations.\n", *pLoopCount);

	return status;
}

