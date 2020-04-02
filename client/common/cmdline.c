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
#include <freerdp/channels/urbdrc.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/version.h>

#include "compatibility.h"
#include "cmdline.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.cmdline")

static BOOL freerdp_client_print_codepages(const char* arg)
{
	size_t count = 0, x;
	DWORD column = 2;
	const char* filter = NULL;
	char buffer[80];
	RDP_CODEPAGE* pages;

	if (arg)
		filter = strchr(arg, ',') + 1;
	pages = freerdp_keyboard_get_matching_codepages(column, filter, &count);
	if (!pages)
		return TRUE;

	printf("%-10s %-8s %-60s %-36s %-48s\n", "<id>", "<locale>", "<win langid>", "<language>",
	       "<country>");
	for (x = 0; x < count; x++)
	{
		const RDP_CODEPAGE* page = &pages[x];
		if (strnlen(page->subLanguageSymbol, ARRAYSIZE(page->subLanguageSymbol)) > 0)
			_snprintf(buffer, sizeof(buffer), "[%s|%s]", page->primaryLanguageSymbol,
			          page->subLanguageSymbol);
		else
			_snprintf(buffer, sizeof(buffer), "[%s]", page->primaryLanguageSymbol);
		printf("id=0x%04" PRIx16 ": [%-6s] %-60s %-36s %-48s\n", page->id, page->locale, buffer,
		       page->primaryLanguage, page->subLanguage);
	}
	freerdp_codepages_free(pages);
	return TRUE;
}

static BOOL freerdp_path_valid(const char* path, BOOL* special)
{
	const char DynamicDrives[] = "DynamicDrives";
	BOOL isPath = FALSE;
	BOOL isSpecial;
	if (!path)
		return FALSE;

	isSpecial = (strncmp(path, "*", 2) == 0) ||
	                    (strncmp(path, DynamicDrives, sizeof(DynamicDrives)) == 0) ||
	                    (strncmp(path, "%", 2) == 0)
	                ? TRUE
	                : FALSE;
	if (!isSpecial)
		isPath = PathFileExistsA(path);

	if (special)
		*special = isSpecial;

	return isSpecial || isPath;
}

static BOOL freerdp_sanitize_drive_name(char* name, const char* invalid, const char* replacement)
{
	if (!name || !invalid || !replacement)
		return FALSE;
	if (strlen(invalid) != strlen(replacement))
		return FALSE;

	while (*invalid != '\0')
	{
		const char what = *invalid++;
		const char with = *replacement++;

		char* cur = name;
		while ((cur = strchr(cur, what)) != NULL)
			*cur = with;
	}
	return TRUE;
}

static BOOL freerdp_client_add_drive(rdpSettings* settings, const char* path, const char* name)
{
	RDPDR_DRIVE* drive;

	drive = (RDPDR_DRIVE*)calloc(1, sizeof(RDPDR_DRIVE));

	if (!drive)
		return FALSE;

	drive->Type = RDPDR_DTYP_FILESYSTEM;

	if (name)
	{
		/* Path was entered as secondary argument, swap */
		if (PathFileExistsA(name))
		{
			if (!PathFileExistsA(path) || (!PathIsRelativeA(name) && PathIsRelativeA(path)))
			{
				const char* tmp = path;
				path = name;
				name = tmp;
			}
		}
	}

	if (name)
	{
		if (!(drive->Name = _strdup(name)))
			goto fail;
	}
	else /* We need a name to send to the server. */
	    if (!(drive->Name = _strdup(path)))
		goto fail;

	if (!path || !freerdp_sanitize_drive_name(drive->Name, "\\/", "__"))
		goto fail;
	else
	{
		BOOL isSpecial = FALSE;
		BOOL isPath = freerdp_path_valid(path, &isSpecial);

		if ((!isPath && !isSpecial) || !(drive->Path = _strdup(path)))
			goto fail;
	}

	if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)drive))
		goto fail;

	return TRUE;

fail:
	free(drive->Path);
	free(drive->Name);
	free(drive);
	return FALSE;
}

static BOOL copy_value(const char* value, char** dst)
{
	if (!dst || !value)
		return FALSE;

	free(*dst);
	(*dst) = _strdup(value);
	return (*dst) != NULL;
}

static BOOL append_value(const char* value, char** dst)
{
	size_t x = 0, y;
	char* tmp;
	if (!dst || !value)
		return FALSE;

	if (*dst)
		x = strlen(*dst);
	y = strlen(value);
	tmp = realloc(*dst, x + y + 2);
	if (!tmp)
		return FALSE;
	if (x == 0)
		tmp[0] = '\0';
	else
		strcat(tmp, ",");
	strcat(tmp, value);
	*dst = tmp;
	return TRUE;
}

