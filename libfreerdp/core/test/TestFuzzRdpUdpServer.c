/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Fuzzer for the server side of RDP-UDP
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

/* What a client can send to the server's shared UDP port, before and after it is accepted:
 *
 * - the whole input as a SYN, for rdpudp_get_syn_cookie_hash() and rdpudp_accept()
 * - a valid version 3 SYN, then the input as datagrams for rdpudp_input(), each prefixed with a
 *   16 bit big endian length, with the timers run in between
 *
 * The server sends its answers to a loopback socket that is emptied after every input. */

#include <stdint.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/wlog.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "../rdpudp.h"

#define SYN_LENGTH 1232

typedef struct
{
	BOOL ready;
	SOCKET server;
	SOCKET client;
	struct sockaddr_in clientAddr;
	wLog* log;
} FuzzCtx;

static FuzzCtx g_ctx;

static BOOL on_receive(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                       WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static SOCKET open_loopback(struct sockaddr_in* bound)
{
	SOCKET sockfd = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET)
		return INVALID_SOCKET;

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t length = sizeof(addr);
	u_long arg = 1;
	if ((_bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0) ||
	    (_getsockname(sockfd, (struct sockaddr*)bound, &length) != 0) ||
	    (_ioctlsocket(sockfd, FIONBIO, &arg) != 0))
	{
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}

static BOOL fuzz_init(void)
{
	if (g_ctx.ready)
		return TRUE;

	WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
	g_ctx.log = WLog_Get("com.freerdp.core.test.fuzz.rdpudp");

	struct sockaddr_in unused = { 0 };
	g_ctx.server = open_loopback(&unused);
	g_ctx.client = open_loopback(&g_ctx.clientAddr);
	if ((g_ctx.server == INVALID_SOCKET) || (g_ctx.client == INVALID_SOCKET))
		return FALSE;
	g_ctx.ready = TRUE;
	return TRUE;
}

/* what the server sent: nobody reads it, but a full socket buffer would fail its sends */
static void drain_client(void)
{
	char buffer[2048];
	while (_recv(g_ctx.client, buffer, sizeof(buffer), 0) > 0)
	{
	}
}

/* [MS-RDPEUDP] 2.2.2.1, 2.2.2.5 and 2.2.2.6: a version 3 SYN, zero padded to the MTU */
static void make_syn(BYTE syn[SYN_LENGTH])
{
	memset(syn, 0, SYN_LENGTH);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, SYN_LENGTH);
	Stream_Write_UINT32_BE(s, 0xFFFFFFFF); /* snSourceAck */
	Stream_Write_UINT16_BE(s, 64);         /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, 0x0001 | 0x1000); /* RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX */
	Stream_Write_UINT32_BE(s, 1000);            /* snInitialSequenceNumber */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uUpStreamMtu */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uDownStreamMtu */
	Stream_Write_UINT16_BE(s, 0x0001);          /* RDPUDP_VERSION_INFO_VALID */
	Stream_Write_UINT16_BE(s, RDPUDP_PROTOCOL_VERSION_3);
	/* cookieHash: zero */
}

static void fuzz_syn(const uint8_t* data, size_t size)
{
	BYTE hash[32] = WINPR_C_ARRAY_INIT;
	(void)rdpudp_get_syn_cookie_hash(data, size, hash);

	rdpUdp* udp = rdpudp_new(g_ctx.log, nullptr, on_receive, nullptr);
	if (!udp)
		return;
	(void)rdpudp_accept(udp, g_ctx.server, (const struct sockaddr*)&g_ctx.clientAddr,
	                    sizeof(g_ctx.clientAddr), data, size);
	rdpudp_free(udp);
}

static void fuzz_datagrams(const uint8_t* data, size_t size)
{
	rdpUdp* udp = rdpudp_new(g_ctx.log, nullptr, on_receive, nullptr);
	if (!udp)
		return;

	BYTE syn[SYN_LENGTH];
	make_syn(syn);
	if (!rdpudp_accept(udp, g_ctx.server, (const struct sockaddr*)&g_ctx.clientAddr,
	                   sizeof(g_ctx.clientAddr), syn, sizeof(syn)))
		goto out;

	size_t offset = 0;
	size_t count = 0;
	while (size - offset >= 2)
	{
		const size_t length = ((size_t)data[offset] << 8) | data[offset + 1];
		offset += 2;
		if (length > size - offset)
			break;
		/* rdpudp_input works in place, like on the listener's receive buffer */
		BYTE datagram[0x10000];
		memcpy(datagram, &data[offset], length);
		if (!rdpudp_input(udp, datagram, length))
			break;
		offset += length;
		if ((++count % 8) == 0)
		{
			if (!rdpudp_check(udp))
				break;
			drain_client();
		}
	}

out:
	rdpudp_free(udp);
	drain_client();
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);
int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if (!fuzz_init())
		return 0;
	fuzz_syn(Data, Size);
	fuzz_datagrams(Data, Size);
	return 0;
}
