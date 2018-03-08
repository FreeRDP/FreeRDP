/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Command-Line Interface
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
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

#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/path.h>

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/locale/keyboard.h>

#include <freerdp/utils/passphrase.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/version.h>

#include "compatibility.h"
#include "cmdline.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.cmdline")

BOOL freerdp_client_print_version(void)
{
	printf("This is FreeRDP version %s (%s)\n", FREERDP_VERSION_FULL,
	       GIT_REVISION);
	return TRUE;
}

BOOL freerdp_client_print_buildconfig(void)
{
	printf("%s", freerdp_get_build_config());
	return TRUE;
}

static void freerdp_client_print_command_line_args(COMMAND_LINE_ARGUMENT_A* arg)
{
	if (!arg)
		return;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			printf("    %s", "/");
			printf("%-20s", arg->Name);
			printf("\t%s\n", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED)
		         || (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			BOOL overlong = FALSE;
			printf("    %s", "/");

			if (arg->Format)
			{
				size_t length = (strlen(arg->Name) + strlen(arg->Format) + 2);

				if (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL)
					length += 2;

				if (length >= 20 + 8 + 8)
					overlong = TRUE;

				if (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL)
					printf("%s[:%s]", arg->Name, overlong ? "..." : arg->Format);
				else
					printf("%s:%s", arg->Name, overlong ? "..." : arg->Format);
			}
			else
			{
				printf("%-20s", arg->Name);
			}

			printf("\t%s\n", arg->Text);
		}
		else if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			printf("    %s", arg->Default ? "-" : "+");
			printf("%-20s", arg->Name);
			printf("\t%s (default:%s)\n", arg->Text, arg->Default ? "on" : "off");
		}
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

BOOL freerdp_client_print_command_line_help(int argc, char** argv)
{
	return freerdp_client_print_command_line_help_ex(argc, argv, NULL);
}

BOOL freerdp_client_print_command_line_help_ex(int argc, char** argv,
        COMMAND_LINE_ARGUMENT_A* custom)
{
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
	freerdp_client_print_command_line_args(custom);
	freerdp_client_print_command_line_args(args);
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
	printf("Parallel Port Redirection: /parallel:<name>,<device>\n");
	printf("Printer Redirection: /printer:<device>,<driver>\n");
	printf("\n");
	printf("Audio Output Redirection: /sound:sys:oss,dev:1,format:1\n");
	printf("Audio Output Redirection: /sound:sys:alsa\n");
	printf("Audio Input Redirection: /microphone:sys:oss,dev:1,format:1\n");
	printf("Audio Input Redirection: /microphone:sys:alsa\n");
	printf("\n");
	printf("Multimedia Redirection: /multimedia:sys:oss,dev:/dev/dsp1,decoder:ffmpeg\n");
	printf("Multimedia Redirection: /multimedia:sys:alsa\n");
	printf("USB Device Redirection: /usb:id,dev:054c:0268\n");
	printf("\n");
	printf("For Gateways, the https_proxy environment variable is respected:\n");
#ifdef _WIN32
	printf("    set HTTPS_PROXY=http://proxy.contoso.com:3128/\n");
#else
	printf("    export https_proxy=http://proxy.contoso.com:3128/\n");
#endif
	printf("    xfreerdp /g:rdp.contoso.com ...\n");
	printf("\n");
	printf("More documentation is coming, in the meantime consult source files\n");
	printf("\n");
	return TRUE;
}

static int freerdp_client_command_line_pre_filter(void* context, int index,
        int argc, LPCSTR* argv)
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

				if (!(settings->ConnectionFile = _strdup(argv[index])))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}

		if (length > 13)
		{
			if (_stricmp(&(argv[index])[length - 13], ".msrcIncident") == 0)
			{
				settings = (rdpSettings*) context;

				if (!(settings->AssistanceFile = _strdup(argv[index])))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}
	}

	return 0;
}

