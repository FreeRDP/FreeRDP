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

static BOOL append_value(rdpSettings* settings, const char* value, size_t settingsId)
{
	BOOL rc;
	const char* src;

	size_t slen = 0, alen, dlen;
	char* dst;
	if (!settings || !value)
		return FALSE;

	src = freerdp_settings_get_string(settings, settingsId);
	if (src)
		slen = strlen(src);
	alen = strlen(value);
	dlen = slen + alen + 1;

	dst = calloc(dlen, sizeof(char));
	if (!dst)
		return FALSE;
	memcpy(dst, src, slen);
	memcpy(&dst[slen], value, alen);

	rc = freerdp_settings_set_string(settings, settingsId, dst);
	free(dst);
	return rc;
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
	printf("USB Device Redirection: /usb:id:054c:0268#4669:6e6b,addr:04:0c\n");
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

				if (!freerdp_settings_set_string(settings, FreeRDP_ConnectionFile, argv[index]))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}

		if (length > 13)
		{
			if (_stricmp(&(argv[index])[length - 13], ".msrcIncident") == 0)
			{
				settings = (rdpSettings*)context;

				if (!freerdp_settings_set_string(settings, FreeRDP_AssistanceFile, argv[index]))
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

		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
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

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectPrinters, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

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

		freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
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

		freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
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

		freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
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
			freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
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
		freerdp_settings_set_bool(settings, FreeRDP_MultiTouchInput, enable);
	}
	CommandLineSwitchCase(arg, "gestures")
	{
		freerdp_settings_set_bool(settings, FreeRDP_MultiTouchGestures, enable);
	}
	CommandLineSwitchCase(arg, "echo")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportEchoChannel, enable);
	}
	CommandLineSwitchCase(arg, "ssh-agent")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportSSHAgentChannel, enable);
	}
	CommandLineSwitchCase(arg, "disp")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, enable);
	}
	CommandLineSwitchCase(arg, "geometry")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportGeometryTracking, enable);
	}
	CommandLineSwitchCase(arg, "video")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportGeometryTracking,
		                          enable); /* this requires geometry tracking */
		freerdp_settings_set_bool(settings, FreeRDP_SupportVideoOptimized, enable);
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
		freerdp_settings_set_bool(settings, FreeRDP_SupportHeartbeatPdu, enable);
	}
	CommandLineSwitchCase(arg, "multitransport")
	{
		freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, enable);

		if (freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport))
			freerdp_settings_set_uint32(
			    settings, FreeRDP_MultitransportFlags,
			    (TRANSPORT_TYPE_UDP_FECR | TRANSPORT_TYPE_UDP_FECL | TRANSPORT_TYPE_UDP_PREFERRED));
		else
			freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags, 0);
	}
	CommandLineSwitchCase(arg, "password-is-pin")
	{
		freerdp_settings_set_bool(settings, FreeRDP_PasswordIsSmartcardPin, enable);
	}
	CommandLineSwitchEnd(arg) return status ? 1 : -1;
}

BOOL freerdp_parse_username(rdpSettings* settings, const char* username, size_t settingsUserId,
                            size_t settingsDomainId)
{
	const char* p;
	size_t length = 0;
	p = strchr(username, '\\');

	if (p)
	{
		if (!freerdp_settings_set_string(settings, settingsUserId, &p[1]) ||
		    !freerdp_settings_set_string_len(settings, settingsDomainId, username, length - 1))
			return FALSE;
	}
	else if (username)
	{
		/* Do not break up the name for '@'; both credSSP and the
		 * ClientInfo PDU expect 'user@corp.net' to be transmitted
		 * as username 'user@corp.net', domain empty (not NULL!).
		 */
		if (!freerdp_settings_set_string(settings, settingsUserId, username) ||
		    !freerdp_settings_set_string(settings, settingsDomainId, NULL))
			return FALSE;
	}
	else
		return FALSE;

	return TRUE;
}

