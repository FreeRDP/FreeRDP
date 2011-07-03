/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include "tcp.h"

FRDP_BOOL
tcp_connect(rdpTcp * tcp, const char * hostname, int port)
{
	int status;
	int sockfd = -1;
	char servname[10];
	struct addrinfo hints = { 0 };
	struct addrinfo * res, * ai;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(servname, sizeof(servname), "%d", port);
	status = getaddrinfo(hostname, servname, &hints, &res);

	if (status != 0)
	{
		printf("transport_connect: getaddrinfo (%s)\n", gai_strerror(status));
		return False;
	}

	for (ai = res; ai; ai = ai->ai_next)
	{
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (sockfd < 0)
			continue;

		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == 0)
		{
			printf("connected to %s:%s\n", hostname, servname);
			break;
		}

		sockfd = -1;
	}
	freeaddrinfo(res);

	if (sockfd == -1)
	{
		printf("unable to connect to %s:%s\n", hostname, servname);
		return False;
	}

	tcp->sockfd = sockfd;

	return True;
}

FRDP_BOOL
tcp_disconnect(rdpTcp * tcp)
{
	if (tcp->sockfd != -1)
	{
		close(tcp->sockfd);
		tcp->sockfd = -1;
	}

	return True;
}

FRDP_BOOL
tcp_set_blocking_mode(rdpTcp * tcp, FRDP_BOOL blocking)
{
	int flags;
	flags = fcntl(tcp->sockfd, F_GETFL);

	if (flags == -1)
	{
		printf("transport_configure_sockfd: fcntl failed.\n");
		return False;
	}

	if (blocking == True)
	{
		/* blocking */
		fcntl(tcp->sockfd, F_SETFL, flags & ~(O_NONBLOCK));
	}
	else
	{
		/* non-blocking */
		fcntl(tcp->sockfd, F_SETFL, flags | O_NONBLOCK);
	}

	return True;
}

rdpTcp*
tcp_new()
{
	rdpTcp *tcp = (rdpTcp*) xzalloc(sizeof(rdpTcp));

	if (tcp != NULL)
	{
		tcp->sockfd = -1;
		tcp->connect = tcp_connect;
		tcp->disconnect = tcp_disconnect;
		tcp->set_blocking_mode = tcp_set_blocking_mode;
	}

	return tcp;
}

void
tcp_free(rdpTcp* tcp)
{
	if (tcp != NULL)
	{
		xfree(tcp);
	}
}