BOOL freerdp_client_add_device_channel(rdpSettings* settings, int count,
                                       char** params)
{
	if (strcmp(params[0], "drive") == 0)
	{
		RDPDR_DRIVE* drive;

		if (count < 3)
			return FALSE;

		settings->DeviceRedirection = TRUE;
		drive = (RDPDR_DRIVE*) calloc(1, sizeof(RDPDR_DRIVE));

		if (!drive)
			return FALSE;

		drive->Type = RDPDR_DTYP_FILESYSTEM;

		if (count > 1)
		{
			if (!(drive->Name = _strdup(params[1])))
			{
				free(drive);
				return FALSE;
			}
		}

		if (count > 2)
		{
			const BOOL isPath = PathFileExistsA(params[2]);
			const BOOL isSpecial = (strncmp(params[2], "*", 2) == 0) ||
			                       (strncmp(params[2], "%", 2) == 0) ? TRUE : FALSE;

			if ((!isPath && !isSpecial) || !(drive->Path = _strdup(params[2])))
			{
				free(drive->Name);
				free(drive);
				return FALSE;
			}
		}

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) drive))
		{
			free(drive->Path);
			free(drive->Name);
			free(drive);
			return FALSE;
		}

		return TRUE;
	}
	else if (strcmp(params[0], "printer") == 0)
	{
		RDPDR_PRINTER* printer;

		if (count < 1)
			return FALSE;

		settings->RedirectPrinters = TRUE;
		settings->DeviceRedirection = TRUE;

		if (count > 1)
		{
			printer = (RDPDR_PRINTER*) calloc(1, sizeof(RDPDR_PRINTER));

			if (!printer)
				return FALSE;

			printer->Type = RDPDR_DTYP_PRINT;

			if (count > 1)
			{
				if (!(printer->Name = _strdup(params[1])))
				{
					free(printer);
					return FALSE;
				}
			}

			if (count > 2)
			{
				if (!(printer->DriverName = _strdup(params[2])))
				{
					free(printer->Name);
					free(printer);
					return FALSE;
				}
			}

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) printer))
			{
				free(printer->DriverName);
				free(printer->Name);
				free(printer);
				return FALSE;
			}
		}

		return TRUE;
	}
	else if (strcmp(params[0], "smartcard") == 0)
	{
		RDPDR_SMARTCARD* smartcard;

		if (count < 1)
			return FALSE;

		settings->RedirectSmartCards = TRUE;
		settings->DeviceRedirection = TRUE;
		smartcard = (RDPDR_SMARTCARD*) calloc(1, sizeof(RDPDR_SMARTCARD));

		if (!smartcard)
			return FALSE;

		smartcard->Type = RDPDR_DTYP_SMARTCARD;

		if (count > 1 && strlen(params[1]))
		{
			if (!(smartcard->Name = _strdup(params[1])))
			{
				free(smartcard);
				return FALSE;
			}
		}

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) smartcard))
		{
			free(smartcard->Name);
			free(smartcard);
			return FALSE;
		}

		return TRUE;
	}
	else if (strcmp(params[0], "serial") == 0)
	{
		RDPDR_SERIAL* serial;

		if (count < 1)
			return FALSE;

		settings->RedirectSerialPorts = TRUE;
		settings->DeviceRedirection = TRUE;
		serial = (RDPDR_SERIAL*) calloc(1, sizeof(RDPDR_SERIAL));

		if (!serial)
			return FALSE;

		serial->Type = RDPDR_DTYP_SERIAL;

		if (count > 1)
		{
			if (!(serial->Name = _strdup(params[1])))
			{
				free(serial);
				return FALSE;
			}
		}

		if (count > 2)
		{
			if (!(serial->Path = _strdup(params[2])))
			{
				free(serial->Name);
				free(serial);
				return FALSE;
			}
		}

		if (count > 3)
		{
			if (!(serial->Driver = _strdup(params[3])))
			{
				free(serial->Path);
				free(serial->Name);
				free(serial);
				return FALSE;
			}
		}

		if (count > 4)
		{
			if (!(serial->Permissive = _strdup(params[4])))
			{
				free(serial->Driver);
				free(serial->Path);
				free(serial->Name);
				free(serial);
				return FALSE;
			}
		}

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) serial))
		{
			free(serial->Permissive);
			free(serial->Driver);
			free(serial->Path);
			free(serial->Name);
			free(serial);
			return FALSE;
		}

		return TRUE;
	}
	else if (strcmp(params[0], "parallel") == 0)
	{
		RDPDR_PARALLEL* parallel;

		if (count < 1)
			return FALSE;

		settings->RedirectParallelPorts = TRUE;
		settings->DeviceRedirection = TRUE;
		parallel = (RDPDR_PARALLEL*) calloc(1, sizeof(RDPDR_PARALLEL));

		if (!parallel)
			return FALSE;

		parallel->Type = RDPDR_DTYP_PARALLEL;

		if (count > 1)
		{
			if (!(parallel->Name = _strdup(params[1])))
			{
				free(parallel);
				return FALSE;
			}
		}

		if (count > 2)
		{
			if (!(parallel->Path = _strdup(params[2])))
			{
				free(parallel->Name);
				free(parallel);
				return FALSE;
			}
		}

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) parallel))
		{
			free(parallel->Path);
			free(parallel->Name);
			free(parallel);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

BOOL freerdp_client_add_static_channel(rdpSettings* settings, int count,
                                       char** params)
{
	int index;
	ADDIN_ARGV* args;

	if (!settings || !params || !params[0])
		return FALSE;

	if (freerdp_static_channel_collection_find(settings, params[0]))
		return TRUE;

	args = (ADDIN_ARGV*) calloc(1, sizeof(ADDIN_ARGV));

	if (!args)
		return FALSE;

	args->argc = count;
	args->argv = (char**) calloc(args->argc, sizeof(char*));

	if (!args->argv)
		goto error_argv;

	for (index = 0; index < args->argc; index++)
	{
		args->argv[index] = _strdup(params[index]);

		if (!args->argv[index])
		{
			for (--index; index >= 0; --index)
				free(args->argv[index]);

			goto error_argv_strdup;
		}
	}

	if (!freerdp_static_channel_collection_add(settings, args))
		goto error_argv_index;

	return TRUE;
error_argv_index:

	for (index = 0; index < args->argc; index++)
		free(args->argv[index]);

error_argv_strdup:
	free(args->argv);
error_argv:
	free(args);
	return FALSE;
}

BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, int count,
                                        char** params)
{
	int index;
	ADDIN_ARGV* args;

	if (!settings || !params || !params[0])
		return FALSE;

	if (freerdp_dynamic_channel_collection_find(settings, params[0]))
		return TRUE;

	args = (ADDIN_ARGV*) malloc(sizeof(ADDIN_ARGV));

	if (!args)
		return FALSE;

	args->argc = count;
	args->argv = (char**) calloc(args->argc, sizeof(char*));

	if (!args->argv)
		goto error_argv;

	for (index = 0; index < args->argc; index++)
	{
		args->argv[index] = _strdup(params[index]);

		if (!args->argv[index])
		{
			for (--index; index >= 0; --index)
				free(args->argv[index]);

			goto error_argv_strdup;
		}
	}

	if (!freerdp_dynamic_channel_collection_add(settings, args))
		goto error_argv_index;

	return TRUE;
error_argv_index:

	for (index = 0; index < args->argc; index++)
		free(args->argv[index]);

error_argv_strdup:
	free(args->argv);
error_argv:
	free(args);
	return FALSE;
}

static char** freerdp_command_line_parse_comma_separated_values_ex(const char* name,
        const char* list,
        size_t* count)
{
	char** p;
	char* str;
	size_t nArgs;
	size_t index;
	size_t nCommas;
	size_t prefix, len;
	nCommas = 0;
	assert(NULL != count);
	*count = 0;

	if (!list)
	{
		if (name)
		{
			p = (char**) calloc(1UL, sizeof(char*));

			if (p)
			{
				p[0] = name;
				*count = 1;
				return p;
			}
		}

		return NULL;
	}

	{
		const char* it = list;

		while ((it = strchr(it, ',')) != NULL)
		{
			it++;
			nCommas++;
		}
	}

	nArgs = nCommas + 1;

	if (name)
		nArgs++;

	prefix = (nArgs + 1UL) * sizeof(char*);
	len = strlen(list);
	p = (char**) calloc(len + prefix + 1, sizeof(char*));

	if (!p)
		return NULL;

	str = &((char*)p)[prefix];
	memcpy(str, list, len);

	if (name)
		p[0] = (char*)name;

	for (index = name ? 1 : 0; index < nArgs; index++)
	{
		char* comma = strchr(str, ',');
		p[index] = str;

		if (comma)
		{
			str = comma + 1;
			*comma = '\0';
		}
	}

	*count = nArgs;
	return p;
}

