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
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/path.h>
#include <winpr/ncrypt.h>

#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/channels/encomsp.h>
#include <freerdp/channels/rdp2tcp.h>
#include <freerdp/channels/remdesk.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/channels/disp.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/utils/proxy_utils.h>
#include <freerdp/channels/urbdrc.h>
#include <freerdp/channels/rdpdr.h>

#if defined(CHANNEL_AINPUT_CLIENT)
#include <freerdp/channels/ainput.h>
#endif

#include <freerdp/client/cmdline.h>
#include <freerdp/version.h>
#include <freerdp/client/utils/smartcard_cli.h>

#include "cmdline.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.cmdline")

static BOOL option_starts_with(const char* what, const char* val);
static BOOL option_ends_with(const char* str, const char* ext);
static BOOL option_equals(const char* what, const char* val);

static BOOL freerdp_client_print_codepages(const char* arg)
{
	size_t count = 0;
	DWORD column = 2;
	const char* filter = NULL;
	RDP_CODEPAGE* pages;

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
		char buffer[80] = { 0 };

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
	char* dname;
	RDPDR_DEVICE* device = NULL;

	if (name)
	{
		/* Path was entered as secondary argument, swap */
		if (winpr_PathFileExists(name))
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
	else
	{
		BOOL isSpecial = FALSE;
		BOOL isPath = freerdp_path_valid(path, &isSpecial);

		if (!isPath && !isSpecial)
			goto fail;
	}

	if (!freerdp_device_collection_add(settings, device))
		goto fail;

	return TRUE;

fail:
	freerdp_device_free(device);
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
	size_t size;
	char* tmp;
	if (!dst || !value)
		return FALSE;

	if (*dst)
		x = strlen(*dst);
	y = strlen(value);

	size = x + y + 2;
	tmp = realloc(*dst, size);
	if (!tmp)
		return FALSE;
	if (x == 0)
		tmp[0] = '\0';
	else
		winpr_str_append(",", tmp, size, NULL);
	winpr_str_append(value, tmp, size, NULL);
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
	printf("This is FreeRDP version %s (%s)\n", FREERDP_VERSION_FULL, FREERDP_GIT_REVISION);
	return TRUE;
}

BOOL freerdp_client_print_buildconfig(void)
{
	printf("%s", freerdp_get_build_config());
	return TRUE;
}

static void freerdp_client_print_scancodes(void)
{
	DWORD x;
	printf("RDP scancodes and their name for use with /kbd:remap\n");

	for (x = 0; x < UINT16_MAX; x++)
	{
		const char* name = freerdp_keyboard_scancode_name(x);
		if (name)
			printf("0x%04" PRIx32 "  --> %s\n", x, name);
	}
}

static BOOL is_delimiter(const char* delimiters, char c)
{
	char d;
	while ((d = *delimiters++) != '\0')
	{
		if (d == c)
			return TRUE;
	}
	return FALSE;
}

static char* print_token(char* text, size_t start_offset, size_t* current, size_t limit,
                         const char* delimiters)
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
			if (is_delimiter(delimiters, text[x]))
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
	char* cur = print_token(str, start_offset, &current, limit, "[], ");

	while (cur)
	{
		cur++;
		cur = print_token(cur, start_offset + 1, &current, limit, "[], ");
	}

	free(str);
	return current;
}

static size_t print_description(const char* text, size_t start_offset, size_t current)
{
	const size_t limit = 80;
	char* str = _strdup(text);
	char* cur = print_token(str, start_offset, &current, limit, " ");

	while (cur)
	{
		cur++;
		cur = print_token(cur, start_offset, &current, limit, " ");
	}

	free(str);
	current += (size_t)printf("\n");
	return current;
}

