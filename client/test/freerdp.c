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

#include "connection.h"

#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>

#define PARAM_EQUALS(_param)		(strcmp(_param, argv[*i]) == 0)

#define CHECK_VALUE_PRESENT(_msg)	do { \
	*i = *i + 1; \
	if (*i == argc) \
	{ printf(_msg "\n"); return False; } \
	} while(0)

#define STRING_COPY(_str)		do { \
	settings->_str = xmalloc(strlen(argv[*i])); \
	memcpy(settings->_str, argv[*i], strlen(argv[*i])); \
	settings->_str[strlen(argv[*i])] = '\0'; \
	} while(0)

boolean freerdp_process_params(int argc, char* argv[], rdpSettings* settings, int* i)
{
	char* p;

	if (argc < *i + 1)
	{
		if (*i == 1)
			printf("no parameters specified.\n");

		return False;
	}

	while (*i < argc)
	{
		if (PARAM_EQUALS("-a"))
		{
			CHECK_VALUE_PRESENT("missing server depth");
			settings->color_depth = atoi(argv[*i]);
		}
		else if (strcmp("-u", argv[*i]) == 0)
		{
			CHECK_VALUE_PRESENT("missing username");
			STRING_COPY(username);
		}
		else if (PARAM_EQUALS("-p"))
		{
			CHECK_VALUE_PRESENT("missing password");
			STRING_COPY(password);
		}
		else if (PARAM_EQUALS("-d"))
		{
			CHECK_VALUE_PRESENT("missing domain");
			STRING_COPY(domain);
		}
		else if (PARAM_EQUALS("-g"))
		{
			CHECK_VALUE_PRESENT("missing screen dimensions");

			settings->width = strtol(argv[*i], &p, 10);

			if (*p == 'x')
				settings->height = strtol(p + 1, &p, 10);

			if ((settings->width < 16) || (settings->height < 16) ||
					(settings->width > 4096) || (settings->height > 4096))
			{
				printf("invalid screen dimensions\n");
				return False;
			}
		}
		else if (PARAM_EQUALS("-n"))
		{
			CHECK_VALUE_PRESENT("missing hostname");
			STRING_COPY(hostname);
		}
		else if (PARAM_EQUALS("-o"))
		{
			settings->console_audio = True;
		}
		else if (PARAM_EQUALS("-0"))
		{
			settings->console_session = True;
		}
		else if (PARAM_EQUALS("-z"))
		{
			settings->compression = True;
		}
		else if (PARAM_EQUALS("--sec"))
		{
			CHECK_VALUE_PRESENT("missing protocol security");

			if (PARAM_EQUALS("rdp")) /* Standard RDP */
			{
				settings->rdp_security = 1;
				settings->tls_security = 0;
				settings->nla_security = 0;
			}
			else if (PARAM_EQUALS("tls")) /* TLS */
			{
				settings->rdp_security = 0;
				settings->tls_security = 1;
				settings->nla_security = 0;
			}
			else if (PARAM_EQUALS("nla")) /* NLA */
			{
				settings->rdp_security = 0;
				settings->tls_security = 0;
				settings->nla_security = 1;
			}
			else
			{
				printf("unknown protocol security\n");
				return False;
			}
		}
		else if (PARAM_EQUALS("-h") || PARAM_EQUALS("--help"))
		{
			printf("help\n");
			return False;
		}
		else if (argv[*i][0] != '-')
		{
			if (argv[*i][0] == '[' && (p = strchr(argv[*i], ']'))
				&& (p[1] == 0 || (p[1] == ':' && !strchr(p + 2, ':'))))
			{
				/* Either "[...]" or "[...]:..." with at most one : after the brackets */
				settings->hostname = (char*) xmalloc(strlen(argv[*i] + 1));
				strncpy(settings->hostname, argv[*i] + 1, strlen(argv[*i] + 1));

				if ((p = strchr((const char*)settings->hostname, ']')))
				{
					*p = 0;
					if (p[1] == ':')
						settings->port = (uint16) atoi(p + 2);
				}
			}
			else
			{
				/* Port number is cut off and used if exactly one : in the string */
				settings->hostname = (char*) xmalloc(strlen(argv[*i]));
				strncpy(settings->hostname, argv[*i], strlen(argv[*i]));

				if ((p = strchr((const char*)settings->hostname, ':')) && !strchr(p + 1, ':'))
				{
					*p = 0;
					settings->port = (uint16) atoi(p + 1);
				}
			}

			/*
			 * server hostname is the last argument for the current session.
			 * arguments followed will be parsed for the next session.
			 */

			*i = *i + 1;

			return True;
		}

		*i = *i + 1;
	}

	return True;
}

int main(int argc, char* argv[])
{
	rdpRdp* rdp;
	int index = 1;
	rdpSettings* settings;

	rdp = rdp_new();
	settings = rdp->settings;

	if (freerdp_process_params(argc, argv, settings, &index) != True)
	{
		printf("failed to process parameters.\n");
		return 0;
	}

	printf("hostname:%s username:%s password:%s\n",
			settings->hostname, settings->username, settings->password);

	rdp_client_connect(rdp);

	return 0;
}