static BOOL value_to_int(const char* value, LONGLONG* result, LONGLONG min, LONGLONG max)
{
	long long rc;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoi64(value, NULL, 0);

	if (errno != 0)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

static BOOL value_to_uint(const char* value, ULONGLONG* result, ULONGLONG min, ULONGLONG max)
{
	unsigned long long rc;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoui64(value, NULL, 0);

	if (errno != 0)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

BOOL freerdp_client_print_version(void)
{
	printf("This is FreeRDP version %s (%s)\n", FREERDP_VERSION_FULL, GIT_REVISION);
	return TRUE;
}

BOOL freerdp_client_print_buildconfig(void)
{
	printf("%s", freerdp_get_build_config());
	return TRUE;
}

static char* print_token(char* text, size_t start_offset, size_t* current, size_t limit,
                         const char delimiter)
{
	int rc;
	size_t len = strlen(text);

	if (*current < start_offset)
	{
		rc = printf("%*c", (int)(start_offset - *current), ' ');
		if (rc < 0)
			return NULL;
		*current += (size_t)rc;
	}

	if (*current + len > limit)
	{
		size_t x;

		for (x = MIN(len, limit - start_offset); x > 1; x--)
		{
			if (text[x] == delimiter)
			{
				printf("%.*s\n", (int)x, text);
				*current = 0;
				return &text[x];
			}
		}

		return NULL;
	}

	rc = printf("%s", text);
	if (rc < 0)
		return NULL;
	*current += (size_t)rc;
	return NULL;
}

static size_t print_optionals(const char* text, size_t start_offset, size_t current)
{
	const size_t limit = 80;
	char* str = _strdup(text);
	char* cur = print_token(str, start_offset, &current, limit, '[');

	while (cur)
		cur = print_token(cur, start_offset, &current, limit, '[');

	free(str);
	return current;
}

static size_t print_description(const char* text, size_t start_offset, size_t current)
{
	const size_t limit = 80;
	char* str = _strdup(text);
	char* cur = print_token(str, start_offset, &current, limit, ' ');

	while (cur)
	{
		cur++;
		cur = print_token(cur, start_offset, &current, limit, ' ');
	}

	free(str);
	current += (size_t)printf("\n");
	return current;
}

static void freerdp_client_print_command_line_args(COMMAND_LINE_ARGUMENT_A* arg)
{
	if (!arg)
		return;

	do
	{
		int rc;
		size_t pos = 0;
		const size_t description_offset = 30 + 8;

		if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
			rc = printf("    %s%s", arg->Default ? "-" : "+", arg->Name);
		else
			rc = printf("    /%s", arg->Name);

		if (rc < 0)
			return;
		pos += (size_t)rc;

		if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) ||
		    (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			if (arg->Format)
			{
				if (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL)
				{
					rc = printf("[:");
					if (rc < 0)
						return;
					pos += (size_t)rc;
					pos = print_optionals(arg->Format, pos, pos);
					rc = printf("]");
					if (rc < 0)
						return;
					pos += (size_t)rc;
				}
				else
				{
					rc = printf(":");
					if (rc < 0)
						return;
					pos += (size_t)rc;
					pos = print_optionals(arg->Format, pos, pos);
				}

				if (pos > description_offset)
				{
					printf("\n");
					pos = 0;
				}
			}
		}

		rc = printf("%*c", (int)(description_offset - pos), ' ');
		if (rc < 0)
			return;
		pos += (size_t)rc;

		if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			rc = printf("%s ", arg->Default ? "Disable" : "Enable");
			if (rc < 0)
				return;
			pos += (size_t)rc;
		}

		print_description(arg->Text, description_offset, pos);
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

BOOL freerdp_client_print_command_line_help(int argc, char** argv)
{
	return freerdp_client_print_command_line_help_ex(argc, argv, NULL);
}

BOOL freerdp_client_print_command_line_help_ex(int argc, char** argv,
                                               COMMAND_LINE_ARGUMENT_A* custom)
{
	const char* name = "FreeRDP";
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
	memcpy(largs, args, sizeof(args));

	if (argc > 0)
		name = argv[0];

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
	freerdp_client_print_command_line_args(largs);
	printf("\n");
	printf("Examples:\n");
	printf("    %s connection.rdp /p:Pwd123! /f\n", name);
	printf("    %s /u:CONTOSO\\JohnDoe /p:Pwd123! /v:rdp.contoso.com\n", name);
	printf("    %s /u:JohnDoe /p:Pwd123! /w:1366 /h:768 /v:192.168.1.100:4489\n", name);
	printf("    %s /u:JohnDoe /p:Pwd123! /vmconnect:C824F53E-95D2-46C6-9A18-23A5BB403532 "
	       "/v:192.168.1.100\n",
	       name);
	printf("\n");
	printf("Clipboard Redirection: +clipboard\n");
	printf("\n");
	printf("Drive Redirection: /drive:home,/home/user\n");
	printf("Smartcard Redirection: /smartcard:<device>\n");
	printf("Serial Port Redirection: /serial:<name>,<device>,[SerCx2|SerCx|Serial],[permissive]\n");
	printf("Serial Port Redirection: /serial:COM1,/dev/ttyS0\n");
	printf("Parallel Port Redirection: /parallel:<name>,<device>\n");
	printf("Printer Redirection: /printer:<device>,<driver>\n");
	printf("TCP redirection: /rdp2tcp:/usr/bin/rdp2tcp\n");
	printf("\n");
	printf("Audio Output Redirection: /sound:sys:oss,dev:1,format:1\n");
	printf("Audio Output Redirection: /sound:sys:alsa\n");
	printf("Audio Input Redirection: /microphone:sys:oss,dev:1,format:1\n");
	printf("Audio Input Redirection: /microphone:sys:alsa\n");
	printf("\n");
	printf("Multimedia Redirection: /video\n");
#ifdef CHANNEL_URBDRC_CLIENT
	printf("USB Device Redirection: /usb:id,dev:054c:0268\n");
#endif
	printf("\n");
	printf("For Gateways, the https_proxy environment variable is respected:\n");
#ifdef _WIN32
	printf("    set HTTPS_PROXY=http://proxy.contoso.com:3128/\n");
#else
	printf("    export https_proxy=http://proxy.contoso.com:3128/\n");
#endif
	printf("    %s /g:rdp.contoso.com ...\n", name);
	printf("\n");
	printf("More documentation is coming, in the meantime consult source files\n");
	printf("\n");
	return TRUE;
}

static int freerdp_client_command_line_pre_filter(void* context, int index, int argc, LPSTR* argv)
{
	if (index == 1)
	{
		size_t length;
		rdpSettings* settings;

		if (argc <= index)
			return -1;

		length = strlen(argv[index]);

		if (length > 4)
		{
			if (_stricmp(&(argv[index])[length - 4], ".rdp") == 0)
			{
				settings = (rdpSettings*)context;

				if (!copy_value(argv[index], &settings->ConnectionFile))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}

		if (length > 13)
		{
			if (_stricmp(&(argv[index])[length - 13], ".msrcIncident") == 0)
			{
				settings = (rdpSettings*)context;

				if (!copy_value(argv[index], &settings->AssistanceFile))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}
	}

	return 0;
}

BOOL freerdp_client_add_device_channel(rdpSettings* settings, size_t count, char** params)
{
	if (strcmp(params[0], "drive") == 0)
	{
		BOOL rc;
		if (count < 2)
			return FALSE;

		settings->DeviceRedirection = TRUE;
		if (count < 3)
			rc = freerdp_client_add_drive(settings, params[1], NULL);
		else
			rc = freerdp_client_add_drive(settings, params[2], params[1]);

		return rc;
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
			printer = (RDPDR_PRINTER*)calloc(1, sizeof(RDPDR_PRINTER));

			if (!printer)
				return FALSE;

			printer->Type = RDPDR_DTYP_PRINT;

			if (!(printer->Name = _strdup(params[1])))
			{
				free(printer);
				return FALSE;
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

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)printer))
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
		smartcard = (RDPDR_SMARTCARD*)calloc(1, sizeof(RDPDR_SMARTCARD));

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

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)smartcard))
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
		serial = (RDPDR_SERIAL*)calloc(1, sizeof(RDPDR_SERIAL));

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

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)serial))
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
		parallel = (RDPDR_PARALLEL*)calloc(1, sizeof(RDPDR_PARALLEL));

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

		if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)parallel))
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

BOOL freerdp_client_add_static_channel(rdpSettings* settings, size_t count, char** params)
{
	int index;
	ADDIN_ARGV* args;

	if (!settings || !params || !params[0] || (count > INT_MAX))
		return FALSE;

	if (freerdp_static_channel_collection_find(settings, params[0]))
		return TRUE;

	args = (ADDIN_ARGV*)calloc(1, sizeof(ADDIN_ARGV));

	if (!args)
		return FALSE;

	args->argc = (int)count;
	args->argv = (char**)calloc((size_t)args->argc, sizeof(char*));

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

BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, size_t count, char** params)
{
	int index;
	ADDIN_ARGV* args;

	if (!settings || !params || !params[0] || (count > INT_MAX))
		return FALSE;

	if (freerdp_dynamic_channel_collection_find(settings, params[0]))
		return TRUE;

	args = (ADDIN_ARGV*)malloc(sizeof(ADDIN_ARGV));

	if (!args)
		return FALSE;

	args->argc = (int)count;
	args->argv = (char**)calloc((size_t)args->argc, sizeof(char*));

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

static int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings = (rdpSettings*)context;
	BOOL status = TRUE;
	BOOL enable = arg->Value ? TRUE : FALSE;
	CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "a")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

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
		p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		status = freerdp_client_add_static_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "drive")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "serial")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "parallel")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "smartcard")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "printer")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "usb")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx(URBDRC_CHANNEL_NAME, arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
	CommandLineSwitchCase(arg, "multitouch")
	{
		settings->MultiTouchInput = enable;
	}
	CommandLineSwitchCase(arg, "gestures")
	{
		settings->MultiTouchGestures = enable;
	}
	CommandLineSwitchCase(arg, "echo")
	{
		settings->SupportEchoChannel = enable;
	}
	CommandLineSwitchCase(arg, "ssh-agent")
	{
		settings->SupportSSHAgentChannel = enable;
	}
	CommandLineSwitchCase(arg, "disp")
	{
		settings->SupportDisplayControl = enable;
	}
	CommandLineSwitchCase(arg, "geometry")
	{
		settings->SupportGeometryTracking = enable;
	}
	CommandLineSwitchCase(arg, "video")
	{
		settings->SupportGeometryTracking = enable; /* this requires geometry tracking */
		settings->SupportVideoOptimized = enable;
	}
	CommandLineSwitchCase(arg, "sound")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx("rdpsnd", arg->Value, &count);
		status = freerdp_client_add_static_channel(settings, count, p);
		if (status)
		{
			status = freerdp_client_add_dynamic_channel(settings, count, p);
		}
		free(p);
	}
	CommandLineSwitchCase(arg, "microphone")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx("audin", arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
#if defined(CHANNEL_TSMF_CLIENT)
	CommandLineSwitchCase(arg, "multimedia")
	{
		char** p;
		size_t count;
		p = CommandLineParseCommaSeparatedValuesEx("tsmf", arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, p);
		free(p);
	}
#endif
	CommandLineSwitchCase(arg, "heartbeat")
	{
		settings->SupportHeartbeatPdu = enable;
	}
	CommandLineSwitchCase(arg, "multitransport")
	{
		settings->SupportMultitransport = enable;

		if (settings->SupportMultitransport)
			settings->MultitransportFlags =
			    (TRANSPORT_TYPE_UDP_FECR | TRANSPORT_TYPE_UDP_FECL | TRANSPORT_TYPE_UDP_PREFERRED);
		else
			settings->MultitransportFlags = 0;
	}
	CommandLineSwitchCase(arg, "password-is-pin")
	{
		settings->PasswordIsSmartcardPin = enable;
	}
	CommandLineSwitchEnd(arg) return status ? 1 : -1;
}