static void freerdp_client_print_command_line_args(const COMMAND_LINE_ARGUMENT_A* arg)
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
                                               const COMMAND_LINE_ARGUMENT_A* custom)
{
	const char* name = "FreeRDP";
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

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
	printf("Smartcard logon with Kerberos authentication: /smartcard-logon /sec:nla\n");

	printf("Serial Port Redirection: /serial:<name>,<device>,[SerCx2|SerCx|Serial],[permissive]\n");
	printf("Serial Port Redirection: /serial:COM1,/dev/ttyS0\n");
	printf("Parallel Port Redirection: /parallel:<name>,<device>\n");
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
		size_t length;
		rdpSettings* settings;

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

BOOL freerdp_client_add_device_channel(rdpSettings* settings, size_t count, const char** params)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(params);
	WINPR_ASSERT(count > 0);

	if (option_equals(params[0], "drive"))
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
	else if (option_equals(params[0], "printer"))
	{
		RDPDR_DEVICE* printer;

		if (count < 1)
			return FALSE;

		settings->RedirectPrinters = TRUE;
		settings->DeviceRedirection = TRUE;

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
		RDPDR_DEVICE* smartcard;

		if (count < 1)
			return FALSE;

		settings->RedirectSmartCards = TRUE;
		settings->DeviceRedirection = TRUE;

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
	else if (option_equals(params[0], "serial"))
	{
		RDPDR_DEVICE* serial;

		if (count < 1)
			return FALSE;

		settings->RedirectSerialPorts = TRUE;
		settings->DeviceRedirection = TRUE;

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
	else if (option_equals(params[0], "parallel"))
	{
		RDPDR_DEVICE* parallel;

		if (count < 1)
			return FALSE;

		settings->RedirectParallelPorts = TRUE;
		settings->DeviceRedirection = TRUE;

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

BOOL freerdp_client_add_static_channel(rdpSettings* settings, size_t count, const char** params)
{
	ADDIN_ARGV* _args;

	if (!settings || !params || !params[0] || (count > INT_MAX))
		return FALSE;

	if (freerdp_static_channel_collection_find(settings, params[0]))
		return TRUE;

	_args = freerdp_addin_argv_new(count, (const char**)params);

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

BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, size_t count, const char** params)
{
	ADDIN_ARGV* _args;

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

static BOOL read_pem_file(rdpSettings* settings, size_t id, const char* file)
{
	INT64 s;
	int rs;
	size_t fr;
	char* ptr;
	BOOL rc = FALSE;
	FILE* fp = winpr_fopen(file, "r");
	if (!fp)
		goto fail;
	rs = _fseeki64(fp, 0, SEEK_END);
	if (rs < 0)
		goto fail;
	s = _ftelli64(fp);
	if (s < 0)
		goto fail;
	rs = _fseeki64(fp, 0, SEEK_SET);
	if (rs < 0)
		goto fail;

	if (!freerdp_settings_set_string_len(settings, id, NULL, (size_t)s + 1ull))
		goto fail;

	ptr = freerdp_settings_get_string_writable(settings, id);
	fr = fread(ptr, (size_t)s, 1, fp);
	if (fr != 1)
		goto fail;
	rc = TRUE;
fail:
	if (!rc)
	{
		char buffer[8192] = { 0 };
		WLog_WARN(TAG, "Failed to read file '%s' [%s]", file,
		          winpr_strerror(errno, buffer, sizeof(buffer)));
	}
	if (fp)
		fclose(fp);
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
	size_t id;
	CmdLineSubOptionType opttype;
	CmdLineSubOptionCb cb;
} CmdLineSubOptions;

static BOOL parseSubOptions(rdpSettings* settings, const CmdLineSubOptions* opts, size_t count,
                            const char* arg)
{
	BOOL found = FALSE;
	size_t xx;

	for (xx = 0; xx < count; xx++)
	{
		const CmdLineSubOptions* opt = &opts[xx];

		if (option_starts_with(opt->optname, arg))
		{
			const size_t optlen = strlen(opt->optname);
			const char* val = &arg[optlen];
			BOOL status;

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

static int freerdp_client_command_line_post_filter(void* context, COMMAND_LINE_ARGUMENT_A* arg)
{
	rdpSettings* settings = (rdpSettings*)context;
	BOOL status = TRUE;
	BOOL enable = arg->Value ? TRUE : FALSE;
	union
	{
		char** p;
		const char** pc;
	} ptr;

	CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "a")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

		if ((status = freerdp_client_add_device_channel(settings, count, ptr.pc)))
		{
			settings->DeviceRedirection = TRUE;
		}

		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "kerberos")
	{
		size_t count;

		ptr.p = CommandLineParseCommaSeparatedValuesEx("kerberos", arg->Value, &count);
		if (ptr.pc)
		{
			size_t x;
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

			for (x = 1; x < count; x++)
			{
				const char* cur = ptr.pc[x];
				if (!parseSubOptions(settings, opts, ARRAYSIZE(opts), cur))
				{
					free(ptr.p);
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}
		}
		free(ptr.p);
	}

	CommandLineSwitchCase(arg, "vc")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		status = freerdp_client_add_static_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "dvc")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "drive")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "serial")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "parallel")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "smartcard")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "printer")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(arg->Name, arg->Value, &count);
		status = freerdp_client_add_device_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "usb")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(URBDRC_CHANNEL_NAME, arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, ptr.pc);
		free(ptr.p);
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
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx(RDPSND_CHANNEL_NAME, arg->Value, &count);
		status = freerdp_client_add_static_channel(settings, count, ptr.pc);
		if (status)
		{
			status = freerdp_client_add_dynamic_channel(settings, count, ptr.pc);
		}
		free(ptr.p);
	}
	CommandLineSwitchCase(arg, "microphone")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx("audin", arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, ptr.pc);
		free(ptr.p);
	}
#if defined(CHANNEL_TSMF_CLIENT)
	CommandLineSwitchCase(arg, "multimedia")
	{
		size_t count;
		ptr.p = CommandLineParseCommaSeparatedValuesEx("tsmf", arg->Value, &count);
		status = freerdp_client_add_dynamic_channel(settings, count, ptr.pc);
		free(ptr.p);
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
	CommandLineSwitchEnd(arg)

	        return status
	    ? 1
	    : -1;
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

static BOOL freerdp_apply_connection_type(rdpSettings* settings, UINT32 type)
{
	struct network_settings
	{
		size_t id;
		BOOL value[7];
	};
	const struct network_settings config[] = {
		{ FreeRDP_DisableWallpaper, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_AllowFontSmoothing, { FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE } },
		{ FreeRDP_AllowDesktopComposition, { FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE } },
		{ FreeRDP_DisableFullWindowDrag, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_DisableMenuAnims, { TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE } },
		{ FreeRDP_DisableThemes, { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE } },
		{ FreeRDP_NetworkAutoDetect, { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE } }
	};

	switch (type)
	{
		case CONNECTION_TYPE_MODEM:
		case CONNECTION_TYPE_BROADBAND_LOW:
		case CONNECTION_TYPE_BROADBAND_HIGH:
		case CONNECTION_TYPE_SATELLITE:
		case CONNECTION_TYPE_WAN:
		case CONNECTION_TYPE_LAN:
		case CONNECTION_TYPE_AUTODETECT:
			break;
		default:
			WLog_WARN(TAG, "Invalid ConnectionType %" PRIu32 ", aborting", type);
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
		case CONNECTION_TYPE_MODEM:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_BROADBAND_LOW:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_SATELLITE:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_BROADBAND_HIGH:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_WAN:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_LAN:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
			break;
		case CONNECTION_TYPE_AUTODETECT:
			if (!freerdp_apply_connection_type(settings, type))
				return FALSE;
				/* Automatically activate GFX and RFX codec support */
#ifdef WITH_GFX_H264
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE))
				return FALSE;
#endif
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE) ||
			    !freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, TRUE))
				return FALSE;
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static UINT32 freerdp_get_keyboard_layout_for_type(const char* name, DWORD type)
{
	size_t count = 0, x;
	RDP_KEYBOARD_LAYOUT* layouts =
	    freerdp_keyboard_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD, &count);

	if (!layouts || (count == 0))
		return FALSE;

	for (x = 0; x < count; x++)
	{
		const RDP_KEYBOARD_LAYOUT* layout = &layouts[x];
		if (option_equals(layout->name, name))
		{
			return layout->code;
		}
	}

	freerdp_keyboard_layouts_free(layouts, count);
	return 0;
}

