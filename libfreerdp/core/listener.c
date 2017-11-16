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
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/windows.h>
#include <freerdp/log.h>

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#endif

#include <winpr/handle.h>

#include "listener.h"


#define TAG FREERDP_TAG("core.listener")

static BOOL freerdp_listener_open(freerdp_listener* instance, const char* bind_address, UINT16 port)
{
	int status;
	int sockfd;
	char addr[64];
	void* sin_addr;
	int option_value;
	char servname[16];
	struct addrinfo* ai;
	struct addrinfo* res;
	struct addrinfo hints = { 0 };
	rdpListener* listener = (rdpListener*) instance->listener;
#ifdef _WIN32
	u_long arg;
#endif
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (!bind_address)
		hints.ai_flags = AI_PASSIVE;

	sprintf_s(servname, sizeof(servname), "%"PRIu16"", port);
	status = getaddrinfo(bind_address, servname, &hints, &res);

	if (status != 0)
	{
#ifdef _WIN32
		WLog_ERR("getaddrinfo error: %s", gai_strerrorA(status));
#else
		WLog_ERR(TAG, "getaddrinfo");
#endif
		return FALSE;
	}

	for (ai = res; ai && (listener->num_sockfds < 5); ai = ai->ai_next)
	{
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6))
			continue;

		if (listener->num_sockfds == MAX_LISTENER_HANDLES)
		{
			WLog_ERR(TAG, "too many listening sockets");
			continue;
		}

		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sockfd == -1)
		{
			WLog_ERR(TAG, "socket");
			continue;
		}

		if (ai->ai_family == AF_INET)
			sin_addr = &(((struct sockaddr_in*) ai->ai_addr)->sin_addr);
		else
			sin_addr = &(((struct sockaddr_in6*) ai->ai_addr)->sin6_addr);

		inet_ntop(ai->ai_family, sin_addr, addr, sizeof(addr));
		option_value = 1;

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*) &option_value, sizeof(option_value)) == -1)
			WLog_ERR(TAG, "setsockopt");

#ifndef _WIN32
		fcntl(sockfd, F_SETFL, O_NONBLOCK);
#else
		arg = 1;
		ioctlsocket(sockfd, FIONBIO, &arg);
#endif
		status = _bind((SOCKET) sockfd, ai->ai_addr, ai->ai_addrlen);

		if (status != 0)
		{
			closesocket((SOCKET) sockfd);
			continue;
		}

		status = _listen((SOCKET) sockfd, 10);

		if (status != 0)
		{
			WLog_ERR(TAG, "listen");
			closesocket((SOCKET) sockfd);
			continue;
		}

		/* FIXME: these file descriptors do not work on Windows */
		listener->sockfds[listener->num_sockfds] = sockfd;
		listener->events[listener->num_sockfds] = WSACreateEvent();

		if (!listener->events[listener->num_sockfds])
		{
			listener->num_sockfds = 0;
			break;
		}

		WSAEventSelect(sockfd, listener->events[listener->num_sockfds], FD_READ | FD_ACCEPT | FD_CLOSE);
		listener->num_sockfds++;
		WLog_INFO(TAG, "Listening on %s:%s", addr, servname);
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
	HANDLE hevent;

	if (listener->num_sockfds == MAX_LISTENER_HANDLES)
	{
		WLog_ERR(TAG, "too many listening sockets");
		return FALSE;
	}

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sockfd == -1)
	{
		WLog_ERR(TAG, "socket");
		return FALSE;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	unlink(path);
	status = _bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));

	if (status != 0)
	{
		WLog_ERR(TAG, "bind");
		closesocket((SOCKET) sockfd);
		return FALSE;
	}

	status = _listen(sockfd, 10);

	if (status != 0)
	{
		WLog_ERR(TAG, "listen");
		closesocket((SOCKET) sockfd);
		return FALSE;
	}

	hevent = CreateFileDescriptorEvent(NULL, FALSE, FALSE, sockfd, WINPR_FD_READ);

	if (!hevent)
	{
		WLog_ERR(TAG, "failed to create sockfd event");
		closesocket((SOCKET) sockfd);
		return FALSE;
	}

	listener->sockfds[listener->num_sockfds] = sockfd;
	listener->events[listener->num_sockfds] = hevent;
	listener->num_sockfds++;
	WLog_INFO(TAG, "Listening on socket %s.", addr.sun_path);
	return TRUE;
#else
	return TRUE;
