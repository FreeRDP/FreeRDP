/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <freerdp/utils/print.h>

#ifndef _WIN32
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#else
#define close(_fd) closesocket(_fd)
#endif

#include "listener.h"

static boolean freerdp_listener_open(freerdp_listener* instance, const char* bind_address, uint16 port)
{
	rdpListener* listener = (rdpListener*)instance->listener;
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

	snprintf(servname, sizeof(servname), "%d", port);
	status = getaddrinfo(bind_address, servname, &hints, &res);
	if (status != 0)
	{
		perror("getaddrinfo");
		return false;
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
			perror("bind");
			close(sockfd);
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
			sin_addr = &(((struct sockaddr_in*)ai->ai_addr)->sin_addr);
		else
			sin_addr = &(((struct sockaddr_in6*)ai->ai_addr)->sin6_addr);

		printf("Listening on %s port %s.\n", inet_ntop(ai->ai_family, sin_addr, buf, sizeof(buf)), servname);
	}

	freeaddrinfo(res);

	return (listener->num_sockfds > 0 ? true : false);
}

static void freerdp_listener_close(freerdp_listener* instance)
{
	int i;

	rdpListener* listener = (rdpListener*)instance->listener;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		close(listener->sockfds[i]);
	}
	listener->num_sockfds = 0;
}

static boolean freerdp_listener_get_fds(freerdp_listener* instance, void** rfds, int* rcount)
{
	rdpListener* listener = (rdpListener*)instance->listener;
	int i;

	if (listener->num_sockfds < 1)
		return false;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		rfds[*rcount] = (void*)(long)(listener->sockfds[i]);
		(*rcount)++;
	}

	return true;
}

static boolean freerdp_listener_check_fds(freerdp_listener* instance)
{
	rdpListener* listener = (rdpListener*)instance->listener;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_size;
	int peer_sockfd;
	int i;
	freerdp_peer* client;
	void* sin_addr;

	if (listener->num_sockfds < 1)
		return false;

	for (i = 0; i < listener->num_sockfds; i++)
	{
		peer_addr_size = sizeof(peer_addr);
		peer_sockfd = accept(listener->sockfds[i], (struct sockaddr *)&peer_addr, &peer_addr_size);

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
			return false;
		}

		client = freerdp_peer_new(peer_sockfd);

		if (peer_addr.ss_family == AF_INET)
			sin_addr = &(((struct sockaddr_in*)&peer_addr)->sin_addr);
		else
			sin_addr = &(((struct sockaddr_in6*)&peer_addr)->sin6_addr);
		inet_ntop(peer_addr.ss_family, sin_addr, client->hostname, sizeof(client->hostname));

		IFCALL(instance->PeerAccepted, instance, client);
	}

	return true;
}

freerdp_listener* freerdp_listener_new(void)
{
	freerdp_listener* instance;
	rdpListener* listener;

	instance = xnew(freerdp_listener);
	instance->Open = freerdp_listener_open;
	instance->GetFileDescriptor = freerdp_listener_get_fds;
	instance->CheckFileDescriptor = freerdp_listener_check_fds;
	instance->Close = freerdp_listener_close;

	listener = xnew(rdpListener);
	listener->instance = instance;

	instance->listener = (void*)listener;

	return instance;
}

void freerdp_listener_free(freerdp_listener* instance)
{
	rdpListener* listener;

	listener = (rdpListener*)instance->listener;
	xfree(listener);

	xfree(instance);
}

