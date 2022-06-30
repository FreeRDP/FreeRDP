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
		if (_strnicmp(path, "%", 2) == 0)
			name = "home";
		else if (_strnicmp(path, "*", 2) == 0)
			name = "hotplug-all";
		else if (_strnicmp(path, "DynamicDrives", 2) == 0)
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
	printf("RDP scancodes and their name for use with /kbd-remap\n");

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

BOOL freerdp_client_add_device_channel(rdpSettings* settings, size_t count, const char** params)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(params);
	WINPR_ASSERT(count > 0);

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
	else if (strcmp(params[0], "smartcard") == 0)
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
	else if (strcmp(params[0], "serial") == 0)
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
	else if (strcmp(params[0], "parallel") == 0)
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
		return FALSE;
	rs = _fseeki64(fp, 0, SEEK_END);
	if (rs < 0)
		goto fail;
	s = _ftelli64(fp);
	if (s < 0)
		goto fail;
	rs = _fseeki64(fp, 0, SEEK_SET);
	if (rs < 0)
		goto fail;

	if (!freerdp_settings_set_string_len(settings, id, NULL, s + 1ull))
		goto fail;

	ptr = freerdp_settings_get_string_writable(settings, id);
	fr = fread(ptr, (size_t)s, 1, fp);
	if (fr != 1)
		goto fail;
	rc = TRUE;
fail:
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
		size_t optlen = strlen(opt->optname);

		if (strncmp(opt->optname, arg, optlen) == 0)
		{
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
		settings->NetworkAutoDetect = FALSE;
	}
	else if (type == CONNECTION_TYPE_BROADBAND_LOW)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = FALSE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
		settings->NetworkAutoDetect = FALSE;
	}
	else if (type == CONNECTION_TYPE_SATELLITE)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
		settings->NetworkAutoDetect = FALSE;
	}
	else if (type == CONNECTION_TYPE_BROADBAND_HIGH)
	{
		settings->DisableWallpaper = TRUE;
		settings->AllowFontSmoothing = FALSE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = TRUE;
		settings->DisableMenuAnims = TRUE;
		settings->DisableThemes = FALSE;
		settings->NetworkAutoDetect = FALSE;
	}
	else if (type == CONNECTION_TYPE_WAN)
	{
		settings->DisableWallpaper = FALSE;
		settings->AllowFontSmoothing = TRUE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = FALSE;
		settings->DisableMenuAnims = FALSE;
		settings->DisableThemes = FALSE;
		settings->NetworkAutoDetect = FALSE;
	}
	else if (type == CONNECTION_TYPE_LAN)
	{
		settings->DisableWallpaper = FALSE;
		settings->AllowFontSmoothing = TRUE;
		settings->AllowDesktopComposition = TRUE;
		settings->DisableFullWindowDrag = FALSE;
		settings->DisableMenuAnims = FALSE;
		settings->DisableThemes = FALSE;
		settings->NetworkAutoDetect = FALSE;
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

	WLog_DBG(TAG, "windows: %d/%d posix: %d/%d", windows_cli_status, windows_cli_count,
	         posix_cli_status, posix_cli_count);
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

		arg = CommandLineFindArgumentA(largs, "smartcard-list");
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			freerdp_smartcard_list(settings);
		}

		arg = CommandLineFindArgumentA(largs, "kbd-scancode-list");

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			freerdp_client_print_scancodes();
			goto out;
		}
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

