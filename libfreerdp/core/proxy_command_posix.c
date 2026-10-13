/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * POSIX external command transport
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freerdp/config.h>
#include "proxy_command.h"

#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char** environ;
#endif

#include <freerdp/log.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/winsock.h>

#define TAG FREERDP_TAG("core.proxycommand")

typedef struct
{
	proxyCommandBase base;
	int fd;
	pid_t pid;
	HANDLE event;
} proxyCommandPosix;

static int proxy_command_read(void* userContext, void* data, int bytes)
{
	proxyCommandPosix* proxy = userContext;
	if (!data || (bytes <= 0))
		return 0;
	if (!proxy_command_check(&proxy->base))
		return -1;
	(void)WSAResetEvent(proxy->event);
	const int result = _recv((SOCKET)proxy->fd, data, bytes, 0);
	if (result > 0)
		proxy_command_mark_received(&proxy->base);
	else if ((result < 0) && (errno == EINTR))
		errno = EAGAIN;
	return result;
}

static int proxy_command_write(void* userContext, const void* data, int bytes)
{
	proxyCommandPosix* proxy = userContext;
	if (!data || (bytes <= 0))
		return 0;
	if (!proxy_command_check(&proxy->base))
		return -1;
	/* WinPR suppresses SIGPIPE; SO_NOSIGPIPE is set below on macOS. */
	const int result = _send((SOCKET)proxy->fd, data, bytes, 0);
	if ((result < 0) && (errno == EINTR))
		errno = EAGAIN;
	return result;
}

static BOOL proxy_command_wait(void* userContext, BOOL waitWrite, DWORD timeout)
{
	proxyCommandPosix* proxy = userContext;
	const ULONGLONG started = GetTickCount64();
	do
	{
		if (!proxy_command_check(&proxy->base))
			return FALSE;
		const ULONGLONG elapsed = GetTickCount64() - started;
		const DWORD remaining = (elapsed < timeout) ? timeout - (DWORD)elapsed : 0;
		struct pollfd pfd = { .fd = proxy->fd, .events = waitWrite ? POLLOUT : POLLIN };
		const DWORD wait = proxy_command_wait_timeout(&proxy->base, remaining);
		const int rc = poll(&pfd, 1, (int)wait);
		if (!proxy_command_check(&proxy->base))
			return FALSE;
		if (rc > 0)
			return TRUE; /* EOF/error readiness is consumed by the next Read/Write. */
		if ((rc < 0) && (errno != EINTR))
			return FALSE;
	} while ((timeout == INFINITE) || (GetTickCount64() - started < timeout));
	return FALSE;
}

static HANDLE proxy_command_event(void* userContext)
{
	proxyCommandPosix* proxy = userContext;
	return proxy->event;
}

static BOOL proxy_command_close(void* userContext)
{
	proxyCommandPosix* proxy = userContext;
	if (proxy->event)
	{
		(void)CloseHandle(proxy->event);
		proxy->event = nullptr;
	}
	if (proxy->fd >= 0)
	{
		close(proxy->fd);
		proxy->fd = -1;
	}
	if (proxy->pid > 0)
	{
		/* The child owns a new process group, including pipelines and nested helpers.
		 * Do not reap its leader until after signalling the group (avoid PID reuse). */
		(void)kill(-proxy->pid, SIGTERM);
		const ULONGLONG deadline = GetTickCount64() + 250;
		int rc = 0;
		int status = 0;
		siginfo_t info = WINPR_C_ARRAY_INIT;
		do
		{
			rc = waitid(P_PID, (id_t)proxy->pid, &info, WEXITED | WNOHANG | WNOWAIT);
			if (info.si_pid || ((rc < 0) && (errno != EINTR)))
				break;
			Sleep(10);
		} while (GetTickCount64() < deadline);
		/* Also terminate surviving descendants when the shell has already exited. */
		(void)kill(-proxy->pid, SIGKILL);
		if (rc == 0 || (rc < 0 && errno == EINTR))
		{
			do
			{
				rc = (int)waitpid(proxy->pid, &status, 0);
			} while ((rc < 0) && (errno == EINTR));
		}
		proxy->pid = -1;
	}
	return TRUE;
}