BOOL freerdp_parse_hostname(rdpSettings* settings, const char* hostname, size_t settingsHostId,
                            size_t settingsPortId)
{
	const char* p;
	p = strrchr(hostname, ':');

	if (p)
	{
		size_t length = (size_t)(p - hostname);
		LONGLONG val;

		if (!value_to_int(p + 1, &val, 1, UINT16_MAX))
			return FALSE;

		if (!freerdp_settings_set_string_len(settings, settingsHostId, hostname, length - 1) ||
		    !freerdp_settings_set_uint32(settings, settingsPortId, val))
			return FALSE;
	}
	else
	{
		if (!freerdp_settings_set_string(settings, settingsHostId, hostname))
			return FALSE;
	}

	return TRUE;
}

BOOL freerdp_set_connection_type(rdpSettings* settings, UINT32 type)
{
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType, type))
		return FALSE;

	if (type == CONNECTION_TYPE_MODEM)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, TRUE);
	}
	else if (type == CONNECTION_TYPE_BROADBAND_LOW)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
	}
	else if (type == CONNECTION_TYPE_SATELLITE)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
	}
	else if (type == CONNECTION_TYPE_BROADBAND_HIGH)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
	}
	else if (type == CONNECTION_TYPE_WAN)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
	}
	else if (type == CONNECTION_TYPE_LAN)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
	}
	else if (type == CONNECTION_TYPE_AUTODETECT)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE);

		/* Automatically activate GFX and RFX codec support */
#ifdef WITH_GFX_H264
		freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE);