#endif
}

static BOOL freerdp_listener_open_from_socket(freerdp_listener* instance, int fd)
{
#ifndef _WIN32
	rdpListener* listener = (rdpListener*) instance->listener;

	if (listener->num_sockfds == MAX_LISTENER_HANDLES)
	{
		WLog_ERR(TAG, "too many listening sockets");
		return FALSE;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		return FALSE;

	listener->sockfds[listener->num_sockfds] = fd;
	listener->events[listener->num_sockfds] =
	    CreateFileDescriptorEvent(NULL, FALSE, FALSE, fd, WINPR_FD_READ);

	if (!listener->events[listener->num_sockfds])
		return FALSE;

	listener->num_sockfds++;
	WLog_INFO(TAG, "Listening on socket %d.", fd);
	return TRUE;
#else
	return FALSE;
#endif
}

static void freerdp_listener_close(freerdp_listener* instance)
{
	int i;
	rdpListener* listener = (rdpListener*) instance->listener;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		closesocket((SOCKET) listener->sockfds[i]);
		CloseHandle(listener->events[i]);
	}

	listener->num_sockfds = 0;
}

static BOOL freerdp_listener_get_fds(freerdp_listener* instance, void** rfds, int* rcount)
{
	int index;
	rdpListener* listener = (rdpListener*) instance->listener;

	if (listener->num_sockfds < 1)
		return FALSE;

	for (index = 0; index < listener->num_sockfds; index++)
	{
		rfds[*rcount] = (void*)(long)(listener->sockfds[index]);
		(*rcount)++;
	}

	return TRUE;
}

DWORD freerdp_listener_get_event_handles(freerdp_listener* instance, HANDLE* events, DWORD nCount)
{
	int index;
	rdpListener* listener = (rdpListener*) instance->listener;

	if (listener->num_sockfds < 1)
		return 0;

	if (listener->num_sockfds > nCount)
		return 0;

	for (index = 0; index < listener->num_sockfds; index++)
	{
		events[index] = listener->events[index];
	}

	return listener->num_sockfds;
}

static BOOL freerdp_listener_check_fds(freerdp_listener* instance)
{
	int i;
	void* sin_addr;
	int peer_sockfd;
	freerdp_peer* client = NULL;
	int peer_addr_size;
	struct sockaddr_storage peer_addr;
	rdpListener* listener = (rdpListener*) instance->listener;
	static const BYTE localhost6_bytes[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	BOOL peer_accepted;

	if (listener->num_sockfds < 1)
		return FALSE;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		WSAResetEvent(listener->events[i]);
		peer_addr_size = sizeof(peer_addr);
		peer_sockfd = _accept(listener->sockfds[i], (struct sockaddr*) &peer_addr, &peer_addr_size);
		peer_accepted = FALSE;

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
			WLog_DBG(TAG, "accept");
			free(client);
			return FALSE;
		}

		client = freerdp_peer_new(peer_sockfd);

		if (!client)
		{
			closesocket((SOCKET) peer_sockfd);
			return FALSE;
		}

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

		IFCALLRET(instance->PeerAccepted, peer_accepted, instance, client);

		if (!peer_accepted)
		{
			WLog_ERR(TAG, "PeerAccepted callback failed");
			closesocket((SOCKET) peer_sockfd);
			freerdp_peer_free(client);
		}
	}

	return TRUE;
}

freerdp_listener* freerdp_listener_new(void)
{
	freerdp_listener* instance;
	rdpListener* listener;
	instance = (freerdp_listener*) calloc(1, sizeof(freerdp_listener));

	if (!instance)
		return NULL;

	instance->Open = freerdp_listener_open;
	instance->OpenLocal = freerdp_listener_open_local;
	instance->OpenFromSocket = freerdp_listener_open_from_socket;
	instance->GetFileDescriptor = freerdp_listener_get_fds;
	instance->GetEventHandles = freerdp_listener_get_event_handles;
	instance->CheckFileDescriptor = freerdp_listener_check_fds;
	instance->Close = freerdp_listener_close;
	listener = (rdpListener*) calloc(1, sizeof(rdpListener));

	if (!listener)
	{
		free(instance);
		return NULL;
	}

	listener->instance = instance;
	instance->listener = (void*) listener;
	return instance;
}

void freerdp_listener_free(freerdp_listener* instance)
{
	if (instance)
	{
		free(instance->listener);
		free(instance);
	}
}

