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

#include <winpr/environment.h>

#include <freerdp/config.h>
#include <freerdp/freerdp.h>

#include <errno.h>
#include <freerdp/utils/passphrase.h>

#ifdef _WIN32

#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <wincred.h>

static char read_chr(FILE* f)
{
	char chr;
	const BOOL isTty = _isatty(_fileno(f));
	if (isTty)
		return fgetc(f);
	if (fscanf_s(f, "%c", &chr, (UINT32)sizeof(char)) && !feof(f))
		return chr;
	return 0;
}

int freerdp_interruptible_getc(rdpContext* context, FILE* f)
{
	return read_chr(f);
}

const char* freerdp_passphrase_read(rdpContext* context, const char* prompt, char* buf,
                                    size_t bufsiz, int from_stdin)
{
	WCHAR UserNameW[CREDUI_MAX_USERNAME_LENGTH + 1] = { 'p', 'r', 'e', 'f', 'i',
		                                                'l', 'l', 'e', 'd', '\0' };
	WCHAR PasswordW[CREDUI_MAX_PASSWORD_LENGTH + 1] = { 0 };
	BOOL fSave = FALSE;
	DWORD dwFlags = 0;
	WCHAR* promptW = ConvertUtf8ToWCharAlloc(prompt, NULL);
	const DWORD status =
	    CredUICmdLinePromptForCredentialsW(promptW, NULL, 0, UserNameW, ARRAYSIZE(UserNameW),
	                                       PasswordW, ARRAYSIZE(PasswordW), &fSave, dwFlags);
	free(promptW);
	if (ConvertWCharNToUtf8(PasswordW, ARRAYSIZE(PasswordW), buf, bufsiz) < 0)
		return NULL;
	return buf;
}

#elif !defined(ANDROID)

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <freerdp/utils/signal.h>
#include <freerdp/log.h>
#if defined(WINPR_HAVE_POLL_H) && !defined(__APPLE__)
#include <poll.h>
#else
#include <time.h>
#include <sys/select.h>
#endif

#define TAG FREERDP_TAG("utils.passphrase")

static int wait_for_fd(int fd, int timeout)
{
	int status = 0;
#if defined(WINPR_HAVE_POLL_H) && !defined(__APPLE__)
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
	FD_SET(fd, &rset);

	if (timeout)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	do
	{
		status = select(fd + 1, &rset, NULL, NULL, timeout ? &tv : NULL);
	} while ((status < 0) && (errno == EINTR));

#endif
	return status;
}

static void replace_char(char* buffer, WINPR_ATTR_UNUSED size_t buffer_len, const char* toreplace)
{
	while (*toreplace != '\0')
	{
		char* ptr = NULL;
		while ((ptr = strrchr(buffer, *toreplace)) != NULL)
			*ptr = '\0';
		toreplace++;
	}
}

static const char* freerdp_passphrase_read_tty(rdpContext* context, const char* prompt, char* buf,
                                               size_t bufsiz, int from_stdin)
{
	BOOL terminal_needs_reset = FALSE;
	char term_name[L_ctermid] = { 0 };

	FILE* fout = NULL;

	if (bufsiz == 0)
	{
		errno = EINVAL;
		return NULL;
	}

	ctermid(term_name);
	int terminal_fildes = 0;
	if (from_stdin || (strcmp(term_name, "") == 0))
	{
		fout = stdout;
		terminal_fildes = STDIN_FILENO;
	}
	else
	{
		const int term_file = open(term_name, O_RDWR);
		if (term_file < 0)
		{
			fout = stdout;
			terminal_fildes = STDIN_FILENO;
		}
		else
		{
			fout = fdopen(term_file, "w");
			if (!fout)
			{
				close(term_file);
				return NULL;
			}
			terminal_fildes = term_file;
		}
	}

	struct termios orig_flags = { 0 };
	if (tcgetattr(terminal_fildes, &orig_flags) != -1)
	{
		struct termios new_flags = { 0 };
		new_flags = orig_flags;
		new_flags.c_lflag &= (uint32_t)~ECHO;
		new_flags.c_lflag |= ECHONL;
		terminal_needs_reset = TRUE;
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &new_flags) == -1)
			terminal_needs_reset = FALSE;
	}

	FILE* fp = fdopen(terminal_fildes, "r");
	if (!fp)
		goto error;

	(void)fprintf(fout, "%s", prompt);
	(void)fflush(fout);

	{
		char* ptr = NULL;
		size_t ptr_len = 0;
		const SSIZE_T res = freerdp_interruptible_get_line(context, &ptr, &ptr_len, fp);
		if (res < 0)
			goto error;

		replace_char(ptr, ptr_len, "\r\n");

		strncpy(buf, ptr, MIN(bufsiz, ptr_len));
		free(ptr);
	}

	if (terminal_needs_reset)
	{
		if (tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags) == -1)
			goto error;
	}

	if (terminal_fildes != STDIN_FILENO)
		(void)fclose(fp);

	return buf;

