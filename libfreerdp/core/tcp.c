/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transmission Control Protocol (TCP)
 *
 * Copyright 2011 Vic Lee
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include <winpr/crt.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)

#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <sys/types.h>
#include <arpa/inet.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#else
#include <time.h>
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef __FreeBSD__
#ifndef SOL_TCP
#define SOL_TCP	IPPROTO_TCP
#endif
#endif

#ifdef __APPLE__
#ifndef SOL_TCP
#define SOL_TCP	IPPROTO_TCP
#endif
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif
#endif

#else

#include <winpr/windows.h>

#include <winpr/crt.h>

#define SHUT_RDWR SD_BOTH
#define close(_fd) closesocket(_fd)

#endif

#include <freerdp/log.h>

#include <winpr/stream.h>

#include "tcp.h"

#define TAG FREERDP_TAG("core")

/* Simple Socket BIO */

struct _WINPR_BIO_SIMPLE_SOCKET
{
	SOCKET socket;
	HANDLE hEvent;
};
typedef struct _WINPR_BIO_SIMPLE_SOCKET WINPR_BIO_SIMPLE_SOCKET;

static int transport_bio_simple_init(BIO* bio, SOCKET socket, int shutdown);
static int transport_bio_simple_uninit(BIO* bio);

static void transport_bio_simple_check_reset_event(BIO* bio)
{
	u_long nbytes = 0;
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

#ifndef _WIN32
	return;
#endif

	_ioctlsocket(ptr->socket, FIONREAD, &nbytes);

	if (nbytes < 1)
		WSAResetEvent(ptr->hEvent);
}

long transport_bio_simple_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_simple_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status = 0;
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	status = _send(ptr->socket, buf, size, 0);

	if (status <= 0)
	{
		error = WSAGetLastError();

		if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) ||
			(error == WSAEINPROGRESS) || (error == WSAEALREADY))
		{
			BIO_set_flags(bio, (BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY));
		}
		else
		{
			BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		}
	}

	return status;
}

static int transport_bio_simple_read(BIO* bio, char* buf, int size)
{
	int error;
	int status = 0;
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_READ);

	status = _recv(ptr->socket, buf, size, 0);

	if (status > 0)
	{
		transport_bio_simple_check_reset_event(bio);
		return status;
	}

	if (status == 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return 0;
	}

	error = WSAGetLastError();

	if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) ||
		(error == WSAEINPROGRESS) || (error == WSAEALREADY))
	{
		BIO_set_flags(bio, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
	}
	else
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}

	transport_bio_simple_check_reset_event(bio);

	return -1;
}

static int transport_bio_simple_puts(BIO* bio, const char* str)
{
	return 1;
}

static int transport_bio_simple_gets(BIO* bio, char* str, int size)
{
	return 1;
}

static long transport_bio_simple_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	int status = -1;
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

	if (cmd == BIO_C_SET_SOCKET)
	{
		transport_bio_simple_uninit(bio);
		transport_bio_simple_init(bio, (SOCKET) arg2, (int) arg1);
		return 1;
	}
	else if (cmd == BIO_C_GET_SOCKET)
	{
		if (!bio->init || !arg2)
			return 0;

		*((ULONG_PTR*) arg2) = (ULONG_PTR) ptr->socket;

		return 1;
	}
	else if (cmd == BIO_C_GET_EVENT)
	{
		if (!bio->init || !arg2)
			return 0;

		*((ULONG_PTR*) arg2) = (ULONG_PTR) ptr->hEvent;

		return 1;
	}
	else if (cmd == BIO_C_SET_NONBLOCK)
	{
#ifndef _WIN32
		int flags;

		flags = fcntl((int) ptr->socket, F_GETFL);

		if (flags == -1)
			return 0;

		if (arg1)
			fcntl((int) ptr->socket, F_SETFL, flags | O_NONBLOCK);
		else
			fcntl((int) ptr->socket, F_SETFL, flags & ~(O_NONBLOCK));
#else
		/* the internal socket is always non-blocking */
#endif
		return 1;
	}
	else if (cmd == BIO_C_WAIT_READ)
	{
		int timeout = (int) arg1;
		int sockfd = (int) ptr->socket;
#ifdef HAVE_POLL_H
		struct pollfd pollset;

		pollset.fd = sockfd;
		pollset.events = POLLIN;
		pollset.revents = 0;

		do
		{
			status = poll(&pollset, 1, timeout);
		}
		while ((status < 0) && (errno == EINTR));
#else
		fd_set rset;
		struct timeval tv;

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
		}
		while ((status < 0) && (errno == EINTR));
