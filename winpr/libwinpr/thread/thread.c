/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <winpr/handle.h>

#include <winpr/thread.h>

/**
 * api-ms-win-core-processthreads-l1-1-1.dll
 *
 * CreateRemoteThread
 * CreateRemoteThreadEx
 * CreateThread
 * DeleteProcThreadAttributeList
 * ExitThread
 * FlushInstructionCache
 * FlushProcessWriteBuffers
 * GetCurrentThread
 * GetCurrentThreadId
 * GetCurrentThreadStackLimits
 * GetExitCodeThread
 * GetPriorityClass
 * GetStartupInfoW
 * GetThreadContext
 * GetThreadId
 * GetThreadIdealProcessorEx
 * GetThreadPriority
 * GetThreadPriorityBoost
 * GetThreadTimes
 * InitializeProcThreadAttributeList
 * OpenThread
 * OpenThreadToken
 * QueryProcessAffinityUpdateMode
 * QueueUserAPC
 * ResumeThread
 * SetPriorityClass
 * SetThreadContext
 * SetThreadPriority
 * SetThreadPriorityBoost
 * SetThreadStackGuarantee
 * SetThreadToken
 * SuspendThread
 * SwitchToThread
 * TerminateThread
 * UpdateProcThreadAttribute
 */

#ifndef _WIN32

#include <winpr/crt.h>
#include <winpr/platform.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_EVENTFD_H
#include <sys/eventfd.h>
#endif

#include <execinfo.h>

#include <errno.h>

#include <winpr/collections.h>

#include "thread.h"

#include "../handle/handle.h"


static wListDictionary *thread_list = NULL;

static void dump_thread(WINPR_THREAD *thread)
{
#if defined(WITH_DEBUG_THREADS)
  void *stack[20];

	fprintf(stderr, "Called from:\n");
backtrace_symbols_fd(stack, 20, STDERR_FILENO);
	fprintf(stderr, "Thread handle created still not closed!\n");
backtrace_symbols_fd(thread->create_stack, 20, STDERR_FILENO);

	if (thread->started)
		fprintf(stderr, "Thread still running!\n");
	else if (!thread->exit_stack)
		fprintf(stderr, "Thread suspended.\n");
	else
	{
		fprintf(stderr, "Thread exited at:\n");
backtrace_symbols_fd(thread->exit_stack, 20, STDERR_FILENO);
	}

#endif
}

/**
 * TODO: implement thread suspend/resume using pthreads
 * http://stackoverflow.com/questions/3140867/suspend-pthreads-without-using-condition
 */
static BOOL set_event(WINPR_THREAD *thread)
{
	int length;
	BOOL status = FALSE;
#ifdef HAVE_EVENTFD_H
	eventfd_t val = 1;

	do
	{
		length = eventfd_write(thread->pipe_fd[0], val);
	}
	while ((length < 0) && (errno == EINTR));

	status = (length == 0) ? TRUE : FALSE;
#else

	if (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0)
	{
		length = write(thread->pipe_fd[1], "-", 1);

		if (length == 1)
			status = TRUE;
	}
	else
	{
		status = TRUE;
	}

#endif
	thread->started = FALSE;
	return status;
}

static BOOL reset_event(WINPR_THREAD *thread)
{
	int length;
	BOOL status = FALSE;

	while (WaitForSingleObject(thread, 0) == WAIT_OBJECT_0)
	{
#ifdef HAVE_EVENTFD_H
		eventfd_t value;

		do
		{
			length = eventfd_read(thread->pipe_fd[0], &value);
		}
		while ((length < 0) && (errno == EINTR));

		if ((length > 0) && (!status))
			status = TRUE;

#else
		length = read(thread->pipe_fd[0], &length, 1);

		if ((length == 1) && (!status))
			status = TRUE;

#endif
	}

	thread->started = TRUE;
	return status;
}

static int thread_compare(void *a, void *b)
{
	pthread_t *p1 = a;
	pthread_t *p2 = b;
	int rc = pthread_equal(*p1, *p2);
	return rc;
}

void winpr_StartThread(WINPR_THREAD *thread)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (thread->dwStackSize > 0)
		pthread_attr_setstacksize(&attr, (size_t) thread->dwStackSize);

	pthread_create(&thread->thread, &attr, (pthread_start_routine) thread->lpStartAddress, thread->lpParameter);
	pthread_attr_destroy(&attr);
	reset_event(thread);
	ListDictionary_Add(thread_list, &thread->thread, thread);
	dump_thread(thread);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
					LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	WINPR_THREAD *thread;
	thread = (WINPR_THREAD *) calloc(1, sizeof(WINPR_THREAD));

	if (!thread)
		return NULL;

	thread->dwStackSize = dwStackSize;
	thread->lpParameter = lpParameter;
	thread->lpStartAddress = lpStartAddress;
	thread->lpThreadAttributes = lpThreadAttributes;
#if defined(WITH_DEBUG_THREADS)
	backtrace(thread->create_stack, 20);
	dump_thread(thread);
#endif
#ifdef HAVE_EVENTFD_H
	thread->pipe_fd[0] = eventfd(0, EFD_NONBLOCK);

	if (thread->pipe_fd[0] < 0)
	{
		fprintf(stderr, "[%s]: failed to create thread\n", __func__);
		free(thread);
		return NULL;
	}

#else

	if (pipe(thread->pipe_fd) < 0)
	{
		fprintf(stderr, "[%s]: failed to create thread\n", __func__);
		free(thread);
		return NULL;
	}

