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

#include "network.h"

rdpNetwork *
network_new(void)
{
	rdpNetwork * network;

	network = (rdpNetwork *) xmalloc(sizeof(rdpNetwork));
	memset(network, 0, sizeof(rdpNetwork));

	return network;
}

void
network_free(rdpNetwork * network)
{
	xfree(network);
}

int
network_connect(rdpNetwork * network, const char * server, int port)
{
	return 0;
}

int
network_disconnect(rdpNetwork * network)
{
	return 0;
}

int
network_start_tls(rdpNetwork * network)
{
	return 0;
}

int
network_send(rdpNetwork * network, STREAM * stream)
{
	return 0;
}

int
network_check_fds(rdpNetwork * network)
{
	return 0;
}