static BOOL ends_with(const char* str, const char* ext)
{
	const size_t strLen = strlen(str);
	const size_t extLen = strlen(ext);

	if (strLen < extLen)
		return FALSE;

	return _strnicmp(&str[strLen - extLen], ext, extLen) == 0;
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
		CommandLineSwitchCase(arg, "monitor-list")
		{
			settings->ListMonitors = enable;
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

			settings->KeyboardLayout = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "kbd-remap")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_KeyboardRemappingList, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;
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
		CommandLineSwitchCase(arg, "kbd-unicode")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
			if (_stricmp(arg->Value, "rpc") == 0)
			{
				if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, FALSE) ||
				    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, FALSE))
					return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
			}
			else
			{
				char* c = strchr(arg->Value, ',');
				if (c)
				{
					*c++ = '\0';
					if (_stricmp(c, "no-websockets") != 0)
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

					if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets,
					                               FALSE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}

				if (_stricmp(arg->Value, "http") == 0)
				{
					if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, FALSE) ||
					    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE))
						return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
				}
				else if (_stricmp(arg->Value, "auto") == 0)
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
			                                 arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->RemoteApplicationMode = TRUE;
			settings->RemoteAppLanguageBarSupported = TRUE;
			settings->Workarea = TRUE;
			settings->DisableWallpaper = TRUE;
			settings->DisableFullWindowDrag = TRUE;
		}
		CommandLineSwitchCase(arg, "app-workdir")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_RemoteApplicationWorkingDir,
			                                 arg->Value))
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
					const char usesel[14] = "use-selection:";

					const char* cur = ptr.pc[x];
					if (_strnicmp(usesel, cur, sizeof(usesel)) == 0)
					{
						const char* val = &cur[sizeof(usesel)];
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
			if (!freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, (UINT32)val))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
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
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count, x;

				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
				if (!ptr.pc || (count == 0))
					rc = COMMAND_LINE_ERROR;
				else
				{
					for (x = 0; x < count; x++)
					{
						const char* val = ptr.pc[x];
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
				free(ptr.p);
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
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count, x;

				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);
				if (!ptr.pc || (count == 0))
					rc = COMMAND_LINE_ERROR;
				else
				{
					for (x = 0; x < count; x++)
					{
						const char* val = ptr.pc[x];

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
				free(ptr.p);
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
				union
				{
					char** p;
					const char** pc;
				} ptr;
				size_t count = 0;
				ptr.p = CommandLineParseCommaSeparatedValues(arg->Value, &count);

				for (i = 0; i < count; i++)
				{
					if (!strcmp(ptr.pc[i], "40"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_40BIT;
					else if (!strcmp(ptr.pc[i], "56"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_56BIT;
					else if (!strcmp(ptr.pc[i], "128"))
						settings->EncryptionMethods |= ENCRYPTION_METHOD_128BIT;
					else if (!strcmp(ptr.pc[i], "FIPS"))
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
			const char* ciphers = NULL;
			if (!arg->Value)
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			if (strcmp(arg->Value, "netmon") == 0)
			{
				ciphers = "ALL:!ECDH";
			}
			else if (strcmp(arg->Value, "ma") == 0)
			{
				ciphers = "AES128-SHA";
			}
			else
			{
				ciphers = arg->Value;
			}

			if (!freerdp_settings_set_string(settings, FreeRDP_AllowedTlsCiphers, ciphers))
				return COMMAND_LINE_ERROR_MEMORY;
		}
		CommandLineSwitchCase(arg, "tls-seclevel")
		{
			LONGLONG val;

			if (!value_to_int(arg->Value, &val, 0, 5))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

			settings->TlsSecLevel = (UINT32)val;
		}
		CommandLineSwitchCase(arg, "enforce-tlsv1_2")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_EnforceTLSv1_2, enable))
				return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
		}
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
				const char name[5] = "name:";
				const char fingerprints[12] = "fingerprint:";

				const char* cur = ptr.pc[x];
				if (_strnicmp(deny, cur, sizeof(deny)) == 0)
					settings->AutoDenyCertificate = TRUE;
				else if (_strnicmp(ignore, cur, sizeof(ignore)) == 0)
					settings->IgnoreCertificate = TRUE;
				else if (_strnicmp(tofu, cur, 4) == 0)
					settings->AutoAcceptCertificate = TRUE;
				else if (_strnicmp(name, cur, sizeof(name)) == 0)
				{
					const char* val = &cur[sizeof(name)];
					if (!freerdp_settings_set_string(settings, FreeRDP_CertificateName, val))
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
			free(ptr.p);

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
					if (_strnicmp(cur, "sticky:", 7) == 0)
					{
						const char* val = cur + 7;
						Floatbar &= ~0x02u;

						if (_strnicmp(val, "on", 3) == 0)
							Floatbar |= 0x02u;
						else if (_strnicmp(val, "off", 4) == 0)
							Floatbar &= ~0x02u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* default:[visible|hidden] */
					else if (_strnicmp(cur, "default:", 8) == 0)
					{
						const char* val = cur + 8;
						Floatbar &= ~0x04u;

						if (_strnicmp(val, "visible", 8) == 0)
							Floatbar |= 0x04u;
						else if (_strnicmp(val, "hidden", 7) == 0)
							Floatbar &= ~0x04u;
						else
							return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
					}
					/* show:[always|fullscreen|window] */
					else if (_strnicmp(cur, "show:", 5) == 0)
					{
						const char* val = cur + 5;
						Floatbar &= ~0x30u;

						if (_strnicmp(val, "always", 7) == 0)
							Floatbar |= 0x30u;
						else if (_strnicmp(val, "fullscreen", 11) == 0)
							Floatbar |= 0x10u;
						else if (_strnicmp(val, "window", 7) == 0)
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
		CommandLineSwitchCase(arg, "bitmap-cache")
		{
			settings->BitmapCacheEnabled = enable;
		}
		CommandLineSwitchCase(arg, "persist-cache")
		{
			settings->BitmapCachePersistEnabled = enable;
		}
		CommandLineSwitchCase(arg, "persist-cache-file")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_BitmapCachePersistFile, arg->Value))
				return COMMAND_LINE_ERROR_MEMORY;

			settings->BitmapCachePersistEnabled = TRUE;
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
				freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE);
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

BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings)
{
	UINT32 index;

	/* Always load FreeRDP advanced input dynamic channel */
#if defined(CHANNEL_AINPUT_CLIENT)
	{
		const char* p[] = { AINPUT_CHANNEL_NAME };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}
#endif

	if (settings->AudioPlayback)
	{
		const char* p[] = { RDPSND_CHANNEL_NAME };

		if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	/* for audio playback also load the dynamic sound channel */
	if (settings->AudioPlayback)
	{
		const char* p[] = { RDPSND_CHANNEL_NAME };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->AudioCapture)
	{
		const char* p[] = { "audin" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

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

	if (settings->RedirectClipboard)
	{
		const char* params[] = { CLIPRDR_SVC_CHANNEL_NAME };

		if (!freerdp_client_add_static_channel(settings, ARRAYSIZE(params), params))
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
		if (!freerdp_client_load_static_channel_addin(channels, settings, ENCOMSP_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;
	}

	if (settings->RemdeskVirtualChannel)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, REMDESK_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;
	}

	if (settings->RDP2TCPArgs)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RDP2TCP_DVC_CHANNEL_NAME,
		                                              settings->RDP2TCPArgs))
			return FALSE;
	}

	for (index = 0; index < settings->StaticChannelCount; index++)
	{
		ADDIN_ARGV* _args = settings->StaticChannelArray[index];

		if (!freerdp_client_load_static_channel_addin(channels, settings, _args->argv[0], _args))
			return FALSE;
	}

	if (settings->RemoteApplicationMode)
	{
		if (!freerdp_client_load_static_channel_addin(channels, settings, RAIL_SVC_CHANNEL_NAME,
		                                              settings))
			return FALSE;
	}

	if (settings->MultiTouchInput)
	{
		const char* p[] = { "rdpei" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->SupportGraphicsPipeline)
	{
		const char* p[] = { "rdpgfx" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->SupportEchoChannel)
	{
		const char* p[] = { "echo" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->SupportSSHAgentChannel)
	{
		const char* p[] = { "sshagent" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (settings->SupportDisplayControl)
	{
		const char* p[] = { DISP_CHANNEL_NAME };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportGeometryTracking))
	{
		const char* p[] = { "geometry" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
			return FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportVideoOptimized))
	{
		const char* p[] = { "video" };

		if (!freerdp_client_add_dynamic_channel(settings, ARRAYSIZE(p), p))
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