#endif
		freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE);
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
			freerdp_settings_set_bool(settings, FreeRDP_ListMonitors, TRUE);
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
	freerdp_settings_set_bool(settings, FreeRDP_SmartcardLogon, TRUE);
	/* TODO: why not? freerdp_settings_get_bool(settings, FreeRDP_UseRdpSecurityLayer) = TRUE; */
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

	freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, NULL);
	freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, NULL);
	freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, NULL);

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
			freerdp_settings_set_string(settings, FreeRDP_ServerHostname, NULL);
			p = strchr(arg->Value, '[');

			/* ipv4 */
			if (!p)
			{
				p = strchr(arg->Value, ':');

				if (p)
				{
					LONGLONG val;

					p[0] = '\0';
					if (!value_to_int(&p[1], &val, 1, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					length = (size_t)(p - arg->Value);
					if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, arg->Value))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				else
				{
					if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, arg->Value))
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

				if (!freerdp_settings_set_string_len(settings, FreeRDP_ServerHostname, p + 1,
				                                     length - 1))
					return COMMAND_LINE_ERROR_MEMORY;

				if (*(p2 + 1) == ':')
				{
					LONGLONG val;

					if (!value_to_int(&p[2], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}

				printf("hostname %s port %" PRIu32 "\n",
				       freerdp_settings_get_string(settings, FreeRDP_ServerHostname),
				       freerdp_settings_get_uint32(settings, FreeRDP_ServerPort));
			}
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationServiceClass,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "redirect-prefer")
		{
			size_t count = 0;
			char* cur = arg->Value;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_RedirectionPreferType, 0))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
				{
					UINT32 type =
					    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPreferType);
					type |= mask << (count * 3);
					if (!freerdp_settings_set_uint32(settings, FreeRDP_RedirectionPreferType, type))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				count++;
			} while (cur != NULL);

			if (count > 3)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "credentials-delegation")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableCredentialsDelegation, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "vmconnect")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE) ||
			    !freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 2179) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, FALSE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
					return COMMAND_LINE_ERROR_MEMORY;
			}
		}
		CommandLineSwitchCase(arg, "w")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, (UINT32)val);
		}
		CommandLineSwitchCase(arg, "h")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, val);
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

				if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, w) ||
				    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, h))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else
			{
				BOOL rc = TRUE;
				if (!(str = _strdup(arg->Value)))
					return COMMAND_LINE_ERROR_MEMORY;

				p = strchr(str, '%');

				if (p)
				{
					BOOL partial = FALSE;

					if (strchr(p, 'w'))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseWidth,
						                               TRUE))
							rc = FALSE;
						partial = TRUE;
					}

					if (strchr(p, 'h'))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseHeight,
						                               TRUE))
							rc = FALSE;
						partial = TRUE;
					}

					if (!partial)
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseWidth,
						                               TRUE) ||
						    !freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseHeight,
						                               TRUE))
							rc = FALSE;
					}

					*p = '\0';
					{
						LONGLONG val;

						if (!value_to_int(str, &val, 0, 100))
							rc = FALSE;

						if (!freerdp_settings_set_uint32(settings, FreeRDP_PercentScreen, val))
							rc = FALSE;
					}
				}

				free(str);
				if (!rc)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "f")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "suppress-output")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SuppressOutput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "multimon")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (_stricmp(arg->Value, "force") == 0)
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_ForceMultimon, enable))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}
		}
		CommandLineSwitchCase(arg, "span")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SpanMonitors, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "workarea")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Workarea, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "monitors")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				char** p;
				BOOL rc = TRUE;
				size_t count = 0;
				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				if (!p)
					return COMMAND_LINE_ERROR_MEMORY;

				if (count > 16)
					count = 16;

				rc = freerdp_settings_set_uint32(settings, FreeRDP_NumMonitorIds, count);
				if (rc)
				{
					for (i = 0; i < count; i++)
					{
						LONGLONG val;
						UINT32 sval;

						if (!value_to_int(p[i], &val, 0, UINT16_MAX))
						{
							rc = FALSE;
							break;
						}

						sval = val;
						if (!freerdp_settings_set_pointer_array(settings, FreeRDP_MonitorIds, i,
						                                        &sval))
						{
							rc = FALSE;
							break;
						}
					}
				}
				free(p);
				if (!rc)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "monitor-list")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ListMonitors, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "t")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WindowTitle, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Decorations, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "dynamic-resolution")
		{
			if (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
			{
				WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "smart-sizing")
		{
			if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
			{
				WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			if (!freerdp_settings_set_bool(settings, FreeRDP_SmartSizing, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (arg->Value)
			{
				unsigned long w, h;

				if (!parseSizeValue(arg->Value, &w, &h) || (w > UINT16_MAX) || (h > UINT16_MAX))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (!freerdp_settings_set_bool(settings, FreeRDP_SmartSizingWidth, (UINT32)w) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_SmartSizingHeight, (UINT32)h))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			switch (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth))
			{
				case 32:
				case 24:
				case 16:
				case 15:
				case 8:
					if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, (UINT32)val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "admin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "relax-order-checks")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowUnanouncedOrdersFromServer,
			                               enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "restricted-admin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "pth")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_string(settings, FreeRDP_PasswordHash, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "client-hostname")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ClientHostname, arg->Value))
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

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardCodePage, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "u")
		{
			user = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "d")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Domain, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "p")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Password, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "g")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, NULL))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
					if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					if (!freerdp_settings_set_string_len(settings, FreeRDP_GatewayHostname,
					                                     arg->Value, s))
						return COMMAND_LINE_ERROR_MEMORY;
				}
				else
				{
					if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, arg->Value))
						return COMMAND_LINE_ERROR_MEMORY;
				}
			}
			else
			{
				if (!freerdp_settings_set_string(
				        settings, FreeRDP_GatewayHostname,
				        freerdp_settings_get_string(settings, FreeRDP_ServerHostname)))
					return COMMAND_LINE_ERROR_MEMORY;
			}

			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT);
		}
		CommandLineSwitchCase(arg, "proxy")
		{
			/* initial value */
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP))
				return COMMAND_LINE_ERROR_MEMORY;

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
					{
						if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType,
						                                 PROXY_TYPE_IGNORE))
							return COMMAND_LINE_ERROR_MEMORY;
					}
					if (_stricmp("http", arg->Value) == 0)
					{
						if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType,
						                                 PROXY_TYPE_HTTP))
							return COMMAND_LINE_ERROR_MEMORY;
					}
					else if (_stricmp("socks5", arg->Value) == 0)
					{
						if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType,
						                                 PROXY_TYPE_SOCKS))
							return COMMAND_LINE_ERROR_MEMORY;
					}
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
					if (!freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, arg->Value))
					{
						WLog_ERR(TAG, "unable to allocate proxy username");
						return COMMAND_LINE_ERROR_MEMORY;
					}

					*atPtr = '\0';

					if (!freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, colonPtr + 1))
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
					if (!freerdp_settings_set_uint16(settings, FreeRDP_ProxyPort, val))
						return FALSE;
					*p = '\0';
				}

				p = strchr(arg->Value, '/');
				if (p)
					*p = '\0';
				if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, arg->Value))
					return FALSE;
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

			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "gd")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "gp")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE);
		}
		CommandLineSwitchCase(arg, "gt")
		{
			if (_stricmp(arg->Value, "rpc") == 0)
			{
				freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE);
				freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, FALSE);
			}
			else if (_stricmp(arg->Value, "http") == 0)
			{
				freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE);
				freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE);
			}
			else if (_stricmp(arg->Value, "auto") == 0)
			{
				freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE);
				freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE);
			}
		}
		CommandLineSwitchCase(arg, "gat")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, arg->Value))
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
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram,
			                                 arg->Value) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_HiDefRemoteApp, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_SpanMonitors, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_RemoteApplicationMode, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_RemoteAppLanguageBarSupported, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_Workarea, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-workdir")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, arg->Value,
			                                      strlen(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationName, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationIcon, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationFile, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationGuid, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "compression")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "compression-level")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "drives")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "home-drive")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectHomeDrive, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "ipv6")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_PreferIPv6OverIPv4, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "clipboard")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "shell")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AlternateShell, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "shell-dir")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ShellWorkingDirectory, arg->Value))
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
					if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					break;

				case AUDIO_MODE_PLAY_ON_SERVER:
					if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, TRUE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					break;

				case AUDIO_MODE_NONE:
					if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, FALSE) ||
					    !freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "wallpaper")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "window-drag")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, x) ||
			    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, y))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "menu-anims")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "themes")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "timeout")
		{
			ULONGLONG val;
			if (!value_to_uint(arg->Value, &val, 1, 600000))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "aero")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (_stricmp(arg->Value, "sw") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (_stricmp(arg->Value, "hw") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, FALSE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "gfx")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
					for (x = 0; (x < count) && (rc == CHANNEL_RC_OK); x++)
					{
						const char* val = p[x];
#ifdef WITH_GFX_H264
						if (_strnicmp("AVC444", val, 7) == 0)
						{
							if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE))
								rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
						else if (_strnicmp("AVC420", val, 7) == 0)
						{
							if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, FALSE))
								rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
						else
