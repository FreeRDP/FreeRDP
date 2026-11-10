/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * UDP listener and flow tests over loopback
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
#include <winpr/interlocked.h>
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

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <freerdp/crypto/certificate.h>
#include <freerdp/peer.h>
#include <freerdp/server/udp.h>

#include "../multitransport_internal.h"
#include "../rdp.h"
#include "../rdpemt.h"
#include "../rdpudp.h"
#include "../udplistener.h"
#include "../udplistener_internal.h"
#include "../../crypto/certificate.h"

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

/* [MS-RDPEUDP] 2.2.2.1, 2.2.2.5, 2.2.2.9: version 3 SYN with cookie hash, zero-padded to MTU */
static void make_syn(BYTE syn[SYN_LENGTH], const BYTE hash[32])
{
	memset(syn, 0, SYN_LENGTH);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, SYN_LENGTH);
	Stream_Write_UINT32_BE(s, 0xFFFFFFFF); /* Source acknowledgement number (snSourceAck) */
	Stream_Write_UINT16_BE(s, 64);         /* The receive window size */
	Stream_Write_UINT16_BE(s,
	                       0x0001 | 0x1000); /* Set RDPUDP_FLAG_SYN and RDPUDP_FLAG_SYNEX flags */
	Stream_Write_UINT32_BE(s, 1000);         /* Initial sequence number */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);   /* Upstream MTU */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);   /* Downstream MTU */
	Stream_Write_UINT16_BE(s, 0x0001);       /* Set the RDPUDP_VERSION_INFO_VALID flag */
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

/* Loopback socket on an ephemeral port; the listener takes ownership. */
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

/* Polls the listener until a datagram is queued in the flow. */
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
	TEST(freerdp_udp_listener_add_flow(listener, a));
	TEST(freerdp_udp_listener_add_flow(listener, b));
	TEST(!freerdp_udp_listener_add_flow(listener, a));

	const SOCKET clientA = open_client(port);
	const SOCKET clientB = open_client(port);
	const SOCKET stranger = open_client(port);
	TEST((clientA != INVALID_SOCKET) && (clientB != INVALID_SOCKET) &&
	     (stranger != INVALID_SOCKET));

	/* Send junk and a SYN with an unknown hash; the listener should maintain no state. */
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

	/* After SYN match, route datagrams by source address. */
	TEST(_send(clientB, "abc", 3, 0) == 3);
	TEST(_send(clientA, "de", 2, 0) == 2);
	TEST(pump(listener, b, data, &length));
	TEST((length == 3) && (memcmp(data, "abc", 3) == 0));
	TEST(pump(listener, a, data, &length));
	TEST((length == 2) && (memcmp(data, "de", 2) == 0));

	/* Replies are sent via the listener's socket. */
	TEST(freerdp_udp_flow_send(a, (const BYTE*)"reply", 5));
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(clientA, reply, sizeof(reply), 0) == 5);
	TEST(memcmp(reply, "reply", 5) == 0);

	freerdp_udp_flow_free(a);
	/* Keep flow b registered to verify the listener releases it. */
	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(b);
	closesocket(clientA);
	closesocket(clientB);
	closesocket(stranger);
	return TRUE;
}

/* Forgetting a flow on a non-owning listener is a no-op. The flow remains registered
 * with its owner until freed. */
static BOOL remove_flow_on_another_listener_is_a_no_op(void)
{
	const BYTE hash[32] = { 50 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* a = freerdp_udp_listener_new();
	freerdp_udp_listener* b = freerdp_udp_listener_new();
	TEST(a && b);
	TEST(freerdp_udp_listener_open_from_socket(a, (int)server));
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_listener_add_flow(a, flow));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(pump(a, flow, data, &length));

	freerdp_udp_listener_remove_flow(b, flow);
	TEST(!freerdp_udp_listener_add_flow(b, flow)); /* Flow is still owned by listener a. */
	TEST(_send(client, "abc", 3, 0) == 3);
	TEST(pump(a, flow, data, &length));
	TEST((length == 3) && (memcmp(data, "abc", 3) == 0));

	/* Freeing the flow removes it from listener a; the hash can be reused and
	 * subsequent client datagrams are dropped. */
	freerdp_udp_flow_free(flow);
	TEST(_send(client, "def", 3, 0) == 3);
	Sleep(20);
	TEST(freerdp_udp_listener_check(a));
	rdpUdpFlow* again = freerdp_udp_flow_new(hash);
	TEST(again);
	TEST(freerdp_udp_listener_add_flow(a, again));

	freerdp_udp_flow_free(again);
	freerdp_udp_listener_free(a);
	freerdp_udp_listener_free(b);
	closesocket(client);
	return TRUE;
}

/* Valid SYNs are padded to MTU; a truncated SYN with a known hash should not
 * hijack the flow, allowing the legitimate client to connect. */
static BOOL a_short_syn_is_ignored(void)
{
	const BYTE hash[32] = { 40 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_listener_add_flow(listener, flow));

	const SOCKET spoofer = open_client(port);
	const SOCKET client = open_client(port);
	TEST((spoofer != INVALID_SOCKET) && (client != INVALID_SOCKET));
	make_syn(syn, hash);
	TEST(_send(spoofer, (const char*)syn, 64, 0) == 64);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(pump(listener, flow, data, &length));
	TEST(length == sizeof(syn));
	TEST(!udpflow_read(flow, data, &length));

	freerdp_udp_flow_free(flow);
	freerdp_udp_listener_free(listener);
	closesocket(spoofer);
	closesocket(client);
	return TRUE;
}

