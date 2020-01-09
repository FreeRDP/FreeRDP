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
#include <winpr/platform.h>
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

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#endif

#ifdef __APPLE__
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
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
#include "../crypto/opensslcompat.h"

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

static long transport_bio_simple_callback(BIO* bio, int mode, const char* argp, int argi, long argl,
                                          long ret)
{
	return 1;
}

static int transport_bio_simple_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status = 0;
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);
	status = _send(ptr->socket, buf, size, 0);

	if (status <= 0)
	{
		error = WSAGetLastError();

		if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) || (error == WSAEINPROGRESS) ||
		    (error == WSAEALREADY))
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
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_READ);
	WSAResetEvent(ptr->hEvent);
	status = _recv(ptr->socket, buf, size, 0);

	if (status > 0)
	{
		return status;
	}

	if (status == 0)
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
		return 0;
	}

	error = WSAGetLastError();

	if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) || (error == WSAEINPROGRESS) ||
	    (error == WSAEALREADY))
	{
		BIO_set_flags(bio, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
	}
	else
	{
		BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	}

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
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);

	if (cmd == BIO_C_SET_SOCKET)
	{
		transport_bio_simple_uninit(bio);
		transport_bio_simple_init(bio, (SOCKET)arg2, (int)arg1);
		return 1;
	}
	else if (cmd == BIO_C_GET_SOCKET)
	{
		if (!BIO_get_init(bio) || !arg2)
			return 0;

		*((SOCKET*)arg2) = ptr->socket;
		return 1;
	}
	else if (cmd == BIO_C_GET_EVENT)
	{
		if (!BIO_get_init(bio) || !arg2)
			return 0;

		*((HANDLE*)arg2) = ptr->hEvent;
		return 1;
	}
	else if (cmd == BIO_C_SET_NONBLOCK)
	{
#ifndef _WIN32
		int flags;
		flags = fcntl((int)ptr->socket, F_GETFL);

		if (flags == -1)
			return 0;

		if (arg1)
			fcntl((int)ptr->socket, F_SETFL, flags | O_NONBLOCK);
		else
			fcntl((int)ptr->socket, F_SETFL, flags & ~(O_NONBLOCK));

#else
		/* the internal socket is always non-blocking */
#endif
		return 1;
	}
	else if (cmd == BIO_C_WAIT_READ)
	{
		int timeout = (int)arg1;
		int sockfd = (int)ptr->socket;
#ifdef HAVE_POLL_H
		struct pollfd pollset;
		pollset.fd = sockfd;
		pollset.events = POLLIN;
		pollset.revents = 0;

		do
		{
			status = poll(&pollset, 1, timeout);
		} while ((status < 0) && (errno == EINTR));

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
		} while ((status < 0) && (errno == EINTR));

#endif
	}
	else if (cmd == BIO_C_WAIT_WRITE)
	{
		int timeout = (int)arg1;
		int sockfd = (int)ptr->socket;
#ifdef HAVE_POLL_H
		struct pollfd pollset;
		pollset.fd = sockfd;
		pollset.events = POLLOUT;
		pollset.revents = 0;

		do
		{
			status = poll(&pollset, 1, timeout);
		} while ((status < 0) && (errno == EINTR));

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
		} while ((status < 0) && (errno == EINTR));

#endif
	}

	switch (cmd)
	{
		case BIO_C_SET_FD:
			if (arg2)
			{
				transport_bio_simple_uninit(bio);
				transport_bio_simple_init(bio, (SOCKET) * ((int*)arg2), (int)arg1);
				status = 1;
			}

			break;

		case BIO_C_GET_FD:
			if (BIO_get_init(bio))
			{
				if (arg2)
					*((int*)arg2) = (int)ptr->socket;

				status = (int)ptr->socket;
			}

			break;

		case BIO_CTRL_GET_CLOSE:
			status = BIO_get_shutdown(bio);
			break;

		case BIO_CTRL_SET_CLOSE:
			BIO_set_shutdown(bio, (int)arg1);
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
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);
	ptr->socket = socket;
	BIO_set_shutdown(bio, shutdown);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	BIO_set_init(bio, 1);
	ptr->hEvent = WSACreateEvent();

	if (!ptr->hEvent)
		return 0;

	/* WSAEventSelect automatically sets the socket in non-blocking mode */
	if (WSAEventSelect(ptr->socket, ptr->hEvent, FD_READ | FD_ACCEPT | FD_CLOSE))
	{
		WLog_ERR(TAG, "WSAEventSelect returned 0x%08X", WSAGetLastError());
		return 0;
	}

	return 1;
}

