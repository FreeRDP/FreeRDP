/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stddef.h>
#include <freerdp/utils/signal.h>
volatile sig_atomic_t terminal_needs_reset = 0;
int terminal_fildes = 0;
struct termios orig_flags;
struct termios new_flags;

static void fatal_handler(int signum)
{
	struct sigaction default_sigaction;

	if (terminal_needs_reset)
		tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags);

	default_sigaction.sa_handler = SIG_DFL;
	sigfillset(&(default_sigaction.sa_mask));
	default_sigaction.sa_flags = 0;

	sigaction(signum, &default_sigaction, NULL);
	raise(signum);
}

void freerdp_handle_signals(void)
{
	const int fatal_signals[] = {
		SIGABRT,
		SIGALRM,
		SIGBUS,
		SIGFPE,
		SIGHUP,
		SIGILL,
		SIGINT,
		SIGKILL,
		SIGPIPE,
		SIGQUIT,
		SIGSEGV,
		SIGTERM,
		SIGUSR1,
		SIGUSR2,
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
		SIGXCPU,
		SIGXFSZ
	};
	int signal_index;
	sigset_t orig_set;
	struct sigaction orig_sigaction, fatal_sigaction;

	sigfillset(&(fatal_sigaction.sa_mask));
	pthread_sigmask(SIG_BLOCK, &(fatal_sigaction.sa_mask), &orig_set);

	fatal_sigaction.sa_handler = fatal_handler;
	fatal_sigaction.sa_flags  = 0;

	for (signal_index = 0;
		signal_index < (sizeof fatal_signals / sizeof fatal_signals[0]);
		signal_index++)
		if (sigaction(fatal_signals[signal_index],
			NULL, &orig_sigaction) == 0)
			if (orig_sigaction.sa_handler != SIG_IGN)
				sigaction(fatal_signals[signal_index],
					&fatal_sigaction, NULL); 

	pthread_sigmask(SIG_SETMASK, &orig_set, NULL);
}
