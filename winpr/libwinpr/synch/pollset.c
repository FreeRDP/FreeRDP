#include <errno.h>

#include "pollset.h"
#include <winpr/handle.h>
#include "../log.h"

#define TAG WINPR_TAG("sync.pollset")

#ifdef HAVE_POLL_H
static DWORD handle_mode_to_pollevent(ULONG mode)
{
	DWORD event = 0;

	if (mode & WINPR_FD_READ)
		event |= POLLIN;

	if (mode & WINPR_FD_WRITE)
		event |= POLLOUT;

	return event;
}
#endif

BOOL pollset_init(WINPR_POLL_SET* set, size_t nhandles)
{
#ifdef HAVE_POLL_H
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

	FD_ZERO(&set->rset);
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
#ifdef HAVE_POLL_H
	if (!set->isStatic)
		free(set->pollset);
#else
	free(set->fdIndex);
#endif
}

void pollset_reset(WINPR_POLL_SET* set)
{
#ifndef HAVE_POLL_H
	FD_ZERO(&set->rset);
	FD_ZERO(&set->wset);
	set->maxFd = 0;
	set->nread = set->nwrite = 0;
#endif
	set->fillIndex = 0;
}

BOOL pollset_add(WINPR_POLL_SET* set, int fd, ULONG mode)
{
#ifdef HAVE_POLL_H
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
		FD_SET(fd, &set->rset);
		set->nread++;
	}

	if (mode & WINPR_FD_WRITE)
	{
		FD_SET(fd, &set->wset);
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
	int ret;
#ifdef HAVE_POLL_H
	do
	{
		ret = poll(set->pollset, set->fillIndex, dwMilliseconds);
	} while (ret < 0 && errno == EINTR);
#else
	struct timeval staticTimeout;
	struct timeval* timeout;

	if (dwMilliseconds == INFINITE || dwMilliseconds == 0)
	{
		timeout = NULL;
	}
	else
	{
		timeout = &staticTimeout;
		timeout->tv_sec = dwMilliseconds / 1000;
		timeout->tv_usec = (dwMilliseconds % 1000) * 1000;
	}

	do
	{
		ret = select(set->maxFd + 1, set->nread ? &set->rset : NULL,
		             set->nwrite ? &set->wset : NULL, NULL, timeout);
	} while (ret < 0 && errno == EINTR);
#endif

	return ret;
}

BOOL pollset_isSignaled(WINPR_POLL_SET* set, size_t idx)
{
	if (idx > set->fillIndex)
	{
		WLog_ERR(TAG, "%s: index=%d out of pollset(fillIndex=%d)", __FUNCTION__, idx,
		         set->fillIndex);
		return FALSE;
	}

#ifdef HAVE_POLL_H
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