static BOOL a_bound_address_rejects_another_cookie_hash(void)
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
	TEST(freerdp_udp_listener_add_flow(listener, a));
	TEST(freerdp_udp_listener_add_flow(listener, b));

	const SOCKET bound = open_client(port);
	const SOCKET other = open_client(port);
	TEST((bound != INVALID_SOCKET) && (other != INVALID_SOCKET));

	make_syn(synA, hashA);
	make_syn(synB, hashB);
	TEST(_send(bound, (const char*)synA, sizeof(synA), 0) == sizeof(synA));
	TEST(pump(listener, a, data, &length));

	/* SYN with hash B from address A is dropped; repeated SYN for A still arrives. */
	TEST(_send(bound, (const char*)synB, sizeof(synB), 0) == sizeof(synB));
	TEST(_send(bound, (const char*)synA, sizeof(synA), 0) == sizeof(synA));
	TEST(pump(listener, a, data, &length));
	TEST((length == sizeof(synA)) && (memcmp(data, synA, length) == 0));
	TEST(!udpflow_read(b, data, &length));

	/* Flow b remains available to its own client. */
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

static BOOL a_full_flow_drops_new_datagrams(void)
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
		if (freerdp_udp_flow_deliver(flow, number, sizeof(number)))
			taken++;
	}
	TEST((taken > 0) && (taken < 1000));
	TEST(!freerdp_udp_flow_deliver(flow, data, sizeof(data))); /* The flow buffer is now full. */
	TEST(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);

	/* Verify queued datagrams are read in order. */
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

	/* Exceeds maximum allowed datagram size */
	BYTE large[UDPFLOW_MAX_DATAGRAM + 1] = WINPR_C_ARRAY_INIT;
	TEST(!freerdp_udp_flow_deliver(flow, large, sizeof(large)));
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

static BOOL a_flow_sends_through_each_path(void)
{
	const BYTE hash[32] = { 5 };
	SendTrace trace = WINPR_C_ARRAY_INIT;

	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(memcmp(freerdp_udp_flow_get_cookie_hash(flow), hash, sizeof(hash)) == 0);
	/* Flow with no bound client cannot send */
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
	TEST(freerdp_udp_listener_add_flow(listener, flow));

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

/* Shared listener opens on the first flow and closes when the last flow is freed. */
static BOOL the_shared_listener_opens_and_closes_with_its_flows(void)
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
	TEST(udplistener_add_flow_on_address(local, sizeof(addr), first));
	TEST(udplistener_add_flow_on_address(local, sizeof(addr), second));
	/* A cookie hash must have a unique owner */
	TEST(!udplistener_add_flow_on_address(local, sizeof(addr), same));
	freerdp_udp_flow_free(same);

	/* IPv4-mapped addresses from dual-stack sockets should resolve to the same socket */
	struct sockaddr_in6 mapped = { 0 };
	mapped.sin6_family = AF_INET6;
	mapped.sin6_port = htons(port);
	mapped.sin6_addr.s6_addr[10] = 0xFF;
	mapped.sin6_addr.s6_addr[11] = 0xFF;
	memcpy(&mapped.sin6_addr.s6_addr[12], &addr.sin_addr, sizeof(addr.sin_addr));
	const BYTE mappedHash[32] = { 11 };
	rdpUdpFlow* viaMapped = freerdp_udp_flow_new(mappedHash);
	TEST(viaMapped);
	TEST(udplistener_add_flow_on_address((const struct sockaddr*)&mapped, sizeof(mapped),
	                                     viaMapped));
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

	/* Ensure no other socket can bind the port and intercept datagrams */
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

/* Verify that a relay supports bidirectional communication and that freeing the flow
 * releases the cookie hash. */
static BOOL a_relay_forwards_both_directions(void)
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
	TEST(freerdp_udp_listener_add_relay(listener, hash, pair[1]));

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

	/* Freeing the flow terminates the relay and releases the hash */
	freerdp_udp_flow_free(flow);
	BOOL free_again = FALSE;
	for (size_t x = 0; !free_again && (x < 200); x++)
	{
		int next[2] = { -1, -1 };
		TEST(freerdp_udp_socket_pair(next));
		free_again = freerdp_udp_listener_add_relay(listener, hash, next[1]);
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

/* Verify concurrent clients reach their respective flows via hash, then source address. */
static BOOL many_clients_reach_their_own_flows(void)
{
	enum
	{
		CLIENTS = 64
	};
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	rdpUdpFlow* flows[CLIENTS] = WINPR_C_ARRAY_INIT;
	SOCKET clients[CLIENTS] = WINPR_C_ARRAY_INIT;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));

	for (size_t x = 0; x < CLIENTS; x++)
	{
		BYTE hash[32] = { 0x40, (BYTE)x };
		flows[x] = freerdp_udp_flow_new(hash);
		TEST(flows[x]);
		TEST(freerdp_udp_listener_add_flow(listener, flows[x]));
		clients[x] = open_client(port);
		TEST(clients[x] != INVALID_SOCKET);
	}

	/* Send SYNs in reverse order, followed by one datagram per client */
	for (size_t x = CLIENTS; x > 0; x--)
	{
		const BYTE hash[32] = { 0x40, (BYTE)(x - 1) };
		make_syn(syn, hash);
		TEST(_send(clients[x - 1], (const char*)syn, sizeof(syn), 0) == sizeof(syn));
		TEST(pump(listener, flows[x - 1], data, &length));
		TEST(length == sizeof(syn));
	}
	for (size_t x = 0; x < CLIENTS; x++)
	{
		const BYTE id = (BYTE)x;
		TEST(_send(clients[x], (const char*)&id, 1, 0) == 1);
	}
	for (size_t x = 0; x < CLIENTS; x++)
	{
		TEST(pump(listener, flows[x], data, &length));
		TEST((length == 1) && (data[0] == (BYTE)x));
	}

	/* Freeing a flow also removes its associated address binding */
	freerdp_udp_flow_free(flows[0]);
	flows[0] = nullptr;
	TEST(_send(clients[0], "x", 1, 0) == 1);
	TEST(_send(clients[1], "y", 1, 0) == 1);
	TEST(pump(listener, flows[1], data, &length));
	TEST((length == 1) && (data[0] == 'y'));

	freerdp_udp_listener_free(listener);
	for (size_t x = 0; x < CLIENTS; x++)
	{
		freerdp_udp_flow_free(flows[x]);
		closesocket(clients[x]);
	}
	return TRUE;
}

