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

#include <freerdp/config.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/event.h>

#if defined(CHANNEL_AINPUT_CLIENT)
#include <freerdp/client/ainput.h>
#endif

#if defined(CHANNEL_RDPEI_CLIENT)
#include <freerdp/client/rdpei.h>
#endif

#if defined(CHANNEL_ENCOMSP_CLIENT)
#include <freerdp/client/encomsp.h>
#endif

#ifdef __cplusplus
extern "C"
{
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

#define RDP_CLIENT_INTERFACE_VERSION 1
#define RDP_CLIENT_ENTRY_POINT_NAME "RdpClientEntry"

	typedef int (*pRdpClientEntry)(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

	/* Common Client Interface */
#define FREERDP_MAX_TOUCH_CONTACTS 10

	typedef struct
	{
		ALIGN64 INT32 id;
		ALIGN64 UINT32 count;
		ALIGN64 INT32 x;
		ALIGN64 INT32 y;
		ALIGN64 UINT32 flags;
		ALIGN64 UINT32 pressure;
	} FreeRDP_TouchContact;

	struct rdp_client_context
	{
		rdpContext context;
		ALIGN64 HANDLE thread; /**< (offset 0) */
#if defined(CHANNEL_AINPUT_CLIENT)
		ALIGN64 AInputClientContext* ainput; /**< (offset 1) */
#else
	UINT64 reserved1;
#endif

#if defined(CHANNEL_RDPEI_CLIENT)
		ALIGN64 RdpeiClientContext* rdpei; /**< (offset 2) */
#else
	UINT64 reserved2;
#endif

		ALIGN64 INT32 lastX;        /**< (offset 3) */
		ALIGN64 INT32 lastY;        /**< (offset 4) */
		ALIGN64 BOOL mouse_grabbed; /** < (offset 5) */

#if defined(CHANNEL_ENCOMSP_CLIENT)
		ALIGN64 EncomspClientContext* encomsp; /** < (offset 6) */
		ALIGN64 BOOL controlToggle;            /**< (offset 7) */
#else
	    UINT64 reserved3[2];
#endif
		ALIGN64 FreeRDP_TouchContact contacts[FREERDP_MAX_TOUCH_CONTACTS]; /**< (offset 8) */
		UINT64 reserved[128 - 68];                                         /**< (offset 68) */
	};

	/* Common client functions */

	FREERDP_API rdpContext* freerdp_client_context_new(const RDP_CLIENT_ENTRY_POINTS* pEntryPoints);
	FREERDP_API void freerdp_client_context_free(rdpContext* context);

	FREERDP_API int freerdp_client_start(rdpContext* context);
	FREERDP_API int freerdp_client_stop(rdpContext* context);

	FREERDP_API freerdp* freerdp_client_get_instance(rdpContext* context);
	FREERDP_API HANDLE freerdp_client_get_thread(rdpContext* context);

	FREERDP_API int freerdp_client_settings_parse_command_line(rdpSettings* settings, int argc,
	                                                           char** argv, BOOL allowUnknown);

	FREERDP_API int freerdp_client_settings_parse_connection_file(rdpSettings* settings,
	                                                              const char* filename);
	FREERDP_API int freerdp_client_settings_parse_connection_file_buffer(rdpSettings* settings,
	                                                                     const BYTE* buffer,
	                                                                     size_t size);
	FREERDP_API int freerdp_client_settings_write_connection_file(const rdpSettings* settings,
	                                                              const char* filename,
	                                                              BOOL unicode);

	FREERDP_API int freerdp_client_settings_parse_assistance_file(rdpSettings* settings, int argc,
	                                                              char* argv[]);

	FREERDP_API BOOL client_cli_authenticate_ex(freerdp* instance, char** username, char** password,
	                                            char** domain, rdp_auth_reason reason);

	FREERDP_API BOOL client_cli_choose_smartcard(SmartcardCertInfo** cert_list, DWORD count,
	                                             DWORD* choice, BOOL gateway);

	FREERDP_API void
	freerdp_client_OnChannelConnectedEventHandler(void* context,
	                                              const ChannelConnectedEventArgs* e);
	FREERDP_API void
	freerdp_client_OnChannelDisconnectedEventHandler(void* context,
	                                                 const ChannelDisconnectedEventArgs* e);

#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("Use client_cli_authenticate_ex",
	                                 BOOL client_cli_authenticate(freerdp* instance,
	                                                              char** username, char** password,
	                                                              char** domain));
	FREERDP_API
	WINPR_DEPRECATED_VAR("Use client_cli_authenticate_ex",
	                     BOOL client_cli_gw_authenticate(freerdp* instance, char** username,
	                                                     char** password, char** domain));

	FREERDP_API WINPR_DEPRECATED_VAR(
	    "Use client_cli_verify_certificate_ex",
	    DWORD client_cli_verify_certificate(freerdp* instance, const char* common_name,
	                                        const char* subject, const char* issuer,
	                                        const char* fingerprint, BOOL host_mismatch));
#endif

	FREERDP_API DWORD client_cli_verify_certificate_ex(freerdp* instance, const char* host,
	                                                   UINT16 port, const char* common_name,
	                                                   const char* subject, const char* issuer,
	                                                   const char* fingerprint, DWORD flags);

#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("Use client_cli_verify_changed_certificate_ex",
	                                 DWORD client_cli_verify_changed_certificate(
	                                     freerdp* instance, const char* common_name,
	                                     const char* subject, const char* issuer,
	                                     const char* fingerprint, const char* old_subject,
	                                     const char* old_issuer, const char* old_fingerprint));
