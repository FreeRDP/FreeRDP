/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Signal handling
 *
 * Copyright 2011 Shea Levy <shea@shealevy.com>
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

#include <freerdp/config.h>

#include <stddef.h>
#include <errno.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/signal.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("utils.signal")

#ifdef _WIN32

int freerdp_handle_signals(void)
{
	errno = ENOSYS;
	return -1;
}

BOOL freerdp_add_signal_cleanup_handler(void* context, void (*fkt)(void*))
{
	return FALSE;
}

BOOL freerdp_del_signal_cleanup_handler(void* context, void (*fkt)(void*))
{
	return FALSE;
}
#else

#include <pthread.h>
#include <winpr/debug.h>

static BOOL handlers_registered = FALSE;
static pthread_mutex_t signal_handler_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
	void* context;
	freerdp_signal_handler_t handler;
} cleanup_handler_t;

static size_t cleanup_handler_count = 0;
static cleanup_handler_t cleanup_handlers[20] = { 0 };

static void lock(void)
{
	const int rc = pthread_mutex_lock(&signal_handler_lock);
	if (rc != 0)
		WLog_ERR(TAG, "[pthread_mutex_lock] failed with %s [%d]", strerror(rc), rc);
}

static void unlock(void)
{
	const int rc = pthread_mutex_unlock(&signal_handler_lock);
	if (rc != 0)
		WLog_ERR(TAG, "[pthread_mutex_lock] failed with %s [%d]", strerror(rc), rc);
}

static void term_handler(int signum)
{
	static BOOL recursive = FALSE;

	if (!recursive)
	{
		recursive = TRUE;
		WLog_ERR(TAG, "Caught signal '%s' [%d]", strsignal(signum), signum);
	}

	lock();
	for (size_t x = 0; x < cleanup_handler_count; x++)
	{
		const cleanup_handler_t empty = { 0 };
		cleanup_handler_t* cur = &cleanup_handlers[x];
		if (cur->handler)
			cur->handler(signum, strsignal(signum), cur->context);
		*cur = empty;
	}
	cleanup_handler_count = 0;
	unlock();
}

static void fatal_handler(int signum)
{
	struct sigaction default_sigaction;
	sigset_t this_mask;
	static BOOL recursive = FALSE;

	if (!recursive)
	{
		recursive = TRUE;
		WLog_ERR(TAG, "Caught signal '%s' [%d]", strsignal(signum), signum);

		winpr_log_backtrace(TAG, WLOG_ERROR, 20);
	}

	default_sigaction.sa_handler = SIG_DFL;
	sigfillset(&(default_sigaction.sa_mask));
	default_sigaction.sa_flags = 0;
	sigaction(signum, &default_sigaction, NULL);
	sigemptyset(&this_mask);
	sigaddset(&this_mask, signum);
	pthread_sigmask(SIG_UNBLOCK, &this_mask, NULL);
	raise(signum);
}

static const int term_signals[] = { SIGINT, SIGKILL, SIGQUIT, SIGSTOP, SIGTERM };

static const int fatal_signals[] = { SIGABRT,   SIGALRM, SIGBUS,  SIGFPE,  SIGHUP,  SIGILL,
	                                 SIGSEGV,   SIGTSTP, SIGTTIN, SIGTTOU, SIGUSR1, SIGUSR2,
#ifdef SIGPOLL
	                                 SIGPOLL,
#endif
#ifdef SIGPROF
	                                 SIGPROF,
#endif
#ifdef SIGSYS
	                                 SIGSYS,
#endif
	                                 SIGTRAP,
#ifdef SIGVTALRM
	                                 SIGVTALRM,
#endif
	                                 SIGXCPU,   SIGXFSZ };

static BOOL register_handlers(const int* signals, size_t count, void (*handler)(int))
{
	WINPR_ASSERT(signals || (count == 0));
	WINPR_ASSERT(handler);

	sigset_t orig_set = { 0 };
	struct sigaction saction = { 0 };

	pthread_sigmask(SIG_BLOCK, &(saction.sa_mask), &orig_set);

	sigfillset(&(saction.sa_mask));
	sigdelset(&(saction.sa_mask), SIGCONT);

	saction.sa_handler = handler;
	saction.sa_flags = 0;

	for (size_t x = 0; x < count; x++)
	{
		struct sigaction orig_sigaction = { 0 };
		if (sigaction(signals[x], NULL, &orig_sigaction) == 0)
		{
			if (orig_sigaction.sa_handler != SIG_IGN)
			{
				sigaction(signals[x], &saction, NULL);
			}
		}
	}

	pthread_sigmask(SIG_SETMASK, &orig_set, NULL);

	return TRUE;
}

int freerdp_handle_signals(void)
{
	int rc = -1;

	lock();

	WLog_DBG(TAG, "Registering signal hook...");

	if (!register_handlers(fatal_signals, ARRAYSIZE(fatal_signals), fatal_handler))
		goto fail;
	if (!register_handlers(term_signals, ARRAYSIZE(term_signals), term_handler))
		goto fail;

	/* Ignore SIGPIPE signal. */
	signal(SIGPIPE, SIG_IGN);
	handlers_registered = TRUE;
	rc = 0;
fail:
	unlock();
	return rc;
}

BOOL freerdp_add_signal_cleanup_handler(void* context, freerdp_signal_handler_t handler)
{
	BOOL rc = FALSE;
	lock();
	if (handlers_registered)
	{
		if (cleanup_handler_count < ARRAYSIZE(cleanup_handlers))
		{
			cleanup_handler_t* cur = &cleanup_handlers[cleanup_handler_count++];
			cur->context = context;
			cur->handler = handler;
		}
		else
			WLog_WARN(TAG, "Failed to register cleanup handler, only %" PRIuz " handlers supported",
			          ARRAYSIZE(cleanup_handlers));
	}
	rc = TRUE;
	unlock();
	return rc;
}

BOOL freerdp_del_signal_cleanup_handler(void* context, freerdp_signal_handler_t handler)
{
	BOOL rc = FALSE;
	lock();
	if (handlers_registered)
	{
		for (size_t x = 0; x < cleanup_handler_count; x++)
		{
			cleanup_handler_t* cur = &cleanup_handlers[x];
			if ((cur->context == context) && (cur->handler == handler))
			{
				const cleanup_handler_t empty = { 0 };
				for (size_t y = x + 1; y < cleanup_handler_count - 1; y++)
				{
					*cur++ = cleanup_handlers[y];
				}

				*cur = empty;
				cleanup_handler_count--;
				break;
			}
		}
	}
	rc = TRUE;
	unlock();
	return rc;
}

#endif