/* Wait until the listener accepts a new relay for the hash (i.e., old relay has ended). */
static BOOL relay_hash_is_free(freerdp_udp_listener* listener, const BYTE hash[32])
{
	for (size_t x = 0; x < 200; x++)
	{
		int next[2] = { -1, -1 };
		if (!freerdp_udp_socket_pair(next))
			return FALSE;
		const BOOL taken = freerdp_udp_listener_add_relay(listener, hash, next[1]);
		if (!taken)
			closesocket((SOCKET)next[1]);
		closesocket((SOCKET)next[0]);
		if (taken)
			return TRUE;
		Sleep(5);
	}
	return FALSE;
}

/* Relay ends on request. Oversized datagrams are dropped. */
static BOOL removing_a_relay_ends_it(void)
{
	const BYTE hash[32] = { 60 };
	const BYTE unknown[32] = { 61 };
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
	TEST(freerdp_udp_listener_add_relay(listener, hash, pair[1]));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(flow), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, flow, data, &length));

	static BYTE large[UDPFLOW_MAX_DATAGRAM + 1];
	TEST(_send(client, (const char*)large, sizeof(large), 0) == sizeof(large));
	TEST(_send(client, "small", 5, 0) == 5);
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(flow), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, flow, data, &length));
	TEST((length == 5) && (memcmp(data, "small", 5) == 0));

	/* Waking the relayed flow's worker sends nothing to the client. */
	udpflow_wake(flow);
	HANDLE events[2] = WINPR_C_ARRAY_INIT;
	const DWORD count = udpflow_get_events(flow, events);
	TEST(count == 2);
	TEST(WaitForMultipleObjects(count, events, FALSE, 1000) == WAIT_OBJECT_0);
#if !defined(_WIN32)
	Sleep(100);
	char junk[16] = WINPR_C_ARRAY_INIT;
	TEST(recv((int)client, junk, sizeof(junk), MSG_DONTWAIT) < 0);
#endif

#if !defined(_WIN32)
	/* Stream sockets are rejected as they do not preserve datagram boundaries. */
	int stream[2] = { -1, -1 };
	TEST(socketpair(AF_UNIX, SOCK_STREAM, 0, stream) == 0);
	TEST(!freerdp_udp_listener_add_relay(listener, unknown, stream[1]));
	close(stream[0]);
	close(stream[1]);
#endif

	TEST(!freerdp_udp_listener_remove_relay(listener, unknown));
	TEST(freerdp_udp_listener_remove_relay(listener, hash));
	TEST(relay_hash_is_free(listener, hash));

	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(flow);
	closesocket(client);
	return TRUE;
}

#if !defined(_WIN32)
/* SOCK_DGRAM socket pairs do not provide EOF on close; the relay ends only when a send to the
 * closed end fails. */
static BOOL a_datagram_relay_ends_when_its_peer_closes(void)
{
	const BYTE hash[32] = { 62 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	TEST(freerdp_udp_listener_start(listener));

	int pair[2] = { -1, -1 };
	TEST(socketpair(AF_UNIX, SOCK_DGRAM, 0, pair) == 0);
	TEST(freerdp_udp_listener_add_relay(listener, hash, pair[1]));
	close(pair[0]); /* The peer disconnects without sending the empty end-of-relay datagram. */

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(relay_hash_is_free(listener, hash));

	freerdp_udp_listener_free(listener);
	closesocket(client);
	return TRUE;
}
#endif

/* A single relay thread serves multiple relays concurrently. */
static BOOL relays_share_one_thread(void)
{
	const BYTE hashA[32] = { 20 };
	const BYTE hashB[32] = { 21 };
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

	int pairA[2] = { -1, -1 };
	int pairB[2] = { -1, -1 };
	TEST(freerdp_udp_socket_pair(pairA));
	TEST(freerdp_udp_socket_pair(pairB));
	rdpUdpFlow* a = freerdp_udp_flow_new(hashA);
	rdpUdpFlow* b = freerdp_udp_flow_new(hashB);
	TEST(a && b);
	TEST(freerdp_udp_flow_set_socket(a, pairA[0]));
	TEST(freerdp_udp_flow_set_socket(b, pairB[0]));
	TEST(freerdp_udp_listener_add_relay(listener, hashA, pairA[1]));
	TEST(freerdp_udp_listener_add_relay(listener, hashB, pairB[1]));

	const SOCKET clientA = open_client(port);
	const SOCKET clientB = open_client(port);
	TEST((clientA != INVALID_SOCKET) && (clientB != INVALID_SOCKET));
	make_syn(syn, hashA);
	TEST(_send(clientA, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	make_syn(syn, hashB);
	TEST(_send(clientB, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(a), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, a, data, &length));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(b), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, b, data, &length));

	/* Empty datagrams must be dropped to avoid being mistaken for the end of the relay. */
	TEST(_send(clientA, "", 0, 0) == 0);
	TEST(_send(clientA, "after", 5, 0) == 5);
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(a), 2000) == WAIT_OBJECT_0);
	TEST(pump(nullptr, a, data, &length));
	TEST((length == 5) && (memcmp(data, "after", 5) == 0));

	TEST(freerdp_udp_flow_send(b, (const BYTE*)"to b", 4));
	TEST(freerdp_udp_flow_send(a, (const BYTE*)"to a", 4));
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(clientA, reply, sizeof(reply), 0) == 4);
	TEST(memcmp(reply, "to a", 4) == 0);
	TEST(_recv(clientB, reply, sizeof(reply), 0) == 4);
	TEST(memcmp(reply, "to b", 4) == 0);

	/* Ending one relay should not affect others. */
	freerdp_udp_flow_free(a);
	Sleep(50);
	TEST(freerdp_udp_flow_send(b, (const BYTE*)"more", 4));
	TEST(_recv(clientB, reply, sizeof(reply), 0) == 4);
	TEST(memcmp(reply, "more", 4) == 0);

	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(b);
	closesocket(clientA);
	closesocket(clientB);
	return TRUE;
}

