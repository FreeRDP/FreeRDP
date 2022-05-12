/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * event implementation
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
#ifndef WINPR_LIBWINPR_SYNCH_EVENT_H_
#define WINPR_LIBWINPR_SYNCH_EVENT_H_

#include "../handle/handle.h"

#include <winpr/config.h>

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

struct winpr_event_impl
{
	int fds[2];
};

typedef struct winpr_event_impl WINPR_EVENT_IMPL;

struct winpr_event
{
	WINPR_HANDLE common;

	WINPR_EVENT_IMPL impl;
	BOOL bAttached;
	BOOL bManualReset;
	char* name;
#if defined(WITH_DEBUG_EVENTS)
	void* create_stack;
#endif
};
typedef struct winpr_event WINPR_EVENT;

BOOL winpr_event_init(WINPR_EVENT_IMPL* event);
void winpr_event_init_from_fd(WINPR_EVENT_IMPL* event, int fd);
BOOL winpr_event_set(WINPR_EVENT_IMPL* event);
BOOL winpr_event_reset(WINPR_EVENT_IMPL* event);
void winpr_event_uninit(WINPR_EVENT_IMPL* event);

#endif /* WINPR_LIBWINPR_SYNCH_EVENT_H_ */
