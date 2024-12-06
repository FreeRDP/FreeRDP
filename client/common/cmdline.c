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

#include <freerdp/config.h>

#include <ctype.h>
#include <errno.h>

#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/path.h>
#include <winpr/ncrypt.h>
#include <winpr/environment.h>
#include <winpr/timezone.h>

#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/channels/encomsp.h>
#include <freerdp/channels/rdpear.h>
#include <freerdp/channels/rdp2tcp.h>
#include <freerdp/channels/remdesk.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/channels/disp.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/utils/proxy_utils.h>
#include <freerdp/utils/string.h>
#include <freerdp/channels/urbdrc.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/locale/locale.h>

#if defined(CHANNEL_AINPUT_CLIENT)
#include <freerdp/channels/ainput.h>
#endif

#include <freerdp/channels/audin.h>
#include <freerdp/channels/echo.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/version.h>
#include <freerdp/client/utils/smartcard_cli.h>

#include <openssl/tls1.h>
#include "cmdline.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.cmdline")

static const char str_force[] = "force";

static const char* option_starts_with(const char* what, const char* val);
static BOOL option_ends_with(const char* str, const char* ext);
static BOOL option_equals(const char* what, const char* val);

static BOOL freerdp_client_print_codepages(const char* arg)
{
	size_t count = 0;
	DWORD column = 2;
	const char* filter = NULL;
	RDP_CODEPAGE* pages = NULL;

	if (arg)
	{
		filter = strchr(arg, ',');
		if (!filter)
			filter = arg;
		else
			filter++;
	}
	pages = freerdp_keyboard_get_matching_codepages(column, filter, &count);
	if (!pages)
		return TRUE;

	printf("%-10s %-8s %-60s %-36s %-48s\n", "<id>", "<locale>", "<win langid>", "<language>",
	       "<country>");
	for (size_t x = 0; x < count; x++)
	{
		const RDP_CODEPAGE* page = &pages[x];
		char buffer[2048] = { 0 };

		if (strnlen(page->subLanguageSymbol, ARRAYSIZE(page->subLanguageSymbol)) > 0)
			(void)_snprintf(buffer, sizeof(buffer), "[%s|%s]", page->primaryLanguageSymbol,
			                page->subLanguageSymbol);
		else
			(void)_snprintf(buffer, sizeof(buffer), "[%s]", page->primaryLanguageSymbol);
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
	BOOL isSpecial = 0;
	if (!path)
		return FALSE;

	isSpecial =
	    (option_equals("*", path) || option_equals(DynamicDrives, path) || option_equals("%", path))
	        ? TRUE
	        : FALSE;
	if (!isSpecial)
		isPath = winpr_PathFileExists(path);

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

static char* name_from_path(const char* path)
{
	const char* name = "NULL";
	if (path)
	{
		if (option_equals("%", path))
			name = "home";
		else if (option_equals("*", path))
			name = "hotplug-all";
		else if (option_equals("DynamicDrives", path))
			name = "hotplug";
		else
			name = path;
	}
	return _strdup(name);
}

static BOOL freerdp_client_add_drive(rdpSettings* settings, const char* path, const char* name)
{
	char* dname = NULL;
	RDPDR_DEVICE* device = NULL;

	if (name)
	{
		BOOL skip = FALSE;
		if (path)
		{
			switch (path[0])
			{
				case '*':
				case '%':
					skip = TRUE;
					break;
				default:
					break;
			}
		}
		/* Path was entered as secondary argument, swap */
		if (!skip && winpr_PathFileExists(name))
		{
			if (!winpr_PathFileExists(path) || (!PathIsRelativeA(name) && PathIsRelativeA(path)))
			{
				const char* tmp = path;
				path = name;
				name = tmp;
			}
		}
	}

	if (name)
		dname = _strdup(name);
	else /* We need a name to send to the server. */
		dname = name_from_path(path);

	if (freerdp_sanitize_drive_name(dname, "\\/", "__"))
	{
		const char* args[] = { dname, path };
		device = freerdp_device_new(RDPDR_DTYP_FILESYSTEM, ARRAYSIZE(args), args);
	}
	free(dname);
	if (!device)
		goto fail;

	if (!path)
		goto fail;

	{
		BOOL isSpecial = FALSE;
		BOOL isPath = freerdp_path_valid(path, &isSpecial);

		if (!isPath && !isSpecial)
		{
			WLog_WARN(TAG, "Invalid drive to redirect: '%s' does not exist, skipping.", path);
			freerdp_device_free(device);
		}
		else if (!freerdp_device_collection_add(settings, device))
			goto fail;
	}

	return TRUE;

fail:
	freerdp_device_free(device);
	return FALSE;
}

static BOOL value_to_int(const char* value, LONGLONG* result, LONGLONG min, LONGLONG max)
{
	long long rc = 0;

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
	unsigned long long rc = 0;

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
	printf("This is FreeRDP version %s (%s)\n", FREERDP_VERSION_FULL, FREERDP_GIT_REVISION);
	return TRUE;
}

BOOL freerdp_client_print_version_ex(int argc, char** argv)
{
	WINPR_ASSERT(argc >= 0);
	WINPR_ASSERT(argv || (argc == 0));
	const char* name = (argc > 0) ? argv[0] : "argc < 1";
	printf("This is FreeRDP version [%s] %s (%s)\n", name, FREERDP_VERSION_FULL,
	       FREERDP_GIT_REVISION);
	return TRUE;
}

BOOL freerdp_client_print_buildconfig(void)
{
	printf("%s", freerdp_get_build_config());
	return TRUE;
}

BOOL freerdp_client_print_buildconfig_ex(int argc, char** argv)
{
	WINPR_ASSERT(argc >= 0);
	WINPR_ASSERT(argv || (argc == 0));
	const char* name = (argc > 0) ? argv[0] : "argc < 1";
	printf("[%s] %s", name, freerdp_get_build_config());
	return TRUE;
}

static void freerdp_client_print_scancodes(void)
{
	printf("RDP scancodes and their name for use with /kbd:remap\n");

	for (UINT32 x = 0; x < UINT16_MAX; x++)
	{
		const char* name = freerdp_keyboard_scancode_name(x);
		if (name)
			printf("0x%04" PRIx32 "  --> %s\n", x, name);
	}
}

static BOOL is_delimiter(char c, const char* delimiters)
{
	char d = 0;
	while ((d = *delimiters++) != '\0')
	{
		if (c == d)
			return TRUE;
	}
	return FALSE;
}

static const char* get_last(const char* start, size_t len, const char* delimiters)
{
	const char* last = NULL;
	for (size_t x = 0; x < len; x++)
	{
		char c = start[x];
		if (is_delimiter(c, delimiters))
			last = &start[x];
	}
	return last;
}

static SSIZE_T next_delimiter(const char* text, size_t len, size_t max, const char* delimiters)
{
	if (len < max)
		return -1;

	const char* last = get_last(text, max, delimiters);
	if (!last)
		return -1;

	return (SSIZE_T)(last - text);
}

static SSIZE_T forced_newline_at(const char* text, size_t len, size_t limit,
                                 const char* force_newline)
{
	char d = 0;
	while ((d = *force_newline++) != '\0')
	{
		const char* tok = strchr(text, d);
		if (tok)
		{
			const size_t offset = tok - text;
			if ((offset > len) || (offset > limit))
				continue;
			return (SSIZE_T)(offset);
		}
	}
	return -1;
}

static BOOL print_align(size_t start_offset, size_t* current)
{
	WINPR_ASSERT(current);
	if (*current < start_offset)
	{
		const int rc = printf("%*c", (int)(start_offset - *current), ' ');
		if (rc < 0)
			return FALSE;
		*current += (size_t)rc;
	}
	return TRUE;
}

static char* print_token(char* text, size_t start_offset, size_t* current, size_t limit,
                         const char* delimiters, const char* force_newline)
{
	int rc = 0;
	const size_t tlen = strnlen(text, limit);
	size_t len = tlen;
	const SSIZE_T force_at = forced_newline_at(text, len, limit - *current, force_newline);
	BOOL isForce = (force_at >= 0);

	if (isForce)
		len = MIN(len, (size_t)force_at);

	if (!print_align(start_offset, current))
		return NULL;

	const SSIZE_T delim = next_delimiter(text, len, limit - *current, delimiters);
	const BOOL isDelim = delim > 0;
	if (isDelim)
	{
		len = MIN(len, (size_t)delim + 1);
	}

	rc = printf("%.*s", (int)len, text);
	if (rc < 0)
		return NULL;

	if (isForce || isDelim)
	{
		printf("\n");
		*current = 0;

		const size_t offset = len + ((isForce && (force_at == 0)) ? 1 : 0);
		return &text[offset];
	}

	*current += (size_t)rc;

	if (tlen == (size_t)rc)
		return NULL;
	return &text[(size_t)rc];
}

static size_t print_optionals(const char* text, size_t start_offset, size_t current)
{
	const size_t limit = 80;
	char* str = _strdup(text);
	char* cur = str;

	do
	{
		cur = print_token(cur, start_offset + 1, &current, limit, "[], ", "\r\n");
	} while (cur != NULL);

	free(str);
	return current;
}

static size_t print_description(const char* text, size_t start_offset, size_t current)
{
	const size_t limit = 80;
	char* str = _strdup(text);
	char* cur = str;

	while (cur != NULL)
		cur = print_token(cur, start_offset, &current, limit, " ", "\r\n");

	free(str);
	const int rc = printf("\n");
	if (rc >= 0)
	{
		WINPR_ASSERT(SIZE_MAX - rc > current);
		current += (size_t)rc;
	}
	return current;
}

static int cmp_cmdline_args(const void* pva, const void* pvb)
{
	const COMMAND_LINE_ARGUMENT_A* a = (const COMMAND_LINE_ARGUMENT_A*)pva;
	const COMMAND_LINE_ARGUMENT_A* b = (const COMMAND_LINE_ARGUMENT_A*)pvb;

	if (!a->Name && !b->Name)
		return 0;
	if (!a->Name)
		return 1;
	if (!b->Name)
		return -1;
	return strcmp(a->Name, b->Name);
}

static void freerdp_client_print_command_line_args(COMMAND_LINE_ARGUMENT_A* parg, size_t count)
{
	if (!parg)
		return;

	qsort(parg, count, sizeof(COMMAND_LINE_ARGUMENT_A), cmp_cmdline_args);

	const COMMAND_LINE_ARGUMENT_A* arg = parg;
	do
	{
		int rc = 0;
		size_t pos = 0;
		const size_t description_offset = 30 + 8;

		if (arg->Flags & (COMMAND_LINE_VALUE_BOOL | COMMAND_LINE_VALUE_FLAG))
		{
			if ((arg->Flags & ~COMMAND_LINE_VALUE_BOOL) == 0)
				rc = printf("    %s%s", arg->Default ? "-" : "+", arg->Name);
			else if ((arg->Flags & COMMAND_LINE_VALUE_OPTIONAL) != 0)
				rc = printf("    [%s|/]%s", arg->Default ? "-" : "+", arg->Name);
			else
			{
				rc = printf("    %s%s", arg->Default ? "-" : "+", arg->Name);
			}
		}
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

static COMMAND_LINE_ARGUMENT_A* create_merged_args(const COMMAND_LINE_ARGUMENT_A* custom,
                                                   SSIZE_T count, size_t* pcount)
{
	WINPR_ASSERT(pcount);
	if (count < 0)
	{
		const COMMAND_LINE_ARGUMENT_A* cur = custom;
		count = 0;
		while (cur && cur->Name)
		{
			count++;
			cur++;
		}
	}

	COMMAND_LINE_ARGUMENT_A* largs =
	    calloc((size_t)count + ARRAYSIZE(global_cmd_args), sizeof(COMMAND_LINE_ARGUMENT_A));
	*pcount = 0;
	if (!largs)
		return NULL;

	size_t lcount = 0;
	const COMMAND_LINE_ARGUMENT_A* cur = custom;
	while (cur && cur->Name)
	{
		largs[lcount++] = *cur++;
	}

	cur = global_cmd_args;
	while (cur && cur->Name)
	{
		largs[lcount++] = *cur++;
	}
	*pcount = lcount;
	return largs;
}

BOOL freerdp_client_print_command_line_help_ex(int argc, char** argv,
                                               const COMMAND_LINE_ARGUMENT_A* custom)
{
	const char* name = "FreeRDP";

	/* allocate a merged copy of implementation defined and default arguments */
	size_t lcount = 0;
	COMMAND_LINE_ARGUMENT_A* largs = create_merged_args(custom, -1, &lcount);
	if (!largs)
		return FALSE;

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

	freerdp_client_print_command_line_args(largs, lcount);
	free(largs);

	printf("\n");
	printf("Examples:\n");
	printf("    %s connection.rdp /p:Pwd123! /f\n", name);
	printf("    %s /u:CONTOSO\\JohnDoe /p:Pwd123! /v:rdp.contoso.com\n", name);
	printf("    %s /u:JohnDoe /p:Pwd123! /w:1366 /h:768 /v:192.168.1.100:4489\n", name);
	printf("    %s /u:JohnDoe /p:Pwd123! /vmconnect:C824F53E-95D2-46C6-9A18-23A5BB403532 "
	       "/v:192.168.1.100\n",
	       name);
	printf("    %s /u:\\AzureAD\\user@corp.example /p:pwd /v:host\n", name);
	printf("\n");
	printf("Clipboard Redirection: +clipboard\n");
	printf("\n");
	printf("Drive Redirection: /drive:home,/home/user\n");
	printf("Smartcard Redirection: /smartcard:<device>\n");
	printf("Smartcard logon with Kerberos authentication: /smartcard-logon /sec:nla\n");

#if defined(CHANNEL_SERIAL_CLIENT)
	printf("Serial Port Redirection: /serial:<name>,<device>,[SerCx2|SerCx|Serial],[permissive]\n");
	printf("Serial Port Redirection: /serial:COM1,/dev/ttyS0\n");
#endif
#if defined(CHANNEL_PARALLEL_CLIENT)
	printf("Parallel Port Redirection: /parallel:<name>,<device>\n");
#endif
	printf("Printer Redirection: /printer:<device>,<driver>,[default]\n");
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

static BOOL option_is_rdp_file(const char* option)
{
	WINPR_ASSERT(option);

	if (option_ends_with(option, ".rdp"))
		return TRUE;
	if (option_ends_with(option, ".rdpw"))
		return TRUE;
	return FALSE;
}

static BOOL option_is_incident_file(const char* option)
{
	WINPR_ASSERT(option);

	if (option_ends_with(option, ".msrcIncident"))
		return TRUE;
	return FALSE;
}

static int freerdp_client_command_line_pre_filter(void* context, int index, int argc, LPSTR* argv)
{
	if (index == 1)
	{
		size_t length = 0;
		rdpSettings* settings = NULL;

		if (argc <= index)
			return -1;

		length = strlen(argv[index]);

		if (length > 4)
		{
			if (option_is_rdp_file(argv[index]))
			{
				settings = (rdpSettings*)context;

				if (!freerdp_settings_set_string(settings, FreeRDP_ConnectionFile, argv[index]))
					return COMMAND_LINE_ERROR_MEMORY;

				return 1;
			}
		}

		if (length > 13)
		{
			if (option_is_incident_file(argv[index]))
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

BOOL freerdp_client_add_device_channel(rdpSettings* settings, size_t count,
                                       const char* const* params)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(params);
	WINPR_ASSERT(count > 0);

	if (option_equals(params[0], "drive"))
	{
		BOOL rc = 0;
		if (count < 2)
			return FALSE;

		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;
		if (count < 3)
			rc = freerdp_client_add_drive(settings, params[1], NULL);
		else
			rc = freerdp_client_add_drive(settings, params[2], params[1]);

		return rc;
	}
	else if (option_equals(params[0], "printer"))
	{
		RDPDR_DEVICE* printer = NULL;

		if (count < 1)
			return FALSE;

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectPrinters, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

		printer = freerdp_device_new(RDPDR_DTYP_PRINT, count - 1, &params[1]);
		if (!printer)
			return FALSE;

		if (!freerdp_device_collection_add(settings, printer))
		{
			freerdp_device_free(printer);
			return FALSE;
		}

		return TRUE;
	}
	else if (option_equals(params[0], "smartcard"))
	{
		RDPDR_DEVICE* smartcard = NULL;

		if (count < 1)
			return FALSE;

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

		smartcard = freerdp_device_new(RDPDR_DTYP_SMARTCARD, count - 1, &params[1]);

		if (!smartcard)
			return FALSE;

		if (!freerdp_device_collection_add(settings, smartcard))
		{
			freerdp_device_free(smartcard);
			return FALSE;
		}

		return TRUE;
	}
#if defined(CHANNEL_SERIAL_CLIENT)
	else if (option_equals(params[0], "serial"))
	{
		RDPDR_DEVICE* serial = NULL;

		if (count < 1)
			return FALSE;

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSerialPorts, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

		serial = freerdp_device_new(RDPDR_DTYP_SERIAL, count - 1, &params[1]);

		if (!serial)
			return FALSE;

		if (!freerdp_device_collection_add(settings, serial))
		{
			freerdp_device_free(serial);
			return FALSE;
		}

		return TRUE;
	}
#endif
	else if (option_equals(params[0], "parallel"))
	{
		RDPDR_DEVICE* parallel = NULL;

		if (count < 1)
			return FALSE;

		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectParallelPorts, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE;

		parallel = freerdp_device_new(RDPDR_DTYP_PARALLEL, count - 1, &params[1]);

		if (!parallel)
			return FALSE;

		if (!freerdp_device_collection_add(settings, parallel))
		{
			freerdp_device_free(parallel);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

BOOL freerdp_client_del_static_channel(rdpSettings* settings, const char* name)
{
	return freerdp_static_channel_collection_del(settings, name);
}

BOOL freerdp_client_add_static_channel(rdpSettings* settings, size_t count,
                                       const char* const* params)
{
	ADDIN_ARGV* _args = NULL;

	if (!settings || !params || !params[0] || (count > INT_MAX))
		return FALSE;

	if (freerdp_static_channel_collection_find(settings, params[0]))
		return TRUE;

	_args = freerdp_addin_argv_new(count, params);

	if (!_args)
		return FALSE;

	if (!freerdp_static_channel_collection_add(settings, _args))
		goto fail;

	return TRUE;
fail:
	freerdp_addin_argv_free(_args);
	return FALSE;
}

BOOL freerdp_client_del_dynamic_channel(rdpSettings* settings, const char* name)
{
	return freerdp_dynamic_channel_collection_del(settings, name);
}

BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, size_t count,
                                        const char* const* params)
{
	ADDIN_ARGV* _args = NULL;

	if (!settings || !params || !params[0] || (count > INT_MAX))
		return FALSE;

	if (freerdp_dynamic_channel_collection_find(settings, params[0]))
		return TRUE;

	_args = freerdp_addin_argv_new(count, params);

	if (!_args)
		return FALSE;

	if (!freerdp_dynamic_channel_collection_add(settings, _args))
		goto fail;

	return TRUE;

fail:
	freerdp_addin_argv_free(_args);
	return FALSE;
}

static BOOL read_pem_file(rdpSettings* settings, FreeRDP_Settings_Keys_String id, const char* file)
{
	size_t length = 0;
	char* pem = crypto_read_pem(file, &length);
	if (!pem || (length == 0))
	{
		free(pem);
		return FALSE;
	}

	BOOL rc = freerdp_settings_set_string_len(settings, id, pem, length);
	free(pem);
	return rc;
}

/** @brief suboption type */
typedef enum
{
	CMDLINE_SUBOPTION_STRING,
	CMDLINE_SUBOPTION_FILE,
} CmdLineSubOptionType;

typedef BOOL (*CmdLineSubOptionCb)(const char* value, rdpSettings* settings);
typedef struct
{
	const char* optname;
	FreeRDP_Settings_Keys_String id;
	CmdLineSubOptionType opttype;
	CmdLineSubOptionCb cb;
} CmdLineSubOptions;

static BOOL parseSubOptions(rdpSettings* settings, const CmdLineSubOptions* opts, size_t count,
                            const char* arg)
{
	BOOL found = FALSE;

	for (size_t xx = 0; xx < count; xx++)
	{
		const CmdLineSubOptions* opt = &opts[xx];

		if (option_starts_with(opt->optname, arg))
		{
			const size_t optlen = strlen(opt->optname);
			const char* val = &arg[optlen];
			BOOL status = 0;

			switch (opt->opttype)
			{
				case CMDLINE_SUBOPTION_STRING:
					status = freerdp_settings_set_string(settings, opt->id, val);
					break;
				case CMDLINE_SUBOPTION_FILE:
					status = read_pem_file(settings, opt->id, val);
					break;
				default:
					WLog_ERR(TAG, "invalid subOption type");
					return FALSE;
			}

			if (!status)
				return FALSE;

			if (opt->cb && !opt->cb(val, settings))
				return FALSE;

			found = TRUE;
			break;
		}
	}

	if (!found)
		WLog_ERR(TAG, "option %s not handled", arg);

	return found;
}

#define fail_at(arg, rc) fail_at_((arg), (rc), __FILE__, __func__, __LINE__)
static int fail_at_(const COMMAND_LINE_ARGUMENT_A* arg, int rc, const char* file, const char* fkt,
                    size_t line)
{
	const DWORD level = WLOG_ERROR;
	wLog* log = WLog_Get(TAG);
	if (WLog_IsLevelActive(log, level))
		WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt,
		                  "Command line parsing failed at '%s' value '%s' [%d]", arg->Name,
		                  arg->Value, rc);
	return rc;
}

static int freerdp_client_command_line_post_filter_int(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings = (rdpSettings*)context;
	int status = CHANNEL_RC_OK;
	BOOL enable = arg->Value ? TRUE : FALSE;

	CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "a")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);

		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			status = COMMAND_LINE_ERROR;

		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "kerberos")
	{
		size_t count = 0;

		char** ptr = CommandLineParseCommaSeparatedValuesEx("kerberos", arg->Value, &count);
		if (ptr)
		{
			const CmdLineSubOptions opts[] = {
				{ "kdc-url:", FreeRDP_KerberosKdcUrl, CMDLINE_SUBOPTION_STRING, NULL },
				{ "start-time:", FreeRDP_KerberosStartTime, CMDLINE_SUBOPTION_STRING, NULL },
				{ "lifetime:", FreeRDP_KerberosLifeTime, CMDLINE_SUBOPTION_STRING, NULL },
				{ "renewable-lifetime:", FreeRDP_KerberosRenewableLifeTime,
				  CMDLINE_SUBOPTION_STRING, NULL },
				{ "cache:", FreeRDP_KerberosCache, CMDLINE_SUBOPTION_STRING, NULL },
				{ "armor:", FreeRDP_KerberosArmor, CMDLINE_SUBOPTION_STRING, NULL },
				{ "pkinit-anchors:", FreeRDP_PkinitAnchors, CMDLINE_SUBOPTION_STRING, NULL },
				{ "pkcs11-module:", FreeRDP_Pkcs11Module, CMDLINE_SUBOPTION_STRING, NULL }
			};

			for (size_t x = 1; x < count; x++)
			{
				const char* cur = ptr[x];
				if (!parseSubOptions(settings, opts, ARRAYSIZE(opts), cur))
				{
					CommandLineParserFree(ptr);
					return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
				}
			}
		}
		CommandLineParserFree(ptr);
	}

	CommandLineSwitchCase(arg, "vc")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		if (!freerdp_client_add_static_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		if (!freerdp_client_add_dynamic_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "drive")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
#if defined(CHANNEL_SERIAL_CLIENT)
	CommandLineSwitchCase(arg, "serial")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
#endif
#if defined(CHANNEL_PARALLEL_CLIENT)
	CommandLineSwitchCase(arg, "parallel")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
#endif
	CommandLineSwitchCase(arg, "smartcard")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "printer")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		if (!freerdp_client_add_device_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "usb")
	{
		size_t count = 0;
		char** ptr =
		    CommandLineParseCommaSeparatedValuesEx(URBDRC_CHANNEL_NAME, arg->Value, &count);
		if (!freerdp_client_add_dynamic_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "multitouch")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_MultiTouchInput, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "gestures")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_MultiTouchGestures, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "echo")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportEchoChannel, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "ssh-agent")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportSSHAgentChannel, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "disp")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "geometry")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGeometryTracking, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "video")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGeometryTracking,
		                               enable)) /* this requires geometry tracking */
			return fail_at(arg, COMMAND_LINE_ERROR);
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportVideoOptimized, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "sound")
	{
		size_t count = 0;
		char** ptr =
		    CommandLineParseCommaSeparatedValuesEx(RDPSND_CHANNEL_NAME, arg->Value, &count);
		if (!freerdp_client_add_static_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		if (!freerdp_client_add_dynamic_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
	CommandLineSwitchCase(arg, "microphone")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx(AUDIN_CHANNEL_NAME, arg->Value, &count);
		if (!freerdp_client_add_dynamic_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
#if defined(CHANNEL_TSMF_CLIENT)
	CommandLineSwitchCase(arg, "multimedia")
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValuesEx("tsmf", arg->Value, &count);
		if (!freerdp_client_add_dynamic_channel(settings, count, (const char* const*)ptr))
			status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		CommandLineParserFree(ptr);
		if (status)
			return fail_at(arg, status);
	}
#endif
	CommandLineSwitchCase(arg, "heartbeat")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportHeartbeatPdu, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchCase(arg, "multitransport")
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, enable))
			return fail_at(arg, COMMAND_LINE_ERROR);

		UINT32 flags = 0;
		if (freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport))
			flags =
			    (TRANSPORT_TYPE_UDP_FECR | TRANSPORT_TYPE_UDP_FECL | TRANSPORT_TYPE_UDP_PREFERRED);

		if (!freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags, flags))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}
	CommandLineSwitchEnd(arg)

	    return status;
}