static UINT32 freerdp_map_keyboard_layout_name_to_id(const char* name)
{
	size_t x;
	const UINT32 variants[] = { RDP_KEYBOARD_LAYOUT_TYPE_STANDARD, RDP_KEYBOARD_LAYOUT_TYPE_VARIANT,
		                        RDP_KEYBOARD_LAYOUT_TYPE_IME };

	for (x = 0; x < ARRAYSIZE(variants); x++)
	{
		UINT32 rc = freerdp_get_keyboard_layout_for_type(name, variants[x]);
		if (rc > 0)
			return rc;
	}

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
	int status;
	DWORD flags;
	int detect_status;
	const COMMAND_LINE_ARGUMENT_A* arg;
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
	int status;
	DWORD flags;
	int detect_status;
	const COMMAND_LINE_ARGUMENT_A* arg;
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
	int posix_cli_status;
	size_t posix_cli_count;
	int windows_cli_status;
	size_t windows_cli_count;
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

	size_t x, count = 0;
	RDP_KEYBOARD_LAYOUT* layouts;
	layouts = freerdp_keyboard_get_layouts(type, &count);

	printf("\n%s\n", msg);

	for (x = 0; x < count; x++)
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

static void freerdp_client_print_tune_list(const rdpSettings* settings)
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
}

int freerdp_client_settings_command_line_status_print_ex(rdpSettings* settings, int status,
                                                         int argc, char** argv,
                                                         const COMMAND_LINE_ARGUMENT_A* custom)
{
	const COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		freerdp_client_print_version();
		goto out;
	}

	if (status == COMMAND_LINE_STATUS_PRINT_BUILDCONFIG)
	{
		freerdp_client_print_version();
		freerdp_client_print_buildconfig();
		goto out;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		CommandLineParseArgumentsA(argc, argv, largs, 0x112, NULL, NULL, NULL);

		arg = CommandLineFindArgumentA(largs, "list");
		WINPR_ASSERT(arg);

		if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
		{
			if (option_equals("tune", arg->Value))
				freerdp_client_print_tune_list(settings);
			else if (option_equals("kbd", arg->Value))
				freerdp_client_print_keyboard_list();
			else if (option_equals("kbd-lang", arg->Value))
			{
				const char* val = NULL;
				if (option_starts_with("kbd-lang:", arg->Value))
					val = &arg->Value[9];
				freerdp_client_print_codepages(val);
			}
			else if (option_equals("kbd-scancode", arg->Value))
				freerdp_client_print_scancodes();
			else if (option_equals("monitor", arg->Value))
				settings->ListMonitors = TRUE;
			else if (option_equals("smartcard", arg->Value))
				freerdp_smartcard_list(settings);
			else
				return COMMAND_LINE_ERROR;
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
			settings->ListMonitors = TRUE;
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

static BOOL prepare_default_settings(rdpSettings* settings, COMMAND_LINE_ARGUMENT_A* args,
                                     BOOL rdp_file)
{
	size_t x;
	const char* arguments[] = { "network", "gfx", "rfx", "bpp" };
	WINPR_ASSERT(settings);
	WINPR_ASSERT(args);

	if (rdp_file)
		return FALSE;

	for (x = 0; x < ARRAYSIZE(arguments); x++)
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
	settings->SmartcardEmulation = TRUE;
	return TRUE;
}

BOOL option_starts_with(const char* what, const char* val)
{
	WINPR_ASSERT(what);
	WINPR_ASSERT(val);
	const size_t wlen = strlen(what);

	return _strnicmp(what, val, wlen) == 0;
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

static int parse_on_off_option(const char* value)
{
	WINPR_ASSERT(value);
	const char* sep = strchr(value, ':');
	if (!sep)
		return 1;
	if (option_equals("on", &sep[1]))
		return 1;
	if (option_equals("off", &sep[1]))
		return 0;
	return -1;
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
	LONGLONG val;

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
		const struct map_t map[] = {
			{ "1.0", TLS1_VERSION },
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

static int parse_tls_options(rdpSettings* settings, const COMMAND_LINE_ARGUMENT_A* arg)
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
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxAVC444 = bval > 0;
					codecSelected = TRUE;
				}
				else if (option_starts_with("AVC420", val))
				{
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxH264 = bval > 0;
					codecSelected = TRUE;
				}
				else
#endif
				    if (option_starts_with("RFX", val))
				{
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						RemoteFxCodec = bval > 0;
					codecSelected = TRUE;
				}
				else if (option_starts_with("progressive", val))
				{
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else
						GfxProgressive = bval > 0;
					codecSelected = TRUE;
				}
				else if (option_starts_with("mask:", val))
				{
					ULONGLONG v;
					const char* uv = &val[5];
					if (!value_to_uint(uv, &v, 0, UINT32_MAX))
						rc = COMMAND_LINE_ERROR;
					else
						settings->GfxCapsFilter = (UINT32)v;
				}
				else if (option_starts_with("small-cache", val))
				{
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, bval > 0))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("thin-client", val))
				{
					const int bval = parse_on_off_option(val);
					if (bval < 0)
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, bval > 0))
						rc = COMMAND_LINE_ERROR;
					if ((rc == CHANNEL_RC_OK) && (bval > 0))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, bval > 0))
							rc = COMMAND_LINE_ERROR;
					}
				}
			}

			if ((rc == CHANNEL_RC_OK) && codecSelected)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, GfxAVC444))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, GfxH264))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, RemoteFxCodec))
					rc = COMMAND_LINE_ERROR;
				if (!freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, GfxProgressive))
					rc = COMMAND_LINE_ERROR;
			}
		}
		free(ptr);
		if (rc != CHANNEL_RC_OK)
			return rc;
	}
	return CHANNEL_RC_OK;
}

