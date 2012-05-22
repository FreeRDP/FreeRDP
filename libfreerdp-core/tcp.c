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
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>

#ifdef __APPLE__
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif
#endif

#else
#define SHUT_RDWR SD_BOTH
#define close(_fd) closesocket(_fd)
#endif

#include <freerdp/utils/tcp.h>
#include <freerdp/utils/uds.h>
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
	tcp->settings->ip_address = xstrdup(tcp->ip_address);
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
	uint32 option_value;
	socklen_t option_len;

	if (hostname[0] == '/')
	{
		tcp->sockfd = freerdp_uds_connect(hostname);

		if (tcp->sockfd < 0)
			return false;
	}
	else
	{
		tcp->sockfd = freerdp_tcp_connect(hostname, port);

		if (tcp->sockfd < 0)
			return false;

		tcp_get_ip_address(tcp);
		tcp_get_mac_address(tcp);

		option_value = 1;
		option_len = sizeof(option_value);
		setsockopt(tcp->sockfd, IPPROTO_TCP, TCP_NODELAY, (void*) &option_value, option_len);

		/* receive buffer must be a least 32 K */
		if (getsockopt(tcp->sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &option_value, &option_len) == 0)
		{
			if (option_value < (1024 * 32))
			{
				option_value = 1024 * 32;
				option_len = sizeof(option_value);
				setsockopt(tcp->sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &option_value, option_len);
			}
		}

		tcp_set_keep_alive_mode(tcp);
	}

	return true;
}

int tcp_read(rdpTcp* tcp, uint8* data, int length)
{
	return freerdp_tcp_read(tcp->sockfd, data, length);
}

int tcp_write(rdpTcp* tcp, uint8* data, int length)
{
	return freerdp_tcp_write(tcp->sockfd, data, length);
}

boolean tcp_disconnect(rdpTcp* tcp)
{
	freerdp_tcp_disconnect(tcp->sockfd);
	tcp->sockfd = -1;

	return true;
}

boolean tcp_set_blocking_mode(rdpTcp* tcp, boolean blocking)
{
#ifndef _WIN32
	int flags;
	flags = fcntl(tcp->sockfd, F_GETFL);

	if (flags == -1)
	{
		printf("tcp_set_blocking_mode: fcntl failed.\n");
		return false;
	}

	if (blocking == true)
		fcntl(tcp->sockfd, F_SETFL, flags & ~(O_NONBLOCK));
	else
		fcntl(tcp->sockfd, F_SETFL, flags | O_NONBLOCK);
#else
	u_long arg = blocking;
	ioctlsocket(tcp->sockfd, FIONBIO, &arg);
	tcp->wsa_event = WSACreateEvent();
	WSAEventSelect(tcp->sockfd, tcp->wsa_event, FD_READ);
#endif

	return true;
}

boolean tcp_set_keep_alive_mode(rdpTcp* tcp)
{
#ifndef _WIN32
	uint32 option_value;
	socklen_t option_len;

	option_value = 1;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*) &option_value, option_len) < 0)
	{
		perror("setsockopt() SOL_SOCKET, SO_KEEPALIVE:");
		return false;
	}

#ifdef TCP_KEEPIDLE
	option_value = 5;
	option_len = sizeof(option_value);

	if (setsockopt(tcp->sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void*) &option_value, option_len) < 0)
	{
		perror("setsockopt() IPPROTO_TCP, SO_KEEPIDLE:");
		return false;
	}
#endif
#endif

	return true;
}

rdpTcp* tcp_new(rdpSettings* settings)
{
	rdpTcp* tcp;

	tcp = (rdpTcp*) xzalloc(sizeof(rdpTcp));

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