static int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	int status = freerdp_client_command_line_post_filter_int(context, arg);
	return status == CHANNEL_RC_OK ? 1 : -1;
}

static BOOL freerdp_parse_username_ptr(const char* username, const char** user, size_t* userlen,
                                       const char** domain, size_t* domainlen)
{
	WINPR_ASSERT(user);
	WINPR_ASSERT(userlen);
	WINPR_ASSERT(domain);
	WINPR_ASSERT(domainlen);

	if (!username)
		return FALSE;

	const char* p = strchr(username, '\\');

	*user = NULL;
	*userlen = 0;

	*domain = NULL;
	*domainlen = 0;

	if (p)
	{
		const size_t length = (size_t)(p - username);
		*user = &p[1];
		*userlen = strlen(*user);

		*domain = username;
		*domainlen = length;
	}
	else
	{
		/* Do not break up the name for '@'; both credSSP and the
		 * ClientInfo PDU expect 'user@corp.net' to be transmitted
		 * as username 'user@corp.net', domain empty (not NULL!).
		 */
		*user = username;
		*userlen = strlen(username);
	}

	return TRUE;
}

static BOOL freerdp_parse_username_settings(const char* username, rdpSettings* settings,
                                            FreeRDP_Settings_Keys_String userID,
                                            FreeRDP_Settings_Keys_String domainID)
{
	const char* user = NULL;
	const char* domain = NULL;
	size_t userlen = 0;
	size_t domainlen = 0;

	const BOOL rc = freerdp_parse_username_ptr(username, &user, &userlen, &domain, &domainlen);
	if (!rc)
		return FALSE;
	if (!freerdp_settings_set_string_len(settings, userID, user, userlen))
		return FALSE;
	return freerdp_settings_set_string_len(settings, domainID, domain, domainlen);
}

BOOL freerdp_parse_username(const char* username, char** puser, char** pdomain)
{
	const char* user = NULL;
	const char* domain = NULL;
	size_t userlen = 0;
	size_t domainlen = 0;

	*puser = NULL;
	*pdomain = NULL;

	const BOOL rc = freerdp_parse_username_ptr(username, &user, &userlen, &domain, &domainlen);
	if (!rc)
		return FALSE;

	if (userlen > 0)
	{
		*puser = strndup(user, userlen);
		if (!*puser)
			return FALSE;
	}

	if (domainlen > 0)
	{
		*pdomain = strndup(domain, domainlen);
		if (!*pdomain)
		{
			free(*puser);
			*puser = NULL;
			return FALSE;
		}
	}

	return TRUE;
}

BOOL freerdp_parse_hostname(const char* hostname, char** host, int* port)
{
	char* p = NULL;
	p = strrchr(hostname, ':');

	if (p)
	{
		size_t length = (size_t)(p - hostname);
		LONGLONG val = 0;

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

static BOOL freerdp_apply_connection_type(rdpSettings* settings, UINT32 type)
{
	struct network_settings
	{
		FreeRDP_Settings_Keys_Bool id;
		BOOL value[7];
	};
	const struct network_settings config[] = {
		{ FreeRDP_DisableWallpaper, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_AllowFontSmoothing, { FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE } },
		{ FreeRDP_AllowDesktopComposition, { FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE } },
		{ FreeRDP_DisableFullWindowDrag, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_DisableMenuAnims, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_DisableThemes, { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE } }
	};

	switch (type)
	{
		case CONNECTION_TYPE_INVALID:
			return TRUE;

		case CONNECTION_TYPE_MODEM:
		case CONNECTION_TYPE_BROADBAND_LOW:
		case CONNECTION_TYPE_BROADBAND_HIGH:
		case CONNECTION_TYPE_SATELLITE:
		case CONNECTION_TYPE_WAN:
		case CONNECTION_TYPE_LAN:
		case CONNECTION_TYPE_AUTODETECT:
			break;
		default:
			WLog_WARN(TAG, "Unknown ConnectionType %" PRIu32 ", aborting", type);
			return FALSE;
	}

	for (size_t x = 0; x < ARRAYSIZE(config); x++)
	{
		const struct network_settings* cur = &config[x];
		if (!freerdp_settings_set_bool(settings, cur->id, cur->value[type - 1]))
			return FALSE;
	}
	return TRUE;
}

BOOL freerdp_set_connection_type(rdpSettings* settings, UINT32 type)
{

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType, type))
		return FALSE;

	switch (type)
	{
		case CONNECTION_TYPE_INVALID:
		case CONNECTION_TYPE_MODEM:
		case CONNECTION_TYPE_BROADBAND_LOW:
		case CONNECTION_TYPE_SATELLITE:
		case CONNECTION_TYPE_BROADBAND_HIGH:
		case CONNECTION_TYPE_WAN:
		case CONNECTION_TYPE_LAN:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_AUTODETECT:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
				/* Automatically activate GFX and RFX codec support */
#ifdef WITH_GFX_H264
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE))
				return FALSE;
#endif
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
				return FALSE;
			break;
		default:
			WLog_WARN(TAG, "Unknown ConnectionType %" PRIu32 ", aborting", type);
			return FALSE;
	}

	return TRUE;
}

static UINT32 freerdp_get_keyboard_layout_for_type(const char* name, DWORD type)
{
	UINT32 res = 0;
	size_t count = 0;
	RDP_KEYBOARD_LAYOUT* layouts =
	    freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD, &count);

	if (!layouts || (count == 0))
		goto fail;

	for (size_t x = 0; x < count; x++)
	{
		const RDP_KEYBOARD_LAYOUT* layout = &layouts[x];
		if (option_equals(layout->name, name))
		{
			res = layout->code;
			break;
		}
	}

fail:
	freerdp_keyboard_layouts_free(layouts, count);
	return res;
}

static UINT32 freerdp_map_keyboard_layout_name_to_id(const char* name)
{
	const UINT32 variants[] = { RDP_KEYBOARD_LAYOUT_TYPE_STANDARD, RDP_KEYBOARD_LAYOUT_TYPE_VARIANT,
		                        RDP_KEYBOARD_LAYOUT_TYPE_IME };

	for (size_t x = 0; x < ARRAYSIZE(variants); x++)
	{
		UINT32 rc = freerdp_get_keyboard_layout_for_type(name, variants[x]);
		if (rc > 0)
			return rc;
	}

	return 0;
}

