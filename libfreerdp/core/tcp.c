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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include <winpr/crt.h>
#include <winpr/winsock.h>

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#else
#include <time.h>
#include <sys/select.h>
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

#endif

#include <freerdp/utils/tcp.h>
#include <freerdp/utils/uds.h>
#include <winpr/stream.h>

#include "tcp.h"

/* Simple Socket BIO */

static int transport_bio_simple_new(BIO* bio);
static int transport_bio_simple_free(BIO* bio);

long transport_bio_simple_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_simple_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status = 0;

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	status = _send((SOCKET) bio->num, buf, size, 0);

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

	if (!buf)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_READ);

	status = _recv((SOCKET) bio->num, buf, size, 0);
	if (status > 0)
		return status;

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

	switch (cmd)
	{
		case BIO_C_SET_FD:
			if (arg2)
			{
				transport_bio_simple_free(bio);
				bio->flags = BIO_FLAGS_SHOULD_RETRY;
				bio->num = *((int*) arg2);
				bio->shutdown = (int) arg1;
				bio->init = 1;
				status = 1;
			}
			break;

		case BIO_C_GET_FD:
			if (bio->init)
			{
				if (arg2)
					*((int*) arg2) = bio->num;
				status = bio->num;
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

static int transport_bio_simple_new(BIO* bio)
{
	bio->init = 0;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	return 1;
}

static int transport_bio_simple_free(BIO* bio)
{
	if (!bio)
		return 0;

	if (bio->shutdown)
	{
		if (bio->init)
			closesocket((SOCKET) bio->num);

		bio->init = 0;
		bio->flags = 0;
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

long transport_bio_buffered_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int transport_bio_buffered_write(BIO* bio, const char* buf, int num)
{
	int status, ret;
	rdpTcp* tcp = (rdpTcp*) bio->ptr;
	int nchunks, committedBytes, i;
	DataChunk chunks[2];

	ret = num;
	tcp->writeBlocked = FALSE;
	BIO_clear_flags(bio, BIO_FLAGS_WRITE);

	/* we directly append extra bytes in the xmit buffer, this could be prevented
	 * but for now it makes the code more simple.
	 */
	if (buf && num && !ringbuffer_write(&tcp->xmitBuffer, (const BYTE*) buf, num))
	{
		fprintf(stderr, "%s: an error occured when writing(toWrite=%d)\n", __FUNCTION__, num);
		return -1;
	}

	committedBytes = 0;
	nchunks = ringbuffer_peek(&tcp->xmitBuffer, chunks, ringbuffer_used(&tcp->xmitBuffer));

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
					tcp->writeBlocked = TRUE;
					goto out; /* EWOULDBLOCK */
				}
			}

			committedBytes += status;
			chunks[i].size -= status;
			chunks[i].data += status;
		}
	}

out:
	ringbuffer_commit_read_bytes(&tcp->xmitBuffer, committedBytes);
	return ret;
}

static int transport_bio_buffered_read(BIO* bio, char* buf, int size)
{
	int status;
	rdpTcp* tcp = (rdpTcp*) bio->ptr;

	tcp->readBlocked = FALSE;
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
			tcp->readBlocked = TRUE;
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
	rdpTcp* tcp = (rdpTcp*) bio->ptr;

	switch (cmd)
	{
		case BIO_CTRL_FLUSH:
			return 1;

		case BIO_CTRL_WPENDING:
			return ringbuffer_used(&tcp->xmitBuffer);

		case BIO_CTRL_PENDING:
			return 0;

		default:
			return BIO_ctrl(bio->next_bio, cmd, arg1, arg2);
	}

	return 0;
}

static int transport_bio_buffered_new(BIO* bio)
{
	bio->init = 1;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	return 1;
}