static int transport_bio_simple_uninit(BIO* bio)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);

	if (BIO_get_shutdown(bio))
	{
		if (BIO_get_init(bio) && ptr)
		{
			_shutdown(ptr->socket, SD_BOTH);
			closesocket(ptr->socket);
			ptr->socket = 0;
		}
	}

	if (ptr && ptr->hEvent)
	{
		CloseHandle(ptr->hEvent);
		ptr->hEvent = NULL;
	}

	BIO_set_init(bio, 0);
	BIO_set_flags(bio, 0);
	return 1;
}

static int transport_bio_simple_new(BIO* bio)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr;
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	ptr = (WINPR_BIO_SIMPLE_SOCKET*)calloc(1, sizeof(WINPR_BIO_SIMPLE_SOCKET));

	if (!ptr)
		return 0;

	BIO_set_data(bio, ptr);
	return 1;
}

static int transport_bio_simple_free(BIO* bio)
{
	WINPR_BIO_SIMPLE_SOCKET* ptr;

	if (!bio)
		return 0;

	transport_bio_simple_uninit(bio);
	ptr = (WINPR_BIO_SIMPLE_SOCKET*)BIO_get_data(bio);

	if (ptr)
	{
		BIO_set_data(bio, NULL);
		free(ptr);
	}

	return 1;
}

BIO_METHOD* BIO_s_simple_socket(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_SIMPLE, "SimpleSocket")))
			return NULL;

		BIO_meth_set_write(bio_methods, transport_bio_simple_write);
		BIO_meth_set_read(bio_methods, transport_bio_simple_read);
		BIO_meth_set_puts(bio_methods, transport_bio_simple_puts);
		BIO_meth_set_gets(bio_methods, transport_bio_simple_gets);
		BIO_meth_set_ctrl(bio_methods, transport_bio_simple_ctrl);
		BIO_meth_set_create(bio_methods, transport_bio_simple_new);
		BIO_meth_set_destroy(bio_methods, transport_bio_simple_free);
	}

	return bio_methods;
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

static long transport_bio_buffered_callback(BIO* bio, int mode, const char* argp, int argi,
                                            long argl, long ret)
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
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*)BIO_get_data(bio);
	BIO* next_bio = NULL;
	ret = num;
	ptr->writeBlocked = FALSE;
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	/* we directly append extra bytes in the xmit buffer, this could be prevented
	 * but for now it makes the code more simple.
	 */
	if (buf && num && !ringbuffer_write(&ptr->xmitBuffer, (const BYTE*)buf, num))
	{
		WLog_ERR(TAG, "an error occurred when writing (num: %d)", num);
		return -1;
	}

	committedBytes = 0;
	nchunks = ringbuffer_peek(&ptr->xmitBuffer, chunks, ringbuffer_used(&ptr->xmitBuffer));
	next_bio = BIO_next(bio);

	for (i = 0; i < nchunks; i++)
	{
		while (chunks[i].size)
		{
			status = BIO_write(next_bio, chunks[i].data, chunks[i].size);

			if (status <= 0)
			{
				if (!BIO_should_retry(next_bio))
				{
					BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
					ret = -1; /* fatal error */
					goto out;
				}

				if (BIO_should_write(next_bio))
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
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*)BIO_get_data(bio);
	BIO* next_bio = BIO_next(bio);
	ptr->readBlocked = FALSE;
	BIO_clear_flags(bio, BIO_FLAGS_READ);
	status = BIO_read(next_bio, buf, size);

	if (status <= 0)
	{
		if (!BIO_should_retry(next_bio))
		{
			BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
			goto out;
		}

		BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);

		if (BIO_should_read(next_bio))
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
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*)BIO_get_data(bio);

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
			status = (int)ptr->readBlocked;
			break;

		case BIO_C_WRITE_BLOCKED:
			status = (int)ptr->writeBlocked;
			break;

		default:
			status = BIO_ctrl(BIO_next(bio), cmd, arg1, arg2);
			break;
	}

	return status;
}