#endif

	FREERDP_API DWORD client_cli_verify_changed_certificate_ex(
	    freerdp* instance, const char* host, UINT16 port, const char* common_name,
	    const char* subject, const char* issuer, const char* fingerprint, const char* old_subject,
	    const char* old_issuer, const char* old_fingerprint, DWORD flags);

	FREERDP_API BOOL client_cli_present_gateway_message(freerdp* instance, UINT32 type,
	                                                    BOOL isDisplayMandatory,
	                                                    BOOL isConsentMandatory, size_t length,
	                                                    const WCHAR* message);

	FREERDP_API BOOL client_auto_reconnect(freerdp* instance);
	FREERDP_API BOOL client_auto_reconnect_ex(freerdp* instance,
	                                          BOOL (*window_events)(freerdp* instance));

	typedef enum
	{
		FREERDP_TOUCH_DOWN = 0x01,
		FREERDP_TOUCH_UP = 0x02,
		FREERDP_TOUCH_MOTION = 0x04,
		FREERDP_TOUCH_HAS_PRESSURE = 0x100
	} FreeRDPTouchEventType;

	FREERDP_API BOOL freerdp_client_handle_touch(rdpClientContext* cctx, UINT32 flags, INT32 finger,
	                                             UINT32 pressure, INT32 x, INT32 y);

	FREERDP_API BOOL freerdp_client_send_wheel_event(rdpClientContext* cctx, UINT16 mflags);

	FREERDP_API BOOL freerdp_client_send_mouse_event(rdpClientContext* cctx, UINT64 mflags, INT32 x,
	                                                 INT32 y);

	FREERDP_API BOOL freerdp_client_send_button_event(rdpClientContext* cctx, BOOL relative,
	                                                  UINT16 mflags, INT32 x, INT32 y);

	FREERDP_API BOOL freerdp_client_send_extended_button_event(rdpClientContext* cctx,
	                                                           BOOL relative, UINT16 mflags,
	                                                           INT32 x, INT32 y);

	FREERDP_API int freerdp_client_common_stop(rdpContext* context);

	FREERDP_API BOOL freerdp_client_load_channels(freerdp* instance);

#if defined(CHANNEL_ENCOMSP_CLIENT)
	FREERDP_API BOOL freerdp_client_encomsp_toggle_control(EncomspClientContext* encomsp);
	FREERDP_API BOOL freerdp_client_encomsp_set_control(EncomspClientContext* encomsp,
	                                                    BOOL control);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_H */