#endif
	pthread_mutex_init(&thread->mutex, 0);
	WINPR_HANDLE_SET_TYPE(thread, HANDLE_TYPE_THREAD);
	handle = (HANDLE) thread;

	if (NULL == thread_list)
	{
		thread_list = ListDictionary_New(TRUE);
		thread_list->objectKey.fnObjectEquals = thread_compare;
	}

	if (!(dwCreationFlags & CREATE_SUSPENDED))
		winpr_StartThread(thread);
	else
		set_event(thread);

	return handle;
}

void CloseThread(WINPR_THREAD *thread)
{
	if (!thread_list)
	{
		fprintf(stderr, "[%s]: Thread list does not exist, check call!\n", __func__);
		dump_thread(thread);
	}
	else if (!ListDictionary_Contains(thread_list, &thread->thread))
	{
		fprintf(stderr, "[%s]: Thread list does not contain this thread! check call!\n", __func__);
		dump_thread(thread);
	}
	else
	{
		dump_thread(thread);
		ListDictionary_Remove(thread_list, &thread->thread);

		if (ListDictionary_Count(thread_list) < 1)
		{
			ListDictionary_Free(thread_list);
			thread_list = NULL;
		}
	}

	int rc = pthread_mutex_destroy(&thread->mutex);

	if (rc)
	{
		fprintf(stderr, "[%s]: failed to destroy mutex [%d] %s (%d)\n", __func__,  rc, strerror(errno), errno);
	}

	if (thread->pipe_fd[0])
		close(thread->pipe_fd[0]);

	if (thread->pipe_fd[1])
		close(thread->pipe_fd[1]);

	free(thread);
}

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
						  LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	fprintf(stderr, "[%s]: not implemented\n", __func__);
	return NULL;
}

VOID ExitThread(DWORD dwExitCode)
{
	pthread_t tid = pthread_self();

	if (NULL == thread_list)
	{
		fprintf(stderr, "[%s]: function called without existing thread list!\n", __func__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		fprintf(stderr, "[%s]: function called, but no matching entry in thread list!\n", __func__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else
	{
		WINPR_THREAD *thread;
		ListDictionary_Lock(thread_list);
		thread = ListDictionary_GetItemValue(thread_list, &tid);
		assert(thread);
#if defined(WITH_DEBUG_THREADS)
		backtrace(thread->exit_stack, 20);
#endif
		set_event(thread);
		thread->dwExitCode = dwExitCode;
		ListDictionary_Unlock(thread_list);
	}

	pthread_exit((void *)(size_t) dwExitCode);
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD *thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return FALSE;

	thread = (WINPR_THREAD *) Object;
	*lpExitCode = thread->dwExitCode;
	return TRUE;
}

HANDLE _GetCurrentThread(VOID)
{
	HANDLE hdl = NULL;
	pthread_t tid = pthread_self();

	if (NULL == thread_list)
	{
		fprintf(stderr, "[%s]: function called without existing thread list!\n", __func__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		fprintf(stderr, "[%s]: function called, but no matching entry in thread list!\n", __func__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else
	{
		hdl = ListDictionary_GetItemValue(thread_list, &tid);
	}

	return hdl;
}

DWORD GetCurrentThreadId(VOID)
{
#if defined(__linux__) && !defined(__ANDROID__)
	pid_t tid;
	tid = syscall(SYS_gettid);
	return (DWORD) tid;
#else
	pthread_t tid;
	tid = pthread_self();
	return (DWORD) tid;
#endif
}

DWORD ResumeThread(HANDLE hThread)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD *thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return 0;

	thread = (WINPR_THREAD *) Object;
	pthread_mutex_lock(&thread->mutex);

	if (!thread->started)
		winpr_StartThread(thread);
	else
		fprintf(stderr, "[%s]: Thread already started!\n", __func__);

	pthread_mutex_unlock(&thread->mutex);
	return 0;
}

DWORD SuspendThread(HANDLE hThread)
{
	fprintf(stderr, "[%s]: Function not implemented!\n", __func__);
	return 0;
}

BOOL SwitchToThread(VOID)
{
	return TRUE;
}

BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD *thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return 0;

	thread = (WINPR_THREAD *) Object;
	pthread_mutex_lock(&thread->mutex);
#ifndef ANDROID
	pthread_cancel(thread->thread);
#else
	fprintf(stderr, "[%s]: Function not supported on this platform!\n", __func__);
#endif
	pthread_mutex_unlock(&thread->mutex);
	return TRUE;
}

#if defined(WITH_DEBUG_THREADS)
VOID DumpThreadHandles(void)
{
void *stack[20];
backtrace(stack, 20);
  fprintf(stderr, "---------------- Called from ----------------------------\n");
backtrace_symbols_fd(stack, 20, STDERR_FILENO);
	fprintf(stderr, "---------------- Start Dumping thread handles -----------\n");

	if (!thread_list)
		fprintf(stderr, "All threads properly shut down and disposed of.\n");
	else
	{
		ULONG_PTR *keys = NULL;
		ListDictionary_Lock(thread_list);
		int x, count = ListDictionary_GetKeys(thread_list, &keys);
		fprintf(stderr, "Dumping %d elements\n", count);

		for (x=0; x<count; x++)
		{
			WINPR_THREAD *thread = ListDictionary_GetItemValue(thread_list, (void *)keys[x]);
			fprintf(stderr, "Thread [%d] handle created still not closed!\n", x);
			backtrace_symbols_fd(thread->create_stack, 20, STDERR_FILENO);

			if (thread->started)
				fprintf(stderr, "Thread [%d] still running!\n",	x);
			else
			{
				fprintf(stderr, "Thread [%d] exited at:\n", x);
			backtrace_symbols_fd(thread->exit_stack, 20, STDERR_FILENO);
			}
		}

		if (keys)
			free(keys);

		ListDictionary_Unlock(thread_list);
	}

	fprintf(stderr, "---------------- End Dumping thread handles -------------\n");
}
#endif
#endif

