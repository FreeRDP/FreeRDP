/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Passphrase Handling Utils
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

#include <errno.h>
#include <freerdp/utils/passphrase.h>
#ifdef _WIN32
char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz)
{
	errno=ENOSYS;
	return NULL;
}
#else
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <freerdp/utils/signal.h>

char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz)
{
	char read_char;
	char* buf_iter;
	char term_name[L_ctermid];
	int term_file, write_file;
	ssize_t nbytes;
	size_t read_bytes = 0;

	if (bufsiz == 0)
	{
		errno = EINVAL;
		return NULL;
	}

	ctermid(term_name);
	if(strcmp(term_name, "") == 0
		|| (term_file = open(term_name, O_RDWR)) == -1)
	{
		write_file = STDERR_FILENO;
		terminal_fildes = STDIN_FILENO;
	}
	else
	{
		write_file = term_file;
		terminal_fildes = term_file;
	}

	if (tcgetattr(terminal_fildes, &orig_flags) != -1)
	{
		new_flags = orig_flags;
		new_flags.c_lflag &= ~ECHO;
		new_flags.c_lflag |= ECHONL;
		terminal_needs_reset = 1;
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &new_flags) == -1)
			terminal_needs_reset = 0;
	}

	if (write(write_file, prompt, strlen(prompt)) == (ssize_t) -1)
		goto error;

	buf_iter = buf;
	while ((nbytes = read(terminal_fildes, &read_char, sizeof read_char)) == (sizeof read_char))
	{
		if (read_char == '\n')
			break;
		if (read_bytes < (bufsiz - (size_t) 1))
		{
			read_bytes++;
			*buf_iter = read_char;
			buf_iter++;
		}
	}
	*buf_iter = '\0';
	buf_iter = NULL;
	read_char = '\0';
	if (nbytes == (ssize_t) -1)
		goto error;

	if (terminal_needs_reset)
	{
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags) == -1)	
			goto error;
		terminal_needs_reset = 0;
	}

	if (terminal_fildes != STDIN_FILENO)
	{
		if (close(terminal_fildes) == -1)
			goto error;
	}

	return buf;

	error:
	{
		int saved_errno = errno;
		buf_iter = NULL;
		read_char = '\0';
		if (terminal_needs_reset)
			tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags);
		if (terminal_fildes != STDIN_FILENO)
			close(terminal_fildes);
		errno = saved_errno;
		return NULL;
	}
}
#endif