static char** freerdp_command_line_parse_comma_separated_values(char* list,
        size_t* count)
{
	return freerdp_command_line_parse_comma_separated_values_ex(NULL, list, count);
}

static char** freerdp_command_line_parse_comma_separated_values_offset(
    const char* name, char* list, size_t* count)
{
	return freerdp_command_line_parse_comma_separated_values_ex(name, list, count);
}

static int freerdp_client_command_line_post_filter(void* context,
        COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings = (rdpSettings*) context;
	BOOL status = TRUE;
	CommandLineSwitchStart(arg)
	CommandLineSwitchCase(arg, "a")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

		if ((status = freerdp_client_add_device_channel(settings, count, p)))
		{
			settings->DeviceRedirection = TRUE;
		}

		free(p);
	}
	CommandLineSwitchCase(arg, "vc")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);
		status = freerdp_client_add_static_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "drive")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Name, arg->Value,
		        &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "serial")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Name, arg->Value,
		        &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "parallel")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Name, arg->Value,
		        &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "smartcard")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Name, arg->Value,
		        &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "printer")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset(arg->Name, arg->Value,
		        &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "usb")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset("urbdrc", arg->Value,
		        &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
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
	CommandLineSwitchCase(arg, "ssh-agent")
	{
		settings->SupportSSHAgentChannel = TRUE;
	}
	CommandLineSwitchCase(arg, "disp")
	{
		settings->SupportDisplayControl = TRUE;
	}
	CommandLineSwitchCase(arg, "geometry")
	{
		settings->SupportGeometryTracking = TRUE;
	}
	CommandLineSwitchCase(arg, "video")
	{
		settings->SupportGeometryTracking = TRUE; /* this requires geometry tracking */
		settings->SupportVideoOptimized = TRUE;
	}
	CommandLineSwitchCase(arg, "sound")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset("rdpsnd", arg->Value,
		        &count);
		status = freerdp_client_add_static_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "microphone")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset("audin", arg->Value,
		        &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "multimedia")
	{
		char** p;
		size_t count;
		p = freerdp_command_line_parse_comma_separated_values_offset("tsmf", arg->Value,
		        &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "heartbeat")
	{
		settings->SupportHeartbeatPdu = TRUE;
	}
	CommandLineSwitchCase(arg, "multitransport")
	{
		settings->SupportMultitransport = TRUE;
		settings->MultitransportFlags = (TRANSPORT_TYPE_UDP_FECR |
		                                 TRANSPORT_TYPE_UDP_FECL | TRANSPORT_TYPE_UDP_PREFERRED);
	}
	CommandLineSwitchCase(arg, "password-is-pin")
	{
		settings->PasswordIsSmartcardPin = TRUE;
	}
	CommandLineSwitchEnd(arg)
	return status ? 1 : -1;
}

BOOL freerdp_parse_username(const char* username, char** user, char** domain)
{
	char* p;
	int length = 0;
	p = strchr(username, '\\');
	*user = NULL;
	*domain = NULL;

	if (p)
	{
		length = (int)(p - username);
		*user = _strdup(&p[1]);

		if (!*user)
			return FALSE;

		*domain = (char*) calloc(length + 1UL, sizeof(char));

		if (!*domain)
		{
			free(*user);
			*user = NULL;
			return FALSE;
		}

		strncpy(*domain, username, length);
		(*domain)[length] = '\0';
	}
	else if (username)
	{
		/* Do not break up the name for '@'; both credSSP and the
		 * ClientInfo PDU expect 'user@corp.net' to be transmitted
		 * as username 'user@corp.net', domain empty (not NULL!).
		 */
		*user = _strdup(username);

		if (!*user)
			return FALSE;

		*domain = _strdup("\0");

		if (!*domain)
		{
			free(*user);
			*user = NULL;
			return FALSE;
		}
	}
	else
		return FALSE;

	return TRUE;
}

BOOL freerdp_parse_hostname(const char* hostname, char** host, int* port)
{
	char* p;
	p = strrchr(hostname, ':');

	if (p)
	{
		unsigned long val;
		SSIZE_T length = (p - hostname);
		errno = 0;
		val = strtoul(p + 1, NULL, 0);

		if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
			return FALSE;

		*host = (char*) calloc(length + 1UL, sizeof(char));

		if (!(*host))
			return FALSE;

		CopyMemory(*host, hostname, length);
		(*host)[length] = '\0';
		*port = val;
	}
	else
	{
		*host = _strdup(hostname);

		if (!(*host))
			return FALSE;

		*port = -1;
	}

	return TRUE;
}

BOOL freerdp_set_connection_type(rdpSettings* settings, int type)
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

	return TRUE;
}

int freerdp_map_keyboard_layout_name_to_id(char* name)
{
	int i;
	int id = 0;
	RDP_KEYBOARD_LAYOUT* layouts;
	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);

	if (!layouts)
		return -1;

	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = layouts[i].code;
	}

	free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);

	if (!layouts)
		return -1;

	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = layouts[i].code;
	}

	free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);

	if (!layouts)
		return -1;

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

