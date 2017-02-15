/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_H
#define FREERDP_CLIENT_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Client Entry Points
 */

typedef BOOL (*pRdpGlobalInit)(void);
typedef void (*pRdpGlobalUninit)(void);

typedef BOOL (*pRdpClientNew)(freerdp* instance, rdpContext* context);
typedef void (*pRdpClientFree)(freerdp* instance, rdpContext* context);

typedef int (*pRdpClientStart)(rdpContext* context);
typedef int (*pRdpClientStop)(rdpContext* context);

struct rdp_client_entry_points_v1
{
	DWORD Size;
	DWORD Version;

	rdpSettings* settings;

	pRdpGlobalInit GlobalInit;
	pRdpGlobalUninit GlobalUninit;

	DWORD ContextSize;
	pRdpClientNew ClientNew;
	pRdpClientFree ClientFree;

	pRdpClientStart ClientStart;
	pRdpClientStop ClientStop;
};

#define RDP_CLIENT_INTERFACE_VERSION	1
#define RDP_CLIENT_ENTRY_POINT_NAME	"RdpClientEntry"

typedef int (*pRdpClientEntry)(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

/* Common Client Interface */

#define DEFINE_RDP_CLIENT_COMMON() \
	HANDLE thread

struct rdp_client_context
{
	rdpContext context;
	DEFINE_RDP_CLIENT_COMMON();
};

/* Common client functions */

FREERDP_API rdpContext* freerdp_client_context_new(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);
FREERDP_API void freerdp_client_context_free(rdpContext* context);

FREERDP_API int freerdp_client_start(rdpContext* context);
FREERDP_API int freerdp_client_stop(rdpContext* context);

FREERDP_API freerdp* freerdp_client_get_instance(rdpContext* context);
FREERDP_API HANDLE freerdp_client_get_thread(rdpContext* context);

FREERDP_API int freerdp_client_settings_parse_command_line(rdpSettings* settings,
	int argc, char** argv, BOOL allowUnknown);

FREERDP_API int freerdp_client_settings_parse_connection_file(rdpSettings* settings, const char* filename);
FREERDP_API int freerdp_client_settings_parse_connection_file_buffer(rdpSettings* settings, const BYTE* buffer, size_t size);
FREERDP_API int freerdp_client_settings_write_connection_file(const rdpSettings* settings, const char* filename, BOOL unicode);

FREERDP_API int freerdp_client_settings_parse_assistance_file(rdpSettings* settings, const char* filename);

FREERDP_API BOOL client_cli_authenticate(freerdp* instance, char** username, char** password, char** domain);
FREERDP_API BOOL client_cli_gw_authenticate(freerdp* instance, char** username, char** password, char** domain);

FREERDP_API DWORD client_cli_verify_certificate(freerdp* instance, const char* common_name,
				   const char* subject, const char* issuer,
				   const char* fingerprint, BOOL host_mismatch);

FREERDP_API DWORD client_cli_verify_changed_certificate(freerdp* instance, const char* common_name,
					   const char* subject, const char* issuer,
					   const char* fingerprint,
					   const char* old_subject, const char* old_issuer,
					   const char* old_fingerprint);
#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_H */
