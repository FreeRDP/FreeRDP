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

#include <winpr/debug.h>

#include <errno.h>
#include <fcntl.h>

#include <winpr/collections.h>

#include "thread.h"

#include "../handle/handle.h"
#include "../log.h"
#define TAG WINPR_TAG("thread")

static pthread_once_t thread_initialized = PTHREAD_ONCE_INIT;

static HANDLE_CLOSE_CB _ThreadHandleCloseCb;
static wListDictionary* thread_list = NULL;

static BOOL ThreadCloseHandle(HANDLE handle);
static void cleanup_handle(void* obj);

static BOOL ThreadIsHandled(HANDLE handle)
{
	WINPR_THREAD* pThread = (WINPR_THREAD*) handle;

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

static void dump_thread(WINPR_THREAD* thread)
{
#if defined(WITH_DEBUG_THREADS)
	void* stack = winpr_backtrace(20);
	char** msg;
	size_t used, i;
	WLog_DBG(TAG, "Called from:");
	msg = winpr_backtrace_symbols(stack, &used);

	for (i = 0; i < used; i++)
		WLog_DBG(TAG, "[%d]: %s", i, msg[i]);

	free(msg);
	winpr_backtrace_free(stack);
	WLog_DBG(TAG, "Thread handle created still not closed!");
	msg = winpr_backtrace_symbols(thread->create_stack, &used);

	for (i = 0; i < used; i++)
		WLog_DBG(TAG, "[%d]: %s", i, msg[i]);

	free(msg);

	if (thread->started)
	{
		WLog_DBG(TAG, "Thread still running!");
	}
	else if (!thread->exit_stack)
	{
		WLog_DBG(TAG, "Thread suspended.");
	}
	else
	{
		WLog_DBG(TAG, "Thread exited at:");
		msg = winpr_backtrace_symbols(thread->exit_stack, &used);

		for (i = 0; i < used; i++)
			WLog_DBG(TAG, "[%d]: %s", i, msg[i]);

		free(msg);
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

static BOOL thread_compare(void* a, void* b)
{
	pthread_t* p1 = a;
	pthread_t* p2 = b;
	BOOL rc = pthread_equal(*p1, *p2);
	return rc;
}

/* Thread launcher function responsible for registering
 * cleanup handlers and calling pthread_exit, if not done
 * in thread function. */
static void* thread_launcher(void* arg)
{
	DWORD res = -1;
	void* rc = NULL;
	WINPR_THREAD* thread = (WINPR_THREAD*) arg;

	if (!thread)
	{
		WLog_ERR(TAG, "Called with invalid argument %p", arg);
		goto exit;
	}
	else
	{
		void *(*fkt)(void*) = (void*) thread->lpStartAddress;

		if (!fkt)
		{
			WLog_ERR(TAG, "Thread function argument is %p", fkt);
			goto exit;
		}

		rc = fkt(thread->lpParameter);
	}

exit:

	if (thread)
	{
		if (!thread->exited)
			thread->dwExitCode = (DWORD)(size_t)rc;

		set_event(thread);

		res = thread->dwExitCode;
		if (thread->detached || !thread->started)
			cleanup_handle(thread);
	}
	pthread_exit((void*) (size_t) res);
	return rc;
}

static void winpr_StartThread(WINPR_THREAD *thread)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (thread->dwStackSize > 0)
		pthread_attr_setstacksize(&attr, (size_t) thread->dwStackSize);

	thread->started = TRUE;
	reset_event(thread);

	pthread_create(&thread->thread, &attr, thread_launcher, thread);
	ListDictionary_Add(thread_list, &thread->thread, thread);
	pthread_attr_destroy(&attr);
	dump_thread(thread);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
					LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	WINPR_THREAD* thread;

	thread = (WINPR_THREAD*) calloc(1, sizeof(WINPR_THREAD));

	if (!thread)
		return NULL;

	pthread_once(&thread_initialized, ThreadInitialize);
	thread->dwStackSize = dwStackSize;
	thread->lpParameter = lpParameter;
	thread->lpStartAddress = lpStartAddress;
	thread->lpThreadAttributes = lpThreadAttributes;

#if defined(WITH_DEBUG_THREADS)
	thread->create_stack = winpr_backtrace(20);
	dump_thread(thread);
#endif
	
#ifdef HAVE_EVENTFD_H
	thread->pipe_fd[0] = eventfd(0, EFD_NONBLOCK);

	if (thread->pipe_fd[0] < 0)
	{
		WLog_ERR(TAG, "failed to create thread");
		free(thread);
		return NULL;
	}
#else
	if (pipe(thread->pipe_fd) < 0)
	{
		WLog_ERR(TAG, "failed to create thread");
		free(thread);
		return NULL;
	}
	
	{
		int flags = fcntl(thread->pipe_fd[0], F_GETFL);
		fcntl(thread->pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
	}
#endif
	
	pthread_mutex_init(&thread->mutex, 0);
	WINPR_HANDLE_SET_TYPE(thread, HANDLE_TYPE_THREAD);
	handle = (HANDLE) thread;

	if (!thread_list)
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
	WINPR_THREAD* thread = (WINPR_THREAD*) obj;
	int rc = pthread_mutex_destroy(&thread->mutex);

	if (rc)
		WLog_ERR(TAG, "failed to destroy mutex [%d] %s (%d)",
				rc, strerror(errno), errno);

	if (thread->pipe_fd[0])
		close(thread->pipe_fd[0]);

	if (thread->pipe_fd[1])
		close(thread->pipe_fd[1]);

	if (thread_list && ListDictionary_Contains(thread_list, &thread->thread))
		ListDictionary_Remove(thread_list, &thread->thread);

#if defined(WITH_DEBUG_THREADS)

	if (thread->create_stack)
		winpr_backtrace_free(thread->create_stack);

	if (thread->exit_stack)
		winpr_backtrace_free(thread->exit_stack);

#endif
	free(thread);
}

BOOL ThreadCloseHandle(HANDLE handle)
{
	WINPR_THREAD* thread = (WINPR_THREAD*) handle;

	if (!thread_list)
	{
		WLog_ERR(TAG, "Thread list does not exist, check call!");
		dump_thread(thread);
	}
	else if (!ListDictionary_Contains(thread_list, &thread->thread))
	{
		WLog_ERR(TAG, "Thread list does not contain this thread! check call!");
		dump_thread(thread);
	}
	else
	{
		ListDictionary_Lock(thread_list);
		dump_thread(thread);

		if ((thread->started) && (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0))
		{
			WLog_ERR(TAG, "Thread running, setting to detached state!");
			thread->detached = TRUE;
			pthread_detach(thread->thread);
		}
		else
		{
			cleanup_handle(thread);
		}

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
	WLog_ERR(TAG, "not implemented");
	return NULL;
}

VOID ExitThread(DWORD dwExitCode)
{
	DWORD rc;
	pthread_t tid = pthread_self();

	if (!thread_list)
	{
		WLog_ERR(TAG, "function called without existing thread list!");
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
		pthread_exit(0);
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		WLog_ERR(TAG, "function called, but no matching entry in thread list!");
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
		pthread_exit(0);
	}
	else
	{
		WINPR_THREAD* thread;

		ListDictionary_Lock(thread_list);
		thread = ListDictionary_GetItemValue(thread_list, &tid);

		assert(thread);
		thread->exited = TRUE;
		thread->dwExitCode = dwExitCode;
#if defined(WITH_DEBUG_THREADS)
		thread->exit_stack = winpr_backtrace(20);
#endif
		ListDictionary_Unlock(thread_list);
		set_event(thread);

		rc = thread->dwExitCode;
		if (thread->detached || !thread->started)
			cleanup_handle(thread);

		pthread_exit((void*) (size_t) rc);
	}
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return FALSE;

	thread = (WINPR_THREAD*) Object;
	*lpExitCode = thread->dwExitCode;
	return TRUE;
}

HANDLE _GetCurrentThread(VOID)
{
	HANDLE hdl = NULL;
	pthread_t tid = pthread_self();

	if (!thread_list)
	{
		WLog_ERR(TAG, "function called without existing thread list!");
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	else if (!ListDictionary_Contains(thread_list, &tid))
	{
		WLog_ERR(TAG, "function called, but no matching entry in thread list!");
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

	thread = (WINPR_THREAD*) Object;
	pthread_mutex_lock(&thread->mutex);

	if (!thread->started)
		winpr_StartThread(thread);
	else
		WLog_WARN(TAG, "Thread already started!");

	pthread_mutex_unlock(&thread->mutex);
	return 0;
}

DWORD SuspendThread(HANDLE hThread)
{
	WLog_ERR(TAG, "Function not implemented!");
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
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return 0;

	thread = (WINPR_THREAD*) Object;
	thread->exited = TRUE;
	thread->dwExitCode = dwExitCode;
	pthread_mutex_lock(&thread->mutex);
#ifndef ANDROID
	pthread_cancel(thread->thread);
#else
	WLog_ERR(TAG, "Function not supported on this platform!");
#endif
	pthread_mutex_unlock(&thread->mutex);
	set_event(thread);
	return TRUE;
}

#if defined(WITH_DEBUG_THREADS)
VOID DumpThreadHandles(void)
{
	char** msg;
	size_t used, i;
	void* stack = winpr_backtrace(20);
	WLog_DBG(TAG, "---------------- Called from ----------------------------");
	msg = winpr_backtrace_symbols(stack, &used);

	for (i = 0; i < used; i++)
	{
		WLog_DBG(TAG, "[%d]: %s", i, msg[i]);
	}

	free(msg);
	winpr_backtrace_free(stack);
	WLog_DBG(TAG, "---------------- Start Dumping thread handles -----------");

	if (!thread_list)
	{
		WLog_DBG(TAG, "All threads properly shut down and disposed of.");
	}
	else
	{
		ULONG_PTR *keys = NULL;
		ListDictionary_Lock(thread_list);
		int x, count = ListDictionary_GetKeys(thread_list, &keys);
		WLog_DBG(TAG, "Dumping %d elements", count);

		for (x = 0; x < count; x++)
		{
			WINPR_THREAD* thread = ListDictionary_GetItemValue(thread_list, (void*) keys[x]);
			WLog_DBG(TAG, "Thread [%d] handle created still not closed!", x);
			msg = winpr_backtrace_symbols(thread->create_stack, &used);

			for (i = 0; i < used; i++)
			{
				WLog_DBG(TAG, "[%d]: %s", i, msg[i]);
			}

			free(msg);

			if (thread->started)
			{
				WLog_DBG(TAG, "Thread [%d] still running!",	x);
			}
			else
			{
				WLog_DBG(TAG, "Thread [%d] exited at:", x);
				msg = winpr_backtrace_symbols(thread->exit_stack, &used);

				for (i=0; i<used; i++)
					WLog_DBG(TAG, "[%d]: %s", i, msg[i]);

				free(msg);
			}
		}

		if (keys)
			free(keys);

		ListDictionary_Unlock(thread_list);
	}

	WLog_DBG(TAG, "---------------- End Dumping thread handles -------------");
}
#endif
#endif

