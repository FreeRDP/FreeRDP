/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Hewlett-Packard Development Company, L.P.
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
 *
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

#include <winpr/assert.h>

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

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#include <winpr/debug.h>

#include <errno.h>
#include <fcntl.h>

#include <winpr/collections.h>

#include "thread.h"
#include "apc.h"

#include "../handle/handle.h"
#include "../log.h"
#define TAG WINPR_TAG("thread")

static WINPR_THREAD mainThread;

#if defined(WITH_THREAD_LIST)
static wListDictionary* thread_list = NULL;
#endif

static BOOL ThreadCloseHandle(HANDLE handle);
static void cleanup_handle(void* obj);

static BOOL ThreadIsHandled(HANDLE handle)
{
	return WINPR_HANDLE_IS_HANDLED(handle, HANDLE_TYPE_THREAD, FALSE);
}

static int ThreadGetFd(HANDLE handle)
{
	WINPR_THREAD* pThread = (WINPR_THREAD*)handle;

	if (!ThreadIsHandled(handle))
		return -1;

	return pThread->event.fds[0];
}

#define run_mutex_init(fkt, mux, arg) run_mutex_init_(fkt, #fkt, mux, arg)
static BOOL run_mutex_init_(int (*fkt)(pthread_mutex_t*, const pthread_mutexattr_t*),
                            const char* name, pthread_mutex_t* mutex,
                            const pthread_mutexattr_t* mutexattr)
{
	int rc;

	WINPR_ASSERT(fkt);
	WINPR_ASSERT(mutex);

	rc = fkt(mutex, mutexattr);
	if (rc != 0)
	{
		WLog_WARN(TAG, "[%s] failed with [%s]", name, strerror(rc));
	}
	return rc == 0;
}

#define run_mutex_fkt(fkt, mux) run_mutex_fkt_(fkt, #fkt, mux)
static BOOL run_mutex_fkt_(int (*fkt)(pthread_mutex_t* mux), const char* name,
                           pthread_mutex_t* mutex)
{
	int rc;

	WINPR_ASSERT(fkt);
	WINPR_ASSERT(mutex);

	rc = fkt(mutex);
	if (rc != 0)
	{
		WLog_WARN(TAG, "[%s] failed with [%s]", name, strerror(rc));
	}
	return rc == 0;
}

#define run_cond_init(fkt, cond, arg) run_cond_init_(fkt, #fkt, cond, arg)
static BOOL run_cond_init_(int (*fkt)(pthread_cond_t*, const pthread_condattr_t*), const char* name,
                           pthread_cond_t* condition, const pthread_condattr_t* conditionattr)
{
	int rc;

	WINPR_ASSERT(fkt);
	WINPR_ASSERT(condition);

	rc = fkt(condition, conditionattr);
	if (rc != 0)
	{
		WLog_WARN(TAG, "[%s] failed with [%s]", name, strerror(rc));
	}
	return rc == 0;
}

#define run_cond_fkt(fkt, cond) run_cond_fkt_(fkt, #fkt, cond)
static BOOL run_cond_fkt_(int (*fkt)(pthread_cond_t* mux), const char* name,
                          pthread_cond_t* condition)
{
	int rc;

	WINPR_ASSERT(fkt);
	WINPR_ASSERT(condition);

	rc = fkt(condition);
	if (rc != 0)
	{
		WLog_WARN(TAG, "[%s] failed with [%s]", name, strerror(rc));
	}
	return rc == 0;
}

static int pthread_mutex_checked_unlock(pthread_mutex_t* mutex)
{
	WINPR_ASSERT(mutex);
	WINPR_ASSERT(pthread_mutex_trylock(mutex) == EBUSY);
	return pthread_mutex_unlock(mutex);
}

static BOOL mux_condition_bundle_init(mux_condition_bundle* bundle)
{
	WINPR_ASSERT(bundle);

	bundle->val = FALSE;
	if (!run_mutex_init(pthread_mutex_init, &bundle->mux, NULL))
		return FALSE;

	if (!run_cond_init(pthread_cond_init, &bundle->cond, NULL))
		return FALSE;
	return TRUE;
}

static void mux_condition_bundle_uninit(mux_condition_bundle* bundle)
{
	mux_condition_bundle empty = { 0 };

	WINPR_ASSERT(bundle);

	run_cond_fkt(pthread_cond_destroy, &bundle->cond);
	run_mutex_fkt(pthread_mutex_destroy, &bundle->mux);
	*bundle = empty;
}