static int proxy_command_safe_fd(int fd)
{
	/* Leave standard descriptor numbers available for the child's dup2 actions,
	 * even if the embedding application closed its own stdin/stdout/stderr. */
	const int result = fcntl(fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
	close(fd);
	return result;
}

rdpTransportLayer* proxy_command_platform_connect(rdpContext* context, const char* command,
                                                  DWORD timeout)
{
#if !defined(HAVE_PROXY_COMMAND_CLOSEFROM) && !defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
	/* Never launch a command with arbitrary application descriptors inherited. */
	WLog_ERR(TAG, "ProxyCommand requires closefrom spawn actions or close-on-exec-default support");
	errno = ENOTSUP;
	return nullptr;
#endif
	rdpTransportLayer* layer =
	    transport_layer_new(freerdp_get_transport(context), sizeof(proxyCommandPosix));
	if (!layer)
		return nullptr;
	proxyCommandPosix* proxy = layer->userContext;
	proxy->base.context = context;
	proxy->base.deadline = timeout ? GetTickCount64() + timeout : 0;
	proxy->fd = -1;
	proxy->pid = -1;
	layer->Read = proxy_command_read;
	layer->Write = proxy_command_write;
	layer->Wait = proxy_command_wait;
	layer->GetEvent = proxy_command_event;
	layer->Close = proxy_command_close;

	int sockets[2] = { -1, -1 };
	int rc = 0;
	posix_spawn_file_actions_t actions;
	posix_spawnattr_t attributes;
	BOOL haveActions = FALSE;
	BOOL haveAttributes = FALSE;
	if (!proxy_command_check(&proxy->base))
		goto fail;
	int socketType = SOCK_STREAM;
#if defined(SOCK_CLOEXEC)
	socketType |= SOCK_CLOEXEC;
#endif
	if (socketpair(AF_UNIX, socketType, 0, sockets) < 0)
		goto fail;
	sockets[0] = proxy_command_safe_fd(sockets[0]);
	sockets[1] = proxy_command_safe_fd(sockets[1]);
	if ((sockets[0] < 0) || (sockets[1] < 0))
		goto fail;

	if ((rc = posix_spawn_file_actions_init(&actions)) != 0)
		goto fail;
	haveActions = TRUE;
	if ((rc = posix_spawnattr_init(&attributes)) != 0)
		goto fail;
	haveAttributes = TRUE;
	short flags = POSIX_SPAWN_SETPGROUP;
#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
	flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
	if (fcntl(STDERR_FILENO, F_GETFD) >= 0 &&
	    (rc = posix_spawn_file_actions_adddup2(&actions, STDERR_FILENO, STDERR_FILENO)) != 0)
		goto fail;
#endif
	if ((rc = posix_spawnattr_setflags(&attributes, flags)) != 0 ||
	    (rc = posix_spawnattr_setpgroup(&attributes, 0)) != 0 ||
	    (rc = posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO)) != 0 ||
	    (rc = posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO)) != 0)
		goto fail;
#if defined(HAVE_PROXY_COMMAND_CLOSEFROM)
	if ((rc = posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1)) != 0)
		goto fail;
#else
	if ((rc = posix_spawn_file_actions_addclose(&actions, sockets[0])) != 0 ||
	    (rc = posix_spawn_file_actions_addclose(&actions, sockets[1])) != 0)
		goto fail;
#endif
	char* const argv[] = { "/bin/sh", "-c", (char*)command, nullptr };
#if defined(__APPLE__)
	char** env = *_NSGetEnviron();
#else
	char** env = environ;
#endif
	rc = posix_spawn(&proxy->pid, argv[0], &actions, &attributes, argv, env);
	if (rc != 0)
		goto fail;
	close(sockets[1]);
	sockets[1] = -1;
	proxy->fd = sockets[0];
	sockets[0] = -1;
#if defined(SO_NOSIGPIPE)
	const int on = 1;
	if (setsockopt(proxy->fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) < 0)
		goto fail;
#endif
	proxy->event = WSACreateEvent();
	if (!proxy->event || WSAEventSelect((SOCKET)proxy->fd, proxy->event, FD_READ | FD_CLOSE))
		goto fail;
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attributes);
	return layer;

fail:
	WLog_ERR(TAG, "Could not start ProxyCommand transport: %s", strerror(rc ? rc : errno));
	if (haveActions)
		posix_spawn_file_actions_destroy(&actions);
	if (haveAttributes)
		posix_spawnattr_destroy(&attributes);
	if (sockets[0] >= 0)
		close(sockets[0]);
	if (sockets[1] >= 0)
		close(sockets[1]);
	transport_layer_free(layer);
	return nullptr;
}
#elif !defined(_WIN32)

#include <errno.h>
#include <freerdp/log.h>

rdpTransportLayer* proxy_command_platform_connect(WINPR_ATTR_UNUSED rdpContext* context,
                                                  WINPR_ATTR_UNUSED const char* command,
                                                  WINPR_ATTR_UNUSED DWORD timeout)
{
	WLog_ERR(FREERDP_TAG("core.proxycommand"),
	         "External process transports are unsupported on this platform");
	errno = ENOTSUP;
	return nullptr;
}
#endif
