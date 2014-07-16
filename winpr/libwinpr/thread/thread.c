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

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include <errno.h>

#include <winpr/collections.h>

#include "thread.h"

#include "../handle/handle.h"

static pthread_once_t thread_initialized = PTHREAD_ONCE_INIT;

static HANDLE_CLOSE_CB _ThreadHandleCloseCb;
static wListDictionary *thread_list = NULL;

static BOOL ThreadCloseHandle(HANDLE handle);
static void cleanup_handle(void *obj);

static BOOL ThreadIsHandled(HANDLE handle)
{
	WINPR_THREAD *pThread = (WINPR_THREAD *)handle;

	if (!pThread || pThread->Type != HANDLE_TYPE_THREAD)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}


static void ThreadInitialize(void)
{
	_ThreadHandleCloseCb.IsHandled = ThreadIsHandled;
	_ThreadHandleCloseCb.CloseHandle = ThreadCloseHandle;
	RegisterHandleCloseCb(&_ThreadHandleCloseCb);
}

static void dump_thread(WINPR_THREAD *thread)
{
#if defined(WITH_DEBUG_THREADS) && defined(HAVE_EXECINFO_H)
	void *stack[20];
	fprintf(stderr, "[%s]: Called from:\n", __FUNCTION__);
	backtrace_symbols_fd(stack, 20, STDERR_FILENO);
	fprintf(stderr, "[%s]: Thread handle created still not closed!\n", __FUNCTION__);
	backtrace_symbols_fd(thread->create_stack, 20, STDERR_FILENO);

	if (thread->started)
		fprintf(stderr, "[%s]: Thread still running!\n", __FUNCTION__);
	else if (!thread->exit_stack)
		fprintf(stderr, "[%s]: Thread suspended.\n", __FUNCTION__);
	else
	{
		fprintf(stderr, "[%s]: Thread exited at:\n", __FUNCTION__);
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
	return status;
}

static BOOL reset_event(WINPR_THREAD *thread)
{
	int length;
	BOOL status = FALSE;
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
	return status;
}

static int thread_compare(void *a, void *b)
{
	pthread_t *p1 = a;
	pthread_t *p2 = b;
	int rc = pthread_equal(*p1, *p2);
	return rc;
}

/* Thread launcher function responsible for registering
 * cleanup handlers and calling pthread_exit, if not done
 * in thread function. */
static void *thread_launcher(void *arg)
{
	void *rc = NULL;
	WINPR_THREAD *thread = (WINPR_THREAD *)arg;

	if (!thread)
	{
		fprintf(stderr, "[%s]: Called with invalid argument %p\n", __FUNCTION__, arg);
		goto exit;
	}
	else
	{
		void *(*fkt)(void *) = (void *)thread->lpStartAddress;

		if (!fkt)
		{
			fprintf(stderr, "[%s]: Thread function argument is %p\n", fkt);
			goto exit;
		}

		rc = fkt(thread->lpParameter);
	}

exit:

	if (!thread->exited)
		thread->dwExitCode = (DWORD)(size_t)rc;

	set_event(thread);

	if (thread->detached || !thread->started)
		cleanup_handle(thread);

	pthread_exit(thread->dwExitCode);
	return rc;
}

static void winpr_StartThread(WINPR_THREAD *thread)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (thread->dwStackSize > 0)
		pthread_attr_setstacksize(&attr, (size_t) thread->dwStackSize);

	pthread_create(&thread->thread, &attr, thread_launcher, thread);
	pthread_attr_destroy(&attr);
	reset_event(thread);
	ListDictionary_Add(thread_list, &thread->thread, thread);
	dump_thread(thread);
	thread->started = TRUE;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
					LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	WINPR_THREAD *thread;
	thread = (WINPR_THREAD *) calloc(1, sizeof(WINPR_THREAD));

	if (!thread)
		return NULL;

	pthread_once(&thread_initialized, ThreadInitialize);
	thread->dwStackSize = dwStackSize;
	thread->lpParameter = lpParameter;
	thread->lpStartAddress = lpStartAddress;
	thread->lpThreadAttributes = lpThreadAttributes;
#if defined(WITH_DEBUG_THREADS) && defined(HAVE_EXECINFO_H)
	backtrace(thread->create_stack, 20);
	dump_thread(thread);
#endif
#ifdef HAVE_EVENTFD_H
	thread->pipe_fd[0] = eventfd(0, EFD_NONBLOCK);

	if (thread->pipe_fd[0] < 0)
	{
		fprintf(stderr, "[%s]: failed to create thread\n", __FUNCTION__);
		free(thread);
		return NULL;
	}

#else

	if (pipe(thread->pipe_fd) < 0)
	{
		fprintf(stderr, "[%s]: failed to create thread\n", __FUNCTION__);
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

void cleanup_handle(void *obj)
{
	WINPR_THREAD *thread = (WINPR_THREAD *)obj;
	int rc = pthread_mutex_destroy(&thread->mutex);

	if (rc)
		fprintf(stderr, "[%s]: failed to destroy mutex [%d] %s (%d)\n",
				__FUNCTION__,  rc, strerror(errno), errno);

	if (thread->pipe_fd[0])
		close(thread->pipe_fd[0]);

	if (thread->pipe_fd[1])
		close(thread->pipe_fd[1]);

	if (thread_list && ListDictionary_Contains(thread_list, &thread->thread))
		ListDictionary_Remove(thread_list, &thread->thread);

	free(thread);
}

BOOL ThreadCloseHandle(HANDLE handle)
{
	WINPR_THREAD *thread = (WINPR_THREAD *)handle;

	if (!thread_list)
	{
		fprintf(stderr, "[%s]: Thread list does not exist, check call!\n", __FUNCTION__);
		dump_thread(thread);
	}
	else if (!ListDictionary_Contains(thread_list, &thread->thread))
	{
		fprintf(stderr, "[%s]: Thread list does not contain this thread! check call!\n", __FUNCTION__);
		dump_thread(thread);
	}
	else
	{
		ListDictionary_Lock(thread_list);
		dump_thread(thread);

		if ((thread->started) && (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0))
		{
			fprintf(stderr, "[%s]: Thread running, setting to detached state!\n", __FUNCTION__);
			thread->detached = TRUE;
			pthread_detach(thread->thread);
		}
		else
			cleanup_handle(thread);

		ListDictionary_Unlock(thread_list);

		if (ListDictionary_Count(thread_list) < 1)
		{
			ListDictionary_Free(thread_list);
			thread_list = NULL;
		}
	}

	return TRUE;
}

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
						  LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	fprintf(stderr, "[%s]: not implemented\n", __FUNCTION__);
	return NULL;
}

VOID ExitThread(DWORD dwExitCode)
{
	pthread_t tid = pthread_self();

	if (NULL == thread_list)
	{
		fprintf(stderr, "[%s]: function called without existing thread list!\n", __FUNCTION__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		fprintf(stderr, "[%s]: function called, but no matching entry in thread list!\n", __FUNCTION__);
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
		thread->exited = TRUE;
		thread->dwExitCode = dwExitCode;
#if defined(WITH_DEBUG_THREADS) && defined(HAVE_EXECINFO_H)
		backtrace(thread->exit_stack, 20);
#endif
		ListDictionary_Unlock(thread_list);
	}
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
		fprintf(stderr, "[%s]: function called without existing thread list!\n", __FUNCTION__);
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		fprintf(stderr, "[%s]: function called, but no matching entry in thread list!\n", __FUNCTION__);
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
	pthread_t tid;
	tid = pthread_self();
	return (DWORD) tid;
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
		fprintf(stderr, "[%s]: Thread already started!\n", __FUNCTION__);

	pthread_mutex_unlock(&thread->mutex);
	return 0;
}

DWORD SuspendThread(HANDLE hThread)
{
	fprintf(stderr, "[%s]: Function not implemented!\n", __FUNCTION__);
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
	thread->exited = TRUE;
	thread->dwExitCode = dwExitCode;
	pthread_mutex_lock(&thread->mutex);
#ifndef ANDROID
	pthread_cancel(thread->thread);
#else
	fprintf(stderr, "[%s]: Function not supported on this platform!\n", __FUNCTION__);
#endif
	pthread_mutex_unlock(&thread->mutex);
	return TRUE;
}

#if defined(WITH_DEBUG_THREADS)
VOID DumpThreadHandles(void)
{
	void *stack[20];
#if defined(HAVE_EXECINFO_H)
	backtrace(stack, 20);
#endif
  fprintf(stderr, "---------------- %s ----------------------\n", __FUNCTION);
	fprintf(stderr, "---------------- Called from ----------------------------\n");
#if defined(HAVE_EXECINFO_H)
	backtrace_symbols_fd(stack, 20, STDERR_FILENO);
#endif
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
#if defined(HAVE_EXECINFO_H)
			backtrace_symbols_fd(thread->create_stack, 20, STDERR_FILENO);
#endif

			if (thread->started)
				fprintf(stderr, "Thread [%d] still running!\n",	x);
			else
			{
				fprintf(stderr, "Thread [%d] exited at:\n", x);
#if defined(HAVE_EXECINFO_H)
				backtrace_symbols_fd(thread->exit_stack, 20, STDERR_FILENO);
#endif
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