BOOL freerdp_parse_username(const char* username, char** user, char** domain)
{
	char* p;
	size_t length = 0;
	p = strchr(username, '\\');
	*user = NULL;
	*domain = NULL;

	if (p)
	{
		length = (size_t)(p - username);
		*user = _strdup(&p[1]);

		if (!*user)
			return FALSE;

		*domain = (char*)calloc(length + 1UL, sizeof(char));

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
		size_t length = (size_t)(p - hostname);
		LONGLONG val;

		if (!value_to_int(p + 1, &val, 1, UINT16_MAX))
			return FALSE;

		*host = (char*)calloc(length + 1UL, sizeof(char));

		if (!(*host))
			return FALSE;

		CopyMemory(*host, hostname, length);
		(*host)[length] = '\0';
		*port = (UINT16)val;
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

BOOL freerdp_set_connection_type(rdpSettings* settings, UINT32 type)
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

		/* Automatically activate GFX and RFX codec support */
#ifdef WITH_GFX_H264
		settings->GfxAVC444 = TRUE;
		settings->GfxH264 = TRUE;
#endif
		settings->RemoteFxCodec = TRUE;
		settings->SupportGraphicsPipeline = TRUE;
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static int freerdp_map_keyboard_layout_name_to_id(char* name)
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
			id = (int)layouts[i].code;
	}

	freerdp_keyboard_layouts_free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);

	if (!layouts)
		return -1;

	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = (int)layouts[i].code;
	}

	freerdp_keyboard_layouts_free(layouts);

	if (id)
		return id;

	layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);

	if (!layouts)
		return -1;

	for (i = 0; layouts[i].code; i++)
	{
		if (_stricmp(layouts[i].name, name) == 0)
			id = (int)layouts[i].code;
	}

	freerdp_keyboard_layouts_free(layouts);

	if (id)
		return id;

	return 0;
}

static int freerdp_detect_command_line_pre_filter(void* context, int index, int argc, LPSTR* argv)
{
	size_t length;
	WINPR_UNUSED(context);

	if (index == 1)
	{
		if (argc < index)
			return -1;

		length = strlen(argv[index]);

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

static int freerdp_detect_windows_style_command_line_syntax(int argc, char** argv, size_t* count,
                                                            BOOL ignoreUnknown)
{
	int status;
	DWORD flags;
	int detect_status;
	COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
	memcpy(largs, args, sizeof(args));

	flags = COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_SILENCE_PARSER;
	flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;

	if (ignoreUnknown)
	{
		flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	}

	*count = 0;
	detect_status = 0;
	CommandLineClearArgumentsA(largs);
	status = CommandLineParseArgumentsA(argc, argv, largs, flags, NULL,
	                                    freerdp_detect_command_line_pre_filter, NULL);

	if (status < 0)
		return status;

	arg = largs;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		(*count)++;
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((status <= COMMAND_LINE_ERROR) && (status >= COMMAND_LINE_ERROR_LAST))
		detect_status = -1;

	return detect_status;
}

static int freerdp_detect_posix_style_command_line_syntax(int argc, char** argv, size_t* count,
                                                          BOOL ignoreUnknown)
{
	int status;
	DWORD flags;
	int detect_status;
	COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
	memcpy(largs, args, sizeof(args));

	flags = COMMAND_LINE_SEPARATOR_SPACE | COMMAND_LINE_SILENCE_PARSER;
	flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;

	if (ignoreUnknown)
	{
		flags |= COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	}

	*count = 0;
	detect_status = 0;
	CommandLineClearArgumentsA(largs);
	status = CommandLineParseArgumentsA(argc, argv, largs, flags, NULL,
	                                    freerdp_detect_command_line_pre_filter, NULL);

	if (status < 0)
		return status;

	arg = largs;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		(*count)++;
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if ((status <= COMMAND_LINE_ERROR) && (status >= COMMAND_LINE_ERROR_LAST))
		detect_status = -1;

	return detect_status;
}

static BOOL freerdp_client_detect_command_line(int argc, char** argv, DWORD* flags)
{
	int old_cli_status;
	size_t old_cli_count;
	int posix_cli_status;
	size_t posix_cli_count;
	int windows_cli_status;
	size_t windows_cli_count;
	BOOL compatibility = FALSE;
	const BOOL ignoreUnknown = TRUE;
	windows_cli_status = freerdp_detect_windows_style_command_line_syntax(
	    argc, argv, &windows_cli_count, ignoreUnknown);
	posix_cli_status =
	    freerdp_detect_posix_style_command_line_syntax(argc, argv, &posix_cli_count, ignoreUnknown);
	old_cli_status = freerdp_detect_old_command_line_syntax(argc, argv, &old_cli_count);
	/* Default is POSIX syntax */
	*flags = COMMAND_LINE_SEPARATOR_SPACE;
	*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	*flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;

	if (posix_cli_status <= COMMAND_LINE_STATUS_PRINT)
		return compatibility;

	/* Check, if this may be windows style syntax... */
	if ((windows_cli_count && (windows_cli_count >= posix_cli_count)) ||
	    (windows_cli_status <= COMMAND_LINE_STATUS_PRINT))
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

	WLog_DBG(TAG, "windows: %d/%d posix: %d/%d compat: %d/%d", windows_cli_status,
	         windows_cli_count, posix_cli_status, posix_cli_count, old_cli_status, old_cli_count);
	return compatibility;
}

int freerdp_client_settings_command_line_status_print(rdpSettings* settings, int status, int argc,
                                                      char** argv)
{
	return freerdp_client_settings_command_line_status_print_ex(settings, status, argc, argv, NULL);
}

int freerdp_client_settings_command_line_status_print_ex(rdpSettings* settings, int status,
                                                         int argc, char** argv,
                                                         COMMAND_LINE_ARGUMENT_A* custom)
{
	COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
	memcpy(largs, args, sizeof(args));

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
		COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
		memcpy(largs, args, sizeof(largs));
		CommandLineParseArgumentsA(argc, argv, largs, 0x112, NULL, NULL, NULL);

		arg = CommandLineFindArgumentA(largs, "kbd-lang-list");

		if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
		{
			freerdp_client_print_codepages(arg->Value);
		}

		arg = CommandLineFindArgumentA(largs, "kbd-list");

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			DWORD i;
			RDP_KEYBOARD_LAYOUT* layouts;
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
			// if (!layouts) /* FIXME*/
			printf("\nKeyboard Layouts\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08" PRIX32 "\t%s\n", layouts[i].code, layouts[i].name);

			freerdp_keyboard_layouts_free(layouts);
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
			// if (!layouts) /* FIXME*/
			printf("\nKeyboard Layout Variants\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08" PRIX32 "\t%s\n", layouts[i].code, layouts[i].name);

			freerdp_keyboard_layouts_free(layouts);
			layouts = freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);
			// if (!layouts) /* FIXME*/
			printf("\nKeyboard Input Method Editors (IMEs)\n");

			for (i = 0; layouts[i].code; i++)
				printf("0x%08" PRIX32 "\t%s\n", layouts[i].code, layouts[i].name);

			freerdp_keyboard_layouts_free(layouts);
			printf("\n");
		}

		arg = CommandLineFindArgumentA(largs, "monitor-list");

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

	return _strnicmp(&str[strLen - extLen], ext, extLen) == 0;
}