static int freerdp_detect_command_line_pre_filter(void* context, int index, int argc, LPSTR* argv)
{
	size_t length = 0;
	WINPR_UNUSED(context);

	if (index == 1)
	{
		if (argc < index)
			return -1;

		length = strlen(argv[index]);

		if (length > 4)
		{
			if (option_is_rdp_file(argv[index]))
			{
				return 1;
			}
		}

		if (length > 13)
		{
			if (option_is_incident_file(argv[index]))
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
	int status = 0;
	DWORD flags = 0;
	int detect_status = 0;
	const COMMAND_LINE_ARGUMENT_A* arg = NULL;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

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
	int status = 0;
	DWORD flags = 0;
	int detect_status = 0;
	const COMMAND_LINE_ARGUMENT_A* arg = NULL;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

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
	int posix_cli_status = 0;
	size_t posix_cli_count = 0;
	int windows_cli_status = 0;
	size_t windows_cli_count = 0;
	const BOOL ignoreUnknown = TRUE;
	windows_cli_status = freerdp_detect_windows_style_command_line_syntax(
	    argc, argv, &windows_cli_count, ignoreUnknown);
	posix_cli_status =
	    freerdp_detect_posix_style_command_line_syntax(argc, argv, &posix_cli_count, ignoreUnknown);

	/* Default is POSIX syntax */
	*flags = COMMAND_LINE_SEPARATOR_SPACE;
	*flags |= COMMAND_LINE_SIGIL_DASH | COMMAND_LINE_SIGIL_DOUBLE_DASH;
	*flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;

	if (posix_cli_status <= COMMAND_LINE_STATUS_PRINT)
		return FALSE;

	/* Check, if this may be windows style syntax... */
	if ((windows_cli_count && (windows_cli_count >= posix_cli_count)) ||
	    (windows_cli_status <= COMMAND_LINE_STATUS_PRINT))
	{
		windows_cli_count = 1;
		*flags = COMMAND_LINE_SEPARATOR_COLON;
		*flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;
	}

	WLog_DBG(TAG, "windows: %d/%" PRIuz " posix: %d/%" PRIuz "", windows_cli_status,
	         windows_cli_count, posix_cli_status, posix_cli_count);
	if ((posix_cli_count == 0) && (windows_cli_count == 0))
	{
		if ((posix_cli_status == COMMAND_LINE_ERROR) && (windows_cli_status == COMMAND_LINE_ERROR))
			return TRUE;
	}
	return FALSE;
}

int freerdp_client_settings_command_line_status_print(rdpSettings* settings, int status, int argc,
                                                      char** argv)
{
	return freerdp_client_settings_command_line_status_print_ex(settings, status, argc, argv, NULL);
}

static void freerdp_client_print_keyboard_type_list(const char* msg, DWORD type)
{
	size_t count = 0;
	RDP_KEYBOARD_LAYOUT* layouts = NULL;
	layouts = freerdp_keyboard_get_layouts(type, &count);

	printf("\n%s\n", msg);

	for (size_t x = 0; x < count; x++)
	{
		const RDP_KEYBOARD_LAYOUT* layout = &layouts[x];
		printf("0x%08" PRIX32 "\t%s\n", layout->code, layout->name);
	}

	freerdp_keyboard_layouts_free(layouts, count);
}

static void freerdp_client_print_keyboard_list(void)
{
	freerdp_client_print_keyboard_type_list("Keyboard Layouts", RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
	freerdp_client_print_keyboard_type_list("Keyboard Layout Variants",
	                                        RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
	freerdp_client_print_keyboard_type_list("Keyboard Layout Variants",
	                                        RDP_KEYBOARD_LAYOUT_TYPE_IME);
}

static void freerdp_client_print_timezone_list(void)
{
	DWORD index = 0;
	DYNAMIC_TIME_ZONE_INFORMATION info = { 0 };
	while (EnumDynamicTimeZoneInformation(index++, &info) != ERROR_NO_MORE_ITEMS)
	{
		char TimeZoneKeyName[ARRAYSIZE(info.TimeZoneKeyName) + 1] = { 0 };

		(void)ConvertWCharNToUtf8(info.TimeZoneKeyName, ARRAYSIZE(info.TimeZoneKeyName),
		                          TimeZoneKeyName, ARRAYSIZE(TimeZoneKeyName));
		printf("%" PRIu32 ": '%s'\n", index, TimeZoneKeyName);
	}
}

static void freerdp_client_print_tune_list(const rdpSettings* settings)
{
	SSIZE_T type = 0;

	for (SSIZE_T x = 0; x < FreeRDP_Settings_StableAPI_MAX; x++)
	{
		const char* name = freerdp_settings_get_name_for_key(x);
		type = freerdp_settings_get_type_for_key(x);

		// NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
		switch (type)
		{
			case RDP_SETTINGS_TYPE_BOOL:
				printf("%" PRIdz "\t%50s\tBOOL\t%s\n", x, name,
				       freerdp_settings_get_bool(settings, (FreeRDP_Settings_Keys_Bool)x)
				           ? "TRUE"
				           : "FALSE");
				break;
			case RDP_SETTINGS_TYPE_UINT16:
				printf("%" PRIdz "\t%50s\tUINT16\t%" PRIu16 "\n", x, name,
				       freerdp_settings_get_uint16(settings, (FreeRDP_Settings_Keys_UInt16)x));
				break;
			case RDP_SETTINGS_TYPE_INT16:
				printf("%" PRIdz "\t%50s\tINT16\t%" PRId16 "\n", x, name,
				       freerdp_settings_get_int16(settings, (FreeRDP_Settings_Keys_Int16)x));
				break;
			case RDP_SETTINGS_TYPE_UINT32:
				printf("%" PRIdz "\t%50s\tUINT32\t%" PRIu32 "\n", x, name,
				       freerdp_settings_get_uint32(settings, (FreeRDP_Settings_Keys_UInt32)x));
				break;
			case RDP_SETTINGS_TYPE_INT32:
				printf("%" PRIdz "\t%50s\tINT32\t%" PRId32 "\n", x, name,
				       freerdp_settings_get_int32(settings, (FreeRDP_Settings_Keys_Int32)x));
				break;
			case RDP_SETTINGS_TYPE_UINT64:
				printf("%" PRIdz "\t%50s\tUINT64\t%" PRIu64 "\n", x, name,
				       freerdp_settings_get_uint64(settings, (FreeRDP_Settings_Keys_UInt64)x));
				break;
			case RDP_SETTINGS_TYPE_INT64:
				printf("%" PRIdz "\t%50s\tINT64\t%" PRId64 "\n", x, name,
				       freerdp_settings_get_int64(settings, (FreeRDP_Settings_Keys_Int64)x));
				break;
			case RDP_SETTINGS_TYPE_STRING:
				printf("%" PRIdz "\t%50s\tSTRING\t%s"
				       "\n",
				       x, name,
				       freerdp_settings_get_string(settings, (FreeRDP_Settings_Keys_String)x));
				break;
			case RDP_SETTINGS_TYPE_POINTER:
				printf("%" PRIdz "\t%50s\tPOINTER\t%p"
				       "\n",
				       x, name,
				       freerdp_settings_get_pointer(settings, (FreeRDP_Settings_Keys_Pointer)x));
				break;
			default:
				break;
		}
		// NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
	}
}

int freerdp_client_settings_command_line_status_print_ex(rdpSettings* settings, int status,
                                                         int argc, char** argv,
                                                         const COMMAND_LINE_ARGUMENT_A* custom)
{
	const COMMAND_LINE_ARGUMENT_A* arg = NULL;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
		goto out;
	}

	if (status == COMMAND_LINE_STATUS_PRINT_BUILDCONFIG)
	{
		freerdp_client_print_version_ex(argc, argv);
		freerdp_client_print_buildconfig_ex(argc, argv);
		goto out;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		(void)CommandLineParseArgumentsA(argc, argv, largs, 0x112, NULL, NULL, NULL);

		arg = CommandLineFindArgumentA(largs, "list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
		{
			if (option_equals("timezones", arg->Value))
				freerdp_client_print_timezone_list();
			else if (option_equals("tune", arg->Value))
				freerdp_client_print_tune_list(settings);
			else if (option_equals("kbd", arg->Value))
				freerdp_client_print_keyboard_list();
			else if (option_starts_with("kbd-lang", arg->Value))
			{
				const char* val = NULL;
				if (option_starts_with("kbd-lang:", arg->Value))
					val = &arg->Value[9];
				else if (!option_equals("kbd-lang", arg->Value))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (val && strchr(val, ','))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				freerdp_client_print_codepages(val);
			}
			else if (option_equals("kbd-scancode", arg->Value))
				freerdp_client_print_scancodes();
			else if (option_equals("monitor", arg->Value))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_ListMonitors, TRUE))
					return COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("smartcard", arg->Value))
			{
				BOOL opts = FALSE;
				if (option_starts_with("smartcard:", arg->Value))
					opts = TRUE;
				else if (!option_equals("smartcard", arg->Value))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

				if (opts)
				{
					const char* sub = strchr(arg->Value, ':') + 1;
					const CmdLineSubOptions options[] = {
						{ "pkinit-anchors:", FreeRDP_PkinitAnchors, CMDLINE_SUBOPTION_STRING,
						  NULL },
						{ "pkcs11-module:", FreeRDP_Pkcs11Module, CMDLINE_SUBOPTION_STRING, NULL }
					};

					size_t count = 0;

					char** ptr = CommandLineParseCommaSeparatedValuesEx("smartcard", sub, &count);
					if (!ptr)
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					if (count < 2)
					{
						CommandLineParserFree(ptr);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}

					for (size_t x = 1; x < count; x++)
					{
						const char* cur = ptr[x];
						if (!parseSubOptions(settings, options, ARRAYSIZE(options), cur))
						{
							CommandLineParserFree(ptr);
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						}
					}

					CommandLineParserFree(ptr);
				}

				freerdp_smartcard_list(settings);
			}
			else
			{
				freerdp_client_print_command_line_help_ex(argc, argv, custom);
				return COMMAND_LINE_ERROR;
			}
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		arg = CommandLineFindArgumentA(largs, "tune-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
		{
			WLog_WARN(TAG, "Option /tune-list is deprecated, use /list:tune instead");
			freerdp_client_print_tune_list(settings);
		}

		arg = CommandLineFindArgumentA(largs, "kbd-lang-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
		{
			WLog_WARN(TAG, "Option /kbd-lang-list is deprecated, use /list:kbd-lang instead");
			freerdp_client_print_codepages(arg->Value);
		}

		arg = CommandLineFindArgumentA(largs, "kbd-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			WLog_WARN(TAG, "Option /kbd-list is deprecated, use /list:kbd instead");
			freerdp_client_print_keyboard_list();
		}

		arg = CommandLineFindArgumentA(largs, "monitor-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			WLog_WARN(TAG, "Option /monitor-list is deprecated, use /list:monitor instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_ListMonitors, TRUE))
				return COMMAND_LINE_ERROR;
		}

		arg = CommandLineFindArgumentA(largs, "smartcard-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			WLog_WARN(TAG, "Option /smartcard-list is deprecated, use /list:smartcard instead");
			freerdp_smartcard_list(settings);
		}

		arg = CommandLineFindArgumentA(largs, "kbd-scancode-list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			WLog_WARN(TAG,
			          "Option /kbd-scancode-list is deprecated, use /list:kbd-scancode instead");
			freerdp_client_print_scancodes();
			goto out;
		}
#endif
		goto out;
	}
	else if (status < 0)
	{
		freerdp_client_print_command_line_help_ex(argc, argv, custom);
		goto out;
	}

out:
	if (status <= COMMAND_LINE_STATUS_PRINT && status >= COMMAND_LINE_STATUS_PRINT_LAST)
		return 0;
	return status;
}

/**
 * parses a string value with the format <v1>x<v2>
 *
 * @param input input string
 * @param v1 pointer to output v1
 * @param v2 pointer to output v2
 * @return if the parsing was successful
 */
static BOOL parseSizeValue(const char* input, unsigned long* v1, unsigned long* v2)
{
	const char* xcharpos = NULL;
	char* endPtr = NULL;
	unsigned long v = 0;
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

static BOOL prepare_default_settings(rdpSettings* settings, COMMAND_LINE_ARGUMENT_A* args,
                                     BOOL rdp_file)
{
	const char* arguments[] = { "network", "gfx", "rfx", "bpp" };
	WINPR_ASSERT(settings);
	WINPR_ASSERT(args);

	if (rdp_file)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(arguments); x++)
	{
		const char* arg = arguments[x];
		const COMMAND_LINE_ARGUMENT_A* p = CommandLineFindArgumentA(args, arg);
		if (p && (p->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			return FALSE;
	}

	return freerdp_set_connection_type(settings, CONNECTION_TYPE_AUTODETECT);
}

static BOOL setSmartcardEmulation(const char* value, rdpSettings* settings)
{
	return freerdp_settings_set_bool(settings, FreeRDP_SmartcardEmulation, TRUE);
}

const char* option_starts_with(const char* what, const char* val)
{
	WINPR_ASSERT(what);
	WINPR_ASSERT(val);
	const size_t wlen = strlen(what);

	if (_strnicmp(what, val, wlen) != 0)
		return NULL;
	return &val[wlen];
}

BOOL option_ends_with(const char* str, const char* ext)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(ext);
	const size_t strLen = strlen(str);
	const size_t extLen = strlen(ext);

	if (strLen < extLen)
		return FALSE;

	return _strnicmp(&str[strLen - extLen], ext, extLen) == 0;
}

BOOL option_equals(const char* what, const char* val)
{
	WINPR_ASSERT(what);
	WINPR_ASSERT(val);
	return _stricmp(what, val) == 0;
}

typedef enum
{
	PARSE_ON,
	PARSE_OFF,
	PARSE_NONE,
	PARSE_FAIL
} PARSE_ON_OFF_RESULT;

static PARSE_ON_OFF_RESULT parse_on_off_option(const char* value)
{
	WINPR_ASSERT(value);
	const char* sep = strchr(value, ':');
	if (!sep)
		return PARSE_NONE;
	if (option_equals("on", &sep[1]))
		return PARSE_ON;
	if (option_equals("off", &sep[1]))
		return PARSE_OFF;
	return PARSE_FAIL;
}

typedef enum
{
	CLIP_DIR_PARSE_ALL,
	CLIP_DIR_PARSE_OFF,
	CLIP_DIR_PARSE_LOCAL,
	CLIP_DIR_PARSE_REMOTE,
	CLIP_DIR_PARSE_FAIL
} PARSE_CLIP_DIR_RESULT;

static PARSE_CLIP_DIR_RESULT parse_clip_direciton_to_option(const char* value)
{
	WINPR_ASSERT(value);
	const char* sep = strchr(value, ':');
	if (!sep)
		return CLIP_DIR_PARSE_FAIL;
	if (option_equals("all", &sep[1]))
		return CLIP_DIR_PARSE_ALL;
	if (option_equals("off", &sep[1]))
		return CLIP_DIR_PARSE_OFF;
	if (option_equals("local", &sep[1]))
		return CLIP_DIR_PARSE_LOCAL;
	if (option_equals("remote", &sep[1]))
		return CLIP_DIR_PARSE_REMOTE;
	return CLIP_DIR_PARSE_FAIL;
}

static int parse_tls_ciphers(rdpSettings* settings, const char* Value)
{
	const char* ciphers = NULL;
	if (!Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	if (option_equals(Value, "netmon"))
	{
		ciphers = "ALL:!ECDH:!ADH:!DHE";
	}
	else if (option_equals(Value, "ma"))
	{
		ciphers = "AES128-SHA";
	}
	else
	{
		ciphers = Value;
	}

	if (!freerdp_settings_set_string(settings, FreeRDP_AllowedTlsCiphers, ciphers))
		return COMMAND_LINE_ERROR_MEMORY;
	return 0;
}

static int parse_tls_seclevel(rdpSettings* settings, const char* Value)
{
	LONGLONG val = 0;

	if (!value_to_int(Value, &val, 0, 5))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, (UINT32)val))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	return 0;
}

static int parse_tls_secrets_file(rdpSettings* settings, const char* Value)
{
	if (!Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	if (!freerdp_settings_set_string(settings, FreeRDP_TlsSecretsFile, Value))
		return COMMAND_LINE_ERROR_MEMORY;
	return 0;
}

static int parse_tls_enforce(rdpSettings* settings, const char* Value)
{
	UINT16 version = TLS1_2_VERSION;

	if (Value)
	{
		struct map_t
		{
			const char* name;
			UINT16 version;
		};
		const struct map_t map[] = { { "1.0", TLS1_VERSION },
			                         { "1.1", TLS1_1_VERSION },
			                         { "1.2", TLS1_2_VERSION }
#if defined(TLS1_3_VERSION)
			                         ,
			                         { "1.3", TLS1_3_VERSION }
#endif
		};

		for (size_t x = 0; x < ARRAYSIZE(map); x++)
		{
			const struct map_t* cur = &map[x];
			if (option_equals(cur->name, Value))
			{
				version = cur->version;
				break;
			}
		}
	}

	if (!(freerdp_settings_set_uint16(settings, FreeRDP_TLSMinVersion, version) &&
	      freerdp_settings_set_uint16(settings, FreeRDP_TLSMaxVersion, version)))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	return 0;
}

static int parse_tls_cipher_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	int rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "tls")
	{
		if (option_starts_with("ciphers:", arg->Value))
			rc = parse_tls_ciphers(settings, &arg->Value[8]);
		else if (option_starts_with("seclevel:", arg->Value))
			rc = parse_tls_seclevel(settings, &arg->Value[9]);
		else if (option_starts_with("secrets-file:", arg->Value))
			rc = parse_tls_secrets_file(settings, &arg->Value[13]);
		else if (option_starts_with("enforce:", arg->Value))
			rc = parse_tls_enforce(settings, &arg->Value[8]);
	}

#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
	CommandLineSwitchCase(arg, "tls-ciphers")
	{
		WLog_WARN(TAG, "Option /tls-ciphers is deprecated, use /tls:ciphers instead");
		rc = parse_tls_ciphers(settings, arg->Value);
	}
	CommandLineSwitchCase(arg, "tls-seclevel")
	{
		WLog_WARN(TAG, "Option /tls-seclevel is deprecated, use /tls:seclevel instead");
		rc = parse_tls_seclevel(settings, arg->Value);
	}
	CommandLineSwitchCase(arg, "tls-secrets-file")
	{
		WLog_WARN(TAG, "Option /tls-secrets-file is deprecated, use /tls:secrets-file instead");
		rc = parse_tls_secrets_file(settings, arg->Value);
	}
	CommandLineSwitchCase(arg, "enforce-tlsv1_2")
	{
		WLog_WARN(TAG, "Option /enforce-tlsv1_2 is deprecated, use /tls:enforce:1.2 instead");
		rc = parse_tls_enforce(settings, "1.2");
	}
#endif
	CommandLineSwitchDefault(arg)
	{
	}
	CommandLineSwitchEnd(arg)

	    return rc;
}

