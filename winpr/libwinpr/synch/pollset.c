#ifndef _WIN32
#include <errno.h>

#include "pollset.h"
#include <winpr/handle.h>
#include <winpr/sysinfo.h>
#include <winpr/assert.h>
#include "../log.h"

#define TAG WINPR_TAG("sync.pollset")

#ifdef WINPR_HAVE_POLL_H
static INT16 handle_mode_to_pollevent(ULONG mode)
{
	INT16 event = 0;

	if (mode & WINPR_FD_READ)
		event |= POLLIN;

	if (mode & WINPR_FD_WRITE)
		event |= POLLOUT;

	return event;
}
#endif

BOOL pollset_init(WINPR_POLL_SET* set, size_t nhandles)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	if (nhandles > MAXIMUM_WAIT_OBJECTS)
	{
		set->isStatic = FALSE;
		set->pollset = calloc(nhandles, sizeof(*set->pollset));
		if (!set->pollset)
			return FALSE;
	}
	else
	{
		set->pollset = set->staticSet;
		set->isStatic = TRUE;
	}
#else
	set->fdIndex = calloc(nhandles, sizeof(*set->fdIndex));
	if (!set->fdIndex)
		return FALSE;

	FD_ZERO(&set->rset_base);
	FD_ZERO(&set->rset);
	FD_ZERO(&set->wset_base);
	FD_ZERO(&set->wset);
	set->maxFd = 0;
	set->nread = set->nwrite = 0;
#endif

	set->size = nhandles;
	set->fillIndex = 0;
	return TRUE;
}

void pollset_uninit(WINPR_POLL_SET* set)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	if (!set->isStatic)
		free(set->pollset);
#else
	free(set->fdIndex);
#endif
}

void pollset_reset(WINPR_POLL_SET* set)
{
	WINPR_ASSERT(set);
#ifndef WINPR_HAVE_POLL_H
	FD_ZERO(&set->rset_base);
	FD_ZERO(&set->wset_base);
	set->maxFd = 0;
	set->nread = set->nwrite = 0;
#endif
	set->fillIndex = 0;
}

BOOL pollset_add(WINPR_POLL_SET* set, int fd, ULONG mode)
{
	WINPR_ASSERT(set);
#ifdef WINPR_HAVE_POLL_H
	struct pollfd* item;
	if (set->fillIndex == set->size)
		return FALSE;

	item = &set->pollset[set->fillIndex];
	item->fd = fd;
	item->revents = 0;
	item->events = handle_mode_to_pollevent(mode);
#else
	FdIndex* fdIndex = &set->fdIndex[set->fillIndex];
	if (mode & WINPR_FD_READ)
	{
		FD_SET(fd, &set->rset_base);
		set->nread++;
	}

	if (mode & WINPR_FD_WRITE)
	{
		FD_SET(fd, &set->wset_base);
		set->nwrite++;
	}

	if (fd > set->maxFd)
		set->maxFd = fd;

	fdIndex->fd = fd;
	fdIndex->mode = mode;
#endif
	set->fillIndex++;
	return TRUE;
}

int pollset_poll(WINPR_POLL_SET* set, DWORD dwMilliseconds)
{
	WINPR_ASSERT(set);
	int ret = 0;
	UINT64 dueTime, now;

	now = GetTickCount64();
	if (dwMilliseconds == INFINITE)
		dueTime = 0xFFFFFFFFFFFFFFFF;
	else
		dueTime = now + dwMilliseconds;

#ifdef WINPR_HAVE_POLL_H
	int timeout;

	do
	{
		if (dwMilliseconds == INFINITE)
			timeout = -1;
		else
			timeout = (int)(dueTime - now);

		ret = poll(set->pollset, set->fillIndex, timeout);
		if (ret >= 0)
			return ret;

		if (errno != EINTR)
			return -1;

		now = GetTickCount64();
	} while (now < dueTime);

#else
	do
	{
		struct timeval staticTimeout;
		struct timeval* timeout;

		fd_set* rset = NULL;
		fd_set* wset = NULL;

		if (dwMilliseconds == INFINITE)
		{
			timeout = NULL;
		}
		else
		{
			long waitTime = (long)(dueTime - now);

			timeout = &staticTimeout;
			timeout->tv_sec = waitTime / 1000;
			timeout->tv_usec = (waitTime % 1000) * 1000;
		}

		if (set->nread)
		{
			rset = &set->rset;
			memcpy(rset, &set->rset_base, sizeof(*rset));
		}

		if (set->nwrite)
		{
			wset = &set->wset;
			memcpy(wset, &set->wset_base, sizeof(*wset));
		}

		ret = select(set->maxFd + 1, rset, wset, NULL, timeout);
		if (ret >= 0)
			return ret;

		if (errno != EINTR)
			return -1;

		now = GetTickCount64();

	} while (now < dueTime);

	FD_ZERO(&set->rset);
	FD_ZERO(&set->wset);
#endif

	return 0; /* timeout */
}

BOOL pollset_isSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "%s: index=%d out of pollset(fillIndex=%" PRIuz ")", __FUNCTION__, idx,
		         set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & set->pollset[idx].events);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	if ((fdIndex->mode & WINPR_FD_READ) && FD_ISSET(fdIndex->fd, &set->rset))
		return TRUE;

	if ((fdIndex->mode & WINPR_FD_WRITE) && FD_ISSET(fdIndex->fd, &set->wset))
		return TRUE;

	return FALSE;
#endif
}

BOOL pollset_isReadSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "%s: index=%d out of pollset(fillIndex=%" PRIuz ")", __FUNCTION__, idx,
		         set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & POLLIN);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	return FD_ISSET(fdIndex->fd, &set->rset);
#endif
}

BOOL pollset_isWriteSignaled(WINPR_POLL_SET* set, size_t idx)
{
	WINPR_ASSERT(set);

	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "%s: index=%d out of pollset(fillIndex=%" PRIuz ")", __FUNCTION__, idx,
		         set->fillIndex);
		return FALSE;
	}

#ifdef WINPR_HAVE_POLL_H
	return !!(set->pollset[idx].revents & POLLOUT);
#else
	FdIndex* fdIndex = &set->fdIndex[idx];
	if (fdIndex->fd < 0)
		return FALSE;

	return FD_ISSET(fdIndex->fd, &set->wset);
#endif
}

#endif
