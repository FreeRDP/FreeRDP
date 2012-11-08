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
	{ "size", COMMAND_LINE_VALUE_REQUIRED, "<width>x<height>", "1024x768", NULL, -1, NULL, "screen size" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "fullscreen" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, "<depth>", "16", NULL, -1, NULL, "session bpp (color depth)" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console", "admin (or console) session" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "multi-monitor" },
	{ "workarea", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "work area" },
	{ "t", COMMAND_LINE_VALUE_REQUIRED, "<title>", NULL, NULL, -1, "title", "window title" },
	{ "decorations", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "window decorations" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "addin" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\|@]<user>", NULL, NULL, -1, NULL, "username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "domain" },
	{ "g", COMMAND_LINE_VALUE_REQUIRED, "<gateway>[:port]", NULL, NULL, -1, NULL, "gateway" },
	{ "gu", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\|@]<user>", NULL, NULL, -1, NULL, "gateway username" },
	{ "gp", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "gateway password" },
	{ "gd", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "gateway domain" },
	{ "z", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "compression" },
	{ "shell", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "alternate shell" },
	{ "shell-dir", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "shell working directory" },
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "audio output mode" },
	{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "audio input (microphone)" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "smooth fonts (cleartype)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "wallpaper" },
	{ "gdi", COMMAND_LINE_VALUE_REQUIRED, "<sw|hw>", NULL, NULL, -1, NULL, "GDI rendering" },
	{ "rfx", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "RemoteFX" },
	{ "rfx-mode", COMMAND_LINE_VALUE_REQUIRED, "<image|video>", NULL, NULL, -1, NULL, "RemoteFX mode" },
	{ "frame-ack", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Frame acknowledgement" },
	{ "nsc", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "NSCodec" },
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

int freerdp_client_print_version()
{
	printf("This is FreeRDP version %s (git %s)\n", FREERDP_VERSION_FULL, GIT_REVISION);
	return 1;
}

int freerdp_client_print_command_line_help()
{
	char* str;
	int length;
	COMMAND_LINE_ARGUMENT_A* arg;

	printf("\n");
	printf("FreeRDP - A Free Remote Desktop Protocol Implementation\n");
	printf("See www.freerdp.com for more information\n");
	printf("\n");

	printf("Usage: xfreerdp [file] [options] [/v:<server>[:port]]\n");
	printf("\n");

	printf("Syntax:\n");
	printf("    /flag (enables flag)\n");
	printf("    /option:<value> (specifies option with value)\n");
	printf("    +toggle -toggle (enables or disables toggle, where '/' is a synonym of '+')\n");
	printf("\n");

	arg = args;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			printf("    %s", "/");
			printf("%-20s", arg->Name);
			printf("\t%s\n", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) || (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			printf("    %s", "/");

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
			length = strlen(arg->Name) + 32;
			str = malloc(length + 1);
			sprintf_s(str, length + 1, "%s (default:%s)", arg->Name,
					arg->Default ? "on" : "off");

			printf("    %s", arg->Default ? "-" : "+");

			printf("%-20s", str);
			free(str);

			printf("\t%s\n", arg->Text);
		}
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	printf("\n");

	printf("Examples:\n");
	printf("    xfreerdp connection.rdp /p:Pwd123! /f\n");
	printf("    xfreerdp /u:CONTOSO\\JohnDoe /p:Pwd123! /v:rdp.contoso.com\n");
	printf("    xfreerdp /u:JohnDoe /p:Pwd123! /w:1366 /h:768 /v:192.168.1.100:4489\n");
	printf("\n");

	return 1;
}

int freerdp_client_command_line_pre_filter(void* context, int index, LPCSTR arg)
{
	if (index == 1)
	{
		int length;
		rdpSettings* settings;

		length = strlen(arg);

		if (length > 4)
		{
			if (_stricmp(&arg[length - 4], ".rdp") == 0)
			{
				settings = (rdpSettings*) context;
				settings->ConnectionFile = _strdup(arg);
			}
		}
	}

	return 1;
}

int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	CommandLineSwitchStart(arg)

	CommandLineSwitchCase(arg, "a")
	{
		int index;
		int nCommas;

		nCommas = 0;

		for (index = 0; arg->Value[index]; index++)
			nCommas += (arg->Value[index] == ',') ? 1 : 0;
	}

	CommandLineSwitchEnd(arg)

	return 1;
}

int freerdp_client_parse_command_line_arguments(int argc, char** argv, rdpSettings* settings)
{
	char* p;
	char* str;
	int length;
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;

	status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags, settings,
			freerdp_client_command_line_pre_filter, freerdp_client_command_line_post_filter);

	if (status == COMMAND_LINE_STATUS_PRINT_HELP)
	{
		freerdp_client_print_command_line_help();
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
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
				settings->ServerPort = atoi(&p[1]);
				settings->ServerHostname = (char*) malloc(length + 1);
				strncpy(settings->ServerHostname, arg->Value, length);
				settings->ServerHostname[length] = '\0';
			}
			else
			{
				settings->ServerHostname = _strdup(arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "port")
		{
			settings->ServerPort = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "w")
		{
			settings->DesktopWidth = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "h")
		{
			settings->DesktopHeight = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "size")
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
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->Fullscreen = TRUE;
		}
		CommandLineSwitchCase(arg, "workarea")
		{
			settings->Workarea = TRUE;
		}
		CommandLineSwitchCase(arg, "t")
		{
			settings->WindowTitle = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			settings->Decorations = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			settings->ColorDepth = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "u")
		{
			p = strchr(arg->Value, '\\');

			if (!p)
				p = strchr(arg->Value, '@');

			if (p)
			{
				length = p - arg->Value;
				settings->Domain = (char*) malloc(length + 1);
				strncpy(settings->Domain, arg->Value, length);
				settings->Domain[length] = '\0';
				settings->Username = _strdup(&p[1]);
			}
			else
			{
				settings->Username = _strdup(arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "d")
		{
			settings->Domain = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "p")
		{
			settings->Password = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "g")
		{
			p = strchr(arg->Value, ':');

			if (p)
			{
				length = p - arg->Value;
				settings->GatewayPort = atoi(&p[1]);
				settings->GatewayHostname = (char*) malloc(length + 1);
				strncpy(settings->GatewayHostname, arg->Value, length);
				settings->GatewayHostname[length] = '\0';
			}
			else
			{
				settings->GatewayHostname = _strdup(arg->Value);
			}

			settings->GatewayUsageMethod = TRUE;
		}
		CommandLineSwitchCase(arg, "gu")
		{
			p = strchr(arg->Value, '\\');

			if (!p)
				p = strchr(arg->Value, '@');

			if (p)
			{
				length = p - arg->Value;
				settings->GatewayDomain = (char*) malloc(length + 1);
				strncpy(settings->GatewayDomain, arg->Value, length);
				settings->GatewayDomain[length] = '\0';
				settings->GatewayUsername = _strdup(&p[1]);
			}
			else
			{
				settings->GatewayUsername = _strdup(arg->Value);
			}

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gd")
		{
			settings->GatewayDomain = _strdup(arg->Value);
			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gp")
		{
			settings->GatewayPassword = _strdup(arg->Value);
			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "z")
		{
			settings->CompressionEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "shell")
		{
			settings->AlternateShell = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "shell-dir")
		{
			settings->ShellWorkingDirectory = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "fonts")
		{
			settings->AllowFontSmoothing = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "wallpaper")
		{
			settings->DisableWallpaper = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "window-drag")
		{
			settings->DisableFullWindowDrag = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "menu-anims")
		{
			settings->DisableMenuAnims = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "themes")
		{
			settings->DisableThemes = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "aero")
		{
			settings->AllowDesktopComposition = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (strcmp(arg->Value, "sw") == 0)
				settings->SoftwareGdi = TRUE;
			else if (strcmp(arg->Value, "hw") == 0)
				settings->SoftwareGdi = FALSE;
		}
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = TRUE;
			settings->FastPathOutput = TRUE;
			settings->ColorDepth = 32;
			settings->PerformanceFlags = PERF_FLAG_NONE;
			settings->LargePointerFlag = TRUE;
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (strcmp(arg->Value, "video") == 0)
				settings->RemoteFxCodecMode = 0x00;
			else if (strcmp(arg->Value, "image") == 0)
				settings->RemoteFxCodecMode = 0x02;
		}
		CommandLineSwitchCase(arg, "frame-ack")
		{
			settings->FrameAcknowledge = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			settings->NSCodec = TRUE;
		}
		CommandLineSwitchCase(arg, "nego")
		{
			settings->NegotiateSecurityLayer = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionId = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionBlob = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strcmp("rdp", arg->Value) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = FALSE;
				settings->DisableEncryption = TRUE;
				settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}
			else if (strcmp("tls", arg->Value) == 0) /* TLS */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = TRUE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = FALSE;
			}
			else if (strcmp("nla", arg->Value) == 0) /* NLA */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = TRUE;
				settings->ExtSecurity = FALSE;
			}
			else if (strcmp("ext", arg->Value) == 0) /* NLA Extended */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = TRUE;
			}
			else
			{
				printf("unknown protocol security: %s\n", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "sec-rdp")
		{
			settings->RdpSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			settings->TlsSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			settings->NlaSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			settings->ExtSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "cert-name")
		{
			settings->CertificateName = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			settings->IgnoreCertificate = TRUE;
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	settings->PerformanceFlags = PERF_FLAG_NONE;

	if (settings->AllowFontSmoothing)
		settings->PerformanceFlags |= PERF_ENABLE_FONT_SMOOTHING;

	if (settings->AllowDesktopComposition)
		settings->PerformanceFlags |= PERF_ENABLE_DESKTOP_COMPOSITION;

	if (settings->DisableWallpaper)
		settings->PerformanceFlags |= PERF_DISABLE_WALLPAPER;

	if (settings->DisableFullWindowDrag)
		settings->PerformanceFlags |= PERF_DISABLE_FULLWINDOWDRAG;

	if (settings->DisableMenuAnims)
		settings->PerformanceFlags |= PERF_DISABLE_MENUANIMATIONS;

	if (settings->DisableThemes)
		settings->PerformanceFlags |= PERF_DISABLE_THEMING;

	return 1;
}