static void activate_smartcard_logon_rdp(rdpSettings* settings)
{
	settings->SmartcardLogon = TRUE;
	/* TODO: why not? settings->UseRdpSecurityLayer = TRUE; */
	freerdp_settings_set_bool(settings, FreeRDP_PasswordIsSmartcardPin, TRUE);
}

/**
 * parses a string value with the format <v1>x<v2>
 * @param input: input string
 * @param v1: pointer to output v1
 * @param v2: pointer to output v2
 * @return if the parsing was successful
 */
static BOOL parseSizeValue(const char* input, unsigned long* v1, unsigned long* v2)
{
	const char* xcharpos;
	char* endPtr;
	unsigned long v;
	errno = 0;
	v = strtoul(input, &endPtr, 10);

	if ((v == 0 || v == ULONG_MAX) && (errno != 0))
		return FALSE;

	if (v1)
		*v1 = v;

	xcharpos = strchr(input, 'x');

	if (!xcharpos || xcharpos != endPtr)
		return FALSE;

	errno = 0;
	v = strtoul(xcharpos + 1, &endPtr, 10);

	if ((v == 0 || v == ULONG_MAX) && (errno != 0))
		return FALSE;

	if (*endPtr != '\0')
		return FALSE;

	if (v2)
		*v2 = v;

	return TRUE;
}