static int transport_bio_buffered_new(BIO* bio)
{
	WINPR_BIO_BUFFERED_SOCKET* ptr;
	BIO_set_init(bio, 1);
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);
	ptr = (WINPR_BIO_BUFFERED_SOCKET*)calloc(1, sizeof(WINPR_BIO_BUFFERED_SOCKET));

	if (!ptr)
		return -1;

	BIO_set_data(bio, (void*)ptr);

	if (!ringbuffer_init(&ptr->xmitBuffer, 0x10000))
		return -1;

	return 1;
}

/* Free the buffered BIO.
 * Do not free other elements in the BIO stack,
 * let BIO_free_all handle that. */
static int transport_bio_buffered_free(BIO* bio)
{
	WINPR_BIO_BUFFERED_SOCKET* ptr = (WINPR_BIO_BUFFERED_SOCKET*)BIO_get_data(bio);

	if (!ptr)
		return 0;

	ringbuffer_destroy(&ptr->xmitBuffer);
	free(ptr);
	return 1;
}

BIO_METHOD* BIO_s_buffered_socket(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_BUFFERED, "BufferedSocket")))
			return NULL;

		BIO_meth_set_write(bio_methods, transport_bio_buffered_write);
		BIO_meth_set_read(bio_methods, transport_bio_buffered_read);
		BIO_meth_set_puts(bio_methods, transport_bio_buffered_puts);
		BIO_meth_set_gets(bio_methods, transport_bio_buffered_gets);
		BIO_meth_set_ctrl(bio_methods, transport_bio_buffered_ctrl);
		BIO_meth_set_create(bio_methods, transport_bio_buffered_new);
		BIO_meth_set_destroy(bio_methods, transport_bio_buffered_free);
	}

	return bio_methods;
}

char* freerdp_tcp_address_to_string(const struct sockaddr_storage* addr, BOOL* pIPv6)
{
	char ipAddress[INET6_ADDRSTRLEN + 1] = { 0 };
	struct sockaddr_in6* sockaddr_ipv6 = (struct sockaddr_in6*)addr;
	struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)addr;

	if (addr == NULL)
	{
		return NULL;
	}

	switch (sockaddr_ipv4->sin_family)
	{
		case AF_INET:
			if (!inet_ntop(sockaddr_ipv4->sin_family, &sockaddr_ipv4->sin_addr, ipAddress,
			               sizeof(ipAddress)))
				return NULL;

			break;

		case AF_INET6:
			if (!inet_ntop(sockaddr_ipv6->sin6_family, &sockaddr_ipv6->sin6_addr, ipAddress,
			               sizeof(ipAddress)))
				return NULL;

			break;

		case AF_UNIX:
			sprintf_s(ipAddress, ARRAYSIZE(ipAddress), "127.0.0.1");
			break;

		default:
			return NULL;
	}

	if (pIPv6 != NULL)
	{
		*pIPv6 = (sockaddr_ipv4->sin_family == AF_INET6);
	}

	return _strdup(ipAddress);
}

static char* freerdp_tcp_get_ip_address(int sockfd, BOOL* pIPv6)
{
	struct sockaddr_storage saddr = { 0 };
	socklen_t length = sizeof(struct sockaddr_storage);

	if (getsockname(sockfd, (struct sockaddr*)&saddr, &length) != 0)
	{
		return NULL;
	}

	return freerdp_tcp_address_to_string(&saddr, pIPv6);
}

