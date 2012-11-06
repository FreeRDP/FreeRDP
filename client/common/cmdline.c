/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Command-Line Interface
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

#include <freerdp/client/cmdline.h>

COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "v", COMMAND_LINE_VALUE_REQUIRED, "<server>[:port]", NULL, NULL, -1, NULL, "destination server" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "server port" },
	{ "w", COMMAND_LINE_VALUE_REQUIRED, "<width>", "1024", NULL, -1, NULL, "width" },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, "<height>", "768", NULL, -1, NULL, "height" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "fullscreen" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, "<depth>", "16", NULL, -1, NULL, "session bpp (color depth)" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console", "admin (or console) session" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "multi-monitor" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "addin" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\]<user>", NULL, NULL, -1, NULL, "username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "domain" },
	{ "z", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "compression" },
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "audio output mode" },
	{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "audio input (microphone)" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "smooth fonts (cleartype)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "wallpaper" },
	{ "codec", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "codec" },
	{ "nego", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "protocol security negotiation" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED, "<rdp|tls|nla|ext>", NULL, NULL, -1, NULL, "force specific protocol security" },
	{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "rdp protocol security" },
	{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "tls protocol security" },
	{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "nla protocol security" },
	{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "nla extended protocol security" },
	{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, "<name>", NULL, NULL, -1, NULL, "certificate name" },
	{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "ignore certificate" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "print help" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

int freerdp_client_print_command_line_help()
{
	char* str;
	int length;
	COMMAND_LINE_ARGUMENT_A* arg;

	arg = args;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			printf("%4s", "/");
			printf("%-20s", arg->Name);
			printf("\t%s\n", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) || (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			printf("%4s", "/");

			if (arg->Format)
			{
				length = strlen(arg->Name) + strlen(arg->Format) + 2;
				str = malloc(length + 1);
				sprintf_s(str, length + 1, "%s:%s", arg->Name, arg->Format);
				printf("%-20s", str);
				free(str);
			}
			else
			{
				printf("%-20s", arg->Name);
			}

			printf("\t%s\n", arg->Text);
		}
		else if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			if (arg->Default == BoolValueTrue)
				printf("%4s", "-");
			else
				printf("%4s", "+");

			printf("%-20s", arg->Name);

			printf("\t%s\n", arg->Text);
		}
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

int freerdp_client_parse_command_line_arguments(int argc, char** argv, rdpSettings* settings)
{
	char* p;
	int length;
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;

	status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags);

	if (status == COMMAND_LINE_STATUS_PRINT_HELP)
	{
		freerdp_client_print_command_line_help();
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}

	arg = args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "v")
		{
			p = strchr(arg->Value, ':');

			if (p)
			{
				length = p - arg->Value;
				settings->port = atoi(&p[1]);
				settings->hostname = (char*) malloc(length + 1);
				strncpy(settings->hostname, arg->Value, length);
				settings->hostname[length] = '\0';
			}
			else
			{
				settings->hostname = _strdup(arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "port")
		{
			settings->port = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "w")
		{
			settings->width = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "h")
		{
			settings->height = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->fullscreen = TRUE;
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			settings->color_depth = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "u")
		{
			settings->username = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			settings->domain = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "p")
		{
			settings->password = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "z")
		{
			settings->compression = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "fonts")
		{
			settings->smooth_fonts = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "wallpaper")
		{
			settings->disable_wallpaper = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "window-drag")
		{
			settings->disable_full_window_drag = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "menu-anims")
		{
			settings->disable_menu_animations = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "themes")
		{
			settings->disable_theming = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "aero")
		{
			settings->desktop_composition = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "nego")
		{
			settings->security_layer_negotiation = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			settings->send_preconnection_pdu = TRUE;
			settings->preconnection_id = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			settings->send_preconnection_pdu = TRUE;
			settings->preconnection_blob = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strcmp("rdp", arg->Value) == 0) /* Standard RDP */
			{
				settings->rdp_security = TRUE;
				settings->tls_security = FALSE;
				settings->nla_security = FALSE;
				settings->ext_security = FALSE;
				settings->encryption = TRUE;
				settings->encryption_method = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->encryption_level = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}
			else if (strcmp("tls", arg->Value) == 0) /* TLS */
			{
				settings->rdp_security = FALSE;
				settings->tls_security = TRUE;
				settings->nla_security = FALSE;
				settings->ext_security = FALSE;
			}
			else if (strcmp("nla", arg->Value) == 0) /* NLA */
			{
				settings->rdp_security = FALSE;
				settings->tls_security = FALSE;
				settings->nla_security = TRUE;
				settings->ext_security = FALSE;
			}
			else if (strcmp("ext", arg->Value) == 0) /* NLA Extended */
			{
				settings->rdp_security = FALSE;
				settings->tls_security = FALSE;
				settings->nla_security = FALSE;
				settings->ext_security = TRUE;
			}
			else
			{
				printf("unknown protocol security: %s\n", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "sec-rdp")
		{
			settings->rdp_security = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			settings->tls_security = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			settings->nla_security = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			settings->ext_security = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "cert-name")
		{
			settings->certificate_name = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			settings->ignore_certificate = TRUE;
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	settings->performance_flags = PERF_FLAG_NONE;

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

	return 1;
}
