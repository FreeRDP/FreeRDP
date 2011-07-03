/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Network Transport Layer
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
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include "transport.h"

rdpTransport *
transport_new(void)
{
	rdpTransport * transport;

	transport = (rdpTransport *) xmalloc(sizeof(rdpTransport));
	memset(transport, 0, sizeof(rdpTransport));

	transport->sockfd = -1;

	return transport;
}

void
transport_free(rdpTransport * transport)
{
	xfree(transport);
}

static int
transport_connect_sockfd(rdpTransport * transport, const char * server, int port)
{
	struct addrinfo hints = { 0 };
	struct addrinfo * res, * ai;
	int r;
	char servname[10];
	int sockfd = -1;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(servname, sizeof(servname), "%d", port);
	r = getaddrinfo(server, servname, &hints, &res);
	if (r != 0)
	{
		printf("transport_connect: getaddrinfo (%s)\n", gai_strerror(r));
		return -1;
	}

	for (ai = res; ai; ai = ai->ai_next)
	{
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd < 0)
			continue;

		r = connect(sockfd, ai->ai_addr, ai->ai_addrlen);
		if (r == 0)
		{
			printf("connected to %s:%s\n", server, servname);
			break;
		}

		sockfd = -1;
	}
	freeaddrinfo(res);

	if (sockfd == -1)
	{
		printf("unable to connect to %s:%s\n", server, servname);
		return -1;
	}

	transport->sockfd = sockfd;

	return 0;
}

static int
transport_configure_sockfd(rdpTransport * transport)
{
	int flags;

	flags = fcntl(transport->sockfd, F_GETFL);
	if (flags == -1)
	{
		printf("transport_configure_sockfd: fcntl failed.\n");
		return -1;
	}
	fcntl(transport->sockfd, F_SETFL, flags | O_NONBLOCK);

	return 0;
}

int
transport_connect(rdpTransport * transport, const char * server, int port)
{
	int r;

	r = transport_connect_sockfd(transport, server, port);
	if (r != 0)
		return r;

	r = transport_configure_sockfd(transport);
	if (r != 0)
		return r;

	return 0;
}

int
transport_disconnect(rdpTransport * transport)
{
	if (transport->sockfd != -1)
	{
		close(transport->sockfd);
		transport->sockfd = -1;
	}
	return 0;
}

int
transport_start_tls(rdpTransport * transport)
{
	return 0;
}

int
transport_send(rdpTransport * transport, STREAM * stream)
{
	return 0;
}

int
transport_check_fds(rdpTransport * transport)
{
	return 0;
}