static BOOL mux_condition_bundle_signal(mux_condition_bundle* bundle)
{
	BOOL rc = TRUE;
	WINPR_ASSERT(bundle);

	if (!run_mutex_fkt(pthread_mutex_lock, &bundle->mux))
		return FALSE;
	bundle->val = TRUE;
	if (!run_cond_fkt(pthread_cond_signal, &bundle->cond))
		rc = FALSE;
	if (!run_mutex_fkt(pthread_mutex_checked_unlock, &bundle->mux))
		rc = FALSE;
	return rc;
}

static BOOL mux_condition_bundle_lock(mux_condition_bundle* bundle)
{
	WINPR_ASSERT(bundle);
	return run_mutex_fkt(pthread_mutex_lock, &bundle->mux);
}

static BOOL mux_condition_bundle_unlock(mux_condition_bundle* bundle)
{
	WINPR_ASSERT(bundle);
	return run_mutex_fkt(pthread_mutex_checked_unlock, &bundle->mux);
}

static BOOL mux_condition_bundle_wait(mux_condition_bundle* bundle, const char* name)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(bundle);
	WINPR_ASSERT(name);
	WINPR_ASSERT(pthread_mutex_trylock(&bundle->mux) == EBUSY);

	while (!bundle->val)
	{
		int r = pthread_cond_wait(&bundle->cond, &bundle->mux);
		if (r != 0)
		{
			WLog_ERR(TAG, "failed to wait for %s [%s]", name, strerror(r));
			switch (r)
			{
				case ENOTRECOVERABLE:
				case EPERM:
				case ETIMEDOUT:
				case EINVAL:
					goto fail;

				default:
					break;
			}
		}
	}

	rc = bundle->val;

fail:
	return rc;
}

static BOOL signal_thread_ready(WINPR_THREAD* thread)
{
	WINPR_ASSERT(thread);

	return mux_condition_bundle_signal(&thread->isCreated);
}

static BOOL signal_thread_is_running(WINPR_THREAD* thread)
{
	WINPR_ASSERT(thread);

	return mux_condition_bundle_signal(&thread->isRunning);
}

static DWORD ThreadCleanupHandle(HANDLE handle)
{
	DWORD status = WAIT_FAILED;
	WINPR_THREAD* thread = (WINPR_THREAD*)handle;

	if (!ThreadIsHandled(handle))
		return WAIT_FAILED;

	if (!run_mutex_fkt(pthread_mutex_lock, &thread->mutex))
		return WAIT_FAILED;

	if (!thread->joined)
	{
		int rc = pthread_join(thread->thread, NULL);

		if (rc != 0)
		{
			WLog_ERR(TAG, "pthread_join failure: [%d] %s", rc, strerror(rc));
			goto fail;
		}
		else
			thread->joined = TRUE;
	}

	status = WAIT_OBJECT_0;

fail:
	if (!run_mutex_fkt(pthread_mutex_checked_unlock, &thread->mutex))
		return WAIT_FAILED;

	return status;
}

static HANDLE_OPS ops = { ThreadIsHandled,
	                      ThreadCloseHandle,
	                      ThreadGetFd,
	                      ThreadCleanupHandle,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

static void dump_thread(WINPR_THREAD* thread)
{
#if defined(WITH_DEBUG_THREADS)
	void* stack = winpr_backtrace(20);
	char** msg;
	size_t used, i;
	WLog_DBG(TAG, "Called from:");
	msg = winpr_backtrace_symbols(stack, &used);

	for (i = 0; i < used; i++)
		WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

	free(msg);
	winpr_backtrace_free(stack);
	WLog_DBG(TAG, "Thread handle created still not closed!");
	msg = winpr_backtrace_symbols(thread->create_stack, &used);

	for (i = 0; i < used; i++)
		WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

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
			WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

		free(msg);
	}
#else
	WINPR_UNUSED(thread);
#endif
}

/**
 * TODO: implement thread suspend/resume using pthreads
 * http://stackoverflow.com/questions/3140867/suspend-pthreads-without-using-condition
 */
static BOOL set_event(WINPR_THREAD* thread)
{
	return winpr_event_set(&thread->event);
}

static BOOL reset_event(WINPR_THREAD* thread)
{
	return winpr_event_reset(&thread->event);
}

#if defined(WITH_THREAD_LIST)
static BOOL thread_compare(const void* a, const void* b)
{
	const pthread_t* p1 = a;
	const pthread_t* p2 = b;
	BOOL rc = pthread_equal(*p1, *p2);
	return rc;
}
#endif