static int parse_tls_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	for (size_t x = 0; x < count; x++)
	{
		COMMAND_LINE_ARGUMENT_A larg = *arg;
		larg.Value = ptr[x];

		int rc = parse_tls_cipher_options(settings, &larg);
		if (rc != 0)
		{
			CommandLineParserFree(ptr);
			return rc;
		}
	}
	CommandLineParserFree(ptr);
	return 0;
}

static int parse_gfx_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
		return COMMAND_LINE_ERROR;

	if (arg->Value)
	{
		int rc = CHANNEL_RC_OK;
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		if (!ptr || (count == 0))
			rc = COMMAND_LINE_ERROR;
		else
		{
			BOOL GfxH264 = FALSE;
			BOOL GfxAVC444 = FALSE;
			BOOL RemoteFxCodec = FALSE;
			BOOL GfxProgressive = FALSE;
			BOOL codecSelected = FALSE;

			for (size_t x = 0; x < count; x++)
			{
				const char* val = ptr[x];
#ifdef WITH_GFX_H264
				if (option_starts_with("AVC444", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxAVC444 = bval != PARSE_OFF;
					codecSelected = TRUE;
				}
				else if (option_starts_with("AVC420", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxH264 = bval != PARSE_OFF;
					codecSelected = TRUE;
				}
				else
#endif
				    if (option_starts_with("RFX", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						RemoteFxCodec = bval != PARSE_OFF;
					codecSelected = TRUE;
				}
				else if (option_starts_with("progressive", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxProgressive = bval != PARSE_OFF;
					codecSelected = TRUE;
				}
				else if (option_starts_with("mask:", val))
				{
					ULONGLONG v = 0;
					const char* uv = &val[5];
					if (!value_to_uint(uv, &v, 0, UINT32_MAX))
						rc = COMMAND_LINE_ERROR;
					else
					{
						if (!freerdp_settings_set_uint32(settings, FreeRDP_GfxCapsFilter,
						                                 (UINT32)v))
							rc = COMMAND_LINE_ERROR;
					}
				}
				else if (option_starts_with("small-cache", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache,
					                                    bval != PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("thin-client", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient,
					                                    bval != PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
					if ((rc == CHANNEL_RC_OK) && (bval > 0))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache,
						                               bval != PARSE_OFF))
							rc = COMMAND_LINE_ERROR;
					}
				}
				else if (option_starts_with("frame-ack", val))
				{
					const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
					if (bval == PARSE_FAIL)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSuspendFrameAck,
					                                    bval == PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
				}
				else
					rc = COMMAND_LINE_ERROR;
			}

			if ((rc == CHANNEL_RC_OK) && codecSelected)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, GfxAVC444))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, GfxAVC444))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, GfxH264))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, RemoteFxCodec))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, GfxProgressive))
					rc = COMMAND_LINE_ERROR;
			}
		}
		CommandLineParserFree(ptr);
		if (rc != CHANNEL_RC_OK)
			return rc;
	}
	return CHANNEL_RC_OK;
}

static int parse_kbd_layout(rdpSettings* settings, const char* value)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(value);

	int rc = 0;
	LONGLONG ival = 0;
	const BOOL isInt = value_to_int(value, &ival, 1, UINT32_MAX);
	if (!isInt)
	{
		ival = freerdp_map_keyboard_layout_name_to_id(value);

		if (ival == 0)
		{
			WLog_ERR(TAG, "Could not identify keyboard layout: %s", value);
			WLog_ERR(TAG, "Use /list:kbd to list available layouts");
			rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
	}

	if (rc == 0)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, (UINT32)ival))
			rc = COMMAND_LINE_ERROR;
	}
	return rc;
}

#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
static int parse_codec_cache_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!arg->Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheV3Enabled, TRUE))
		return COMMAND_LINE_ERROR;

	if (option_equals(arg->Value, "rfx"))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
			return COMMAND_LINE_ERROR;
	}
	else if (option_equals(arg->Value, "nsc"))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE))
			return COMMAND_LINE_ERROR;
	}

#if defined(WITH_JPEG)
	else if (option_equals(arg->Value, "jpeg"))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, TRUE))
			return COMMAND_LINE_ERROR;

		if (freerdp_settings_get_uint32(settings, FreeRDP_JpegQuality) == 0)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, 75))
				return COMMAND_LINE_ERROR;
		}
	}

#endif
	return 0;
}
#endif

static BOOL check_kbd_remap_valid(const char* token)
{
	DWORD key = 0;
	DWORD value = 0;

	WINPR_ASSERT(token);
	/* The remapping is only allowed for scancodes, so maximum is 999=999 */
	if (strlen(token) > 10)
		return FALSE;

	if (!freerdp_extract_key_value(token, &key, &value))
	{
		WLog_WARN(TAG, "/kbd:remap invalid entry '%s'", token);
		return FALSE;
	}
	return TRUE;
}

static int parse_host_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!arg->Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, NULL))
		return COMMAND_LINE_ERROR_MEMORY;
	char* p = strchr(arg->Value, '[');

	/* ipv4 */
	if (!p)
	{
		const char scheme[] = "://";
		const char* val = strstr(arg->Value, scheme);
		if (val)
			val += strnlen(scheme, sizeof(scheme));
		else
			val = arg->Value;
		p = strchr(val, ':');

		if (p)
		{
			LONGLONG lval = 0;
			size_t length = 0;

			if (!value_to_int(&p[1], &lval, 1, UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			length = (size_t)(p - arg->Value);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, (UINT16)lval))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_string_len(settings, FreeRDP_ServerHostname, arg->Value,
			                                     length))
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
		size_t length = 0;
		char* p2 = strchr(arg->Value, ']');

		/* not a valid [] ipv6 addr found */
		if (!p2)
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		length = (size_t)(p2 - p);
		if (!freerdp_settings_set_string_len(settings, FreeRDP_ServerHostname, p + 1, length - 1))
			return COMMAND_LINE_ERROR_MEMORY;

		if (*(p2 + 1) == ':')
		{
			LONGLONG val = 0;

			if (!value_to_int(&p2[2], &val, 0, UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, (UINT16)val))
				return COMMAND_LINE_ERROR;
		}

		printf("hostname %s port %" PRIu32 "\n",
		       freerdp_settings_get_string(settings, FreeRDP_ServerHostname),
		       freerdp_settings_get_uint32(settings, FreeRDP_ServerPort));
	}
	return 0;
}

static int parse_redirect_prefer_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char* cur = arg->Value;
	if (!arg->Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_RedirectionPreferType, 0))
		return COMMAND_LINE_ERROR;

	UINT32 value = 0;
	do
	{
		UINT32 mask = 0;
		char* next = strchr(cur, ',');

		if (next)
		{
			*next = '\0';
			next++;
		}

		if (option_equals("fqdn", cur))
			mask = 0x06U;
		else if (option_equals("ip", cur))
			mask = 0x05U;
		else if (option_equals("netbios", cur))
			mask = 0x03U;
		else
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		cur = next;
		mask = (mask & 0x07);
		value |= mask << (count * 3);
		count++;
	} while (cur != NULL);

	if (!freerdp_settings_set_uint32(settings, FreeRDP_RedirectionPreferType, value))
		return COMMAND_LINE_ERROR;

	if (count > 3)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	return 0;
}

static int parse_prevent_session_lock_options(rdpSettings* settings,
                                              const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!freerdp_settings_set_uint32(settings, FreeRDP_FakeMouseMotionInterval, 180))
		return COMMAND_LINE_ERROR_MEMORY;

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		LONGLONG val = 0;

		if (!value_to_int(arg->Value, &val, 1, UINT32_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_FakeMouseMotionInterval, (UINT32)val))
			return COMMAND_LINE_ERROR_MEMORY;
	}

	return 0;
}

static int parse_vmconnect_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE))
		return COMMAND_LINE_ERROR;

	UINT32 port = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);
	if (port == 3389)
		port = 2179;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, port))
		return COMMAND_LINE_ERROR;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, FALSE))
		return COMMAND_LINE_ERROR;

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
			return COMMAND_LINE_ERROR;

		if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
			return COMMAND_LINE_ERROR_MEMORY;
	}
	return 0;
}

static int parse_size_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	int status = 0;
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (!arg->Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	char* p = strchr(arg->Value, 'x');

	if (p)
	{
		unsigned long w = 0;
		unsigned long h = 0;

		if (!parseSizeValue(arg->Value, &w, &h) || (w > UINT16_MAX) || (h > UINT16_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, (UINT32)w))
			return COMMAND_LINE_ERROR;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, (UINT32)h))
			return COMMAND_LINE_ERROR;
	}
	else
	{
		char* str = _strdup(arg->Value);
		if (!str)
			return COMMAND_LINE_ERROR_MEMORY;

		p = strchr(str, '%');

		if (p)
		{
			BOOL partial = FALSE;

			status = COMMAND_LINE_ERROR;
			if (strchr(p, 'w'))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseWidth, TRUE))
					goto fail;
				partial = TRUE;
			}

			if (strchr(p, 'h'))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseHeight, TRUE))
					goto fail;
				partial = TRUE;
			}

			if (!partial)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseWidth, TRUE))
					goto fail;
				if (!freerdp_settings_set_bool(settings, FreeRDP_PercentScreenUseHeight, TRUE))
					goto fail;
			}

			*p = '\0';
			{
				LONGLONG val = 0;

				if (!value_to_int(str, &val, 0, 100))
				{
					status = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					goto fail;
				}

				if (!freerdp_settings_set_uint32(settings, FreeRDP_PercentScreen, (UINT32)val))
					goto fail;
			}

			status = 0;
		}

	fail:
		free(str);
	}

	return status;
}

static int parse_monitors_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		size_t count = 0;
		UINT32* MonitorIds = NULL;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);

		if (!ptr)
			return COMMAND_LINE_ERROR_MEMORY;

		if (count > 16)
			count = 16;

		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, count))
		{
			CommandLineParserFree(ptr);
			return FALSE;
		}

		MonitorIds = freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorIds, 0);
		for (UINT32 i = 0; i < count; i++)
		{
			LONGLONG val = 0;

			if (!value_to_int(ptr[i], &val, 0, UINT16_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			MonitorIds[i] = (UINT32)val;
		}

		CommandLineParserFree(ptr);
	}

	return 0;
}

static int parse_dynamic_resolution_options(rdpSettings* settings,
                                            const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	const BOOL val = arg->Value != 0;

	if (val && freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
	{
		WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, val))
		return COMMAND_LINE_ERROR;
	if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, val))
		return COMMAND_LINE_ERROR;

	return 0;
}

static int parse_smart_sizing_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		WLog_ERR(TAG, "Smart sizing and dynamic resolution are mutually exclusive options");
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_SmartSizing, TRUE))
		return COMMAND_LINE_ERROR;

	if (arg->Value)
	{
		unsigned long w = 0;
		unsigned long h = 0;

		if (!parseSizeValue(arg->Value, &w, &h) || (w > UINT16_MAX) || (h > UINT16_MAX))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		if (!freerdp_settings_set_uint32(settings, FreeRDP_SmartSizingWidth, (UINT32)w))
			return COMMAND_LINE_ERROR;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_SmartSizingHeight, (UINT32)h))
			return COMMAND_LINE_ERROR;
	}
	return 0;
}

static int parse_bpp_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	LONGLONG val = 0;

	if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	switch (val)
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
	return 0;
}

static int parse_kbd_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	int rc = CHANNEL_RC_OK;
	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (!ptr || (count == 0))
		rc = COMMAND_LINE_ERROR;
	else
	{
		for (size_t x = 0; x < count; x++)
		{
			const char* val = ptr[x];

			if (option_starts_with("remap:", val))
			{
				/* Append this new occurrence to the already existing list */
				char* now = _strdup(&val[6]);
				const char* old =
				    freerdp_settings_get_string(settings, FreeRDP_KeyboardRemappingList);

				/* Basic sanity test. Entries must be like <key>=<value>, e.g. 1=2 */
				if (!check_kbd_remap_valid(now))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (old)
				{
					const size_t olen = strlen(old);
					const size_t alen = strlen(now);
					const size_t tlen = olen + alen + 2;
					char* tmp = calloc(tlen, sizeof(char));
					if (!tmp)
						rc = COMMAND_LINE_ERROR_MEMORY;
					else
						(void)_snprintf(tmp, tlen, "%s,%s", old, now);
					free(now);
					now = tmp;
				}

				if (rc == 0)
				{
					if (!freerdp_settings_set_string(settings, FreeRDP_KeyboardRemappingList, now))
						rc = COMMAND_LINE_ERROR;
				}
				free(now);
			}
			else if (option_starts_with("layout:", val))
			{
				rc = parse_kbd_layout(settings, &val[7]);
			}
			else if (option_starts_with("lang:", val))
			{
				LONGLONG ival = 0;
				const BOOL isInt = value_to_int(&val[5], &ival, 1, UINT32_MAX);
				if (!isInt)
					ival = freerdp_get_locale_id_from_string(&val[5]);

				if (ival <= 0)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardCodePage,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("type:", val))
			{
				LONGLONG ival = 0;
				const BOOL isInt = value_to_int(&val[5], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("subtype:", val))
			{
				LONGLONG ival = 0;
				const BOOL isInt = value_to_int(&val[8], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("fn-key:", val))
			{
				LONGLONG ival = 0;
				const BOOL isInt = value_to_int(&val[7], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("unicode", val))
			{
				const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
				if (bval == PARSE_FAIL)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput,
				                                    bval != PARSE_OFF))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (option_starts_with("pipe:", val))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, TRUE))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_string(settings, FreeRDP_KeyboardPipeName, &val[5]))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
			else if (count == 1)
			{
				/* Legacy, allow /kbd:<value> for setting keyboard layout */
				rc = parse_kbd_layout(settings, val);
			}
#endif
			else
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (rc != 0)
				break;
		}
	}
	CommandLineParserFree(ptr);
	return rc;
}

static int parse_proxy_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	/* initial value */
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP))
		return COMMAND_LINE_ERROR_MEMORY;

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		const char* cur = arg->Value;

		if (!cur)
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		/* value is [scheme://][user:password@]hostname:port */
		if (!proxy_parse_uri(settings, cur))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	else
	{
		WLog_ERR(TAG, "Option http-proxy needs argument.");
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	return 0;
}

