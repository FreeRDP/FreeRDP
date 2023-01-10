/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
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

#include <winpr/config.h>

#include <winpr/windows.h>

#include <winpr/synch.h>

#include "../log.h"
#include "../thread/apc.h"
#include "../thread/thread.h"
#include "../synch/pollset.h"

#define TAG WINPR_TAG("synch.sleep")

#ifndef _WIN32

#include <time.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#ifdef WINPR_HAVE_UNISTD_H
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#include <unistd.h>
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

VOID Sleep(DWORD dwMilliseconds)
{
	usleep(dwMilliseconds * 1000);
}

DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
	WINPR_THREAD* thread = winpr_GetCurrentThread();
	WINPR_POLL_SET pollset;
	int status;
	DWORD ret = WAIT_FAILED;
	BOOL autoSignalled;

	if (!thread)
	{
		WLog_ERR(TAG, "unable to retrieve currentThread");
		return WAIT_FAILED;
	}

	/* treat re-entrancy if a completion is calling us */
	if (thread->apc.treatingCompletions)
		bAlertable = FALSE;

	if (!bAlertable || !thread->apc.length)
	{
		usleep(dwMilliseconds * 1000);
		return 0;
	}

	if (!pollset_init(&pollset, thread->apc.length))
	{
		WLog_ERR(TAG, "unable to initialize pollset");
		return WAIT_FAILED;
	}

	if (!apc_collectFds(thread, &pollset, &autoSignalled))
	{
		WLog_ERR(TAG, "unable to APC file descriptors");
		goto out;
	}

	if (!autoSignalled)
	{
		/* we poll and wait only if no APC member is ready */
		status = pollset_poll(&pollset, dwMilliseconds);
		if (status < 0)
		{
			WLog_ERR(TAG, "polling of apc fds failed");
			goto out;
		}
	}

	if (apc_executeCompletions(thread, &pollset, 0))
	{
		ret = WAIT_IO_COMPLETION;
	}
	else
	{
		/* according to the spec return value is 0 see
		 * https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepex*/
		ret = 0;
	}
out:
	pollset_uninit(&pollset);
	return ret;
}

#endif

VOID USleep(DWORD dwMicroseconds)
{
#ifndef _WIN32
	usleep(dwMicroseconds);
#else
	static LARGE_INTEGER freq = { 0 };
	LARGE_INTEGER t1 = { 0 };
	LARGE_INTEGER t2 = { 0 };

	QueryPerformanceCounter(&t1);

	if (freq.QuadPart == 0)
	{
		QueryPerformanceFrequency(&freq);
	}

	// in order to save cpu cyles we use Sleep() for the large share ...
	if (dwMicroseconds >= 1000)
	{
		Sleep(dwMicroseconds / 1000);
	}
	// ... and busy loop until all the requested micro seconds have passed
	do
	{
		QueryPerformanceCounter(&t2);
	} while (((t2.QuadPart - t1.QuadPart) * 1000000) / freq.QuadPart < dwMicroseconds);
#endif
}