static int freerdp_detect_command_line_pre_filter(void* context, int index,
        int argc, LPCSTR* argv)
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

		if (length > 13)
		{
			if (_stricmp(&(argv[index])[length - 13], ".msrcIncident") == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}

static int freerdp_detect_windows_style_command_line_syntax(int argc, char** argv,
        size_t* count, BOOL ignoreUnknown)
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
        size_t* count, BOOL ignoreUnknown)
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
	size_t posix_cli_count;
	int windows_cli_status;
	size_t windows_cli_count;
	BOOL compatibility = FALSE;
	windows_cli_status = freerdp_detect_windows_style_command_line_syntax(argc,
	                     argv, &windows_cli_count, ignoreUnknown);
	posix_cli_status = freerdp_detect_posix_style_command_line_syntax(argc, argv,
	                   &posix_cli_count, ignoreUnknown);
	old_cli_status = freerdp_detect_old_command_line_syntax(argc, argv,
	                 &old_cli_count);
	/* Default is POSIX syntax */
	*flags = COMMAND_LINE_SEPARATOR_SPACE;
	*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	*flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;

	if (posix_cli_status <= COMMAND_LINE_STATUS_PRINT)
		return compatibility;

	/* Check, if this may be windows style syntax... */
	if ((windows_cli_count && (windows_cli_count >= posix_cli_count))
	    || (windows_cli_status <= COMMAND_LINE_STATUS_PRINT))
	{
		windows_cli_count = 1;
		*flags = COMMAND_LINE_SEPARATOR_COLON;
		*flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;
	}
	else if (old_cli_status >= 0)
	{
		/* Ignore legacy parsing in case there is an error in the command line. */
		if ((old_cli_status == 1) || ((old_cli_count > posix_cli_count)
		                              && (old_cli_status != -1)))
		{
			*flags = COMMAND_LINE_SEPARATOR_SPACE;
			*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
			compatibility = TRUE;
		}
	}

	WLog_DBG(TAG, "windows: %d/%d posix: %d/%d compat: %d/%d", windows_cli_status,
	         windows_cli_count,
	         posix_cli_status, posix_cli_count, old_cli_status, old_cli_count);
	return compatibility;
}

int freerdp_client_settings_command_line_status_print(rdpSettings* settings,
        int status, int argc, char** argv)
{
	return freerdp_client_settings_command_line_status_print_ex(
	           settings, status, argc, argv, NULL);
}

int freerdp_client_settings_command_line_status_print_ex(rdpSettings* settings,
        int status, int argc, char** argv, COMMAND_LINE_ARGUMENT_A* custom)
{
	COMMAND_LINE_ARGUMENT_A* arg;

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}

	if (status == COMMAND_LINE_STATUS_PRINT_BUILDCONFIG)
	{
		freerdp_client_print_version();
		freerdp_client_print_buildconfig();
		return COMMAND_LINE_STATUS_PRINT_BUILDCONFIG;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		arg = CommandLineFindArgumentA(args, "kbd-list");

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			DWORD i;
			RDP_KEYBOARD_LAYOUT* layouts;
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
			//if (!layouts) /* FIXME*/
			printf("\nKeyboard Layouts\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08"PRIX32"\t%s\n", layouts[i].code, layouts[i].name);

			free(layouts);
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
			//if (!layouts) /* FIXME*/
			printf("\nKeyboard Layout Variants\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08"PRIX32"\t%s\n", layouts[i].code, layouts[i].name);

			free(layouts);
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);
			//if (!layouts) /* FIXME*/
			printf("\nKeyboard Input Method Editors (IMEs)\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08"PRIX32"\t%s\n", layouts[i].code, layouts[i].name);

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
		freerdp_client_print_command_line_help_ex(argc, argv, custom);
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	return 0;
}

