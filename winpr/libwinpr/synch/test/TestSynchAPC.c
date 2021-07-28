/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * TestSyncAPC
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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
#include <winpr/wtypes.h>
#include <winpr/thread.h>
#include <winpr/synch.h>

typedef struct
{
	BOOL error;
	BOOL called;
} UserApcArg;

static void CALLBACK userApc(ULONG_PTR arg)
{
	UserApcArg* userArg = (UserApcArg*)arg;
	userArg->called = TRUE;
}

static DWORD WINAPI uncleanThread(LPVOID lpThreadParameter)
{
	/* this thread post an APC that will never get executed */
	UserApcArg* userArg = (UserApcArg*)lpThreadParameter;
	if (!QueueUserAPC((PAPCFUNC)userApc, _GetCurrentThread(), (ULONG_PTR)lpThreadParameter))
	{
		userArg->error = TRUE;
		return 1;
	}

	return 0;
}

static DWORD WINAPI cleanThread(LPVOID lpThreadParameter)
{
	Sleep(500);

	SleepEx(500, TRUE);
	return 0;
}

typedef struct
{
	HANDLE timer1;
	DWORD timer1Calls;
	HANDLE timer2;
	DWORD timer2Calls;
	BOOL endTest;
} UncleanCloseData;

static VOID CALLBACK Timer1APCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	UncleanCloseData* data = (UncleanCloseData*)lpArg;
	data->timer1Calls++;
	CloseHandle(data->timer2);
	data->endTest = TRUE;
}

static VOID CALLBACK Timer2APCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	UncleanCloseData* data = (UncleanCloseData*)lpArg;
	data->timer2Calls++;
}

static DWORD /*WINAPI*/ closeHandleTest(LPVOID lpThreadParameter)
{
	LARGE_INTEGER dueTime;
	UncleanCloseData* data = (UncleanCloseData*)lpThreadParameter;
	data->endTest = FALSE;

	dueTime.QuadPart = -500;
	if (!SetWaitableTimer(data->timer1, &dueTime, 0, Timer1APCProc, lpThreadParameter, FALSE))
		return 1;

	dueTime.QuadPart = -900;
	if (!SetWaitableTimer(data->timer2, &dueTime, 0, Timer2APCProc, lpThreadParameter, FALSE))
		return 1;

	while (!data->endTest)
	{
		SleepEx(100, TRUE);
	}
	return 0;
}

int TestSynchAPC(int argc, char* argv[])
{
	HANDLE thread = NULL;
	UserApcArg userApcArg;

	userApcArg.error = FALSE;
	userApcArg.called = FALSE;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* first post an APC and check it is executed during a SleepEx */
	if (!QueueUserAPC((PAPCFUNC)userApc, _GetCurrentThread(), (ULONG_PTR)&userApcArg))
		return 1;

	if (SleepEx(100, FALSE) != 0)
		return 2;

	if (SleepEx(100, TRUE) != WAIT_IO_COMPLETION)
		return 3;

	if (!userApcArg.called)
		return 4;

	userApcArg.called = FALSE;

	/* test that the APC is cleaned up even when not called */
	thread = CreateThread(NULL, 0, uncleanThread, &userApcArg, 0, NULL);
	if (!thread)
		return 10;
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	if (userApcArg.called || userApcArg.error)
		return 11;

	/* test a remote APC queuing */
	thread = CreateThread(NULL, 0, cleanThread, &userApcArg, 0, NULL);
	if (!thread)
		return 20;

	if (!QueueUserAPC((PAPCFUNC)userApc, thread, (ULONG_PTR)&userApcArg))
		return 21;

	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	if (!userApcArg.called)
		return 22;

#if 0
	/* test cleanup of timer completions */
	memset(&uncleanCloseData, 0, sizeof(uncleanCloseData));
	uncleanCloseData.timer1 = CreateWaitableTimerA(NULL, FALSE, NULL);
	if (!uncleanCloseData.timer1)
		return 31;

	uncleanCloseData.timer2 = CreateWaitableTimerA(NULL, FALSE, NULL);
	if (!uncleanCloseData.timer2)
		return 32;

	thread = CreateThread(NULL, 0, closeHandleTest, &uncleanCloseData, 0, NULL);
	if (!thread)
		return 33;

	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	if (uncleanCloseData.timer1Calls != 1 || uncleanCloseData.timer2Calls != 0)
		return 34;
	CloseHandle(uncleanCloseData.timer1);
#endif
	return 0;
}
