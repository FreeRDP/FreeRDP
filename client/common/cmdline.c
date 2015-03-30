/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Command-Line Interface
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/locale/keyboard.h>


#include <freerdp/client/cmdline.h>
#include <freerdp/version.h>

#include "compatibility.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.cmdline")

COMMAND_LINE_ARGUMENT_A args[] =
{
	{ "v", COMMAND_LINE_VALUE_REQUIRED, "<server>[:port]", NULL, NULL, -1, NULL, "Server hostname" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Server port" },
	{ "w", COMMAND_LINE_VALUE_REQUIRED, "<width>", "1024", NULL, -1, NULL, "Width" },
	{ "h", COMMAND_LINE_VALUE_REQUIRED, "<height>", "768", NULL, -1, NULL, "Height" },
	{ "size", COMMAND_LINE_VALUE_REQUIRED, "<width>x<height> or <percent>%", "1024x768", NULL, -1, NULL, "Screen size" },
	{ "f", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Fullscreen mode" },
	{ "bpp", COMMAND_LINE_VALUE_REQUIRED, "<depth>", "16", NULL, -1, NULL, "Session bpp (color depth)" },
	{ "kbd", COMMAND_LINE_VALUE_REQUIRED, "0x<layout id> or <layout name>", NULL, NULL, -1, NULL, "Keyboard layout" },
	{ "kbd-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, NULL, NULL, NULL, -1, NULL, "List keyboard layouts" },
	{ "kbd-type", COMMAND_LINE_VALUE_REQUIRED, "<type id>", NULL, NULL, -1, NULL, "Keyboard type" },
	{ "kbd-subtype", COMMAND_LINE_VALUE_REQUIRED, "<subtype id>", NULL, NULL, -1, NULL, "Keyboard subtype" },
	{ "kbd-fn-key", COMMAND_LINE_VALUE_REQUIRED, "<function key count>", NULL, NULL, -1, NULL, "Keyboard function key count" },
	{ "admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "console", "Admin (or console) session" },
	{ "restricted-admin", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "restrictedAdmin", "Restricted admin mode" },
	{ "pth", COMMAND_LINE_VALUE_REQUIRED, "<password hash>", NULL, NULL, -1, "pass-the-hash", "Pass the hash (restricted admin mode)" },
	{ "client-hostname", COMMAND_LINE_VALUE_REQUIRED, "<name>", NULL, NULL, -1, NULL, "Client Hostname to send to server" },
	{ "multimon", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "Use multiple monitors" },
	{ "span", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "Span screen over multiple monitors" },
	{ "workarea", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Use available work area" },
	{ "monitors", COMMAND_LINE_VALUE_REQUIRED, "<0,1,2...>", NULL, NULL, -1, NULL, "Select monitors to use" },
	{ "monitor-list", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT, NULL, NULL, NULL, -1, NULL, "List detected monitors" },
	{ "t", COMMAND_LINE_VALUE_REQUIRED, "<title>", NULL, NULL, -1, "title", "Window title" },
	{ "decorations", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Window decorations" },
	{ "smart-sizing", COMMAND_LINE_VALUE_OPTIONAL, "<width>x<height>", NULL, NULL, -1, NULL, "Scale remote desktop to window size" },
	{ "a", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, "addin", "Addin" },
	{ "vc", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Static virtual channel" },
	{ "dvc", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Dynamic virtual channel" },
	{ "u", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\]<user> or <user>[@<domain>]", NULL, NULL, -1, NULL, "Username" },
	{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "Password" },
	{ "d", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "Domain" },
	{ "g", COMMAND_LINE_VALUE_OPTIONAL, "<gateway>[:port]", NULL, NULL, -1, NULL, "Gateway Hostname" },
	{ "gu", COMMAND_LINE_VALUE_REQUIRED, "[<domain>\\]<user> or <user>[@<domain>]", NULL, NULL, -1, NULL, "Gateway username" },
	{ "gp", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "Gateway password" },
	{ "gd", COMMAND_LINE_VALUE_REQUIRED, "<domain>", NULL, NULL, -1, NULL, "Gateway domain" },
	{ "gt", COMMAND_LINE_VALUE_REQUIRED, "<rpc|http|auto>", NULL, NULL, -1, NULL, "Gateway transport type" },
	{ "gateway-usage-method", COMMAND_LINE_VALUE_REQUIRED, "<direct|detect>", NULL, NULL, -1, "gum", "Gateway usage method" },
	{ "load-balance-info", COMMAND_LINE_VALUE_REQUIRED, "<info string>", NULL, NULL, -1, NULL, "Load balance info" },
	{ "app", COMMAND_LINE_VALUE_REQUIRED, "<executable path> or <||alias>", NULL, NULL, -1, NULL, "Remote application program" },
	{ "app-name", COMMAND_LINE_VALUE_REQUIRED, "<app name>", NULL, NULL, -1, NULL, "Remote application name for user interface" },
	{ "app-icon", COMMAND_LINE_VALUE_REQUIRED, "<icon path>", NULL, NULL, -1, NULL, "Remote application icon for user interface" },
	{ "app-cmd", COMMAND_LINE_VALUE_REQUIRED, "<parameters>", NULL, NULL, -1, NULL, "Remote application command-line parameters" },
	{ "app-file", COMMAND_LINE_VALUE_REQUIRED, "<file name>", NULL, NULL, -1, NULL, "File to open with remote application" },
	{ "app-guid", COMMAND_LINE_VALUE_REQUIRED, "<app guid>", NULL, NULL, -1, NULL, "Remote application GUID" },
	{ "compression", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, "z", "Compression" },
	{ "compression-level", COMMAND_LINE_VALUE_REQUIRED, "<level>", NULL, NULL, -1, NULL, "Compression level (0,1,2)" },
	{ "shell", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Alternate shell" },
	{ "shell-dir", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Shell working directory" },
	{ "sound", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, "audio", "Audio output (sound)" },
	{ "microphone", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, "mic", "Audio input (microphone)" },
	{ "audio-mode", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Audio output mode" },
	{ "multimedia", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, "mmr", "Redirect multimedia (video)" },
	{ "network", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Network connection type" },
	{ "drive", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Redirect drive" },
	{ "drives", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Redirect all drives" },
	{ "home-drive", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Redirect home drive" },
	{ "clipboard", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Redirect clipboard" },
	{ "serial", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, "tty", "Redirect serial device" },
	{ "parallel", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "Redirect parallel device" },
	{ "smartcard", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "Redirect smartcard device" },
	{ "printer", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "Redirect printer device" },
	{ "usb", COMMAND_LINE_VALUE_REQUIRED, NULL, NULL, NULL, -1, NULL, "Redirect USB device" },
	{ "multitouch", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Redirect multitouch input" },
	{ "gestures", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Consume multitouch input locally" },
	{ "echo", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "echo", "Echo channel" },
	{ "disp", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "Display control" },
	{ "fonts", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Smooth fonts (ClearType)" },
	{ "aero", COMMAND_LINE_VALUE_BOOL, NULL, NULL, BoolValueFalse, -1, NULL, "Desktop composition" },
	{ "window-drag", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Full window drag" },
	{ "menu-anims", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Menu animations" },
	{ "themes", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Themes" },
	{ "wallpaper", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Wallpaper" },
	{ "gdi", COMMAND_LINE_VALUE_REQUIRED, "<sw|hw>", NULL, NULL, -1, NULL, "GDI rendering" },
	{ "gfx", COMMAND_LINE_VALUE_OPTIONAL, NULL, NULL, NULL, -1, NULL, "RDP8 graphics pipeline (experimental)" },
	{ "gfx-thin-client", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "RDP8 graphics pipeline thin client mode" },
	{ "gfx-small-cache", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "RDP8 graphics pipeline small cache mode" },
	{ "gfx-progressive", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "RDP8 graphics pipeline progressive codec" },
	{ "gfx-h264", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "RDP8.1 graphics pipeline H264 codec" },
	{ "rfx", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "RemoteFX" },
	{ "rfx-mode", COMMAND_LINE_VALUE_REQUIRED, "<image|video>", NULL, NULL, -1, NULL, "RemoteFX mode" },
	{ "frame-ack", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Frame acknowledgement" },
	{ "nsc", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, "nscodec", "NSCodec" },
	{ "jpeg", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "JPEG codec" },
	{ "jpeg-quality", COMMAND_LINE_VALUE_REQUIRED, "<percentage>", NULL, NULL, -1, NULL, "JPEG quality" },
	{ "nego", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "protocol security negotiation" },
	{ "sec", COMMAND_LINE_VALUE_REQUIRED, "<rdp|tls|nla|ext>", NULL, NULL, -1, NULL, "force specific protocol security" },
	{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "rdp protocol security" },
	{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "tls protocol security" },
	{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "nla protocol security" },
	{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "nla extended protocol security" },
	{ "tls-ciphers", COMMAND_LINE_VALUE_REQUIRED, "<netmon|ma|ciphers>", NULL, NULL, -1, NULL, "Allowed TLS ciphers" },
	{ "cert-name", COMMAND_LINE_VALUE_REQUIRED, "<name>", NULL, NULL, -1, NULL, "certificate name" },
	{ "cert-ignore", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL, "ignore certificate" },
	{ "pcb", COMMAND_LINE_VALUE_REQUIRED, "<blob>", NULL, NULL, -1, NULL, "Preconnection Blob" },
	{ "pcid", COMMAND_LINE_VALUE_REQUIRED, "<id>", NULL, NULL, -1, NULL, "Preconnection Id" },
	{ "spn-class", COMMAND_LINE_VALUE_REQUIRED, "<service class>", NULL, NULL, -1, NULL, "SPN authentication service class" },
	{ "credentials-delegation", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Disable credentials delegation" },
	{ "vmconnect", COMMAND_LINE_VALUE_OPTIONAL, "<vmid>", NULL, NULL, -1, NULL, "Hyper-V console (use port 2179, disable negotiation)" },
	{ "authentication", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "authentication (hack!)" },
	{ "encryption", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "encryption (hack!)" },
	{ "grab-keyboard", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "grab keyboard" },
	{ "toggle-fullscreen", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Alt+Ctrl+Enter toggles fullscreen" },
	{ "mouse-motion", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "mouse-motion" },
	{ "parent-window", COMMAND_LINE_VALUE_REQUIRED, "<window id>", NULL, NULL, -1, NULL, "Parent window id" },
	{ "bitmap-cache", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "bitmap cache" },
	{ "offscreen-cache", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "offscreen bitmap cache" },
	{ "glyph-cache", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "glyph cache" },
	{ "codec-cache", COMMAND_LINE_VALUE_REQUIRED, "<rfx|nsc|jpeg>", NULL, NULL, -1, NULL, "bitmap codec cache" },
	{ "fast-path", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "fast-path input/output" },
	{ "max-fast-path-size", COMMAND_LINE_VALUE_OPTIONAL, "<size>", NULL, NULL, -1, NULL, "maximum fast-path update size" },
	{ "async-input", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "asynchronous input" },
	{ "async-update", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "asynchronous update" },
	{ "async-transport", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "asynchronous transport (unstable)" },
	{ "async-channels", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "asynchronous channels (unstable)" },
	{ "wm-class", COMMAND_LINE_VALUE_REQUIRED, "<class name>", NULL, NULL, -1, NULL, "set the WM_CLASS hint for the window instance" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "print help" },
	{ "play-rfx", COMMAND_LINE_VALUE_REQUIRED, "<pcap file>", NULL, NULL, -1, NULL, "Replay rfx pcap file" },
	{ "auth-only", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Authenticate only." },
	{ "auto-reconnect", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Automatic reconnection" },
	{ "reconnect-cookie", COMMAND_LINE_VALUE_REQUIRED, "<base64 cookie>", NULL, NULL, -1, NULL, "Pass base64 reconnect cookie to the connection" },
	{ "print-reconnect-cookie", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Print base64 reconnect cookie after connecting" },
	{ "heartbeat", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Support heartbeat PDUs" },
	{ "multitransport", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Support multitransport protocol" },
	{ "assistance", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL, "Remote assistance password" },
	{ "encryption-methods", COMMAND_LINE_VALUE_REQUIRED, "<40,56,128,FIPS>", NULL, NULL, -1, NULL, "RDP standard security encryption methods" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

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
				length = (int)(strlen(arg->Name) + strlen(arg->Format) + 2);
				str = (char*) calloc(length + 1UL, sizeof(char));
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
			length = (int) strlen(arg->Name) + 32;
			str = (char*) calloc(length + 1UL, sizeof(char));
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
	printf("    xfreerdp /u:JohnDoe /p:Pwd123! /vmconnect:C824F53E-95D2-46C6-9A18-23A5BB403532 /v:192.168.1.100\n");
	printf("\n");

	printf("Clipboard Redirection: +clipboard\n");
	printf("\n");

	printf("Drive Redirection: /drive:home,/home/user\n");
	printf("Smartcard Redirection: /smartcard:<device>\n");
	printf("Serial Port Redirection: /serial:<name>,<device>,[SerCx2|SerCx|Serial],[permissive]\n");
	printf("Serial Port Redirection: /serial:COM1,/dev/ttyS0\n");
	printf("Parallel Port Redirection: /parallel:<device>\n");
	printf("Printer Redirection: /printer:<device>,<driver>\n");
	printf("\n");

	printf("Audio Output Redirection: /sound:sys:alsa\n");
	printf("Audio Input Redirection: /microphone:sys:alsa\n");
	printf("\n");

	printf("Multimedia Redirection: /multimedia:sys:alsa\n");
	printf("USB Device Redirection: /usb:id,dev:054c:0268\n");
	printf("\n");

	printf("More documentation is coming, in the meantime consult source files\n");
	printf("\n");

	return 1;
}

int freerdp_client_command_line_pre_filter(void* context, int index, int argc, LPCSTR* argv)
{
	if (index == 1)
	{
		int length;
		rdpSettings* settings;

		length = (int) strlen(argv[index]);

		if (length > 4)
		{
			if (_stricmp(&(argv[index])[length - 4], ".rdp") == 0)
			{
				settings = (rdpSettings*) context;
				settings->ConnectionFile = _strdup(argv[index]);

				return 1;
			}
		}

		if (length > 13)
		{
			if (_stricmp(&(argv[index])[length - 13], ".msrcIncident") == 0)
			{
				settings = (rdpSettings*) context;
				settings->AssistanceFile = _strdup(argv[index]);

				return 1;
			}
		}
	}

	return 0;
}

int freerdp_client_add_device_channel(rdpSettings* settings, int count, char** params)
{
	if (strcmp(params[0], "drive") == 0)
	{
		RDPDR_DRIVE* drive;

		if (count < 3)
			return -1;

		settings->DeviceRedirection = TRUE;

		drive = (RDPDR_DRIVE*) calloc(1, sizeof(RDPDR_DRIVE));

		if (!drive)
			return -1;

		drive->Type = RDPDR_DTYP_FILESYSTEM;

		if (count > 1)
			drive->Name = _strdup(params[1]);

		if (count > 2)
			drive->Path = _strdup(params[2]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) drive);

		return 1;
	}
	else if (strcmp(params[0], "printer") == 0)
	{
		RDPDR_PRINTER* printer;

		if (count < 1)
			return -1;

		settings->RedirectPrinters = TRUE;
		settings->DeviceRedirection = TRUE;

		if (count > 1)
		{
			printer = (RDPDR_PRINTER*) calloc(1, sizeof(RDPDR_PRINTER));

			if (!printer)
				return -1;

			printer->Type = RDPDR_DTYP_PRINT;

			if (count > 1)
				printer->Name = _strdup(params[1]);

			if (count > 2)
				printer->DriverName = _strdup(params[2]);

			freerdp_device_collection_add(settings, (RDPDR_DEVICE*) printer);
		}

		return 1;
	}
	else if (strcmp(params[0], "smartcard") == 0)
	{
		RDPDR_SMARTCARD* smartcard;

		if (count < 1)
			return -1;

		settings->RedirectSmartCards = TRUE;
		settings->DeviceRedirection = TRUE;

		if (count > 1)
		{
			smartcard = (RDPDR_SMARTCARD*) calloc(1, sizeof(RDPDR_SMARTCARD));

			if (!smartcard)
				return -1;

			smartcard->Type = RDPDR_DTYP_SMARTCARD;

			if (count > 1)
				smartcard->Name = _strdup(params[1]);

			if (count > 2)
				smartcard->Path = _strdup(params[2]);

			freerdp_device_collection_add(settings, (RDPDR_DEVICE*) smartcard);
		}

		return 1;
	}
	else if (strcmp(params[0], "serial") == 0)
	{
		RDPDR_SERIAL* serial;

		if (count < 1)
			return -1;

		settings->RedirectSerialPorts = TRUE;
		settings->DeviceRedirection = TRUE;

		serial = (RDPDR_SERIAL*) calloc(1, sizeof(RDPDR_SERIAL));

		if (!serial)
			return -1;

		serial->Type = RDPDR_DTYP_SERIAL;

		if (count > 1)
			serial->Name = _strdup(params[1]);

		if (count > 2)
			serial->Path = _strdup(params[2]);

		if (count > 3)
			serial->Driver = _strdup(params[3]);

		if (count > 4)
			serial->Permissive = _strdup(params[4]);

		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) serial);

		return 1;
	}
	else if (strcmp(params[0], "parallel") == 0)
	{
		RDPDR_PARALLEL* parallel;

		if (count < 1)
			return -1;

		settings->RedirectParallelPorts = TRUE;
		settings->DeviceRedirection = TRUE;

		parallel = (RDPDR_PARALLEL*) calloc(1, sizeof(RDPDR_PARALLEL));

		if (!parallel)
			return -1;

		parallel->Type = RDPDR_DTYP_PARALLEL;

		if (count > 1)
			parallel->Name = _strdup(params[1]);

		if (count > 2)
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
	args->argv = (char**) calloc(args->argc, sizeof(char*));

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
	args->argv = (char**) calloc(args->argc, sizeof(char*));

	for (index = 0; index < args->argc; index++)
		args->argv[index] = _strdup(params[index]);

	freerdp_dynamic_channel_collection_add(settings, args);

	return 0;
}

static char** freerdp_command_line_parse_comma_separated_values(char* list, int* count)
{
	char** p;
	char* str;
	int nArgs;
	int index;
	int nCommas;

	nCommas = 0;

	assert(NULL != count);

	*count = 0;
	if (!list)
		return NULL;

	for (index = 0; list[index]; index++)
		nCommas += (list[index] == ',') ? 1 : 0;

	nArgs = nCommas + 1;
	p = (char**) calloc((nArgs + 1UL), sizeof(char*));

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

static char** freerdp_command_line_parse_comma_separated_values_offset(char* list, int* count)
{
	char** p;
	char** t;

	p = freerdp_command_line_parse_comma_separated_values(list, count);
	if (!p)
		return NULL;

	t = (char**) realloc(p, sizeof(char*) * (*count + 1));
	if (!t)
		return NULL;
	p = t;
	MoveMemory(&p[1], p, sizeof(char*) * *count);
	(*count)++;

	return p;
}

int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings = (rdpSettings*) context;

	CommandLineSwitchStart(arg)

	CommandLineSwitchCase(arg, "a")
	{
		char** p;
		int count;

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

		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		freerdp_client_add_static_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		freerdp_client_add_dynamic_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "drive")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
		p[0] = "drive";

		freerdp_client_add_device_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "serial")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
		p[0] = "serial";

		freerdp_client_add_device_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "parallel")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
		p[0] = "parallel";

		freerdp_client_add_device_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "smartcard")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
		p[0] = "smartcard";

		freerdp_client_add_device_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "printer")
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			char** p;
			int count;

			p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
			p[0] = "printer";

			freerdp_client_add_device_channel(settings, count, p);

			free(p);
		}
		else
		{
			char* p[1];
			int count;

			count = 1;
			p[0] = "printer";

			freerdp_client_add_device_channel(settings, count, p);
		}
	}
	CommandLineSwitchCase(arg, "usb")
	{
		char** p;
		int count;

		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
		p[0] = "urbdrc";

		freerdp_client_add_dynamic_channel(settings, count, p);

		free(p);
	}
	CommandLineSwitchCase(arg, "multitouch")
	{
		settings->MultiTouchInput = TRUE;
	}
	CommandLineSwitchCase(arg, "gestures")
	{
		settings->MultiTouchGestures = TRUE;
	}
	CommandLineSwitchCase(arg, "echo")
	{
		settings->SupportEchoChannel = TRUE;
	}
	CommandLineSwitchCase(arg, "disp")
	{
		settings->SupportDisplayControl = TRUE;
	}
	CommandLineSwitchCase(arg, "sound")
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			char** p;
			int count;

			p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
			p[0] = "rdpsnd";

			freerdp_client_add_static_channel(settings, count, p);

			free(p);
		}
		else
		{
			char* p[1];
			int count;

			count = 1;
			p[0] = "rdpsnd";

			freerdp_client_add_static_channel(settings, count, p);
		}
	}
	CommandLineSwitchCase(arg, "microphone")
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			char** p;
			int count;

			p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
			p[0] = "audin";

			freerdp_client_add_dynamic_channel(settings, count, p);

			free(p);
		}
		else
		{
			char* p[1];
			int count;

			count = 1;
			p[0] = "audin";

			freerdp_client_add_dynamic_channel(settings, count, p);
		}
	}
	CommandLineSwitchCase(arg, "multimedia")
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			char** p;
			int count;

			p = freerdp_command_line_parse_comma_separated_values_offset(arg->Value, &count);
			p[0] = "tsmf";

			freerdp_client_add_dynamic_channel(settings, count, p);

			free(p);
		}
		else
		{
			char* p[1];
			int count;

			count = 1;
			p[0] = "tsmf";

			freerdp_client_add_dynamic_channel(settings, count, p);
		}
	}
	CommandLineSwitchCase(arg, "heartbeat")
	{
		settings->SupportHeartbeatPdu = TRUE;
	}
	CommandLineSwitchCase(arg, "multitransport")
	{
		settings->SupportMultitransport = TRUE;
		settings->MultitransportFlags = (TRANSPORT_TYPE_UDP_FECR | TRANSPORT_TYPE_UDP_FECL | TRANSPORT_TYPE_UDP_PREFERRED);
	}

	CommandLineSwitchEnd(arg)

	return 0;
}

int freerdp_parse_username(char* username, char** user, char** domain)
{
	char* p;
	int length;

	p = strchr(username, '\\');

	if (p)
	{
		length = (int) (p - username);
		*domain = (char*) calloc(length + 1UL, sizeof(char));
		strncpy(*domain, username, length);
		(*domain)[length] = '\0';
		*user = _strdup(&p[1]);
	}
	else
	{
		/* Do not break up the name for '@'; both credSSP and the
		 * ClientInfo PDU expect 'user@corp.net' to be transmitted
		 * as username 'user@corp.net', domain empty.
		 */
		*user = _strdup(username);
		*domain = NULL;
	}

	return 0;
}

int freerdp_parse_hostname(char* hostname, char** host, int* port)
{
	char* p;
	int length;

	p = strrchr(hostname, ':');

	if (p)
	{
		length = (p - hostname);
		*host = (char*) calloc(length + 1UL, sizeof(char));

		if (!(*host))
			return -1;

		CopyMemory(*host, hostname, length);
		(*host)[length] = '\0';
		*port = atoi(p + 1);
	}
	else
	{
		*host = _strdup(hostname);

		if (!(*host))
			return -1;

		*port = -1;
	}

	return 0;
}

int freerdp_set_connection_type(rdpSettings* settings, int type)
{
	settings->ConnectionType = type;

	if (type == CONNECTION_TYPE_MODEM)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = FALSE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = TRUE;
	}
	else if (type == CONNECTION_TYPE_BROADBAND_LOW)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = FALSE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
	}
	else if (type == CONNECTION_TYPE_SATELLITE)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
	}
	else if (type == CONNECTION_TYPE_BROADBAND_HIGH)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
	}
	else if (type == CONNECTION_TYPE_WAN)
	{
		settings->DisableWallpaper = FALSE;
		settings->AllowFontSmoothing = TRUE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = FALSE;
		settings->DisableMenuAnims = FALSE;
		settings->DisableThemes = FALSE;
	}
	else if (type == CONNECTION_TYPE_LAN)
	{
		settings->DisableWallpaper = FALSE;
		settings->AllowFontSmoothing = TRUE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = FALSE;
		settings->DisableMenuAnims = FALSE;
		settings->DisableThemes = FALSE;
	}
	else if (type == CONNECTION_TYPE_AUTODETECT)
	{
		settings->DisableWallpaper = FALSE;
		settings->AllowFontSmoothing = TRUE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = FALSE;
		settings->DisableMenuAnims = FALSE;
		settings->DisableThemes = FALSE;

		settings->NetworkAutoDetect = TRUE;
	}

	return 0;
}

int freerdp_map_keyboard_layout_name_to_id(char* name)
{
	int i;
	int id = 0;
	RDP_KEYBOARD_LAYOUT* layouts;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = layouts[i].code;
	}
	free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = layouts[i].code;
	}
	free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);
	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = layouts[i].code;
	}
	free(layouts);

	if (id)
		return id;

	return 0;
}

int freerdp_detect_command_line_pre_filter(void* context, int index, int argc, LPCSTR* argv)
{
	int length;

	if (index == 1)
	{
		length = (int) strlen(argv[index]);

		if (length > 4)
		{
			if (_stricmp(&(argv[index])[length - 4], ".rdp") == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}

int freerdp_detect_windows_style_command_line_syntax(int argc, char** argv,
	int* count, BOOL ignoreUnknown)
{
	int status;
	DWORD flags;
	int detect_status;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SEPARATOR_COLON;
	flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;
	if (ignoreUnknown)
	{
		flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	}

	*count = 0;
	detect_status = 0;
	CommandLineClearArgumentsA(args);

	status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags,
			NULL, freerdp_detect_command_line_pre_filter, NULL);

	if (status < 0)
		return status;

	arg = args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		(*count)++;
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((status <= COMMAND_LINE_ERROR) && (status >= COMMAND_LINE_ERROR_LAST))
		detect_status = -1;

	return detect_status;
}

int freerdp_detect_posix_style_command_line_syntax(int argc, char** argv,
	int* count, BOOL ignoreUnknown)
{
	int status;
	DWORD flags;
	int detect_status;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SEPARATOR_SPACE;
	flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;
	if (ignoreUnknown)
	{
		flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	}

	*count = 0;
	detect_status = 0;
	CommandLineClearArgumentsA(args);

	status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags,
			NULL, freerdp_detect_command_line_pre_filter, NULL);

	if (status < 0)
		return status;

	arg = args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		(*count)++;
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((status <= COMMAND_LINE_ERROR) && (status >= COMMAND_LINE_ERROR_LAST))
		detect_status = -1;

	return detect_status;
}

static BOOL freerdp_client_detect_command_line(int argc, char** argv,
	DWORD* flags, BOOL ignoreUnknown)
{
	int old_cli_status;
	int old_cli_count;
	int posix_cli_status;
	int posix_cli_count;
	int windows_cli_status;
	int windows_cli_count;
	BOOL compatibility = FALSE;

	windows_cli_status = freerdp_detect_windows_style_command_line_syntax(argc, argv, &windows_cli_count, ignoreUnknown);
	posix_cli_status = freerdp_detect_posix_style_command_line_syntax(argc, argv, &posix_cli_count, ignoreUnknown);
	old_cli_status = freerdp_detect_old_command_line_syntax(argc, argv, &old_cli_count);

	/* Default is POSIX syntax */
	*flags = COMMAND_LINE_SEPARATOR_SPACE;
	*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	*flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;

	if (posix_cli_status <= COMMAND_LINE_STATUS_PRINT)
		return compatibility;

	/* Check, if this may be windows style syntax... */
	if ((windows_cli_count && (windows_cli_count >= posix_cli_count)) || (windows_cli_status <= COMMAND_LINE_STATUS_PRINT))
	{
		windows_cli_count = 1;
		*flags = COMMAND_LINE_SEPARATOR_COLON;
		*flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;
	}
	else if (old_cli_status >= 0)
	{
		/* Ignore legacy parsing in case there is an error in the command line. */

		if ((old_cli_status == 1) || ((old_cli_count > posix_cli_count) && (old_cli_status != -1)))
		{
			*flags = COMMAND_LINE_SEPARATOR_SPACE;
			*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;

			compatibility = TRUE;
		}
	}

	WLog_DBG(TAG, "windows: %d/%d posix: %d/%d compat: %d/%d", windows_cli_status, windows_cli_count,
		posix_cli_status, posix_cli_count, old_cli_status, old_cli_count);

	return compatibility;
}

int freerdp_client_settings_command_line_status_print(rdpSettings* settings, int status, int argc, char** argv)
{
	COMMAND_LINE_ARGUMENT_A* arg;

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		arg = CommandLineFindArgumentA(args, "kbd-list");

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			int i;
			RDP_KEYBOARD_LAYOUT* layouts;

			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
			printf("\nKeyboard Layouts\n");
			for (i = 0; layouts[i].code; i++)
				printf("0x%08X\t%s\n", (int) layouts[i].code, layouts[i].name);
			free(layouts);

			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
			printf("\nKeyboard Layout Variants\n");
			for (i = 0; layouts[i].code; i++)
				printf("0x%08X\t%s\n", (int) layouts[i].code, layouts[i].name);
			free(layouts);

			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);
			printf("\nKeyboard Input Method Editors (IMEs)\n");
			for (i = 0; layouts[i].code; i++)
				printf("0x%08X\t%s\n", (int) layouts[i].code, layouts[i].name);
			free(layouts);

			printf("\n");
		}

		arg = CommandLineFindArgumentA(args, "monitor-list");

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			settings->ListMonitors = TRUE;
		}

		return COMMAND_LINE_STATUS_PRINT;
	}
	else if (status < 0)
	{
		freerdp_client_print_command_line_help(argc, argv);
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	return 0;
}

int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings,
	int argc, char** argv, BOOL allowUnknown)
{
	char* p;
	char* str;
	int length;
	int status;
	DWORD flags;
	BOOL compatibility;
	COMMAND_LINE_ARGUMENT_A* arg;

	compatibility = freerdp_client_detect_command_line(argc, argv, &flags, allowUnknown);

	if (compatibility)
	{
		WLog_WARN(TAG,  "Using deprecated command-line interface!");
		return freerdp_client_parse_old_command_line_arguments(argc, argv, settings);
	}
	else
	{
		CommandLineClearArgumentsA(args);

		if (allowUnknown)
		{
			flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
		}
		status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags, settings,
				freerdp_client_command_line_pre_filter, freerdp_client_command_line_post_filter);

		if (status < 0)
			return status;
	}

	CommandLineFindArgumentA(args, "v");

	arg = args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "v")
		{
			p = strchr(arg->Value, '[');
			/* ipv4 */
			if (!p)
			{
				p = strchr(arg->Value, ':');
				if (p)
				{
					length = (int) (p - arg->Value);
					settings->ServerPort = atoi(&p[1]);
					settings->ServerHostname = (char*) calloc(length + 1UL, sizeof(char));
					strncpy(settings->ServerHostname, arg->Value, length);
					settings->ServerHostname[length] = '\0';
				}
				else
				{
					settings->ServerHostname = _strdup(arg->Value);
				}
			}
			else /* ipv6 */
			{
				char *p2 = strchr(arg->Value, ']');
				/* not a valid [] ipv6 addr found */
				if (!p2)
					continue;

				length = p2 - p;
				settings->ServerHostname = (char*) calloc(length, sizeof(char));
				strncpy(settings->ServerHostname, p+1, length-1);
				if (*(p2 + 1) == ':')
				{
					settings->ServerPort = atoi(&p2[2]);
				}
				printf("hostname %s port %d\n", settings->ServerHostname, settings->ServerPort);
			}
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			settings->AuthenticationServiceClass = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "credentials-delegation")
		{
			settings->DisableCredentialsDelegation = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "vmconnect")
		{
			settings->ServerPort = 2179;
			settings->NegotiateSecurityLayer = FALSE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				settings->SendPreconnectionPdu = TRUE;
				settings->PreconnectionBlob = _strdup(arg->Value);
			}
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
			else
			{
				p = strchr(str, '%');
				if(p)
				{
					settings->PercentScreen = atoi(str);
				}
			}

			free(str);
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->Fullscreen = TRUE;
		}
		CommandLineSwitchCase(arg, "multimon")
		{
			settings->UseMultimon = TRUE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (_stricmp(arg->Value, "force") == 0)
				{
					settings->ForceMultimon = TRUE;
				}
			}
		}
		CommandLineSwitchCase(arg, "span")
		{
			settings->SpanMonitors = TRUE;
		}
		CommandLineSwitchCase(arg, "workarea")
		{
			settings->Workarea = TRUE;
		}
		CommandLineSwitchCase(arg, "monitors")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				char** p;
				int count = 0;

				p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

				if (count > 16)
					count = 16;

				settings->NumMonitorIds = (UINT32) count;

				for (i = 0; i < settings->NumMonitorIds; i++)
				{
					settings->MonitorIds[i] = atoi(p[i]);
				}

				free(p);
			}
		}
		CommandLineSwitchCase(arg, "monitor-list")
		{
			settings->ListMonitors = TRUE;
		}
		CommandLineSwitchCase(arg, "t")
		{
			settings->WindowTitle = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			settings->Decorations = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "smart-sizing")
		{
			settings->SmartSizing = TRUE;

			if (arg->Value)
			{
				str = _strdup(arg->Value);
				if ((p = strchr(str, 'x')))
				{
					*p = '\0';
					settings->SmartSizingWidth = atoi(str);
					settings->SmartSizingHeight = atoi(&p[1]);
				}
				free(str);
			}
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			settings->ColorDepth = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "admin")
		{
			settings->ConsoleSession = TRUE;
		}
		CommandLineSwitchCase(arg, "restricted-admin")
		{
			settings->ConsoleSession = TRUE;
			settings->RestrictedAdminModeRequired = TRUE;
		}
		CommandLineSwitchCase(arg, "pth")
		{
			settings->ConsoleSession = TRUE;
			settings->RestrictedAdminModeRequired = TRUE;
			settings->PasswordHash = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "client-hostname")
		{
			settings->ClientHostname = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "kbd")
		{
			unsigned long int id;
			char* pEnd;

			id = strtoul(arg->Value, &pEnd, 16);

			if (pEnd != (arg->Value + strlen(arg->Value)))
				id = 0;

			if (id == 0)
			{
				id = (unsigned long int) freerdp_map_keyboard_layout_name_to_id(arg->Value);

				if (!id)
				{
					WLog_ERR(TAG,  "Could not identify keyboard layout: %s", arg->Value);
				}
			}

			settings->KeyboardLayout = (UINT32) id;
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			settings->KeyboardType = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			settings->KeyboardSubType = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			settings->KeyboardFunctionKey = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "u")
		{
			char* user;
			char* domain;

			freerdp_parse_username(arg->Value, &user, &domain);

			settings->Username = user;
			settings->Domain = domain;
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
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				p = strchr(arg->Value, ':');

				if (p)
				{
					length = (int) (p - arg->Value);
					settings->GatewayPort = atoi(&p[1]);
					settings->GatewayHostname = (char*) calloc(length + 1UL, sizeof(char));
					strncpy(settings->GatewayHostname, arg->Value, length);
					settings->GatewayHostname[length] = '\0';
				}
				else
				{
					settings->GatewayHostname = _strdup(arg->Value);
				}
			}
			else
			{
				settings->GatewayHostname = _strdup(settings->ServerHostname);
			}

			settings->GatewayEnabled = TRUE;
			settings->GatewayUseSameCredentials = TRUE;

			freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT);
		}
		CommandLineSwitchCase(arg, "gu")
		{
			char* user;
			char* domain;

			freerdp_parse_username(arg->Value, &user, &domain);

			settings->GatewayUsername = user;
			settings->GatewayDomain = domain;

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
		CommandLineSwitchCase(arg, "gt")
		{
			if (_stricmp(arg->Value, "rpc") == 0)
			{
				settings->GatewayRpcTransport = TRUE;
				settings->GatewayHttpTransport = FALSE;
			}
			else if (_stricmp(arg->Value, "http") == 0)
			{
				settings->GatewayRpcTransport = FALSE;
				settings->GatewayHttpTransport = TRUE;
			}
			else if (_stricmp(arg->Value, "auto") == 0)
			{
				settings->GatewayRpcTransport = TRUE;
				settings->GatewayHttpTransport = TRUE;
			}
		}
		CommandLineSwitchCase(arg, "gateway-usage-method")
		{
			int type;
			char* pEnd;

			type = strtol(arg->Value, &pEnd, 10);

			if (type == 0)
			{
				if (_stricmp(arg->Value, "none") == 0)
					type = TSC_PROXY_MODE_NONE_DIRECT;
				else if (_stricmp(arg->Value, "direct") == 0)
					type = TSC_PROXY_MODE_DIRECT;
				else if (_stricmp(arg->Value, "detect") == 0)
					type = TSC_PROXY_MODE_DETECT;
				else if (_stricmp(arg->Value, "default") == 0)
					type = TSC_PROXY_MODE_DEFAULT;
			}

			freerdp_set_gateway_usage_method(settings, (UINT32) type);
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
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			settings->LoadBalanceInfo = (BYTE*) _strdup(arg->Value);
			settings->LoadBalanceInfoLength = (UINT32) strlen((char*) settings->LoadBalanceInfo);
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
		CommandLineSwitchCase(arg, "compression")
		{
			settings->CompressionEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "compression-level")
		{
			settings->CompressionLevel = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "drives")
		{
			settings->RedirectDrives = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "home-drive")
		{
			settings->RedirectHomeDrive = arg->Value ? TRUE : FALSE;
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
		CommandLineSwitchCase(arg, "audio-mode")
		{
			int mode;

			mode = atoi(arg->Value);

			if (mode == AUDIO_MODE_REDIRECT)
			{
				settings->AudioPlayback = TRUE;
			}
			else if (mode == AUDIO_MODE_PLAY_ON_SERVER)
			{
				settings->RemoteConsoleAudio = TRUE;
			}
			else if (mode == AUDIO_MODE_NONE)
			{
				settings->AudioPlayback = FALSE;
				settings->RemoteConsoleAudio = FALSE;
			}
		}
		CommandLineSwitchCase(arg, "network")
		{
			int type;
			char* pEnd;

			type = strtol(arg->Value, &pEnd, 10);

			if (type == 0)
			{
				if (_stricmp(arg->Value, "modem") == 0)
					type = CONNECTION_TYPE_MODEM;
				else if (_stricmp(arg->Value, "broadband") == 0)
					type = CONNECTION_TYPE_BROADBAND_HIGH;
				else if (_stricmp(arg->Value, "broadband-low") == 0)
					type = CONNECTION_TYPE_BROADBAND_LOW;
				else if (_stricmp(arg->Value, "broadband-high") == 0)
					type = CONNECTION_TYPE_BROADBAND_HIGH;
				else if (_stricmp(arg->Value, "wan") == 0)
					type = CONNECTION_TYPE_WAN;
				else if (_stricmp(arg->Value, "lan") == 0)
					type = CONNECTION_TYPE_LAN;
				else if ((_stricmp(arg->Value, "autodetect") == 0) ||
						(_stricmp(arg->Value, "auto") == 0) ||
						(_stricmp(arg->Value, "detect") == 0))
				{
					type = CONNECTION_TYPE_AUTODETECT;
				}
			}

			freerdp_set_connection_type(settings, type);
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
			if (_stricmp(arg->Value, "sw") == 0)
				settings->SoftwareGdi = TRUE;
			else if (_stricmp(arg->Value, "hw") == 0)
				settings->SoftwareGdi = FALSE;
		}
		CommandLineSwitchCase(arg, "gfx")
		{
			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-thin-client")
		{
			settings->GfxThinClient = arg->Value ? TRUE : FALSE;
			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-small-cache")
		{
			settings->GfxSmallCache = arg->Value ? TRUE : FALSE;
			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			settings->GfxProgressive = arg->Value ? TRUE : FALSE;
			settings->GfxThinClient = settings->GfxProgressive ? FALSE : TRUE;
			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			settings->GfxH264 = arg->Value ? TRUE : FALSE;
			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = TRUE;
			settings->FastPathOutput = TRUE;
			settings->ColorDepth = 32;
			settings->LargePointerFlag = TRUE;
			settings->FrameMarkerCommandEnabled = TRUE;
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
			settings->FastPathOutput = TRUE;
			settings->ColorDepth = 32;
			settings->LargePointerFlag = TRUE;
			settings->FrameMarkerCommandEnabled = TRUE;
		}
		CommandLineSwitchCase(arg, "jpeg")
		{
			settings->JpegCodec = TRUE;
			settings->JpegQuality = 75;
		}
		CommandLineSwitchCase(arg, "jpeg-quality")
		{
			settings->JpegQuality = atoi(arg->Value) % 100;
		}
		CommandLineSwitchCase(arg, "nego")
		{
			settings->NegotiateSecurityLayer = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionBlob = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionId = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strcmp("rdp", arg->Value) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = FALSE;
				settings->UseRdpSecurityLayer = TRUE;
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
				WLog_ERR(TAG,  "unknown protocol security: %s", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "encryption-methods")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				char** p;
				int count = 0;

				p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

				for (i = 0; i < count; i++)
				{
					if (!strcmp(p[i], "40"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_40BIT;
					else if (!strcmp(p[i], "56"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_56BIT;
					else if (!strcmp(p[i], "128"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_128BIT;
					else if (!strcmp(p[i], "FIPS"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_FIPS;
					else
						WLog_ERR(TAG,  "unknown encryption method '%s'", p[i]);
				}

				free(p);
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
		CommandLineSwitchCase(arg, "tls-ciphers")
		{
			if (strcmp(arg->Value, "netmon") == 0)
			{
				settings->AllowedTlsCiphers = _strdup("ALL:!ECDH");
			}
			else if (strcmp(arg->Value, "ma") == 0)
			{
				settings->AllowedTlsCiphers = _strdup("AES128-SHA");
			}
			else
			{
				settings->AllowedTlsCiphers = _strdup(arg->Value);
			}
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
			settings->UseRdpSecurityLayer = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "grab-keyboard")
		{
			settings->GrabKeyboard = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "toggle-fullscreen")
		{
			settings->ToggleFullscreen = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "mouse-motion")
		{
			settings->MouseMotion = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "parent-window")
		{
			settings->ParentWindowId = strtol(arg->Value, NULL, 0);
		}
		CommandLineSwitchCase(arg, "bitmap-cache")
		{
			settings->BitmapCacheEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "offscreen-cache")
		{
			settings->OffscreenSupportLevel = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "glyph-cache")
		{
			settings->GlyphSupportLevel = arg->Value ? GLYPH_SUPPORT_FULL : GLYPH_SUPPORT_NONE;
		}
		CommandLineSwitchCase(arg, "codec-cache")
		{
			settings->BitmapCacheV3Enabled = TRUE;

			if (strcmp(arg->Value, "rfx") == 0)
			{
				settings->RemoteFxCodec = TRUE;
			}
			else if (strcmp(arg->Value, "nsc") == 0)
			{
				settings->NSCodec = TRUE;
			}
			else if (strcmp(arg->Value, "jpeg") == 0)
			{
				settings->JpegCodec = TRUE;

				if (settings->JpegQuality == 0)
					settings->JpegQuality = 75;
			}
		}
		CommandLineSwitchCase(arg, "fast-path")
		{
			settings->FastPathInput = arg->Value ? TRUE : FALSE;
			settings->FastPathOutput = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			settings->MultifragMaxRequestSize = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "async-input")
		{
			settings->AsyncInput = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "async-update")
		{
			settings->AsyncUpdate = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "async-channels")
		{
			settings->AsyncChannels = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "async-transport")
		{
			settings->AsyncTransport = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "wm-class")
		{
			settings->WmClass = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			settings->PlayRemoteFxFile = _strdup(arg->Value);
			settings->PlayRemoteFx = TRUE;
		}
		CommandLineSwitchCase(arg, "auth-only")
		{
			settings->AuthenticationOnly = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "auto-reconnect")
		{
			settings->AutoReconnectionEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "reconnect-cookie")
		{
			BYTE *base64;
			int length;
			crypto_base64_decode((const char *) (arg->Value), (int) strlen(arg->Value),
								&base64, &length);
			if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
			{
				memcpy(settings->ServerAutoReconnectCookie, base64, length);
				free(base64);
			}
			else
			{
				WLog_ERR(TAG,  "reconnect-cookie:  invalid base64 '%s'", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "print-reconnect-cookie")
		{
			settings->PrintReconnectCookie = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "assistance")
		{
			settings->RemoteAssistanceMode = TRUE;
			settings->RemoteAssistancePassword = _strdup(arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	freerdp_performance_flags_make(settings);

	if (settings->SupportGraphicsPipeline)
	{
		settings->FastPathOutput = TRUE;
		settings->ColorDepth = 32;
		settings->LargePointerFlag = TRUE;
		settings->FrameMarkerCommandEnabled = TRUE;
	}

	arg = CommandLineFindArgumentA(args, "port");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		settings->ServerPort = atoi(arg->Value);
	}

	arg = CommandLineFindArgumentA(args, "p");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		FillMemory(arg->Value, strlen(arg->Value), '*');
	}

	arg = CommandLineFindArgumentA(args, "gp");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		FillMemory(arg->Value, strlen(arg->Value), '*');
	}

	return status;
}

int freerdp_client_load_static_channel_addin(rdpChannels* channels, rdpSettings* settings, char* name, void* data)
{
	void* entry;

	entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			WLog_INFO(TAG,  "loading channel %s", name);
			return 0;
		}
	}

	return -1;
}

int freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	UINT32 index;
	ADDIN_ARGV* args;

	if ((freerdp_static_channel_collection_find(settings, "rdpsnd")) ||
			(freerdp_dynamic_channel_collection_find(settings, "tsmf")))
	{
		settings->DeviceRedirection = TRUE; /* rdpsnd requires rdpdr to be registered */
		settings->AudioPlayback = TRUE; /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (freerdp_dynamic_channel_collection_find(settings, "audin"))
	{
		settings->AudioCapture = TRUE;
	}

	if (settings->NetworkAutoDetect ||
		settings->SupportHeartbeatPdu ||
		settings->SupportMultitransport)
	{
		settings->DeviceRedirection = TRUE; /* these RDP8 features require rdpdr to be registered */
	}

	if (settings->RedirectDrives || settings->RedirectHomeDrive || settings->RedirectSerialPorts
			|| settings->RedirectSmartCards || settings->RedirectPrinters)
	{
		settings->DeviceRedirection = TRUE; /* All of these features require rdpdr */
	}

	if (settings->RedirectDrives)
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			char* params[3];

			params[0] = "drive";
			params[1] = "media";
			params[2] = "*";

			freerdp_client_add_device_channel(settings, 3, (char**) params);
		}
	}

	if (settings->RedirectHomeDrive)
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			char* params[3];

			params[0] = "drive";
			params[1] = "home";
			params[2] = "%";

			freerdp_client_add_device_channel(settings, 3, (char**) params);
		}
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

	if (settings->RedirectSmartCards)
	{
		RDPDR_SMARTCARD* smartcard;

		smartcard = (RDPDR_SMARTCARD*) calloc(1, sizeof(RDPDR_SMARTCARD));

		if (!smartcard)
			return -1;

		smartcard->Type = RDPDR_DTYP_SMARTCARD;
		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) smartcard);
	}

	if (settings->RedirectPrinters)
	{
		RDPDR_PRINTER* printer;

		printer = (RDPDR_PRINTER*) calloc(1, sizeof(RDPDR_PRINTER));

		if (!printer)
			return -1;

		printer->Type = RDPDR_DTYP_PRINT;
		freerdp_device_collection_add(settings, (RDPDR_DEVICE*) printer);
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

	if (settings->LyncRdpMode)
	{
		settings->EncomspVirtualChannel = TRUE;
		settings->RemdeskVirtualChannel = TRUE;
		settings->CompressionEnabled = FALSE;
	}

	if (settings->RemoteAssistanceMode)
	{
		settings->EncomspVirtualChannel = TRUE;
		settings->RemdeskVirtualChannel = TRUE;
	}

	if (settings->EncomspVirtualChannel)
		freerdp_client_load_static_channel_addin(channels, settings, "encomsp", settings);

	if (settings->RemdeskVirtualChannel)
		freerdp_client_load_static_channel_addin(channels, settings, "remdesk", settings);

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		args = settings->StaticChannelArray[index];
		freerdp_client_load_static_channel_addin(channels, settings, args->argv[0], args);
	}

	if (settings->RemoteApplicationMode)
	{
		freerdp_client_load_static_channel_addin(channels, settings, "rail", settings);
	}

	if (settings->MultiTouchInput)
	{
		char* p[1];
		int count;

		count = 1;
		p[0] = "rdpei";

		freerdp_client_add_dynamic_channel(settings, count, p);
	}

	if (settings->SupportGraphicsPipeline)
	{
		char* p[1];
		int count;

		count = 1;
		p[0] = "rdpgfx";

		freerdp_client_add_dynamic_channel(settings, count, p);
	}

	if (settings->SupportEchoChannel)
	{
		char* p[1];
		int count;

		count = 1;
		p[0] = "echo";

		freerdp_client_add_dynamic_channel(settings, count, p);
	}

	if (settings->SupportDisplayControl)
	{
		char* p[1];
		int count;

		count = 1;
		p[0] = "disp";

		freerdp_client_add_dynamic_channel(settings, count, p);
	}

	if (settings->DynamicChannelCount)
		settings->SupportDynamicChannels = TRUE;

	if (settings->SupportDynamicChannels)
	{
		freerdp_client_load_static_channel_addin(channels, settings, "drdynvc", settings);
	}

	return 1;
}
