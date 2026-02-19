#include <pthread.h>
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>

#include <winpr/wlog.h>
#include <winpr/debug.h>

#include <freerdp/log.h>
#include <freerdp/utils/signal.h>

#include "signal.h"

#define TAG FREERDP_TAG("utils.signal.posix")

static pthread_mutex_t signal_handler_lock = PTHREAD_MUTEX_INITIALIZER;

void fsig_lock(void)
{
	const int rc = pthread_mutex_lock(&signal_handler_lock);
	if (rc != 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "[pthread_mutex_lock] failed with %s [%d]",
		         winpr_strerror(rc, ebuffer, sizeof(ebuffer)), rc);
	}
}

void fsig_unlock(void)
{
	const int rc = pthread_mutex_unlock(&signal_handler_lock);
	if (rc != 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "[pthread_mutex_lock] failed with %s [%d]",
		         winpr_strerror(rc, ebuffer, sizeof(ebuffer)), rc);
	}
}

static void fatal_handler(int signum)
{
	struct sigaction default_sigaction;
	sigset_t this_mask;
	static BOOL recursive = FALSE;

	if (!recursive)
	{
		recursive = TRUE;
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
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
	(void)raise(signum);
}

static const int term_signals[] = { SIGINT, SIGKILL, SIGQUIT, SIGSTOP, SIGTERM };

static const int fatal_signals[] = { SIGABRT,   SIGALRM, SIGBUS,  SIGFPE,  SIGHUP,
	                                 SIGILL,    SIGSEGV, SIGTTIN, SIGTTOU,
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

static void unregister_handlers(const int* signals, size_t count)
{
	WINPR_ASSERT(signals || (count == 0));

	sigset_t orig_set = { 0 };
	struct sigaction saction = { 0 };

	pthread_sigmask(SIG_BLOCK, &(saction.sa_mask), &orig_set);

	sigfillset(&(saction.sa_mask));
	sigdelset(&(saction.sa_mask), SIGCONT);

	saction.sa_handler = SIG_IGN;
	saction.sa_flags = 0;

	for (size_t x = 0; x < count; x++)
	{
		sigaction(signals[x], &saction, NULL);
	}

	pthread_sigmask(SIG_SETMASK, &orig_set, NULL);
}

static void unregister_all_handlers(void)
{
	unregister_handlers(fatal_signals, ARRAYSIZE(fatal_signals));
	unregister_handlers(term_signals, ARRAYSIZE(term_signals));
}

int freerdp_handle_signals(void)
{
	int rc = -1;

	fsig_lock();

	WLog_DBG(TAG, "Registering signal hook...");

	(void)atexit(unregister_all_handlers);
	if (!register_handlers(fatal_signals, ARRAYSIZE(fatal_signals), fatal_handler))
		goto fail;
	if (!register_handlers(term_signals, ARRAYSIZE(term_signals), fsig_term_handler))
		goto fail;

	/* Ignore SIGPIPE signal. */
	(void)signal(SIGPIPE, SIG_IGN);
	fsig_handlers_registered = TRUE;
	rc = 0;
fail:
	fsig_unlock();
	return rc;
}
