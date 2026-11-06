/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Tests of the UDP listener and flows, over loopback
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
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <freerdp/server/udp.h>

#include "../rdpudp.h"
#include "../udplistener.h"

#define SYN_LENGTH 1232

#define TEST(cond)                                                         \
	do                                                                     \
	{                                                                      \
		if (!(cond))                                                       \
		{                                                                  \
			(void)printf("%s:%d: %s failed\n", __func__, __LINE__, #cond); \
			return FALSE;                                                  \
		}                                                                  \
	} while (0)

/* [MS-RDPEUDP] 2.2.2.1, 2.2.2.5 to 2.2.2.7: a version 3 SYN with hash, with zero padding to the
 * MTU */
static void make_syn(BYTE syn[SYN_LENGTH], const BYTE hash[32])
{
	memset(syn, 0, SYN_LENGTH);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, SYN_LENGTH);
	Stream_Write_UINT32_BE(s, 0xFFFFFFFF);      /* snSourceAck */
	Stream_Write_UINT16_BE(s, 64);              /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, 0x0001 | 0x1000); /* RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX */
	Stream_Write_UINT32_BE(s, 1000);            /* snInitialSequenceNumber */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uUpStreamMtu */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uDownStreamMtu */
	Stream_Write_UINT16_BE(s, 0x0001);          /* RDPUDP_VERSION_INFO_VALID */
	Stream_Write_UINT16_BE(s, RDPUDP_PROTOCOL_VERSION_3);
	Stream_Write(s, hash, 32);
}

