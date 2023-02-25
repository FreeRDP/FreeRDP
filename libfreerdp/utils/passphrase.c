/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <freerdp/config.h>

#include <errno.h>
#include <freerdp/utils/passphrase.h>

#ifdef _WIN32

#include <stdio.h>
#include <io.h>
#include <conio.h>

char read_chr(int isTty)
{
	char chr;
	if (isTty)
		return _getch();
	if (scanf_s("%c", &chr, (UINT32)sizeof(char)) && !feof(stdin))
		return chr;
	return 0;
}

char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz, int from_stdin)
{
	const char CTRLC = 3;
	const char BACKSPACE = '\b';
	const char NEWLINE = '\n';
	const char CARRIAGERETURN = '\r';
	const char SHOW_ASTERISK = TRUE;

	size_t read_cnt = 0, chr;
	char isTty;

	if (from_stdin)
	{
		printf("%s ", prompt);
		fflush(stdout);
		isTty = _isatty(_fileno(stdin));
		while (read_cnt < bufsiz - 1 && (chr = read_chr(isTty)) && chr != NEWLINE &&
		       chr != CARRIAGERETURN)
		{
			if (chr == BACKSPACE)
			{
				if (read_cnt > 0)
				{
					if (SHOW_ASTERISK)
						printf("\b \b");
					read_cnt--;
				}
			}
			else if (chr == CTRLC)
			{
				if (read_cnt != 0)
				{
					while (read_cnt > 0)
					{
						if (SHOW_ASTERISK)
							printf("\b \b");
						read_cnt--;
					}
				}
				else
					goto fail;
			}
			else
			{
				*(buf + read_cnt) = chr;
				read_cnt++;
				if (SHOW_ASTERISK)
					printf("*");
			}
		}
		*(buf + read_cnt) = '\0';
		printf("\n");
		fflush(stdout);
		return buf;
	}
fail:
	errno = ENOSYS;
	return NULL;
}

#elif !defined(ANDROID)

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <freerdp/utils/signal.h>

#ifdef WINPR_HAVE_POLL_H
#include <poll.h>
#else
#include <time.h>
#include <sys/select.h>
#endif

static int wait_for_fd(int fd, int timeout)
{
	int status;
#ifdef WINPR_HAVE_POLL_H
	struct pollfd pollset = { 0 };
	pollset.fd = fd;
	pollset.events = POLLIN;
	pollset.revents = 0;

	do
	{
		status = poll(&pollset, 1, timeout);
	} while ((status < 0) && (errno == EINTR));

#else
	fd_set rset = { 0 };
	struct timeval tv = { 0 };
	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);

	if (timeout)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	do
	{
		status = select(sockfd + 1, &rset, NULL, NULL, timeout ? &tv : NULL);
	} while ((status < 0) && (errno == EINTR));

#endif
	return status;
}

static void replace_char(char* buffer, size_t buffer_len, const char* toreplace)
{
	while (*toreplace != '\0')
	{
		char* ptr;
		while ((ptr = strrchr(buffer, *toreplace)) != NULL)
			*ptr = '\0';
		toreplace++;
	}
}

char* freerdp_passphrase_read(rdpContext* context, const char* prompt, char* buf, size_t bufsiz,
                              int from_stdin)
{
	BOOL terminal_needs_reset = FALSE;
	char term_name[L_ctermid] = { 0 };
	int term_file;

	FILE* fout = NULL;

	if (bufsiz == 0)
	{
		errno = EINVAL;
		return NULL;
	}

	ctermid(term_name);
	int terminal_fildes;
	if (from_stdin || strcmp(term_name, "") == 0 || (term_file = open(term_name, O_RDWR)) == -1)
	{
		fout = stdout;
		terminal_fildes = STDIN_FILENO;
	}
	else
	{
		fout = fdopen(term_file, "w");
		terminal_fildes = term_file;
	}

	struct termios orig_flags = { 0 };
	if (tcgetattr(terminal_fildes, &orig_flags) != -1)
	{
		struct termios new_flags = { 0 };
		new_flags = orig_flags;
		new_flags.c_lflag &= ~ECHO;
		new_flags.c_lflag |= ECHONL;
		terminal_needs_reset = TRUE;
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &new_flags) == -1)
			terminal_needs_reset = FALSE;
	}

	FILE* fp = fdopen(terminal_fildes, "r");
	if (!fp)
		goto error;

	fprintf(fout, "%s", prompt);
	fflush(fout);

	char* ptr = NULL;
	size_t ptr_len = 0;

	const SSIZE_T res = freerdp_interruptible_get_line(context, &ptr, &ptr_len, fp);
	if (res < 0)
		goto error;
	replace_char(ptr, ptr_len, "\r\n");

	strncpy(buf, ptr, MIN(bufsiz, ptr_len));
	free(ptr);
	if (terminal_needs_reset)
	{
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags) == -1)
			goto error;
	}

	if (terminal_fildes != STDIN_FILENO)
	{
		if (fclose(fp) == -1)
			goto error;
	}

	return buf;

error:
{
	int saved_errno = errno;
	if (terminal_needs_reset)
		tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags);
	if (terminal_fildes != STDIN_FILENO)
	{
		fclose(fp);
	}
	errno = saved_errno;
	return NULL;
}
}

int freerdp_interruptible_getc(rdpContext* context, FILE* f)
{
	int rc = EOF;
	const int fd = fileno(f);

	const int orig = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, orig | O_NONBLOCK);
	do
	{
		const int res = wait_for_fd(fd, 10);
		if (res != 0)
		{
				char c = 0;
			    const ssize_t rd = read(fd, &c, 1);
			    if (rd == 1)
				    rc = c;
			    break;
		}
	} while (!freerdp_shall_disconnect_context(context));

	fcntl(fd, F_SETFL, orig);
	return rc;
}

#else

char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz, int from_stdin)
{
	return NULL;
}

int freerdp_interruptible_getc(rdpContext* context, FILE* f)
{
	return EOF;
}
#endif

SSIZE_T freerdp_interruptible_get_line(rdpContext* context, char** plineptr, size_t* psize,
                                       FILE* stream)
{
	char c;
	char* n;
	size_t step = 32;
	size_t used = 0;
	char* ptr = NULL;
	size_t len = 0;

	if (!plineptr || !psize)
	{
		errno = EINVAL;
		return -1;
	}

	do
	{
		if (used + 2 >= len)
		{
			len += step;
			n = realloc(ptr, len);

			if (!n)
			{
				return -1;
			}

			ptr = n;
		}

		c = freerdp_interruptible_getc(context, stream);
		if (c != EOF)
			ptr[used++] = c;
	} while ((c != '\n') && (c != '\r') && (c != EOF));

	ptr[used] = '\0';
	if (c == EOF)
	{
		free(ptr);
		return EOF;
	}
	*plineptr = ptr;
	*psize = used;
	return used;
}
