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

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>

#include <freerdp/client/cmdline.h>

COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "v", COMMAND_LINE_VALUE_REQUIRED, "<server>[:port]", NULL, NULL, -1, NULL, "Server hostname" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Server port" },
	{ "w", COMMAND_LINE_VALUE_REQUIRED, "<width>", "1024", NULL, -1, NULL, "Width" },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, "<height>", "768", NULL, -1, NULL, "Height" },
	{ "size", COMMAND_LINE_VALUE_REQUIRED, "<width>x<height>", "1024x768", NULL, -1, NULL, "Screen size" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Fullscreen mode" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, "<depth>", "16", NULL, -1, NULL, "Session bpp (color depth)" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console", "Admin (or console) session" },
	{ "multimon", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Multi-monitor" },
	{ "workarea", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Work area" },
	{ "t", COMMAND_LINE_VALUE_REQUIRED, "<title>", NULL, NULL, -1, "title", "Window title" },
	{ "decorations", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "Window decorations" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "Addin" },
	{ "vc", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Static virtual channel" },
	{ "dvc", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Dynamic virtual channel" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\]<user>", NULL, NULL, -1, NULL, "Username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "Password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "Domain" },
	{ "g", COMMAND_LINE_VALUE_REQUIRED, "<gateway>[:port]", NULL, NULL, -1, NULL, "Gateway Hostname" },
	{ "gu", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\]<user>", NULL, NULL, -1, NULL, "Gateway username" },
	{ "gp", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "Gateway password" },
	{ "gd", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "Gateway domain" },
	{ "app", COMMAND_LINE_VALUE_REQUIRED, "||<alias> or <executable path>", NULL, NULL, -1, NULL, "Remote application program" },
	{ "app-name", COMMAND_LINE_VALUE_REQUIRED, "<app name>", NULL, NULL, -1, NULL, "Remote application name for user interface" },
	{ "app-icon", COMMAND_LINE_VALUE_REQUIRED, "<icon path>", NULL, NULL, -1, NULL, "Remote application icon for user interface" },
	{ "app-cmd", COMMAND_LINE_VALUE_REQUIRED, "<parameters>", NULL, NULL, -1, NULL, "Remote application command-line parameters" },
	{ "app-file", COMMAND_LINE_VALUE_REQUIRED, "<file name>", NULL, NULL, -1, NULL, "File to open with remote application" },
	{ "app-guid", COMMAND_LINE_VALUE_REQUIRED, "<app guid>", NULL, NULL, -1, NULL, "Remote application GUID" },
	{ "z", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Compression" },
	{ "shell", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Alternate shell" },
	{ "shell-dir", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Shell working directory" },
	{ "audio", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Audio output mode" },
	{ "mic", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Audio input (microphone)" },
	{ "clipboard", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Redirect clipboard" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Smooth fonts (cleartype)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "Desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Wallpaper" },
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
	{ "authentication", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "authentication (hack!)" },
	{ "encryption", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "encryption (hack!)" },
	{ "grab-keyboard", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "grab keyboard" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "print help" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

BOOL freerdp_detect_new_command_line_syntax(int argc, char* argv[])
{
	int index;

	for (index = 1; index < argc; index++)
	{
		if (argv[index][0] == '/')
			return TRUE;
	}

	return FALSE;
}

BOOL freerdp_detect_old_command_line_syntax(int argc, char* argv[])
{
	return (!freerdp_detect_new_command_line_syntax(argc, argv));
}

int freerdp_client_print_version()
{
	printf("This is FreeRDP version %s (git %s)\n", FREERDP_VERSION_FULL, GIT_REVISION);
	return 1;
}

int freerdp_client_print_command_line_help(int argc, char** argv)
{
	char* str;
	int length;
	COMMAND_LINE_ARGUMENT_A* arg;

	printf("\n");
	printf("FreeRDP - A Free Remote Desktop Protocol Implementation\n");
	printf("See www.freerdp.com for more information\n");
	printf("\n");

	printf("Usage: %s [file] [options] [/v:<server>[:port]]\n", argv[0]);
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

	printf("Clipboard Redirection: +clipboard\n");
	printf("\n");

	printf("Drive Redirection: /a:drive,home,/home\n");
	printf("Smartcard Redirection: /a:smartcard,<device>\n");
	printf("Printer Redirection: /a:printer,<device>,<driver>\n");
	printf("Serial Port Redirection: /a:serial,<device>\n");
	printf("Parallel Port Redirection: /a:parallel,<device>\n");
	printf("Printer Redirection: /a:printer,<device>,<driver>\n");
	printf("\n");

	printf("Audio Input Redirection: /dvc:audin,sys:alsa\n");
	printf("Audio Output Redirection: /vc:rdpsnd,sys:alsa\n");
	printf("\n");

	printf("Multimedia Redirection: /dvc:tsmf,sys:alsa\n");
	printf("USB Device Redirection: /dvc:urbdrc,id,dev:054c:0268\n");
	printf("\n");

	printf("More documentation is coming, in the meantime consult source files\n");
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

int freerdp_client_add_device_channel(rdpSettings* settings, int count, char** params)
{
	if (strcmp(params[0], "drive") == 0)
	{
		RDPDR_DRIVE* drive;

		if (count < 3)
			return -1;

		drive = (RDPDR_DRIVE*) malloc(sizeof(RDPDR_DRIVE));
		ZeroMemory(drive, sizeof(RDPDR_DRIVE));

		drive->Type = RDPDR_DTYP_FILESYSTEM;
		drive->Name = _strdup(params[1]);
		drive->Path = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) drive);

		return 1;
	}
	else if (strcmp(params[0], "printer") == 0)
	{
		RDPDR_PRINTER* printer;

		if (count < 2)
			return -1;

		printer = (RDPDR_PRINTER*) malloc(sizeof(RDPDR_PRINTER));
		ZeroMemory(printer, sizeof(RDPDR_PRINTER));

		printer->Type = RDPDR_DTYP_PRINT;
		printer->Name = _strdup(params[1]);

		if (params[2])
			printer->DriverName = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) printer);

		return 1;
	}
	else if (strcmp(params[0], "smartcard") == 0)
	{
		RDPDR_SMARTCARD* smartcard;

		if (count < 2)
			return -1;

		smartcard = (RDPDR_SMARTCARD*) malloc(sizeof(RDPDR_SMARTCARD));
		ZeroMemory(smartcard, sizeof(RDPDR_SMARTCARD));

		smartcard->Type = RDPDR_DTYP_SMARTCARD;
		smartcard->Name = _strdup(params[1]);

		if (count > 2)
			smartcard->Path = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) smartcard);

		return 1;
	}
	else if (strcmp(params[0], "serial") == 0)
	{
		RDPDR_SERIAL* serial;

		if (count < 2)
			return -1;

		serial = (RDPDR_SERIAL*) malloc(sizeof(RDPDR_SERIAL));
		ZeroMemory(serial, sizeof(RDPDR_SERIAL));

		serial->Type = RDPDR_DTYP_SERIAL;
		serial->Name = _strdup(params[1]);
		serial->Path = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) serial);

		return 1;
	}
	else if (strcmp(params[0], "parallel") == 0)
	{
		RDPDR_PARALLEL* parallel;

		if (count < 2)
			return -1;

		parallel = (RDPDR_PARALLEL*) malloc(sizeof(RDPDR_PARALLEL));
		ZeroMemory(parallel, sizeof(RDPDR_PARALLEL));

		parallel->Type = RDPDR_DTYP_PARALLEL;
		parallel->Name = _strdup(params[1]);
		parallel->Path = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) parallel);

		return 1;
	}

	return 0;
}

int freerdp_client_add_static_channel(rdpSettings* settings, int count, char** params)
{
	int index;
	ADDIN_ARGV* args;

	args = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));

	args->argc = count;
	args->argv = (char**) malloc(sizeof(char*) * args->argc);

	for (index = 0; index < args->argc; index++)
		args->argv[index] = _strdup(params[index]);

	freerdp_static_channel_collection_add(settings, args);

	return 0;
}