#if defined(__linux__)
/* When the listener binds to the wildcard address, the reply must use the destination address
 * of the request. On Linux, 127.0.0.0/8 is local, but the return route typically selects
 * 127.0.0.1. */
static BOOL a_reply_uses_the_request_destination_address(void)
{
	const BYTE hash[32] = { 30 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;

	/* Find a free port for the wildcard address. */
	UINT16 port = 0;
	const SOCKET probe = open_server(&port);
	TEST(probe != INVALID_SOCKET);
	closesocket(probe);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open(listener, "0.0.0.0", port));

	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_listener_add_flow(listener, flow));

	const SOCKET client = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	TEST(client != INVALID_SOCKET);
	struct sockaddr_in server = { 0 };
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK + 1);
	server.sin_port = htons(port);
	make_syn(syn, hash);
	TEST(_sendto(client, (const char*)syn, sizeof(syn), 0, (const struct sockaddr*)&server,
	             sizeof(server)) == sizeof(syn));
	TEST(pump(listener, flow, data, &length));

	TEST(freerdp_udp_flow_send(flow, (const BYTE*)"reply", 5));
	char reply[8] = WINPR_C_ARRAY_INIT;
	struct sockaddr_in from = { 0 };
	socklen_t fromLength = sizeof(from);
	TEST(_recvfrom(client, reply, sizeof(reply), 0, (struct sockaddr*)&from, &fromLength) == 5);
	TEST(from.sin_addr.s_addr == htonl(INADDR_LOOPBACK + 1));
	TEST(from.sin_port == htons(port));

	freerdp_udp_flow_free(flow);
	freerdp_udp_listener_free(listener);
	closesocket(client);
	return TRUE;
}
#endif

#if !defined(_WIN32)
/* Child process side of a server serving clients from a process other than the port owner;
 * uses a flow on one end of a socket pair, similar to OfferTunnel. */
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
	freerdp_udp_flow_free(flow); /* Freeing the flow closes the socket, ending the relay. */
	return 0;
}