error:
{
	// NOLINTNEXTLINE(clang-analyzer-unix.Stream)
	int saved_errno = errno;
	if (terminal_needs_reset)
		(void)tcsetattr(terminal_fildes, TCSAFLUSH, &orig_flags);

	if (terminal_fildes != STDIN_FILENO)
	{
		if (fp)
			(void)fclose(fp);
	}
	// NOLINTNEXTLINE(clang-analyzer-unix.Stream)
	errno = saved_errno;
}

	return NULL;
}

static const char* freerdp_passphrase_read_askpass(const char* prompt, char* buf, size_t bufsiz,
                                                   char const* askpass_env)
{
	char command[4096] = { 0 };

	(void)sprintf_s(command, sizeof(command), "%s 'FreeRDP authentication\n%s'", askpass_env,
	                prompt);
	// NOLINTNEXTLINE(clang-analyzer-optin.taint.GenericTaint)
	FILE* askproc = popen(command, "r");
	if (!askproc)
		return NULL;
	WINPR_ASSERT(bufsiz <= INT32_MAX);
	if (fgets(buf, (int)bufsiz, askproc) != NULL)
		buf[strcspn(buf, "\r\n")] = '\0';
	else
		buf = NULL;
	const int status = pclose(askproc);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		buf = NULL;

	return buf;
}

const char* freerdp_passphrase_read(rdpContext* context, const char* prompt, char* buf,
                                    size_t bufsiz, int from_stdin)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	const char* askpass_env = getenv("FREERDP_ASKPASS");

	if (askpass_env)
		return freerdp_passphrase_read_askpass(prompt, buf, bufsiz, askpass_env);
	else
		return freerdp_passphrase_read_tty(context, prompt, buf, bufsiz, from_stdin);
}

static BOOL set_termianl_nonblock(int ifd, BOOL nonblock);

static void restore_terminal(void)
{
	(void)set_termianl_nonblock(-1, FALSE);
}