static int parse_dump_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	BOOL failed = FALSE;
	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (!ptr)
		failed = TRUE;
	else
	{
		BOOL modernsyntax = FALSE;
		BOOL oldsyntax = FALSE;
		for (size_t x = 0; (x < count) && !failed; x++)
		{
			const char* carg = ptr[x];
			if (option_starts_with("file:", carg))
			{
				const char* val = &carg[5];
				if (oldsyntax)
					failed = TRUE;
				else if (!freerdp_settings_set_string(settings, FreeRDP_TransportDumpFile, val))
					failed = TRUE;
				modernsyntax = TRUE;
			}
			else if (option_equals("replay", carg))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDump, FALSE))
					failed = TRUE;
				else if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay, TRUE))
					failed = TRUE;
			}
			else if (option_equals("record", carg))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDump, TRUE))
					failed = TRUE;
				else if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay, FALSE))
					failed = TRUE;
			}
			else if (option_equals("nodelay", carg))
			{
				if (oldsyntax)
					failed = TRUE;
				else if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplayNodelay,
				                                    TRUE))
					failed = TRUE;
				modernsyntax = TRUE;
			}
			else
			{
				/* compat:
				 * support syntax record,<filename> and replay,<filename>
				 */
				if (modernsyntax)
					failed = TRUE;
				else if (!freerdp_settings_set_string(settings, FreeRDP_TransportDumpFile, carg))
					failed = TRUE;
				oldsyntax = TRUE;
			}
		}

		if (oldsyntax && (count != 2))
			failed = TRUE;
	}
	CommandLineParserFree(ptr);
	if (failed)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	return 0;
}

static int parse_clipboard_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (arg->Value == BoolValueTrue || arg->Value == BoolValueFalse)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard,
		                               (arg->Value == BoolValueTrue)))
			return COMMAND_LINE_ERROR;
	}
	else
	{
		int rc = 0;
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		for (size_t x = 0; (x < count) && (rc == 0); x++)
		{
			const char* usesel = "use-selection:";

			const char* cur = ptr[x];
			if (option_starts_with(usesel, cur))
			{
				const char* val = &cur[strlen(usesel)];
				if (!freerdp_settings_set_string(settings, FreeRDP_ClipboardUseSelection, val))
					rc = COMMAND_LINE_ERROR_MEMORY;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, TRUE))
					return COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("direction-to", cur))
			{
				const UINT32 mask =
				    freerdp_settings_get_uint32(settings, FreeRDP_ClipboardFeatureMask) &
				    ~(CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_REMOTE_TO_LOCAL);
				const PARSE_CLIP_DIR_RESULT bval = parse_clip_direciton_to_option(cur);
				UINT32 bflags = 0;
				switch (bval)
				{
					case CLIP_DIR_PARSE_ALL:
						bflags |= CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_REMOTE_TO_LOCAL;
						break;
					case CLIP_DIR_PARSE_LOCAL:
						bflags |= CLIPRDR_FLAG_REMOTE_TO_LOCAL;
						break;
					case CLIP_DIR_PARSE_REMOTE:
						bflags |= CLIPRDR_FLAG_LOCAL_TO_REMOTE;
						break;
					case CLIP_DIR_PARSE_OFF:
						break;
					case CLIP_DIR_PARSE_FAIL:
					default:
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						break;
				}

				if (!freerdp_settings_set_uint32(settings, FreeRDP_ClipboardFeatureMask,
				                                 mask | bflags))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else if (option_starts_with("files-to", cur))
			{
				const UINT32 mask =
				    freerdp_settings_get_uint32(settings, FreeRDP_ClipboardFeatureMask) &
				    ~(CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES);
				const PARSE_CLIP_DIR_RESULT bval = parse_clip_direciton_to_option(cur);
				UINT32 bflags = 0;
				switch (bval)
				{
					case CLIP_DIR_PARSE_ALL:
						bflags |=
						    CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES;
						break;
					case CLIP_DIR_PARSE_LOCAL:
						bflags |= CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES;
						break;
					case CLIP_DIR_PARSE_REMOTE:
						bflags |= CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES;
						break;
					case CLIP_DIR_PARSE_OFF:
						break;
					case CLIP_DIR_PARSE_FAIL:
					default:
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
						break;
				}

				if (!freerdp_settings_set_uint32(settings, FreeRDP_ClipboardFeatureMask,
				                                 mask | bflags))
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineParserFree(ptr);

		if (rc)
			return rc;
	}
	return 0;
}

static int parse_audio_mode_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	LONGLONG val = 0;

	if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	switch (val)
	{
		case AUDIO_MODE_REDIRECT:
			if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE))
				return COMMAND_LINE_ERROR;
			break;

		case AUDIO_MODE_PLAY_ON_SERVER:
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, TRUE))
				return COMMAND_LINE_ERROR;
			break;

		case AUDIO_MODE_NONE:
			if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, FALSE))
				return COMMAND_LINE_ERROR;
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, FALSE))
				return COMMAND_LINE_ERROR;
			break;

		default:
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	return 0;
}

static int parse_network_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	UINT32 type = CONNECTION_TYPE_INVALID;

	if (option_equals(arg->Value, "invalid"))
		type = CONNECTION_TYPE_INVALID;
	else if (option_equals(arg->Value, "modem"))
		type = CONNECTION_TYPE_MODEM;
	else if (option_equals(arg->Value, "broadband"))
		type = CONNECTION_TYPE_BROADBAND_HIGH;
	else if (option_equals(arg->Value, "broadband-low"))
		type = CONNECTION_TYPE_BROADBAND_LOW;
	else if (option_equals(arg->Value, "broadband-high"))
		type = CONNECTION_TYPE_BROADBAND_HIGH;
	else if (option_equals(arg->Value, "wan"))
		type = CONNECTION_TYPE_WAN;
	else if (option_equals(arg->Value, "lan"))
		type = CONNECTION_TYPE_LAN;
	else if ((option_equals(arg->Value, "autodetect")) || (option_equals(arg->Value, "auto")) ||
	         (option_equals(arg->Value, "detect")))
	{
		type = CONNECTION_TYPE_AUTODETECT;
	}
	else
	{
		LONGLONG val = 0;

		if (!value_to_int(arg->Value, &val, 0, 7))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

		type = (UINT32)val;
	}

	if (!freerdp_set_connection_type(settings, type))
		return COMMAND_LINE_ERROR;
	return 0;
}

static int parse_sec_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (count == 0)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	FreeRDP_Settings_Keys_Bool singleOptionWithoutOnOff = FreeRDP_BOOL_UNUSED;
	for (size_t x = 0; x < count; x++)
	{
		const char* cur = ptr[x];
		const PARSE_ON_OFF_RESULT bval = parse_on_off_option(cur);
		if (bval == PARSE_FAIL)
		{
			CommandLineParserFree(ptr);
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		const BOOL val = bval != PARSE_OFF;
		FreeRDP_Settings_Keys_Bool id = FreeRDP_BOOL_UNUSED;
		if (option_starts_with("rdp", cur)) /* Standard RDP */
		{
			id = FreeRDP_RdpSecurity;
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		else if (option_starts_with("tls", cur)) /* TLS */
			id = FreeRDP_TlsSecurity;
		else if (option_starts_with("nla", cur)) /* NLA */
			id = FreeRDP_NlaSecurity;
		else if (option_starts_with("ext", cur)) /* NLA Extended */
			id = FreeRDP_ExtSecurity;
		else if (option_equals("aad", cur)) /* RDSAAD */
			id = FreeRDP_AadSecurity;
		else
		{
			WLog_ERR(TAG, "unknown protocol security: %s", arg->Value);
			CommandLineParserFree(ptr);
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		if ((bval == PARSE_NONE) && (count == 1))
			singleOptionWithoutOnOff = id;
		if (!freerdp_settings_set_bool(settings, id, val))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}

	if (singleOptionWithoutOnOff != FreeRDP_BOOL_UNUSED)
	{
		const FreeRDP_Settings_Keys_Bool options[] = { FreeRDP_AadSecurity,
			                                           FreeRDP_UseRdpSecurityLayer,
			                                           FreeRDP_RdpSecurity, FreeRDP_NlaSecurity,
			                                           FreeRDP_TlsSecurity };

		for (size_t i = 0; i < ARRAYSIZE(options); i++)
		{
			if (!freerdp_settings_set_bool(settings, options[i], FALSE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}

		if (!freerdp_settings_set_bool(settings, singleOptionWithoutOnOff, TRUE))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		if (singleOptionWithoutOnOff == FreeRDP_RdpSecurity)
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, TRUE))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
	}
	CommandLineParserFree(ptr);
	return 0;
}

static int parse_encryption_methods_options(rdpSettings* settings,
                                            const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		size_t count = 0;
		char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);

		UINT32 EncryptionMethods = 0;
		for (UINT32 i = 0; i < count; i++)
		{
			if (option_equals(ptr[i], "40"))
				EncryptionMethods |= ENCRYPTION_METHOD_40BIT;
			else if (option_equals(ptr[i], "56"))
				EncryptionMethods |= ENCRYPTION_METHOD_56BIT;
			else if (option_equals(ptr[i], "128"))
				EncryptionMethods |= ENCRYPTION_METHOD_128BIT;
			else if (option_equals(ptr[i], "FIPS"))
				EncryptionMethods |= ENCRYPTION_METHOD_FIPS;
			else
				WLog_ERR(TAG, "unknown encryption method '%s'", ptr[i]);
		}

		if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods, EncryptionMethods))
			return COMMAND_LINE_ERROR;
		CommandLineParserFree(ptr);
	}
	return 0;
}

static int parse_cert_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	int rc = 0;
	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	for (size_t x = 0; (x < count) && (rc == 0); x++)
	{
		const char deny[] = "deny";
		const char ignore[] = "ignore";
		const char tofu[] = "tofu";
		const char name[] = "name:";
		const char fingerprints[] = "fingerprint:";

		const char* cur = ptr[x];
		if (option_equals(deny, cur))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoDenyCertificate, TRUE))
				return COMMAND_LINE_ERROR;
		}
		else if (option_equals(ignore, cur))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE))
				return COMMAND_LINE_ERROR;
		}
		else if (option_equals(tofu, cur))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoAcceptCertificate, TRUE))
				return COMMAND_LINE_ERROR;
		}
		else if (option_starts_with(name, cur))
		{
			const char* val = &cur[strnlen(name, sizeof(name))];
			if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, val))
				rc = COMMAND_LINE_ERROR_MEMORY;
		}
		else if (option_starts_with(fingerprints, cur))
		{
			const char* val = &cur[strnlen(fingerprints, sizeof(fingerprints))];
			if (!freerdp_settings_append_string(settings, FreeRDP_CertificateAcceptedFingerprints,
			                                    ",", val))
				rc = COMMAND_LINE_ERROR_MEMORY;
		}
		else
			rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	CommandLineParserFree(ptr);

	return rc;
}

static int parse_mouse_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValuesEx("mouse", arg->Value, &count);
	int rc = 0;
	if (ptr)
	{
		for (size_t x = 1; x < count; x++)
		{
			const char* cur = ptr[x];

			const PARSE_ON_OFF_RESULT bval = parse_on_off_option(cur);
			if (bval == PARSE_FAIL)
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			else
			{
				const BOOL val = bval != PARSE_OFF;

				if (option_starts_with("relative", cur))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_MouseUseRelativeMove, val))
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				else if (option_starts_with("grab", cur))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_GrabMouse, val))
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}

			if (rc != 0)
				break;
		}
	}
	CommandLineParserFree(ptr);

	return rc;
}

static int parse_floatbar_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	/* Defaults are enabled, visible, sticky, fullscreen */
	UINT32 Floatbar = 0x0017;

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
			if (option_starts_with("sticky:", cur))
			{
				Floatbar &= ~0x02u;

				const PARSE_ON_OFF_RESULT bval = parse_on_off_option(cur);
				switch (bval)
				{
					case PARSE_ON:
					case PARSE_NONE:
						Floatbar |= 0x02u;
						break;
					case PARSE_OFF:
						Floatbar &= ~0x02u;
						break;
					case PARSE_FAIL:
					default:
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}
			/* default:[visible|hidden] */
			else if (option_starts_with("default:", cur))
			{
				const char* val = cur + 8;
				Floatbar &= ~0x04u;

				if (option_equals("visible", val))
					Floatbar |= 0x04u;
				else if (option_equals("hidden", val))
					Floatbar &= ~0x04u;
				else
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			/* show:[always|fullscreen|window] */
			else if (option_starts_with("show:", cur))
			{
				const char* val = cur + 5;
				Floatbar &= ~0x30u;

				if (option_equals("always", val))
					Floatbar |= 0x30u;
				else if (option_equals("fullscreen", val))
					Floatbar |= 0x10u;
				else if (option_equals("window", val))
					Floatbar |= 0x20u;
				else
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		} while (start);
	}
	if (!freerdp_settings_set_uint32(settings, FreeRDP_Floatbar, Floatbar))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	return 0;
}

static int parse_reconnect_cookie_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	BYTE* base64 = NULL;
	size_t length = 0;
	if (!arg->Value)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	crypto_base64_decode((const char*)(arg->Value), strlen(arg->Value), &base64, &length);

	if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
	{
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie, base64,
		                                      1))
			return COMMAND_LINE_ERROR;
	}
	else
	{
		WLog_ERR(TAG, "reconnect-cookie:  invalid base64 '%s'", arg->Value);
	}

	free(base64);
	return 0;
}

static int parse_scale_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	LONGLONG val = 0;

	if (!value_to_int(arg->Value, &val, 100, 180))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	switch (val)
	{
		case 100:
		case 140:
		case 180:
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor, (UINT32)val))
				return COMMAND_LINE_ERROR;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceScaleFactor, (UINT32)val))
				return COMMAND_LINE_ERROR;
			break;

		default:
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	return 0;
}

static int parse_scale_device_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	LONGLONG val = 0;

	if (!value_to_int(arg->Value, &val, 100, 180))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	switch (val)
	{
		case 100:
		case 140:
		case 180:
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceScaleFactor, (UINT32)val))
				return COMMAND_LINE_ERROR;
			break;

		default:
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}
	return 0;
}

static int parse_smartcard_logon_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;

	if (!freerdp_settings_set_bool(settings, FreeRDP_SmartcardLogon, TRUE))
		return COMMAND_LINE_ERROR;

	char** ptr = CommandLineParseCommaSeparatedValuesEx("smartcard-logon", arg->Value, &count);
	if (ptr)
	{
		const CmdLineSubOptions opts[] = {
			{ "cert:", FreeRDP_SmartcardCertificate, CMDLINE_SUBOPTION_FILE,
			  setSmartcardEmulation },
			{ "key:", FreeRDP_SmartcardPrivateKey, CMDLINE_SUBOPTION_FILE, setSmartcardEmulation },
			{ "pin:", FreeRDP_Password, CMDLINE_SUBOPTION_STRING, NULL },
			{ "csp:", FreeRDP_CspName, CMDLINE_SUBOPTION_STRING, NULL },
			{ "reader:", FreeRDP_ReaderName, CMDLINE_SUBOPTION_STRING, NULL },
			{ "card:", FreeRDP_CardName, CMDLINE_SUBOPTION_STRING, NULL },
			{ "container:", FreeRDP_ContainerName, CMDLINE_SUBOPTION_STRING, NULL }
		};

		for (size_t x = 1; x < count; x++)
		{
			const char* cur = ptr[x];
			if (!parseSubOptions(settings, opts, ARRAYSIZE(opts), cur))
			{
				CommandLineParserFree(ptr);
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
		}
	}
	CommandLineParserFree(ptr);
	return 0;
}

static int parse_tune_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValuesEx("tune", arg->Value, &count);
	if (!ptr)
		return COMMAND_LINE_ERROR;
	for (size_t x = 1; x < count; x++)
	{
		const char* cur = ptr[x];
		char* sep = strchr(cur, ':');
		if (!sep)
		{
			CommandLineParserFree(ptr);
			return COMMAND_LINE_ERROR;
		}
		*sep++ = '\0';
		if (!freerdp_settings_set_value_for_name(settings, cur, sep))
		{
			CommandLineParserFree(ptr);
			return COMMAND_LINE_ERROR;
		}
	}

	CommandLineParserFree(ptr);
	return 0;
}