#endif
						    if (_strnicmp("RFX", val, 4) == 0)
						{
							if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, FALSE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, FALSE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
								rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
						else if (_strnicmp("mask:", val, 5) == 0)
						{
							ULONGLONG v;
							const char* uv = &val[5];
							if (!value_to_uint(uv, &v, 0, UINT32_MAX))
								rc = COMMAND_LINE_ERROR;
							else
							{
								if (!freerdp_settings_set_uint32(settings, FreeRDP_GfxCapsFilter,
								                                 v))
									rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
							}
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
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (freerdp_settings_get_bool(settings, FreeRDP_GfxThinClient))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "gfx-small-cache")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (enable)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, enable) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, !enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (enable)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
#ifdef WITH_GFX_H264
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
					for (x = 0; (x < count) && (rc == CHANNEL_RC_OK); x++)
					{
						const char* val = p[x];

						if (_strnicmp("AVC444", val, 7) == 0)
						{
							if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE))
								rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
						else if (_strnicmp("AVC420", val, 7) == 0)
						{
							if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE) ||
							    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, FALSE))
								rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
						else if (_strnicmp("mask:", val, 5) == 0)
						{
							ULONGLONG v;
							const char* uv = &val[5];
							if (!value_to_uint(uv, &v, 0, UINT32_MAX))
								rc = COMMAND_LINE_ERROR;
							else
							{
								if (!freerdp_settings_set_uint32(settings, FreeRDP_GfxCapsFilter,
								                                 v))
									rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
							}
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
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			UINT32 mode = UINT32_MAX;
			if (!arg->Value)
			{
			}
			else if (strcmp(arg->Value, "video") == 0)
				mode = 0x00;
			else if (strcmp(arg->Value, "image") == 0)
				mode = 0x02;

			if (mode != UINT32_MAX)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxCodecMode, mode))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "frame-ack")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_FrameAcknowledge, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
#if defined(WITH_JPEG)
		CommandLineSwitchCase(arg, "jpeg")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, 75))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "jpeg-quality")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 100))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