static int transport_bio_buffered_free(BIO* bio)
{
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

BOOL transport_bio_buffered_drain(BIO *bio)
{
	int status;
	rdpTcp* tcp = (rdpTcp*) bio->ptr;

	if (!ringbuffer_used(&tcp->xmitBuffer))
		return 1;

	status = transport_bio_buffered_write(bio, NULL, 0);

	return status >= 0;
}

void tcp_get_ip_address(rdpTcp* tcp)
{
	BYTE* ip;
	socklen_t length;
	struct sockaddr_in sockaddr;

	length = sizeof(sockaddr);
	ZeroMemory(&sockaddr, length);

	if (getsockname(tcp->sockfd, (struct sockaddr*) &sockaddr, &length) == 0)
	{
		ip = (BYTE*) (&sockaddr.sin_addr);
		sprintf_s(tcp->ip_address, sizeof(tcp->ip_address),
			 "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	}
	else
	{
		strcpy(tcp->ip_address, "127.0.0.1");
	}

	tcp->settings->IPv6Enabled = 0;

	free(tcp->settings->ClientAddress);
	tcp->settings->ClientAddress = _strdup(tcp->ip_address);
}

void tcp_get_mac_address(rdpTcp* tcp)
{
#ifdef LINUX
	BYTE* mac;
	struct ifreq if_req;
	struct if_nameindex* ni;

	ni = if_nameindex();
	mac = tcp->mac_address;

	while (ni->if_name != NULL)
	{
		if (strcmp(ni->if_name, "lo") != 0)
			break;

		ni++;
	}

	strncpy(if_req.ifr_name, ni->if_name, IF_NAMESIZE);

	if (ioctl(tcp->sockfd, SIOCGIFHWADDR, &if_req) != 0)
	{
		fprintf(stderr, "failed to obtain MAC address\n");
		return;
	}

	memmove((void*) mac, (void*) &if_req.ifr_ifru.ifru_hwaddr.sa_data[0], 6);
#endif

	/* fprintf(stderr, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); */
}

BOOL tcp_connect(rdpTcp* tcp, const char* hostname, int port, int timeout)
{
	int status;
	UINT32 option_value;
	socklen_t option_len;

	if (!hostname)
		return FALSE;

	if (hostname[0] == '/')
	{
		tcp->sockfd = freerdp_uds_connect(hostname);

		if (tcp->sockfd < 0)
			return FALSE;

		tcp->socketBio = BIO_new_fd(tcp->sockfd, 1);

		if (!tcp->socketBio)
			return FALSE;
	}
	else
	{
#ifdef HAVE_POLL_H
		struct pollfd pollfds;
#else
		fd_set cfds;
		struct timeval tv;
#endif

		tcp->socketBio = BIO_new(BIO_s_connect());

		if (!tcp->socketBio)
			return FALSE;

		if (BIO_set_conn_hostname(tcp->socketBio, hostname) < 0 || BIO_set_conn_int_port(tcp->socketBio, &port) < 0)
			return FALSE;

		BIO_set_nbio(tcp->socketBio, 1);

		status = BIO_do_connect(tcp->socketBio);

		if ((status <= 0) && !BIO_should_retry(tcp->socketBio))
			return FALSE;

		tcp->sockfd = BIO_get_fd(tcp->socketBio, NULL);

		if (tcp->sockfd < 0)
			return FALSE;

		if (status <= 0)
		{
#ifdef HAVE_POLL_H
			pollfds.fd = tcp->sockfd;
			pollfds.events = POLLOUT;
			pollfds.revents = 0;
			do
			{
				status = poll(&pollfds, 1, timeout * 1000);
			}
			while ((status < 0) && (errno == EINTR));
#else
			FD_ZERO(&cfds);
			FD_SET(tcp->sockfd, &cfds);

			tv.tv_sec = timeout;
			tv.tv_usec = 0;

			status = _select(tcp->sockfd + 1, NULL, &cfds, NULL, &tv);
#endif
			if (status == 0)
			{
				return FALSE; /* timeout */
			}
		}

		BIO_set_close(tcp->socketBio, BIO_NOCLOSE);
		BIO_free(tcp->socketBio);

		tcp->socketBio = BIO_new(BIO_s_simple_socket());

		if (!tcp->socketBio)
			return -1;

		BIO_set_fd(tcp->socketBio, tcp->sockfd, BIO_CLOSE);
	}

	SetEventFileDescriptor(tcp->event, tcp->sockfd);

	tcp_get_ip_address(tcp);
	tcp_get_mac_address(tcp);

	option_value = 1;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, IPPROTO_TCP, TCP_NODELAY, (void*) &option_value, option_len) < 0)
		fprintf(stderr, "%s: unable to set TCP_NODELAY\n", __FUNCTION__);

	/* receive buffer must be a least 32 K */
	if (getsockopt(tcp->sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &option_value, &option_len) == 0)
	{
		if (option_value < (1024 * 32))
		{
			option_value = 1024 * 32;
			option_len = sizeof(option_value);

			if (setsockopt(tcp->sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &option_value, option_len) < 0)
			{
				fprintf(stderr, "%s: unable to set receive buffer len\n", __FUNCTION__);
				return FALSE;
			}
		}
	}

	if (!tcp_set_keep_alive_mode(tcp))
		return FALSE;

	tcp->bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!tcp->bufferedBio)
		return FALSE;

	tcp->bufferedBio->ptr = tcp;

	tcp->bufferedBio = BIO_push(tcp->bufferedBio, tcp->socketBio);

	return TRUE;
}

