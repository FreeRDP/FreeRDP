/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Pool)
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

#ifndef WINPR_POOL_PRIVATE_H
#define WINPR_POOL_PRIVATE_H

#include <winpr/pool.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

struct _TP_CALLBACK_INSTANCE
{
	PTP_WORK Work;
};

struct _TP_POOL
{
	DWORD Minimum;
	DWORD Maximum;
	wArrayList* Threads;
	wQueue* PendingQueue;
	HANDLE TerminateEvent;
	wCountdownEvent* WorkComplete;
};

struct _TP_WORK
{
	PVOID CallbackParameter;
	PTP_WORK_CALLBACK WorkCallback;
	PTP_CALLBACK_ENVIRON CallbackEnvironment;
};

struct _TP_TIMER
{
	void* dummy;
};

struct _TP_WAIT
{
	void* dummy;
};

struct _TP_IO
{
	void* dummy;
};

struct _TP_CLEANUP_GROUP
{
#ifndef _WIN32
	wArrayList* groups;
	PTP_CALLBACK_ENVIRON env;
#else
	void* dummy;
#endif
};

PTP_POOL GetDefaultThreadpool(void);

#endif /* WINPR_POOL_PRIVATE_H */