static INIT_ONCE threads_InitOnce = INIT_ONCE_STATIC_INIT;
static pthread_t mainThreadId;
static DWORD currentThreadTlsIndex = TLS_OUT_OF_INDEXES;

static BOOL initializeThreads(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
	if (!apc_init(&mainThread.apc))
	{
		WLog_ERR(TAG, "failed to initialize APC");
		goto out;
	}

	mainThread.common.Type = HANDLE_TYPE_THREAD;
	mainThreadId = pthread_self();

	currentThreadTlsIndex = TlsAlloc();
	if (currentThreadTlsIndex == TLS_OUT_OF_INDEXES)
	{
		WLog_ERR(TAG, "Major bug, unable to allocate a TLS value for currentThread");
	}

#if defined(WITH_THREAD_LIST)
	thread_list = ListDictionary_New(TRUE);

	if (!thread_list)
	{
		WLog_ERR(TAG, "Couldn't create global thread list");
		goto error_thread_list;
	}

	thread_list->objectKey.fnObjectEquals = thread_compare;
#endif

out:
	return TRUE;
}

static BOOL signal_and_wait_for_ready(WINPR_THREAD* thread)
{
	BOOL res = FALSE;

	WINPR_ASSERT(thread);

	if (!mux_condition_bundle_lock(&thread->isRunning))
		return FALSE;

	if (!signal_thread_ready(thread))
		goto fail;

	if (!mux_condition_bundle_wait(&thread->isRunning, "threadIsRunning"))
		goto fail;

#if defined(WITH_THREAD_LIST)
	if (!ListDictionary_Contains(thread_list, &thread->thread))
	{
		WLog_ERR(TAG, "Thread not in thread_list, startup failed!");
		goto fail;
	}
#endif

	res = TRUE;

fail:
	if (!mux_condition_bundle_unlock(&thread->isRunning))
		return FALSE;

	return res;
}

/* Thread launcher function responsible for registering
 * cleanup handlers and calling pthread_exit, if not done
 * in thread function. */
static void* thread_launcher(void* arg)
{
	DWORD rc = 0;
	WINPR_THREAD* thread = (WINPR_THREAD*)arg;
	LPTHREAD_START_ROUTINE fkt;

	if (!thread)
	{
		WLog_ERR(TAG, "Called with invalid argument %p", arg);
		goto exit;
	}

	if (!TlsSetValue(currentThreadTlsIndex, thread))
	{
		WLog_ERR(TAG, "thread %d, unable to set current thread value", pthread_self());
		goto exit;
	}

	if (!(fkt = thread->lpStartAddress))
	{
		WLog_ERR(TAG, "Thread function argument is %p", (void*)fkt);
		goto exit;
	}

	if (!signal_and_wait_for_ready(thread))
		goto exit;

	rc = fkt(thread->lpParameter);
exit:

	if (thread)
	{
		apc_cleanupThread(thread);

		if (!thread->exited)
			thread->dwExitCode = rc;

		set_event(thread);

		signal_thread_ready(thread);

		if (thread->detached || !thread->started)
			cleanup_handle(thread);
	}

	return NULL;
}