static BOOL a_forked_process_serves_a_client_through_a_relay(void)
{
	const BYTE hash[32] = { 8 };
	const BYTE otherHash[32] = { 10 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;

	int pair[2] = { -1, -1 };
	TEST(freerdp_udp_socket_pair(pair));
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
	TEST(freerdp_udp_listener_add_relay(listener, hash, pair[0]));

	/* A second process cannot claim an existing hash; a refused relay must not close the
	 * provided socket. */
	int other[2] = { -1, -1 };
	TEST(freerdp_udp_socket_pair(other));
	TEST(!freerdp_udp_listener_add_relay(listener, hash, other[0]));
	TEST(freerdp_udp_listener_add_relay(listener, otherHash, other[0]));

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));

	/* The child's reply is relayed and appears to come from the server's address. */
	char reply[8] = WINPR_C_ARRAY_INIT;
	TEST(_recv(client, reply, sizeof(reply), 0) == 5);
	TEST(memcmp(reply, "child", 5) == 0);
	TEST(_send(client, "abc", 3, 0) == 3);

	int status = -1;
	TEST(waitpid(child, &status, 0) == child);
	TEST(WIFEXITED(status) && (WEXITSTATUS(status) == 0));

	/* Once the child process exits, the relay ends and the hash becomes available. */
	BOOL free_again = FALSE;
	for (size_t x = 0; !free_again && (x < 200); x++)
	{
		int next[2] = { -1, -1 };
		TEST(freerdp_udp_socket_pair(next));
		free_again = freerdp_udp_listener_add_relay(listener, hash, next[0]);
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

/* A child that forks without exec closes its copies of the listener's sockets with
 * freerdp_udp_listener_close_after_fork() and keeps running. The parent's listener still serves
 * its flows, and once the parent frees it nothing holds the port any more. */
static BOOL a_forked_child_closes_its_copy_of_the_listener(void)
{
	const BYTE hash[32] = { 12 };
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	int closed[2] = { -1, -1 };
	int go[2] = { -1, -1 };
	TEST((pipe(closed) == 0) && (pipe(go) == 0));

	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	TEST(flow);
	TEST(freerdp_udp_listener_add_flow(listener, flow));
	TEST(freerdp_udp_listener_start(listener));

	const pid_t child = fork();
	TEST(child >= 0);
	if (child == 0)
	{
		freerdp_udp_listener_close_after_fork(listener);
		char c = 0;
		_exit(((write(closed[1], "c", 1) == 1) && (read(go[0], &c, 1) == 1)) ? 0 : 1);
	}
	char c = 0;
	TEST(read(closed[0], &c, 1) == 1);

	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(pump(nullptr, flow, data, &length) && (length == sizeof(syn)));

	freerdp_udp_listener_free(listener);
	freerdp_udp_flow_free(flow);
	const SOCKET again = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	const BOOL rebound = (again != INVALID_SOCKET) &&
	                     (_bind(again, (const struct sockaddr*)&addr, sizeof(addr)) == 0);
	closesocket(again);

	int status = -1;
	TEST(write(go[1], "g", 1) == 1);
	TEST(waitpid(child, &status, 0) == child);
	closesocket(client);
	for (size_t x = 0; x < 2; x++)
	{
		close(closed[x]);
		close(go[x]);
	}
	if (!rebound)
		(void)printf("%s: the child still holds the listener's socket\n", __func__);
	TEST(rebound);
	TEST(WIFEXITED(status) && (WEXITSTATUS(status) == 0));
	return TRUE;
}
#endif

static BOOL offer_nothing(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED rdpUdpFlow* flow,
                          WINPR_ATTR_UNUSED const BYTE cookieHash[32])
{
	return FALSE;
}

static freerdp_peer* test_peer_new(void)
{
	freerdp_peer* peer = calloc(1, sizeof(freerdp_peer));
	if (!peer)
		return nullptr;
	peer->ContextSize = sizeof(rdpContext);
	if (!freerdp_peer_context_new(peer))
	{
		free(peer);
		return nullptr;
	}
	return peer;
}

static void test_peer_free(freerdp_peer* peer)
{
	if (!peer)
		return;
	freerdp_peer_context_free(peer);
	free(peer);
}

/* The peer's UDP settings: callbacks are taken from a struct of at least the 3.34.0 size and
 * cleared with nullptr, the send limit is kept for the next tunnel, and without a tunnel there is
 * no send state. */
static BOOL peer_udp_settings_without_a_tunnel(void)
{
	FreeRDP_UdpPeerCallbacks callbacks = WINPR_C_ARRAY_INIT;
	FreeRDP_UdpSendState state = WINPR_C_ARRAY_INIT;
	int custom = 0;
	const size_t required =
	    offsetof(FreeRDP_UdpPeerCallbacks, ManualWorker) + sizeof(callbacks.ManualWorker);

	TEST(!freerdp_peer_set_udp_callbacks(nullptr, nullptr, nullptr));
	TEST(!freerdp_peer_set_udp_send_limit(nullptr, 1));
	state.size = sizeof(state);
	TEST(!freerdp_peer_get_udp_send_state(nullptr, &state));

	freerdp_peer* peer = test_peer_new();
	TEST(peer);
	rdpMultitransport* multi = peer->context->rdp->multitransport;
	TEST(multi);

	callbacks.OfferTunnel = offer_nothing;
	callbacks.ManualWorker = TRUE;
	callbacks.Size = required - 1;
	TEST(!freerdp_peer_set_udp_callbacks(peer, &callbacks, &custom));
	TEST(!multi->udpCallbacks.OfferTunnel && !multi->udpCallbacks.ManualWorker);

	callbacks.Size = required;
	TEST(freerdp_peer_set_udp_callbacks(peer, &callbacks, &custom));
	TEST((multi->udpCallbacks.OfferTunnel == offer_nothing) && multi->udpCallbacks.ManualWorker);
	TEST(multi->udpCustom == &custom);

	TEST(freerdp_peer_set_udp_callbacks(peer, nullptr, nullptr));
	TEST(!multi->udpCallbacks.OfferTunnel && !multi->udpCallbacks.ManualWorker);

	TEST(freerdp_peer_set_udp_send_limit(peer, 12345));
	TEST(multi->udpSendLimit == 12345);
	TEST(freerdp_peer_set_udp_send_limit(peer, 0));
	TEST(multi->udpSendLimit == 0);

	state.size = offsetof(FreeRDP_UdpSendState, rateBytesPerSecond);
	TEST(!freerdp_peer_get_udp_send_state(peer, &state));
	state.size = sizeof(state);
	TEST(!freerdp_peer_get_udp_send_state(peer, &state));

	test_peer_free(peer);
	return TRUE;
}

/* A self-signed RSA certificate for the server end of a tunnel. */
static BOOL make_certificate(X509** cert, EVP_PKEY** key)
{
	*cert = nullptr;
	*key = nullptr;
	EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
	BOOL rc = ctx && (EVP_PKEY_keygen_init(ctx) == 1) &&
	          (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) == 1) &&
	          (EVP_PKEY_keygen(ctx, key) == 1);
	EVP_PKEY_CTX_free(ctx);
	if (!rc)
		return FALSE;

	X509* x = X509_new();
	X509_NAME* name = x ? X509_get_subject_name(x) : nullptr;
	rc = name && (X509_set_version(x, 2) == 1) &&
	     (ASN1_INTEGER_set(X509_get_serialNumber(x), 1) == 1) &&
	     X509_gmtime_adj(X509_getm_notBefore(x), 0) &&
	     X509_gmtime_adj(X509_getm_notAfter(x), 3600) && (X509_set_pubkey(x, *key) == 1) &&
	     (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"localhost",
	                                 -1, -1, 0) == 1) &&
	     (X509_set_issuer_name(x, name) == 1) && (X509_sign(x, *key, EVP_sha256()) > 0);
	if (!rc)
	{
		X509_free(x);
		EVP_PKEY_free(*key);
		*key = nullptr;
		return FALSE;
	}
	*cert = x;
	return TRUE;
}

typedef struct
{
	volatile LONG events;
	volatile LONG ready;
	volatile LONG failed;
	DWORD owner;                 /* 0, or the only thread its events may come on */
	volatile LONG foreignThread; /* an event came on another thread than owner */
	char data[16];
	volatile LONG dataLength;
} TunnelEnd;

static void tunnel_event(void* custom, WINPR_ATTR_UNUSED rdpEmt* emt, RDPEMT_EVENT event,
                         const BYTE* data, size_t length)
{
	TunnelEnd* end = custom;
	(void)InterlockedIncrement(&end->events);
	if (end->owner && (end->owner != GetCurrentThreadId()))
		(void)InterlockedExchange(&end->foreignThread, 1);
	if (event == RDPEMT_EVENT_READY)
		(void)InterlockedExchange(&end->ready, 1);
	else if (event == RDPEMT_EVENT_FAILED)
		(void)InterlockedExchange(&end->failed, 1);
	else if ((event == RDPEMT_EVENT_DATA) && (length < sizeof(end->data)))
	{
		memcpy(end->data, data, length);
		(void)InterlockedExchange(&end->dataLength, (LONG)length);
	}
}

/* One pass of an application event loop that owns both the listener and a ManualWorker tunnel:
 * wait on their events, then let each do its work. */