char* freerdp_tcp_get_peer_address(SOCKET sockfd)
{
	struct sockaddr_storage saddr = { 0 };
	socklen_t length = sizeof(struct sockaddr_storage);

	if (getpeername(sockfd, (struct sockaddr*)&saddr, &length) != 0)
	{
		return NULL;
	}

	return freerdp_tcp_address_to_string(&saddr, NULL);
}

static int freerdp_uds_connect(const char* path)
{
#ifndef _WIN32
	int status;
	int sockfd;
	struct sockaddr_un addr = { 0 };
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sockfd == -1)
	{
		WLog_ERR(TAG, "socket");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
	status = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

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

struct addrinfo* freerdp_tcp_resolve_host(const char* hostname, int port, int ai_flags)
{
	char* service = NULL;
	char port_str[16];
	int status;
	struct addrinfo hints = { 0 };
	struct addrinfo* result = NULL;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = ai_flags;

	if (port >= 0)
	{
		sprintf_s(port_str, sizeof(port_str) - 1, "%d", port);
		service = port_str;
	}

	status = getaddrinfo(hostname, service, &hints, &result);

	if (status)
	{
		freeaddrinfo(result);
		return NULL;
	}

	return result;
}

static BOOL freerdp_tcp_is_hostname_resolvable(rdpContext* context, const char* hostname)
{
	struct addrinfo* result = freerdp_tcp_resolve_host(hostname, -1, 0);

	if (!result)
	{
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_DNS_NAME_NOT_FOUND);

		return FALSE;
	}

	freerdp_set_last_error_log(context, 0);
	freeaddrinfo(result);
	return TRUE;
}

static BOOL freerdp_tcp_connect_timeout(rdpContext* context, int sockfd, struct sockaddr* addr,
                                        socklen_t addrlen, int timeout)
{
	BOOL rc = FALSE;
	HANDLE handles[2];
	int status = 0;
	int count = 0;
	u_long arg = 0;
	DWORD tout = (timeout > 0) ? (DWORD)timeout * 1000U : INFINITE;

	handles[count] = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!handles[count])
		return FALSE;

	status = WSAEventSelect(sockfd, handles[count++], FD_READ | FD_WRITE | FD_CONNECT | FD_CLOSE);

	if (status < 0)
	{
		WLog_ERR(TAG, "WSAEventSelect failed with %d", WSAGetLastError());
		goto fail;
	}

	handles[count++] = context->abortEvent;
	status = _connect(sockfd, addr, addrlen);

	if (status < 0)
	{
		status = WSAGetLastError();

		switch (status)
		{
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				break;

			default:
				goto fail;
		}
	}

	status = WaitForMultipleObjects(count, handles, FALSE, tout);

	if (WAIT_OBJECT_0 != status)
	{
		if (status == WAIT_OBJECT_0 + 1)
			freerdp_set_last_error_log(context, FREERDP_ERROR_CONNECT_CANCELLED);

		goto fail;
	}

	status = recv(sockfd, NULL, 0, 0);

	if (status == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSAECONNRESET)
			goto fail;
	}

	status = WSAEventSelect(sockfd, handles[0], 0);

	if (status < 0)
	{
		WLog_ERR(TAG, "WSAEventSelect failed with %d", WSAGetLastError());
		goto fail;
	}

	if (_ioctlsocket(sockfd, FIONBIO, &arg) != 0)
		goto fail;

	rc = TRUE;
fail:
	CloseHandle(handles[0]);
	return rc;
}

typedef struct
{
	SOCKET s;
	struct addrinfo* addr;
	struct addrinfo* result;
} t_peer;

static void peer_free(t_peer* peer)
{
	if (peer->s != INVALID_SOCKET)
		closesocket(peer->s);

	freeaddrinfo(peer->addr);
	memset(peer, 0, sizeof(t_peer));
	peer->s = INVALID_SOCKET;
}