int freerdp_client_add_dynamic_channel(rdpSettings* settings, int count, char** params)
{
	int index;
	ADDIN_ARGV* args;

	args = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));

	args->argc = count;
	args->argv = (char**) malloc(sizeof(char*) * args->argc);

	for (index = 0; index < args->argc; index++)
		args->argv[index] = _strdup(params[index]);

	freerdp_dynamic_channel_collection_add(settings, args);

	return 0;
}

char** freerdp_command_line_parse_comma_separated_values(char* list, int* count)
{
	char** p;
	char* str;
	int nArgs;
	int index;
	int nCommas;

	nArgs = nCommas = 0;

	for (index = 0; list[index]; index++)
		nCommas += (list[index] == ',') ? 1 : 0;

	nArgs = nCommas + 1;
	p = (char**) malloc(sizeof(char*) * (nArgs + 1));
	ZeroMemory(p, sizeof(char*) * (nArgs + 1));

	str = (char*) list;

	p[0] = str;

	for (index = 1; index < nArgs; index++)
	{
		p[index] = strchr(p[index - 1], ',');
		*p[index] = '\0';
		p[index]++;
	}

	p[index] = str + strlen(str);

	*count = nArgs;

	return p;
}

int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings;

	CommandLineSwitchStart(arg)

	CommandLineSwitchCase(arg, "a")
	{
		char** p;
		int count;
		settings = (rdpSettings*) context;

		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		if (freerdp_client_add_device_channel(settings, count, p) > 0)
		{
			settings->DeviceRedirection = TRUE;
		}

		free(p);
	}
	CommandLineSwitchCase(arg, "vc")
	{
		char** p;
		int count;
		settings = (rdpSettings*) context;

		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		freerdp_client_add_static_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		char** p;
		int count;
		settings = (rdpSettings*) context;

		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		freerdp_client_add_dynamic_channel(settings, count, p);

		free(p);
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

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

	flags = COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SIGIL_PLUS_MINUS;

	status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags, settings,
			freerdp_client_command_line_pre_filter, freerdp_client_command_line_post_filter);

	if (status == COMMAND_LINE_STATUS_PRINT_HELP)
	{
		freerdp_client_print_command_line_help(argc, argv);
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
			settings->GatewayUseSameCredentials = TRUE;
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
		CommandLineSwitchCase(arg, "app")
		{
			settings->RemoteApplicationProgram = _strdup(arg->Value);

			settings->RemoteApplicationMode = TRUE;
			settings->RemoteAppLanguageBarSupported = TRUE;
			settings->Workarea = TRUE;
			settings->DisableWallpaper = TRUE;
			settings->DisableFullWindowDrag = TRUE;
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			settings->RemoteApplicationName = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			settings->RemoteApplicationIcon = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			settings->RemoteApplicationCmdLine = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			settings->RemoteApplicationFile = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			settings->RemoteApplicationGuid = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "z")
		{
			settings->CompressionEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "clipboard")
		{
			settings->RedirectClipboard = arg->Value ? TRUE : FALSE;
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
		CommandLineSwitchCase(arg, "authentication")
		{
			settings->Authentication = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "encryption")
		{
			settings->DisableEncryption = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "grab-keyboard")
		{
			settings->GrabKeyboard = arg->Value ? TRUE : FALSE;
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

int freerdp_client_load_static_channel_addin(rdpChannels* channels, rdpSettings* settings, char* name, void* data)
{
	void* entry;

	entry = freerdp_load_channel_addin_entry(name, NULL, NULL, 0);

	if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			printf("loading channel %s\n", name);
			return 0;
		}
	}

	return -1;
}

int freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	int index;
	ADDIN_ARGV* args;

	if ((freerdp_static_channel_collection_find(settings, "rdpsnd")) ||
			(freerdp_dynamic_channel_collection_find(settings, "tsmf")))
	{
		settings->AudioPlayback = TRUE; /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (settings->DeviceRedirection)
	{
		freerdp_client_load_static_channel_addin(channels, settings, "rdpdr", settings);

		if (!freerdp_static_channel_collection_find(settings, "rdpsnd"))
		{
			char* params[2];

			params[0] = "rdpsnd";
			params[1] = "sys:fake";

			freerdp_client_add_static_channel(settings, 2, (char**) params);
		}
	}

	if (settings->RedirectClipboard)
	{
		if (!freerdp_static_channel_collection_find(settings, "cliprdr"))
		{
			char* params[1];

			params[0] = "cliprdr";

			freerdp_client_add_static_channel(settings, 1, (char**) params);
		}
	}

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		args = settings->StaticChannelArray[index];
		freerdp_client_load_static_channel_addin(channels, settings, args->argv[0], args);
	}

	if (settings->RemoteApplicationMode)
	{
		freerdp_client_load_static_channel_addin(channels, settings, "rail", settings);
	}

	if (settings->DynamicChannelCount)
	{
		freerdp_client_load_static_channel_addin(channels, settings, "drdynvc", settings);
	}

	return 1;
}