static SOCKET open_client(UINT16 port)
{
	SOCKET sockfd = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET)
		return INVALID_SOCKET;
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	if (_connect(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
	{
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}

/* A loopback socket on a free port. The listener takes ownership of this socket. */
static SOCKET open_server(UINT16* port)
{
	SOCKET sockfd = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET)
		return INVALID_SOCKET;
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t length = sizeof(addr);
	if ((_bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0) ||
	    (_getsockname(sockfd, (struct sockaddr*)&addr, &length) != 0))
	{
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
	*port = ntohs(addr.sin_port);
	return sockfd;
}

/* Runs the listener until flow has a datagram. */
static BOOL pump(freerdp_udp_listener* listener, rdpUdpFlow* flow, BYTE* data, size_t* length)
{
	for (size_t x = 0; x < 200; x++)
	{
		if (listener && !freerdp_udp_listener_check(listener))
			return FALSE;
		if (udpflow_read(flow, data, length))
			return TRUE;
		Sleep(5);
	}
	return FALSE;
}

static BOOL sorts_by_cookie_hash_then_address(void)
{
	BYTE hashA[32] = { 1 };
	BYTE hashB[32] = { 2 };
	BYTE hashC[32] = { 3 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));

	rdpUdpFlow* a = freerdp_udp_flow_new(hashA);
	rdpUdpFlow* b = freerdp_udp_flow_new(hashB);
	TEST(a && b);
	TEST(freerdp_udp_listener_expect(listener, a));
	TEST(freerdp_udp_listener_expect(listener, b));
	TEST(!freerdp_udp_listener_expect(listener, a));

	const SOCKET clientA = open_client(port);
	const SOCKET clientB = open_client(port);
	const SOCKET stranger = open_client(port);
	TEST((clientA != INVALID_SOCKET) && (clientB != INVALID_SOCKET) &&
	     (stranger != INVALID_SOCKET));

	/* no SYN with a known hash, no flow: the listener keeps nothing for a stranger */
	TEST(_send(stranger, "junk", 4, 0) == 4);
	make_syn(syn, hashC);
	TEST(_send(stranger, (const char*)syn, sizeof(syn), 0) == sizeof(syn));

	make_syn(syn, hashB);
	TEST(_send(clientB, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(pump(listener, b, data, &length));
	TEST((length == sizeof(syn)) && (memcmp(data, syn, length) == 0));
	TEST(!udpflow_read(a, data, &length));

	make_syn(syn, hashA);
	TEST(_send(clientA, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(pump(listener, a, data, &length));

	/* after the SYN the address shows who the client is */
	TEST(_send(clientB, "abc", 3, 0) == 3);
	TEST(_send(clientA, "de", 2, 0) == 2);
	TEST(pump(listener, b, data, &length));
	TEST((length == 3) && (memcmp(data, "abc", 3) == 0));
	TEST(pump(listener, a, data, &length));
	TEST((length == 2) && (memcmp(data, "de", 2) == 0));

	/* and the flow answers on the listener's socket */
	TEST(freerdp_udp_flow_send(a, (const BYTE*)"reply", 5));
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(clientA, reply, sizeof(reply), 0) == 5);
	TEST(memcmp(reply, "reply", 5) == 0);

	freerdp_udp_flow_free(a);
	/* the test leaves b for the listener to release */
	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(b);
	closesocket(clientA);
	closesocket(clientB);
	closesocket(stranger);
	return TRUE;
}

static BOOL a_bound_address_does_not_take_another_hash(void)
{
	BYTE hashA[32] = { 1 };
	BYTE hashB[32] = { 2 };
	BYTE synA[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE synB[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));

	rdpUdpFlow* a = freerdp_udp_flow_new(hashA);
	rdpUdpFlow* b = freerdp_udp_flow_new(hashB);
	TEST(a && b);
	TEST(freerdp_udp_listener_expect(listener, a));
	TEST(freerdp_udp_listener_expect(listener, b));

	const SOCKET bound = open_client(port);
	const SOCKET other = open_client(port);
	TEST((bound != INVALID_SOCKET) && (other != INVALID_SOCKET));

	make_syn(synA, hashA);
	make_syn(synB, hashB);
	TEST(_send(bound, (const char*)synA, sizeof(synA), 0) == sizeof(synA));
	TEST(pump(listener, a, data, &length));

	/* the SYN with the other hash goes to no flow, a repeated SYN still arrives */
	TEST(_send(bound, (const char*)synB, sizeof(synB), 0) == sizeof(synB));
	TEST(_send(bound, (const char*)synA, sizeof(synA), 0) == sizeof(synA));
	TEST(pump(listener, a, data, &length));
	TEST((length == sizeof(synA)) && (memcmp(data, synA, length) == 0));
	TEST(!udpflow_read(b, data, &length));

	/* the flow of the other hash is still free for its client */
	TEST(_send(other, (const char*)synB, sizeof(synB), 0) == sizeof(synB));
	TEST(pump(listener, b, data, &length));
	TEST((length == sizeof(synB)) && (memcmp(data, synB, length) == 0));

	freerdp_udp_flow_free(a);
	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(b);
	closesocket(bound);
	closesocket(other);
	return TRUE;
}

static BOOL a_full_flow_loses_datagrams_only(void)
{
	const BYTE hash[32] = { 4 };
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	HANDLE event = freerdp_udp_flow_get_event(flow);
	TEST(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);

	size_t taken = 0;
	for (UINT32 x = 0; x < 1000; x++)
	{
		BYTE number[4] = WINPR_C_ARRAY_INIT;
		memcpy(number, &x, sizeof(x));
		if (freerdp_udp_flow_input(flow, number, sizeof(number)))
			taken++;
	}
	TEST((taken > 0) && (taken < 1000));
	TEST(!freerdp_udp_flow_input(flow, data, sizeof(data))); /* full */
	TEST(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);

	/* what it kept comes out in order */
	for (UINT32 x = 0; x < taken; x++)
	{
		UINT32 number = UINT32_MAX;
		TEST(udpflow_read(flow, data, &length));
		TEST(length == sizeof(number));
		memcpy(&number, data, sizeof(number));
		TEST(number == x);
	}
	TEST(!udpflow_read(flow, data, &length));
	TEST(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);

	/* too large for any client */
	BYTE large[UDPFLOW_MAX_DATAGRAM + 1] = WINPR_C_ARRAY_INIT;
	TEST(!freerdp_udp_flow_input(flow, large, sizeof(large)));
	freerdp_udp_flow_free(flow);
	return TRUE;
}

typedef struct
{
	const BYTE* data;
	size_t length;
	size_t calls;
} SendTrace;

static BOOL trace_send(void* custom, const BYTE* data, size_t length)
{
	SendTrace* trace = custom;
	trace->data = data;
	trace->length = length;
	trace->calls++;
	return TRUE;
}

static BOOL a_flow_can_send_any_way(void)
{
	const BYTE hash[32] = { 5 };
	SendTrace trace = WINPR_C_ARRAY_INIT;

	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(memcmp(freerdp_udp_flow_get_cookie_hash(flow), hash, sizeof(hash)) == 0);
	/* a flow without a client has nowhere to send to */
	TEST(!freerdp_udp_flow_send(flow, hash, sizeof(hash)));

	freerdp_udp_flow_set_send(flow, trace_send, &trace);
	TEST(freerdp_udp_flow_send(flow, hash, sizeof(hash)));
	TEST((trace.calls == 1) && (trace.data == hash) && (trace.length == sizeof(hash)));
	freerdp_udp_flow_free(flow);
	return TRUE;
}

static BOOL a_listener_runs_on_its_own_thread(void)
{
	const BYTE hash[32] = { 6 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	HANDLE events[4] = WINPR_C_ARRAY_INIT;
	TEST(freerdp_udp_listener_get_event_handles(listener, events, ARRAYSIZE(events)) == 1);
	TEST(freerdp_udp_listener_start(listener));

	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_listener_expect(listener, flow));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(flow), 2000) == WAIT_OBJECT_0);
	TEST(udpflow_read(flow, data, &length));
	TEST(length == sizeof(syn));

	freerdp_udp_flow_free(flow);
	freerdp_udp_listener_free(listener);
	closesocket(client);
	return TRUE;
}

/* The shared listener opens with its first flow and closes with its last. */
static BOOL the_shared_listener_follows_its_flows(void)
{
	const BYTE hash[32] = { 7 };
	const BYTE otherHash[32] = { 9 };
	UINT16 port = 0;
	const SOCKET probe = open_server(&port);
	TEST(probe != INVALID_SOCKET);
	closesocket(probe);

	rdpUdpFlow* first = freerdp_udp_flow_new(hash);
	rdpUdpFlow* second = freerdp_udp_flow_new(otherHash);
	rdpUdpFlow* same = freerdp_udp_flow_new(hash);
	TEST(first && second && same);
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	const struct sockaddr* local = (const struct sockaddr*)&addr;
	TEST(udplistener_expect_on_address(local, sizeof(addr), first));
	TEST(udplistener_expect_on_address(local, sizeof(addr), second));
	/* a cookie hash has one owner */
	TEST(!udplistener_expect_on_address(local, sizeof(addr), same));
	freerdp_udp_flow_free(same);

	/* the socket is on the address that the client reached, in the form that a dual stack socket
	 * also reports */
	struct sockaddr_in6 mapped = { 0 };
	mapped.sin6_family = AF_INET6;
	mapped.sin6_port = htons(port);
	mapped.sin6_addr.s6_addr[10] = 0xFF;
	mapped.sin6_addr.s6_addr[11] = 0xFF;
	memcpy(&mapped.sin6_addr.s6_addr[12], &addr.sin_addr, sizeof(addr.sin_addr));
	const BYTE mappedHash[32] = { 11 };
	rdpUdpFlow* viaMapped = freerdp_udp_flow_new(mappedHash);
	TEST(viaMapped);
	TEST(udplistener_expect_on_address((const struct sockaddr*)&mapped, sizeof(mapped), viaMapped));
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	make_syn(syn, mappedHash);
	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(viaMapped), 2000) == WAIT_OBJECT_0);
	TEST(udpflow_read(viaMapped, data, &length));
	TEST(freerdp_udp_flow_send(viaMapped, (const BYTE*)"hi", 2));
	char reply[4] = WINPR_C_ARRAY_INIT;
	TEST(_recv(client, reply, sizeof(reply), 0) == 2);
	closesocket(client);
	freerdp_udp_flow_free(viaMapped);

	/* nobody else gets a part of the port's datagrams */
	SOCKET other = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	TEST(other != INVALID_SOCKET);
	TEST(_bind(other, (const struct sockaddr*)&addr, sizeof(addr)) != 0);

	freerdp_udp_flow_free(first);
	TEST(_bind(other, (const struct sockaddr*)&addr, sizeof(addr)) != 0);
	freerdp_udp_flow_free(second);
	TEST(_bind(other, (const struct sockaddr*)&addr, sizeof(addr)) == 0);
	closesocket(other);
	return TRUE;
}

