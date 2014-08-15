/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * TCP Utils
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/winsock.h>

#include <freerdp/utils/debug.h>
#include <freerdp/utils/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <errno.h>
#include <fcntl.h>

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
#include <sys/select.h>
#endif

#ifdef __APPLE__
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif
#endif

#else /* ifdef _WIN32 */

#include <winpr/windows.h>

#include <winpr/crt.h>

#define SHUT_RDWR SD_BOTH
#define close(_fd) closesocket(_fd)
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0

#endif

int freerdp_tcp_connect(const char* hostname, int port)
{
	int status;
	int sockfd;
	char servname[32];
	struct addrinfo hints;
	struct addrinfo* ai = NULL;
	struct addrinfo* res = NULL;

	ZeroMemory(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	sprintf_s(servname, 32, "%d", port);
	status = getaddrinfo(hostname, servname, &hints, &res);

	if (status != 0)
	{
		//DEBUG_WARN( "tcp_connect: getaddrinfo (%s)\n", gai_strerror(status));
		return -1;
	}

	sockfd = -1;

	for (ai = res; ai; ai = ai->ai_next)
	{
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sockfd < 0)
			continue;

		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == 0)
		{
			DEBUG_WARN( "connected to %s:%s\n", hostname, servname);
			break;
		}

		close(sockfd);
		sockfd = -1;
	}

	freeaddrinfo(res);

	if (sockfd == -1)
	{
		DEBUG_WARN( "unable to connect to %s:%s\n", hostname, servname);
		return -1;
	}

	return sockfd;
}

int freerdp_tcp_read(int sockfd, BYTE* data, int length)
{
	int status;

	status = recv(sockfd, data, length, 0);

	if (status == 0)
	{
		return -1; /* peer disconnected */
	}
	else if (status < 0)
	{
#ifdef _WIN32
		int wsa_error = WSAGetLastError();

		/* No data available */
		if (wsa_error == WSAEWOULDBLOCK)
			return 0;

		DEBUG_WARN( "recv() error: %d\n", wsa_error);
#else
		/* No data available */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		DEBUG_WARN("recv");
#endif
		return -1;
	}

	return status;
}

int freerdp_tcp_write(int sockfd, BYTE* data, int length)
{
	int status;

	status = send(sockfd, data, length, MSG_NOSIGNAL);

	if (status < 0)
	{
#ifdef _WIN32
		int wsa_error = WSAGetLastError();

		/* No data available */
		if (wsa_error == WSAEWOULDBLOCK)
			status = 0;
		else
			DEBUG_WARN("send");
#else
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			status = 0;
		else
			DEBUG_WARN("send");
#endif
	}

	return status;
}

int freerdp_tcp_wait_read(int sockfd)
{
	int status;

#ifdef HAVE_POLL_H
	struct pollfd pollfds;
#else
	fd_set fds;
	struct timeval timeout;
#endif

	if (sockfd < 1)
	{
		DEBUG_WARN( "Invalid socket to watch: %d\n", sockfd);
		return 0 ;
	}

#ifdef HAVE_POLL_H
	pollfds.fd = sockfd;
	pollfds.events = POLLIN;
	pollfds.revents = 0;
	do
	{
		status = poll(&pollfds, 1, 5 * 1000);
	}
	while ((status < 0) && (errno == EINTR));
#else
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	status = _select(sockfd+1, &fds, NULL, NULL, &timeout);
#endif

	return status > 0 ? 1 : 0;
}

int freerdp_tcp_wait_write(int sockfd)
{
	int status;

#ifdef HAVE_POLL_H
	struct pollfd pollfds;
#else
	fd_set fds;
	struct timeval timeout;
#endif

	if (sockfd < 1)
	{
		DEBUG_WARN( "Invalid socket to watch: %d\n", sockfd);
		return 0 ;
	}

#ifdef HAVE_POLL_H
	pollfds.fd = sockfd;
	pollfds.events = POLLOUT;
	pollfds.revents = 0;
	do
	{
		status = poll(&pollfds, 1, 5 * 1000);
	}
	while ((status < 0) && (errno == EINTR));
#else
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	status = _select(sockfd+1, NULL, &fds, NULL, &timeout);
#endif

	return status > 0 ? 1 : 0;
}

int freerdp_tcp_disconnect(int sockfd)
{
	if (sockfd != -1)
	{
		shutdown(sockfd, SHUT_RDWR);
		close(sockfd);
	}

	return 0;
}

int freerdp_tcp_set_no_delay(int sockfd, BOOL no_delay)
{
	UINT32 option_value;
	socklen_t option_len;

	option_value = no_delay;
	option_len = sizeof(option_value);

	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*) &option_value, option_len);

	return 0;
}

int freerdp_wsa_startup()
{
#ifdef _WIN32
	WSADATA wsaData;
	return WSAStartup(0x101, &wsaData);
#else
	return 0;
#endif
}

int freerdp_wsa_cleanup()
{
#ifdef _WIN32
	return WSACleanup();
#else
	return 0;
#endif
}