int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings, int argc,
                                                         char** argv, BOOL allowUnknown)
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
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(args)];
	memcpy(largs, args, sizeof(args));

	/* Command line detection fails if only a .rdp or .msrcIncident file
	 * is supplied. Check this case first, only then try to detect
	 * legacy command line syntax. */
	if (argc > 1)
	{
		ext = ends_with(argv[1], ".rdp");
		assist = ends_with(argv[1], ".msrcIncident");
	}

	if (!ext && !assist)
		compatibility = freerdp_client_detect_command_line(argc, argv, &flags);
	else
		compatibility = freerdp_client_detect_command_line(argc - 1, &argv[1], &flags);

	settings->ProxyHostname = NULL;
	settings->ProxyUsername = NULL;
	settings->ProxyPassword = NULL;

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
			if (freerdp_client_settings_parse_assistance_file(settings, argc, argv) < 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		CommandLineClearArgumentsA(largs);
		status = CommandLineParseArgumentsA(argc, argv, largs, flags, settings,
		                                    freerdp_client_command_line_pre_filter,
		                                    freerdp_client_command_line_post_filter);

		if (status < 0)
			return status;
	}

	CommandLineFindArgumentA(largs, "v");
	arg = largs;
	errno = 0;

	do
	{
		BOOL enable = arg->Value ? TRUE : FALSE;

		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "v")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			free(settings->ServerHostname);
			settings->ServerHostname = NULL;
			p = strchr(arg->Value, '[');

			/* ipv4 */
			if (!p)
			{
				p = strchr(arg->Value, ':');

				if (p)
				{
					LONGLONG val;

					if (!value_to_int(&p[1], &val, 1, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (size_t)(p - arg->Value);
					settings->ServerPort = (UINT16)val;

					if (!(settings->ServerHostname = (char*)calloc(length + 1UL, sizeof(char))))
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

				length = (size_t)(p2 - p);

				if (!(settings->ServerHostname = (char*)calloc(length, sizeof(char))))
					return COMMAND_LINE_ERROR_MEMORY;

				strncpy(settings->ServerHostname, p + 1, length - 1);

				if (*(p2 + 1) == ':')
				{
					LONGLONG val;

					if (!value_to_int(&p[2], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					settings->ServerPort = (UINT16)val;
				}

				printf("hostname %s port %" PRIu32 "\n", settings->ServerHostname,
				       settings->ServerPort);
			}
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			if (!copy_value(arg->Value, &settings->AuthenticationServiceClass))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "redirect-prefer")
		{
			size_t count = 0;
			char* cur = arg->Value;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			settings->RedirectionPreferType = 0;

			do
			{
				UINT32 mask;
				char* next = strchr(cur, ',');

				if (next)
				{
					*next = '\0';
					next++;
				}

				if (_strnicmp(cur, "fqdn", 5) == 0)
					mask = 0x06U;
				else if (_strnicmp(cur, "ip", 3) == 0)
					mask = 0x05U;
				else if (_strnicmp(cur, "netbios", 8) == 0)
					mask = 0x03U;
				else
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				cur = next;
				mask = (mask & 0x07);
				settings->RedirectionPreferType |= mask << (count * 3);
				count++;
			} while (cur != NULL);

			if (count > 3)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "credentials-delegation")
		{
			settings->DisableCredentialsDelegation = !enable;
		}
		CommandLineSwitchCase(arg, "vmconnect")
		{
			settings->VmConnectMode = TRUE;
			settings->ServerPort = 2179;
			settings->NegotiateSecurityLayer = FALSE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				settings->SendPreconnectionPdu = TRUE;

				if (!copy_value(arg->Value, &settings->PreconnectionBlob))
					return COMMAND_LINE_ERROR_MEMORY;
			}
		}
		CommandLineSwitchCase(arg, "w")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopWidth = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "h")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopHeight = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "size")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			p = strchr(arg->Value, 'x');

			if (p)
			{
				unsigned long w, h;

				if (!parseSizeValue(arg->Value, &w, &h) || (w > UINT16_MAX) || (h > UINT16_MAX))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				settings->DesktopWidth = (UINT32)w;
				settings->DesktopHeight = (UINT32)h;
			}
			else
			{
				if (!(str = _strdup(arg->Value)))
					return COMMAND_LINE_ERROR_MEMORY;

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
						LONGLONG val;

						if (!value_to_int(str, &val, 0, 100))
						{
							free(str);
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}

						settings->PercentScreen = (UINT32)val;
					}
				}

				free(str);
			}
		}
		CommandLineSwitchCase(arg, "f")
		{
			settings->Fullscreen = enable;
		}
		CommandLineSwitchCase(arg, "suppress-output")
		{
			settings->SuppressOutput = enable;
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
			settings->SpanMonitors = enable;
		}
		CommandLineSwitchCase(arg, "workarea")
		{
			settings->Workarea = enable;
		}
		CommandLineSwitchCase(arg, "monitors")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				char** p;
				size_t count = 0;
				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				if (!p)
					return COMMAND_LINE_ERROR_MEMORY;

				if (count > 16)
					count = 16;

				settings->NumMonitorIds = (UINT32)count;

				for (i = 0; i < settings->NumMonitorIds; i++)
				{
					LONGLONG val;

					if (!value_to_int(p[i], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					settings->MonitorIds[i] = (UINT32)val;
				}

				free(p);
			}
		}
		CommandLineSwitchCase(arg, "monitor-list")
		{
			settings->ListMonitors = enable;
		}
		CommandLineSwitchCase(arg, "t")
		{
			if (!copy_value(arg->Value, &settings->WindowTitle))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			settings->Decorations = enable;
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
				unsigned long w, h;

				if (!parseSizeValue(arg->Value, &w, &h) || (w > UINT16_MAX) || (h > UINT16_MAX))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				settings->SmartSizingWidth = (UINT32)w;
				settings->SmartSizingHeight = (UINT32)h;
			}
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			switch (settings->ColorDepth)
			{
				case 32:
				case 24:
				case 16:
				case 15:
				case 8:
					settings->ColorDepth = (UINT32)val;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "admin")
		{
			settings->ConsoleSession = enable;
		}
		CommandLineSwitchCase(arg, "relax-order-checks")
		{
			settings->AllowUnanouncedOrdersFromServer = enable;
		}
		CommandLineSwitchCase(arg, "restricted-admin")
		{
			settings->ConsoleSession = enable;
			settings->RestrictedAdminModeRequired = enable;
		}
		CommandLineSwitchCase(arg, "pth")
		{
			settings->ConsoleSession = TRUE;
			settings->RestrictedAdminModeRequired = TRUE;

			if (!copy_value(arg->Value, &settings->PasswordHash))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "client-hostname")
		{
			if (!copy_value(arg->Value, &settings->ClientHostname))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "kbd")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 1, UINT32_MAX))
			{
				const int rc = freerdp_map_keyboard_layout_name_to_id(arg->Value);

				if (rc <= 0)
				{
					WLog_ERR(TAG, "Could not identify keyboard layout: %s", arg->Value);
					WLog_ERR(TAG, "Use /kbd-list to list available layouts");
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}

				/* Found a valid mapping, reset errno */
				val = rc;
				errno = 0;
			}

			settings->KeyboardLayout = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-lang")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 1, UINT32_MAX))
			{
				WLog_ERR(TAG, "Could not identify keyboard active language %s", arg->Value);
				WLog_ERR(TAG, "Use /kbd-lang-list to list available layouts");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			settings->KeyboardCodePage = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardType = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardSubType = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardFunctionKey = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "u")
		{
			user = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			if (!copy_value(arg->Value, &settings->Domain))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "p")
		{
			if (!copy_value(arg->Value, &settings->Password))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "g")
		{
			free(settings->GatewayHostname);

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (!arg->Value)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				p = strchr(arg->Value, ':');

				if (p)
				{
					size_t s;
					LONGLONG val;

					if (!value_to_int(&p[1], &val, 0, UINT32_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					s = (size_t)(p - arg->Value);
					settings->GatewayPort = (UINT32)val;

					if (!(settings->GatewayHostname = (char*)calloc(s + 1UL, sizeof(char))))
						return COMMAND_LINE_ERROR_MEMORY;

					strncpy(settings->GatewayHostname, arg->Value, s);
					settings->GatewayHostname[s] = '\0';
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
			/* initial value */
			settings->ProxyType = PROXY_TYPE_HTTP;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				char* atPtr;
				if (!arg->Value)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				/* value is [scheme://][user:password@]hostname:port */
				p = strstr(arg->Value, "://");

				if (p)
				{
					*p = '\0';

					if (_stricmp("no_proxy", arg->Value) == 0)
						settings->ProxyType = PROXY_TYPE_IGNORE;

					if (_stricmp("http", arg->Value) == 0)
						settings->ProxyType = PROXY_TYPE_HTTP;
					else if (_stricmp("socks5", arg->Value) == 0)
						settings->ProxyType = PROXY_TYPE_SOCKS;
					else
					{
						WLog_ERR(TAG, "Only HTTP and SOCKS5 proxies supported by now");
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					arg->Value = p + 3;
				}

				/* arg->Value is now [user:password@]hostname:port */
				atPtr = strrchr(arg->Value, '@');

				if (atPtr)
				{
					/* got a login / password,
					 *               atPtr
					 *               v
					 * [user:password@]hostname:port
					 *      ^
					 *      colonPtr
					 */
					char* colonPtr = strchr(arg->Value, ':');

					if (!colonPtr || (colonPtr > atPtr))
					{
						WLog_ERR(
						    TAG,
						    "invalid syntax for proxy, expected syntax is user:password@host:port");
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					*colonPtr = '\0';
					settings->ProxyUsername = _strdup(arg->Value);

					if (!settings->ProxyUsername)
					{
						WLog_ERR(TAG, "unable to allocate proxy username");
						return COMMAND_LINE_ERROR_MEMORY;
					}

					*atPtr = '\0';
					settings->ProxyPassword = _strdup(colonPtr + 1);

					if (!settings->ProxyPassword)
					{
						WLog_ERR(TAG, "unable to allocate proxy password");
						return COMMAND_LINE_ERROR_MEMORY;
					}

					arg->Value = atPtr + 1;
				}

				p = strchr(arg->Value, ':');

				if (p)
				{
					LONGLONG val;

					if (!value_to_int(&p[1], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (size_t)(p - arg->Value);
					settings->ProxyPort = (UINT16)val;
					settings->ProxyHostname = (char*)malloc(length + 1);
					strncpy(settings->ProxyHostname, arg->Value, length);
					settings->ProxyHostname[length] = '\0';
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
			if (!copy_value(arg->Value, &settings->GatewayDomain))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gp")
		{
			if (!copy_value(arg->Value, &settings->GatewayPassword))
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
			if (!copy_value(arg->Value, &settings->GatewayAccessToken))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "gateway-usage-method")
		{
			UINT32 type = 0;

			if (_stricmp(arg->Value, "none") == 0)
				type = TSC_PROXY_MODE_NONE_DIRECT;
			else if (_stricmp(arg->Value, "direct") == 0)
				type = TSC_PROXY_MODE_DIRECT;
			else if (_stricmp(arg->Value, "detect") == 0)
				type = TSC_PROXY_MODE_DETECT;
			else if (_stricmp(arg->Value, "default") == 0)
				type = TSC_PROXY_MODE_DEFAULT;
			else
			{
				LONGLONG val;

				if (!value_to_int(arg->Value, &val, TSC_PROXY_MODE_NONE_DIRECT,
				                  TSC_PROXY_MODE_NONE_DETECT))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			freerdp_set_gateway_usage_method(settings, type);
		}
		CommandLineSwitchCase(arg, "app")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationProgram))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->RemoteApplicationMode = TRUE;
			settings->RemoteAppLanguageBarSupported = TRUE;
			settings->Workarea = TRUE;
			settings->DisableWallpaper = TRUE;
			settings->DisableFullWindowDrag = TRUE;
		}
		CommandLineSwitchCase(arg, "app-workdir")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationWorkingDir))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			if (!copy_value(arg->Value, (char**)&settings->LoadBalanceInfo))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->LoadBalanceInfoLength = (UINT32)strlen((char*)settings->LoadBalanceInfo);
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationName))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationIcon))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationCmdLine))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationFile))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			if (!copy_value(arg->Value, &settings->RemoteApplicationGuid))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "compression")
		{
			settings->CompressionEnabled = enable;
		}
		CommandLineSwitchCase(arg, "compression-level")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->CompressionLevel = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "drives")
		{
			settings->RedirectDrives = enable;
		}
		CommandLineSwitchCase(arg, "home-drive")
		{
			settings->RedirectHomeDrive = enable;
		}
		CommandLineSwitchCase(arg, "ipv6")
		{
			settings->PreferIPv6OverIPv4 = enable;
		}
		CommandLineSwitchCase(arg, "clipboard")
		{
			settings->RedirectClipboard = enable;
		}
		CommandLineSwitchCase(arg, "shell")
		{
			if (!copy_value(arg->Value, &settings->AlternateShell))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "shell-dir")
		{
			if (!copy_value(arg->Value, &settings->ShellWorkingDirectory))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "audio-mode")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			switch (val)
			{
				case AUDIO_MODE_REDIRECT:
					settings->AudioPlayback = TRUE;
					break;

				case AUDIO_MODE_PLAY_ON_SERVER:
					settings->RemoteConsoleAudio = TRUE;
					break;

				case AUDIO_MODE_NONE:
					settings->AudioPlayback = FALSE;
					settings->RemoteConsoleAudio = FALSE;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "network")
		{
			UINT32 type = 0;

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
			         (_stricmp(arg->Value, "auto") == 0) || (_stricmp(arg->Value, "detect") == 0))
			{
				type = CONNECTION_TYPE_AUTODETECT;
			}
			else
			{
				LONGLONG val;

				if (!value_to_int(arg->Value, &val, 1, 7))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				type = (UINT32)val;
			}

			if (!freerdp_set_connection_type(settings, type))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "fonts")
		{
			settings->AllowFontSmoothing = enable;
		}
		CommandLineSwitchCase(arg, "wallpaper")
		{
			settings->DisableWallpaper = !enable;
		}
		CommandLineSwitchCase(arg, "window-drag")
		{
			settings->DisableFullWindowDrag = !enable;
		}
		CommandLineSwitchCase(arg, "window-position")
		{
			unsigned long x, y;

			if (!arg->Value)
				return COMMAND_LINE_ERROR_MISSING_ARGUMENT;

			if (!parseSizeValue(arg->Value, &x, &y) || x > UINT16_MAX || y > UINT16_MAX)
			{
				WLog_ERR(TAG, "invalid window-position argument");
				return COMMAND_LINE_ERROR_MISSING_ARGUMENT;
			}

			settings->DesktopPosX = (UINT32)x;
			settings->DesktopPosY = (UINT32)y;
		}
		CommandLineSwitchCase(arg, "menu-anims")
		{
			settings->DisableMenuAnims = !enable;
		}
		CommandLineSwitchCase(arg, "themes")
		{
			settings->DisableThemes = !enable;
		}
		CommandLineSwitchCase(arg, "timeout")
		{
			ULONGLONG val;
			if (!value_to_uint(arg->Value, &val, 1, 600000))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			settings->TcpAckTimeout = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "aero")
		{
			settings->AllowDesktopComposition = enable;
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
				int rc = CHANNEL_RC_OK;
				char** p;
				size_t count, x;

				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
				if (!p || (count == 0))
					rc = COMMAND_LINE_ERROR;
				else
				{
					for (x = 0; x < count; x++)
					{
						const char* val = p[x];
#ifdef WITH_GFX_H264
						if (_strnicmp("AVC444", val, 7) == 0)
						{
							settings->GfxH264 = TRUE;
							settings->GfxAVC444 = TRUE;
						}
						else if (_strnicmp("AVC420", val, 7) == 0)
						{
							settings->GfxH264 = TRUE;
							settings->GfxAVC444 = FALSE;
						}
						else
#endif
						    if (_strnicmp("RFX", val, 4) == 0)
						{
							settings->GfxAVC444 = FALSE;
							settings->GfxH264 = FALSE;
							settings->RemoteFxCodec = TRUE;
						}
						else if (_strnicmp("mask:", val, 5) == 0)
						{
							ULONGLONG v;
							const char* uv = &val[5];
							if (!value_to_uint(uv, &v, 0, UINT32_MAX))
								rc = COMMAND_LINE_ERROR;
							else
								settings->GfxCapsFilter = (UINT32)v;
						}
						else
							rc = COMMAND_LINE_ERROR;
					}
				}
				free(p);
				if (rc != CHANNEL_RC_OK)
					return rc;
			}
		}
		CommandLineSwitchCase(arg, "gfx-thin-client")
		{
			settings->GfxThinClient = enable;

			if (settings->GfxThinClient)
				settings->GfxSmallCache = TRUE;

			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-small-cache")
		{
			settings->GfxSmallCache = enable;

			if (enable)
				settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			settings->GfxProgressive = enable;
			settings->GfxThinClient = !enable;

			if (enable)
				settings->SupportGraphicsPipeline = TRUE;
		}
#ifdef WITH_GFX_H264
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			settings->SupportGraphicsPipeline = TRUE;
			settings->GfxH264 = TRUE;

			if (arg->Value)
			{
				int rc = CHANNEL_RC_OK;
				char** p;
				size_t count, x;

				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
				if (!p || (count == 0))
					rc = COMMAND_LINE_ERROR;
				else
				{
					for (x = 0; x < count; x++)
					{
						const char* val = p[x];

						if (_strnicmp("AVC444", val, 7) == 0)
						{
							settings->GfxH264 = TRUE;
							settings->GfxAVC444 = TRUE;
						}
						else if (_strnicmp("AVC420", val, 7) == 0)
						{
							settings->GfxH264 = TRUE;
							settings->GfxAVC444 = FALSE;
						}
						else if (_strnicmp("mask:", val, 5) == 0)
						{
							ULONGLONG v;
							const char* uv = &val[5];
							if (!value_to_uint(uv, &v, 0, UINT32_MAX))
								rc = COMMAND_LINE_ERROR;
							else
								settings->GfxCapsFilter = (UINT32)v;
						}
						else
							rc = COMMAND_LINE_ERROR;
					}
				}
				free(p);
				if (rc != CHANNEL_RC_OK)
					return rc;
			}
		}
