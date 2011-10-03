/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Utils Unit Tests
 *
 * Copyright 2011 Vic Lee, 2011 Shea Levy
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

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/utils/signal.h>

#include "test_utils.h"

int init_utils_suite(void)
{
	return 0;
}

int clean_utils_suite(void)
{
	return 0;
}

int add_utils_suite(void)
{
	add_test_suite(utils);

	add_test_function(mutex);
	add_test_function(semaphore);
	add_test_function(load_plugin);
	add_test_function(wait_obj);
	add_test_function(args);
	add_test_function(passphrase_read);
	add_test_function(handle_signals);

	return 0;
}

void test_mutex(void)
{
	freerdp_mutex mutex;

	mutex = freerdp_mutex_new();
	freerdp_mutex_lock(mutex);
	freerdp_mutex_unlock(mutex);
	freerdp_mutex_free(mutex);
}

void test_semaphore(void)
{
	freerdp_sem sem;

	sem = freerdp_sem_new(1);
	freerdp_sem_wait(sem);
	freerdp_sem_signal(sem);
	freerdp_sem_free(sem);
}

void test_load_plugin(void)
{
	void* entry;

#ifdef _WIN32
	/* untested */
	entry = freerdp_load_plugin("..\\channels\\cliprdr\\cliprdr", "VirtualChannelEntry");
#else
	entry = freerdp_load_plugin("../channels/cliprdr/cliprdr.so", "VirtualChannelEntry");
#endif
	CU_ASSERT(entry != NULL);
}

void test_wait_obj(void)
{
	struct wait_obj* wo;
	int set;

	wo = wait_obj_new();

	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 0);

	wait_obj_set(wo);
	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 1);

	wait_obj_clear(wo);
	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 0);

	wait_obj_select(&wo, 1, 1000);

	wait_obj_free(wo);
}

static int process_plugin_args(rdpSettings* settings, const char* name,
	RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	/*printf("load plugin: %s\n", name);*/
	return 1;
}

static int process_ui_args(rdpSettings* settings, const char* opt,
	const char* val, void* user_data)
{
	/*printf("ui arg: %s %s\n", opt, val);*/
	return 1;
}

void test_args(void)
{
	char* argv_c[] =
	{
		"freerdp", "-a", "8", "-u", "testuser", "-d", "testdomain", "-g", "640x480", "address1:3389",
		"freerdp", "-a", "16", "-u", "testuser", "-d", "testdomain", "-g", "1280x960", "address2:3390"
	};
	char** argv = argv_c;
	int argc = sizeof(argv_c) / sizeof(char*);
	int i;
	int c;
	rdpSettings* settings;

	i = 0;
	while (argc > 0)
	{
		settings = settings_new(NULL);

		i++;
		c = freerdp_parse_args(settings, argc, argv, process_plugin_args, NULL, process_ui_args, NULL);
		CU_ASSERT(c > 0);
		if (c == 0)
		{
			settings_free(settings);
			break;
		}
		CU_ASSERT(settings->color_depth == i * 8);
		CU_ASSERT(settings->width == i * 640);
		CU_ASSERT(settings->height == i * 480);
		CU_ASSERT(settings->port == i + 3388);

		settings_free(settings);
		argc -= c;
		argv += c;
	}
	CU_ASSERT(i == 2);
}

