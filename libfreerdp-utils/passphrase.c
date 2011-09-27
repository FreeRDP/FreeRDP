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

char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz)
{
	char read_char;
	char* buf_iter;
	char term_name[L_ctermid];
	int term_file, write_file, read_file, reset_terminal = 0;
	ssize_t nbytes;
	size_t read_bytes = 0;
	struct termios orig_flags, no_echo_flags;

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
		read_file = STDIN_FILENO;
	}
	else
	{
		write_file = term_file;
		read_file = term_file;
	}

	if (tcgetattr(read_file, &orig_flags) != -1)
	{
		reset_terminal = 1;
		no_echo_flags = orig_flags;
		no_echo_flags.c_lflag &= ~ECHO;
		no_echo_flags.c_lflag |= ECHONL;
		if (tcsetattr(read_file, TCSAFLUSH, &no_echo_flags) == -1)
			reset_terminal = 0;
	}

	if (write(write_file, prompt, strlen(prompt)) == (ssize_t) -1)
		goto error;

	buf_iter = buf;
	while ((nbytes = read(read_file, &read_char, sizeof read_char)) == (sizeof read_char))
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

	if (reset_terminal)
	{
		if (tcsetattr(read_file, TCSADRAIN, &orig_flags) == -1)	
			goto error;
		reset_terminal = 0;
	}

	if (read_file != STDIN_FILENO)
	{
		if (close(read_file) == -1)
			goto error;
	}

	return buf;

	error:
	{
		int saved_errno = errno;
		buf_iter = NULL;
		read_char = '\0';
		if (reset_terminal)
			tcsetattr(read_file, TCSANOW, &orig_flags);
		if (read_file != STDIN_FILENO)
			close(read_file);
		errno = saved_errno;
		return NULL;
	}
}
#endif