#endif
		CommandLineSwitchCase(arg, "nego")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_PreconnectionId, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (strcmp("rdp", arg->Value) == 0) /* Standard RDP */
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (strcmp("tls", arg->Value) == 0) /* TLS */
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (strcmp("nla", arg->Value) == 0) /* NLA */
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (strcmp("ext", arg->Value) == 0) /* NLA Extended */
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
				UINT32 EncryptionMethods =
				    freerdp_settings_get_uint32(settings, FreeRDP_EncryptionMethods);
				p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				for (i = 0; i < count; i++)
				{
					if (!strcmp(p[i], "40"))
						EncryptionMethods |= ENCRYPTION_METHOD_40BIT;
					else if (!strcmp(p[i], "56"))
						EncryptionMethods |= ENCRYPTION_METHOD_56BIT;
					else if (!strcmp(p[i], "128"))
						EncryptionMethods |= ENCRYPTION_METHOD_128BIT;
					else if (!strcmp(p[i], "FIPS"))
						EncryptionMethods |= ENCRYPTION_METHOD_FIPS;
					else
						WLog_ERR(TAG, "unknown encryption method '%s'", p[i]);
				}

				free(p);
				if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods,
				                                 EncryptionMethods))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "from-stdin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_CredentialsFromStdin, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

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
			if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "tls-ciphers")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (strcmp(arg->Value, "netmon") == 0)
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_AllowedTlsCiphers, "ALL:!ECDH"))
					return COMMAND_LINE_ERROR_MEMORY;
			}
			else if (strcmp(arg->Value, "ma") == 0)
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_AllowedTlsCiphers, "AES128-SHA"))
					return COMMAND_LINE_ERROR_MEMORY;
			}
			else
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_AllowedTlsCiphers, arg->Value))
					return COMMAND_LINE_ERROR_MEMORY;
			}
		}
		CommandLineSwitchCase(arg, "tls-seclevel")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 5))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
					freerdp_settings_set_bool(settings, FreeRDP_AutoDenyCertificate, TRUE);
				else if (_strnicmp(ignore, cur, sizeof(ignore)) == 0)
					freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE);
				else if (_strnicmp(tofu, cur, 4) == 0)
					freerdp_settings_set_bool(settings, FreeRDP_AutoAcceptCertificate, TRUE);
				else if (_strnicmp(name, cur, sizeof(name)) == 0)
				{
					const char* val = &cur[sizeof(name)];
					if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, val))
						rc = COMMAND_LINE_ERROR_MEMORY;
				}
				else if (_strnicmp(fingerprints, cur, sizeof(fingerprints)) == 0)
				{
					const char* val = &cur[sizeof(fingerprints)];
					if (!append_value(settings, val, FreeRDP_CertificateAcceptedFingerprints))
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
			if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-tofu")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoAcceptCertificate, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-deny")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoDenyCertificate, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "authentication")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Authentication, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "encryption")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, !enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "grab-keyboard")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GrabKeyboard, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "unmap-buttons")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnmapButtons, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "toggle-fullscreen")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ToggleFullscreen, enable))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "floatbar")
		{
			/* Defaults are enabled, visible, sticky, fullscreen */
			if (!freerdp_settings_set_uint32(settings, FreeRDP_Floatbar, 0x0017))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (arg->Value)
			{
				char* start = arg->Value;

				do
				{
					DWORD floatbar = freerdp_settings_get_uint32(settings, FreeRDP_Floatbar);
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
						floatbar &= ~0x02u;

						if (_strnicmp(val, "on", 3) == 0)
							floatbar |= 0x02u;
						else if (_strnicmp(val, "off", 4) == 0)
							floatbar &= ~0x02u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* default:[visible|hidden] */
					else if (_strnicmp(cur, "default:", 8) == 0)
					{
						const char* val = cur + 8;
						floatbar &= ~0x04u;

						if (_strnicmp(val, "visible", 8) == 0)
							floatbar |= 0x04u;
						else if (_strnicmp(val, "hidden", 7) == 0)
							floatbar &= ~0x04u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* show:[always|fullscreen|window] */
					else if (_strnicmp(cur, "show:", 5) == 0)
					{
						const char* val = cur + 5;
						floatbar &= ~0x30u;

						if (_strnicmp(val, "always", 7) == 0)
							floatbar |= 0x30u;
						else if (_strnicmp(val, "fullscreen", 11) == 0)
							floatbar |= 0x10u;
						else if (_strnicmp(val, "window", 7) == 0)
							floatbar |= 0x20u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					else
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					if (!freerdp_settings_set_uint32(settings, FreeRDP_Floatbar, floatbar))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				} while (start);
			}
		}
		CommandLineSwitchCase(arg, "mouse-motion")
		{
			freerdp_settings_set_bool(settings, FreeRDP_MouseMotion, enable);
		}
		CommandLineSwitchCase(arg, "parent-window")
		{
			ULONGLONG val;

			if (!value_to_uint(arg->Value, &val, 0, UINT64_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint64(settings, FreeRDP_ParentWindowId, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "offscreen-cache")
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel,
			                                 (UINT32)enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "glyph-cache")
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel,
			                                 arg->Value ? GLYPH_SUPPORT_FULL : GLYPH_SUPPORT_NONE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "codec-cache")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheV3Enabled, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (strcmp(arg->Value, "rfx") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (strcmp(arg->Value, "nsc") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

#if defined(WITH_JPEG)
			else if (strcmp(arg->Value, "jpeg") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, TRUE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (freerdp_settings_get_uint32(settings, FreeRDP_JpegQuality) == 0)
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, 75))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}
#endif
		}
		CommandLineSwitchCase(arg, "fast-path")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "max-loop-time")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (val < 0)
				val = 10 * 60 * 60 * 1000; /* 10 hours can be considered as infinite */

			if (!freerdp_settings_set_uint32(settings, FreeRDP_MaxTimeInCheckLoop, (UINT32)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "auto-request-control")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceRequestControl,
			                               enable))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "async-input")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AsyncInput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "async-update")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AsyncUpdate, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "async-channels")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AsyncChannels, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "wm-class")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WmClass, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_PlayRemoteFx, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			if (!freerdp_settings_set_bool(settings, FreeRDP_PlayRemoteFx, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "auth-only")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AuthenticationOnly, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "auto-reconnect")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "auto-reconnect-max-retries")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 1000))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "reconnect-cookie")
		{
			BOOL rc = TRUE;
			BYTE* base64 = NULL;
			int length;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			crypto_base64_decode((const char*)(arg->Value), (int)strlen(arg->Value), &base64,
			                     &length);

			if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
			{
				if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie,
				                                      base64, (size_t)length))
					rc = FALSE;
			}
			else
			{
				WLog_ERR(TAG, "reconnect-cookie:  invalid base64 '%s'", arg->Value);
			}

			free(base64);
			if (!rc)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "print-reconnect-cookie")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_PrintReconnectCookie, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "pwidth")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalWidth, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "pheight")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalHeight, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "orientation")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint16(settings, FreeRDP_DesktopOrientation, (UINT16)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "old-license")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_OldLicenseBehaviour, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
					if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor,
					                                 (UINT32)val) ||
					    !freerdp_settings_set_uint32(settings, FreeRDP_DeviceScaleFactor,
					                                 (UINT32)val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "scale-desktop")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 100, 500))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor, (UINT32)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
					if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceScaleFactor,
					                                 (UINT32)val))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					break;

				default:
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
		CommandLineSwitchCase(arg, "action-script")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ActionScript, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "rdp2tcp")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RDP2TCPArgs, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "fipsmode")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_FIPSMode, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "smartcard-logon")
		{
			if (!freerdp_settings_get_bool(settings, FreeRDP_SmartcardLogon))
				activate_smartcard_logon_rdp(settings);
		}

		CommandLineSwitchCase(arg, "tune")
		{
			size_t x, count;
			char** p = CommandLineParseCommaSeparatedValuesEx("tune", arg->Value, &count);
			if (!p)
				return COMMAND_LINE_ERROR;
			for (x = 1; x < count; x++)
			{
				char* cur = p[x];
				char* sep = strchr(cur, ':');
				if (!sep)
				{
					free(p);
					return COMMAND_LINE_ERROR;
				}
				*sep++ = '\0';
				if (!freerdp_settings_set_value_for_name(settings, cur, sep))
				{
					free(p);
					return COMMAND_LINE_ERROR;
				}
			}

			free(p);
		}
		CommandLineSwitchCase(arg, "tune-list")
		{
			size_t x;
			SSIZE_T type = 0;

			printf("%s\t%50s\t%s\t%s", "<index>", "<key>", "<type>", "<default value>\n");
			for (x = 0; x < FreeRDP_Settings_StableAPI_MAX; x++)
			{
				const char* name = freerdp_settings_get_name_for_key(x);
				type = freerdp_settings_get_type_for_key(x);

				switch (type)
				{
					case RDP_SETTINGS_TYPE_BOOL:
						printf("%" PRIuz "\t%50s\tBOOL\t%s\n", x, name,
						       freerdp_settings_get_bool(settings, x) ? "TRUE" : "FALSE");
						break;
					case RDP_SETTINGS_TYPE_UINT16:
						printf("%" PRIuz "\t%50s\tUINT16\t%" PRIu16 "\n", x, name,
						       freerdp_settings_get_uint16(settings, x));
						break;
					case RDP_SETTINGS_TYPE_INT16:
						printf("%" PRIuz "\t%50s\tINT16\t%" PRId16 "\n", x, name,
						       freerdp_settings_get_int16(settings, x));
						break;
					case RDP_SETTINGS_TYPE_UINT32:
						printf("%" PRIuz "\t%50s\tUINT32\t%" PRIu32 "\n", x, name,
						       freerdp_settings_get_uint32(settings, x));
						break;
					case RDP_SETTINGS_TYPE_INT32:
						printf("%" PRIuz "\t%50s\tINT32\t%" PRId32 "\n", x, name,
						       freerdp_settings_get_int32(settings, x));
						break;
					case RDP_SETTINGS_TYPE_UINT64:
						printf("%" PRIuz "\t%50s\tUINT64\t%" PRIu64 "\n", x, name,
						       freerdp_settings_get_uint64(settings, x));
						break;
					case RDP_SETTINGS_TYPE_INT64:
						printf("%" PRIuz "\t%50s\tINT64\t%" PRId64 "\n", x, name,
						       freerdp_settings_get_int64(settings, x));
						break;
					case RDP_SETTINGS_TYPE_STRING:
						printf("%" PRIuz "\t%50s\tSTRING\t%s"
						       "\n",
						       x, name, freerdp_settings_get_string(settings, x));
						break;
					case RDP_SETTINGS_TYPE_POINTER:
						printf("%" PRIuz "\t%50s\tPOINTER\t%p"
						       "\n",
						       x, name, freerdp_settings_get_pointer(settings, x));
						break;
					default:
						break;
				}
			}
			return COMMAND_LINE_STATUS_PRINT;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if (user)
	{
		const char* domain = freerdp_settings_get_string(settings, FreeRDP_Domain);

		if (!domain && user)
		{
			BOOL ret;
			ret = freerdp_parse_username(settings, user, FreeRDP_Username, FreeRDP_Domain);
			if (!ret)
				return COMMAND_LINE_ERROR;
		}
		else if (!freerdp_settings_set_string(settings, FreeRDP_Username, user))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		free(user);
	}

	if (gwUser)
	{
		if (!freerdp_settings_get_string(settings, FreeRDP_GatewayDomain) && gwUser)
		{
			BOOL ret;
			ret = freerdp_parse_username(settings, user, FreeRDP_GatewayUsername,
			                             FreeRDP_GatewayDomain);
			if (!ret)
				return COMMAND_LINE_ERROR;
		}
		else if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, gwUser))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		free(gwUser);
	}

	if (promptForPassword)
	{
		const size_t size = 512;

		if (!freerdp_settings_get_string(settings, FreeRDP_Password))
		{
			BOOL rc;
			char* pwd = calloc(size, sizeof(char));

			if (!pwd)
				return COMMAND_LINE_ERROR;

			rc = freerdp_passphrase_read("Password: ", pwd, size, 1) != NULL;
			if (rc)
				rc = freerdp_settings_set_string(settings, FreeRDP_Password, pwd);
			free(pwd);
			if (!rc)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		if (freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_GatewayUseSameCredentials))
		{
			if (!freerdp_settings_get_string(settings, FreeRDP_GatewayPassword))
			{
				BOOL rc;
				char* str = calloc(size, sizeof(char));

				if (!str)
					return COMMAND_LINE_ERROR;

				rc = freerdp_passphrase_read("Gateway Password: ", str, size, 1) != NULL;
				if (rc)
					rc = freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, str);
				free(str);
				if (!rc)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
	}

	freerdp_performance_flags_make(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec) ||
	    freerdp_settings_get_bool(settings, FreeRDP_NSCodec) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportGraphicsPipeline))
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, TRUE))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}

	arg = CommandLineFindArgumentA(largs, "port");

	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		LONGLONG val;

		if (!value_to_int(arg->Value, &val, 1, UINT16_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, val))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
                                                     const char* name, void* data)
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

	if (freerdp_settings_get_bool(settings, FreeRDP_AudioPlayback))
	{
		char* p[] = { "rdpsnd" };

		if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	/* for audio playback also load the dynamic sound channel */
	if (freerdp_settings_get_bool(settings, FreeRDP_AudioPlayback))
	{
		char* p[] = { "rdpsnd" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_AudioCapture))
	{
		char* p[] = { "audin" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (freerdp_static_channel_collection_find(settings, "rdpsnd") ||
	    freerdp_dynamic_channel_collection_find(settings, "rdpsnd")
#if defined(CHANNEL_TSMF_CLIENT)
	    || freerdp_dynamic_channel_collection_find(settings, "tsmf")
#endif
	)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection,
		                          TRUE); /* rdpsnd requires rdpdr to be registered */
		freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback,
		                          TRUE); /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (freerdp_dynamic_channel_collection_find(settings, "audin"))
	{
		freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, TRUE);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportHeartbeatPdu) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport))
	{
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection,
		                          TRUE); /* these RDP8 features require rdpdr to be registered */
	}

	if (freerdp_settings_get_string(settings, FreeRDP_DrivesToRedirect) &&
	    (strlen(freerdp_settings_get_string(settings, FreeRDP_DrivesToRedirect)) != 0))
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

		value = _strdup(freerdp_settings_get_string(settings, FreeRDP_DrivesToRedirect));
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
			char* subcontext = NULL;
			char* start = strtok_s(tok, "(", &subcontext);
			char* end = strtok_s(NULL, ")", &subcontext);
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

		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE);
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_RedirectDrives))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectDrives) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectHomeDrive) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectSerialPorts) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectPrinters))
	{
		freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection,
		                          TRUE); /* All of these features require rdpdr */
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectHomeDrive))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_DeviceRedirection))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectPrinters))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectClipboard))
	{
		char* params[1];
		params[0] = "cliprdr";

		if (!freerdp_client_add_static_channel(settings, 1, (char**)params))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_LyncRdpMode))
	{
		freerdp_settings_set_bool(settings, FreeRDP_EncomspVirtualChannel, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_RemdeskVirtualChannel, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, FALSE);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteAssistanceMode))
	{
		freerdp_settings_set_bool(settings, FreeRDP_EncomspVirtualChannel, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_RemdeskVirtualChannel, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_EncomspVirtualChannel))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "encomsp", settings))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RemdeskVirtualChannel))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "remdesk", settings))
			return FALSE;
	}

	if (freerdp_settings_get_string(settings, FreeRDP_RDP2TCPArgs))
	{
		if (!freerdp_client_load_static_channel_addin(
		        channels, settings, "rdp2tcp",
		        (void*)freerdp_settings_get_string(settings, FreeRDP_RDP2TCPArgs)))
			return FALSE;
	}

	for (index = 0; index < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);
	     index++)
	{
		const ADDIN_ARGV* args = (const ADDIN_ARGV*)freerdp_settings_get_pointer_array(
		    settings, FreeRDP_StaticChannelArray, index);

		if (!freerdp_client_load_static_channel_addin(channels, settings, args->argv[0],
		                                              (void*)args))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "rail", settings))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_MultiTouchInput))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "rdpei";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportGraphicsPipeline))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "rdpgfx";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportEchoChannel))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "echo";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportSSHAgentChannel))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "sshagent";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportDisplayControl))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "disp";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportGeometryTracking))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "geometry";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportVideoOptimized))
	{
		char* p[1];
		size_t count;
		count = 1;
		p[0] = "video";

		if (!freerdp_client_add_dynamic_channel(settings, count, p))
			return FALSE;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount))
		freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicChannels, TRUE);

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportDynamicChannels))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, "drdynvc", settings))
			return FALSE;
	}

	return TRUE;
}
