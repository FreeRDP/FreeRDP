/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
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
#ifndef FREERDP_SERVER_PROXY_CONFIG_H
#define FREERDP_SERVER_PROXY_CONFIG_H

#include <winpr/wtypes.h>
#include <winpr/ini.h>

#include <freerdp/api.h>
#include <freerdp/server/proxy/proxy_modules_api.h>

typedef struct proxy_config proxyConfig;

struct proxy_config
{
	/* server */
	char* Host;
	UINT16 Port;

	/* target */
	BOOL FixedTarget;
	char* TargetHost;
	UINT16 TargetPort;
	char* TargetUser;
	char* TargetDomain;
	char* TargetPassword;

	/* input */
	BOOL Keyboard;
	BOOL Mouse;
	BOOL Multitouch;

	/* server security */
	BOOL ServerTlsSecurity;
	BOOL ServerRdpSecurity;
	BOOL ServerNlaSecurity;

	/* client security */
	BOOL ClientNlaSecurity;
	BOOL ClientTlsSecurity;
	BOOL ClientRdpSecurity;
	BOOL ClientAllowFallbackToTls;

	/* channels */
	BOOL GFX;
	BOOL DisplayControl;
	BOOL Clipboard;
	BOOL AudioOutput;
	BOOL AudioInput;
	BOOL RemoteApp;
	BOOL DeviceRedirection;
	BOOL VideoRedirection;
	BOOL CameraRedirection;

	BOOL PassthroughIsBlacklist;
	char** Passthrough;
	size_t PassthroughCount;
	char** Intercept;
	size_t InterceptCount;

	/* clipboard specific settings */
	BOOL TextOnly;
	UINT32 MaxTextLength;

	/* gfx settings */
	BOOL DecodeGFX;

	/* modules */
	char** Modules; /* module file names to load */
	size_t ModulesCount;

	char** RequiredPlugins; /* required plugin names */
	size_t RequiredPluginsCount;

	char* CertificateFile;
	char* CertificateContent;

	char* PrivateKeyFile;
	char* PrivateKeyContent;
};

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief pf_server_config_dump Dumps a default INI configuration file
	 * @param file The file to write to. Existing files are truncated.
	 * @return TRUE for success, FALSE if the file could not be written.
	 */
	FREERDP_API BOOL pf_server_config_dump(const char* file);

	/**
	 * @brief server_config_load_ini Create a proxyConfig from a already loaded
	 * INI file.
	 *
	 * @param ini A pointer to the parsed INI file. Must NOT be NULL.
	 *
	 * @return A proxyConfig or NULL in case of failure.
	 */
	FREERDP_API proxyConfig* server_config_load_ini(wIniFile* ini);
	/**
	 * @brief pf_server_config_load_file Create a proxyConfig from a INI file found at path.
	 *
	 * @param path The path of the INI file
	 *
	 * @return A proxyConfig or NULL in case of failure.
	 */
	FREERDP_API proxyConfig* pf_server_config_load_file(const char* path);

	/**
	 * @brief pf_server_config_load_buffer Create a proxyConfig from a memory string buffer in INI
	 * file format
	 *
	 * @param buffer A pointer to the '\0' terminated INI string.
	 *
	 * @return A proxyConfig or NULL in case of failure.
	 */
	FREERDP_API proxyConfig* pf_server_config_load_buffer(const char* buffer);

	/**
	 * @brief pf_server_config_print Print the configuration to stdout
	 *
	 * @param config A pointer to the configuration to print. Must NOT be NULL.
	 */
	FREERDP_API void pf_server_config_print(const proxyConfig* config);

	/**
	 * @brief pf_server_config_free Releases all resources associated with proxyConfig
	 *
	 * @param config A pointer to the proxyConfig to clean up. Might be NULL.
	 */
	FREERDP_API void pf_server_config_free(proxyConfig* config);

	/**
	 * @brief pf_config_required_plugins_count
	 *
	 * @param config A pointer to the proxyConfig. Must NOT be NULL.
	 *
	 * @return The number of required plugins configured.
	 */
	FREERDP_API size_t pf_config_required_plugins_count(const proxyConfig* config);

	/**
	 * @brief pf_config_required_plugin
	 * @param config A pointer to the proxyConfig. Must NOT be NULL.
	 * @param index The index of the plugin to return
	 *
	 * @return The name of the plugin or NULL.
	 */
	FREERDP_API const char* pf_config_required_plugin(const proxyConfig* config, size_t index);

	/**
	 * @brief pf_config_modules_count
	 *
	 * @param config A pointer to the proxyConfig. Must NOT be NULL.
	 *
	 * @return The number of proxy modules configured.
	 */
	FREERDP_API size_t pf_config_modules_count(const proxyConfig* config);

	/**
	 * @brief pf_config_modules
	 * @param config A pointer to the proxyConfig. Must NOT be NULL.
	 *
	 * @return An array of strings of size pf_config_modules_count with the module names.
	 */
	FREERDP_API const char** pf_config_modules(const proxyConfig* config);

	/**
	 * @brief pf_config_clone Create a copy of the configuration
	 * @param dst A pointer that receives the newly allocated copy
	 * @param config The source configuration to copy
	 *
	 * @return TRUE for success, FALSE otherwise
	 */
	FREERDP_API BOOL pf_config_clone(proxyConfig** dst, const proxyConfig* config);

	/**
	 * @brief pf_config_plugin Register a proxy plugin handling event filtering
	 * defined in the configuration.
	 *
	 * @param plugins_manager The plugin manager
	 * @param userdata A proxyConfig* to use as reference
	 *
	 * @return  TRUE for success, FALSE for failure
	 */
	FREERDP_API BOOL pf_config_plugin(proxyPluginsManager* plugins_manager, void* userdata);

#ifdef __cplusplus
};
#endif
#endif /* FREERDP_SERVER_PROXY_CONFIG_H */