#endif
	}
	else if (cmd == BIO_C_WAIT_WRITE)
	{
		int timeout = (int) arg1;
		int sockfd = (int) ptr->socket;
#ifdef HAVE_POLL_H
		struct pollfd pollset;

		pollset.fd = sockfd;
		pollset.events = POLLOUT;
		pollset.revents = 0;

		do
		{
			status = poll(&pollset, 1, timeout);
		}
		while ((status < 0) && (errno == EINTR));
#else
		fd_set rset;
		struct timeval tv;

		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);

		if (timeout)
		{
			tv.tv_sec = timeout / 1000;
			tv.tv_usec = (timeout % 1000) * 1000;
		}

		do
		{
			status = select(sockfd + 1, NULL, &rset, NULL, timeout ? &tv : NULL);
		}
		while ((status < 0) && (errno == EINTR));
#endif
	}

	switch (cmd)
	{
		case BIO_C_SET_FD:
			if (arg2)
			{
				transport_bio_simple_uninit(bio);
				transport_bio_simple_init(bio, (SOCKET) *((int*) arg2), (int) arg1);
				status = 1;
			}
			break;

		case BIO_C_GET_FD:
			if (bio->init)
			{
				if (arg2)
					*((int*) arg2) = (int) ptr->socket;
				status = (int) ptr->socket;
			}
			break;

		case BIO_CTRL_GET_CLOSE:
			status = bio->shutdown;
			break;

		case BIO_CTRL_SET_CLOSE:
			bio->shutdown = (int) arg1;
			status = 1;
			break;

		case BIO_CTRL_DUP:
			status = 1;
			break;

		case BIO_CTRL_FLUSH:
			status = 1;
			break;

		default:
			status = 0;
			break;
	}

	return status;
}

static int transport_bio_simple_init(BIO* bio, SOCKET socket, int shutdown)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

	ptr->socket = socket;

	bio->shutdown = shutdown;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	bio->init = 1;

#ifdef _WIN32
		ptr->hEvent = WSACreateEvent(); /* creates a manual reset event */

		if (!ptr->hEvent)
			return 0;

		/* WSAEventSelect automatically sets the socket in non-blocking mode */
		WSAEventSelect(ptr->socket, ptr->hEvent, FD_READ | FD_CLOSE);
#else
		ptr->hEvent = CreateFileDescriptorEvent(NULL, FALSE, FALSE, (int) ptr->socket);

		if (!ptr->hEvent)
			return 0;
#endif

	return 1;
}

static int transport_bio_simple_uninit(BIO* bio)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*) bio->ptr;

	if (bio->shutdown)
	{
		if (bio->init)
		{
			_shutdown(ptr->socket, SD_BOTH);
			closesocket(ptr->socket);
			ptr->socket = 0;
		}
	}

	if (ptr->hEvent)
	{
		CloseHandle(ptr->hEvent);
		ptr->hEvent = NULL;
	}

	bio->init = 0;
	bio->flags = 0;

	return 1;
}

static int transport_bio_simple_new(BIO* bio)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr;

	bio->init = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;

	ptr = (WINPR_BIO_SIMPLE_SOCKET*) calloc(1, sizeof(WINPR_BIO_SIMPLE_SOCKET));

	if (!ptr)
		return 0;

	bio->ptr = ptr;

	return 1;
}

