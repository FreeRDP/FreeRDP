/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <stdio.h>
#include <string.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/collections.h>
#include <winpr/cmdline.h>

#include "pf_server.h"
#include <freerdp/server/proxy/proxy_config.h>

#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_log.h>

#include <freerdp/channels/cliprdr.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/channels/audin.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/disp.h>
#include <freerdp/channels/rail.h>
#include <freerdp/channels/rdpei.h>
#include <freerdp/channels/tsmf.h>
#include <freerdp/channels/video.h>
#include <freerdp/channels/rdpecam.h>

#include "pf_utils.h"

#define TAG PROXY_TAG("config")

#define CONFIG_PRINT_SECTION(section) WLog_INFO(TAG, "\t%s:", section)
#define CONFIG_PRINT_STR(config, key) WLog_INFO(TAG, "\t\t%s: %s", #key, config->key)
#define CONFIG_PRINT_STR_CONTENT(config, key) \
	WLog_INFO(TAG, "\t\t%s: %s", #key, config->key ? "set" : NULL)
#define CONFIG_PRINT_BOOL(config, key) \
	WLog_INFO(TAG, "\t\t%s: %s", #key, config->key ? "TRUE" : "FALSE")
#define CONFIG_PRINT_UINT16(config, key) WLog_INFO(TAG, "\t\t%s: %" PRIu16 "", #key, config->key)
#define CONFIG_PRINT_UINT32(config, key) WLog_INFO(TAG, "\t\t%s: %" PRIu32 "", #key, config->key)

static char** pf_config_parse_comma_separated_list(const char* list, size_t* count)
{
	if (!list || !count)
		return NULL;

	if (strlen(list) == 0)
	{
		*count = 0;
		return NULL;
	}

	return CommandLineParseCommaSeparatedValues(list, count);
}

static BOOL pf_config_get_uint16(wIniFile* ini, const char* section, const char* key,
                                 UINT16* result, BOOL required)
{
	int val;
	const char* strval;

	WINPR_ASSERT(result);

	strval = IniFile_GetKeyValueString(ini, section, key);
	if (!strval && required)
	{
		WLog_ERR(TAG, "[%s]: key '%s.%s' does not exist.", __FUNCTION__, section, key);
		return FALSE;
	}
	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val <= 0) || (val > UINT16_MAX))
	{
		WLog_ERR(TAG, "[%s]: invalid value %d for key '%s.%s'.", __FUNCTION__, val, section, key);
		return FALSE;
	}

	*result = (UINT16)val;
	return TRUE;
}

static BOOL pf_config_get_uint32(wIniFile* ini, const char* section, const char* key,
                                 UINT32* result, BOOL required)
{
	int val;
	const char* strval;

	WINPR_ASSERT(result);

	strval = IniFile_GetKeyValueString(ini, section, key);
	if (!strval && required)
	{
		WLog_ERR(TAG, "[%s]: key '%s.%s' does not exist.", __FUNCTION__, section, key);
		return FALSE;
	}

	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val < 0) || (val > INT32_MAX))
	{
		WLog_ERR(TAG, "[%s]: invalid value %d for key '%s.%s'.", __FUNCTION__, val, section, key);
		return FALSE;
	}

	*result = (UINT32)val;
	return TRUE;
}

static BOOL pf_config_get_bool(wIniFile* ini, const char* section, const char* key, BOOL fallback)
{
	int num_value;
	const char* str_value;

	str_value = IniFile_GetKeyValueString(ini, section, key);
	if (!str_value)
	{
		WLog_WARN(TAG, "[%s]: key '%s.%s' not found, value defaults to %s.", __FUNCTION__, section,
		          key, fallback ? "true" : "false");
		return fallback;
	}

	if (_stricmp(str_value, "TRUE") == 0)
		return TRUE;
	if (_stricmp(str_value, "FALSE") == 0)
		return FALSE;

	num_value = IniFile_GetKeyValueInt(ini, section, key);

	if (num_value != 0)
		return TRUE;

	return FALSE;
}