static int freerdp_tcp_connect_multi(rdpContext* context, char** hostnames, UINT32* ports,
                                     UINT32 count, int port, int timeout)
{
	UINT32 index;
	UINT32 sindex = count;
	int status = -1;
	SOCKET sockfd = INVALID_SOCKET;
	HANDLE* events;
	struct addrinfo* addr;
	struct addrinfo* result;
	t_peer* peers;
	events = (HANDLE*)calloc(count + 1, sizeof(HANDLE));
	peers = (t_peer*)calloc(count, sizeof(t_peer));

	if (!peers || !events || (count < 1))
	{
		free(peers);
		free(events);
		return -1;
	}

	for (index = 0; index < count; index++)
	{
		int curPort = port;

		if (ports)
			curPort = ports[index];

		result = freerdp_tcp_resolve_host(hostnames[index], curPort, 0);

		if (!result)
			continue;

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

		peers[index].s = _socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

		if (peers[index].s == INVALID_SOCKET)
		{
			freeaddrinfo(result);
			continue;
		}

		peers[index].addr = addr;
		peers[index].result = result;
	}

	for (index = 0; index < count; index++)
	{
		sockfd = peers[index].s;
		addr = peers[index].addr;

		if ((sockfd == INVALID_SOCKET) || (!addr))
			continue;

		/* blocking tcp connect */
		status = _connect(sockfd, addr->ai_addr, addr->ai_addrlen);

		if (status >= 0)
		{
			/* connection success */
			sindex = index;
			break;
		}
	}

	if (sindex < count)
	{
		sockfd = peers[sindex].s;
		peers[sindex].s = INVALID_SOCKET;
	}
	else
		freerdp_set_last_error_log(context, FREERDP_ERROR_CONNECT_CANCELLED);

	for (index = 0; index < count; index++)
		peer_free(&peers[index]);

	free(peers);
	free(events);
	return sockfd;
}

static BOOL freerdp_tcp_set_keep_alive_mode(const rdpSettings* settings, int sockfd)
{
	const BOOL keepalive = (freerdp_settings_get_bool(settings, FreeRDP_TcpKeepAlive));
	UINT32 optval;
	socklen_t optlen;
	optval = keepalive ? 1 : 0;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_SOCKET, SO_KEEPALIVE");
	}

#ifndef _WIN32
#ifdef TCP_KEEPIDLE
	optval = keepalive ? freerdp_settings_get_uint32(settings, FreeRDP_TcpKeepAliveDelay) : 0;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() IPPROTO_TCP, TCP_KEEPIDLE");
	}

#endif
#ifndef SOL_TCP
	/* "tcp" from /etc/protocols as getprotobyname(3C) */
#define SOL_TCP 6
#endif
#ifdef TCP_KEEPCNT
	optval = keepalive ? freerdp_settings_get_uint32(settings, FreeRDP_TcpKeepAliveRetries) : 0;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_KEEPCNT");
	}

#endif
#ifdef TCP_KEEPINTVL
	optval = keepalive ? freerdp_settings_get_uint32(settings, FreeRDP_TcpKeepAliveInterval) : 0;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_KEEPINTVL");
	}

#endif
#endif
#if defined(__MACOSX__) || defined(__IOS__)
	optval = 1;
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_SOCKET, SO_NOSIGPIPE");
	}

#endif
#ifdef TCP_USER_TIMEOUT
	optval = freerdp_settings_get_uint32(settings, FreeRDP_TcpAckTimeout);
	optlen = sizeof(optval);

	if (setsockopt(sockfd, SOL_TCP, TCP_USER_TIMEOUT, (void*)&optval, optlen) < 0)
	{
		WLog_WARN(TAG, "setsockopt() SOL_TCP, TCP_USER_TIMEOUT");
	}

#endif
	return TRUE;
}