static int transport_bio_simple_free(BIO* bio)
{
	if (!bio)
		return 0;

	transport_bio_simple_uninit(bio);

	if (bio->ptr)
	{
		free(bio->ptr);
		bio->ptr = NULL;
	}

	return 1;
}

static BIO_METHOD transport_bio_simple_socket_methods =
{
	BIO_TYPE_SIMPLE,
	"SimpleSocket",
	transport_bio_simple_write,
	transport_bio_simple_read,
	transport_bio_simple_puts,
	transport_bio_simple_gets,
	transport_bio_simple_ctrl,
	transport_bio_simple_new,
	transport_bio_simple_free,
	NULL,
};

BIO_METHOD* BIO_s_simple_socket(void)
{
	return &transport_bio_simple_socket_methods;
}

/* Buffered Socket BIO */

struct _WINPR_BIO_BUFFERED_SOCKET
{
	BIO* bufferedBio;
	BOOL readBlocked;
	BOOL writeBlocked;
	RingBuffer xmitBuffer;
};
typedef struct _WINPR_BIO_BUFFERED_SOCKET WINPR_BIO_BUFFERED_SOCKET;

long transport_bio_buffered_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_buffered_write(BIO* bio, const char* buf, int num)
{
	int i, ret;
	int status;
	int nchunks;
	int committedBytes;
	DataChunk chunks[2];
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*) bio->ptr;

	ret = num;
	ptr->writeBlocked = FALSE;
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	/* we directly append extra bytes in the xmit buffer, this could be prevented
	 * but for now it makes the code more simple.
	 */
	if (buf && num && !ringbuffer_write(&ptr->xmitBuffer, (const BYTE*) buf, num))
	{
		WLog_ERR(TAG, "an error occured when writing (num: %d)", num);
		return -1;
	}

	committedBytes = 0;
	nchunks = ringbuffer_peek(&ptr->xmitBuffer, chunks, ringbuffer_used(&ptr->xmitBuffer));

	for (i = 0; i < nchunks; i++)
	{
		while (chunks[i].size)
		{
			status = BIO_write(bio->next_bio, chunks[i].data, chunks[i].size);

			if (status <= 0)
			{
				if (!BIO_should_retry(bio->next_bio))
				{
					BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
					ret = -1; /* fatal error */
					goto out;
				}

				if (BIO_should_write(bio->next_bio))
				{
					BIO_set_flags(bio, BIO_FLAGS_WRITE);
					ptr->writeBlocked = TRUE;
					goto out; /* EWOULDBLOCK */
				}
			}

			committedBytes += status;
			chunks[i].size -= status;
			chunks[i].data += status;
		}
	}

out:
	ringbuffer_commit_read_bytes(&ptr->xmitBuffer, committedBytes);

	return ret;
}

static int transport_bio_buffered_read(BIO* bio, char* buf, int size)
{
	int status;
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*) bio->ptr;

	ptr->readBlocked = FALSE;
	BIO_clear_flags(bio, BIO_FLAGS_READ);

	status = BIO_read(bio->next_bio, buf, size);

	if (status <= 0)
	{
		if (!BIO_should_retry(bio->next_bio))
		{
			BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
			goto out;
		}

		BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);

		if (BIO_should_read(bio->next_bio))
		{
			BIO_set_flags(bio, BIO_FLAGS_READ);
			ptr->readBlocked = TRUE;
			goto out;
		}
	}

out:
	return status;
}

static int transport_bio_buffered_puts(BIO* bio, const char* str)
{
	return 1;
}

static int transport_bio_buffered_gets(BIO* bio, char* str, int size)
{
	return 1;
}

