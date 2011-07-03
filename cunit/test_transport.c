/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Transport Unit Tests
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
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/stream.h>

#include "tpkt.h"
#include "transport.h"
#include "test_transport.h"

static const char test_server[] = "192.168.1.200";

static const uint8 test_x224_req[] =
{
	"\x03\x00\x00\x2C\x27\xE0\x00\x00\x00\x00\x00\x43\x6F\x6F\x6B\x69"
	"\x65\x3A\x20\x6D\x73\x74\x73\x68\x61\x73\x68\x3D\x65\x6C\x74\x6F"
	"\x6e\x73\x0D\x0A\x01\x00\x08\x00\x00\x00\x00\x00"
};

int init_transport_suite(void)
{
	return 0;
}

int clean_transport_suite(void)
{
	return 0;
}

int add_transport_suite(void)
{
	add_test_suite(transport);

	add_test_function(transport);

	return 0;
}

static int test_finished = 0;

static int
packet_received(rdpTransport * transport, STREAM * stream, void * extra)
{
	uint16 length;
	length = tpkt_read_header(stream);
	CU_ASSERT(length == 19);
	freerdp_hexdump(stream->buffer, length);
	test_finished = 1;
}

void test_transport(void)
{
	rdpTransport * transport;
	STREAM * stream;
	int r;

	transport = transport_new();
	transport->recv_callback = packet_received;

	r = transport_connect(transport, test_server, 3389);
	CU_ASSERT(r == True);
	
	stream = stream_new(sizeof(test_x224_req));
	stream_write_buffer(stream, test_x224_req, sizeof(test_x224_req));
	r = transport_send(transport, stream);
	CU_ASSERT(r == 0);

	while (!test_finished)
	{
		transport_check_fds(transport);
		sleep(1);
	}

	r = transport_disconnect(transport);
	CU_ASSERT(r == True);

	transport_free(transport);
}