static int parse_app_option_program(rdpSettings* settings, const char* cmd)
{
	const FreeRDP_Settings_Keys_Bool ids[] = { FreeRDP_RemoteApplicationMode,
		                                       FreeRDP_RemoteAppLanguageBarSupported,
		                                       FreeRDP_Workarea, FreeRDP_DisableWallpaper,
		                                       FreeRDP_DisableFullWindowDrag };

	if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationProgram, cmd))
		return COMMAND_LINE_ERROR_MEMORY;

	for (size_t y = 0; y < ARRAYSIZE(ids); y++)
	{
		if (!freerdp_settings_set_bool(settings, ids[y], TRUE))
			return COMMAND_LINE_ERROR;
	}
	return CHANNEL_RC_OK;
}

static int parse_app_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	int rc = CHANNEL_RC_OK;
	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (!ptr || (count == 0))
		rc = COMMAND_LINE_ERROR;
	else
	{
		struct app_map
		{
			const char* name;
			SSIZE_T id;
			int (*fkt)(rdpSettings* settings, const char* value);
		};
		const struct app_map amap[] = { { "program:", FreeRDP_RemoteApplicationProgram,
			                              parse_app_option_program },
			                            { "workdir:", FreeRDP_RemoteApplicationWorkingDir, NULL },
			                            { "name:", FreeRDP_RemoteApplicationName, NULL },
			                            { "icon:", FreeRDP_RemoteApplicationIcon, NULL },
			                            { "cmd:", FreeRDP_RemoteApplicationCmdLine, NULL },
			                            { "file:", FreeRDP_RemoteApplicationFile, NULL },
			                            { "guid:", FreeRDP_RemoteApplicationGuid, NULL },
			                            { "hidef:", FreeRDP_HiDefRemoteApp, NULL } };
		for (size_t x = 0; x < count; x++)
		{
			BOOL handled = FALSE;
			const char* val = ptr[x];

			for (size_t y = 0; y < ARRAYSIZE(amap); y++)
			{
				const struct app_map* cur = &amap[y];
				if (option_starts_with(cur->name, val))
				{
					const char* xval = &val[strlen(cur->name)];
					if (cur->fkt)
						rc = cur->fkt(settings, xval);
					else
					{
						const char* name = freerdp_settings_get_name_for_key(cur->id);
						if (!freerdp_settings_set_value_for_name(settings, name, xval))
							rc = COMMAND_LINE_ERROR_MEMORY;
					}

					handled = TRUE;
					break;
				}
			}

#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
			if (!handled && (count == 1))
			{
				/* Legacy path, allow /app:command and /app:||command syntax */
				rc = parse_app_option_program(settings, val);
			}
			else
#endif
			    if (!handled)
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (rc != 0)
				break;
		}
	}

	CommandLineParserFree(ptr);
	return rc;
}

static int parse_cache_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	int rc = CHANNEL_RC_OK;
	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (!ptr || (count == 0))
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	for (size_t x = 0; x < count; x++)
	{
		const char* val = ptr[x];

		if (option_starts_with("codec:", val))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheV3Enabled, TRUE))
				rc = COMMAND_LINE_ERROR;
			else if (option_equals(arg->Value, "rfx"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_equals(arg->Value, "nsc"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE))
					rc = COMMAND_LINE_ERROR;
			}

#if defined(WITH_JPEG)
			else if (option_equals(arg->Value, "jpeg"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, TRUE))
					rc = COMMAND_LINE_ERROR;

				if (freerdp_settings_get_uint32(settings, FreeRDP_JpegQuality) == 0)
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, 75))
						return COMMAND_LINE_ERROR;
				}
			}

#endif
		}
		else if (option_starts_with("persist-file:", val))
		{

			if (!freerdp_settings_set_string(settings, FreeRDP_BitmapCachePersistFile, &val[13]))
				rc = COMMAND_LINE_ERROR_MEMORY;
			else if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled, TRUE))
				rc = COMMAND_LINE_ERROR;
		}
		else
		{
			const PARSE_ON_OFF_RESULT bval = parse_on_off_option(val);
			if (bval == PARSE_FAIL)
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			else
			{
				if (option_starts_with("bitmap", val))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled,
					                               bval != PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("glyph", val))
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel,
					                                 bval != PARSE_OFF ? GLYPH_SUPPORT_FULL
					                                                   : GLYPH_SUPPORT_NONE))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("persist", val))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
					                               bval != PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("offscreen", val))
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel,
					                                 bval != PARSE_OFF))
						rc = COMMAND_LINE_ERROR;
				}
			}
		}
	}

	CommandLineParserFree(ptr);
	return rc;
}

static BOOL parse_gateway_host_option(rdpSettings* settings, const char* host)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(host);

	char* name = NULL;
	int port = -1;
	if (!freerdp_parse_hostname(host, &name, &port))
		return FALSE;
	const BOOL rc = freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, name);
	free(name);
	if (!rc)
		return FALSE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, TRUE))
		return FALSE;
	if (!freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT))
		return FALSE;

	return TRUE;
}

static BOOL parse_gateway_cred_option(rdpSettings* settings, const char* value,
                                      FreeRDP_Settings_Keys_String what)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(value);

	switch (what)
	{
		case FreeRDP_GatewayUsername:
			if (!freerdp_parse_username_settings(value, settings, FreeRDP_GatewayUsername,
			                                     FreeRDP_GatewayDomain))
				return FALSE;
			break;
		default:
			if (!freerdp_settings_set_string(settings, what, value))
				return FALSE;
			break;
	}

	return freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE);
}

static BOOL parse_gateway_type_option(rdpSettings* settings, const char* value)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(value);

	if (option_equals(value, "rpc"))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, FALSE))
			return FALSE;
	}
	else
	{
		if (option_equals(value, "http"))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, FALSE))
				return FALSE;
		}
		else if (option_equals(value, "auto"))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, FALSE))
				return FALSE;
		}
		else if (option_equals(value, "arm"))
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, TRUE))
				return FALSE;
		}
	}
	return TRUE;
}

static BOOL parse_gateway_usage_option(rdpSettings* settings, const char* value)
{
	UINT32 type = 0;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(value);

	if (option_equals(value, "none"))
		type = TSC_PROXY_MODE_NONE_DIRECT;
	else if (option_equals(value, "direct"))
		type = TSC_PROXY_MODE_DIRECT;
	else if (option_equals(value, "detect"))
		type = TSC_PROXY_MODE_DETECT;
	else if (option_equals(value, "default"))
		type = TSC_PROXY_MODE_DEFAULT;
	else
	{
		LONGLONG val = 0;

		if (!value_to_int(value, &val, TSC_PROXY_MODE_NONE_DIRECT, TSC_PROXY_MODE_NONE_DETECT))
			return FALSE;
	}

	return freerdp_set_gateway_usage_method(settings, type);
}

static BOOL parse_gateway_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(arg);

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
	if (count == 0)
		return TRUE;
	WINPR_ASSERT(ptr);

	if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE))
		goto fail;

	BOOL allowHttpOpts = FALSE;
	for (size_t x = 0; x < count; x++)
	{
		BOOL validOption = FALSE;
		const char* argval = ptr[x];

		WINPR_ASSERT(argval);

		const char* gw = option_starts_with("g:", argval);
		if (gw)
		{
			if (!parse_gateway_host_option(settings, gw))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* gu = option_starts_with("u:", argval);
		if (gu)
		{
			if (!parse_gateway_cred_option(settings, gu, FreeRDP_GatewayUsername))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* gd = option_starts_with("d:", argval);
		if (gd)
		{
			if (!parse_gateway_cred_option(settings, gd, FreeRDP_GatewayDomain))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* gp = option_starts_with("p:", argval);
		if (gp)
		{
			if (!parse_gateway_cred_option(settings, gp, FreeRDP_GatewayPassword))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* gt = option_starts_with("type:", argval);
		if (gt)
		{
			if (!parse_gateway_type_option(settings, gt))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = freerdp_settings_get_bool(settings, FreeRDP_GatewayHttpTransport);
		}

		const char* gat = option_starts_with("access-token:", argval);
		if (gat)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, gat))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* bearer = option_starts_with("bearer:", argval);
		if (bearer)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHttpExtAuthBearer, bearer))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* gwurl = option_starts_with("url:", argval);
		if (gwurl)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUrl, gwurl))
				goto fail;
			if (!freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		const char* um = option_starts_with("usage-method:", argval);
		if (um)
		{
			if (!parse_gateway_usage_option(settings, um))
				goto fail;
			validOption = TRUE;
			allowHttpOpts = FALSE;
		}

		if (allowHttpOpts)
		{
			if (option_equals(argval, "no-websockets"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, FALSE))
					goto fail;
				validOption = TRUE;
			}
			else if (option_equals(argval, "extauth-sspi-ntlm"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpExtAuthSspiNtlm, TRUE))
					goto fail;
				validOption = TRUE;
			}
		}

		if (!validOption)
			goto fail;
	}

	rc = TRUE;
fail:
	CommandLineParserFree(ptr);
	return rc;
}

static void fill_credential_string(COMMAND_LINE_ARGUMENT_A* args, const char* value)
{
	WINPR_ASSERT(args);
	WINPR_ASSERT(value);

	const COMMAND_LINE_ARGUMENT_A* arg = CommandLineFindArgumentA(args, value);
	if (!arg)
		return;

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		FillMemory(arg->Value, strlen(arg->Value), '*');
}

static void fill_credential_strings(COMMAND_LINE_ARGUMENT_A* args)
{
	const char* credentials[] = {
		"p",
		"smartcard-logon",
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		"gp",
		"gat",
#endif
		"pth",
		"reconnect-cookie",
		"assistance"
	};

	for (size_t x = 0; x < ARRAYSIZE(credentials); x++)
	{
		const char* cred = credentials[x];
		fill_credential_string(args, cred);
	}

	const COMMAND_LINE_ARGUMENT_A* arg = CommandLineFindArgumentA(args, "gateway");
	if (arg && ((arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT) != 0))
	{
		const char* gwcreds[] = { "p:", "access-token:" };
		char* saveptr = NULL;
		char* tok = strtok_s(arg->Value, ",", &saveptr);
		while (tok)
		{
			for (size_t x = 0; x < ARRAYSIZE(gwcreds); x++)
			{
				const char* opt = gwcreds[x];
				if (option_starts_with(opt, tok))
				{
					char* val = &tok[strlen(opt)];
					FillMemory(val, strlen(val), '*');
				}
			}
			tok = strtok_s(NULL, ",", &saveptr);
		}
	}
}

