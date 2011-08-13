/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Arguments Parsing
 *
 * Copyright 2009-2011 Jay Sorg
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/args.h>

/**
 * Parse command-line arguments and update rdpSettings members accordingly.
 * @param settings pointer to rdpSettings struct to be updated.
 * @param argc number of arguments available.
 * @param argv string array of the arguments.
 * @param plugin_callback function to be called when a plugin needs to be loaded.
 * @param plugin_user_data pointer to be passed to the plugin_callback function.
 * @param ui_callback function to be called when a UI-specific argument is being processed.
 * @param ui_user_data pointer to be passed to the ui_callback function.
 * @return number of arguments that has been parsed, or 0 if error occurred.
 */
int freerdp_parse_args(rdpSettings* settings, int argc, char** argv,
	ProcessPluginArgs plugin_callback, void* plugin_user_data,
	ProcessUIArgs ui_callback, void* ui_user_data)
{
	int t;
	char* p;
	int i, j;
	int index = 1;
	int num_extensions = 0;
	FRDP_PLUGIN_DATA* plugin_data;

	while (index < argc)
	{
		if (strcmp("-a", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing color depth\n");
				return 0;
			}
			settings->color_depth = atoi(argv[index]);
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing username\n");
				return 0;
			}
			settings->username = xstrdup(argv[index]);
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing password\n");
				return 0;
			}
			settings->password = xstrdup(argv[index]);
			settings->autologon = 1;

			/*
			 * Overwrite original password which could be revealed by a simple "ps aux" command.
			 * This approach won't hide the password length, but it is better than nothing.
			 */

			memset(argv[index], '*', strlen(argv[index]));
		}
		else if (strcmp("-d", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing domain\n");
				return 0;
			}
			settings->domain = xstrdup(argv[index]);
		}
		else if (strcmp("-s", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing shell\n");
				return 0;
			}
			settings->shell = xstrdup(argv[index]);
		}
		else if (strcmp("-c", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing directory\n");
				return 0;
			}
			settings->directory = xstrdup(argv[index]);
		}
		else if (strcmp("-g", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing width\n");
				return 0;
			}
			settings->width = strtol(argv[index], &p, 10);
			if (*p == 'x')
			{
				settings->height = strtol(p + 1, &p, 10);
			}
			else
			{
				if (ui_callback != NULL)
					ui_callback(settings, "-g", p, ui_user_data);
			}
		}
		else if (strcmp("-t", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing port number\n");
				return 0;
			}
			settings->port = atoi(argv[index]);
		}
		else if (strcmp("-n", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing client hostname\n");
				return 0;
			}
			strncpy(settings->client_hostname, argv[index], sizeof(settings->client_hostname) - 1);
			settings->client_hostname[sizeof(settings->client_hostname) - 1] = 0;
		}
		else if (strcmp("-o", argv[index]) == 0)
		{
			settings->console_audio = 1;
		}
		else if (strcmp("-0", argv[index]) == 0)
		{
			settings->console_session = 1;
		}
		else if (strcmp("-z", argv[index]) == 0)
		{
			settings->compression = 1;
		}
		else if (strcmp("--no-osb", argv[index]) == 0)
		{
			settings->offscreen_bitmap_cache = 0;
		}
		else if (strcmp("--rfx", argv[index]) == 0)
		{
			settings->rfx_decode = True;
			settings->fast_path_input = True;
			settings->color_depth = 32;
			settings->frame_acknowledge = False;
			settings->performance_flags = PERF_FLAG_NONE;
			settings->large_pointer = True;
		}
		else if (strcmp("-m", argv[index]) == 0)
		{
			settings->mouse_motion = 0;
		}
		else if (strcmp("--remote-app", argv[index]) == 0)
		{
			settings->remote_app = True;
		}
		else if (strcmp("-x", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing performance flag\n");
				return 0;
			}
			if (argv[index][0] == 'm') /* modem */
			{
				settings->performance_flags = PERF_DISABLE_WALLPAPER |
					PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS |
					PERF_DISABLE_THEMING;
			}
			else if (argv[index][0] == 'b') /* broadband */
			{
				settings->performance_flags = PERF_DISABLE_WALLPAPER;
			}
			else if (argv[index][0] == 'l') /* lan */
			{
				settings->performance_flags = PERF_FLAG_NONE;
			}
			else
			{
				settings->performance_flags = strtol(argv[index], 0, 16);
			}
		}
		else if (strcmp("--no-rdp", argv[index]) == 0)
		{
			settings->rdp_security = False;
		}
		else if (strcmp("--no-tls", argv[index]) == 0)
		{
			settings->tls_security = False;
		}
		else if (strcmp("--no-nla", argv[index]) == 0)
		{
			settings->nla_security = False;
		}
		else if (strcmp("--sec", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing protocol security\n");
				return 0;
			}
			if (strncmp("rdp", argv[index], 1) == 0) /* Standard RDP */
			{
				settings->rdp_security = True;
				settings->tls_security = False;
				settings->nla_security = False;
			}
			else if (strncmp("tls", argv[index], 1) == 0) /* TLS */
			{
				settings->rdp_security = False;
				settings->tls_security = True;
				settings->nla_security = False;
			}
			else if (strncmp("nla", argv[index], 1) == 0) /* NLA */
			{
				settings->rdp_security = False;
				settings->tls_security = False;
				settings->nla_security = True;
			}
			else
			{
				printf("unknown protocol security\n");
				return 0;
			}
		}
		else if (strcmp("--plugin", argv[index]) == 0)
		{
			index++;
			t = index;
			if (index == argc)
			{
				printf("missing plugin name\n");
				return 0;
			}
			plugin_data = NULL;
			if (index < argc - 1 && strcmp("--data", argv[index + 1]) == 0)
			{
				index += 2;
				i = 0;
				while (index < argc && strcmp("--", argv[index]) != 0)
				{
					plugin_data = (FRDP_PLUGIN_DATA*)xrealloc(plugin_data, sizeof(FRDP_PLUGIN_DATA) * (i + 2));
					plugin_data[i].size = sizeof(FRDP_PLUGIN_DATA);
					plugin_data[i + 1].size = 0;
					for (j = 0, p = argv[index]; j < 4 && p != NULL; j++)
					{
						if (*p == '\'')
						{
							plugin_data[i].data[j] = p + 1;
							p = strchr(p + 1, '\'');
							if (p)
								*p++ = 0;
						}
						else
							plugin_data[i].data[j] = p;

						p = strchr(p, ':');
						if (p != NULL)
							*p++ = 0;
					}
					index++;
					i++;
				}
			}

			if (plugin_callback != NULL)
			{
				if (!plugin_callback(settings, argv[t], plugin_data, plugin_user_data))
					return 0;
			}
		}
		else if (strcmp("--ext", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing extension name\n");
				return 0;
			}
			if (num_extensions >= sizeof(settings->extensions) / sizeof(struct rdp_ext_set))
			{
				printf("maximum extensions reached\n");
				return 0;
			}
			snprintf(settings->extensions[num_extensions].name,
				sizeof(settings->extensions[num_extensions].name),
				"%s", argv[index]);
			settings->extensions[num_extensions].data = NULL;
			if (index < argc - 1 && strcmp("--data", argv[index + 1]) == 0)
			{
				index += 2;
				settings->extensions[num_extensions].data = argv[index];
				i = 0;
				while (index < argc && strcmp("--", argv[index]) != 0)
				{
					index++;
					i++;
				}
			}
			num_extensions++;
		}
		else if (strcmp("--version", argv[index]) == 0)
		{
			printf("This is FreeRDP version %s\n", FREERDP_VERSION_FULL);
			return 0;
		}
		else if (argv[index][0] != '-')
		{
			if (argv[index][0] == '[' && (p = strchr(argv[index], ']'))
				&& (p[1] == 0 || (p[1] == ':' && !strchr(p + 2, ':'))))
			{
				/* Either "[...]" or "[...]:..." with at most one : after the brackets */
				settings->hostname = xstrdup(argv[index] + 1);
				if ((p = strchr((char*)settings->hostname, ']')))
				{
					*p = 0;
					if (p[1] == ':')
						settings->port = atoi(p + 2);
				}
			}
			else
			{
				/* Port number is cut off and used if exactly one : in the string */
				settings->hostname = xstrdup(argv[index]);
				if ((p = strchr((char*)settings->hostname, ':')) && !strchr(p + 1, ':'))
				{
					*p = 0;
					settings->port = atoi(p + 1);
				}
			}
			/* server is the last argument for the current session. arguments
			   followed will be parsed for the next session. */
			index++;

			return index;
		}
		else
		{
			if (ui_callback != NULL)
			{
				t = ui_callback(settings, argv[index], (index + 1 < argc && argv[index + 1][0] != '-' ?
					argv[index + 1] : NULL), ui_user_data);
				if (t == 0)
				{
					printf("invalid option: %s\n", argv[index]);
					return 0;
				}
				index += t - 1;
			}
		}
		index++;
	}
	printf("missing server name\n");
	return 0;
}