BOOL tcp_disconnect(rdpTcp* tcp)
{
	freerdp_tcp_disconnect(tcp->sockfd);
	tcp->sockfd = -1;

	return TRUE;
}

BOOL tcp_set_blocking_mode(rdpTcp* tcp, BOOL blocking)
{
#ifndef _WIN32
	int flags;
	flags = fcntl(tcp->sockfd, F_GETFL);

	if (flags == -1)
	{
		fprintf(stderr, "%s: fcntl failed, %s.\n", __FUNCTION__, strerror(errno));
		return FALSE;
	}

	if (blocking == TRUE)
		fcntl(tcp->sockfd, F_SETFL, flags & ~(O_NONBLOCK));
	else
		fcntl(tcp->sockfd, F_SETFL, flags | O_NONBLOCK);
#else
	int status;
	u_long arg = blocking;

	status = ioctlsocket(tcp->sockfd, FIONBIO, &arg);

	if (status != NO_ERROR)
		fprintf(stderr, "ioctlsocket() failed with error: %ld\n", status);

	tcp->wsa_event = WSACreateEvent();
	WSAEventSelect(tcp->sockfd, tcp->wsa_event, FD_READ);
#endif

	return TRUE;
}

BOOL tcp_set_keep_alive_mode(rdpTcp* tcp)
{
#ifndef _WIN32
	UINT32 option_value;
	socklen_t option_len;

	option_value = 1;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*) &option_value, option_len) < 0)
	{
		perror("setsockopt() SOL_SOCKET, SO_KEEPALIVE:");
		return FALSE;
	}

#ifdef TCP_KEEPIDLE
	option_value = 5;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void*) &option_value, option_len) < 0)
	{
		perror("setsockopt() IPPROTO_TCP, TCP_KEEPIDLE:");
		return FALSE;
	}
#endif

#ifdef TCP_KEEPCNT
	option_value = 3;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, SOL_TCP, TCP_KEEPCNT, (void *) &option_value, option_len) < 0)
	{
		perror("setsockopt() SOL_TCP, TCP_KEEPCNT:");
		return FALSE;
	}
#endif

#ifdef TCP_KEEPINTVL
	option_value = 2;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, SOL_TCP, TCP_KEEPINTVL, (void *) &option_value, option_len) < 0)
	{
		perror("setsockopt() SOL_TCP, TCP_KEEPINTVL:");
		return FALSE;
	}
#endif
#endif

#ifdef __MACOSX__
	option_value = 1;
	option_len = sizeof(option_value);
	if (setsockopt(tcp->sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void *) &option_value, option_len) < 0)
	{
		perror("setsockopt() SOL_SOCKET, SO_NOSIGPIPE:");
	}
#endif
	return TRUE;
}

