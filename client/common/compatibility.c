/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Compatibility
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>
#include <freerdp/locale/keyboard.h>

#include <freerdp/client/cmdline.h>

#include "compatibility.h"

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

void freerdp_client_old_parse_hostname(char* str, char** ServerHostname, UINT32* ServerPort)
{
	char* p;

	if (str[0] == '[' && (p = strchr(str, ']'))
			&& (p[1] == 0 || (p[1] == ':' && !strchr(p + 2, ':'))))
	{
		/* Either "[...]" or "[...]:..." with at most one : after the brackets */
		*ServerHostname = _strdup(str + 1);

		if ((p = strchr((char*) *ServerHostname, ']')))
		{
			*p = 0;

			if (p[1] == ':')
				*ServerPort = atoi(p + 2);
		}
	}
	else
	{
		/* Port number is cut off and used if exactly one : in the string */
		*ServerHostname = _strdup(str);

		if ((p = strchr((char*) *ServerHostname, ':')) && !strchr(p + 1, ':'))
		{
			*p = 0;
			*ServerPort = atoi(p + 1);
		}
	}
}

int freerdp_client_old_process_plugin(rdpSettings* settings, ADDIN_ARGV* args)
{
	if (strcmp(args->argv[0], "cliprdr") == 0)
	{
		settings->RedirectClipboard = TRUE;
		fprintf(stderr, "--plugin cliprdr -> +clipboard\n");
	}
	else if (strcmp(args->argv[0], "rdpdr") == 0)
	{
		if (args->argc < 2)
			return -1;

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
		freerdp_client_add_dynamic_channel(settings, args->argc - 1, &args->argv[1]);
	}
	else if (strcmp(args->argv[0], "rdpsnd") == 0)
	{
		if (args->argc < 2)
			return -1;

		freerdp_addin_replace_argument_value(args, args->argv[1], "sys", args->argv[1]);
		freerdp_client_add_static_channel(settings, args->argc, args->argv);
	}
	else if (strcmp(args->argv[0], "rail") == 0)
	{
		if (args->argc < 2)
			return -1;

		settings->RemoteApplicationProgram = _strdup(args->argv[1]);
	}
	else
	{
		freerdp_client_add_static_channel(settings, args->argc, args->argv);
	}

	return 1;
}

