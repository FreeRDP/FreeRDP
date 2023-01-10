/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * pollset
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
#ifndef WINPR_LIBWINPR_SYNCH_POLLSET_H_
#define WINPR_LIBWINPR_SYNCH_POLLSET_H_

#include <winpr/wtypes.h>
#include <winpr/synch.h>

#include <winpr/config.h>

#ifndef _WIN32

#ifdef WINPR_HAVE_POLL_H
#include <poll.h>
#else
#include <sys/select.h>

typedef struct
{
	int fd;
	ULONG mode;
} FdIndex;
#endif

struct winpr_poll_set
{
#ifdef WINPR_HAVE_POLL_H
	struct pollfd* pollset;
	struct pollfd staticSet[MAXIMUM_WAIT_OBJECTS];
	BOOL isStatic;
#else
	FdIndex* fdIndex;
	fd_set rset_base;
	fd_set rset;
	fd_set wset_base;
	fd_set wset;
	int nread, nwrite;
	int maxFd;
#endif
	size_t fillIndex;
	size_t size;
};

typedef struct winpr_poll_set WINPR_POLL_SET;

BOOL pollset_init(WINPR_POLL_SET* set, size_t nhandles);
void pollset_uninit(WINPR_POLL_SET* set);
void pollset_reset(WINPR_POLL_SET* set);
BOOL pollset_add(WINPR_POLL_SET* set, int fd, ULONG mode);
int pollset_poll(WINPR_POLL_SET* set, DWORD dwMilliseconds);

BOOL pollset_isSignaled(WINPR_POLL_SET* set, size_t idx);
BOOL pollset_isReadSignaled(WINPR_POLL_SET* set, size_t idx);
BOOL pollset_isWriteSignaled(WINPR_POLL_SET* set, size_t idx);

#endif

#endif /* WINPR_LIBWINPR_SYNCH_POLLSET_H_ */