static long transport_bio_buffered_ctrl(BIO* bio, int cmd, long arg1, void* arg2)
{
	int status = -1;
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*) bio->ptr;

	switch (cmd)
	{
		case BIO_CTRL_FLUSH:
			if (!ringbuffer_used(&ptr->xmitBuffer))
				status = 1;
			else
				status = (transport_bio_buffered_write(bio, NULL, 0) >= 0) ? 1 : -1;
			break;

		case BIO_CTRL_WPENDING:
			status = ringbuffer_used(&ptr->xmitBuffer);
			break;

		case BIO_CTRL_PENDING:
			status = 0;
			break;

		case BIO_C_READ_BLOCKED:
			status = (int) ptr->readBlocked;
			break;

		case BIO_C_WRITE_BLOCKED:
			status = (int) ptr->writeBlocked;
			break;

		default:
			status = BIO_ctrl(bio->next_bio, cmd, arg1, arg2);
			break;
	}

	return status;
}

static int transport_bio_buffered_new(BIO* bio)
{
	WINPR_BIO_BUFFERED_SOCKET* ptr;

	bio->init = 1;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;

	ptr = (WINPR_BIO_BUFFERED_SOCKET*) calloc(1, sizeof(WINPR_BIO_BUFFERED_SOCKET));

	if (!ptr)
		return -1;

	bio->ptr = (void*) ptr;

	if (!ringbuffer_init(&ptr->xmitBuffer, 0x10000))
		return -1;

	return 1;
}

static int transport_bio_buffered_free(BIO* bio)
{
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*) bio->ptr;

	if (bio->next_bio)
	{
		BIO_free(bio->next_bio);
		bio->next_bio = NULL;
	}

	ringbuffer_destroy(&ptr->xmitBuffer);

	free(ptr);

	return 1;
}

static BIO_METHOD transport_bio_buffered_socket_methods =
{
	BIO_TYPE_BUFFERED,
	"BufferedSocket",
	transport_bio_buffered_write,
	transport_bio_buffered_read,
	transport_bio_buffered_puts,
	transport_bio_buffered_gets,
	transport_bio_buffered_ctrl,
	transport_bio_buffered_new,
	transport_bio_buffered_free,
	NULL,
};

BIO_METHOD* BIO_s_buffered_socket(void)
{
	return &transport_bio_buffered_socket_methods;
}

char* freerdp_tcp_get_ip_address(int sockfd)
{
	BYTE* ip;
	socklen_t length;
	char ipAddress[32];
	struct sockaddr_in sockaddr;

	length = sizeof(sockaddr);
	ZeroMemory(&sockaddr, length);

	if (getsockname(sockfd, (struct sockaddr*) &sockaddr, &length) == 0)
	{
		ip = (BYTE*) (&sockaddr.sin_addr);
		sprintf_s(ipAddress, sizeof(ipAddress), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	}
	else
	{
		strcpy(ipAddress, "127.0.0.1");
	}

	return _strdup(ipAddress);
}

int freerdp_uds_connect(const char* path)
{
#ifndef _WIN32
	int status;
	int sockfd;
	struct sockaddr_un addr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sockfd == -1)
	{
		WLog_ERR(TAG, "socket");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	status = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));

	if (status < 0)
	{
		WLog_ERR(TAG, "connect");
		close(sockfd);
		return -1;
	}

	return sockfd;
#else /* ifndef _WIN32 */
	return -1;
#endif
}

BOOL freerdp_tcp_resolve_hostname(const char* hostname)
{
	int status;
	struct addrinfo hints = { 0 };
	struct addrinfo* result = NULL;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	status = getaddrinfo(hostname, NULL, &hints, &result);

	if (status)
		return FALSE;

	freeaddrinfo(result);

	return TRUE;
}

BOOL freerdp_tcp_connect_timeout(int sockfd, struct sockaddr* addr, socklen_t addrlen, int timeout)
{
	int status;

#ifndef _WIN32
	int flags;
	fd_set cfds;
	socklen_t optlen;
	struct timeval tv;

	/* set socket in non-blocking mode */

	flags = fcntl(sockfd, F_GETFL);

	if (flags < 0)
		return FALSE;

	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	/* non-blocking tcp connect */

	status = connect(sockfd, addr, addrlen);

	if (status >= 0)
		return TRUE; /* connection success */

	if (errno != EINPROGRESS)
		return FALSE;

	FD_ZERO(&cfds);
	FD_SET(sockfd, &cfds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	status = _select(sockfd + 1, NULL, &cfds, NULL, &tv);

	if (status != 1)
		return FALSE; /* connection timeout or error */

	status = 0;
	optlen = sizeof(status);

	if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*) &status, &optlen) < 0)
		return FALSE;

	if (status != 0)
		return FALSE;

	/* set socket in blocking mode */

	flags = fcntl(sockfd, F_GETFL);

	if (flags < 0)
		return FALSE;

	fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

