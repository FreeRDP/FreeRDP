/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Compatibility
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>

#include <freerdp/locale/keyboard.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/log.h>

#include "compatibility.h"

#define TAG CLIENT_TAG("common.compatibility")

COMMAND_LINE_ARGUMENT_A old_args[] =
{
	{ "0", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "connect to console session" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "set color depth in bits, default is 16" },
	{ "c", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "shell working directory" },
	{ "D", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "hide window decorations" },
	{ "T", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Window title" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "domain" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "fullscreen mode" },
	{ "g", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "set geometry, using format WxH or X%% or 'workarea', default is 1024x768" },
	{ "h", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "help", "print this help" },
	{ "k", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "set keyboard layout ID" },
	{ "K", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "do not interfere with window manager bindings" },
	{ "n", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "hostname" },
	{ "o", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "console audio" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "password" },
	{ "s", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "set startup-shell" },
	{ "t", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "alternative port number, default is 3389" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "username" },
	{ "x", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "performance flags (m[odem], b[roadband] or l[an])" },
	{ "X", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "embed into another window with a given XID." },
	{ "z", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "enable compression" },
	{ "app", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "RemoteApp connection. This implies -g workarea" },
	{ "ext", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "load an extension" },
	{ "no-auth", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable authentication" },
	{ "authonly", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "authentication only, no UI" },
	{ "from-stdin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "unspecified username, password, domain and hostname params are prompted" },
	{ "no-fastpath", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable fast-path" },
	{ "no-motion", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "don't send mouse motion events" },
	{ "gdi", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "graphics rendering (hw, sw)" },
	{ "no-osb", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable offscreen bitmaps" },
	{ "no-bmp-cache", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable bitmap cache" },
	{ "plugin", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "load a virtual channel plugin" },
	{ "rfx", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "enable RemoteFX" },
	{ "rfx-mode", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "RemoteFX operational flags (v[ideo], i[mage]), default is video" },
	{ "nsc", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "enable NSCodec (experimental)" },
	{ "disable-wallpaper", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disables wallpaper" },
	{ "composition", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "enable desktop composition" },
	{ "disable-full-window-drag", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disables full window drag" },
	{ "disable-menu-animations", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disables menu animations" },
	{ "disable-theming", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disables theming" },
	{ "no-rdp", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable Standard RDP encryption" },
	{ "no-tls", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable TLS encryption" },
	{ "no-nla", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "disable network level authentication" },
	{ "ntlm", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "force NTLM authentication protocol version (1 or 2)" },
	{ "ignore-certificate", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "ignore verification of logon certificate" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "force protocol security (rdp, tls or nla)" },
	{ "secure-checksum", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "use salted checksums with Standard RDP encryption" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "print version information" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

BOOL freerdp_client_old_parse_hostname(char* str, char** ServerHostname, UINT32* ServerPort)
{
	char* p;
	char* host = NULL;

	if (str[0] == '[' && (p = strchr(str, ']'))
	    && (p[1] == 0 || (p[1] == ':' && !strchr(p + 2, ':'))))
	{
		/* Either "[...]" or "[...]:..." with at most one : after the brackets */
		if (!(host = _strdup(str + 1)))
			return FALSE;

		if ((p = strchr(host, ']')))
		{
			*p = 0;

			if (p[1] == ':')
			{
				unsigned long val;
				errno = 0;
				val = strtoul(p + 2, NULL, 0);

				if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
				{
					free(host);
					return FALSE;
				}

				*ServerPort = val;
			}
		}
	}
	else
	{
		/* Port number is cut off and used if exactly one : in the string */
		if (!(host = _strdup(str)))
			return FALSE;

		if ((p = strchr(host, ':')) && !strchr(p + 1, ':'))
		{
			unsigned long val;
			errno = 0;
			val = strtoul(p + 1, NULL, 0);

			if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
			{
				free(host);
				return FALSE;
			}

			*p = 0;
			*ServerPort = val;
		}
	}

	*ServerHostname = host;
	return TRUE;
}

int freerdp_client_old_process_plugin(rdpSettings* settings, ADDIN_ARGV* args)
{
	int args_handled = 0;

	if (strcmp(args->argv[0], "cliprdr") == 0)
	{
		args_handled++;
		settings->RedirectClipboard = TRUE;
		WLog_WARN(TAG,  "--plugin cliprdr -> +clipboard");
	}
	else if (strcmp(args->argv[0], "rdpdr") == 0)
	{
		args_handled++;

		if (args->argc < 2)
			return 1;

		args_handled++;

		if ((strcmp(args->argv[1], "disk") == 0) ||
		    (strcmp(args->argv[1], "drive") == 0))
		{
			freerdp_addin_replace_argument(args, "disk", "drive");
			freerdp_client_add_device_channel(settings, args->argc - 1, &args->argv[1]);
		}
		else if (strcmp(args->argv[1], "printer") == 0)
		{
			freerdp_client_add_device_channel(settings, args->argc - 1, &args->argv[1]);
		}
		else if ((strcmp(args->argv[1], "scard") == 0) ||
		         (strcmp(args->argv[1], "smartcard") == 0))
		{
			freerdp_addin_replace_argument(args, "scard", "smartcard");
			freerdp_client_add_device_channel(settings, args->argc - 1, &args->argv[1]);
		}
		else if (strcmp(args->argv[1], "serial") == 0)
		{
			freerdp_client_add_device_channel(settings, args->argc - 1, &args->argv[1]);
		}
		else if (strcmp(args->argv[1], "parallel") == 0)
		{
			freerdp_client_add_device_channel(settings, args->argc - 1, &args->argv[1]);
		}
	}
	else if (strcmp(args->argv[0], "drdynvc") == 0)
	{
		args_handled++;
		freerdp_client_add_dynamic_channel(settings, args->argc - 1, &args->argv[1]);
	}
	else if (strcmp(args->argv[0], "rdpsnd") == 0)
	{
		args_handled++;

		if (args->argc < 2)
			return 1;

		args_handled++;
		freerdp_addin_replace_argument_value(args, args->argv[1], "sys", args->argv[1]);
		freerdp_client_add_static_channel(settings, args->argc, args->argv);
	}
	else if (strcmp(args->argv[0], "rail") == 0)
	{
		args_handled++;

		if (args->argc < 2)
			return 1;

		args_handled++;

		if (!(settings->RemoteApplicationProgram = _strdup(args->argv[1])))
			return -1;
	}
	else
	{
		freerdp_client_add_static_channel(settings, args->argc, args->argv);
	}

	return args_handled;
}
int freerdp_client_old_command_line_pre_filter(void* context, int index, int argc, LPCSTR* argv)
{
	rdpSettings* settings = (rdpSettings*) context;

	if (index == (argc - 1))
	{
		if (argv[index][0] != '-')
		{
			if ((strcmp(argv[index - 1], "-v") == 0) ||
			    (strcmp(argv[index - 1], "/v") == 0))
			{
				return -1;
			}

			if (_stricmp(&(argv[index])[strlen(argv[index]) - 4], ".rdp") == 0)
			{
				return -1;
			}

			if (!freerdp_client_old_parse_hostname((char*) argv[index],
			                                       &settings->ServerHostname, &settings->ServerPort))
				return -1;

			return 2;
		}
		else
		{
			return -1;
		}
	}

	if (strcmp("--plugin", argv[index]) == 0)
	{
		int args_handled = 0;
		int length;
		char* a, *p;
		int i, j, t;
		int old_index;
		ADDIN_ARGV* args;
		old_index = index;
		index++;
		t = index;

		if (index == argc)
			return -1;

		args = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));

		if (!args)
			return -1;

		args->argv = (char**) calloc(argc, sizeof(char*));

		if (!args->argv)
		{
			free(args);
			return -1;
		}

		args->argc = 1;

		if ((index < argc - 1) && strcmp("--data", argv[index + 1]) == 0)
		{
			i = 0;
			index += 2;

			while ((index < argc) && (strcmp("--", argv[index]) != 0))
			{
				args_handled++;
				args->argc = 1;

				if (!(args->argv[0] = _strdup(argv[t])))
				{
					free(args->argv);
					free(args);
					return -1;
				}

				for (j = 0, p = (char*) argv[index]; (j < 4) && (p != NULL); j++)
				{
					if (*p == '\'')
					{
						a = p + 1;
						p = strchr(p + 1, '\'');

						if (p)
							*p++ = 0;
					}
					else
					{
						a = p;
					}

					if (p != NULL)
					{
						p = strchr(p, ':');
					}

					if (p != NULL)
					{
						length = (int)(p - a);

						if (!(args->argv[j + 1] = (char*) malloc(length + 1)))
						{
							for (; j >= 0; --j)
								free(args->argv[j]);

							free(args->argv);
							free(args);
							return -1;
						}

						CopyMemory(args->argv[j + 1], a, length);
						args->argv[j + 1][length] = '\0';
						p++;
					}
					else
					{
						if (!(args->argv[j + 1] = _strdup(a)))
						{
							for (; j >= 0; --j)
								free(args->argv[j]);

							free(args->argv);
							free(args);
							return -1;
						}
					}

					args->argc++;
				}

				if (settings)
				{
					freerdp_client_old_process_plugin(settings, args);
				}

				for (j = 0; j < args->argc; j++)
					free(args->argv[j]);

				memset(args->argv, 0, argc * sizeof(char*));
				index++;
				i++;
			}
		}
		else
		{
			if (settings)
			{
				if (!(args->argv[0] = _strdup(argv[t])))
				{
					free(args->argv);
					free(args);
					return -1;
				}

				args_handled = freerdp_client_old_process_plugin(settings, args);
				free(args->argv[0]);
			}
		}

		free(args->argv);
		free(args);
		return (index - old_index) + args_handled;
	}

	return 0;
}
int freerdp_client_old_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	return 0;
}
int freerdp_detect_old_command_line_syntax(int argc, char** argv, int* count)
{
	int status;
	DWORD flags;
	int detect_status;
	rdpSettings* settings;
	COMMAND_LINE_ARGUMENT_A* arg;
	*count = 0;
	detect_status = 0;
	flags = COMMAND_LINE_SEPARATOR_SPACE;
	flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	flags |= COMMAND_LINE_SIGIL_NOT_ESCAPED;
	settings = (rdpSettings*) calloc(1, sizeof(rdpSettings));

	if (!settings)
		return -1;

	CommandLineClearArgumentsA(old_args);
	status = CommandLineParseArgumentsA(argc, (const char**) argv, old_args, flags, settings,
	                                    freerdp_client_old_command_line_pre_filter, NULL);

	if (status < 0)
	{
		free(settings);
		return status;
	}

	arg = old_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "a")
		{
			if ((strcmp(arg->Value, "8") == 0) ||
			    (strcmp(arg->Value, "15") == 0) || (strcmp(arg->Value, "16") == 0) ||
			    (strcmp(arg->Value, "24") == 0) || (strcmp(arg->Value, "32") == 0))
			{
				detect_status = 1;
			}
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
		(*count)++;
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((status <= COMMAND_LINE_ERROR) && (status >= COMMAND_LINE_ERROR_LAST))
		detect_status = -1;

	if (detect_status == 0)
	{
		if (settings->ServerHostname)
			detect_status = 1;
	}

	free(settings->ServerHostname);
	free(settings);
	return detect_status;
}
int freerdp_client_parse_old_command_line_arguments(int argc, char** argv, rdpSettings* settings)
{
	char* p;
	char* str;
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
	flags = COMMAND_LINE_SEPARATOR_SPACE;
	flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;
	flags |= COMMAND_LINE_SIGIL_NOT_ESCAPED;
	status = CommandLineParseArgumentsA(argc, (const char**) argv, old_args, flags, settings,
	                                    freerdp_client_old_command_line_pre_filter, freerdp_client_old_command_line_post_filter);

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		return COMMAND_LINE_STATUS_PRINT;
	}
	else if (status < 0)
	{
		if (status != COMMAND_LINE_STATUS_PRINT_HELP)
		{
		}

		freerdp_client_print_command_line_help(argc, argv);
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	arg = old_args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "0")
		{
			settings->ConsoleSession = TRUE;
			WLog_WARN(TAG,  "-0 -> /admin");
		}
		CommandLineSwitchCase(arg, "a")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > INT8_MAX))
				return FALSE;

			settings->ColorDepth = val;
			WLog_WARN(TAG,  "-a %s -> /bpp:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "c")
		{
			WLog_WARN(TAG,  "-c %s -> /shell-dir:%s", arg->Value, arg->Value);

			if (!(settings->ShellWorkingDirectory = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "D")
		{
			settings->Decorations = FALSE;
			WLog_WARN(TAG,  "-D -> -decorations");
		}
		CommandLineSwitchCase(arg, "T")
		{
			if (!(settings->WindowTitle = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-T %s -> /title:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			if (!(settings->Domain = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-d %s -> /d:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->Fullscreen = TRUE;
			WLog_WARN(TAG,  "-f -> /f");
		}
		CommandLineSwitchCase(arg, "g")
		{
			if (!(str = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			p = strchr(str, 'x');

			if (p)
			{
				unsigned long h, w = strtoul(str, NULL, 0);

				if ((errno != 0) || (w == 0) || (w > UINT16_MAX))
				{
					free(str);
					return FALSE;
				}

				h = strtoul(&p[1], NULL, 0);

				if ((errno != 0) || (h == 0) || (h > UINT16_MAX))
				{
					free(str);
					return FALSE;
				}

				*p = '\0';
				settings->DesktopWidth = w;
				settings->DesktopHeight = h;
			}

			free(str);
			WLog_WARN(TAG,  "-g %s -> /size:%s or /w:%"PRIu32" /h:%"PRIu32"", arg->Value, arg->Value,
			          settings->DesktopWidth, settings->DesktopHeight);
		}
		CommandLineSwitchCase(arg, "k")
		{
			sscanf(arg->Value, "%X", &(settings->KeyboardLayout));
			WLog_WARN(TAG,  "-k %s -> /kbd:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "K")
		{
			settings->GrabKeyboard = FALSE;
			WLog_WARN(TAG,  "-K -> -grab-keyboard");
		}
		CommandLineSwitchCase(arg, "n")
		{
			if (!(settings->ClientHostname = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-n -> /client-hostname:%s", arg->Value);
		}
		CommandLineSwitchCase(arg, "o")
		{
			settings->RemoteConsoleAudio = TRUE;
			WLog_WARN(TAG,  "-o -> /audio-mode:1");
		}
		CommandLineSwitchCase(arg, "p")
		{
			if (!(settings->Password = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-p ****** -> /p:******");
			/* Hide the value from 'ps'. */
			FillMemory(arg->Value, strlen(arg->Value), '*');
		}
		CommandLineSwitchCase(arg, "s")
		{
			if (!(settings->AlternateShell = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-s %s -> /shell:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "t")
		{
			unsigned long p = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (p == 0) || (p > UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->ServerPort = p;
			WLog_WARN(TAG,  "-t %s -> /port:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "u")
		{
			if (!(settings->Username = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			WLog_WARN(TAG,  "-u %s -> /u:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "x")
		{
			long type;
			char* pEnd;
			type = strtol(arg->Value, &pEnd, 16);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (type == 0)
			{
				type = CONNECTION_TYPE_LAN;

				if (_stricmp(arg->Value, "m") == 0)
					type = CONNECTION_TYPE_MODEM;
				else if (_stricmp(arg->Value, "b") == 0)
					type = CONNECTION_TYPE_BROADBAND_HIGH;
				else if (_stricmp(arg->Value, "l") == 0)
					type = CONNECTION_TYPE_LAN;

				freerdp_set_connection_type(settings, type);
			}
			else
			{
				settings->PerformanceFlags = type;
				freerdp_performance_flags_split(settings);
			}

			WLog_WARN(TAG,  "-x %s -> /network:", arg->Value);

			if (type == CONNECTION_TYPE_MODEM)
				WLog_WARN(TAG,  "modem");
			else if (CONNECTION_TYPE_BROADBAND_HIGH)
				WLog_WARN(TAG,  "broadband");
			else if (CONNECTION_TYPE_LAN)
				WLog_WARN(TAG,  "lan");

			WLog_WARN(TAG,  "");
		}
		CommandLineSwitchCase(arg, "X")
		{
			settings->ParentWindowId = _strtoui64(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			WLog_WARN(TAG,  "-X %s -> /parent-window:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "z")
		{
			settings->CompressionEnabled = TRUE;
			WLog_WARN(TAG,  "-z -> /compression");
		}
		CommandLineSwitchCase(arg, "app")
		{
			settings->RemoteApplicationMode = TRUE;
			WLog_WARN(TAG,  "--app -> /app: + program name or alias");
		}
		CommandLineSwitchCase(arg, "ext")
		{
		}
		CommandLineSwitchCase(arg, "no-auth")
		{
			settings->Authentication = FALSE;
			WLog_WARN(TAG,  "--no-auth -> -authentication");
		}
		CommandLineSwitchCase(arg, "authonly")
		{
			settings->AuthenticationOnly = TRUE;
		}
		CommandLineSwitchCase(arg, "from-stdin")
		{
			settings->CredentialsFromStdin = TRUE;
		}
		CommandLineSwitchCase(arg, "no-fastpath")
		{
			settings->FastPathInput = FALSE;
			settings->FastPathOutput = FALSE;
			WLog_WARN(TAG,  "--no-fastpath -> -fast-path");
		}
		CommandLineSwitchCase(arg, "no-motion")
		{
			settings->MouseMotion = FALSE;
			WLog_WARN(TAG,  "--no-motion -> -mouse-motion");
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (strcmp(arg->Value, "sw") == 0)
				settings->SoftwareGdi = TRUE;
			else if (strcmp(arg->Value, "hw") == 0)
				settings->SoftwareGdi = FALSE;

			WLog_WARN(TAG,  "--gdi %s -> /gdi:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "no-osb")
		{
			settings->OffscreenSupportLevel = FALSE;
			WLog_WARN(TAG,  "--no-osb -> -offscreen-cache");
		}
		CommandLineSwitchCase(arg, "no-bmp-cache")
		{
			settings->BitmapCacheEnabled = FALSE;
			WLog_WARN(TAG,  "--no-bmp-cache -> -bitmap-cache");
		}
		CommandLineSwitchCase(arg, "plugin")
		{
			WLog_WARN(TAG,  "--plugin -> /a, /vc, /dvc and channel-specific options");
		}
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = TRUE;
			WLog_WARN(TAG,  "--rfx -> /rfx");
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (arg->Value[0] == 'v')
				settings->RemoteFxCodecMode = 0x00;
			else if (arg->Value[0] == 'i')
				settings->RemoteFxCodecMode = 0x02;

			WLog_WARN(TAG,  "--rfx-mode -> /rfx-mode:%s", settings->RemoteFxCodecMode ? "image" : "video");
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			settings->NSCodec = TRUE;
			WLog_WARN(TAG,  "--nsc -> /nsc");
		}
		CommandLineSwitchCase(arg, "disable-wallpaper")
		{
			settings->DisableWallpaper = TRUE;
			WLog_WARN(TAG,  "--disable-wallpaper -> -wallpaper");
		}
		CommandLineSwitchCase(arg, "composition")
		{
			settings->AllowDesktopComposition = TRUE;
			WLog_WARN(TAG,  "--composition -> +composition");
		}
		CommandLineSwitchCase(arg, "disable-full-window-drag")
		{
			settings->DisableFullWindowDrag = TRUE;
			WLog_WARN(TAG,  "--disable-full-window-drag -> -window-drag");
		}
		CommandLineSwitchCase(arg, "disable-menu-animations")
		{
			settings->DisableMenuAnims = TRUE;
			WLog_WARN(TAG,  "--disable-menu-animations -> -menu-anims");
		}
		CommandLineSwitchCase(arg, "disable-theming")
		{
			settings->DisableThemes = TRUE;
			WLog_WARN(TAG,  "--disable-theming -> -themes");
		}
		CommandLineSwitchCase(arg, "ntlm")
		{
		}
		CommandLineSwitchCase(arg, "ignore-certificate")
		{
			settings->IgnoreCertificate = TRUE;
			WLog_WARN(TAG,  "--ignore-certificate -> /cert-ignore");
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strncmp("rdp", arg->Value, 1) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->UseRdpSecurityLayer = FALSE;
			}
			else if (strncmp("tls", arg->Value, 1) == 0) /* TLS */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = TRUE;
				settings->NlaSecurity = FALSE;
			}
			else if (strncmp("nla", arg->Value, 1) == 0) /* NLA */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = TRUE;
			}

			WLog_WARN(TAG,  "--sec %s -> /sec:%s", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "no-rdp")
		{
			settings->RdpSecurity = FALSE;
			WLog_WARN(TAG,  "--no-rdp -> -sec-rdp");
		}
		CommandLineSwitchCase(arg, "no-tls")
		{
			settings->TlsSecurity = FALSE;
			WLog_WARN(TAG,  "--no-tls -> -sec-tls");
		}
		CommandLineSwitchCase(arg, "no-nla")
		{
			settings->NlaSecurity = FALSE;
			WLog_WARN(TAG,  "--no-nla -> -sec-nla");
		}
		CommandLineSwitchCase(arg, "secure-checksum")
		{
			settings->SaltedChecksum = TRUE;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	WLog_WARN(TAG,  "%s -> /v:%s", settings->ServerHostname, settings->ServerHostname);

	if (settings->ServerPort != 3389)
		WLog_WARN(TAG,  " /port:%"PRIu32"", settings->ServerPort);

	WLog_WARN(TAG,  "");
	return 1;
}