int freerdp_tcp_connect(rdpContext* context, rdpSettings* settings, const char* hostname, int port,
                        int timeout)
{
	int sockfd;
	UINT32 optval;
	socklen_t optlen;
	BOOL ipcSocket = FALSE;
	BOOL useExternalDefinedSocket = FALSE;

	if (!hostname)
	{
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

		return -1;
	}

	if (hostname[0] == '/')
		ipcSocket = TRUE;

	if (hostname[0] == '|')
		useExternalDefinedSocket = TRUE;

	if (ipcSocket)
	{
		sockfd = freerdp_uds_connect(hostname);

		if (sockfd < 0)
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

			return -1;
		}
	}
	else if (useExternalDefinedSocket)
		sockfd = port;
	else
	{
		sockfd = -1;

		if (!settings->GatewayEnabled)
		{
			if (!freerdp_tcp_is_hostname_resolvable(context, hostname) ||
			    settings->RemoteAssistanceMode)
			{
				if (settings->TargetNetAddressCount > 0)
				{
					sockfd = freerdp_tcp_connect_multi(
					    context, settings->TargetNetAddresses, settings->TargetNetPorts,
					    settings->TargetNetAddressCount, port, timeout);
				}
			}
		}

		if (sockfd <= 0)
		{
			char* peerAddress;
			struct addrinfo* addr;
			struct addrinfo* result;

			result = freerdp_tcp_resolve_host(hostname, port, 0);

			if (!result)
			{
				freerdp_set_last_error_if_not(context, FREERDP_ERROR_DNS_NAME_NOT_FOUND);

				return -1;
			}
			freerdp_set_last_error_log(context, 0);

			addr = result;

			if ((addr->ai_family == AF_INET6) && (addr->ai_next != 0) &&
			    !settings->PreferIPv6OverIPv4)
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

			if (sockfd < 0)
			{
				freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

				freeaddrinfo(result);
				return -1;
			}

			if ((peerAddress = freerdp_tcp_address_to_string(
			         (const struct sockaddr_storage*)addr->ai_addr, NULL)) != NULL)
			{
				WLog_DBG(TAG, "connecting to peer %s", peerAddress);
				free(peerAddress);
			}

			if (!freerdp_tcp_connect_timeout(context, sockfd, addr->ai_addr, addr->ai_addrlen,
			                                 timeout))
			{
				freeaddrinfo(result);
				close(sockfd);

				freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

				WLog_ERR(TAG, "failed to connect to %s", hostname);
				return -1;
			}

			freeaddrinfo(result);
		}
	}

	free(settings->ClientAddress);
	settings->ClientAddress = freerdp_tcp_get_ip_address(sockfd, &settings->IPv6Enabled);

	if (!settings->ClientAddress)
	{
		if (!useExternalDefinedSocket)
			close(sockfd);

		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

		WLog_ERR(TAG, "Couldn't get socket ip address");
		return -1;
	}

	optval = 1;
	optlen = sizeof(optval);

	if (!ipcSocket && !useExternalDefinedSocket)
	{
		if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&optval, optlen) < 0)
			WLog_ERR(TAG, "unable to set TCP_NODELAY");
	}

	/* receive buffer must be a least 32 K */
	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*)&optval, &optlen) == 0)
	{
		if (optval < (1024 * 32))
		{
			optval = 1024 * 32;
			optlen = sizeof(optval);

			if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*)&optval, optlen) < 0)
			{
				close(sockfd);

				freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

				WLog_ERR(TAG, "unable to set receive buffer len");
				return -1;
			}
		}
	}

	if (!ipcSocket && !useExternalDefinedSocket)
	{
		if (!freerdp_tcp_set_keep_alive_mode(settings, sockfd))
		{
			close(sockfd);

			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_FAILED);

			WLog_ERR(TAG, "Couldn't set keep alive mode.");
			return -1;
		}
	}

	if (WaitForSingleObject(context->abortEvent, 0) == WAIT_OBJECT_0)
	{
		close(sockfd);

		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);

		return -1;
	}

	return sockfd;
}