#endif
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = enable;
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (strcmp(arg->Value, "video") == 0)
				settings->RemoteFxCodecMode = 0x00;
			else if (strcmp(arg->Value, "image") == 0)
				settings->RemoteFxCodecMode = 0x02;
		}
		CommandLineSwitchCase(arg, "frame-ack")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->FrameAcknowledge = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			settings->NSCodec = enable;
		}
#if defined(WITH_JPEG)
		CommandLineSwitchCase(arg, "jpeg")
		{
			settings->JpegCodec = enable;
			settings->JpegQuality = 75;
		}
		CommandLineSwitchCase(arg, "jpeg-quality")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 100))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->JpegQuality = (UINT32)val;
		}
#endif
		CommandLineSwitchCase(arg, "nego")
		{
			settings->NegotiateSecurityLayer = enable;
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			settings->SendPreconnectionPdu = TRUE;

			if (!copy_value(arg->Value, &settings->PreconnectionBlob))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->SendPreconnectionPdu = TRUE;
			settings->PreconnectionId = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

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
				if (!arg->Value)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				promptForPassword = (_strnicmp(arg->Value, "force", 6) == 0);

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
			settings->RdpSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			settings->TlsSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			settings->NlaSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			settings->ExtSecurity = enable;
		}
		CommandLineSwitchCase(arg, "tls-ciphers")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
		CommandLineSwitchCase(arg, "tls-seclevel")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 5))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->TlsSecLevel = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "cert")
		{
			int rc = 0;
			char** p;
			size_t count, x;
			p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
			for (x = 0; (x < count) && (rc == 0); x++)
			{
				const char deny[] = "deny";
				const char ignore[] = "ignore";
				const char tofu[] = "tofu";
				const char name[5] = "name:";
				const char fingerprints[12] = "fingerprint:";

				const char* cur = p[x];
				if (_strnicmp(deny, cur, sizeof(deny)) == 0)
					settings->AutoDenyCertificate = TRUE;
				else if (_strnicmp(ignore, cur, sizeof(ignore)) == 0)
					settings->IgnoreCertificate = TRUE;
				else if (_strnicmp(tofu, cur, 4) == 0)
					settings->AutoAcceptCertificate = TRUE;
				else if (_strnicmp(name, cur, sizeof(name)) == 0)
				{
					const char* val = &cur[sizeof(name)];
					if (!copy_value(val, &settings->CertificateName))
						rc = COMMAND_LINE_ERROR_MEMORY;
				}
				else if (_strnicmp(fingerprints, cur, sizeof(fingerprints)) == 0)
				{
					const char* val = &cur[sizeof(fingerprints)];
					if (!append_value(val, &settings->CertificateAcceptedFingerprints))
						rc = COMMAND_LINE_ERROR_MEMORY;
				}
				else
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			free(p);

			if (rc)
				return rc;
		}
		CommandLineSwitchCase(arg, "cert-name")
		{
			if (!copy_value(arg->Value, &settings->CertificateName))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			settings->IgnoreCertificate = enable;
		}
		CommandLineSwitchCase(arg, "cert-tofu")
		{
			settings->AutoAcceptCertificate = enable;
		}
		CommandLineSwitchCase(arg, "cert-deny")
		{
			settings->AutoDenyCertificate = enable;
		}
		CommandLineSwitchCase(arg, "authentication")
		{
			settings->Authentication = enable;
		}
		CommandLineSwitchCase(arg, "encryption")
		{
			settings->UseRdpSecurityLayer = !enable;
		}
		CommandLineSwitchCase(arg, "grab-keyboard")
		{
			settings->GrabKeyboard = enable;
		}
		CommandLineSwitchCase(arg, "unmap-buttons")
		{
			settings->UnmapButtons = enable;
		}
		CommandLineSwitchCase(arg, "toggle-fullscreen")
		{
			settings->ToggleFullscreen = enable;
		}
		CommandLineSwitchCase(arg, "floatbar")
		{
			/* Defaults are enabled, visible, sticky, fullscreen */
			settings->Floatbar = 0x0017;

			if (arg->Value)
			{
				char* start = arg->Value;

				do
				{
					char* cur = start;
					start = strchr(start, ',');

					if (start)
					{
						*start = '\0';
						start = start + 1;
					}

					/* sticky:[on|off] */
					if (_strnicmp(cur, "sticky:", 7) == 0)
					{
						const char* val = cur + 7;
						settings->Floatbar &= ~0x02u;

						if (_strnicmp(val, "on", 3) == 0)
							settings->Floatbar |= 0x02u;
						else if (_strnicmp(val, "off", 4) == 0)
							settings->Floatbar &= ~0x02u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* default:[visible|hidden] */
					else if (_strnicmp(cur, "default:", 8) == 0)
					{
						const char* val = cur + 8;
						settings->Floatbar &= ~0x04u;

						if (_strnicmp(val, "visible", 8) == 0)
							settings->Floatbar |= 0x04u;
						else if (_strnicmp(val, "hidden", 7) == 0)
							settings->Floatbar &= ~0x04u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* show:[always|fullscreen|window] */
					else if (_strnicmp(cur, "show:", 5) == 0)
					{
						const char* val = cur + 5;
						settings->Floatbar &= ~0x30u;

						if (_strnicmp(val, "always", 7) == 0)
							settings->Floatbar |= 0x30u;
						else if (_strnicmp(val, "fullscreen", 11) == 0)
							settings->Floatbar |= 0x10u;
						else if (_strnicmp(val, "window", 7) == 0)
							settings->Floatbar |= 0x20u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					else
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				} while (start);
			}
		}
		CommandLineSwitchCase(arg, "mouse-motion")
		{
			settings->MouseMotion = enable;
		}
		CommandLineSwitchCase(arg, "parent-window")
		{
			ULONGLONG val;

			if (!value_to_uint(arg->Value, &val, 0, UINT64_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->ParentWindowId = (UINT64)val;
		}
		CommandLineSwitchCase(arg, "client-build-number")
		{
			ULONGLONG val;

			if (!value_to_uint(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ClientBuild, (UINT32)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "bitmap-cache")
		{
			settings->BitmapCacheEnabled = enable;
		}
		CommandLineSwitchCase(arg, "offscreen-cache")
		{
			settings->OffscreenSupportLevel = (UINT32)enable;
		}
		CommandLineSwitchCase(arg, "glyph-cache")
		{
			settings->GlyphSupportLevel = arg->Value ? GLYPH_SUPPORT_FULL : GLYPH_SUPPORT_NONE;
		}
		CommandLineSwitchCase(arg, "codec-cache")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
			settings->FastPathInput = enable;
			settings->FastPathOutput = enable;
		}
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->MultifragMaxRequestSize = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "max-loop-time")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (val < 0)
				settings->MaxTimeInCheckLoop =
				    10 * 60 * 60 * 1000; /* 10 hours can be considered as infinite */
			else
				settings->MaxTimeInCheckLoop = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "auto-request-control")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceRequestControl,
			                               enable))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "async-input")
		{
			settings->AsyncInput = enable;
		}
		CommandLineSwitchCase(arg, "async-update")
		{
			settings->AsyncUpdate = enable;
		}
		CommandLineSwitchCase(arg, "async-channels")
		{
			settings->AsyncChannels = enable;
		}
		CommandLineSwitchCase(arg, "wm-class")
		{
			if (!copy_value(arg->Value, &settings->WmClass))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			if (!copy_value(arg->Value, &settings->PlayRemoteFxFile))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->PlayRemoteFx = TRUE;
		}
		CommandLineSwitchCase(arg, "auth-only")
		{
			settings->AuthenticationOnly = enable;
		}
		CommandLineSwitchCase(arg, "auto-reconnect")
		{
			settings->AutoReconnectionEnabled = enable;
		}
		CommandLineSwitchCase(arg, "auto-reconnect-max-retries")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 1000))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->AutoReconnectMaxRetries = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "reconnect-cookie")
		{
			BYTE* base64 = NULL;
			int length;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			crypto_base64_decode((const char*)(arg->Value), (int)strlen(arg->Value), &base64,
			                     &length);

			if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
			{
				memcpy(settings->ServerAutoReconnectCookie, base64, (size_t)length);
			}
			else
			{
				WLog_ERR(TAG, "reconnect-cookie:  invalid base64 '%s'", arg->Value);
			}

			free(base64);
		}
		CommandLineSwitchCase(arg, "print-reconnect-cookie")
		{
			settings->PrintReconnectCookie = enable;
		}
		CommandLineSwitchCase(arg, "pwidth")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopPhysicalWidth = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "pheight")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopPhysicalHeight = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "orientation")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->DesktopOrientation = (UINT16)val;
		}
		CommandLineSwitchCase(arg, "old-license")
		{
			settings->OldLicenseBehaviour = TRUE;
		}
		CommandLineSwitchCase(arg, "scale")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 100, 180))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			switch (val)
			{
				case 100:
				case 140:
				case 180:
					settings->DesktopScaleFactor = (UINT32)val;
					settings->DeviceScaleFactor = (UINT32)val;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "scale-desktop")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 100, 500))
				return FALSE;

			settings->DesktopScaleFactor = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "scale-device")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 100, 180))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			switch (val)
			{
				case 100:
				case 140:
				case 180:
					settings->DeviceScaleFactor = (UINT32)val;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "action-script")
		{
			if (!copy_value(arg->Value, &settings->ActionScript))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "rdp2tcp")
		{
			free(settings->RDP2TCPArgs);

			if (!(settings->RDP2TCPArgs = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "fipsmode")
		{
			settings->FIPSMode = enable;
		}
		CommandLineSwitchCase(arg, "smartcard-logon")
		{
			if (!settings->SmartcardLogon)
				activate_smartcard_logon_rdp(settings);
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

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

				if (!freerdp_passphrase_read("Gateway Password: ", settings->GatewayPassword, size,
				                             1))
					return COMMAND_LINE_ERROR;
			}
		}
	}

	freerdp_performance_flags_make(settings);

	if (settings->RemoteFxCodec || settings->NSCodec || settings->SupportGraphicsPipeline)
	{
		settings->FastPathOutput = TRUE;
		settings->LargePointerFlag =
		    0x0002; /* (LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384); */
		settings->FrameMarkerCommandEnabled = TRUE;
		settings->ColorDepth = 32;
	}

	arg = CommandLineFindArgumentA(largs, "port");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		LONGLONG val;

		if (!value_to_int(arg->Value, &val, 1, UINT16_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		settings->ServerPort = (UINT32)val;
	}

	arg = CommandLineFindArgumentA(largs, "p");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		FillMemory(arg->Value, strlen(arg->Value), '*');
	}

	arg = CommandLineFindArgumentA(largs, "gp");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		FillMemory(arg->Value, strlen(arg->Value), '*');
	}

	return status;
}

