/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Test UI
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "tpkt.h"
#include "nego.h"
#include "transport.h"

#include <freerdp/utils/memory.h>

rdpNego * nego;
rdpTransport * transport;

int main(int argc, char* argv[])
{
	char* username;
	char* hostname;
	transport = transport_new((rdpSettings*) NULL);
	nego = nego_new(transport);

	if (argc < 3)
	{
		printf("Usage: freerdp-test <hostname> <username>\n");
		return 0;
	}

	hostname = (char*) xmalloc(strlen(argv[1]));
	memcpy(hostname, argv[1], strlen(argv[1]));
	hostname[strlen(argv[1])] = '\0';

	username = (char*) xmalloc(strlen(argv[2]));
	memcpy(username, argv[2], strlen(argv[2]));
	username[strlen(argv[2])] = '\0';

	printf("hostname: %s username: %s\n", hostname, username);

	nego_init(nego);
	nego_set_target(nego, hostname, 3389);
	nego_set_protocols(nego, 1, 1, 0);
	nego_set_cookie(nego, username);
	nego_connect(nego);

	transport_connect_tls(transport);

	return 0;
}