static const char* pf_config_get_str(wIniFile* ini, const char* section, const char* key,
                                     BOOL required)
{
	const char* value;

	value = IniFile_GetKeyValueString(ini, section, key);

	if (!value)
	{
		if (required)
			WLog_ERR(TAG, "[%s]: key '%s.%s' not found.", __FUNCTION__, section, key);
		return NULL;
	}

	return value;
}

static BOOL pf_config_load_server(wIniFile* ini, proxyConfig* config)
{
	const char* host;

	WINPR_ASSERT(config);
	host = pf_config_get_str(ini, "Server", "Host", FALSE);

	if (!host)
		return TRUE;

	config->Host = _strdup(host);

	if (!config->Host)
		return FALSE;

	if (!pf_config_get_uint16(ini, "Server", "Port", &config->Port, TRUE))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_target(wIniFile* ini, proxyConfig* config)
{
	const char* target_value;

	WINPR_ASSERT(config);
	config->FixedTarget = pf_config_get_bool(ini, "Target", "FixedTarget", FALSE);

	if (!pf_config_get_uint16(ini, "Target", "Port", &config->TargetPort, config->FixedTarget))
		return FALSE;

	if (config->FixedTarget)
	{
		target_value = pf_config_get_str(ini, "Target", "Host", TRUE);
		if (!target_value)
			return FALSE;

		config->TargetHost = _strdup(target_value);
		if (!config->TargetHost)
			return FALSE;
	}

	target_value = pf_config_get_str(ini, "Target", "User", FALSE);
	if (target_value)
	{
		config->TargetUser = _strdup(target_value);
		if (!config->TargetUser)
			return FALSE;
	}

	target_value = pf_config_get_str(ini, "Target", "Password", FALSE);
	if (target_value)
	{
		config->TargetPassword = _strdup(target_value);
		if (!config->TargetPassword)
			return FALSE;
	}

	target_value = pf_config_get_str(ini, "Target", "Domain", FALSE);
	if (target_value)
	{
		config->TargetDomain = _strdup(target_value);
		if (!config->TargetDomain)
			return FALSE;
	}

	return TRUE;
}

static BOOL pf_config_load_channels(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->GFX = pf_config_get_bool(ini, "Channels", "GFX", TRUE);
	config->DisplayControl = pf_config_get_bool(ini, "Channels", "DisplayControl", TRUE);
	config->Clipboard = pf_config_get_bool(ini, "Channels", "Clipboard", FALSE);
	config->AudioOutput = pf_config_get_bool(ini, "Channels", "AudioOutput", TRUE);
	config->AudioInput = pf_config_get_bool(ini, "Channels", "AudioInput", TRUE);
	config->DeviceRedirection = pf_config_get_bool(ini, "Channels", "DeviceRedirection", TRUE);
	config->VideoRedirection = pf_config_get_bool(ini, "Channels", "VideoRedirection", TRUE);
	config->CameraRedirection = pf_config_get_bool(ini, "Channels", "CameraRedirection", TRUE);
	config->RemoteApp = pf_config_get_bool(ini, "Channels", "RemoteApp", FALSE);
	config->PassthroughIsBlacklist =
	    pf_config_get_bool(ini, "Channels", "PassthroughIsBlacklist", FALSE);
	config->Passthrough = pf_config_parse_comma_separated_list(
	    pf_config_get_str(ini, "Channels", "Passthrough", FALSE), &config->PassthroughCount);
	config->Intercept = pf_config_parse_comma_separated_list(
	    pf_config_get_str(ini, "Channels", "Intercept", FALSE), &config->InterceptCount);

	return TRUE;
}

static BOOL pf_config_load_input(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->Keyboard = pf_config_get_bool(ini, "Input", "Keyboard", TRUE);
	config->Mouse = pf_config_get_bool(ini, "Input", "Mouse", TRUE);
	config->Multitouch = pf_config_get_bool(ini, "Input", "Multitouch", TRUE);
	return TRUE;
}

static BOOL pf_config_load_security(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->ServerTlsSecurity = pf_config_get_bool(ini, "Security", "ServerTlsSecurity", TRUE);
	config->ServerNlaSecurity = pf_config_get_bool(ini, "Security", "ServerNlaSecurity", FALSE);
	config->ServerRdpSecurity = pf_config_get_bool(ini, "Security", "ServerRdpSecurity", TRUE);

	config->ClientTlsSecurity = pf_config_get_bool(ini, "Security", "ClientTlsSecurity", TRUE);
	config->ClientNlaSecurity = pf_config_get_bool(ini, "Security", "ClientNlaSecurity", TRUE);
	config->ClientRdpSecurity = pf_config_get_bool(ini, "Security", "ClientRdpSecurity", TRUE);
	config->ClientAllowFallbackToTls =
	    pf_config_get_bool(ini, "Security", "ClientAllowFallbackToTls", TRUE);
	return TRUE;
}

static BOOL pf_config_load_clipboard(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->TextOnly = pf_config_get_bool(ini, "Clipboard", "TextOnly", FALSE);

	if (!pf_config_get_uint32(ini, "Clipboard", "MaxTextLength", &config->MaxTextLength, FALSE))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_modules(wIniFile* ini, proxyConfig* config)
{
	const char* modules_to_load;
	const char* required_modules;

	modules_to_load = pf_config_get_str(ini, "Plugins", "Modules", FALSE);
	required_modules = pf_config_get_str(ini, "Plugins", "Required", FALSE);

	WINPR_ASSERT(config);
	config->Modules = pf_config_parse_comma_separated_list(modules_to_load, &config->ModulesCount);

	config->RequiredPlugins =
	    pf_config_parse_comma_separated_list(required_modules, &config->RequiredPluginsCount);
	return TRUE;
}

static BOOL pf_config_load_gfx_settings(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->DecodeGFX = pf_config_get_bool(ini, "GFXSettings", "DecodeGFX", FALSE);
	return TRUE;
}

static BOOL pf_config_load_certificates(wIniFile* ini, proxyConfig* config)
{
	const char* tmp1;
	const char* tmp2;

	WINPR_ASSERT(ini);
	WINPR_ASSERT(config);

	tmp1 = pf_config_get_str(ini, "Certificates", "CertificateFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/CertificateFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->CertificateFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "CertificateContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/CertificateContent has invalid empty value");
			return FALSE;
		}
		config->CertificateContent = _strdup(tmp2);
	}
	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/CertificateFile and Certificates/CertificateContent are "
		              "mutually exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/CertificateFile or Certificates/CertificateContent are "
		              "required settings");
		return FALSE;
	}

	tmp1 = pf_config_get_str(ini, "Certificates", "PrivateKeyFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/PrivateKeyFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->PrivateKeyFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "PrivateKeyContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/PrivateKeyContent has invalid empty value");
			return FALSE;
		}
		config->PrivateKeyContent = _strdup(tmp2);
	}

	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/PrivateKeyFile and Certificates/PrivateKeyContent are "
		              "mutually exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/PrivateKeyFile or Certificates/PrivateKeyContent are "
		              "are required settings");
		return FALSE;
	}

	tmp1 = pf_config_get_str(ini, "Certificates", "RdpKeyFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/RdpKeyFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->RdpKeyFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "RdpKeyContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/RdpKeyContent has invalid empty value");
			return FALSE;
		}
		config->RdpKeyContent = _strdup(tmp2);
	}
	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/RdpKeyFile and Certificates/RdpKeyContent are mutually "
		              "exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/RdpKeyFile or Certificates/RdpKeyContent are "
		              "required settings");
		return FALSE;
	}

	return TRUE;
}