static BOOL drive(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	HANDLE events[8] = WINPR_C_ARRAY_INIT;
	DWORD count = freerdp_udp_listener_get_event_handles(listener, events, ARRAYSIZE(events) - 1);
	TEST(count > 0);
	/* re-fetched before each wait, see freerdp_udp_flow_get_event() */
	events[count++] = freerdp_udp_flow_get_event(flow);
	TEST(events[count - 1]);
	UINT32 timeout = freerdp_udp_flow_get_timeout(flow);
	if (timeout > 20)
		timeout = 20;
	TEST(WaitForMultipleObjects(count, events, FALSE, timeout) != WAIT_FAILED);
	TEST(freerdp_udp_listener_check(listener));
	TEST(freerdp_udp_flow_check(flow));
	return TRUE;
}

/* With ManualWorker the server end of a tunnel has no thread of its own: the application drives
 * it through freerdp_udp_flow_get_event(), _get_timeout() and _check(). A FreeRDP client tunnel
 * sets up TLS and the Tunnel Create Request with it, and data then crosses in both directions. */
static BOOL a_manual_tunnel_runs_on_the_application_thread(void)
{
	const UINT32 requestId = 0x01020304;
	const BYTE cookie[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
	TunnelEnd serverEnd = WINPR_C_ARRAY_INIT;
	TunnelEnd clientEnd = WINPR_C_ARRAY_INIT;
	BOOL rc = FALSE;
	X509* cert = nullptr;
	EVP_PKEY* key = nullptr;
	rdpCertificate* rdpCert = nullptr;
	BYTE* publicKey = nullptr;
	DWORD publicKeyLength = 0;
	freerdp_udp_listener* listener = nullptr;
	rdpEmt* server = nullptr;
	rdpEmt* client = nullptr;
	rdpUdpFlow* flow = nullptr;
	freerdp_peer* peer = nullptr;
	rdpMultitransport* multi = nullptr;
	FreeRDP_UdpSendState state = WINPR_C_ARRAY_INIT;
	UINT16 port = 0;
	SOCKET sockfd = INVALID_SOCKET;
	struct sockaddr_in addr = WINPR_C_ARRAY_INIT;
	const UINT64 deadline = GetTickCount64() + 10000;

	if (!make_certificate(&cert, &key))
		goto out;
	rdpCert = freerdp_certificate_new_from_x509(cert, nullptr);
	if (!rdpCert || !freerdp_certificate_get_public_key(rdpCert, &publicKey, &publicKeyLength))
		goto out;

	sockfd = open_server(&port);
	listener = freerdp_udp_listener_new();
	if ((sockfd == INVALID_SOCKET) || !listener ||
	    !freerdp_udp_listener_open_from_socket(listener, (int)sockfd))
		goto out;

	server = rdpemt_new_server(WLog_Get("com.freerdp.core.test.udplistener"), requestId, cookie,
	                           cert, key, TRUE, tunnel_event, &serverEnd);
	flow = server ? rdpemt_get_flow(server) : nullptr;
	if (!flow || !freerdp_udp_listener_add_flow(listener, flow))
		goto out;

	client = rdpemt_new(WLog_Get("com.freerdp.core.test.udplistener"), requestId, cookie,
	                    "localhost", publicKey, publicKeyLength, tunnel_event, &clientEnd);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	serverEnd.owner = GetCurrentThreadId();
	if (!client || !rdpemt_start(client, (const struct sockaddr*)&addr, sizeof(addr)))
		goto out;

	/* The listener hands the client's SYN to the flow, but without freerdp_udp_flow_check() the
	 * server end does nothing: there is no thread of its own to answer it. */
	for (UINT64 idle = GetTickCount64() + 500; GetTickCount64() < idle;)
	{
		HANDLE events[8] = WINPR_C_ARRAY_INIT;
		const DWORD count =
		    freerdp_udp_listener_get_event_handles(listener, events, ARRAYSIZE(events));
		if ((count == 0) || (WaitForMultipleObjects(count, events, FALSE, 20) == WAIT_FAILED) ||
		    !freerdp_udp_listener_check(listener))
			goto out;
	}
	if (serverEnd.events != 0)
	{
		(void)printf("%s: the server end ran without freerdp_udp_flow_check()\n", __func__);
		goto out;
	}

	/* SYN, TLS handshake, Tunnel Create Request and Response, all driven from here */
	while (!serverEnd.ready || !clientEnd.ready)
	{
		if ((GetTickCount64() > deadline) || serverEnd.failed || clientEnd.failed ||
		    !drive(listener, flow))
		{
			(void)printf("%s: tunnel setup failed\n", __func__);
			goto out;
		}
	}

	if (!rdpemt_send_data(client, (const BYTE*)"to server", 9))
		goto out;
	while (serverEnd.dataLength != 9)
	{
		if ((GetTickCount64() > deadline) || !drive(listener, flow))
			goto out;
	}
	if ((memcmp(serverEnd.data, "to server", 9) != 0) ||
	    !rdpemt_send_data(server, (const BYTE*)"to client", 9) || !rdpemt_flush(server))
		goto out;
	while (clientEnd.dataLength != 9)
	{
		if ((GetTickCount64() > deadline) || !drive(listener, flow))
			goto out;
	}
	if (memcmp(clientEnd.data, "to client", 9) != 0)
	{
		(void)printf("%s: data did not arrive intact\n", __func__);
		goto out;
	}
	/* every event of the server end came inside freerdp_udp_flow_check(), on this thread */
	if (serverEnd.foreignThread)
	{
		(void)printf("%s: a server event came on another thread\n", __func__);
		goto out;
	}

	/* A peer whose tunnel this is reports its send state, and takes a new send limit. */
	peer = test_peer_new();
	if (!peer)
		goto out;
	multi = peer->context->rdp->multitransport;
	if (!multi)
		goto out;
	EnterCriticalSection(&multi->lock);
	multi->reliable = server;
	LeaveCriticalSection(&multi->lock);
	state.size = sizeof(state);
	rc = freerdp_peer_get_udp_send_state(peer, &state) && (state.size == sizeof(state)) &&
	     !state.writeBlocked && (state.queuedBytes == 0) && (state.rttMs > 0) &&
	     (state.rateBytesPerSecond > 0) && freerdp_peer_set_udp_send_limit(peer, 1u << 20);
	if (!rc)
		(void)printf("%s: no send state for a ready tunnel\n", __func__);
out:
	if (multi)
	{
		EnterCriticalSection(&multi->lock);
		multi->reliable = nullptr;
		LeaveCriticalSection(&multi->lock);
	}
	test_peer_free(peer);
	rdpemt_free(client);
	if (listener && flow)
		freerdp_udp_listener_remove_flow(listener, flow);
	rdpemt_free(server);
	freerdp_udp_listener_free(listener);
	free(publicKey);
	freerdp_certificate_free(rdpCert);
	EVP_PKEY_free(key);
	X509_free(cert);
	return rc;
}

/* A session on a second local address ends: the shared listener closes that socket without
 * stopping its thread, so the sessions left keep their reader. */
static BOOL a_socket_no_session_uses_closes_without_a_restart(void)
{
	const BYTE hashA[32] = { 70 };
	const BYTE hashB[32] = { 71 };
	UINT16 port = 0;
	const SOCKET probe = open_server(&port);
	TEST(probe != INVALID_SOCKET);
	closesocket(probe);

	struct sockaddr_in addrA = { 0 };
	addrA.sin_family = AF_INET;
	addrA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addrA.sin_port = htons(port);
	struct sockaddr_in addrB = addrA;
	addrB.sin_addr.s_addr = htonl(INADDR_LOOPBACK + 1); /* 127.0.0.2 */

	/* skip where the host does not answer on 127.0.0.2 */
	SOCKET test = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	TEST(test != INVALID_SOCKET);
	const BOOL second = (_bind(test, (const struct sockaddr*)&addrB, sizeof(addrB)) == 0);
	closesocket(test);
	if (!second)
		return TRUE;

	rdpUdpFlow* a = freerdp_udp_flow_new(hashA);
	rdpUdpFlow* b = freerdp_udp_flow_new(hashB);
	TEST(a && b);
	TEST(udplistener_add_flow_on_address((const struct sockaddr*)&addrA, sizeof(addrA), a));
	TEST(udplistener_add_flow_on_address((const struct sockaddr*)&addrB, sizeof(addrB), b));
	freerdp_udp_listener* listener = a->listener;
	TEST(listener && (b->listener == listener) && listener->thread);
	const HANDLE thread = listener->thread;

	freerdp_udp_flow_free(b);
	TEST(listener->thread == thread);

	/* the thread closes the socket on 127.0.0.2 when it builds its next wait set */
	BOOL closed = FALSE;
	for (size_t x = 0; !closed && (x < 200); x++)
	{
		test = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		TEST(test != INVALID_SOCKET);
		closed = (_bind(test, (const struct sockaddr*)&addrB, sizeof(addrB)) == 0);
		closesocket(test);
		if (!closed)
			Sleep(10);
	}
	TEST(closed);

	/* the session on 127.0.0.1 still gets its datagrams */
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	make_syn(syn, hashA);
	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(freerdp_udp_flow_get_event(a), 2000) == WAIT_OBJECT_0);
	TEST(listener->thread == thread);

	closesocket(client);
	freerdp_udp_flow_free(a);
	return TRUE;
}