#else
	status = connect(sockfd, addr, addrlen);

	if (status >= 0)
		return TRUE;

	return FALSE;
#endif

	return TRUE;
}

#ifndef _WIN32

int freerdp_tcp_connect_multi(char** hostnames, int count, int port, int timeout)
{
	int index;
	int sindex;
	int status;
	int flags;
	int maxfds;
	fd_set cfds;
	int sockfd;
	int* sockfds;
	char port_str[16];
	socklen_t optlen;
	struct timeval tv;
	struct addrinfo hints;
	struct addrinfo* addr;
	struct addrinfo* result;
	struct addrinfo** addrs;
	struct addrinfo** results;

	sindex = -1;

	sprintf_s(port_str, sizeof(port_str) - 1, "%u", port);

	sockfds = (int*) calloc(count, sizeof(int));
	addrs = (struct addrinfo**) calloc(count, sizeof(struct addrinfo*));
	results = (struct addrinfo**) calloc(count, sizeof(struct addrinfo*));

	for (index = 0; index < count; index++)
	{
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		status = getaddrinfo(hostnames[index], port_str, &hints, &result);

		if (status)
		{
			continue;
		}

		addr = result;

		if ((addr->ai_family == AF_INET6) && (addr->ai_next != 0))
		{
			while ((addr = addr->ai_next))
			{
				if (addr->ai_family == AF_INET)
					break;
			}

			if (!addr)
				addr = result;
		}

		sockfds[index] = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

		if (sockfds[index] < 0)
		{
			freeaddrinfo(result);
			sockfds[index] = 0;
			continue;
		}

		addrs[index] = addr;
		results[index] = result;
	}

	maxfds = 0;
	FD_ZERO(&cfds);

	for (index = 0; index < count; index++)
	{
		if (!sockfds[index])
			continue;

		sockfd = sockfds[index];
		addr = addrs[index];

		/* set socket in non-blocking mode */

		flags = fcntl(sockfd, F_GETFL);

		if (flags < 0)
		{
			sockfds[index] = 0;
			continue;
		}

		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

		/* non-blocking tcp connect */

		status = connect(sockfd, addr->ai_addr, addr->ai_addrlen);

		if (status >= 0)
		{
			/* connection success */
			break;
		}

		if (errno != EINPROGRESS)
		{
			sockfds[index] = 0;
			continue;
		}

		FD_SET(sockfd, &cfds);

		if (sockfd > maxfds)
			maxfds = sockfd;
	}

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	status = _select(maxfds + 1, NULL, &cfds, NULL, &tv);

	for (index = 0; index < count; index++)
	{
		if (!sockfds[index])
			continue;

		sockfd = sockfds[index];

		if (FD_ISSET(sockfd, &cfds))
		{
			status = 0;
			optlen = sizeof(status);

			if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*) &status, &optlen) < 0)
			{
				sockfds[index] = 0;
				continue;
			}

			if (status != 0)
			{
				sockfds[index] = 0;
				continue;
			}

			/* set socket in blocking mode */

			flags = fcntl(sockfd, F_GETFL);

			if (flags < 0)
			{
				sockfds[index] = 0;
				continue;
			}

			fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

			sindex = index;
			break;
		}
	}

	if (sindex >= 0)
	{
		sockfd = sockfds[sindex];
	}

	for (index = 0; index < count; index++)
	{
		if (results[index])
			freeaddrinfo(results[index]);
	}

	free(addrs);
	free(results);
	free(sockfds);

	return sockfd;
}