/* A pair of sockets works the same on every platform. Each end tells when it closes. */
static BOOL a_relay_carries_a_client_both_ways(void)
{
	const BYTE hash[32] = { 12 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	TEST(freerdp_udp_listener_start(listener));

	int pair[2] = { -1, -1 };
	TEST(freerdp_udp_socket_pair(pair));
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_flow_set_socket(flow, pair[0]));
	TEST(freerdp_udp_listener_relay(listener, hash, pair[1]));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(flow), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, flow, data, &length));
	TEST(length == sizeof(syn));
	TEST(freerdp_udp_flow_send(flow, (const BYTE*)"back", 4));
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(client, reply, sizeof(reply), 0) == 4);
	TEST(memcmp(reply, "back", 4) == 0);

	/* free the flow: the relay ends, which frees the hash */
	freerdp_udp_flow_free(flow);
	BOOL free_again = FALSE;
	for (size_t x = 0; !free_again && (x < 200); x++)
	{
		int next[2] = { -1, -1 };
		TEST(freerdp_udp_socket_pair(next));
		free_again = freerdp_udp_listener_relay(listener, hash, next[1]);
		if (!free_again)
		{
			closesocket((SOCKET)next[1]);
			Sleep(5);
		}
		closesocket((SOCKET)next[0]);
	}
	TEST(free_again);

	freerdp_udp_listener_free(listener);
	closesocket(client);
	return TRUE;
}