BOOL set_termianl_nonblock(int ifd, BOOL nonblock)
{
	static int fd = -1;
	static bool registered = false;
	static int orig = 0;
	static struct termios termios = { 0 };

	if (ifd >= 0)
		fd = ifd;

	if (fd < 0)
		return FALSE;

	if (nonblock)
	{
		if (!registered)
		{
			(void)atexit(restore_terminal);
			registered = true;
		}

		const int rc1 = fcntl(fd, F_SETFL, orig | O_NONBLOCK);
		if (rc1 != 0)
		{
			char buffer[128] = { 0 };
			WLog_ERR(TAG, "fcntl(F_SETFL) failed with %s",
			         winpr_strerror(errno, buffer, sizeof(buffer)));
			return FALSE;
		}
		const int rc2 = tcgetattr(fd, &termios);
		if (rc2 != 0)
		{
			char buffer[128] = { 0 };
			WLog_ERR(TAG, "tcgetattr() failed with %s",
			         winpr_strerror(errno, buffer, sizeof(buffer)));
			return FALSE;
		}

		struct termios now = termios;
		cfmakeraw(&now);
		const int rc3 = tcsetattr(fd, TCSANOW, &now);
		if (rc3 != 0)
		{
			char buffer[128] = { 0 };
			WLog_ERR(TAG, "tcsetattr(TCSANOW) failed with %s",
			         winpr_strerror(errno, buffer, sizeof(buffer)));
			return FALSE;
		}
	}
	else
	{
		const int rc1 = tcsetattr(fd, TCSANOW, &termios);
		if (rc1 != 0)
		{
			char buffer[128] = { 0 };
			WLog_ERR(TAG, "tcsetattr(TCSANOW) failed with %s",
			         winpr_strerror(errno, buffer, sizeof(buffer)));
			return FALSE;
		}
		const int rc2 = fcntl(fd, F_SETFL, orig);
		if (rc2 != 0)
		{
			char buffer[128] = { 0 };
			WLog_ERR(TAG, "fcntl(F_SETFL) failed with %s",
			         winpr_strerror(errno, buffer, sizeof(buffer)));
			return FALSE;
		}
		fd = -1;
	}
	return TRUE;
}

int freerdp_interruptible_getc(rdpContext* context, FILE* stream)
{
	int rc = EOF;
	const int fd = fileno(stream);

	(void)set_termianl_nonblock(fd, TRUE);

	do
	{
		const int res = wait_for_fd(fd, 10);
		if (res != 0)
		{
			char c = 0;
			const ssize_t rd = read(fd, &c, 1);
			if (rd == 1)
			{
				if (c == 3) /* ctrl + c */
					return EOF;
				if (c == 4) /* ctrl + d */
					return EOF;
				if (c == 26) /* ctrl + z */
					return EOF;
				rc = (int)c;
			}
			break;
		}
	} while (!freerdp_shall_disconnect_context(context));

	(void)set_termianl_nonblock(fd, FALSE);

	return rc;
}

#else

const char* freerdp_passphrase_read(rdpContext* context, const char* prompt, char* buf,
                                    size_t bufsiz, int from_stdin)
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
	int c = 0;
	char* n = NULL;
	size_t step = 32;
	size_t used = 0;
	char* ptr = NULL;
	size_t len = 0;

	if (!plineptr || !psize)
	{
		errno = EINVAL;
		return -1;
	}

	bool echo = true;
#if !defined(_WIN32) && !defined(ANDROID)
	{
		const int fd = fileno(stream);

		struct termios termios = { 0 };
		/* This might fail if /from-stdin is used. */
		if (tcgetattr(fd, &termios) == 0)
			echo = (termios.c_lflag & ECHO) != 0;
		else
			echo = false;
	}
#endif

	if (*plineptr && (*psize > 0))
	{
		ptr = *plineptr;
		used = *psize;
		if (echo)
		{
			printf("%s", ptr);
			(void)fflush(stdout);
		}
	}

	do
	{
		if (used + 2 >= len)
		{
			len += step;
			n = realloc(ptr, len);

			if (!n)
			{
				free(ptr);
				*plineptr = NULL;
				return -1;
			}

			ptr = n;
		}

		c = freerdp_interruptible_getc(context, stream);
		if (c == 127)
		{
			if (used > 0)
			{
				ptr[used--] = '\0';
				if (echo)
				{
					printf("\b");
					printf(" ");
					printf("\b");
					(void)fflush(stdout);
				}
			}
			continue;
		}
		if (echo)
		{
			printf("%c", c);
			(void)fflush(stdout);
		}
		if (c != EOF)
			ptr[used++] = (char)c;
	} while ((c != '\n') && (c != '\r') && (c != EOF));

	printf("\n");
	ptr[used] = '\0';
	if (c == EOF)
	{
		free(ptr);
		*plineptr = NULL;
		return EOF;
	}
	*plineptr = ptr;
	*psize = used;
	return WINPR_ASSERTING_INT_CAST(SSIZE_T, used);
}
