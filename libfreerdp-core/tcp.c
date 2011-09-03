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
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#else
#define close(_fd) closesocket(_fd)
#endif

#include <freerdp/utils/print.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include "tcp.h"

void tcp_get_ip_address(rdpTcp * tcp)
{
	uint8* ip;
	socklen_t length;
	struct sockaddr_in sockaddr;

	length = sizeof(sockaddr);

	if (getsockname(tcp->sockfd, (struct sockaddr*) &sockaddr, &length) == 0)
	{
		ip = (uint8*) (&sockaddr.sin_addr);
		snprintf(tcp->ip_address, sizeof(tcp->ip_address),
			 "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	}
	else
	{
		strncpy(tcp->ip_address, "127.0.0.1", sizeof(tcp->ip_address));
	}

	tcp->ip_address[sizeof(tcp->ip_address) - 1] = 0;

	tcp->settings->ipv6 = 0;
	tcp->settings->ip_address = tcp->ip_address;
}

void tcp_get_mac_address(rdpTcp * tcp)
{
#ifdef LINUX
	uint8* mac;
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
		printf("failed to obtain MAC address\n");
		return;
	}

	memmove((void*) mac, (void*) &if_req.ifr_ifru.ifru_hwaddr.sa_data[0], 6);
#endif

	/* printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); */
}

boolean tcp_connect(rdpTcp* tcp, const char* hostname, uint16 port)
{
	int status;
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

	tcp->sockfd = -1;
	for (ai = res; ai; ai = ai->ai_next)
	{
		tcp->sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (tcp->sockfd < 0)
			continue;

		if (connect(tcp->sockfd, ai->ai_addr, ai->ai_addrlen) == 0)
		{
			printf("connected to %s:%s\n", hostname, servname);
			break;
		}

		close(tcp->sockfd);
		tcp->sockfd = -1;
	}
	freeaddrinfo(res);

	if (tcp->sockfd == -1)
	{
		printf("unable to connect to %s:%s\n", hostname, servname);
		return False;
	}

	tcp_get_ip_address(tcp);
	tcp_get_mac_address(tcp);

	return True;
}

int tcp_read(rdpTcp* tcp, uint8* data, int length)
{
	int status;

	status = recv(tcp->sockfd, data, length, 0);

	if (status <= 0)
	{
		/* No data available */
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		/* When peer disconnects we get status 0 with no error. */
		if (status < 0)
			perror("recv");
		return -1;
	}

	return status;
}

int tcp_write(rdpTcp* tcp, uint8* data, int length)
{
	int status;

	status = send(tcp->sockfd, data, length, MSG_NOSIGNAL);

	if (status < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			status = 0;
		else
			perror("send");
	}

	return status;
}

boolean tcp_disconnect(rdpTcp * tcp)
{
	if (tcp->sockfd != -1)
	{
		close(tcp->sockfd);
		tcp->sockfd = -1;
	}

	return True;
}

boolean tcp_set_blocking_mode(rdpTcp* tcp, boolean blocking)
{
#ifndef _WIN32
	int flags;
	flags = fcntl(tcp->sockfd, F_GETFL);

	if (flags == -1)
	{
		printf("transport_configure_sockfd: fcntl failed.\n");
		return False;
	}

	if (blocking == True)
		fcntl(tcp->sockfd, F_SETFL, flags & ~(O_NONBLOCK));
	else
		fcntl(tcp->sockfd, F_SETFL, flags | O_NONBLOCK);
#else
	u_long arg = blocking;
	ioctlsocket(tcp->sockfd, FIONBIO, &arg);
	tcp->wsa_event = WSACreateEvent();
	WSAEventSelect(tcp->sockfd, tcp->wsa_event, FD_READ);
#endif

	return True;
}

rdpTcp* tcp_new(rdpSettings* settings)
{
	rdpTcp* tcp = (rdpTcp*) xzalloc(sizeof(rdpTcp));

	if (tcp != NULL)
	{
		tcp->sockfd = -1;
		tcp->settings = settings;
	}

	return tcp;
}

void tcp_free(rdpTcp* tcp)
{
	if (tcp != NULL)
	{
		xfree(tcp);
	}
}