static int freerdp_client_settings_parse_command_line_arguments_int(
    rdpSettings* settings, int argc, char* argv[], BOOL allowUnknown,
    COMMAND_LINE_ARGUMENT_A* largs, size_t count,
    int (*handle_option)(const COMMAND_LINE_ARGUMENT_A* arg, void* custom), void* handle_userdata)
{
	char* user = NULL;
	int status = 0;
	BOOL ext = FALSE;
	BOOL assist = FALSE;
	DWORD flags = 0;
	BOOL promptForPassword = FALSE;
	BOOL compatibility = FALSE;
	const COMMAND_LINE_ARGUMENT_A* arg = NULL;

	/* Command line detection fails if only a .rdp or .msrcIncident file
	 * is supplied. Check this case first, only then try to detect
	 * legacy command line syntax. */
	if (argc > 1)
	{
		ext = option_is_rdp_file(argv[1]);
		assist = option_is_incident_file(argv[1]);
	}

	if (!ext && !assist)
		compatibility = freerdp_client_detect_command_line(argc, argv, &flags);
	else
		compatibility = freerdp_client_detect_command_line(argc - 1, &argv[1], &flags);

	if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, NULL))
		return -1;
	if (!freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, NULL))
		return -1;
	if (!freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, NULL))
		return -1;

	if (compatibility)
	{
		WLog_WARN(TAG, "Unsupported command line syntax!");
		WLog_WARN(TAG, "FreeRDP 1.0 style syntax was dropped with version 3!");
		return -1;
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

		prepare_default_settings(settings, largs, ext);
	}

	CommandLineFindArgumentA(largs, "v");
	arg = largs;
	errno = 0;

	/* Disable unicode input unless requested. */
	if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, FALSE))
		return COMMAND_LINE_ERROR_MEMORY;

	do
	{
		BOOL enable = arg->Value ? TRUE : FALSE;

		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		    CommandLineSwitchCase(arg, "v")
		{
			const int rc = parse_host_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationServiceClass,
			                                 arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "sspi-module")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_SspiModule, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "winscard-module")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WinSCardModule, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "redirect-prefer")
		{
			const int rc = parse_redirect_prefer_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "credentials-delegation")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableCredentialsDelegation, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "prevent-session-lock")
		{
			const int rc = parse_prevent_session_lock_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "vmconnect")
		{
			const int rc = parse_vmconnect_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "w")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "h")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, -1, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "size")
		{
			const int rc = parse_size_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "f")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "suppress-output")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SuppressOutput, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "multimon")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (option_equals(arg->Value, str_force))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_ForceMultimon, TRUE))
						return fail_at(arg, COMMAND_LINE_ERROR);
				}
			}
		}
		CommandLineSwitchCase(arg, "span")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SpanMonitors, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "workarea")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Workarea, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "monitors")
		{
			const int rc = parse_monitors_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "t")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WindowTitle, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "decorations")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Decorations, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "dynamic-resolution")
		{
			const int rc = parse_dynamic_resolution_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "smart-sizing")
		{
			const int rc = parse_smart_sizing_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "bpp")
		{
			const int rc = parse_bpp_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "admin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "relax-order-checks")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowUnanouncedOrdersFromServer,
			                               enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "restricted-admin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "remoteGuard")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteCredentialGuard, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "pth")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ConsoleSession, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (!freerdp_settings_set_string(settings, FreeRDP_PasswordHash, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "client-hostname")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ClientHostname, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "kbd")
		{
			int rc = parse_kbd_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "kbd-remap")
		{
			WLog_WARN(TAG, "/kbd-remap:<key>=<value>,<key2>=<value2> is deprecated, use "
			               "/kbd:remap:<key>=<value>,remap:<key2>=<value2>,... instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_KeyboardRemappingList, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "kbd-lang")
		{
			LONGLONG val = 0;

			WLog_WARN(TAG, "/kbd-lang:<value> is deprecated, use /kbd:lang:<value> instead");
			if (!value_to_int(arg->Value, &val, 1, UINT32_MAX))
			{
				WLog_ERR(TAG, "Could not identify keyboard active language %s", arg->Value);
				WLog_ERR(TAG, "Use /list:kbd-lang to list available layouts");
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
			}

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardCodePage, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			LONGLONG val = 0;

			WLog_WARN(TAG, "/kbd-type:<value> is deprecated, use /kbd:type:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "kbd-unicode")
		{
			WLog_WARN(TAG, "/kbd-unicode is deprecated, use /kbd:unicode[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, enable))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			LONGLONG val = 0;

			WLog_WARN(TAG, "/kbd-subtype:<value> is deprecated, use /kbd:subtype:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			LONGLONG val = 0;

			WLog_WARN(TAG, "/kbd-fn-key:<value> is deprecated, use /kbd:fn-key:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#endif
		CommandLineSwitchCase(arg, "u")
		{
			WINPR_ASSERT(arg->Value);
			user = arg->Value;
		}
		CommandLineSwitchCase(arg, "d")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Domain, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "p")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Password, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "gateway")
		{
			if (!parse_gateway_options(settings, arg))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "proxy")
		{
			const int rc = parse_proxy_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "g")
		{
			if (!parse_gateway_host_option(settings, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "gu")
		{
			if (!parse_gateway_cred_option(settings, arg->Value, FreeRDP_GatewayUsername))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "gd")
		{
			if (!parse_gateway_cred_option(settings, arg->Value, FreeRDP_GatewayDomain))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "gp")
		{
			if (!parse_gateway_cred_option(settings, arg->Value, FreeRDP_GatewayPassword))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "gt")
		{
			if (!parse_gateway_type_option(settings, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "gat")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAccessToken, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "gateway-usage-method")
		{
			if (!parse_gateway_usage_option(settings, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
#endif
		CommandLineSwitchCase(arg, "app")
		{
			int rc = parse_app_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			WINPR_ASSERT(arg->Value);
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, arg->Value,
			                                      strlen(arg->Value)))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "app-workdir")
		{
			WLog_WARN(
			    TAG,
			    "/app-workdir:<directory> is deprecated, use /app:workdir:<directory> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
			                                 arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			WLog_WARN(TAG, "/app-name:<directory> is deprecated, use /app:name:<name> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationName, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			WLog_WARN(TAG, "/app-icon:<filename> is deprecated, use /app:icon:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationIcon, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			WLog_WARN(TAG, "/app-cmd:<command> is deprecated, use /app:cmd:<command> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
			                                 arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			WLog_WARN(TAG, "/app-file:<filename> is deprecated, use /app:file:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationFile, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			WLog_WARN(TAG, "/app-guid:<guid> is deprecated, use /app:guid:<guid> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationGuid, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
#endif
		CommandLineSwitchCase(arg, "compression")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "compression-level")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "drives")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectDrives, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "dump")
		{
			const int rc = parse_dump_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "disable-output")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "home-drive")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectHomeDrive, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "ipv4")
		{
			if (arg->Value != NULL && strncmp(arg->Value, str_force, ARRAYSIZE(str_force)) == 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ForceIPvX, 4))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
			else
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_PreferIPv6OverIPv4, FALSE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
		}
		CommandLineSwitchCase(arg, "ipv6")
		{
			if (arg->Value != NULL && strncmp(arg->Value, str_force, ARRAYSIZE(str_force)) == 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ForceIPvX, 6))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
			else
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_PreferIPv6OverIPv4, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
		}
		CommandLineSwitchCase(arg, "clipboard")
		{
			const int rc = parse_clipboard_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "server-name")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_UserSpecifiedServerName, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "shell")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AlternateShell, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "shell-dir")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ShellWorkingDirectory, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "audio-mode")
		{
			const int rc = parse_audio_mode_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "network")
		{
			const int rc = parse_network_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "fonts")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "wallpaper")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "window-drag")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "window-position")
		{
			unsigned long x = 0;
			unsigned long y = 0;

			if (!arg->Value)
				return fail_at(arg, COMMAND_LINE_ERROR_MISSING_ARGUMENT);

			if (!parseSizeValue(arg->Value, &x, &y) || x > UINT16_MAX || y > UINT16_MAX)
			{
				WLog_ERR(TAG, "invalid window-position argument");
				return fail_at(arg, COMMAND_LINE_ERROR_MISSING_ARGUMENT);
			}

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, (UINT32)x))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, (UINT32)y))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "menu-anims")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "themes")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "timeout")
		{
			ULONGLONG val = 0;
			if (!value_to_uint(arg->Value, &val, 1, 600000))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "timezone")
		{
			BOOL found = FALSE;
			DWORD index = 0;
			DYNAMIC_TIME_ZONE_INFORMATION info = { 0 };
			char TimeZoneKeyName[ARRAYSIZE(info.TimeZoneKeyName) + 1] = { 0 };
			while (EnumDynamicTimeZoneInformation(index++, &info) != ERROR_NO_MORE_ITEMS)
			{
				(void)ConvertWCharNToUtf8(info.TimeZoneKeyName, ARRAYSIZE(info.TimeZoneKeyName),
				                          TimeZoneKeyName, ARRAYSIZE(TimeZoneKeyName));

				WINPR_ASSERT(arg->Value);
				if (strncmp(TimeZoneKeyName, arg->Value, ARRAYSIZE(TimeZoneKeyName)) == 0)
				{
					found = TRUE;
					break;
				}
			}
			if (!found)
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_string(settings, FreeRDP_DynamicDSTTimeZoneKeyName,
			                                 TimeZoneKeyName))
				return fail_at(arg, COMMAND_LINE_ERROR);

			TIME_ZONE_INFORMATION* tz =
			    freerdp_settings_get_pointer_writable(settings, FreeRDP_ClientTimeZone);
			if (!tz)
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);

			tz->Bias = info.Bias;
			tz->DaylightBias = info.DaylightBias;
			tz->DaylightDate = info.DaylightDate;
			memcpy(tz->DaylightName, info.DaylightName, sizeof(tz->DaylightName));
			tz->StandardBias = info.StandardBias;
			tz->StandardDate = info.StandardDate;
			memcpy(tz->StandardName, info.StandardName, sizeof(tz->StandardName));
		}
		CommandLineSwitchCase(arg, "aero")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (option_equals(arg->Value, "sw"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
			else if (option_equals(arg->Value, "hw"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, FALSE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
			else
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "gfx")
		{
			int rc = parse_gfx_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "gfx-thin-client")
		{
			WLog_WARN(TAG, "/gfx-thin-client is deprecated, use /gfx:thin-client[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (freerdp_settings_get_bool(settings, FreeRDP_GfxThinClient))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}

			if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "gfx-small-cache")
		{
			WLog_WARN(TAG, "/gfx-small-cache is deprecated, use /gfx:small-cache[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (enable)
				if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			WLog_WARN(TAG, "/gfx-progressive is deprecated, use /gfx:progressive[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (enable)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
		}
#ifdef WITH_GFX_H264
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			WLog_WARN(TAG, "/gfx-h264 is deprecated, use /gfx:avc420 instead");
			int rc = parse_gfx_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#endif
#endif
		CommandLineSwitchCase(arg, "rfx")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (!arg->Value)
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (option_equals(arg->Value, "video"))
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxCodecMode, 0x00))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
			else if (option_equals(arg->Value, "image"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxImageCodec, TRUE))
					return fail_at(arg, COMMAND_LINE_ERROR);
				if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxCodecMode, 0x02))
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
		}
		CommandLineSwitchCase(arg, "frame-ack")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_FrameAcknowledge, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "nsc")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#if defined(WITH_JPEG)
		CommandLineSwitchCase(arg, "jpeg")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_JpegCodec, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, 75))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "jpeg-quality")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, 100))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_JpegQuality, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#endif
		CommandLineSwitchCase(arg, "nego")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "pcb")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "pcid")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_bool(settings, FreeRDP_SendPreconnectionPdu, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_PreconnectionId, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#ifdef _WIN32
		CommandLineSwitchCase(arg, "connect-child-session")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationServiceClass,
			                                 "vs-debug") ||
			    !freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "localhost") ||
			    !freerdp_settings_set_string(settings, FreeRDP_AuthenticationPackageList, "ntlm") ||
			    !freerdp_settings_set_string(settings, FreeRDP_ClientAddress, "0.0.0.0") ||
			    !freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, FALSE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_VmConnectMode, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_ConnectChildSession, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
			    !freerdp_settings_set_uint32(settings, FreeRDP_AuthenticationLevel, 0) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE) ||
			    !freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType, CONNECTION_TYPE_LAN))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
#endif
		CommandLineSwitchCase(arg, "sec")
		{
			const int rc = parse_sec_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "encryption-methods")
		{
			const int rc = parse_encryption_methods_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "args-from")
		{
			WLog_ERR(TAG, "/args-from:%s can not be used in combination with other arguments!",
			         arg->Value);
			return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "from-stdin")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_CredentialsFromStdin, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (!arg->Value)
					return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
				promptForPassword = (option_equals(arg->Value, str_force));

				if (!promptForPassword)
					return fail_at(arg, COMMAND_LINE_ERROR);
			}
		}
		CommandLineSwitchCase(arg, "log-level")
		{
			wLog* root = WLog_GetRoot();

			if (!WLog_SetStringLogLevel(root, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "log-filters")
		{
			if (!WLog_AddStringLogFilters(arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "sec-rdp")
		{
			WLog_WARN(TAG, "/sec-rdp is deprecated, use /sec:rdp[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			WLog_WARN(TAG, "/sec-tls is deprecated, use /sec:tls[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			WLog_WARN(TAG, "/sec-nla is deprecated, use /sec:nla[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			WLog_WARN(TAG, "/sec-ext is deprecated, use /sec:ext[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#endif
		CommandLineSwitchCase(arg, "tls")
		{
			int rc = parse_tls_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "tls-ciphers")
		{
			WLog_WARN(TAG, "/tls-ciphers:<cipher list> is deprecated, use "
			               "/tls:ciphers:<cipher list> instead");
			int rc = parse_tls_cipher_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "tls-seclevel")
		{
			WLog_WARN(TAG,
			          "/tls-seclevel:<level> is deprecated, use /tls:sec-level:<level> instead");
			int rc = parse_tls_cipher_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "tls-secrets-file")
		{
			WLog_WARN(TAG, "/tls-secrets-file:<filename> is deprecated, use "
			               "/tls:secrets-file:<filename> instead");
			int rc = parse_tls_cipher_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "enforce-tlsv1_2")
		{
			WLog_WARN(TAG, "/enforce-tlsv1_2 is deprecated, use /tls:enforce:1.2 instead");
			int rc = parse_tls_cipher_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#endif
		CommandLineSwitchCase(arg, "cert")
		{
			const int rc = parse_cert_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}

#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "cert-name")
		{
			WLog_WARN(TAG, "/cert-name is deprecated, use /cert:name instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			WLog_WARN(TAG, "/cert-ignore is deprecated, use /cert:ignore instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "cert-tofu")
		{
			WLog_WARN(TAG, "/cert-tofu is deprecated, use /cert:tofu instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoAcceptCertificate, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "cert-deny")
		{
			WLog_WARN(TAG, "/cert-deny is deprecated, use /cert:deny instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoDenyCertificate, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
#endif
		CommandLineSwitchCase(arg, "authentication")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_Authentication, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "encryption")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, !enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "grab-keyboard")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GrabKeyboard, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "grab-mouse")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GrabMouse, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "mouse-relative")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_MouseUseRelativeMove, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "mouse")
		{
			const int rc = parse_mouse_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "unmap-buttons")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnmapButtons, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "toggle-fullscreen")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_ToggleFullscreen, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "force-console-callbacks")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseCommonStdioCallbacks, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "floatbar")
		{
			const int rc = parse_floatbar_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "mouse-motion")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_MouseMotion, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "parent-window")
		{
			ULONGLONG val = 0;

			if (!value_to_uint(arg->Value, &val, 0, UINT64_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint64(settings, FreeRDP_ParentWindowId, (UINT64)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "client-build-number")
		{
			ULONGLONG val = 0;

			if (!value_to_uint(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ClientBuild, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "cache")
		{
			int rc = parse_cache_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "bitmap-cache")
		{
			WLog_WARN(TAG, "/bitmap-cache is deprecated, use /cache:bitmap[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "persist-cache")
		{
			WLog_WARN(TAG, "/persist-cache is deprecated, use /cache:persist[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled, enable))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "persist-cache-file")
		{
			WLog_WARN(TAG, "/persist-cache-file:<filename> is deprecated, use "
			               "/cache:persist-file:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_BitmapCachePersistFile, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);

			if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);
		}
		CommandLineSwitchCase(arg, "offscreen-cache")
		{
			WLog_WARN(TAG, "/bitmap-cache is deprecated, use /cache:bitmap[:on|off] instead");
			if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel,
			                                 (UINT32)enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "glyph-cache")
		{
			WLog_WARN(TAG, "/glyph-cache is deprecated, use /cache:glyph[:on|off] instead");
			if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel,
			                                 arg->Value ? GLYPH_SUPPORT_FULL : GLYPH_SUPPORT_NONE))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "codec-cache")
		{
			WLog_WARN(TAG,
			          "/codec-cache:<option> is deprecated, use /cache:codec:<option> instead");
			const int rc = parse_codec_cache_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
#endif
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize,
			                                 (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "auto-request-control")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceRequestControl,
			                               enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "async-update")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AsyncUpdate, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "async-channels")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AsyncChannels, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "wm-class")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WmClass, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_PlayRemoteFxFile, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);

			if (!freerdp_settings_set_bool(settings, FreeRDP_PlayRemoteFx, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "auth-only")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AuthenticationOnly, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "auth-pkg-list")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationPackageList,
			                                 arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "auto-reconnect")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "auto-reconnect-max-retries")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, 1000))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries,
			                                 (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "reconnect-cookie")
		{
			const int rc = parse_reconnect_cookie_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "print-reconnect-cookie")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_PrintReconnectCookie, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "pwidth")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalWidth, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "pheight")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalHeight, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "orientation")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 0, UINT16_MAX))
				return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

			if (!freerdp_settings_set_uint16(settings, FreeRDP_DesktopOrientation, (UINT16)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "old-license")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_OldLicenseBehaviour, TRUE))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "scale")
		{
			const int rc = parse_scale_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "scale-desktop")
		{
			LONGLONG val = 0;

			if (!value_to_int(arg->Value, &val, 100, 500))
				return fail_at(arg, COMMAND_LINE_ERROR);

			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor, (UINT32)val))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "scale-device")
		{
			const int rc = parse_scale_device_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "action-script")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ActionScript, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, RDP2TCP_DVC_CHANNEL_NAME)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RDP2TCPArgs, arg->Value))
				return fail_at(arg, COMMAND_LINE_ERROR_MEMORY);
		}
		CommandLineSwitchCase(arg, "fipsmode")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_FIPSMode, enable))
				return fail_at(arg, COMMAND_LINE_ERROR);
		}
		CommandLineSwitchCase(arg, "smartcard-logon")
		{
			const int rc = parse_smartcard_logon_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchCase(arg, "tune")
		{
			const int rc = parse_tune_options(settings, arg);
			if (rc != 0)
				return fail_at(arg, rc);
		}
		CommandLineSwitchDefault(arg)
		{
			if (handle_option)
			{
				const int rc = handle_option(arg, handle_userdata);
				if (rc != 0)
					return fail_at(arg, rc);
			}
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	if (user)
	{
		if (!freerdp_settings_get_string(settings, FreeRDP_Domain) && user)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Username, NULL))
				return COMMAND_LINE_ERROR;

			if (!freerdp_settings_set_string(settings, FreeRDP_Domain, NULL))
				return COMMAND_LINE_ERROR;

			if (!freerdp_parse_username_settings(user, settings, FreeRDP_Username, FreeRDP_Domain))
				return COMMAND_LINE_ERROR;
		}
		else
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_Username, user))
				return COMMAND_LINE_ERROR;
		}
	}

	if (promptForPassword)
	{
		freerdp* instance = freerdp_settings_get_pointer_writable(settings, FreeRDP_instance);
		if (!freerdp_settings_get_string(settings, FreeRDP_Password))
		{
			char buffer[512 + 1] = { 0 };

			if (!freerdp_passphrase_read(instance->context, "Password: ", buffer,
			                             ARRAYSIZE(buffer) - 1, 1))
				return COMMAND_LINE_ERROR;
			if (!freerdp_settings_set_string(settings, FreeRDP_Password, buffer))
				return COMMAND_LINE_ERROR;
		}

		if (freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled) &&
		    !freerdp_settings_get_bool(settings, FreeRDP_GatewayUseSameCredentials))
		{
			if (!freerdp_settings_get_string(settings, FreeRDP_GatewayPassword))
			{
				char buffer[512 + 1] = { 0 };

				if (!freerdp_passphrase_read(instance->context, "Gateway Password: ", buffer,
				                             ARRAYSIZE(buffer) - 1, 1))
					return COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, buffer))
					return COMMAND_LINE_ERROR;
			}
		}
	}

	freerdp_performance_flags_make(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec) ||
	    freerdp_settings_get_bool(settings, FreeRDP_NSCodec) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportGraphicsPipeline))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE))
			return COMMAND_LINE_ERROR;
		if (!freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, TRUE))
			return COMMAND_LINE_ERROR;
	}

	arg = CommandLineFindArgumentA(largs, "port");
	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		LONGLONG val = 0;

		if (!value_to_int(arg->Value, &val, 1, UINT16_MAX))
			return fail_at(arg, COMMAND_LINE_ERROR_UNEXPECTED_VALUE);

		if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, (UINT32)val))
			return fail_at(arg, COMMAND_LINE_ERROR);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_VmConnectMode))
	{
		const COMMAND_LINE_ARGUMENT_A* nego = CommandLineFindArgumentA(largs, "nego");
		if (nego)
			return fail_at(arg, COMMAND_LINE_ERROR);

		const UINT32 port = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);
		WLog_INFO(TAG, "/vmconnect uses custom port %" PRIu32, port);
	}

	fill_credential_strings(largs);

	return status;
}

static void argv_free(int* pargc, char** pargv[])
{
	WINPR_ASSERT(pargc);
	WINPR_ASSERT(pargv);
	const int argc = *pargc;
	char** argv = *pargv;
	*pargc = 0;
	*pargv = NULL;

	if (!argv)
		return;
	for (int x = 0; x < argc; x++)
		free(argv[x]);
	free((void*)argv);
}