static BOOL freerdp_client_load_static_channel_addin(rdpChannels* channels, rdpSettings* settings,
                                                     char* name, void* data)
{
	PVIRTUALCHANNELENTRY entry = NULL;
	PVIRTUALCHANNELENTRYEX entryEx = NULL;
	entryEx = (PVIRTUALCHANNELENTRYEX)(void*)freerdp_load_channel_addin_entry(
	    name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX);

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

	if (settings->AudioPlayback)
	{
		char* p[] = { "rdpsnd" };

		if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	/* for audio playback also load the dynamic sound channel */
	if (settings->AudioPlayback)
	{
		char* p[] = { "rdpsnd" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->AudioCapture)
	{
		char* p[] = { "audin" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if ((freerdp_static_channel_collection_find(settings, "rdpsnd")) ||
	    (freerdp_dynamic_channel_collection_find(settings, "rdpsnd"))
#if defined(CHANNEL_TSMF_CLIENT)
	    || (freerdp_dynamic_channel_collection_find(settings, "tsmf"))
#endif
	)
	{
		settings->DeviceRedirection = TRUE; /* rdpsnd requires rdpdr to be registered */
		settings->AudioPlayback = TRUE;     /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (freerdp_dynamic_channel_collection_find(settings, "audin"))
	{
		settings->AudioCapture = TRUE;
	}

	if (settings->NetworkAutoDetect || settings->SupportHeartbeatPdu ||
	    settings->SupportMultitransport)
	{
		settings->DeviceRedirection = TRUE; /* these RDP8 features require rdpdr to be registered */
	}

	if (settings->DrivesToRedirect && (strlen(settings->DrivesToRedirect) != 0))
	{
		/*
		 * Drives to redirect:
		 *
		 * Very similar to DevicesToRedirect, but can contain a
		 * comma-separated list of drive letters to redirect.
		 */
		char* value;
		char* tok;
		char* context = NULL;

		value = _strdup(settings->DrivesToRedirect);
		if (!value)
			return FALSE;

		tok = strtok_s(value, ";", &context);
		if (!tok)
		{
			free(value);
			return FALSE;
		}

		while (tok)
		{
			/* Syntax: Comma seperated list of the following entries:
			 * '*'              ... Redirect all drives, including hotplug
			 * 'DynamicDrives'  ... hotplug
			 * <label>(<path>)  ... One or more paths to redirect.
			 * <path>(<label>)  ... One or more paths to redirect.
			 * <path>           ... One or more paths to redirect.
			 */
			/* TODO: Need to properly escape labels and paths */
			BOOL success;
			const char* name = NULL;
			const char* drive = tok;
			char* start = strtok(tok, "(");
			char* end = strtok(NULL, ")");
			if (start && end)
				name = end;

			if (freerdp_path_valid(name, NULL) && freerdp_path_valid(drive, NULL))
			{
				success = freerdp_client_add_drive(settings, name, NULL);
				if (success)
					success = freerdp_client_add_drive(settings, drive, NULL);
			}
			else
				success = freerdp_client_add_drive(settings, drive, name);

			if (!success)
			{
				free(value);
				return FALSE;
			}

			tok = strtok_s(NULL, ";", &context);
		}
		free(value);

		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

		settings->DeviceRedirection = TRUE;
	}
	else if (settings->RedirectDrives)
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			char* params[3];
			params[0] = "drive";
			params[1] = "media";
			params[2] = "*";

			if (!freerdp_client_add_device_channel(settings, 3, (char**)params))
				return FALSE;
		}
	}

	if (settings->RedirectDrives || settings->RedirectHomeDrive || settings->RedirectSerialPorts ||
	    settings->RedirectSmartCards || settings->RedirectPrinters)
	{
		settings->DeviceRedirection = TRUE; /* All of these features require rdpdr */
	}

	if (settings->RedirectHomeDrive)
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			char* params[3];
			params[0] = "drive";
			params[1] = "home";
			params[2] = "%";

			if (!freerdp_client_add_device_channel(settings, 3, (char**)params))
				return FALSE;
		}
	}

	if (settings->DeviceRedirection)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rdpdr", settings))
			return FALSE;

		if (!freerdp_static_channel_collection_find(settings, "rdpsnd") &&
		    !freerdp_dynamic_channel_collection_find(settings, "rdpsnd"))
		{
			char* params[2];
			params[0] = "rdpsnd";
			params[1] = "sys:fake";

			if (!freerdp_client_add_static_channel(settings, 2, (char**)params))
				return FALSE;
		}
	}

	if (settings->RedirectSmartCards)
	{
		RDPDR_SMARTCARD* smartcard;

		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
		{
			smartcard = (RDPDR_SMARTCARD*)calloc(1, sizeof(RDPDR_SMARTCARD));

			if (!smartcard)
				return FALSE;

			smartcard->Type = RDPDR_DTYP_SMARTCARD;

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)smartcard))
				return FALSE;
		}
	}

	if (settings->RedirectPrinters)
	{
		RDPDR_PRINTER* printer;

		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_PRINT))
		{
			printer = (RDPDR_PRINTER*)calloc(1, sizeof(RDPDR_PRINTER));

			if (!printer)
				return FALSE;

			printer->Type = RDPDR_DTYP_PRINT;

			if (!freerdp_device_collection_add(settings, (RDPDR_DEVICE*)printer))
				return FALSE;
		}
	}

	if (settings->RedirectClipboard)
	{
		char* params[1];
		params[0] = "cliprdr";

		if (!freerdp_client_add_static_channel(settings, 1, (char**)params))
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
		settings->NlaSecurity = FALSE;
	}

	if (settings->EncomspVirtualChannel)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "encomsp", settings))
			return FALSE;
	}

	if (settings->RemdeskVirtualChannel)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "remdesk", settings))
			return FALSE;
	}

	if (settings->RDP2TCPArgs)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rdp2tcp",
		                                              settings->RDP2TCPArgs))
			return FALSE;
	}

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		args = settings->StaticChannelArray[index];

		if (!freerdp_client_load_static_channel_addin(channels, settings, args->argv[0], args))
			return FALSE;
	}

	if (settings->RemoteApplicationMode)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rail", settings))
			return FALSE;
	}

	if (settings->MultiTouchInput)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "rdpei";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportGraphicsPipeline)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "rdpgfx";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportEchoChannel)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "echo";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportSSHAgentChannel)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "sshagent";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportDisplayControl)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "disp";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportGeometryTracking)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "geometry";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->SupportVideoOptimized)
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "video";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (settings->DynamicChannelCount)
		settings->SupportDynamicChannels = TRUE;

	if (settings->SupportDynamicChannels)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "drdynvc", settings))
			return FALSE;
	}

	return TRUE;
}