static int parse_kbd_layout(rdpSettings* settings, const char* value)
{
	int rc = 0;
	LONGLONG ival;
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

static BOOL check_kbd_remap_valid(const char* token)
{
	DWORD key, value;

	WINPR_ASSERT(token);
	/* The remapping is only allowed for scancodes, so maximum is 999=999 */
	if (strlen(token) > 10)
		return FALSE;

	int rc = sscanf(token, "%" PRIu32 "=%" PRIu32, &key, &value);
	if (rc != 2)
		rc = sscanf(token, "%" PRIx32 "=%" PRIx32 "", &key, &value);
	if (rc != 2)
		rc = sscanf(token, "%" PRIu32 "=%" PRIx32, &key, &value);
	if (rc != 2)
		rc = sscanf(token, "%" PRIx32 "=%" PRIu32, &key, &value);
	if (rc != 2)
	{
		WLog_WARN(TAG, "/kbd:remap invalid entry '%s'", token);
		return FALSE;
	}
	return TRUE;
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
				/* Append this new occurance to the already existing list */
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
						_snprintf(tmp, tlen, "%s,%s", old, now);
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
				LONGLONG ival;
				const BOOL isInt = value_to_int(&val[5], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardCodePage,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("type:", val))
			{
				LONGLONG ival;
				const BOOL isInt = value_to_int(&val[5], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("subtype:", val))
			{
				LONGLONG ival;
				const BOOL isInt = value_to_int(&val[8], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("fn-key:", val))
			{
				LONGLONG ival;
				const BOOL isInt = value_to_int(&val[7], &ival, 1, UINT32_MAX);
				if (!isInt)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey,
				                                      (UINT32)ival))
					rc = COMMAND_LINE_ERROR;
			}
			else if (option_starts_with("unicode", val))
			{
				const int bval = parse_on_off_option(val);
				if (bval < 0)
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				else if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, bval > 0))
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
	return rc;
}

static int parse_app_option_program(rdpSettings* settings, const char* cmd)
{
	const size_t ids[] = { FreeRDP_RemoteApplicationMode, FreeRDP_RemoteAppLanguageBarSupported,
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
			size_t id;
			int (*fkt)(rdpSettings* settings, const char* value);
		};
		const struct app_map amap[] = {
			{ "program:", FreeRDP_RemoteApplicationProgram, parse_app_option_program },
			{ "workdir:", FreeRDP_RemoteApplicationWorkingDir, NULL },
			{ "name:", FreeRDP_RemoteApplicationName, NULL },
			{ "icon:", FreeRDP_RemoteApplicationIcon, NULL },
			{ "cmd:", FreeRDP_RemoteApplicationCmdLine, NULL },
			{ "file:", FreeRDP_RemoteApplicationFile, NULL },
			{ "guid:", FreeRDP_RemoteApplicationGuid, NULL },
		};
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
					else if (!freerdp_settings_set_string(settings, cur->id, xval))
						rc = COMMAND_LINE_ERROR_MEMORY;

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

	free(ptr);
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

				if (settings->JpegQuality == 0)
					settings->JpegQuality = 75;
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
			const int bval = parse_on_off_option(val);
			if (bval < 0)
				rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			else
			{
				if (option_starts_with("bitmap", val))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, bval > 0))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("glyph", val))
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel,
					                                 bval > 0 ? GLYPH_SUPPORT_FULL
					                                          : GLYPH_SUPPORT_NONE))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("persist", val))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled,
					                               bval > 0))
						rc = COMMAND_LINE_ERROR;
				}
				else if (option_starts_with("offscreen", val))
				{
					if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, bval))
						rc = COMMAND_LINE_ERROR;
				}
			}
		}
	}

	free(ptr);
	return rc;
}

