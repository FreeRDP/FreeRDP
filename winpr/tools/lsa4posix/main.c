/**
 * WinPR: Windows Portable Runtime
 * lsa4posix: per-user LSA daemon entry point
 *
 * A per-user daemon that backs the WinPR LSA client and the TSSSP package. It
 * listens on an AF_UNIX socket in a private per-user directory, authenticates the
 * peer by uid, and serves one worker thread per connection.
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

/* glibc only declares struct ucred / SO_PEERCRED under _GNU_SOURCE; define it
 * before the first system header is included. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/wlog.h>
#include <winpr/environment.h>
#include <winpr/debug.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "lsa4posix.h"

#define TAG LSA4POSIX_TAG

/* Return a malloc'd copy of an environment variable's value, or NULL when unset
 * or empty. Uses the WinPR environment API, which is thread safe unlike getenv. */
static char* dup_env(const char* name)
{
	const DWORD n = GetEnvironmentVariableX(name, nullptr, 0);
	if (n <= 1) /* unset or empty: the reported length-plus-null is 1 */
		return nullptr;

	char* value = malloc(n);
	if (!value)
		return nullptr;

	if (GetEnvironmentVariableX(name, value, n) != (n - 1))
	{
		free(value);
		return nullptr;
	}
	return value;
}

/* First fd systemd passes for socket activation (the LISTEN_FDS protocol). */
#define SD_LISTEN_FDS_START 3

/* systemd socket activation: when the service is started with an already-listening
 * socket passed by the manager, return its fd; otherwise -1 (we create our own). */
static int adopt_activated_socket(void)
{
	char* pidStr = dup_env("LISTEN_PID");
	char* fdsStr = dup_env("LISTEN_FDS");
	int fd = -1;

	if (pidStr && fdsStr)
	{
		const long pid = strtol(pidStr, nullptr, 10);
		const long fds = strtol(fdsStr, nullptr, 10);
		if ((pid == (long)getpid()) && (fds >= 1))
			fd = SD_LISTEN_FDS_START;
	}

	free(pidStr);
	free(fdsStr);
	return fd;
}

static char* default_socket_path(void)
{
	char* env = dup_env("LSA4POSIX_SOCKET");
	if (env)
		return env;

	char* path = nullptr;
	size_t len = 0;
	char* xdg = dup_env("XDG_RUNTIME_DIR");
	if (xdg)
	{
		const int rc = winpr_asprintf(&path, &len, "%s/lsa4posix/lsa.sock", xdg);
		free(xdg);
		if (rc < 0)
			return nullptr;
		return path;
	}
	if (winpr_asprintf(&path, &len, "/tmp/lsa4posix-%u/lsa.sock", (unsigned)geteuid()) < 0)
		return nullptr;
	return path;
}

/* Create the socket's parent directory 0700, owned by this user. */
static BOOL ensure_socket_dir(const char* sockPath)
{
	char* copy = _strdup(sockPath);
	if (!copy)
		return FALSE;
	char* slash = strrchr(copy, '/');
	if (!slash || (slash == copy))
	{
		free(copy);
		return FALSE;
	}
	*slash = '\0';

	BOOL rc = FALSE;
	if ((mkdir(copy, 0700) == 0) || (errno == EEXIST))
	{
		struct stat st = { 0 };
		if ((stat(copy, &st) == 0) && S_ISDIR(st.st_mode) && (st.st_uid == geteuid()))
		{
			(void)chmod(copy, 0700);
			rc = TRUE;
		}
	}
	if (!rc)
		WLog_ERR(TAG, "cannot create a private socket directory");
	free(copy);
	return rc;
}

static int listen_socket(const char* path)
{
	if (!ensure_socket_dir(path))
		return -1;

	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(addr.sun_path))
	{
		WLog_ERR(TAG, "socket path too long");
		return -1;
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	(void)unlink(path);
	const mode_t old = umask(0177); /* socket 0600 */
	const int brc = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	(void)umask(old);
	if ((brc != 0) || (listen(fd, 8) != 0))
	{
		char ebuf[256] = { 0 };
		WLog_ERR(TAG, "bind/listen failed: %s", winpr_strerror(errno, ebuf, sizeof(ebuf)));
		close(fd);
		return -1;
	}
	return fd;
}

/* Reject a peer whose uid is not our own. */
static BOOL peer_allowed(int fd)
{
#if defined(SO_PEERCRED)
	struct ucred cred = { 0 };
	socklen_t len = sizeof(cred);
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
		return FALSE;
	return cred.uid == geteuid();
#else
	uid_t uid = 0;
	gid_t gid = 0;
	if (getpeereid(fd, &uid, &gid) != 0)
		return FALSE;
	return uid == geteuid();
#endif
}

static void* worker(void* arg)
{
	const int fd = (int)(intptr_t)arg;
	lsa4posix_serve_connection(fd);
	return nullptr;
}

int main(int argc, char* argv[])
{
	char* socketPath = nullptr;
	BOOL foreground = FALSE;
	BOOL debug = FALSE;

	for (int i = 1; i < argc; i++)
	{
		if ((strcmp(argv[i], "--socket") == 0) && ((i + 1) < argc))
		{
			free(socketPath);
			socketPath = _strdup(argv[++i]);
		}
		else if (strcmp(argv[i], "--foreground") == 0)
			foreground = TRUE;
		else if (strcmp(argv[i], "--debug") == 0)
			debug = TRUE;
		else if (strcmp(argv[i], "--help") == 0)
		{
			(void)printf("Usage: lsa4posix [--socket PATH] [--foreground] [--debug]\n");
			free(socketPath);
			return 0;
		}
	}

	/* --debug raises every WinPR logger (the servicing packages included) to TRACE. */
	if (debug)
	{
		const BOOL set = WLog_SetLogLevel(WLog_GetRoot(), WLOG_TRACE);
		WINPR_ASSERT(set);
	}

	(void)signal(SIGPIPE, SIG_IGN);

	/* Prefer a socket-activated listener passed by the service manager; otherwise bind
	 * our own on socketPath. */
	BOOL activated = FALSE;
	int listenFd = adopt_activated_socket();
	if (listenFd >= 0)
	{
		activated = TRUE;
		WLog_INFO(TAG, "adopted a socket-activated listener (fd %d)", listenFd);
	}
	else
	{
		if (!socketPath)
			socketPath = default_socket_path();
		if (!socketPath)
			return 1;
		listenFd = listen_socket(socketPath);
		if (listenFd < 0)
		{
			free(socketPath);
			return 1;
		}
		WLog_INFO(TAG, "lsa4posix listening on %s", socketPath);
	}

	if (!foreground)
	{
		if (daemon(0, 0) != 0)
			WLog_WARN(TAG, "daemon() failed, staying in the foreground");
	}

	for (;;)
	{
		const int fd = accept(listenFd, nullptr, nullptr);
		if (fd < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}

		if (!peer_allowed(fd))
		{
			WLog_WARN(TAG, "rejected a connection from another user");
			close(fd);
			continue;
		}

		pthread_t thread = 0;
		if (pthread_create(&thread, nullptr, worker, (void*)(intptr_t)fd) != 0)
		{
			close(fd);
			continue;
		}
		(void)pthread_detach(thread);
	}

	close(listenFd);
	if (!activated && socketPath)
		(void)unlink(socketPath); /* the service manager owns the socket when activated */
	free(socketPath);
	return 0;
}