static BOOL winpr_StartThread(WINPR_THREAD* thread)
{
	BOOL rc = FALSE;
	BOOL locked = FALSE;
	pthread_attr_t attr = { 0 };

	if (!mux_condition_bundle_lock(&thread->isCreated))
		return FALSE;
	locked = TRUE;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (thread->dwStackSize > 0)
		pthread_attr_setstacksize(&attr, (size_t)thread->dwStackSize);

	thread->started = TRUE;
	reset_event(thread);

#if defined(WITH_THREAD_LIST)
	if (!ListDictionary_Add(thread_list, &thread->thread, thread))
	{
		WLog_ERR(TAG, "failed to add the thread to the thread list");
		goto error;
	}
#endif

	if (pthread_create(&thread->thread, &attr, thread_launcher, thread))
		goto error;

	if (!mux_condition_bundle_wait(&thread->isCreated, "threadIsCreated"))
		goto error;

	locked = FALSE;
	if (!mux_condition_bundle_unlock(&thread->isCreated))
		goto error;

	if (!signal_thread_is_running(thread))
	{
		WLog_ERR(TAG, "failed to signal the thread was ready");
		goto error;
	}

	rc = TRUE;
error:
	if (locked)
	{
		if (!mux_condition_bundle_unlock(&thread->isCreated))
			rc = FALSE;
	}

	pthread_attr_destroy(&attr);

	if (rc)
		dump_thread(thread);

	return rc;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                    DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	WINPR_THREAD* thread = (WINPR_THREAD*)calloc(1, sizeof(WINPR_THREAD));

	if (!thread)
		return NULL;

	thread->dwStackSize = dwStackSize;
	thread->lpParameter = lpParameter;
	thread->lpStartAddress = lpStartAddress;
	thread->lpThreadAttributes = lpThreadAttributes;
	thread->common.ops = &ops;
#if defined(WITH_DEBUG_THREADS)
	thread->create_stack = winpr_backtrace(20);
	dump_thread(thread);
#endif

	if (!winpr_event_init(&thread->event))
	{
		WLog_ERR(TAG, "failed to create event");
		goto fail;
	}

	if (!run_mutex_init(pthread_mutex_init, &thread->mutex, NULL))
	{
		WLog_ERR(TAG, "failed to initialize thread mutex");
		goto fail;
	}

	if (!apc_init(&thread->apc))
	{
		WLog_ERR(TAG, "failed to initialize APC");
		goto fail;
	}

	if (!mux_condition_bundle_init(&thread->isCreated))
		goto fail;
	if (!mux_condition_bundle_init(&thread->isRunning))
		goto fail;

	WINPR_HANDLE_SET_TYPE_AND_MODE(thread, HANDLE_TYPE_THREAD, WINPR_FD_READ);
	handle = (HANDLE)thread;

	InitOnceExecuteOnce(&threads_InitOnce, initializeThreads, NULL, NULL);

	if (!(dwCreationFlags & CREATE_SUSPENDED))
	{
		if (!winpr_StartThread(thread))
			goto fail;
	}
	else
	{
		if (!set_event(thread))
			goto fail;
	}

	return handle;
fail:
	cleanup_handle(thread);
	return NULL;
}

void cleanup_handle(void* obj)
{
	WINPR_THREAD* thread = (WINPR_THREAD*)obj;
	if (!thread)
		return;

	if (!apc_uninit(&thread->apc))
		WLog_ERR(TAG, "failed to destroy APC");

	mux_condition_bundle_uninit(&thread->isCreated);
	mux_condition_bundle_uninit(&thread->isRunning);
	run_mutex_fkt(pthread_mutex_destroy, &thread->mutex);

	winpr_event_uninit(&thread->event);

#if defined(WITH_THREAD_LIST)
	ListDictionary_Remove(thread_list, &thread->thread);
#endif
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
	WINPR_THREAD* thread = (WINPR_THREAD*)handle;

#if defined(WITH_THREAD_LIST)
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
#endif
		dump_thread(thread);

		if ((thread->started) && (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0))
		{
			WLog_DBG(TAG, "Thread running, setting to detached state!");
			thread->detached = TRUE;
			pthread_detach(thread->thread);
		}
		else
		{
			cleanup_handle(thread);
		}

#if defined(WITH_THREAD_LIST)
		ListDictionary_Unlock(thread_list);
	}
#endif

	return TRUE;
}

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                          SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress,
                          LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

VOID ExitThread(DWORD dwExitCode)
{
#if defined(WITH_THREAD_LIST)
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
		WINPR_ASSERT(thread);
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

		pthread_exit((void*)(size_t)rc);
	}
#else
	WINPR_UNUSED(dwExitCode);
#endif
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return FALSE;

	thread = (WINPR_THREAD*)Object;
	*lpExitCode = thread->dwExitCode;
	return TRUE;
}

WINPR_THREAD* winpr_GetCurrentThread(VOID)
{
	WINPR_THREAD* ret;

	InitOnceExecuteOnce(&threads_InitOnce, initializeThreads, NULL, NULL);
	if (mainThreadId == pthread_self())
		return (HANDLE)&mainThread;

	ret = TlsGetValue(currentThreadTlsIndex);
	if (!ret)
	{
		WLog_ERR(TAG, "function called, but no matching entry in thread list!");
#if defined(WITH_DEBUG_THREADS)
		DumpThreadHandles();
#endif
	}
	return ret;
}

HANDLE _GetCurrentThread(VOID)
{
	return (HANDLE)winpr_GetCurrentThread();
}

DWORD GetCurrentThreadId(VOID)
{
	pthread_t tid;
	tid = pthread_self();
	/* Since pthread_t can be 64-bits on some systems, take just the    */
	/* lower 32-bits of it for the thread ID returned by this function. */
	return (DWORD)tid & 0xffffffffUL;
}

typedef struct
{
	WINPR_APC_ITEM apc;
	PAPCFUNC completion;
	ULONG_PTR completionArg;
} UserApcItem;

static void userAPC(LPVOID arg)
{
	UserApcItem* userApc = (UserApcItem*)arg;

	userApc->completion(userApc->completionArg);

	userApc->apc.markedForRemove = TRUE;
}