int freerdp_client_old_command_line_pre_filter(void* context, int index, int argc, LPCSTR* argv)
{
	rdpSettings* settings;

	settings = (rdpSettings*) context;

	if (index == (argc - 1))
	{
		if (argv[index][0] != '-')
		{
			if ((strcmp(argv[index - 1], "-v") == 0) ||
					(strcmp(argv[index - 1], "/v") == 0))
			{
				return -1;
			}
			if (_stricmp(&(argv[index])[strlen(argv[index])- 4], ".rdp") == 0)
			{
				return -1;
			}
			freerdp_client_old_parse_hostname((char*) argv[index], &settings->ServerHostname, &settings->ServerPort);
		}
		else
		{
			return -1;
		}
	}

	if (strcmp("--plugin", argv[index]) == 0)
	{
		int length;
		char *a, *p;
		int i, j, t;
		int old_index;
		ADDIN_ARGV* args;

		old_index = index;

		index++;
		t = index;

		if (index == argc)
			return -1;

		args = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));
		args->argv = (char**) malloc(sizeof(char*) * 5);
		args->argc = 1;

		args->argv[0] = _strdup(argv[t]);

		if ((index < argc - 1) && strcmp("--data", argv[index + 1]) == 0)
		{
			i = 0;
			index += 2;
			args->argc = 1;

			while ((index < argc) && (strcmp("--", argv[index]) != 0))
			{
				args->argc = 1;

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
						length = p - a;
						args->argv[j + 1] = malloc(length + 1);
						CopyMemory(args->argv[j + 1], a, length);
						args->argv[j + 1][length] = '\0';
						p++;
					}
					else
					{
						args->argv[j + 1] = _strdup(a);
					}

					args->argc++;
				}

				if (settings->instance)
				{
					freerdp_client_old_process_plugin(settings, args);
				}

				index++;
				i++;
			}
		} else {
				if (settings->instance)
				{
					freerdp_client_old_process_plugin(settings, args);
				}
		}

		for (i=0; i<args->argc; i++)
			free(args->argv[i]);
		free(args->argv);
		free(args);

		return (index - old_index);
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

	settings = (rdpSettings*) malloc(sizeof(rdpSettings));
	ZeroMemory(settings, sizeof(rdpSettings));

	CommandLineClearArgumentsA(old_args);
	status = CommandLineParseArgumentsA(argc, (const char**) argv, old_args, flags, settings,
			freerdp_client_old_command_line_pre_filter, NULL);
	if (status < 0)
		return status;

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

	if (settings->ServerHostname)
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

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "0")
		{
			settings->ConsoleSession = TRUE;
			fprintf(stderr, "-0 -> /admin\n");
		}
		CommandLineSwitchCase(arg, "a")
		{
			settings->ColorDepth = atoi(arg->Value);
			fprintf(stderr, "-a %s -> /bpp:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "c")
		{
			settings->ShellWorkingDirectory = _strdup(arg->Value);
			fprintf(stderr, "-c %s -> /shell-dir:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "D")
		{
			settings->Decorations = FALSE;
			fprintf(stderr, "-D -> -decorations\n");
		}
		CommandLineSwitchCase(arg, "T")
		{
			settings->WindowTitle = _strdup(arg->Value);
			fprintf(stderr, "-T %s -> /title:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			settings->Domain = _strdup(arg->Value);
			fprintf(stderr, "-d %s -> /d:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->Fullscreen = TRUE;
			fprintf(stderr, "-f -> /f\n");
		}
		CommandLineSwitchCase(arg, "g")
		{
			str = _strdup(arg->Value);

			p = strchr(str, 'x');

			if (p)
			{
				*p = '\0';
				settings->DesktopWidth = atoi(str);
				settings->DesktopHeight = atoi(&p[1]);
			}

			free(str);

			fprintf(stderr, "-g %s -> /size:%s or /w:%d /h:%d\n", arg->Value, arg->Value,
					settings->DesktopWidth, settings->DesktopHeight);
		}
		CommandLineSwitchCase(arg, "k")
		{
			sscanf(arg->Value, "%X", &(settings->KeyboardLayout));
			fprintf(stderr, "-k %s -> /kbd:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "K")
		{
			settings->GrabKeyboard = FALSE;
			fprintf(stderr, "-K -> -grab-keyboard\n");
		}
		CommandLineSwitchCase(arg, "n")
		{
			settings->ClientHostname = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "o")
		{
			settings->RemoteConsoleAudio = TRUE;
			fprintf(stderr, "-o -> /audio-mode:1\n");
		}
		CommandLineSwitchCase(arg, "p")
		{
			settings->Password = _strdup(arg->Value);
			fprintf(stderr, "-p ****** -> /p:******\n");
			/* Hide the value from 'ps'. */
			FillMemory(arg->Value, strlen(arg->Value), '*');
		}
		CommandLineSwitchCase(arg, "s")
		{
			settings->AlternateShell = _strdup(arg->Value);
			fprintf(stderr, "-s %s -> /shell:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "t")
		{
			settings->ServerPort = atoi(arg->Value);
			fprintf(stderr, "-t %s -> /port:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "u")
		{
			settings->Username = _strdup(arg->Value);
			fprintf(stderr, "-u %s -> /u:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "x")
		{
			int type;
			char* pEnd;

			type = strtol(arg->Value, &pEnd, 16);

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

			fprintf(stderr, "-x %s -> /network:", arg->Value);

			if (type == CONNECTION_TYPE_MODEM)
				fprintf(stderr, "modem");
			else if (CONNECTION_TYPE_BROADBAND_HIGH)
				fprintf(stderr, "broadband");
			else if (CONNECTION_TYPE_LAN)
				fprintf(stderr, "lan");

			fprintf(stderr, "\n");
		}
		CommandLineSwitchCase(arg, "X")
		{
			settings->ParentWindowId = strtol(arg->Value, NULL, 0);
			fprintf(stderr, "-X %s -> /parent-window:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "z")
		{
			settings->CompressionEnabled = TRUE;
			fprintf(stderr, "-z -> /compression\n");
		}
		CommandLineSwitchCase(arg, "app")
		{
			settings->RemoteApplicationMode = TRUE;
			fprintf(stderr, "--app -> /app: + program name or alias\n");
		}
		CommandLineSwitchCase(arg, "ext")
		{

		}
		CommandLineSwitchCase(arg, "no-auth")
		{
			settings->Authentication = FALSE;
			fprintf(stderr, "--no-auth -> -authentication\n");
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
			fprintf(stderr, "--no-fastpath -> -fast-path\n");
		}
		CommandLineSwitchCase(arg, "no-motion")
		{
			settings->MouseMotion = FALSE;
			fprintf(stderr, "--no-motion -> -mouse-motion\n");
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (strcmp(arg->Value, "sw") == 0)
				settings->SoftwareGdi = TRUE;
			else if (strcmp(arg->Value, "hw") == 0)
				settings->SoftwareGdi = FALSE;

			fprintf(stderr, "--gdi %s -> /gdi:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "no-osb")
		{
			settings->OffscreenSupportLevel = FALSE;
			fprintf(stderr, "--no-osb -> -offscreen-cache\n");
		}
		CommandLineSwitchCase(arg, "no-bmp-cache")
		{
			settings->BitmapCacheEnabled = FALSE;
			fprintf(stderr, "--no-bmp-cache -> -bitmap-cache\n");
		}
		CommandLineSwitchCase(arg, "plugin")
		{
			fprintf(stderr, "--plugin -> /a, /vc, /dvc and channel-specific options\n");
		}
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = TRUE;
			fprintf(stderr, "--rfx -> /rfx\n");
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (arg->Value[0] == 'v')
				settings->RemoteFxCodecMode = 0x00;
			else if (arg->Value[0] == 'i')
				settings->RemoteFxCodecMode = 0x02;

			fprintf(stderr, "--rfx-mode -> /rfx-mode:%s\n", settings->RemoteFxCodecMode ? "image" : "video");
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			settings->NSCodec = TRUE;
			fprintf(stderr, "--nsc -> /nsc\n");
		}
		CommandLineSwitchCase(arg, "disable-wallpaper")
		{
			settings->DisableWallpaper = TRUE;
			fprintf(stderr, "--disable-wallpaper -> -wallpaper\n");
		}
		CommandLineSwitchCase(arg, "composition")
		{
			settings->AllowDesktopComposition = TRUE;
			fprintf(stderr, "--composition -> +composition\n");
		}
		CommandLineSwitchCase(arg, "disable-full-window-drag")
		{
			settings->DisableFullWindowDrag = TRUE;
			fprintf(stderr, "--disable-full-window-drag -> -window-drag\n");
		}
		CommandLineSwitchCase(arg, "disable-menu-animations")
		{
			settings->DisableMenuAnims = TRUE;
			fprintf(stderr, "--disable-menu-animations -> -menu-anims\n");
		}
		CommandLineSwitchCase(arg, "disable-theming")
		{
			settings->DisableThemes = TRUE;
			fprintf(stderr, "--disable-theming -> -themes\n");
		}
		CommandLineSwitchCase(arg, "ntlm")
		{

		}
		CommandLineSwitchCase(arg, "ignore-certificate")
		{
			settings->IgnoreCertificate = TRUE;
			fprintf(stderr, "--ignore-certificate -> /cert-ignore\n");
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strncmp("rdp", arg->Value, 1) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->DisableEncryption = FALSE;
				settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
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

			fprintf(stderr, "--sec %s -> /sec:%s\n", arg->Value, arg->Value);
		}
		CommandLineSwitchCase(arg, "no-rdp")
		{
			settings->RdpSecurity = FALSE;
			fprintf(stderr, "--no-rdp -> -sec-rdp\n");
		}
		CommandLineSwitchCase(arg, "no-tls")
		{
			settings->TlsSecurity = FALSE;
			fprintf(stderr, "--no-tls -> -sec-tls\n");
		}
		CommandLineSwitchCase(arg, "no-nla")
		{
			settings->NlaSecurity = FALSE;
			fprintf(stderr, "--no-nla -> -sec-nla\n");
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

	fprintf(stderr, "%s -> /v:%s", settings->ServerHostname, settings->ServerHostname);

	if (settings->ServerPort != 3389)
		fprintf(stderr, " /port:%d", settings->ServerPort);

	fprintf(stderr, "\n");

	return 1;
}
