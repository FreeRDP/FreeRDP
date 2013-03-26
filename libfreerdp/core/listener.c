/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Listener
 *
 * Copyright 2011 Vic Lee
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
#include <fcntl.h>

#include <winpr/crt.h>

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#else
#define close(_fd) closesocket(_fd)
#endif

#include "listener.h"

#ifdef _WIN32
#if _WIN32_WINNT < 0x0600
static const char *inet_ntop(int af, const void* src, char* dst, socklen_t cnt)
{
	if (af == AF_INET)
	{
		struct sockaddr_in in;

		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	else if (af == AF_INET6)
	{
		struct sockaddr_in6 in;

		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	
	return NULL;
}
#endif
#endif

static BOOL freerdp_listener_open(freerdp_listener* instance, const char* bind_address, UINT16 port)
{
	rdpListener* listener = (rdpListener*) instance->listener;
	int status;
	int sockfd;
	char servname[10];
	struct addrinfo hints = { 0 };
	struct addrinfo* res;
	struct addrinfo* ai;
	int option_value;
	void* sin_addr;
	char buf[50];
#ifdef _WIN32
	u_long arg;
#endif

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (bind_address == NULL)
		hints.ai_flags = AI_PASSIVE;

	sprintf_s(servname, sizeof(servname), "%d", port);
	status = getaddrinfo(bind_address, servname, &hints, &res);

	if (status != 0)
	{
#ifdef _WIN32
		_tprintf(_T("getaddrinfo error: %s\n"), gai_strerror(status));
#else
		perror("getaddrinfo");
#endif
		return FALSE;
	}

	for (ai = res; ai && listener->num_sockfds < 5; ai = ai->ai_next)
	{
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;

		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sockfd == -1)
		{
			perror("socket");
			continue;
		}

		option_value = 1;

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*) &option_value, sizeof(option_value)) == -1)
			perror("setsockopt");

#ifndef _WIN32
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
#else
		arg = 1;
		ioctlsocket(sockfd, FIONBIO, &arg);
#endif

		status = bind(sockfd, ai->ai_addr, ai->ai_addrlen);

		if (status != 0)
		{
#ifdef _WIN32
			_tprintf(L"bind() failed with error: %u\n", WSAGetLastError());
			WSACleanup();
#else
			perror("bind");
			close(sockfd);
#endif
			continue;
		}

		status = listen(sockfd, 10);

		if (status != 0)
		{
			perror("listen");
			close(sockfd);
			continue;
		}

		listener->sockfds[listener->num_sockfds++] = sockfd;

		if (ai->ai_family == AF_INET)
			sin_addr = &(((struct sockaddr_in*) ai->ai_addr)->sin_addr);
		else
			sin_addr = &(((struct sockaddr_in6*) ai->ai_addr)->sin6_addr);

		fprintf(stderr, "Listening on %s port %s.\n", inet_ntop(ai->ai_family, sin_addr, buf, sizeof(buf)), servname);
	}

	freeaddrinfo(res);

	return (listener->num_sockfds > 0 ? TRUE : FALSE);
}

static BOOL freerdp_listener_open_local(freerdp_listener* instance, const char* path)
{
#ifndef _WIN32
	int status;
	int sockfd;
	struct sockaddr_un addr;
	rdpListener* listener = (rdpListener*) instance->listener;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sockfd == -1)
	{
		perror("socket");
		return FALSE;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	unlink(path);

	status = bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));

	if (status != 0)
	{
		perror("bind");
		close(sockfd);
		return FALSE;
	}

	status = listen(sockfd, 10);

	if (status != 0)
	{
		perror("listen");
		close(sockfd);
		return FALSE;
	}

	listener->sockfds[listener->num_sockfds++] = sockfd;

	fprintf(stderr, "Listening on socket %s.\n", addr.sun_path);

	return TRUE;
#else
	return TRUE;
#endif
}

static void freerdp_listener_close(freerdp_listener* instance)
{
	int i;

	rdpListener* listener = (rdpListener*) instance->listener;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		close(listener->sockfds[i]);
	}

	listener->num_sockfds = 0;
}

static BOOL freerdp_listener_get_fds(freerdp_listener* instance, void** rfds, int* rcount)
{
	int i;
	rdpListener* listener = (rdpListener*) instance->listener;

	if (listener->num_sockfds < 1)
		return FALSE;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		rfds[*rcount] = (void*)(long)(listener->sockfds[i]);
		(*rcount)++;
	}

	return TRUE;
}

static BOOL freerdp_listener_check_fds(freerdp_listener* instance)
{
	int i;
	void* sin_addr;
	int peer_sockfd;
	freerdp_peer* client;
	socklen_t peer_addr_size;
	struct sockaddr_storage peer_addr;
	rdpListener* listener = (rdpListener*) instance->listener;
	static const BYTE localhost6_bytes[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

	if (listener->num_sockfds < 1)
		return FALSE;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		peer_addr_size = sizeof(peer_addr);
		peer_sockfd = accept(listener->sockfds[i], (struct sockaddr*) &peer_addr, &peer_addr_size);

		if (peer_sockfd == -1)
		{
#ifdef _WIN32
			int wsa_error = WSAGetLastError();

			/* No data available */
			if (wsa_error == WSAEWOULDBLOCK)
				continue;
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
#endif
			perror("accept");
			return FALSE;
		}

		client = freerdp_peer_new(peer_sockfd);

		sin_addr = NULL;
		if (peer_addr.ss_family == AF_INET)
		{
			sin_addr = &(((struct sockaddr_in*) &peer_addr)->sin_addr);
			if ((*(UINT32*) sin_addr) == 0x0100007f)
				client->local = TRUE;
		}
		else if (peer_addr.ss_family == AF_INET6)
		{
			sin_addr = &(((struct sockaddr_in6*) &peer_addr)->sin6_addr);
			if (memcmp(sin_addr, localhost6_bytes, 16) == 0)
				client->local = TRUE;
		}
#ifndef _WIN32
		else if (peer_addr.ss_family == AF_UNIX)
			client->local = TRUE;
#endif

		if (sin_addr)
			inet_ntop(peer_addr.ss_family, sin_addr, client->hostname, sizeof(client->hostname));

		IFCALL(instance->PeerAccepted, instance, client);
	}

	return TRUE;
}

freerdp_listener* freerdp_listener_new(void)
{
	freerdp_listener* instance;
	rdpListener* listener;

	instance = (freerdp_listener*) malloc(sizeof(freerdp_listener));
	ZeroMemory(instance, sizeof(freerdp_listener));

	instance->Open = freerdp_listener_open;
	instance->OpenLocal = freerdp_listener_open_local;
	instance->GetFileDescriptor = freerdp_listener_get_fds;
	instance->CheckFileDescriptor = freerdp_listener_check_fds;
	instance->Close = freerdp_listener_close;

	listener = (rdpListener*) malloc(sizeof(rdpListener));
	ZeroMemory(listener, sizeof(rdpListener));

	listener->instance = instance;

	instance->listener = (void*) listener;

	return instance;
}

void freerdp_listener_free(freerdp_listener* instance)
{
	rdpListener* listener;

	listener = (rdpListener*) instance->listener;
	free(listener);

	free(instance);
}

