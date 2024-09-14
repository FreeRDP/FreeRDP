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

#include <winpr/cmdline.h>

#include <freerdp/config.h>
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>

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

#define FREERDP_MAX_PEN_DEVICES 10

	typedef struct pen_device
	{
		ALIGN64 INT32 deviceid;
		ALIGN64 UINT32 flags;
		ALIGN64 double max_pressure;
		ALIGN64 BOOL hovering;
		ALIGN64 BOOL pressed;
		ALIGN64 INT32 last_x;
		ALIGN64 INT32 last_y;
	} FreeRDP_PenDevice;

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
		ALIGN64 FreeRDP_PenDevice pens[FREERDP_MAX_PEN_DEVICES];           /**< (offset 9) */
		UINT64 reserved[128 - 9];                                          /**< (offset 9) */
	};

	/* Common client functions */

	FREERDP_API void freerdp_client_context_free(rdpContext* context);

	WINPR_ATTR_MALLOC(freerdp_client_context_free, 1)
	FREERDP_API rdpContext* freerdp_client_context_new(const RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

	FREERDP_API int freerdp_client_start(rdpContext* context);
	FREERDP_API int freerdp_client_stop(rdpContext* context);

	FREERDP_API freerdp* freerdp_client_get_instance(rdpContext* context);
	FREERDP_API HANDLE freerdp_client_get_thread(rdpContext* context);

	FREERDP_API int freerdp_client_settings_parse_command_line(rdpSettings* settings, int argc,
	                                                           char** argv, BOOL allowUnknown);

	/**
	 * @brief freerdp_client_settings_parse_command_line_ex
	 * @param settings Pointer to the settings to populate
	 * @param argc Number of command line arguments
	 * @param argv Array of command line arguments
	 * @param allowUnknown Skip unknown arguments instead of aborting parser
	 * @param args The allowed command line arguments (client specific, client-common ones are added
	 * internally)
	 * @param count Number of client specific command line arguments
	 * @param handle_userdata Custom user data pointer, will be passed to callback
	 * @return >=0 for success, <0 in case of parsing failures
	 * @since version 3.9.0
	 */
	FREERDP_API int freerdp_client_settings_parse_command_line_ex(
	    rdpSettings* settings, int argc, char** argv, BOOL allowUnknown,
	    COMMAND_LINE_ARGUMENT_A* args, size_t count,
	    int (*handle_option)(const COMMAND_LINE_ARGUMENT_A* arg, void* custom),
	    void* handle_userdata);

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

	FREERDP_API BOOL client_cli_choose_smartcard(freerdp* instance, SmartcardCertInfo** cert_list,
	                                             DWORD count, DWORD* choice, BOOL gateway);

	FREERDP_API int client_cli_logon_error_info(freerdp* instance, UINT32 data, UINT32 type);

	FREERDP_API BOOL client_cli_get_access_token(freerdp* instance, AccessTokenType tokenType,
	                                             char** token, size_t count, ...);
	FREERDP_API BOOL client_common_get_access_token(freerdp* instance, const char* request,
	                                                char** token);

	FREERDP_API SSIZE_T client_common_retry_dialog(freerdp* instance, const char* what,
	                                               size_t current, void* userarg);

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

	typedef enum
	{
		FREERDP_PEN_REGISTER = 0x01,
		FREERDP_PEN_ERASER_PRESSED = 0x02,
		FREERDP_PEN_PRESS = 0x04,
		FREERDP_PEN_MOTION = 0x08,
		FREERDP_PEN_RELEASE = 0x10,
		FREERDP_PEN_BARREL_PRESSED = 0x20,
		FREERDP_PEN_HAS_PRESSURE = 0x40,
		FREERDP_PEN_HAS_ROTATION = 0x80,
		FREERDP_PEN_HAS_TILTX = 0x100,
		FREERDP_PEN_HAS_TILTY = 0x200,
		FREERDP_PEN_IS_INVERTED = 0x400
	} FreeRDPPenEventType;

	FREERDP_API BOOL freerdp_client_handle_pen(rdpClientContext* cctx, UINT32 flags, INT32 deviceid,
	                                           ...);
	FREERDP_API BOOL freerdp_client_is_pen(rdpClientContext* cctx, INT32 deviceid);

	FREERDP_API BOOL freerdp_client_pen_cancel_all(rdpClientContext* cctx);

	FREERDP_API BOOL freerdp_client_send_wheel_event(rdpClientContext* cctx, UINT16 mflags);

	FREERDP_API BOOL freerdp_client_send_mouse_event(rdpClientContext* cctx, UINT64 mflags, INT32 x,
	                                                 INT32 y);

	/** @brief this function checks if relative mouse events are supported and enabled for this
	 * session.
	 *
	 *  @param cctx The \b rdpClientContext to check
	 *
	 *  @return \b TRUE if relative mouse events are to be sent, \b FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_client_use_relative_mouse_events(rdpClientContext* cctx);

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