proxyConfig* server_config_load_ini(wIniFile* ini)
{
	proxyConfig* config = NULL;

	WINPR_ASSERT(ini);

	config = calloc(1, sizeof(proxyConfig));
	if (config)
	{
		if (!pf_config_load_server(ini, config))
			goto out;

		if (!pf_config_load_target(ini, config))
			goto out;

		if (!pf_config_load_channels(ini, config))
			goto out;

		if (!pf_config_load_input(ini, config))
			goto out;

		if (!pf_config_load_security(ini, config))
			goto out;

		if (!pf_config_load_modules(ini, config))
			goto out;

		if (!pf_config_load_clipboard(ini, config))
			goto out;

		if (!pf_config_load_gfx_settings(ini, config))
			goto out;

		if (!pf_config_load_certificates(ini, config))
			goto out;
	}
	return config;
out:
	pf_server_config_free(config);
	return NULL;
}

BOOL pf_server_config_dump(const char* file)
{
	BOOL rc = FALSE;
	wIniFile* ini = IniFile_New();
	if (!ini)
		return FALSE;

	/* Proxy server configuration */
	if (IniFile_SetKeyValueString(ini, "Server", "Host", "0.0.0.0") < 0)
		goto fail;
	if (IniFile_SetKeyValueInt(ini, "Server", "Port", 3389) < 0)
		goto fail;

	/* Target configuration */
	if (IniFile_SetKeyValueString(ini, "Target", "Host", "somehost.example.com") < 0)
		goto fail;
	if (IniFile_SetKeyValueInt(ini, "Target", "Port", 3389) < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Target", "FixedTarget", "true") < 0)
		goto fail;

	/* Channel configuration */
	if (IniFile_SetKeyValueString(ini, "Channels", "GFX", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "DisplayControl", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "Clipboard", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "AudioInput", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "AudioOutput", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "DeviceRedirection", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "VideoRedirection", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "CameraRedirection", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "RemoteApp", "false") < 0)
		goto fail;

	if (IniFile_SetKeyValueString(ini, "Channels", "PassthroughIsBlacklist", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "Passthrough", "") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Channels", "Intercept", "") < 0)
		goto fail;

	/* Input configuration */
	if (IniFile_SetKeyValueString(ini, "Input", "Keyboard", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Input", "Mouse", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Input", "Multitouch", "true") < 0)
		goto fail;

	/* Security settings */
	if (IniFile_SetKeyValueString(ini, "Security", "ServerTlsSecurity", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Security", "ServerNlaSecurity", "false") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Security", "ServerRdpSecurity", "true") < 0)
		goto fail;

	if (IniFile_SetKeyValueString(ini, "Security", "ClientTlsSecurity", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Security", "ClientNlaSecurity", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Security", "ClientRdpSecurity", "true") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Security", "ClientAllowFallbackToTls", "true") < 0)
		goto fail;

	/* Module configuration */
	if (IniFile_SetKeyValueString(ini, "Plugins", "Modules", "module1,module2,...") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Plugins", "Required", "module1,module2,...") < 0)
		goto fail;

	/* Clipboard configuration */
	if (IniFile_SetKeyValueString(ini, "Clipboard", "TextOnly", "false") < 0)
		goto fail;
	if (IniFile_SetKeyValueInt(ini, "Clipboard", "MaxTextLength", 0) < 0)
		goto fail;

	/* GFX configuration */
	if (IniFile_SetKeyValueString(ini, "GFXSettings", "DecodeGFX", "false") < 0)
		goto fail;

	/* Certificate configuration */
	if (IniFile_SetKeyValueString(ini, "Certificates", "CertificateFile",
	                              "<absolute path to some certificate file> OR") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Certificates", "CertificateContent",
	                              "<Contents of some certificate file in PEM format>") < 0)
		goto fail;

	if (IniFile_SetKeyValueString(ini, "Certificates", "PrivateKeyFile",
	                              "<absolute path to some private key file> OR") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Certificates", "PrivateKeyContent",
	                              "<Contents of some private key file in PEM format>") < 0)
		goto fail;

	if (IniFile_SetKeyValueString(ini, "Certificates", "RdpKeyFile",
	                              "<absolute path to some private key file> OR") < 0)
		goto fail;
	if (IniFile_SetKeyValueString(ini, "Certificates", "RdpKeyContent",
	                              "<Contents of some private key file in PEM format>") < 0)
		goto fail;

	/* store configuration */
	if (IniFile_WriteFile(ini, file) < 0)
		goto fail;

	rc = TRUE;

fail:
	IniFile_Free(ini);
	return rc;
}

proxyConfig* pf_server_config_load_buffer(const char* buffer)
{
	proxyConfig* config = NULL;
	wIniFile* ini;

	ini = IniFile_New();

	if (!ini)
	{
		WLog_ERR(TAG, "[%s]: IniFile_New() failed!", __FUNCTION__);
		return NULL;
	}

	if (IniFile_ReadBuffer(ini, buffer) < 0)
	{
		WLog_ERR(TAG, "[%s] failed to parse ini: '%s'", __FUNCTION__, buffer);
		goto out;
	}

	config = server_config_load_ini(ini);
out:
	IniFile_Free(ini);
	return config;
}

proxyConfig* pf_server_config_load_file(const char* path)
{
	proxyConfig* config = NULL;
	wIniFile* ini = IniFile_New();

	if (!ini)
	{
		WLog_ERR(TAG, "[%s]: IniFile_New() failed!", __FUNCTION__);
		return NULL;
	}

	if (IniFile_ReadFile(ini, path) < 0)
	{
		WLog_ERR(TAG, "[%s] failed to parse ini file: '%s'", __FUNCTION__, path);
		goto out;
	}

	config = server_config_load_ini(ini);
out:
	IniFile_Free(ini);
	return config;
}

static void pf_server_config_print_list(char** list, size_t count)
{
	size_t i;

	WINPR_ASSERT(list);
	for (i = 0; i < count; i++)
		WLog_INFO(TAG, "\t\t- %s", list[i]);
}

void pf_server_config_print(const proxyConfig* config)
{
	size_t x;

	WINPR_ASSERT(config);
	WLog_INFO(TAG, "Proxy configuration:");

	CONFIG_PRINT_SECTION("Server");
	CONFIG_PRINT_STR(config, Host);
	CONFIG_PRINT_UINT16(config, Port);

	if (config->FixedTarget)
	{
		CONFIG_PRINT_SECTION("Target");
		CONFIG_PRINT_STR(config, TargetHost);
		CONFIG_PRINT_UINT16(config, TargetPort);

		if (config->TargetUser)
			CONFIG_PRINT_STR(config, TargetUser);
		if (config->TargetDomain)
			CONFIG_PRINT_STR(config, TargetDomain);
	}

	CONFIG_PRINT_SECTION("Input");
	CONFIG_PRINT_BOOL(config, Keyboard);
	CONFIG_PRINT_BOOL(config, Mouse);
	CONFIG_PRINT_BOOL(config, Multitouch);

	CONFIG_PRINT_SECTION("Server Security");
	CONFIG_PRINT_BOOL(config, ServerTlsSecurity);
	CONFIG_PRINT_BOOL(config, ServerRdpSecurity);

	CONFIG_PRINT_SECTION("Client Security");
	CONFIG_PRINT_BOOL(config, ClientNlaSecurity);
	CONFIG_PRINT_BOOL(config, ClientTlsSecurity);
	CONFIG_PRINT_BOOL(config, ClientRdpSecurity);
	CONFIG_PRINT_BOOL(config, ClientAllowFallbackToTls);

	CONFIG_PRINT_SECTION("Channels");
	CONFIG_PRINT_BOOL(config, GFX);
	CONFIG_PRINT_BOOL(config, DisplayControl);
	CONFIG_PRINT_BOOL(config, Clipboard);
	CONFIG_PRINT_BOOL(config, AudioOutput);
	CONFIG_PRINT_BOOL(config, AudioInput);
	CONFIG_PRINT_BOOL(config, DeviceRedirection);
	CONFIG_PRINT_BOOL(config, VideoRedirection);
	CONFIG_PRINT_BOOL(config, CameraRedirection);
	CONFIG_PRINT_BOOL(config, RemoteApp);
	CONFIG_PRINT_BOOL(config, PassthroughIsBlacklist);

	if (config->PassthroughCount)
	{
		WLog_INFO(TAG, "\tStatic Channels Proxy:");
		pf_server_config_print_list(config->Passthrough, config->PassthroughCount);
	}

	if (config->InterceptCount)
	{
		WLog_INFO(TAG, "\tStatic Channels Proxy-Intercept:");
		pf_server_config_print_list(config->Intercept, config->InterceptCount);
	}

	CONFIG_PRINT_SECTION("Clipboard");
	CONFIG_PRINT_BOOL(config, TextOnly);
	if (config->MaxTextLength > 0)
		CONFIG_PRINT_UINT32(config, MaxTextLength);

	CONFIG_PRINT_SECTION("GFXSettings");
	CONFIG_PRINT_BOOL(config, DecodeGFX);

	/* modules */
	CONFIG_PRINT_SECTION("Plugins/Modules");
	for (x = 0; x < config->ModulesCount; x++)
		CONFIG_PRINT_STR(config, Modules[x]);

	/* Required plugins */
	CONFIG_PRINT_SECTION("Plugins/Required");
	for (x = 0; x < config->RequiredPluginsCount; x++)
		CONFIG_PRINT_STR(config, RequiredPlugins[x]);

	CONFIG_PRINT_SECTION("Certificates");
	CONFIG_PRINT_STR(config, CertificateFile);
	CONFIG_PRINT_STR_CONTENT(config, CertificateContent);
	CONFIG_PRINT_STR(config, PrivateKeyFile);
	CONFIG_PRINT_STR_CONTENT(config, PrivateKeyContent);
	CONFIG_PRINT_STR(config, RdpKeyFile);
	CONFIG_PRINT_STR_CONTENT(config, RdpKeyContent);
}

void pf_server_config_free(proxyConfig* config)
{
	if (config == NULL)
		return;

	free(config->Passthrough);
	free(config->Intercept);
	free(config->RequiredPlugins);
	free(config->Modules);
	free(config->TargetHost);
	free(config->Host);
	free(config->CertificateFile);
	free(config->CertificateContent);
	free(config->PrivateKeyFile);
	free(config->PrivateKeyContent);
	free(config->RdpKeyFile);
	free(config->RdpKeyContent);
	free(config);
}

size_t pf_config_required_plugins_count(const proxyConfig* config)
{
	WINPR_ASSERT(config);
	return config->RequiredPluginsCount;
}

const char* pf_config_required_plugin(const proxyConfig* config, size_t index)
{
	WINPR_ASSERT(config);
	if (index >= config->RequiredPluginsCount)
		return NULL;

	return config->RequiredPlugins[index];
}

size_t pf_config_modules_count(const proxyConfig* config)
{
	WINPR_ASSERT(config);
	return config->ModulesCount;
}

const char** pf_config_modules(const proxyConfig* config)
{
	union
	{
		char** ppc;
		const char** cppc;
	} cnv;

	WINPR_ASSERT(config);

	cnv.ppc = config->Modules;
	return cnv.cppc;
}

static BOOL pf_config_copy_string(char** dst, const char* src)
{
	*dst = NULL;
	if (src)
		*dst = _strdup(src);
	return TRUE;
}

static BOOL pf_config_copy_string_list(char*** dst, size_t* size, char** src, size_t srcSize)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(size);
	WINPR_ASSERT(src || (srcSize == 0));

	*dst = NULL;
	*size = 0;
	if (srcSize == 0)
		return TRUE;
	{
		char* csv = CommandLineToCommaSeparatedValues(srcSize, src);
		*dst = CommandLineParseCommaSeparatedValues(csv, size);
		free(csv);
	}

	return TRUE;
}

BOOL pf_config_clone(proxyConfig** dst, const proxyConfig* config)
{
	proxyConfig* tmp = calloc(1, sizeof(proxyConfig));

	WINPR_ASSERT(dst);
	WINPR_ASSERT(config);

	if (!tmp)
		return FALSE;

	*tmp = *config;

	if (!pf_config_copy_string(&tmp->Host, config->Host))
		goto fail;
	if (!pf_config_copy_string(&tmp->TargetHost, config->TargetHost))
		goto fail;

	if (!pf_config_copy_string_list(&tmp->Passthrough, &tmp->PassthroughCount, config->Passthrough,
	                                config->PassthroughCount))
		goto fail;
	if (!pf_config_copy_string_list(&tmp->Intercept, &tmp->InterceptCount, config->Intercept,
	                                config->InterceptCount))
		goto fail;
	if (!pf_config_copy_string_list(&tmp->Modules, &tmp->ModulesCount, config->Modules,
	                                config->ModulesCount))
		goto fail;
	if (!pf_config_copy_string_list(&tmp->RequiredPlugins, &tmp->RequiredPluginsCount,
	                                config->RequiredPlugins, config->RequiredPluginsCount))
		goto fail;
	if (!pf_config_copy_string(&tmp->CertificateFile, config->CertificateFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->CertificateContent, config->CertificateContent))
		goto fail;
	if (!pf_config_copy_string(&tmp->PrivateKeyFile, config->PrivateKeyFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->PrivateKeyContent, config->PrivateKeyContent))
		goto fail;
	if (!pf_config_copy_string(&tmp->RdpKeyFile, config->RdpKeyFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->RdpKeyContent, config->RdpKeyContent))
		goto fail;

	*dst = tmp;
	return TRUE;

fail:
	pf_server_config_free(tmp);
	return FALSE;
}

struct config_plugin_data
{
	proxyPluginsManager* mgr;
	const proxyConfig* config;
};

static const char config_plugin_name[] = "config";
static const char config_plugin_desc[] =
    "A plugin filtering according to proxy configuration file rules";

static BOOL config_plugin_unload(proxyPlugin* plugin)
{
	WINPR_ASSERT(plugin);

	/* Here we have to free up our custom data storage. */
	if (plugin)
	{
		free(plugin->custom);
		plugin->custom = NULL;
	}

	return TRUE;
}

static BOOL config_plugin_keyboard_event(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	BOOL rc;
	const struct config_plugin_data* custom;
	const proxyConfig* cfg;
	const proxyKeyboardEventInfo* event_data = (const proxyKeyboardEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	WINPR_UNUSED(event_data);

	custom = plugin->custom;
	WINPR_ASSERT(custom);

	cfg = custom->config;
	WINPR_ASSERT(cfg);

	rc = cfg->Keyboard;
	WLog_DBG(TAG, "%s: %s", __FUNCTION__, rc ? "TRUE" : "FALSE");
	return rc;
}

static BOOL config_plugin_mouse_event(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	BOOL rc;
	const struct config_plugin_data* custom;
	const proxyConfig* cfg;
	const proxyMouseEventInfo* event_data = (const proxyMouseEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	WINPR_UNUSED(event_data);

	custom = plugin->custom;
	WINPR_ASSERT(custom);

	cfg = custom->config;
	WINPR_ASSERT(cfg);

	rc = cfg->Mouse;
	return rc;
}

static BOOL config_plugin_client_channel_data(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	const proxyChannelDataEventInfo* channel = (const proxyChannelDataEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	WLog_DBG(TAG, "%s: %s [0x%04" PRIx16 "] got %" PRIuz, __FUNCTION__, channel->channel_name,
	         channel->channel_id, channel->data_len);
	return TRUE;
}

static BOOL config_plugin_server_channel_data(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	const proxyChannelDataEventInfo* channel = (const proxyChannelDataEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	WLog_DBG(TAG, "%s: %s [0x%04" PRIx16 "] got %" PRIuz, __FUNCTION__, channel->channel_name,
	         channel->channel_id, channel->data_len);
	return TRUE;
}

static BOOL config_plugin_dynamic_channel_create(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	pf_utils_channel_mode rc;
	BOOL accept;
	const struct config_plugin_data* custom;
	const proxyConfig* cfg;
	const proxyChannelDataEventInfo* channel = (const proxyChannelDataEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	custom = plugin->custom;
	WINPR_ASSERT(custom);

	cfg = custom->config;
	WINPR_ASSERT(cfg);

	rc = pf_utils_get_channel_mode(cfg, channel->channel_name);
	switch (rc)
	{

		case PF_UTILS_CHANNEL_INTERCEPT:
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			accept = TRUE;
			break;
		case PF_UTILS_CHANNEL_BLOCK:
		default:
			accept = FALSE;
			break;
	}

	if (accept)
	{
		if (strncmp(RDPGFX_DVC_CHANNEL_NAME, channel->channel_name,
		            sizeof(RDPGFX_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->GFX;
		else if (strncmp(RDPSND_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RDPSND_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->AudioOutput;
		else if (strncmp(RDPSND_LOSSY_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RDPSND_LOSSY_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->AudioOutput;
		else if (strncmp(AUDIN_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(AUDIN_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->AudioInput;
		else if (strncmp(RDPEI_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RDPEI_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->Multitouch;
		else if (strncmp(TSMF_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(TSMF_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->VideoRedirection;
		else if (strncmp(VIDEO_CONTROL_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(VIDEO_CONTROL_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->VideoRedirection;
		else if (strncmp(VIDEO_DATA_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(VIDEO_DATA_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->VideoRedirection;
		else if (strncmp(RDPECAM_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RDPECAM_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->CameraRedirection;
	}

	WLog_DBG(TAG, "%s: %s [0x%04" PRIx16 "]: %s", __FUNCTION__, channel->channel_name,
	         channel->channel_id, accept ? "TRUE" : "FALSE");
	return accept;
}

static BOOL config_plugin_channel_create(proxyPlugin* plugin, proxyData* pdata, void* param)
{
	pf_utils_channel_mode rc;
	BOOL accept;
	const struct config_plugin_data* custom;
	const proxyConfig* cfg;
	const proxyChannelDataEventInfo* channel = (const proxyChannelDataEventInfo*)(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	custom = plugin->custom;
	WINPR_ASSERT(custom);

	cfg = custom->config;
	WINPR_ASSERT(cfg);

	rc = pf_utils_get_channel_mode(cfg, channel->channel_name);
	switch (rc)
	{
		case PF_UTILS_CHANNEL_INTERCEPT:
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			accept = TRUE;
			break;
		case PF_UTILS_CHANNEL_BLOCK:
		default:
			accept = FALSE;
			break;
	}
	if (accept)
	{
		if (strncmp(CLIPRDR_SVC_CHANNEL_NAME, channel->channel_name,
		            sizeof(CLIPRDR_SVC_CHANNEL_NAME)) == 0)
			accept = cfg->Clipboard;
		else if (strncmp(RDPSND_CHANNEL_NAME, channel->channel_name, sizeof(RDPSND_CHANNEL_NAME)) ==
		         0)
			accept = cfg->AudioOutput;
		else if (strncmp(RDPDR_SVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RDPDR_SVC_CHANNEL_NAME)) == 0)
			accept = cfg->DeviceRedirection;
		else if (strncmp(DISP_DVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(DISP_DVC_CHANNEL_NAME)) == 0)
			accept = cfg->DisplayControl;
		else if (strncmp(RAIL_SVC_CHANNEL_NAME, channel->channel_name,
		                 sizeof(RAIL_SVC_CHANNEL_NAME)) == 0)
			accept = cfg->RemoteApp;
	}

	WLog_DBG(TAG, "%s: %s [static]: %s", __FUNCTION__, channel->channel_name,
	         accept ? "TRUE" : "FALSE");
	return accept;
}

BOOL pf_config_plugin(proxyPluginsManager* plugins_manager, void* userdata)
{
	struct config_plugin_data* custom;
	proxyPlugin plugin = { 0 };

	plugin.name = config_plugin_name;
	plugin.description = config_plugin_desc;
	plugin.PluginUnload = config_plugin_unload;

	plugin.KeyboardEvent = config_plugin_keyboard_event;
	plugin.MouseEvent = config_plugin_mouse_event;
	plugin.ClientChannelData = config_plugin_client_channel_data;
	plugin.ServerChannelData = config_plugin_server_channel_data;
	plugin.ChannelCreate = config_plugin_channel_create;
	plugin.DynamicChannelCreate = config_plugin_dynamic_channel_create;
	plugin.userdata = userdata;

	custom = calloc(1, sizeof(struct config_plugin_data));
	if (!custom)
		return FALSE;

	custom->mgr = plugins_manager;
	custom->config = userdata;

	plugin.custom = custom;
	plugin.userdata = userdata;

	return plugins_manager->RegisterPlugin(plugins_manager, &plugin);
}