int tcp_attach(rdpTcp* tcp, int sockfd)
{
	tcp->sockfd = sockfd;
	SetEventFileDescriptor(tcp->event, tcp->sockfd);

	ringbuffer_commit_read_bytes(&tcp->xmitBuffer, ringbuffer_used(&tcp->xmitBuffer));

	if (tcp->socketBio)
	{
		if (BIO_set_fd(tcp->socketBio, sockfd, 1) < 0)
			return -1;
	}
	else
	{
		tcp->socketBio = BIO_new(BIO_s_simple_socket());

		if (!tcp->socketBio)
			return -1;

		BIO_set_fd(tcp->socketBio, sockfd, BIO_CLOSE);
	}

	if (!tcp->bufferedBio)
	{
		tcp->bufferedBio = BIO_new(BIO_s_buffered_socket());

		if (!tcp->bufferedBio)
			return FALSE;

		tcp->bufferedBio->ptr = tcp;

		tcp->bufferedBio = BIO_push(tcp->bufferedBio, tcp->socketBio);
	}

	return 0;
}

HANDLE tcp_get_event_handle(rdpTcp* tcp)
{
	if (!tcp)
		return NULL;
	
#ifndef _WIN32
	return tcp->event;
#else
	return (HANDLE) tcp->wsa_event;
#endif
}


int tcp_wait_read(rdpTcp* tcp, DWORD dwMilliSeconds)
{
	int status;

#ifdef HAVE_POLL_H
	struct pollfd pollset;

	pollset.fd = tcp->sockfd;
	pollset.events = POLLIN;
	pollset.revents = 0;

	do
	{
		status = poll(&pollset, 1, dwMilliSeconds);
	}
	while ((status < 0) && (errno == EINTR));
#else
	struct timeval tv;
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(tcp->sockfd, &rset);

	if (dwMilliSeconds)
	{
		tv.tv_sec = dwMilliSeconds / 1000;
		tv.tv_usec = (dwMilliSeconds % 1000) * 1000;
	}

	do
	{
		status = select(tcp->sockfd + 1, &rset, NULL, NULL, dwMilliSeconds ? &tv : NULL);
	}
	while ((status < 0) && (errno == EINTR));
#endif
	return status;
}

int tcp_wait_write(rdpTcp* tcp, DWORD dwMilliSeconds)
{
	int status;

#ifdef HAVE_POLL_H
	struct pollfd pollset;

	pollset.fd = tcp->sockfd;
	pollset.events = POLLOUT;
	pollset.revents = 0;

	do
	{
		status = poll(&pollset, 1, dwMilliSeconds);
	}
	while ((status < 0) && (errno == EINTR));
#else
	struct timeval tv;
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(tcp->sockfd, &rset);

	if (dwMilliSeconds)
	{
		tv.tv_sec = dwMilliSeconds / 1000;
		tv.tv_usec = (dwMilliSeconds % 1000) * 1000;
	}

	do
	{
		status = select(tcp->sockfd + 1, NULL, &rset, NULL, dwMilliSeconds ? &tv : NULL);
	}
	while ((status < 0) && (errno == EINTR));
#endif
	return status;
}

rdpTcp* tcp_new(rdpSettings* settings)
{
	rdpTcp* tcp;

	tcp = (rdpTcp*) calloc(1, sizeof(rdpTcp));

	if (!tcp)
		return NULL;

	if (!ringbuffer_init(&tcp->xmitBuffer, 0x10000))
		goto out_free;

	tcp->sockfd = -1;
	tcp->settings = settings;

	if (0)
		goto out_ringbuffer; /* avoid unreferenced label warning on Windows */

#ifndef _WIN32
	tcp->event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, tcp->sockfd);

	if (!tcp->event || tcp->event == INVALID_HANDLE_VALUE)
		goto out_ringbuffer;
#endif

	return tcp;
out_ringbuffer:
	ringbuffer_destroy(&tcp->xmitBuffer);
out_free:
	free(tcp);
	return NULL;
}

void tcp_free(rdpTcp* tcp)
{
	if (!tcp)
		return;

	ringbuffer_destroy(&tcp->xmitBuffer);
	CloseHandle(tcp->event);
	free(tcp);
}
