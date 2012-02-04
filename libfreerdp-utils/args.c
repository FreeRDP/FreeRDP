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
#include <freerdp/settings.h>
#include <freerdp/utils/print.h>
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
 * @return number of arguments that were parsed, or FREERDP_ARGS_PARSE_RESULT on failure or --version/--help
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
	RDP_PLUGIN_DATA* plugin_data;

	while (index < argc)
	{
		if ((strcmp("-h", argv[index]) == 0 ) || (strcmp("--help", argv[index]) == 0 ))
		{
			printf("\n"
				"FreeRDP - A Free Remote Desktop Protocol Client\n"
				"See http://www.freerdp.com for more information\n"
				"\n"
				"Usage: %s [options] server:port\n"
				"  -0: connect to console session\n"
				"  -a: set color depth in bit, default is 16\n"
				"  -c: initial working directory\n"
				"  -D: hide window decorations\n"
				"  -T: window title\n"
				"  -d: domain\n"
				"  -f: fullscreen mode\n"
				"  -g: set geometry, using format WxH or X%% or 'workarea', default is 1024x768\n"
				"  -h: print this help\n"
				"  -k: set keyboard layout ID\n"
				"  -K: do not interfere with window manager bindings\n"
				"  -m: don't send mouse motion events\n"
				"  -n: hostname\n"
				"  -o: console audio\n"
				"  -p: password\n"
				"  -s: set startup-shell\n"
				"  -t: alternative port number, default is 3389\n"
				"  -u: username\n"
				"  -x: performance flags (m[odem], b[roadband] or l[an])\n"
				"  -X: embed into another window with a given XID.\n"
				"  -z: enable compression\n"
				"  --app: RemoteApp connection. This implies -g workarea\n"
				"  --ext: load an extension\n"
				"  --no-auth: disable authentication\n"
				"  --no-fastpath: disable fast-path\n"
				"  --gdi: graphics rendering (hw, sw)\n"
				"  --no-osb: disable offscreen bitmaps\n"
				"  --no-bmp-cache: disable bitmap cache\n"
				"  --plugin: load a virtual channel plugin\n"
				"  --rfx: enable RemoteFX\n"
				"  --rfx-mode: RemoteFX operational flags (v[ideo], i[mage]), default is video\n"
				"  --nsc: enable NSCodec (experimental)\n"
				"  --disable-wallpaper: disables wallpaper\n"
				"  --composition: enable desktop composition\n"
				"  --disable-full-window-drag: disables full window drag\n"
				"  --disable-menu-animations: disables menu animations\n"
				"  --disable-theming: disables theming\n"
				"  --no-rdp: disable Standard RDP encryption\n"
				"  --no-tls: disable TLS encryption\n"
				"  --no-nla: disable network level authentication\n"
				"  --ntlm: force NTLM authentication protocol version (1 or 2)\n"
				"  --ignore-certificate: ignore verification of logon certificate\n"
				"  --sec: force protocol security (rdp, tls or nla)\n"
				"  --secure-checksum: use salted checksums with Standard RDP encryption\n"
				"  --version: print version information\n"
				"\n", argv[0]);
			return FREERDP_ARGS_PARSE_HELP; //TODO: What is the correct return
		}
		else if (strcmp("-a", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing color depth\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->color_depth = atoi(argv[index]);
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing username\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->username = xstrdup(argv[index]);
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing password\n");
				return FREERDP_ARGS_PARSE_FAILURE;
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
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->domain = xstrdup(argv[index]);
		}
		else if (strcmp("-s", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing shell\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->shell = xstrdup(argv[index]);
		}
		else if (strcmp("-c", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing directory\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->directory = xstrdup(argv[index]);
		}
		else if (strcmp("-g", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing dimensions\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			if (strncmp("workarea", argv[index], 1) == 0)
			{
				settings->workarea = true;
			}
			else
			{
				settings->width = (uint16) strtol(argv[index], &p, 10);

				if (*p == 'x')
				{
					settings->height = (uint16) strtol(p + 1, &p, 10);
				}
				if (*p == '%')
				{
					settings->percent_screen = settings->width;
					if (settings->percent_screen <= 0 || settings->percent_screen > 100)
					{
						printf("invalid geometry percentage\n");
						return FREERDP_ARGS_PARSE_FAILURE;
					}
				}
				else
				{
					if (ui_callback != NULL)
						ui_callback(settings, "-g", p, ui_user_data);
				}
			}
		}
		else if (strcmp("-f", argv[index]) == 0)
		{
			settings->fullscreen = true;
		}
		else if (strcmp("-D", argv[index]) == 0)
		{
			settings->decorations = false;
		}
		else if (strcmp("-T", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing window title\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->window_title = xstrdup(argv[index]);
		}
		else if (strcmp("-t", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing port number\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->port = atoi(argv[index]);
		}
		else if (strcmp("-k", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing keyboard layout id\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			sscanf(argv[index], "%X", &(settings->kbd_layout));
		}
		else if (strcmp("-K", argv[index]) == 0)
		{
			settings->grab_keyboard = false;
		}
		else if (strcmp("-n", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing client hostname\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			strncpy(settings->client_hostname, argv[index], sizeof(settings->client_hostname) - 1);
			settings->client_hostname[sizeof(settings->client_hostname) - 1] = 0;
		}
		else if (strcmp("-o", argv[index]) == 0)
		{
			settings->console_audio = true;
		}
		else if (strcmp("-0", argv[index]) == 0)
		{
			settings->console_session = true;
		}
		else if (strcmp("-z", argv[index]) == 0)
		{
			settings->compression = true;
		}
		else if (strcmp("--ntlm", argv[index]) == 0)
		{
			index++;

			settings->ntlm_version = atoi(argv[index]);

			if (settings->ntlm_version != 2)
				settings->ntlm_version = 1;
		}
		else if (strcmp("--no-glyph-cache", argv[index]) == 0)
		{
			settings->glyph_cache = false;
		}
		else if (strcmp("--no-osb", argv[index]) == 0)
		{
			settings->offscreen_bitmap_cache = false;
		}
		else if (strcmp("--no-bmp-cache", argv[index]) == 0)
		{
			settings->bitmap_cache = false;
		}
		else if (strcmp("--no-auth", argv[index]) == 0)
		{
			settings->authentication = false;
		}
		else if (strcmp("--ignore-certificate", argv[index]) == 0)
		{
			settings->ignore_certificate = true;
		}
		else if (strcmp("--certificate-name", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing certificate name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->certificate_name = xstrdup(argv[index]);
		}
		else if (strcmp("--no-fastpath", argv[index]) == 0)
		{
			settings->fastpath_input = false;
			settings->fastpath_output = false;
		}
		else if (strcmp("--gdi", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing GDI backend\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (strncmp("sw", argv[index], 1) == 0) /* software */
			{
				settings->sw_gdi = true;
			}
			else if (strncmp("hw", argv[index], 1) == 0) /* hardware */
			{
				settings->sw_gdi = false;
			}
			else
			{
				printf("unknown GDI backend\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--rfx", argv[index]) == 0)
		{
			settings->rfx_codec = true;
			settings->fastpath_output = true;
			settings->color_depth = 32;
			settings->frame_acknowledge = false;
			settings->performance_flags = PERF_FLAG_NONE;
			settings->large_pointer = true;
		}
		else if (strcmp("--rfx-mode", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing RemoteFX mode flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (argv[index][0] == 'v') /* video */
			{
				settings->rfx_codec_mode = 0x00;
			}
			else if (argv[index][0] == 'i') /* image */
			{
				settings->rfx_codec_mode = 0x02;
			}
			else
			{
				printf("unknown RemoteFX mode flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--nsc", argv[index]) == 0)
		{
			settings->ns_codec = true;
		}
		else if (strcmp("--dump-rfx", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing file name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->dump_rfx_file = xstrdup(argv[index]);
			settings->dump_rfx = true;
		}
		else if (strcmp("--play-rfx", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing file name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			settings->play_rfx_file = xstrdup(argv[index]);
			settings->play_rfx = true;
		}
		else if (strcmp("--fonts", argv[index]) == 0)
		{
			settings->smooth_fonts = true;
		}
		else if (strcmp("--disable-wallpaper", argv[index]) == 0)
		{
			settings->disable_wallpaper = true;
		}
		else if (strcmp("--disable-full-window-drag", argv[index]) == 0)
		{
			settings->disable_full_window_drag = true;
		}
		else if (strcmp("--disable-menu-animations", argv[index]) == 0)
		{
			settings->disable_menu_animations = true;
		}
		else if (strcmp("--disable-theming", argv[index]) == 0)
		{
			settings->disable_theming = true;
		}
		else if (strcmp("--composition", argv[index]) == 0)
		{
			settings->desktop_composition = true;
		}
		else if (strcmp("--no-motion", argv[index]) == 0)
		{
			settings->mouse_motion = false;
		}
		else if (strcmp("--app", argv[index]) == 0)
		{
			settings->remote_app = true;
			settings->rail_langbar_supported = true;
			settings->workarea = true;
			settings->performance_flags = PERF_DISABLE_WALLPAPER | PERF_DISABLE_FULLWINDOWDRAG;
		}
		else if (strcmp("-x", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing performance flag\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (argv[index][0] == 'm') /* modem */
			{
				settings->performance_flags = PERF_DISABLE_WALLPAPER |
					PERF_DISABLE_FULLWINDOWDRAG | PERF_DISABLE_MENUANIMATIONS |
					PERF_DISABLE_THEMING;

				settings->connection_type = CONNECTION_TYPE_MODEM;
			}
			else if (argv[index][0] == 'b') /* broadband */
			{
				settings->performance_flags = PERF_DISABLE_WALLPAPER;
				settings->connection_type = CONNECTION_TYPE_BROADBAND_HIGH;
			}
			else if (argv[index][0] == 'l') /* lan */
			{
				settings->performance_flags = PERF_FLAG_NONE;
				settings->connection_type = CONNECTION_TYPE_LAN;
			}
			else
			{
				settings->performance_flags = strtol(argv[index], 0, 16);
			}
		}
		else if (strcmp("-X", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing parent window XID\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}

			settings->parent_window_xid = strtoul(argv[index], NULL, 16);

			if (settings->parent_window_xid == 0)
			{
				printf("invalid parent window XID\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--no-rdp", argv[index]) == 0)
		{
			settings->rdp_security = false;
		}
		else if (strcmp("--no-tls", argv[index]) == 0)
		{
			settings->tls_security = false;
		}
		else if (strcmp("--no-nla", argv[index]) == 0)
		{
			settings->nla_security = false;
		}
		else if (strcmp("--sec", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing protocol security\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (strncmp("rdp", argv[index], 1) == 0) /* Standard RDP */
			{
				settings->rdp_security = true;
				settings->tls_security = false;
				settings->nla_security = false;
				settings->encryption = true;
				settings->encryption_method = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->encryption_level = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}
			else if (strncmp("tls", argv[index], 1) == 0) /* TLS */
			{
				settings->rdp_security = false;
				settings->tls_security = true;
				settings->nla_security = false;
			}
			else if (strncmp("nla", argv[index], 1) == 0) /* NLA */
			{
				settings->rdp_security = false;
				settings->tls_security = false;
				settings->nla_security = true;
			}
			else
			{
				printf("unknown protocol security\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--plugin", argv[index]) == 0)
		{
			index++;
			t = index;
			if (index == argc)
			{
				printf("missing plugin name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			plugin_data = NULL;
			if (index < argc - 1 && strcmp("--data", argv[index + 1]) == 0)
			{
				index += 2;
				i = 0;
				while (index < argc && strcmp("--", argv[index]) != 0)
				{
					if (plugin_data == NULL)
						plugin_data = (RDP_PLUGIN_DATA*) xmalloc(sizeof(RDP_PLUGIN_DATA) * (i + 2));
					else
						plugin_data = (RDP_PLUGIN_DATA*) xrealloc(plugin_data, sizeof(RDP_PLUGIN_DATA) * (i + 2));

					plugin_data[i].size = sizeof(RDP_PLUGIN_DATA);
					plugin_data[i].data[0] = NULL;
					plugin_data[i].data[1] = NULL;
					plugin_data[i].data[2] = NULL;
					plugin_data[i].data[3] = NULL;
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
					return FREERDP_ARGS_PARSE_FAILURE;
			}
		}
		else if (strcmp("--ext", argv[index]) == 0)
		{
			index++;
			if (index == argc)
			{
				printf("missing extension name\n");
				return FREERDP_ARGS_PARSE_FAILURE;
			}
			if (num_extensions >= sizeof(settings->extensions) / sizeof(struct rdp_ext_set))
			{
				printf("maximum extensions reached\n");
				return FREERDP_ARGS_PARSE_FAILURE;
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
		else if (strcmp("--secure-checksum", argv[index]) == 0)
		{
			settings->secure_checksum = true;
		}
		else if (strcmp("--version", argv[index]) == 0)
		{
			printf("This is FreeRDP version %s\n", FREERDP_VERSION_FULL);
			return FREERDP_ARGS_PARSE_VERSION;
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

			if (settings->smooth_fonts)
				settings->performance_flags |= PERF_ENABLE_FONT_SMOOTHING;

			if (settings->desktop_composition)
				settings->performance_flags |= PERF_ENABLE_DESKTOP_COMPOSITION;

			if (settings->disable_wallpaper)
				settings->performance_flags |= PERF_DISABLE_WALLPAPER;

			if (settings->disable_full_window_drag)
				settings->performance_flags |= PERF_DISABLE_FULLWINDOWDRAG;

			if (settings->disable_menu_animations)
				settings->performance_flags |= PERF_DISABLE_MENUANIMATIONS;

			if (settings->disable_theming)
				settings->performance_flags |= PERF_DISABLE_THEMING;

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
					return FREERDP_ARGS_PARSE_FAILURE;
				}
				index += t - 1;
			}
		}
		index++;
	}
	printf("missing server name\n");
	return FREERDP_ARGS_PARSE_FAILURE;
}
