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

#include "transport.h"
#include "test_transport.h"

static const char test_server[] = "192.168.0.1";

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

void test_transport(void)
{
	rdpTransport * transport;
	int r;

	transport = transport_new();

	r = transport_connect(transport, test_server, 3389);
	CU_ASSERT(r == 0);
	
	transport_disconnect(transport);
	CU_ASSERT(r == 0);

	transport_free(transport);
}