static BOOL ends_with(const char* str, const char* ext)
{
	const size_t strLen = strlen(str);
	const size_t extLen = strlen(ext);

	if (strLen < extLen)
		return FALSE;

	return strncmp(&str[strLen - extLen], ext, extLen) == 0;
}
int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings,
        int argc, char** argv, BOOL allowUnknown)
{
	char* p;
	char* user = NULL;
	char* gwUser = NULL;
	char* str;
	size_t length;
	int status;
	BOOL ext = FALSE;
	BOOL assist = FALSE;
	DWORD flags = 0;
	BOOL promptForPassword = FALSE;
	BOOL compatibility = FALSE;
	COMMAND_LINE_ARGUMENT_A* arg;

	/* Command line detection fails if only a .rdp or .msrcIncident file
	 * is supplied. Check this case first, only then try to detect
	 * legacy command line syntax. */
	if (argc > 1)
	{
		ext = ends_with(argv[1], ".rdp");
		assist = ends_with(argv[1], ".msrcIncident");
	}

	if (!ext && !assist)
		compatibility = freerdp_client_detect_command_line(argc, argv, &flags,
		                allowUnknown);

	if (compatibility)
	{
		WLog_WARN(TAG, "Using deprecated command-line interface!");
		return freerdp_client_parse_old_command_line_arguments(argc, argv, settings);
	}
	else
	{
		if (allowUnknown)
			flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

		if (ext)
		{
			if (freerdp_client_settings_parse_connection_file(settings, argv[1]))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		if (assist)
		{
			if (freerdp_client_settings_parse_assistance_file(settings,
			        argv[1]) < 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		CommandLineClearArgumentsA(args);
		status = CommandLineParseArgumentsA(argc, (const char**) argv, args, flags,
		                                    settings,
		                                    freerdp_client_command_line_pre_filter,
		                                    freerdp_client_command_line_post_filter);

		if (status < 0)
			return status;
	}

	CommandLineFindArgumentA(args, "v");
	arg = args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "v")
		{
			free(settings->ServerHostname);
			settings->ServerHostname = NULL;
			p = strchr(arg->Value, '[');

			/* ipv4 */
			if (!p)
			{
				p = strchr(arg->Value, ':');

				if (p)
				{
					unsigned long val = strtoul(&p[1], NULL, 0);

					if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (int)(p - arg->Value);
					settings->ServerPort = val;

					if (!(settings->ServerHostname = (char*) calloc(length + 1UL, sizeof(char))))
						return COMMAND_LINE_ERROR_MEMORY;

					strncpy(settings->ServerHostname, arg->Value, length);
					settings->ServerHostname[length] = '\0';
				}
				else
				{
					if (!(settings->ServerHostname = _strdup(arg->Value)))
						return COMMAND_LINE_ERROR_MEMORY;
				}
			}
			else /* ipv6 */
			{
				char* p2 = strchr(arg->Value, ']');

				/* not a valid [] ipv6 addr found */
				if (!p2)
					continue;

				length = p2 - p;

				if (!(settings->ServerHostname = (char*) calloc(length, sizeof(char))))
					return COMMAND_LINE_ERROR_MEMORY;

				strncpy(settings->ServerHostname, p + 1, length - 1);

				if (*(p2 + 1) == ':')
				{
					unsigned long val = strtoul(&p2[2], NULL, 0);

					if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					settings->ServerPort = val;
				}

				printf("hostname %s port %"PRIu32"\n", settings->ServerHostname, settings->ServerPort);
			}
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			free(settings->AuthenticationServiceClass);

			if (!(settings->AuthenticationServiceClass = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "credentials-delegation")
		{
			settings->DisableCredentialsDelegation = arg->Value ? FALSE : TRUE;
		}
		CommandLineSwitchCase(arg, "vmconnect")
		{
			settings->VmConnectMode = TRUE;
			settings->ServerPort = 2179;
			settings->NegotiateSecurityLayer = FALSE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				settings->SendPreconnectionPdu = TRUE;
				free(settings->PreconnectionBlob);

				if (!(settings->PreconnectionBlob = _strdup(arg->Value)))
					return COMMAND_LINE_ERROR_MEMORY;
			}
		}
		CommandLineSwitchCase(arg, "w")
		{
			long val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopWidth = val;
		}
		CommandLineSwitchCase(arg, "h")
		{
			long val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopHeight = val;
		}
		CommandLineSwitchCase(arg, "size")
		{
			if (!(str = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			p = strchr(str, 'x');

			if (p)
			{
				*p = '\0';
				{
					long val = strtol(str, NULL, 0);

					if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
					{
						free(str);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					settings->DesktopWidth = val;
				}
				{
					long val = strtol(&p[1], NULL, 0);

					if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
					{
						free(str);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					settings->DesktopHeight = val;
				}
			}
			else
			{
				p = strchr(str, '%');

				if (p)
				{
					BOOL partial = FALSE;

					if (strchr(p, 'w'))
					{
						settings->PercentScreenUseWidth = 1;
						partial = TRUE;
					}

					if (strchr(p, 'h'))
					{
						settings->PercentScreenUseHeight = 1;
						partial = TRUE;
					}

					if (!partial)
					{
						settings->PercentScreenUseWidth = 1;
						settings->PercentScreenUseHeight = 1;
					}

					*p = '\0';
					{
						long val = strtol(str, NULL, 0);

						if ((errno != 0) || (val < 0) || (val > 100))
						{
							free(str);
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}

						settings->PercentScreen = val;
					}
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
				size_t count = 0;
				p = freerdp_command_line_parse_comma_separated_values(arg->Value, &count);

				if (!p)
					return COMMAND_LINE_ERROR_MEMORY;

				if (count > 16)
					count = 16;

				settings->NumMonitorIds = (UINT32) count;

				for (i = 0; i < settings->NumMonitorIds; i++)
				{
					unsigned long val = strtoul(p[i], NULL, 0);

					if ((errno != 0) || (val > UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					settings->MonitorIds[i] = val;
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
			free(settings->WindowTitle);

			if (!(settings->WindowTitle = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			settings->Decorations = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "dynamic-resolution")
		{
			if (settings->SmartSizing)
			{
				WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			settings->SupportDisplayControl = TRUE;
			settings->DynamicResolutionUpdate = TRUE;
		}
		CommandLineSwitchCase(arg, "smart-sizing")
		{
			if (settings->DynamicResolutionUpdate)
			{
				WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			settings->SmartSizing = TRUE;

			if (arg->Value)
			{
				if (!(str = _strdup(arg->Value)))
					return COMMAND_LINE_ERROR_MEMORY;

				if ((p = strchr(str, 'x')))
				{
					unsigned long w, h;
					*p = '\0';
					w = strtoul(str, NULL, 0);

					if ((errno != 0) || (w == 0) || (w > UINT16_MAX))
					{
						free(str);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					h = strtoul(&p[1], NULL, 0);

					if ((errno != 0) || (h == 0) || (h > UINT16_MAX))
					{
						free(str);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					settings->SmartSizingWidth = w;
					settings->SmartSizingHeight = h;
				}

				free(str);
			}
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->ColorDepth = val;

			switch (settings->ColorDepth)
			{
				case 32:
				case 24:
				case 16:
				case 15:
				case 8:
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
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
			free(settings->PasswordHash);

			if (!(settings->PasswordHash = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "client-hostname")
		{
			free(settings->ClientHostname);

			if (!(settings->ClientHostname = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "kbd")
		{
			unsigned long id = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (id > UINT32_MAX) || (id == 0))
			{
				const int rc = freerdp_map_keyboard_layout_name_to_id(arg->Value);

				if (rc <= 0)
				{
					WLog_ERR(TAG, "Could not identify keyboard layout: %s", arg->Value);
					WLog_ERR(TAG, "Use /kbd-list to list available layouts");
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}

				/* Found a valid mapping, reset errno */
				id = (unsigned long)rc;
				errno = 0;
			}

			settings->KeyboardLayout = (UINT32) id;
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardType = val;
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardSubType = val;
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardFunctionKey = val;
		}
		CommandLineSwitchCase(arg, "u")
		{
			user = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			free(settings->Domain);

			if (!(settings->Domain = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "p")
		{
			free(settings->Password);

			if (!(settings->Password = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "g")
		{
			free(settings->GatewayHostname);

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				p = strchr(arg->Value, ':');

				if (p)
				{
					unsigned long val = strtoul(&p[1], NULL, 0);

					if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (int)(p - arg->Value);
					settings->GatewayPort = val;

					if (!(settings->GatewayHostname = (char*) calloc(length + 1UL, sizeof(char))))
						return COMMAND_LINE_ERROR_MEMORY;

					strncpy(settings->GatewayHostname, arg->Value, length);
					settings->GatewayHostname[length] = '\0';
				}
				else
				{
					if (!(settings->GatewayHostname = _strdup(arg->Value)))
						return COMMAND_LINE_ERROR_MEMORY;
				}
			}
			else
			{
				if (!(settings->GatewayHostname = _strdup(settings->ServerHostname)))
					return COMMAND_LINE_ERROR_MEMORY;
			}

			settings->GatewayEnabled = TRUE;
			settings->GatewayUseSameCredentials = TRUE;
			freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT);
		}
		CommandLineSwitchCase(arg, "proxy")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				p = strstr(arg->Value, "://");

				if (p)
				{
					*p = '\0';

					if (!strcmp("http", arg->Value))
					{
						settings->ProxyType = PROXY_TYPE_HTTP;
					}
					else
					{
						WLog_ERR(TAG, "Only HTTP proxys supported by now");
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					arg->Value = p + 3;
				}

				p = strchr(arg->Value, ':');

				if (p)
				{
					unsigned long val = strtoul(&p[1], NULL, 0);

					if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (p - arg->Value);
					settings->ProxyPort = val;
					settings->ProxyHostname = (char*) malloc(length + 1);
					strncpy(settings->ProxyHostname, arg->Value, length);
					settings->ProxyHostname[length] = '\0';
					settings->ProxyType = PROXY_TYPE_HTTP;
				}
			}
			else
			{
				WLog_ERR(TAG, "Option http-proxy needs argument.");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "gu")
		{
			if (!(gwUser = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gd")
		{
			free(settings->GatewayDomain);

			if (!(settings->GatewayDomain = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gp")
		{
			free(settings->GatewayPassword);

			if (!(settings->GatewayPassword = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

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
		CommandLineSwitchCase(arg, "gat")
		{
			free(settings->GatewayAccessToken);

			if (!(settings->GatewayAccessToken = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "gateway-usage-method")
		{
			long type;
			char* pEnd;
			type = strtol(arg->Value, &pEnd, 10);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
			free(settings->RemoteApplicationProgram);

			if (!(settings->RemoteApplicationProgram = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->RemoteApplicationMode = TRUE;
			settings->RemoteAppLanguageBarSupported = TRUE;
			settings->Workarea = TRUE;
			settings->DisableWallpaper = TRUE;
			settings->DisableFullWindowDrag = TRUE;
		}
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			free(settings->LoadBalanceInfo);

			if (!(settings->LoadBalanceInfo = (BYTE*) _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->LoadBalanceInfoLength = (UINT32) strlen((char*)
			                                  settings->LoadBalanceInfo);
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			free(settings->RemoteApplicationName);

			if (!(settings->RemoteApplicationName = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			free(settings->RemoteApplicationIcon);

			if (!(settings->RemoteApplicationIcon = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			free(settings->RemoteApplicationCmdLine);

			if (!(settings->RemoteApplicationCmdLine = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			free(settings->RemoteApplicationFile);

			if (!(settings->RemoteApplicationFile = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			free(settings->RemoteApplicationGuid);

			if (!(settings->RemoteApplicationGuid = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "compression")
		{
			settings->CompressionEnabled = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "compression-level")
		{
			unsigned long val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->CompressionLevel = val;
		}
		CommandLineSwitchCase(arg, "drives")
		{
			settings->RedirectDrives = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "home-drive")
		{
			settings->RedirectHomeDrive = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "ipv6")
		{
			settings->PreferIPv6OverIPv4 = TRUE;
		}
		CommandLineSwitchCase(arg, "clipboard")
		{
			settings->RedirectClipboard = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "shell")
		{
			free(settings->AlternateShell);

			if (!(settings->AlternateShell = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "shell-dir")
		{
			free(settings->ShellWorkingDirectory);

			if (!(settings->ShellWorkingDirectory = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "audio-mode")
		{
			long mode = strtol(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
			long type;
			char* pEnd;
			type = strtol(arg->Value, &pEnd, 10);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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

			if (!freerdp_set_connection_type(settings, type))
				return COMMAND_LINE_ERROR;
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

			if (arg->Value)
			{
#ifdef WITH_GFX_H264

				if (_strnicmp("AVC444", arg->Value, 6) == 0)
				{
					settings->GfxH264 = TRUE;
					settings->GfxAVC444 = TRUE;
				}
				else if (_strnicmp("AVC420", arg->Value, 6) == 0)
				{
					settings->GfxH264 = TRUE;
				}
				else
#endif
					if (_strnicmp("RFX", arg->Value, 3) != 0)
						return COMMAND_LINE_ERROR;
			}
		}
		CommandLineSwitchCase(arg, "gfx-thin-client")
		{
			settings->GfxThinClient = arg->Value ? TRUE : FALSE;

			if (settings->GfxThinClient)
				settings->GfxSmallCache = TRUE;

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
#ifdef WITH_GFX_H264
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			settings->SupportGraphicsPipeline = TRUE;
			settings->GfxH264 = TRUE;

			if (arg->Value)
			{
				if (_strnicmp("AVC444", arg->Value, 6) == 0)
				{
					settings->GfxAVC444 = TRUE;
				}
				else if (_strnicmp("AVC420", arg->Value, 6) != 0)
					return COMMAND_LINE_ERROR;
			}
		}
#endif
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = TRUE;
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
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->FrameAcknowledge = val;
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			settings->NSCodec = TRUE;
		}
#if defined(WITH_JPEG)
		CommandLineSwitchCase(arg, "jpeg")
		{
			settings->JpegCodec = TRUE;
			settings->JpegQuality = 75;
		}
		CommandLineSwitchCase(arg, "jpeg-quality")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > 100))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->JpegQuality = val;
		}
#endif
		CommandLineSwitchCase(arg, "nego")
		{
			settings->NegotiateSecurityLayer = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			settings->SendPreconnectionPdu = TRUE;
			free(settings->PreconnectionBlob);

			if (!(settings->PreconnectionBlob = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionId = val;
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
				WLog_ERR(TAG, "unknown protocol security: %s", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "encryption-methods")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				char** p;
				size_t count = 0;
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
						WLog_ERR(TAG, "unknown encryption method '%s'", p[i]);
				}

				free(p);
			}
		}
		CommandLineSwitchCase(arg, "from-stdin")
		{
			settings->CredentialsFromStdin = TRUE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				promptForPassword = (strncmp(arg->Value, "force", 6) == 0);

				if (!promptForPassword)
					return COMMAND_LINE_ERROR;
			}
		}
		CommandLineSwitchCase(arg, "log-level")
		{
			wLog* root = WLog_GetRoot();

			if (!WLog_SetStringLogLevel(root, arg->Value))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "log-filters")
		{
			if (!WLog_AddStringLogFilters(arg->Value))
				return COMMAND_LINE_ERROR;
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
			free(settings->AllowedTlsCiphers);

			if (strcmp(arg->Value, "netmon") == 0)
			{
				if (!(settings->AllowedTlsCiphers = _strdup("ALL:!ECDH")))
					return COMMAND_LINE_ERROR_MEMORY;
			}
			else if (strcmp(arg->Value, "ma") == 0)
			{
				if (!(settings->AllowedTlsCiphers = _strdup("AES128-SHA")))
					return COMMAND_LINE_ERROR_MEMORY;
			}
			else
			{
				if (!(settings->AllowedTlsCiphers = _strdup(arg->Value)))
					return COMMAND_LINE_ERROR_MEMORY;
			}
		}
		CommandLineSwitchCase(arg, "cert-name")
		{
			free(settings->CertificateName);

			if (!(settings->CertificateName = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			settings->IgnoreCertificate = TRUE;
		}
		CommandLineSwitchCase(arg, "cert-tofu")
		{
			settings->AutoAcceptCertificate = TRUE;
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
		CommandLineSwitchCase(arg, "unmap-buttons")
		{
			settings->UnmapButtons = arg->Value ? TRUE : FALSE;
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
			UINT64 val = _strtoui64(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->ParentWindowId = val;
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
			settings->GlyphSupportLevel = arg->Value ? GLYPH_SUPPORT_FULL :
			                              GLYPH_SUPPORT_NONE;
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

#if defined(WITH_JPEG)
			else if (strcmp(arg->Value, "jpeg") == 0)
			{
				settings->JpegCodec = TRUE;

				if (settings->JpegQuality == 0)
					settings->JpegQuality = 75;
			}

#endif
		}
		CommandLineSwitchCase(arg, "fast-path")
		{
			settings->FastPathInput = arg->Value ? TRUE : FALSE;
			settings->FastPathOutput = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->MultifragMaxRequestSize = val;
		}
		CommandLineSwitchCase(arg, "max-loop-time")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->MaxTimeInCheckLoop = val;

			if ((long) settings->MaxTimeInCheckLoop < 0)
			{
				WLog_ERR(TAG, "invalid max loop time: %s", arg->Value);
				return COMMAND_LINE_ERROR;
			}

			if ((long) settings->MaxTimeInCheckLoop <= 0)
			{
				settings->MaxTimeInCheckLoop = 10 * 60 * 60 * 1000; /* 10 hours can be considered as infinite */
			}
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
			free(settings->WmClass);

			if (!(settings->WmClass = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			free(settings->PlayRemoteFxFile);

			if (!(settings->PlayRemoteFxFile = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

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
		CommandLineSwitchCase(arg, "auto-reconnect-max-retries")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->AutoReconnectMaxRetries = val;

			if (settings->AutoReconnectMaxRetries > 1000)
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "reconnect-cookie")
		{
			BYTE* base64;
			int length;
			crypto_base64_decode((const char*)(arg->Value), (int) strlen(arg->Value),
			                     &base64, &length);

			if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
			{
				memcpy(settings->ServerAutoReconnectCookie, base64, length);
				free(base64);
			}
			else
			{
				WLog_ERR(TAG, "reconnect-cookie:  invalid base64 '%s'", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "print-reconnect-cookie")
		{
			settings->PrintReconnectCookie = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "assistance")
		{
			settings->RemoteAssistanceMode = TRUE;
			free(settings->RemoteAssistancePassword);

			if (!(settings->RemoteAssistancePassword = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "pwidth")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopPhysicalWidth = val;
		}
		CommandLineSwitchCase(arg, "pheight")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopPhysicalHeight = val;
		}
		CommandLineSwitchCase(arg, "orientation")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > INT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopOrientation = val;
		}
		CommandLineSwitchCase(arg, "scale")
		{
			unsigned long scaleFactor = strtoul(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (scaleFactor == 100 || scaleFactor == 140 || scaleFactor == 180)
			{
				settings->DesktopScaleFactor = scaleFactor;
				settings->DeviceScaleFactor = scaleFactor;
			}
			else
			{
				WLog_ERR(TAG, "scale:  invalid scale factor (%d)", scaleFactor);
				return COMMAND_LINE_ERROR;
			}
		}
		CommandLineSwitchCase(arg, "scale-desktop")
		{
			unsigned long desktopScaleFactor = strtoul(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (desktopScaleFactor >= 100 && desktopScaleFactor <= 500)
			{
				settings->DesktopScaleFactor = desktopScaleFactor;
			}
			else
			{
				WLog_ERR(TAG, "scale:  invalid desktop scale factor (%d)", desktopScaleFactor);
				return COMMAND_LINE_ERROR;
			}
		}
		CommandLineSwitchCase(arg, "scale-device")
		{
			unsigned long deviceScaleFactor = strtoul(arg->Value, NULL, 0);

			if (errno != 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (deviceScaleFactor == 100 || deviceScaleFactor == 140
			    || deviceScaleFactor == 180)
			{
				settings->DeviceScaleFactor = deviceScaleFactor;
			}
			else
			{
				WLog_ERR(TAG, "scale:  invalid device scale factor (%d)", deviceScaleFactor);
				return COMMAND_LINE_ERROR;
			}
		}
		CommandLineSwitchCase(arg, "action-script")
		{
			free(settings->ActionScript);

			if (!(settings->ActionScript = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "fipsmode")
		{
			settings->FIPSMode = TRUE;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if (user)
	{
		free(settings->Username);

		if (!settings->Domain && user)
		{
			BOOL ret;
			free(settings->Domain);
			ret = freerdp_parse_username(user, &settings->Username, &settings->Domain);
			free(user);

			if (!ret)
				return COMMAND_LINE_ERROR;
		}
		else
			settings->Username = user;
	}

	if (gwUser)
	{
		free(settings->GatewayUsername);

		if (!settings->GatewayDomain && gwUser)
		{
			BOOL ret;
			free(settings->GatewayDomain);
			ret = freerdp_parse_username(gwUser, &settings->GatewayUsername,
			                             &settings->GatewayDomain);
			free(gwUser);

			if (!ret)
				return COMMAND_LINE_ERROR;
		}
		else
			settings->GatewayUsername = gwUser;
	}

	if (promptForPassword)
	{
		const size_t size = 512;

		if (!settings->Password)
		{
			settings->Password = calloc(size, sizeof(char));

			if (!settings->Password)
				return COMMAND_LINE_ERROR;

			if (!freerdp_passphrase_read("Password: ", settings->Password, size, 1))
				return COMMAND_LINE_ERROR;
		}

		if (settings->GatewayEnabled && !settings->GatewayUseSameCredentials)
		{
			if (!settings->GatewayPassword)
			{
				settings->GatewayPassword = calloc(size, sizeof(char));

				if (!settings->GatewayPassword)
					return COMMAND_LINE_ERROR;

				if (!freerdp_passphrase_read("Gateway Password: ", settings->GatewayPassword, size, 1))
					return COMMAND_LINE_ERROR;
			}
		}
	}

	freerdp_performance_flags_make(settings);

	if (settings->RemoteFxCodec || settings->NSCodec
	    || settings->SupportGraphicsPipeline)
	{
		settings->FastPathOutput = TRUE;
		settings->LargePointerFlag = TRUE;
		settings->FrameMarkerCommandEnabled = TRUE;
		settings->ColorDepth = 32;
	}

	arg = CommandLineFindArgumentA(args, "port");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		unsigned long val = strtoul(arg->Value, NULL, 0);

		if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		settings->ServerPort = val;
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

static BOOL freerdp_client_load_static_channel_addin(rdpChannels* channels,
        rdpSettings* settings, char* name, void* data)
{
	PVIRTUALCHANNELENTRY entry = NULL;
	PVIRTUALCHANNELENTRYEX entryEx = NULL;
	entryEx = (PVIRTUALCHANNELENTRYEX) freerdp_load_channel_addin_entry(name, NULL, NULL,
	          FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX);

	if (!entryEx)
		entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (entryEx)
	{
		if (freerdp_channels_client_load_ex(channels, settings, entryEx, data) == 0)
		{
			WLog_INFO(TAG, "loading channelEx %s", name);
			return TRUE;
		}
	}
	else if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			WLog_INFO(TAG, "loading channel %s", name);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	UINT32 index;
	ADDIN_ARGV* args;

	if ((freerdp_static_channel_collection_find(settings, "rdpsnd")) ||
	    (freerdp_dynamic_channel_collection_find(settings, "tsmf")))
	{
		settings->DeviceRedirection = TRUE; /* rdpsnd requires rdpdr to be registered */
		settings->AudioPlayback =
		    TRUE; /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (freerdp_dynamic_channel_collection_find(settings, "audin"))
	{
		settings->AudioCapture = TRUE;
	}

	if (settings->NetworkAutoDetect ||
	    settings->SupportHeartbeatPdu ||
	    settings->SupportMultitransport)
	{
		settings->DeviceRedirection =
		    TRUE; /* these RDP8 features require rdpdr to be registered */
	}

	if (settings->RedirectDrives || settings->RedirectHomeDrive
	    || settings->RedirectSerialPorts
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

			if (!freerdp_client_add_device_channel(settings, 3, (char**) params))
				return FALSE;
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

			if (!freerdp_client_add_device_channel(settings, 3, (char**) params))
				return FALSE;
		}
	}

	if (settings->DeviceRedirection)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rdpdr",
		        settings))
			return FALSE;

		if (!freerdp_static_channel_collection_find(settings, "rdpsnd"))
		{
			char* params[2];
			params[0] = "rdpsnd";
			params[1] = "sys:fake";

			if (!freerdp_client_add_static_channel(settings, 2, (char**) params))
				return FALSE;
		}
	}

	if (settings->RedirectSmartCards)
	{
		RDPDR_SMARTCARD* smartcard;

		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
		{
			smartcard = (RDPDR_SMARTCARD*) calloc(1, sizeof(RDPDR_SMARTCARD));

			if (!smartcard)
				return FALSE;

			smartcard->Type = RDPDR_DTYP_SMARTCARD;

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) smartcard))
				return FALSE;
		}
	}

	if (settings->RedirectPrinters)
	{
		RDPDR_PRINTER* printer;

		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_PRINT))
		{
			printer = (RDPDR_PRINTER*) calloc(1, sizeof(RDPDR_PRINTER));

			if (!printer)
				return FALSE;

			printer->Type = RDPDR_DTYP_PRINT;

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*) printer))
				return FALSE;
		}
	}

	if (settings->RedirectClipboard)
	{
		char* params[1];
		params[0] = "cliprdr";

		if (!freerdp_client_add_static_channel(settings, 1, (char**) params))
			return FALSE;
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
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "encomsp",
		        settings))
			return FALSE;
	}

	if (settings->RemdeskVirtualChannel)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "remdesk",
		        settings))
			return FALSE;
	}

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		args = settings->StaticChannelArray[index];

		if (!freerdp_client_load_static_channel_addin(channels, settings, args->argv[0],
		        args))
			return FALSE;
	}

	if (settings->RemoteApplicationMode)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rail",
		        settings))
			return FALSE;
	}

	if (settings->MultiTouchInput)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "rdpei";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportGraphicsPipeline)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "rdpgfx";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportEchoChannel)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "echo";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportSSHAgentChannel)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "sshagent";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportDisplayControl)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "disp";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportGeometryTracking)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "geometry";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportVideoOptimized)
	{
		char* p[1];
		int count;
		count = 1;
		p[0] = "video";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->DynamicChannelCount)
		settings->SupportDynamicChannels = TRUE;

	if (settings->SupportDynamicChannels)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "drdynvc",
		        settings))
			return FALSE;
	}

	return TRUE;
}