#endif

BOOL freerdp_tcp_set_keep_alive_mode(int sockfd)
{
#ifndef _WIN32
	UINT32 optval;
	socklen_t optlen;

	optval = 1;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_SOCKET, SO_KEEPALIVE");
	}

#ifdef TCP_KEEPIDLE
	optval = 5;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() IPPROTO_TCP, TCP_KEEPIDLE");
	}
#endif

#ifdef TCP_KEEPCNT
	optval = 3;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_KEEPCNT");
	}
#endif

#ifdef TCP_KEEPINTVL
	optval = 2;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_KEEPINTVL");
	}
#endif
#endif

#if defined(__MACOSX__) || defined(__IOS__)
	optval = 1;
	optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_SOCKET, SO_NOSIGPIPE");
	}
#endif

#ifdef TCP_USER_TIMEOUT
	optval = 4000;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_USER_TIMEOUT, (void*) &optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_USER_TIMEOUT");
	}
#endif

	return TRUE;
}

int freerdp_tcp_connect(rdpSettings* settings, const char* hostname, int port, int timeout)
{
	int status;
	int sockfd;
	UINT32 optval;
	socklen_t optlen;
	BOOL ipcSocket = FALSE;

	if (!hostname)
		return -1;

	if (hostname[0] == '/')
		ipcSocket = TRUE;

	if (ipcSocket)
	{
		sockfd = freerdp_uds_connect(hostname);

		if (sockfd < 0)
			return -1;
	}
	else
	{
		sockfd = -1;

		if (!settings->GatewayEnabled)
		{
			if (!freerdp_tcp_resolve_hostname(hostname))
			{
				if (settings->TargetNetAddressCount > 0)
				{
#ifndef _WIN32
					sockfd = freerdp_tcp_connect_multi(settings->TargetNetAddresses,
							settings->TargetNetAddressCount, port, timeout);
#else
					hostname = settings->TargetNetAddresses[0];
#endif
				}
			}
		}

		if (sockfd <= 0)
		{
			char port_str[16];
			struct addrinfo hints;
			struct addrinfo* addr;
			struct addrinfo* result;

			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;

			sprintf_s(port_str, sizeof(port_str) - 1, "%u", port);

			status = getaddrinfo(hostname, port_str, &hints, &result);

			if (status)
			{
				WLog_ERR(TAG, "getaddrinfo: %s", gai_strerror(status));
				return -1;
			}

			addr = result;

			if ((addr->ai_family == AF_INET6) && (addr->ai_next != 0))
			{
				while ((addr = addr->ai_next))
				{
					if (addr->ai_family == AF_INET)
						break;
				}

				if (!addr)
					addr = result;
			}

			sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

			if (sockfd  < 0)
			{
				freeaddrinfo(result);
				return -1;
			}

			if (!freerdp_tcp_connect_timeout(sockfd, addr->ai_addr, addr->ai_addrlen, timeout))
			{
				fprintf(stderr, "failed to connect to %s\n", hostname);
				freeaddrinfo(result);
				return -1;
			}

			freeaddrinfo(result);
		}
	}

	settings->IPv6Enabled = FALSE;

	free(settings->ClientAddress);
	settings->ClientAddress = freerdp_tcp_get_ip_address(sockfd);

	optval = 1;
	optlen = sizeof(optval);

	if (!ipcSocket)
	{
		if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*) &optval, optlen) < 0)
			WLog_ERR(TAG, "unable to set TCP_NODELAY");
	}

	/* receive buffer must be a least 32 K */
	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &optval, &optlen) == 0)
	{
		if (optval < (1024 * 32))
		{
			optval = 1024 * 32;
			optlen = sizeof(optval);

			if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &optval, optlen) < 0)
			{
				WLog_ERR(TAG, "unable to set receive buffer len");
				return -1;
			}
		}
	}

	if (!ipcSocket)
	{
		if (!freerdp_tcp_set_keep_alive_mode(sockfd))
			return -1;
	}

	return sockfd;
}