DWORD QueueUserAPC(PAPCFUNC pfnAPC, HANDLE hThread, ULONG_PTR dwData)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_APC_ITEM* apc;
	UserApcItem* apcItem;

	if (!pfnAPC)
		return 1;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object) || Object->Type != HANDLE_TYPE_THREAD)
	{
		WLog_ERR(TAG, "hThread is not a thread");
		SetLastError(ERROR_INVALID_PARAMETER);
		return (DWORD)0;
	}

	apcItem = calloc(1, sizeof(*apcItem));
	if (!apcItem)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return (DWORD)0;
	}

	apc = &apcItem->apc;
	apc->type = APC_TYPE_USER;
	apc->markedForFree = TRUE;
	apc->alwaysSignaled = TRUE;
	apc->completion = userAPC;
	apc->completionArgs = apc;
	apcItem->completion = pfnAPC;
	apcItem->completionArg = dwData;
	apc_register(hThread, apc);
	return 1;
}

DWORD ResumeThread(HANDLE hThread)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return (DWORD)-1;

	thread = (WINPR_THREAD*)Object;

	if (!run_mutex_fkt(pthread_mutex_lock, &thread->mutex))
		return (DWORD)-1;

	if (!thread->started)
	{
		if (!winpr_StartThread(thread))
		{
			run_mutex_fkt(pthread_mutex_checked_unlock, &thread->mutex);
			return (DWORD)-1;
		}
	}
	else
		WLog_WARN(TAG, "Thread already started!");

	if (!run_mutex_fkt(pthread_mutex_checked_unlock, &thread->mutex))
		return (DWORD)-1;

	return 0;
}

DWORD SuspendThread(HANDLE hThread)
{
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return (DWORD)-1;
}

BOOL SwitchToThread(VOID)
{
	/**
	 * Note: on some operating systems sched_yield is a stub returning -1.
	 * usleep should at least trigger a context switch if any thread is waiting.
	 */
	if (sched_yield() != 0)
		usleep(1);

	return TRUE;
}

BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return FALSE;

	thread = (WINPR_THREAD*)Object;
	thread->exited = TRUE;
	thread->dwExitCode = dwExitCode;

	if (!run_mutex_fkt(pthread_mutex_lock, &thread->mutex))
		return FALSE;

#ifndef ANDROID
	pthread_cancel(thread->thread);
#else
	WLog_ERR(TAG, "Function not supported on this platform!");
#endif

	if (!run_mutex_fkt(pthread_mutex_checked_unlock, &thread->mutex))
		return FALSE;

	set_event(thread);
	return TRUE;
}

VOID DumpThreadHandles(void)
{
#if defined(WITH_DEBUG_THREADS)
	char** msg;
	size_t used, i;
	void* stack = winpr_backtrace(20);
	WLog_DBG(TAG, "---------------- Called from ----------------------------");
	msg = winpr_backtrace_symbols(stack, &used);

	for (i = 0; i < used; i++)
	{
		WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);
	}

	free(msg);
	winpr_backtrace_free(stack);
	WLog_DBG(TAG, "---------------- Start Dumping thread handles -----------");

#if defined(WITH_THREAD_LIST)
	if (!thread_list)
	{
		WLog_DBG(TAG, "All threads properly shut down and disposed of.");
	}
	else
	{
		ULONG_PTR* keys = NULL;
		ListDictionary_Lock(thread_list);
		int x, count = ListDictionary_GetKeys(thread_list, &keys);
		WLog_DBG(TAG, "Dumping %d elements", count);

		for (x = 0; x < count; x++)
		{
			WINPR_THREAD* thread = ListDictionary_GetItemValue(thread_list, (void*)keys[x]);
			WLog_DBG(TAG, "Thread [%d] handle created still not closed!", x);
			msg = winpr_backtrace_symbols(thread->create_stack, &used);

			for (i = 0; i < used; i++)
			{
				WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);
			}

			free(msg);

			if (thread->started)
			{
				WLog_DBG(TAG, "Thread [%d] still running!", x);
			}
			else
			{
				WLog_DBG(TAG, "Thread [%d] exited at:", x);
				msg = winpr_backtrace_symbols(thread->exit_stack, &used);

				for (i = 0; i < used; i++)
					WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

				free(msg);
			}
		}

		free(keys);
		ListDictionary_Unlock(thread_list);
	}
#endif

	WLog_DBG(TAG, "---------------- End Dumping thread handles -------------");
#endif
}
#endif