typedef struct
{
	rdpUdpFlow* flow;
	HANDLE start;
} udpFreeArg;

static DWORD WINAPI free_flow_thread(LPVOID arg)
{
	udpFreeArg* job = arg;
	(void)WaitForSingleObject(job->start, INFINITE);
	freerdp_udp_flow_free(job->flow);
	return 0;
}

/* Peers free their flows while the application frees the listener; with the sanitizers this
 * finds a listener or entry used after it was freed. */
static BOOL a_listener_and_its_flows_free_on_different_threads(void)
{
	for (size_t round = 0; round < 200; round++)
	{
		HANDLE start = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		freerdp_udp_listener* listener = freerdp_udp_listener_new();
		TEST(start && listener);

		udpFreeArg args[4] = WINPR_C_ARRAY_INIT;
		HANDLE threads[ARRAYSIZE(args)] = WINPR_C_ARRAY_INIT;
		for (size_t x = 0; x < ARRAYSIZE(args); x++)
		{
			const BYTE hash[32] = { (BYTE)(x + 1) };
			args[x].flow = freerdp_udp_flow_new(hash);
			args[x].start = start;
			TEST(args[x].flow && freerdp_udp_listener_add_flow(listener, args[x].flow));
			threads[x] = CreateThread(nullptr, 0, free_flow_thread, &args[x], 0, nullptr);
			TEST(threads[x]);
		}
		(void)SetEvent(start);
		freerdp_udp_listener_free(listener);
		for (size_t x = 0; x < ARRAYSIZE(threads); x++)
		{
			(void)WaitForSingleObject(threads[x], INFINITE);
			(void)CloseHandle(threads[x]);
		}
		(void)CloseHandle(start);
	}
	return TRUE;
}

typedef struct
{
	LONG inCall;
	HANDLE entered;
	rdpUdpFlow* flow;
} udpSlowCallback;

static BOOL slow_callback(void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                          WINPR_ATTR_UNUSED size_t length)
{
	udpSlowCallback* slow = custom;
	(void)InterlockedIncrement(&slow->inCall);
	(void)SetEvent(slow->entered);
	Sleep(100);
	(void)InterlockedDecrement(&slow->inCall);
	return TRUE;
}

static DWORD WINAPI slow_send_thread(LPVOID arg)
{
	udpSlowCallback* slow = arg;
	if (!freerdp_udp_flow_send(slow->flow, (const BYTE*)"x", 1))
		(void)printf("the send callback failed\n");
	return 0;
}

/* After set_send and set_receive return, the previous callback is no longer running, so its
 * custom may be freed. */