int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings, int argc,
                                                         char** argv, BOOL allowUnknown)
{
	char* user = NULL;
	char* gwUser = NULL;
	char* str;
	int status;
	BOOL ext = FALSE;
	BOOL assist = FALSE;
	DWORD flags = 0;
	BOOL promptForPassword = FALSE;
	BOOL compatibility = FALSE;
	const COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A largs[ARRAYSIZE(global_cmd_args)];
	memcpy(largs, global_cmd_args, sizeof(global_cmd_args));

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

	freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, NULL);
	freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, NULL);
	freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, NULL);

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
			char* p;

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
					size_t length;

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
					if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, arg->Value))
						return COMMAND_LINE_ERROR_MEMORY;
				}
			}
			else /* ipv6 */
			{
				size_t length;
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

					if (!value_to_int(&p2[2], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					settings->ServerPort = (UINT16)val;
				}

				printf("hostname %s port %" PRIu32 "\n", settings->ServerHostname,
				       settings->ServerPort);
			}
		}
		CommandLineSwitchCase(arg, "spn-class")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationServiceClass,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "sspi-module")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_SspiModule, arg->Value))
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

				if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
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
			char* p;
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
				if (option_equals(arg->Value, "force"))
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
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count = 0;
				UINT32* MonitorIds;
				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				if (!ptr.pc)
					return COMMAND_LINE_ERROR_MEMORY;

				if (count > 16)
					count = 16;

				if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, count))
				{
					free(ptr.p);
					return FALSE;
				}

				MonitorIds =
				    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorIds, 0);
				for (i = 0; i < count; i++)
				{
					LONGLONG val;

					if (!value_to_int(ptr.pc[i], &val, 0, UINT16_MAX))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					MonitorIds[i] = (UINT32)val;
				}

				free(ptr.p);
			}
		}
		CommandLineSwitchCase(arg, "t")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_WindowTitle, arg->Value))
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
			int rc = parse_kbd_options(settings, arg);
			if (rc != 0)
				return rc;
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "kbd-remap")
		{
			WLog_WARN(TAG, "/kbd-remap:<key>=<value>,<key2>=<value2> is deprecated, use "
			               "/kbd:remap:<key>=<value>,remap:<key2>=<value2>,... instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_KeyboardRemappingList, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "kbd-lang")
		{
			LONGLONG val;

			WLog_WARN(TAG, "/kbd-lang:<value> is deprecated, use /kbd:lang:<value> instead");
			if (!value_to_int(arg->Value, &val, 1, UINT32_MAX))
			{
				WLog_ERR(TAG, "Could not identify keyboard active language %s", arg->Value);
				WLog_ERR(TAG, "Use /list:kbd-lang to list available layouts");
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}

			settings->KeyboardCodePage = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-type")
		{
			LONGLONG val;

			WLog_WARN(TAG, "/kbd-type:<value> is deprecated, use /kbd:type:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardType = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-unicode")
		{
			WLog_WARN(TAG, "/kbd-unicode is deprecated, use /kbd:unicode[:on|off] instead");
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "kbd-subtype")
		{
			LONGLONG val;

			WLog_WARN(TAG, "/kbd-subtype:<value> is deprecated, use /kbd:subtype:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardSubType = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-fn-key")
		{
			LONGLONG val;

			WLog_WARN(TAG, "/kbd-fn-key:<value> is deprecated, use /kbd:fn-key:<value> instead");
			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->KeyboardFunctionKey = (UINT32)val;
		}
#endif
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
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				char* p;
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
				if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname,
				                                 settings->ServerHostname))
					return COMMAND_LINE_ERROR_MEMORY;
			}

			settings->GatewayEnabled = TRUE;
			settings->GatewayUseSameCredentials = TRUE;
			freerdp_set_gateway_usage_method(settings, TSC_PROXY_MODE_DIRECT);
		}
		CommandLineSwitchCase(arg, "proxy")
		{
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
		}
		CommandLineSwitchCase(arg, "gu")
		{
			if (!(gwUser = _strdup(arg->Value)))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gd")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gp")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->GatewayUseSameCredentials = FALSE;
		}
		CommandLineSwitchCase(arg, "gt")
		{
			if ((arg->Flags & COMMAND_LINE_VALUE_PRESENT) == 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			WINPR_ASSERT(arg->Value);
			if (option_equals(arg->Value, "rpc"))
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, FALSE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else
			{
				char* c = strchr(arg->Value, ',');
				while (c)
				{
					char* next = strchr(c + 1, ',');
					if (next)
						*next = '\0';
					*c++ = '\0';
					if (option_equals(c, "no-websockets"))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets,
						                               FALSE))
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					else if (option_equals(c, "extauth-sspi-ntlm"))
					{
						if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpExtAuthSspiNtlm,
						                               TRUE))
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					else
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					c = next;
				}

				if (option_equals(arg->Value, "http"))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) ||
					    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				else if (option_equals(arg->Value, "auto"))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
					    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
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

			if (option_equals(arg->Value, "none"))
				type = TSC_PROXY_MODE_NONE_DIRECT;
			else if (option_equals(arg->Value, "direct"))
				type = TSC_PROXY_MODE_DIRECT;
			else if (option_equals(arg->Value, "detect"))
				type = TSC_PROXY_MODE_DETECT;
			else if (option_equals(arg->Value, "default"))
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
			int rc = parse_app_options(settings, arg);
			if (rc != 0)
				return rc;
		}
		CommandLineSwitchCase(arg, "load-balance-info")
		{
			if (!copy_value(arg->Value, (char**)&settings->LoadBalanceInfo))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->LoadBalanceInfoLength = (UINT32)strlen((char*)settings->LoadBalanceInfo);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "app-workdir")
		{
			WLog_WARN(
			    TAG,
			    "/app-workdir:<directory> is deprecated, use /app:workdir:<directory> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-name")
		{
			WLog_WARN(TAG, "/app-name:<directory> is deprecated, use /app:name:<name> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationName, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-icon")
		{
			WLog_WARN(TAG, "/app-icon:<filename> is deprecated, use /app:icon:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationIcon, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-cmd")
		{
			WLog_WARN(TAG, "/app-cmd:<command> is deprecated, use /app:cmd:<command> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationCmdLine,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-file")
		{
			WLog_WARN(TAG, "/app-file:<filename> is deprecated, use /app:file:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationFile, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "app-guid")
		{
			WLog_WARN(TAG, "/app-guid:<guid> is deprecated, use /app:guid:<guid> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationGuid, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
#endif
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
		CommandLineSwitchCase(arg, "dump")
		{
			BOOL failed = FALSE;
			size_t count = 0;
			char** args = CommandLineParseCommaSeparatedValues(arg->Value, &count);
			if (!args || (count != 2))
				failed = TRUE;
			else
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_TransportDumpFile, args[1]))
					failed = TRUE;
				else if (option_equals(args[0], "replay"))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDump, FALSE))
						failed = TRUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay,
					                                    TRUE))
						failed = TRUE;
				}
				else if (option_equals(args[0], "record"))
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDump, TRUE))
						failed = TRUE;
					else if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay,
					                                    FALSE))
						failed = TRUE;
				}
				else
				{
					failed = TRUE;
				}
			}
			free(args);
			if (failed)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "disable-output")
		{
			freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, enable);
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
			if (arg->Value == BoolValueTrue || arg->Value == BoolValueFalse)
			{
				settings->RedirectClipboard = (arg->Value == BoolValueTrue);
			}
			else
			{
				int rc = 0;
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count, x;
				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
				for (x = 0; (x < count) && (rc == 0); x++)
				{
					const char* usesel = "use-selection:";

					const char* cur = ptr.pc[x];
					if (option_starts_with(usesel, cur))
					{
						const char* val = &cur[strlen(usesel)];
						if (!copy_value(val, &settings->XSelectionAtom))
							rc = COMMAND_LINE_ERROR_MEMORY;
						settings->RedirectClipboard = TRUE;
					}
					else
						rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				free(ptr.p);

				if (rc)
					return rc;
			}
		}
		CommandLineSwitchCase(arg, "server-name")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_UserSpecifiedServerName, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
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

			if (option_equals(arg->Value, "modem"))
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
			else if ((option_equals(arg->Value, "autodetect")) ||
			         (option_equals(arg->Value, "auto")) || (option_equals(arg->Value, "detect")))
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
			if (!freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, (UINT32)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "aero")
		{
			settings->AllowDesktopComposition = enable;
		}
		CommandLineSwitchCase(arg, "gdi")
		{
			if (option_equals(arg->Value, "sw"))
				settings->SoftwareGdi = TRUE;
			else if (option_equals(arg->Value, "hw"))
				settings->SoftwareGdi = FALSE;
		}
		CommandLineSwitchCase(arg, "gfx")
		{
			int rc = parse_gfx_options(settings, arg);
			if (rc != 0)
				return rc;
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "gfx-thin-client")
		{
			WLog_WARN(TAG, "/gfx-thin-client is deprecated, use /gfx:thin-client[:on|off] instead");
			settings->GfxThinClient = enable;

			if (settings->GfxThinClient)
				settings->GfxSmallCache = TRUE;

			settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-small-cache")
		{
			WLog_WARN(TAG, "/gfx-small-cache is deprecated, use /gfx:small-cache[:on|off] instead");
			settings->GfxSmallCache = enable;

			if (enable)
				settings->SupportGraphicsPipeline = TRUE;
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			WLog_WARN(TAG, "/gfx-progressive is deprecated, use /gfx:progressive[:on|off] instead");
			settings->GfxProgressive = enable;
			settings->GfxThinClient = !enable;

			if (enable)
				settings->SupportGraphicsPipeline = TRUE;
		}
#ifdef WITH_GFX_H264
		CommandLineSwitchCase(arg, "gfx-h264")
		{
			WLog_WARN(TAG, "/gfx-h264 is deprecated, use /gfx:avc420 instead");
			int rc = parse_gfx_options(settings, arg);
			if (rc != 0)
				return rc;
		}
#endif
#endif
		CommandLineSwitchCase(arg, "rfx")
		{
			settings->RemoteFxCodec = enable;
		}
		CommandLineSwitchCase(arg, "rfx-mode")
		{
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (option_equals(arg->Value, "video"))
				settings->RemoteFxCodecMode = 0x00;
			else if (option_equals(arg->Value, "image"))
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
			freerdp_settings_set_bool(settings, FreeRDP_NSCodec, enable);
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

			if (!freerdp_settings_set_string(settings, FreeRDP_PreconnectionBlob, arg->Value))
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
			BOOL RdpSecurity = FALSE;
			BOOL TlsSecurity = FALSE;
			BOOL NlaSecurity = FALSE;
			BOOL ExtSecurity = FALSE;
			size_t count = 0, x;
			char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
			if (count == 0)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			for (x = 0; x < count; x++)
			{
				const char* cur = ptr[x];
				const int bval = parse_on_off_option(cur);
				if (bval < 0)
				{
					free(ptr);
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				if (option_equals("rdp", cur)) /* Standard RDP */
					RdpSecurity = bval > 0;
				else if (option_equals("tls", cur)) /* TLS */
					TlsSecurity = bval > 0;
				else if (option_equals("nla", cur)) /* NLA */
					NlaSecurity = bval > 0;
				else if (option_equals("ext", cur)) /* NLA Extended */
					ExtSecurity = bval > 0;
				else
				{
					WLog_ERR(TAG, "unknown protocol security: %s", arg->Value);
					free(ptr);
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
			}

			free(ptr);
			if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, RdpSecurity))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, RdpSecurity))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TlsSecurity))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, NlaSecurity))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, ExtSecurity))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
		CommandLineSwitchCase(arg, "encryption-methods")
		{
			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				UINT32 i;
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count = 0;
				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				for (i = 0; i < count; i++)
				{
					if (option_equals(ptr.pc[i], "40"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_40BIT;
					else if (option_equals(ptr.pc[i], "56"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_56BIT;
					else if (option_equals(ptr.pc[i], "128"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_128BIT;
					else if (option_equals(ptr.pc[i], "FIPS"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_FIPS;
					else
						WLog_ERR(TAG, "unknown encryption method '%s'", ptr.pc[i]);
				}

				free(ptr.p);
			}
		}
		CommandLineSwitchCase(arg, "from-stdin")
		{
			settings->CredentialsFromStdin = TRUE;

			if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
			{
				if (!arg->Value)
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				promptForPassword = (option_equals(arg->Value, "force"));

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
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "sec-rdp")
		{
			WLog_WARN(TAG, "/sec-rdp is deprecated, use /sec:rdp[:on|off] instead");
			settings->RdpSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			WLog_WARN(TAG, "/sec-tls is deprecated, use /sec:tls[:on|off] instead");
			settings->TlsSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			WLog_WARN(TAG, "/sec-nla is deprecated, use /sec:nla[:on|off] instead");
			settings->NlaSecurity = enable;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			WLog_WARN(TAG, "/sec-ext is deprecated, use /sec:ext[:on|off] instead");
			settings->ExtSecurity = enable;
		}
#endif
		CommandLineSwitchCase(arg, "tls")
		{
			size_t count, x;
			char** ptr = CommandLineParseCommaSeparatedValues(arg->Value, &count);
			for (x = 0; x < count; x++)
			{
				COMMAND_LINE_ARGUMENT_A larg = *arg;
				larg.Value = ptr[x];

				int rc = parse_tls_options(settings, &larg);
				if (rc != 0)
				{
					free(ptr);
					return rc;
				}
			}
			free(ptr);
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "tls-ciphers")
		{
			WLog_WARN(TAG, "/tls-ciphers:<cipher list> is deprecated, use "
			               "/tls:ciphers:<cipher list> instead");
			int rc = parse_tls_options(settings, arg);
			if (rc != 0)
				return rc;
		}
		CommandLineSwitchCase(arg, "tls-seclevel")
		{
			WLog_WARN(TAG,
			          "/tls-seclevel:<level> is deprecated, use /tls:sec-level:<level> instead");
			int rc = parse_tls_options(settings, arg);
			if (rc != 0)
				return rc;
		}
		CommandLineSwitchCase(arg, "tls-secrets-file")
		{
			WLog_WARN(TAG, "/tls-secrets-file:<filename> is deprecated, use "
			               "/tls:secrets-file:<filename> instead");
			int rc = parse_tls_options(settings, arg);
			if (rc != 0)
				return rc;
		}
		CommandLineSwitchCase(arg, "enforce-tlsv1_2")
		{
			WLog_WARN(TAG, "/enforce-tlsv1_2 is deprecated, use /tls:enforce:1.2 instead");
			int rc = parse_tls_options(settings, arg);
			if (rc != 0)
				return rc;
		}
#endif
		CommandLineSwitchCase(arg, "cert")
		{
			int rc = 0;
			union
			{
				char** p;
				const char** pc;
			} ptr;
			size_t count, x;
			ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
			for (x = 0; (x < count) && (rc == 0); x++)
			{
				const char deny[] = "deny";
				const char ignore[] = "ignore";
				const char tofu[] = "tofu";
				const char name[] = "name:";
				const char fingerprints[] = "fingerprint:";

				const char* cur = ptr.pc[x];
				if (option_equals(deny, cur))
					settings->AutoDenyCertificate = TRUE;
				else if (option_equals(ignore, cur))
					settings->IgnoreCertificate = TRUE;
				else if (option_equals(tofu, cur))
					settings->AutoAcceptCertificate = TRUE;
				else if (option_starts_with(name, cur))
				{
					const char* val = &cur[strnlen(name, sizeof(name))];
					if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, val))
						rc = COMMAND_LINE_ERROR_MEMORY;
				}
				else if (option_starts_with(fingerprints, cur))
				{
					const char* val = &cur[strnlen(fingerprints, sizeof(fingerprints))];
					if (!append_value(val, &settings->CertificateAcceptedFingerprints))
						rc = COMMAND_LINE_ERROR_MEMORY;
				}
				else
					rc = COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			free(ptr.p);

			if (rc)
				return rc;
		}

#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "cert-name")
		{
			WLog_WARN(TAG, "/cert-name is deprecated, use /cert:name instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "cert-ignore")
		{
			WLog_WARN(TAG, "/cert-ignore is deprecated, use /cert:ignore instead");
			settings->IgnoreCertificate = enable;
		}
		CommandLineSwitchCase(arg, "cert-tofu")
		{
			WLog_WARN(TAG, "/cert-tofu is deprecated, use /cert:tofu instead");
			settings->AutoAcceptCertificate = enable;
		}
		CommandLineSwitchCase(arg, "cert-deny")
		{
			WLog_WARN(TAG, "/cert-deny is deprecated, use /cert:deny instead");
			settings->AutoDenyCertificate = enable;
		}
#endif
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
		CommandLineSwitchCase(arg, "grab-mouse")
		{
			settings->GrabMouse = enable;
		}
		CommandLineSwitchCase(arg, "mouse-relative")
		{
			settings->MouseUseRelativeMove = enable;
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

						const int bval = parse_on_off_option(cur);
						if (bval > 0)
							Floatbar |= 0x02u;
						else if (bval == 0)
							Floatbar &= ~0x02u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
				if (!freerdp_settings_set_uint32(settings, FreeRDP_Floatbar, Floatbar))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
		CommandLineSwitchCase(arg, "cache")
		{
			int rc = parse_cache_options(settings, arg);
			if (rc != 0)
				return rc;
		}
#if defined(WITH_FREERDP_DEPRECATED_COMMANDLINE)
		CommandLineSwitchCase(arg, "bitmap-cache")
		{
			WLog_WARN(TAG, "/bitmap-cache is deprecated, use /cache:bitmap[:on|off] instead");
			settings->BitmapCacheEnabled = enable;
		}
		CommandLineSwitchCase(arg, "persist-cache")
		{
			WLog_WARN(TAG, "/persist-cache is deprecated, use /cache:persist[:on|off] instead");
			settings->BitmapCachePersistEnabled = enable;
		}
		CommandLineSwitchCase(arg, "persist-cache-file")
		{
			WLog_WARN(TAG, "/persist-cache-file:<filename> is deprecated, use "
			               "/cache:persist-file:<filename> instead");
			if (!freerdp_settings_set_string(settings, FreeRDP_BitmapCachePersistFile, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->BitmapCachePersistEnabled = TRUE;
		}
		CommandLineSwitchCase(arg, "offscreen-cache")
		{
			WLog_WARN(TAG, "/bitmap-cache is deprecated, use /cache:bitmap[:on|off] instead");
			settings->OffscreenSupportLevel = (UINT32)enable;
		}
		CommandLineSwitchCase(arg, "glyph-cache")
		{
			WLog_WARN(TAG, "/glyph-cache is deprecated, use /cache:glyph[:on|off] instead");
			settings->GlyphSupportLevel = arg->Value ? GLYPH_SUPPORT_FULL : GLYPH_SUPPORT_NONE;
		}
		CommandLineSwitchCase(arg, "codec-cache")
		{
			WLog_WARN(TAG,
			          "/codec-cache:<option> is deprecated, use /cache:codec:<option> instead");
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			settings->BitmapCacheV3Enabled = TRUE;

			if (option_equals(arg->Value, "rfx"))
			{
				settings->RemoteFxCodec = TRUE;
			}
			else if (option_equals(arg->Value, "nsc"))
			{
				freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE);
			}

#if defined(WITH_JPEG)
			else if (option_equals(arg->Value, "jpeg"))
			{
				settings->JpegCodec = TRUE;

				if (settings->JpegQuality == 0)
					settings->JpegQuality = 75;
			}

#endif
		}
#endif
		CommandLineSwitchCase(arg, "max-fast-path-size")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, UINT32_MAX))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->MultifragMaxRequestSize = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "auto-request-control")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceRequestControl,
			                               enable))
				return COMMAND_LINE_ERROR;
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
			if (!freerdp_settings_set_string(settings, FreeRDP_WmClass, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "play-rfx")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_PlayRemoteFxFile, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->PlayRemoteFx = TRUE;
		}
		CommandLineSwitchCase(arg, "auth-only")
		{
			settings->AuthenticationOnly = enable;
		}
		CommandLineSwitchCase(arg, "auth-pkg-list")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_AuthenticationPackageList,
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
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
			size_t length;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			crypto_base64_decode((const char*)(arg->Value), strlen(arg->Value), &base64, &length);

			if ((base64 != NULL) && (length == sizeof(ARC_SC_PRIVATE_PACKET)))
			{
				memcpy(settings->ServerAutoReconnectCookie, base64, length);
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
			if (!freerdp_settings_set_string(settings, FreeRDP_ActionScript, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, RDP2TCP_DVC_CHANNEL_NAME)
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RDP2TCPArgs, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "fipsmode")
		{
			settings->FIPSMode = enable;
		}
		CommandLineSwitchCase(arg, "smartcard-logon")
		{
			size_t count;
			union
			{
				char** p;
				const char** pc;
			} ptr;

			settings->SmartcardLogon = TRUE;

			ptr.p = CommandLineParseCommaSeparatedValuesEx("smartcard-logon", arg->Value, &count);
			if (ptr.pc)
			{
				size_t x;
				const CmdLineSubOptions opts[] = {
					{ "cert:", FreeRDP_SmartcardCertificate, CMDLINE_SUBOPTION_FILE,
					  setSmartcardEmulation },
					{ "key:", FreeRDP_SmartcardPrivateKey, CMDLINE_SUBOPTION_FILE,
					  setSmartcardEmulation },
					{ "pin:", FreeRDP_Password, CMDLINE_SUBOPTION_STRING, NULL },
					{ "csp:", FreeRDP_CspName, CMDLINE_SUBOPTION_STRING, NULL },
					{ "reader:", FreeRDP_ReaderName, CMDLINE_SUBOPTION_STRING, NULL },
					{ "card:", FreeRDP_CardName, CMDLINE_SUBOPTION_STRING, NULL },
					{ "container:", FreeRDP_ContainerName, CMDLINE_SUBOPTION_STRING, NULL }
				};

				for (x = 1; x < count; x++)
				{
					const char* cur = ptr.pc[x];
					if (!parseSubOptions(settings, opts, ARRAYSIZE(opts), cur))
					{
						free(ptr.p);
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
				}
			}
			free(ptr.p);
		}
		CommandLineSwitchCase(arg, "tune")
		{
			size_t x, count;
			union
			{
				char** p;
				const char** pc;
			} ptr;
			ptr.p = CommandLineParseCommaSeparatedValuesEx("tune", arg->Value, &count);
			if (!ptr.pc)
				return COMMAND_LINE_ERROR;
			for (x = 1; x < count; x++)
			{
				const char* cur = ptr.pc[x];
				char* sep = strchr(cur, ':');
				if (!sep)
				{
					free(ptr.p);
					return COMMAND_LINE_ERROR;
				}
				*sep++ = '\0';
				if (!freerdp_settings_set_value_for_name(settings, cur, sep))
				{
					free(ptr.p);
					return COMMAND_LINE_ERROR;
				}
			}

			free(ptr.p);
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

	if (settings->RemoteFxCodec || freerdp_settings_get_bool(settings, FreeRDP_NSCodec) ||
	    settings->SupportGraphicsPipeline)
	{
		settings->FastPathOutput = TRUE;
		settings->FrameMarkerCommandEnabled = TRUE;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
			return COMMAND_LINE_ERROR;
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

	arg = CommandLineFindArgumentA(largs, "smartcard-logon");
	if (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT)
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
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
	size_t settingId;
	const char* channelName;
	void* args;
} ChannelToLoad;

BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	ChannelToLoad dynChannels[] = {
#if defined(CHANNEL_AINPUT_CLIENT)
		{ 0, AINPUT_CHANNEL_NAME, NULL }, /* always loaded */
#endif
		{ FreeRDP_AudioCapture, "audin", NULL },
		{ FreeRDP_AudioPlayback, RDPSND_CHANNEL_NAME, NULL },
#ifdef CHANNEL_RDPEI_CLIENT
		{ FreeRDP_MultiTouchInput, "rdpei", NULL },
#endif
		{ FreeRDP_SupportGraphicsPipeline, "rdpgfx", NULL },
		{ FreeRDP_SupportEchoChannel, "echo", NULL },
		{ FreeRDP_SupportSSHAgentChannel, "sshagent", NULL },
		{ FreeRDP_SupportDisplayControl, DISP_CHANNEL_NAME, NULL },
		{ FreeRDP_SupportGeometryTracking, "geometry", NULL },
		{ FreeRDP_SupportSSHAgentChannel, "sshagent", NULL },
		{ FreeRDP_SupportSSHAgentChannel, "sshagent", NULL },
		{ FreeRDP_SupportVideoOptimized, "video", NULL },
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
	size_t i;

	/**
	 * Step 1: first load dynamic channels according to the settings
	 */
	for (i = 0; i < ARRAYSIZE(dynChannels); i++)
	{
		if ((dynChannels[i].settingId == 0) ||
		    freerdp_settings_get_bool(settings, dynChannels[i].settingId))
		{
			const char* p[] = { dynChannels[i].channelName };

			if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
				return FALSE;
		}
	}

	/**
	 * step 2: do various adjustements in the settings, to handle channels and settings dependencies
	 */
	if ((freerdp_static_channel_collection_find(settings, RDPSND_CHANNEL_NAME)) ||
	    (freerdp_dynamic_channel_collection_find(settings, RDPSND_CHANNEL_NAME))
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

	if (freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect) ||
	    settings->SupportHeartbeatPdu || settings->SupportMultitransport)
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
			WLog_ERR(TAG, "DrivesToRedirect contains invalid data: '%s'",
			         settings->DrivesToRedirect);
			free(value);
			return FALSE;
		}

		while (tok)
		{
			/* Syntax: Comma seperated list of the following entries:
			 * '*'              ... Redirect all drives, including hotplug
			 * 'DynamicDrives'  ... hotplug
			 * '%'              ... user home directory
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

		settings->DeviceRedirection = TRUE;
	}
	else if (settings->RedirectDrives)
	{
		if (!freerdp_device_collection_find(settings, "drive"))
		{
			const char* params[] = { "drive", "media", "*" };

			if (!freerdp_client_add_device_channel(settings, ARRAYSIZE(params), params))
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
			const char* params[] = { "drive", "home", "%" };

			if (!freerdp_client_add_device_channel(settings, ARRAYSIZE(params), params))
				return FALSE;
		}
	}

	if (settings->DeviceRedirection)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RDPDR_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;

		if (!freerdp_static_channel_collection_find(settings, RDPSND_CHANNEL_NAME) &&
		    !freerdp_dynamic_channel_collection_find(settings, RDPSND_CHANNEL_NAME))
		{
			const char* params[] = { RDPSND_CHANNEL_NAME, "sys:fake" };

			if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(params), params))
				return FALSE;

			if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(params), params))
				return FALSE;
		}
	}

	if (settings->RedirectSmartCards)
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

	if (settings->RedirectPrinters)
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

	/* step 3: schedule some static channels to load depending on the settings */
	for (i = 0; i < ARRAYSIZE(staticChannels); i++)
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
				const char* p[] = { staticChannels[i].channelName };
				if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(p), p))
					return FALSE;
			}
		}
	}

	if (settings->RDP2TCPArgs)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RDP2TCP_DVC_CHANNEL_NAME,
		                                              settings->RDP2TCPArgs))
			return FALSE;
	}

	/* step 4: do the static channels loading and init */
	for (i = 0; i < settings->StaticChannelCount; i++)
	{
		ADDIN_ARGV* _args = settings->StaticChannelArray[i];

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
