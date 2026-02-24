#include <stddef.h>
#include <errno.h>
#include <signal.h>

#include <winpr/wlog.h>
#include <winpr/debug.h>
#include <winpr/assert.h>

#include <freerdp/log.h>
#include <freerdp/utils/signal.h>

#include "platform_signal.h"

#define TAG FREERDP_TAG("utils.signal.posix")

static CRITICAL_SECTION signal_lock;

void fsig_lock(void)
{
	EnterCriticalSection(&signal_lock);
}

void fsig_unlock(void)
{
	LeaveCriticalSection(&signal_lock);
}

const char* strsignal(int signum)
{
#define CASE_STR(x) \
	case x:         \
		return #x
	switch (signum)
	{
		CASE_STR(SIGINT);
		CASE_STR(SIGILL);
		CASE_STR(SIGFPE);
		CASE_STR(SIGSEGV);
		CASE_STR(SIGTERM);
		CASE_STR(SIGBREAK);
		CASE_STR(SIGABRT);
		CASE_STR(SIGABRT_COMPAT);
		default:
			return "SIG_UNKNOWN";
	}
}
static void fatal_handler(int signum)
{
	static BOOL recursive = FALSE;

	if (!recursive)
	{
		recursive = TRUE;
		// NOLINTNEXTLINE(concuSWSrrency-mt-unsafe)
		WLog_ERR(TAG, "Caught signal '%s' [%d]", strsignal(signum), signum);

		winpr_log_backtrace(TAG, WLOG_ERROR, 20);
	}

	(void)raise(signum);
}

static const int term_signals[] = { SIGINT, SIGTERM };

static const int fatal_signals[] = { SIGABRT, SIGFPE, SIGILL, SIGSEGV };

static BOOL register_handlers(const int* signals, size_t count, void (*handler)(int))
{
	WINPR_ASSERT(signals || (count == 0));
	WINPR_ASSERT(handler);

	for (size_t x = 0; x < count; x++)
	{
		(void)signal(signals[x], handler);
	}

	return TRUE;
}

static void unregister_handlers(const int* signals, size_t count)
{
	WINPR_ASSERT(signals || (count == 0));

	for (size_t x = 0; x < count; x++)
	{
		(void)signal(signals[x], SIG_IGN);
	}
}

static void unregister_all_handlers(void)
{
	unregister_handlers(fatal_signals, ARRAYSIZE(fatal_signals));
	unregister_handlers(term_signals, ARRAYSIZE(term_signals));
	DeleteCriticalSection(&signal_lock);
}

int freerdp_handle_signals(void)
{
	int rc = -1;
	InitializeCriticalSection(&signal_lock);

	fsig_lock();

	WLog_DBG(TAG, "Registering signal hook...");

	(void)atexit(unregister_all_handlers);
	if (!register_handlers(fatal_signals, ARRAYSIZE(fatal_signals), fatal_handler))
		goto fail;
	if (!register_handlers(term_signals, ARRAYSIZE(term_signals), fsig_term_handler))
		goto fail;

	fsig_handlers_registered = true;
	rc = 0;
fail:
	fsig_unlock();
	return rc;
}