void passphrase_read_prompts_to_tty()
{
	static const int read_nbyte = 11;
	int masterfd;
	char* slavedevice;
	char read_buf[read_nbyte];
	fd_set fd_set_write;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			char buffer[password_size];
			int slavefd;
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			if ((slavefd = open(slavedevice, O_RDWR)) == 0)
				CU_FAIL_FATAL("Could not open slave end of pty");
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(masterfd);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			close(slavefd);
			exit(EXIT_SUCCESS);
		}
	}

	read_buf[read_nbyte - 1] = '\0';

	FD_ZERO(&fd_set_write);
	FD_SET(masterfd, &fd_set_write);
	if (select(masterfd + 1, NULL, &fd_set_write, NULL, NULL) == -1)
		CU_FAIL_FATAL("Master end of pty not writeable");
	if (read(masterfd, read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to slave end of pty");
	CU_ASSERT_STRING_EQUAL(read_buf, "Password: ");

	write(masterfd, "\n", (size_t) 2);
	close(masterfd);
	return;
}

void passphrase_read_reads_from_tty()
{
	static const int read_nbyte = 11;
	int masterfd;
	int pipe_ends[2];
	char* slavedevice;
	char read_buf[read_nbyte];
	fd_set fd_set_write;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	if (pipe(pipe_ends) != 0)
		CU_FAIL_FATAL("Could not create pipe");

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			char buffer[password_size];
			int slavefd;
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			if ((slavefd = open(slavedevice, O_RDWR)) == 0)
				CU_FAIL_FATAL("Could not open slave end of pty");
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(masterfd);
			close(pipe_ends[0]);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			write(pipe_ends[1], buffer, password_size);
			close(slavefd);
			close(pipe_ends[1]);
			exit(EXIT_SUCCESS);
		}
	}

	close(pipe_ends[1]);
	read_buf[read_nbyte - 1] = '\0';

	FD_ZERO(&fd_set_write);
	FD_SET(masterfd, &fd_set_write);
	if (select(masterfd + 1, NULL, &fd_set_write, NULL, NULL) == -1)
		CU_FAIL_FATAL("Master end of pty not writeable");
	if (read(masterfd, read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to slave end of pty");

	write(masterfd, "passw0rd\n", sizeof "passw0rd\n");
	if (read(pipe_ends[0], read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to pipe");
	CU_ASSERT_STRING_EQUAL(read_buf, "passw0rd");
	close(masterfd);
	close(pipe_ends[0]);
	return;
}

void passphrase_read_turns_off_echo_during_read()
{
	static const int read_nbyte = 11;
	int masterfd, slavefd;
	char* slavedevice;
	char read_buf[read_nbyte];
	fd_set fd_set_write;
	struct termios term_flags;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	slavefd = open(slavedevice, O_RDWR|O_NOCTTY);
	if (slavefd == -1)
		CU_FAIL_FATAL("Could not open slave end of pty");

	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	if (!(term_flags.c_lflag & ECHO))
	{
		term_flags.c_lflag |= ECHO;
		if (tcsetattr(slavefd, TCSANOW, &term_flags) != 0)
			CU_FAIL_FATAL("Could not turn ECHO on on slave pty");
	}

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			int child_slavefd;
			char buffer[password_size];
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			if ((child_slavefd = open(slavedevice, O_RDWR)) == 0)
				CU_FAIL_FATAL("Could not open slave end of pty");
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(masterfd);
			close(slavefd);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			close(child_slavefd);
			exit(EXIT_SUCCESS);
		}
	}

	read_buf[read_nbyte - 1] = '\0';

	FD_ZERO(&fd_set_write);
	FD_SET(masterfd, &fd_set_write);
	if (select(masterfd + 1, NULL, &fd_set_write, NULL, NULL) == -1)
		CU_FAIL_FATAL("Master end of pty not writeable");
	if (read(masterfd, read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to slave end of pty");

	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	CU_ASSERT(!(term_flags.c_lflag & ECHO))
	write(masterfd, "\n", (size_t) 2);
	close(masterfd);
	close(slavefd);
	return;
}

void passphrase_read_resets_terminal_after_read()
{
	static const int read_nbyte = 11;
	int masterfd, slavefd, status;
	char* slavedevice;
	char read_buf[read_nbyte];
	fd_set fd_set_write;
	struct termios term_flags;
	pid_t child;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	slavefd = open(slavedevice, O_RDWR|O_NOCTTY);
	if (slavefd == -1)
		CU_FAIL_FATAL("Could not open slave end of pty");

	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	if (!(term_flags.c_lflag & ECHO))
	{
		term_flags.c_lflag |= ECHO;
		if (tcsetattr(slavefd, TCSANOW, &term_flags) != 0)
			CU_FAIL_FATAL("Could not turn ECHO on on slave pty");
	}

	switch (child = fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			int child_slavefd;
			char buffer[password_size];
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			if ((child_slavefd = open(slavedevice, O_RDWR)) == 0)
				CU_FAIL_FATAL("Could not open slave end of pty");
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(masterfd);
			close(slavefd);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			close(child_slavefd);
			exit(EXIT_SUCCESS);
		}
	}

	read_buf[read_nbyte - 1] = '\0';

	FD_ZERO(&fd_set_write);
	FD_SET(masterfd, &fd_set_write);
	if (select(masterfd + 1, NULL, &fd_set_write, NULL, NULL) == -1)
		CU_FAIL_FATAL("Master end of pty not writeable");
	if (read(masterfd, read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to slave end of pty");

	write(masterfd, "\n", (size_t) 2);
	waitpid(child, &status, WUNTRACED);
	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	CU_ASSERT(term_flags.c_lflag & ECHO)
	close(masterfd);
	close(slavefd);
	return;
}

void passphrase_read_turns_on_newline_echo_during_read()
{
	static const int read_nbyte = 11;
	int masterfd, slavefd;
	char* slavedevice;
	char read_buf[read_nbyte];
	fd_set fd_set_write;
	struct termios term_flags;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	slavefd = open(slavedevice, O_RDWR|O_NOCTTY);
	if (slavefd == -1)
		CU_FAIL_FATAL("Could not open slave end of pty");

	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	if (term_flags.c_lflag & ECHONL)
	{
		term_flags.c_lflag &= ~ECHONL;
		if (tcsetattr(slavefd, TCSANOW, &term_flags) != 0)
			CU_FAIL_FATAL("Could not turn ECHO on on slave pty");
	}

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			int child_slavefd;
			char buffer[password_size];
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			if ((child_slavefd = open(slavedevice, O_RDWR)) == 0)
				CU_FAIL_FATAL("Could not open slave end of pty");
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(masterfd);
			close(slavefd);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			close(child_slavefd);
			exit(EXIT_SUCCESS);
		}
	}

	read_buf[read_nbyte - 1] = '\0';

	FD_ZERO(&fd_set_write);
	FD_SET(masterfd, &fd_set_write);
	if (select(masterfd + 1, NULL, &fd_set_write, NULL, NULL) == -1)
		CU_FAIL_FATAL("Master end of pty not writeable");
	if (read(masterfd, read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to slave end of pty");

	if (tcgetattr(slavefd, &term_flags) != 0)
		CU_FAIL_FATAL("Could not get slave pty attributes");
	CU_ASSERT(term_flags.c_lflag & ECHONL)
	write(masterfd, "\n", (size_t) 2);
	close(masterfd);
	close(slavefd);
	return;
}

void passphrase_read_prompts_to_stderr_when_no_tty()
{
	static const int read_nbyte = 11;
	int stdin_pipe[2], stderr_pipe[2];
	char read_buf[read_nbyte];
	struct sigaction ignore, orig;

	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);

	if (pipe(stdin_pipe) != 0 || pipe(stderr_pipe) != 0)
		CU_FAIL_FATAL("Could not create pipe");

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			char buffer[password_size];
			close(stderr_pipe[0]);
			close(stdin_pipe[1]);
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			dup2(stdin_pipe[0], STDIN_FILENO);
			dup2(stderr_pipe[1], STDERR_FILENO);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			exit(EXIT_SUCCESS);
		}
	}
	close(stderr_pipe[1]);
	close(stdin_pipe[0]);

	read_buf[read_nbyte - 1] = '\0';

	if (read(stderr_pipe[0], read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to pipe");
	CU_ASSERT_STRING_EQUAL(read_buf, "Password: ");

	sigaction(SIGPIPE, &ignore, &orig);
	write(stdin_pipe[1], "\n", (size_t) 2);
	sigaction(SIGPIPE, &orig, NULL);
	close(stderr_pipe[0]);
	close(stdin_pipe[1]);
	return;
}

void passphrase_read_reads_from_stdin_when_no_tty()
{
	static const int read_nbyte = 11;
	int stdin_pipe[2], stderr_pipe[2], result_pipe[2];
	char read_buf[read_nbyte];
	struct sigaction ignore, orig;

	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);

	if (pipe(stdin_pipe) != 0
		|| pipe(stderr_pipe) != 0
		|| pipe(result_pipe) !=0)
		CU_FAIL_FATAL("Could not create pipe");

	switch (fork())
	{
	case -1:
		CU_FAIL_FATAL("Could not fork");
	case 0:
		{
			static const int password_size = 512;
			char buffer[password_size];
			close(stderr_pipe[0]);
			close(result_pipe[0]);
			close(stdin_pipe[1]);
			if (setsid() == (pid_t) -1)
				CU_FAIL_FATAL("Could not create new session");

			dup2(stdin_pipe[0], STDIN_FILENO);
			dup2(stderr_pipe[1], STDERR_FILENO);
			freerdp_passphrase_read("Password: ", buffer, password_size);
			write(result_pipe[1], buffer, strlen(buffer) + (size_t) 1);
			exit(EXIT_SUCCESS);
		}
	}
	close(stderr_pipe[1]);
	close(result_pipe[1]);
	close(stdin_pipe[0]);

	read_buf[read_nbyte - 1] = '\0';

	if (read(stderr_pipe[0], read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to pipe");

	sigaction(SIGPIPE, &ignore, &orig);
	write(stdin_pipe[1], "passw0rd\n", sizeof "passw0rd\n");
	sigaction(SIGPIPE, &orig, NULL);

	if (read(result_pipe[0], read_buf, read_nbyte) == (ssize_t) -1)
		CU_FAIL_FATAL("Nothing written to pipe");
	CU_ASSERT_STRING_EQUAL(read_buf, "passw0rd");

	close(stderr_pipe[0]);
	close(stdin_pipe[1]);
	return;
}

void test_passphrase_read(void)
{
	passphrase_read_prompts_to_tty();
	passphrase_read_reads_from_tty();
	passphrase_read_turns_off_echo_during_read();
	passphrase_read_resets_terminal_after_read();
	passphrase_read_turns_on_newline_echo_during_read();
	passphrase_read_prompts_to_stderr_when_no_tty();
	passphrase_read_reads_from_stdin_when_no_tty();
}

void handle_signals_resets_terminal(void)
{
	int status, masterfd;
	char* slavedevice;
	struct termios test_flags;
	pid_t child_pid;

	masterfd = posix_openpt(O_RDWR|O_NOCTTY);

	if (masterfd == -1
		|| grantpt (masterfd) == -1
		|| unlockpt (masterfd) == -1
		|| (slavedevice = ptsname (masterfd)) == NULL)
		CU_FAIL_FATAL("Could not create pty");

	terminal_fildes = open(slavedevice, O_RDWR|O_NOCTTY);
	tcgetattr(terminal_fildes, &orig_flags);
	new_flags = orig_flags;
	new_flags.c_lflag &= ~ECHO;
	tcsetattr(terminal_fildes, TCSANOW, &new_flags);
	terminal_needs_reset = 1;

	if((child_pid = fork()) == 0)
	{
		freerdp_handle_signals();
		raise(SIGINT);
	}
	while(wait(&status) != -1);
	tcgetattr(terminal_fildes, &test_flags);
	CU_ASSERT_EQUAL(orig_flags.c_lflag, test_flags.c_lflag);
	close(masterfd);
	close(terminal_fildes);
}

void test_handle_signals(void)
{
	handle_signals_resets_terminal();
}