static BOOL argv_append(int* pargc, char** pargv[], char* what)
{
	WINPR_ASSERT(pargc);
	WINPR_ASSERT(pargv);

	if (*pargc < 0)
		return FALSE;

	if (!what)
		return FALSE;

	int nargc = *pargc + 1;
	char** tmp = (char**)realloc((void*)*pargv, (size_t)nargc * sizeof(char*));
	if (!tmp)
		return FALSE;

	tmp[*pargc] = what;
	*pargv = tmp;
	*pargc = nargc;
	return TRUE;
}

static BOOL argv_append_dup(int* pargc, char** pargv[], const char* what)
{
	char* copy = NULL;
	if (what)
		copy = _strdup(what);

	const BOOL rc = argv_append(pargc, pargv, copy);
	if (!rc)
		free(copy);
	return rc;
}

static BOOL args_from_fp(FILE* fp, int* aargc, char** aargv[], const char* file, const char* cmd)
{
	BOOL success = FALSE;

	WINPR_ASSERT(aargc);
	WINPR_ASSERT(aargv);
	WINPR_ASSERT(cmd);

	if (!fp)
	{
		WLog_ERR(TAG, "Failed to read command line options from file '%s'", file);
		return FALSE;
	}
	if (!argv_append_dup(aargc, aargv, cmd))
		goto fail;
	while (!feof(fp))
	{
		char* line = NULL;
		size_t size = 0;
		INT64 rc = GetLine(&line, &size, fp);
		if ((rc < 0) || !line)
		{
			/* abort if GetLine failed due to reaching EOF */
			if (feof(fp))
				break;
			goto fail;
		}

		while (rc > 0)
		{
			const char cur = (line[rc - 1]);
			if ((cur == '\n') || (cur == '\r'))
			{
				line[rc - 1] = '\0';
				rc--;
			}
			else
				break;
		}
		/* abort on empty lines */
		if (rc == 0)
		{
			free(line);
			break;
		}
		if (!argv_append(aargc, aargv, line))
		{
			free(line);
			goto fail;
		}
	}

	success = TRUE;
fail:
	fclose(fp);
	if (!success)
		argv_free(aargc, aargv);
	return success;
}

static BOOL args_from_env(const char* name, int* aargc, char** aargv[], const char* arg,
                          const char* cmd)
{
	BOOL success = FALSE;
	char* env = NULL;

	WINPR_ASSERT(aargc);
	WINPR_ASSERT(aargv);
	WINPR_ASSERT(cmd);

	if (!name)
	{
		WLog_ERR(TAG, "%s - environment variable name empty", arg);
		goto cleanup;
	}

	const DWORD size = GetEnvironmentVariableX(name, env, 0);
	if (size == 0)
	{
		WLog_ERR(TAG, "%s - no environment variable '%s'", arg, name);
		goto cleanup;
	}
	env = calloc(size + 1, sizeof(char));
	if (!env)
		goto cleanup;
	const DWORD rc = GetEnvironmentVariableX(name, env, size);
	if (rc != size - 1)
		goto cleanup;
	if (rc == 0)
	{
		WLog_ERR(TAG, "%s - environment variable '%s' is empty", arg);
		goto cleanup;
	}

	if (!argv_append_dup(aargc, aargv, cmd))
		goto cleanup;

	char* context = NULL;
	char* tok = strtok_s(env, "\n", &context);
	while (tok)
	{
		if (!argv_append_dup(aargc, aargv, tok))
			goto cleanup;
		tok = strtok_s(NULL, "\n", &context);
	}

	success = TRUE;
cleanup:
	free(env);
	if (!success)
		argv_free(aargc, aargv);
	return success;
}

int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings, int oargc,
                                                         char* oargv[], BOOL allowUnknown)
{
	return freerdp_client_settings_parse_command_line_arguments_ex(
	    settings, oargc, oargv, allowUnknown, NULL, 0, NULL, NULL);
}

int freerdp_client_settings_parse_command_line_arguments_ex(
    rdpSettings* settings, int oargc, char** oargv, BOOL allowUnknown,
    COMMAND_LINE_ARGUMENT_A* args, size_t count,
    int (*handle_option)(const COMMAND_LINE_ARGUMENT_A* arg, void* custom), void* handle_userdata)
{
	int argc = oargc;
	char** argv = oargv;
	int res = -1;
	int aargc = 0;
	char** aargv = NULL;
	if ((argc == 2) && option_starts_with("/args-from:", argv[1]))
	{
		BOOL success = FALSE;
		const char* file = strchr(argv[1], ':') + 1;
		FILE* fp = stdin;

		if (option_starts_with("fd:", file))
		{
			ULONGLONG result = 0;
			const char* val = strchr(file, ':') + 1;
			if (!value_to_uint(val, &result, 0, INT_MAX))
				return -1;
			fp = fdopen((int)result, "r");
			success = args_from_fp(fp, &aargc, &aargv, file, oargv[0]);
		}
		else if (strncmp(file, "env:", 4) == 0)
		{
			const char* name = strchr(file, ':') + 1;
			success = args_from_env(name, &aargc, &aargv, oargv[1], oargv[0]);
		}
		else if (strcmp(file, "stdin") != 0)
		{
			fp = winpr_fopen(file, "r");
			success = args_from_fp(fp, &aargc, &aargv, file, oargv[0]);
		}
		else
			success = args_from_fp(fp, &aargc, &aargv, file, oargv[0]);

		if (!success)
			return -1;
		argc = aargc;
		argv = aargv;
	}

	WINPR_ASSERT(count <= SSIZE_MAX);
	size_t lcount = 0;
	COMMAND_LINE_ARGUMENT_A* largs = create_merged_args(args, (SSIZE_T)count, &lcount);
	if (!largs)
		goto fail;

	res = freerdp_client_settings_parse_command_line_arguments_int(
	    settings, argc, argv, allowUnknown, largs, lcount, handle_option, handle_userdata);
fail:
	free(largs);
	argv_free(&aargc, &aargv);
	return res;
}

static BOOL freerdp_client_load_static_channel_addin(rdpChannels* channels, rdpSettings* settings,
                                                     const char* name, void* data)
{
	PVIRTUALCHANNELENTRY entry = NULL;
	PVIRTUALCHANNELENTRY pvce = freerdp_load_channel_addin_entry(
	    name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX);
	PVIRTUALCHANNELENTRYEX pvceex = WINPR_FUNC_PTR_CAST(pvce, PVIRTUALCHANNELENTRYEX);

	if (!pvceex)
		entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (pvceex)
	{
		if (freerdp_channels_client_load_ex(channels, settings, pvceex, data) == 0)
		{
			WLog_DBG(TAG, "loading channelEx %s", name);
			return TRUE;
		}
	}
	else if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			WLog_DBG(TAG, "loading channel %s", name);
			return TRUE;
		}
	}

	return FALSE;
}

typedef struct
{
	FreeRDP_Settings_Keys_Bool settingId;
	const char* channelName;
	void* args;
} ChannelToLoad;

BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	ChannelToLoad dynChannels[] = {
#if defined(CHANNEL_AINPUT_CLIENT)
		{ FreeRDP_BOOL_UNUSED, AINPUT_CHANNEL_NAME, NULL }, /* always loaded */
#endif
		{ FreeRDP_AudioCapture, AUDIN_CHANNEL_NAME, NULL },
		{ FreeRDP_AudioPlayback, RDPSND_CHANNEL_NAME, NULL },
#ifdef CHANNEL_RDPEI_CLIENT
		{ FreeRDP_MultiTouchInput, RDPEI_CHANNEL_NAME, NULL },
#endif
		{ FreeRDP_SupportGraphicsPipeline, RDPGFX_CHANNEL_NAME, NULL },
		{ FreeRDP_SupportEchoChannel, ECHO_CHANNEL_NAME, NULL },
		{ FreeRDP_SupportSSHAgentChannel, "sshagent", NULL },
		{ FreeRDP_SupportDisplayControl, DISP_CHANNEL_NAME, NULL },
		{ FreeRDP_SupportGeometryTracking, GEOMETRY_CHANNEL_NAME, NULL },
		{ FreeRDP_SupportVideoOptimized, VIDEO_CHANNEL_NAME, NULL },
		{ FreeRDP_RemoteCredentialGuard, RDPEAR_CHANNEL_NAME, NULL },
	};

	ChannelToLoad staticChannels[] = {
		{ FreeRDP_AudioPlayback, RDPSND_CHANNEL_NAME, NULL },
		{ FreeRDP_RedirectClipboard, CLIPRDR_SVC_CHANNEL_NAME, NULL },
#if defined(CHANNEL_ENCOMSP_CLIENT)
		{ FreeRDP_EncomspVirtualChannel, ENCOMSP_SVC_CHANNEL_NAME, settings },
#endif
		{ FreeRDP_RemdeskVirtualChannel, REMDESK_SVC_CHANNEL_NAME, settings },
		{ FreeRDP_RemoteApplicationMode, RAIL_SVC_CHANNEL_NAME, settings }
	};

	/**
	 * Step 1: first load dynamic channels according to the settings
	 */
	for (size_t i = 0; i < ARRAYSIZE(dynChannels); i++)
	{
		if ((dynChannels[i].settingId == FreeRDP_BOOL_UNUSED) ||
		    freerdp_settings_get_bool(settings, dynChannels[i].settingId))
		{
			const char* const p[] = { dynChannels[i].channelName };

			if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
				return FALSE;
		}
	}

	/**
	 * step 2: do various adjustments in the settings to handle channels and settings dependencies
	 */
	if ((freerdp_static_channel_collection_find(settings, RDPSND_CHANNEL_NAME)) ||
	    (freerdp_dynamic_channel_collection_find(settings, RDPSND_CHANNEL_NAME))
#if defined(CHANNEL_TSMF_CLIENT)
	    || (freerdp_dynamic_channel_collection_find(settings, "tsmf"))
#endif
	)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE; /* rdpsnd requires rdpdr to be registered */
		if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, TRUE))
			return FALSE; /* Both rdpsnd and tsmf require this flag to be set */
	}

	if (freerdp_dynamic_channel_collection_find(settings, AUDIN_CHANNEL_NAME))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, TRUE))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportHeartbeatPdu) ||
	    freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE; /* these RDP8 features require rdpdr to be registered */
	}

	const char* DrivesToRedirect = freerdp_settings_get_string(settings, FreeRDP_DrivesToRedirect);

	if (DrivesToRedirect && (strlen(DrivesToRedirect) != 0))
	{
		/*
		 * Drives to redirect:
		 *
		 * Very similar to DevicesToRedirect, but can contain a
		 * comma-separated list of drive letters to redirect.
		 */
		char* value = NULL;
		char* tok = NULL;
		char* context = NULL;

		value = _strdup(DrivesToRedirect);
		if (!value)
			return FALSE;

		tok = strtok_s(value, ";", &context);
		if (!tok)
		{
			WLog_ERR(TAG, "DrivesToRedirect contains invalid data: '%s'", DrivesToRedirect);
			free(value);
			return FALSE;
		}

		while (tok)
		{
			/* Syntax: Comma separated list of the following entries:
			 * '*'              ... Redirect all drives, including hotplug
			 * 'DynamicDrives'  ... hotplug
			 * '%'              ... user home directory
			 * <label>(<path>)  ... One or more paths to redirect.
			 * <path>(<label>)  ... One or more paths to redirect.
			 * <path>           ... One or more paths to redirect.
			 */
			/* TODO: Need to properly escape labels and paths */
			BOOL success = 0;
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
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_RedirectDrives))
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			const char* const params[] = { "drive", "media", "*" };

			if (!freerdp_client_add_device_channel(settings, ARRAYSIZE(params), params))
				return FALSE;
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectDrives) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectHomeDrive) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectSerialPorts) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards) ||
	    freerdp_settings_get_bool(settings, FreeRDP_RedirectPrinters))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			return FALSE; /* All of these features require rdpdr */
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectHomeDrive))
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			const char* params[] = { "drive", "home", "%" };

			if (!freerdp_client_add_device_channel(settings, ARRAYSIZE(params), params))
				return FALSE;
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_DeviceRedirection))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RDPDR_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;

		if (!freerdp_static_channel_collection_find(settings, RDPSND_CHANNEL_NAME) &&
		    !freerdp_dynamic_channel_collection_find(settings, RDPSND_CHANNEL_NAME))
		{
			const char* const params[] = { RDPSND_CHANNEL_NAME, "sys:fake" };

			if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(params), params))
				return FALSE;

			if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(params), params))
				return FALSE;
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards))
	{
		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
		{
			RDPDR_DEVICE* smartcard = freerdp_device_new(RDPDR_DTYP_SMARTCARD, 0, NULL);

			if (!smartcard)
				return FALSE;

			if (!freerdp_device_collection_add(settings, smartcard))
			{
				freerdp_device_free(smartcard);
				return FALSE;
			}
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectPrinters))
	{
		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_PRINT))
		{
			RDPDR_DEVICE* printer = freerdp_device_new(RDPDR_DTYP_PRINT, 0, NULL);

			if (!printer)
				return FALSE;

			if (!freerdp_device_collection_add(settings, printer))
			{
				freerdp_device_free(printer);
				return FALSE;
			}
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_LyncRdpMode))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_EncomspVirtualChannel, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemdeskVirtualChannel, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, FALSE))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteAssistanceMode))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_EncomspVirtualChannel, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemdeskVirtualChannel, TRUE))
			return FALSE;
		if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
			return FALSE;
	}

	/* step 3: schedule some static channels to load depending on the settings */
	for (size_t i = 0; i < ARRAYSIZE(staticChannels); i++)
	{
		if ((staticChannels[i].settingId == 0) ||
		    freerdp_settings_get_bool(settings, staticChannels[i].settingId))
		{
			if (staticChannels[i].args)
			{
				if (!freerdp_client_load_static_channel_addin(
				        channels, settings, staticChannels[i].channelName, staticChannels[i].args))
					return FALSE;
			}
			else
			{
				const char* const p[] = { staticChannels[i].channelName };
				if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(p), p))
					return FALSE;
			}
		}
	}

	char* RDP2TCPArgs = freerdp_settings_get_string_writable(settings, FreeRDP_RDP2TCPArgs);
	if (RDP2TCPArgs)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RDP2TCP_DVC_CHANNEL_NAME,
		                                              RDP2TCPArgs))
			return FALSE;
	}

	/* step 4: do the static channels loading and init */
	for (UINT32 i = 0; i < freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount); i++)
	{
		ADDIN_ARGV* _args =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_StaticChannelArray, i);

		if (!freerdp_client_load_static_channel_addin(channels, settings, _args->argv[0], _args))
			return FALSE;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount) > 0)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicChannels, TRUE))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportDynamicChannels))
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, DRDYNVC_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;
	}

	return TRUE;
}

void freerdp_client_warn_unmaintained(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[unmaintained] %s client is currently unmaintained!",
	                     app);
	WLog_Print_unchecked(
	    log, log_level,
	    " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	    "known issues!");
	WLog_Print_unchecked(
	    log, log_level,
	    "Be prepared to fix issues yourself though as nobody is actively working on this.");
	WLog_Print_unchecked(
	    log, log_level,
	    " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone) - if you intend using this component write us a message");
}

void freerdp_client_warn_experimental(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[experimental] %s client is currently experimental!",
	                     app);
	WLog_Print_unchecked(
	    log, log_level,
	    " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	    "known issues or create a new one!");
	WLog_Print_unchecked(
	    log, log_level,
	    " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone)");
}

void freerdp_client_warn_deprecated(int argc, char* argv[])
{
	const char* app = (argc > 0) ? argv[0] : "INVALID_ARGV";
	const DWORD log_level = WLOG_WARN;
	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	if (!WLog_IsLevelActive(log, log_level))
		return;

	WLog_Print_unchecked(log, log_level, "[deprecated] %s client has been deprecated", app);
	WLog_Print_unchecked(log, log_level, "As replacement there is a SDL based client available.");
	WLog_Print_unchecked(
	    log, log_level,
	    "If you are interested in keeping %s alive get in touch with the developers", app);
	WLog_Print_unchecked(
	    log, log_level,
	    "The project is hosted at https://github.com/freerdp/freerdp and "
	    " developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- dont hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone)");
}