#if !defined(_WIN32)
/* The other process of a server that serves its clients outside the process with the port. It has a
 * flow on one end of the pair, as OfferTunnel configures it. */
static int relay_child(int fd, const BYTE hash[32])
{
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	if (!flow)
		return 1;
	if (!freerdp_udp_flow_set_socket(flow, fd))
		return 2;

	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	if (WaitForSingleObject(freerdp_udp_flow_get_event(flow), 5000) != WAIT_OBJECT_0)
		return 3;
	if (!udpflow_read(flow, data, &length) || (length != SYN_LENGTH))
		return 4;
	if (!freerdp_udp_flow_send(flow, (const BYTE*)"child", 5))
		return 5;
	if (WaitForSingleObject(freerdp_udp_flow_get_event(flow), 5000) != WAIT_OBJECT_0)
		return 6;
	if (!udpflow_read(flow, data, &length) || (length != 3) || (memcmp(data, "abc", 3) != 0))
		return 7;
	freerdp_udp_flow_free(flow); /* closes the socket, which ends the relay */
	return 0;
}

static BOOL another_process_serves_a_client_through_a_relay(void)
{
	const BYTE hash[32] = { 8 };
	const BYTE otherHash[32] = { 10 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;

	int pair[2] = { -1, -1 };
	TEST(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
	const pid_t child = fork();
	TEST(child >= 0);
	if (child == 0)
	{
		close(pair[0]);
		_exit(relay_child(pair[1], hash));
	}
	close(pair[1]);

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	TEST(freerdp_udp_listener_start(listener));
	TEST(freerdp_udp_listener_relay(listener, hash, pair[0]));

	/* a second process cannot take the client of the first, and keeps its socket */
	int other[2] = { -1, -1 };
	TEST(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, other) == 0);
	TEST(!freerdp_udp_listener_relay(listener, hash, other[0]));
	TEST(freerdp_udp_listener_relay(listener, otherHash, other[0]));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));

	/* the answer comes from the address of the server, through the relay */
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(client, reply, sizeof(reply), 0) == 5);
	TEST(memcmp(reply, "child", 5) == 0);
	TEST(_send(client, "abc", 3, 0) == 3);

	int status = -1;
	TEST(waitpid(child, &status, 0) == child);
	TEST(WIFEXITED(status) && (WEXITSTATUS(status) == 0));

	/* the child is gone, its relay ends and the hash is free again */
	BOOL free_again = FALSE;
	for (size_t x = 0; !free_again && (x < 200); x++)
	{
		int next[2] = { -1, -1 };
		TEST(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, next) == 0);
		free_again = freerdp_udp_listener_relay(listener, hash, next[0]);
		if (!free_again)
		{
			close(next[0]);
			Sleep(5);
		}
		close(next[1]);
	}
	TEST(free_again);

	freerdp_udp_listener_free(listener);
	closesocket(client);
	close(other[1]);
	return TRUE;
}
#endif

int TestUdpListener(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	WSADATA wsaData = WINPR_C_ARRAY_INIT;
	if (WSAStartup(0x0202, &wsaData) != 0)
		return -1;

	int rc = -1;
	if (!sorts_by_cookie_hash_then_address())
		goto out;
	if (!a_bound_address_does_not_take_another_hash())
		goto out;
	if (!a_full_flow_loses_datagrams_only())
		goto out;
	if (!a_flow_can_send_any_way())
		goto out;
	if (!a_listener_runs_on_its_own_thread())
		goto out;
	if (!the_shared_listener_follows_its_flows())
		goto out;
	if (!a_relay_carries_a_client_both_ways())
		goto out;
#if !defined(_WIN32)
	if (!another_process_serves_a_client_through_a_relay())
		goto out;
#endif
	rc = 0;
out:
	WSACleanup();
	return rc;
}