static BOOL setting_a_callback_waits_for_a_running_one(void)
{
	const BYTE hash[32] = { 60 };
	udpSlowCallback slow = WINPR_C_ARRAY_INIT;
	slow.entered = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	slow.flow = freerdp_udp_flow_new(hash);
	TEST(slow.entered && slow.flow);

	freerdp_udp_flow_set_send(slow.flow, slow_callback, &slow);
	HANDLE thread = CreateThread(nullptr, 0, slow_send_thread, &slow, 0, nullptr);
	TEST(thread);
	TEST(WaitForSingleObject(slow.entered, 5000) == WAIT_OBJECT_0);
	freerdp_udp_flow_set_send(slow.flow, nullptr, nullptr);
	TEST(InterlockedCompareExchange(&slow.inCall, 0, 0) == 0);
	(void)WaitForSingleObject(thread, INFINITE);
	(void)CloseHandle(thread);

	/* the receive callback runs on the listener's thread */
	(void)ResetEvent(slow.entered);
	UINT16 port = 0;
	const SOCKET server = open_server(&port);
	TEST(server != INVALID_SOCKET);
	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	TEST(listener);
	TEST(freerdp_udp_listener_open_from_socket(listener, (int)server));
	TEST(freerdp_udp_listener_add_flow(listener, slow.flow));
	freerdp_udp_flow_set_receive(slow.flow, slow_callback, &slow);
	TEST(freerdp_udp_listener_start(listener));
	const SOCKET client = open_client(port);
	TEST(client != INVALID_SOCKET);
	BYTE syn[SYN_LENGTH] = WINPR_C_ARRAY_INIT;
	make_syn(syn, hash);
	TEST(_send(client, (const char*)syn, sizeof(syn), 0) == sizeof(syn));
	TEST(WaitForSingleObject(slow.entered, 5000) == WAIT_OBJECT_0);
	freerdp_udp_flow_set_receive(slow.flow, nullptr, nullptr);
	TEST(InterlockedCompareExchange(&slow.inCall, 0, 0) == 0);

	freerdp_udp_flow_free(slow.flow);
	freerdp_udp_listener_free(listener);
	closesocket(client);
	(void)CloseHandle(slow.entered);
	return TRUE;
}

typedef struct
{
	LONG inCall;
	HANDLE entered;
	rdpMultitransport* multi;
} udpSlowHook;

static BOOL slow_state_changed(void* custom, WINPR_ATTR_UNUSED UINT32 tunnelType,
                               WINPR_ATTR_UNUSED FreeRDP_MultitransportTunnelState state)
{
	udpSlowHook* slow = custom;
	(void)InterlockedIncrement(&slow->inCall);
	(void)SetEvent(slow->entered);
	Sleep(100);
	(void)InterlockedDecrement(&slow->inCall);
	return TRUE;
}

static DWORD WINAPI notify_state_thread(LPVOID arg)
{
	udpSlowHook* slow = arg;
	if (!multitransport_notify_state(slow->multi, FREERDP_MULTITRANSPORT_TUNNEL_READY))
		(void)printf("the tunnel state hook failed\n");
	return 0;
}

/* The channel manager may be closed on another thread than the peer's check: clearing the DVC
 * hooks waits for a hook that is running, so its custom can be freed afterwards. */
static BOOL clearing_the_dvc_hooks_waits_for_a_running_one(void)
{
	freerdp_peer* peer = test_peer_new();
	TEST(peer);
	udpSlowHook slow = WINPR_C_ARRAY_INIT;
	slow.entered = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	slow.multi = peer->context->rdp->multitransport;
	TEST(slow.entered && slow.multi);

	FreeRDP_MultitransportDvcCallbacks hooks = WINPR_C_ARRAY_INIT;
	hooks.TunnelStateChanged = slow_state_changed;
	TEST(freerdp_multitransport_set_dvc_callbacks(peer->context, &hooks, &slow));
	HANDLE thread = CreateThread(nullptr, 0, notify_state_thread, &slow, 0, nullptr);
	TEST(thread);
	TEST(WaitForSingleObject(slow.entered, 5000) == WAIT_OBJECT_0);
	TEST(freerdp_multitransport_set_dvc_callbacks(peer->context, nullptr, nullptr));
	TEST(InterlockedCompareExchange(&slow.inCall, 0, 0) == 0);
	(void)WaitForSingleObject(thread, INFINITE);
	(void)CloseHandle(thread);

	(void)CloseHandle(slow.entered);
	test_peer_free(peer);
	return TRUE;
}

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
	if (!a_bound_address_rejects_another_cookie_hash())
		goto out;
	if (!remove_flow_on_another_listener_is_a_no_op())
		goto out;
	if (!a_short_syn_is_ignored())
		goto out;
	if (!a_full_flow_drops_new_datagrams())
		goto out;
	if (!a_flow_sends_through_each_path())
		goto out;
	if (!a_listener_runs_on_its_own_thread())
		goto out;
	if (!the_shared_listener_opens_and_closes_with_its_flows())
		goto out;
	if (!a_socket_no_session_uses_closes_without_a_restart())
		goto out;
	if (!a_relay_forwards_both_directions())
		goto out;
	if (!many_clients_reach_their_own_flows())
		goto out;
	if (!removing_a_relay_ends_it())
		goto out;
#if !defined(_WIN32)
	if (!a_datagram_relay_ends_when_its_peer_closes())
		goto out;
#endif
	if (!relays_share_one_thread())
		goto out;
	if (!peer_udp_settings_without_a_tunnel())
		goto out;
	if (!a_manual_tunnel_runs_on_the_application_thread())
		goto out;
	if (!a_listener_and_its_flows_free_on_different_threads())
		goto out;
	if (!setting_a_callback_waits_for_a_running_one())
		goto out;
	if (!clearing_the_dvc_hooks_waits_for_a_running_one())
		goto out;
#if defined(__linux__)
	if (!a_reply_uses_the_request_destination_address())
		goto out;
#endif
#if !defined(_WIN32)
	if (!a_forked_process_serves_a_client_through_a_relay())
		goto out;
	if (!a_forked_child_closes_its_copy_of_the_listener())
		goto out;
#endif
	rc = 0;
out:
	WSACleanup();
	return rc;
}
