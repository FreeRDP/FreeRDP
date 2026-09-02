/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2025 Siemens
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
#include <freerdp/client/cmdline.h>

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

/** @brief Opaqye handle for AAD wrapper
 * @since version 3.16.0
 */
typedef struct MIBClientWrapper MIBClientWrapper;

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

		WINPR_ATTR_NODISCARD pRdpGlobalInit GlobalInit;
		pRdpGlobalUninit GlobalUninit;

		DWORD ContextSize;
		WINPR_ATTR_NODISCARD pRdpClientNew ClientNew;
		pRdpClientFree ClientFree;

		WINPR_ATTR_NODISCARD pRdpClientStart ClientStart;
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

	/** @brief The state of the OAuth transaction of a client context
	 *
	 *  Created by an authorization request built with \ref freerdp_client_get_aad_url and
	 *  released by \ref freerdp_client_aad_reset. The layout is private to the library.
	 *  @since version 3.32.0
	 */
	typedef struct client_aad_oauth rdpClientAadOAuth;

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

		ALIGN64 MIBClientWrapper* mibClientWrapper; /**< (offset 10) @since version 3.16.0 */
		ALIGN64 BOOL pressed_buttons[5];            /**< (offset 11) @since version 3.17.0 */
		/** Opaque state of the OAuth transaction started by \ref freerdp_client_get_aad_url,
		 *  released by \ref freerdp_client_aad_reset.
		 *  (offset 16) @since version 3.32.0 */
		ALIGN64 rdpClientAadOAuth* aad_oauth;
		UINT64 reserved[129 - 17]; /**< (offset 17) */
	};

	/* Common client functions */

	FREERDP_API void freerdp_client_context_free(rdpContext* context);

	WINPR_ATTR_MALLOC(freerdp_client_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API rdpContext* freerdp_client_context_new(const RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_start(rdpContext* context);

	FREERDP_API int freerdp_client_stop(rdpContext* context);

	WINPR_ATTR_NODISCARD
	FREERDP_API freerdp* freerdp_client_get_instance(rdpContext* context);

	WINPR_ATTR_NODISCARD
	FREERDP_API HANDLE freerdp_client_get_thread(rdpContext* context);

	WINPR_ATTR_NODISCARD
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
	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_settings_parse_command_line_ex(
	    rdpSettings* settings, int argc, char** argv, BOOL allowUnknown,
	    COMMAND_LINE_ARGUMENT_A* args, size_t count,
	    freerdp_command_line_handle_option_t handle_option, void* handle_userdata);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_settings_parse_connection_file(rdpSettings* settings,
	                                                              const char* filename);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_settings_parse_connection_file_buffer(rdpSettings* settings,
	                                                                     const BYTE* buffer,
	                                                                     size_t size);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_settings_write_connection_file(const rdpSettings* settings,
	                                                              const char* filename,
	                                                              BOOL unicode);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_settings_parse_assistance_file(rdpSettings* settings, int argc,
	                                                              char* argv[]);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_cli_authenticate_ex(freerdp* instance, char** username, char** password,
	                                            char** domain, rdp_auth_reason reason);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_cli_choose_smartcard(freerdp* instance, SmartcardCertInfo** cert_list,
	                                             DWORD count, DWORD* choice, BOOL gateway);

	WINPR_ATTR_NODISCARD
	FREERDP_API int client_cli_logon_error_info(freerdp* instance, UINT32 data, UINT32 type);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_cli_get_access_token(freerdp* instance, AccessTokenType tokenType,
	                                             char** token, size_t count, ...);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_common_get_access_token(freerdp* instance, const char* request,
	                                                char** token);

	WINPR_ATTR_NODISCARD
	FREERDP_API SSIZE_T client_common_retry_dialog(freerdp* instance, const char* what,
	                                               size_t current, void* userarg);

	/** @brief Handle SaveSessionInfo data
	 *
	 *  @param context The RDP context to operate on
	 *  @param type The type of session info received, see \ref RDP_LOGON_INFO_TYPE
	 *  @param data Additional type specific data. See \ref logon_info and \ref logon_info_ex
	 *
	 *  @return TRUE for success, FALSE otherwise
	 *  @since version 3.31.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_common_save_session_info(rdpContext* context, UINT32 type,
	                                                 const void* data);

	FREERDP_API void
	freerdp_client_OnChannelConnectedEventHandler(void* context,
	                                              const ChannelConnectedEventArgs* e);
	FREERDP_API void
	freerdp_client_OnChannelDisconnectedEventHandler(void* context,
	                                                 const ChannelDisconnectedEventArgs* e);

#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use client_cli_authenticate_ex",
	                     WINPR_ATTR_NODISCARD FREERDP_API BOOL client_cli_authenticate(
	                         freerdp* instance, char** username, char** password, char** domain));
	WINPR_DEPRECATED_VAR("Use client_cli_authenticate_ex",
	                     WINPR_ATTR_NODISCARD FREERDP_API BOOL client_cli_gw_authenticate(
	                         freerdp* instance, char** username, char** password, char** domain));

	WINPR_DEPRECATED_VAR("Use client_cli_verify_certificate_ex",
	                     WINPR_ATTR_NODISCARD FREERDP_API DWORD client_cli_verify_certificate(
	                         freerdp* instance, const char* common_name, const char* subject,
	                         const char* issuer, const char* fingerprint, BOOL host_mismatch));
#endif

	WINPR_ATTR_NODISCARD
	FREERDP_API DWORD client_cli_verify_certificate_ex(freerdp* instance, const char* host,
	                                                   UINT16 port, const char* common_name,
	                                                   const char* subject, const char* issuer,
	                                                   const char* fingerprint, DWORD flags);

#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR(
	    "Use client_cli_verify_changed_certificate_ex",
	    WINPR_ATTR_NODISCARD FREERDP_API DWORD client_cli_verify_changed_certificate(
	        freerdp* instance, const char* common_name, const char* subject, const char* issuer,
	        const char* fingerprint, const char* old_subject, const char* old_issuer,
	        const char* old_fingerprint));
#endif

	WINPR_ATTR_NODISCARD
	FREERDP_API DWORD client_cli_verify_changed_certificate_ex(
	    freerdp* instance, const char* host, UINT16 port, const char* common_name,
	    const char* subject, const char* issuer, const char* fingerprint, const char* old_subject,
	    const char* old_issuer, const char* old_fingerprint, DWORD flags);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_cli_present_gateway_message(freerdp* instance, UINT32 type,
	                                                    BOOL isDisplayMandatory,
	                                                    BOOL isConsentMandatory, size_t length,
	                                                    const WCHAR* message);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_auto_reconnect(freerdp* instance);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL client_auto_reconnect_ex(freerdp* instance,
	                                          BOOL (*window_events)(freerdp* instance));

	typedef enum
	{
		FREERDP_TOUCH_DOWN = 0x01,
		FREERDP_TOUCH_UP = 0x02,
		FREERDP_TOUCH_MOTION = 0x04,
		FREERDP_TOUCH_CANCEL = 0x08, /** @since version 3.22.0 */
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
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_is_pen(rdpClientContext* cctx, INT32 deviceid);

	FREERDP_API BOOL freerdp_client_pen_cancel_all(rdpClientContext* cctx);

	FREERDP_API BOOL freerdp_client_send_wheel_event(rdpClientContext* cctx, UINT16 mflags);

	/** @brief this function checks if relative mouse events are supported and enabled for this
	 * session.
	 *
	 *  @param cctx The \b rdpClientContext to check
	 *
	 *  @return \b TRUE if relative mouse events are to be sent, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_use_relative_mouse_events(rdpClientContext* cctx);

	FREERDP_API BOOL freerdp_client_send_button_event(rdpClientContext* cctx, BOOL relative,
	                                                  UINT16 mflags, INT32 x, INT32 y);

	FREERDP_API BOOL freerdp_client_send_extended_button_event(rdpClientContext* cctx,
	                                                           BOOL relative, UINT16 mflags,
	                                                           INT32 x, INT32 y);

	WINPR_ATTR_NODISCARD
	FREERDP_API int freerdp_client_common_stop(rdpContext* context);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_load_channels(freerdp* instance);

#if defined(CHANNEL_ENCOMSP_CLIENT)
	FREERDP_API BOOL freerdp_client_encomsp_toggle_control(EncomspClientContext* encomsp);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_encomsp_set_control(EncomspClientContext* encomsp,
	                                                    BOOL control);
#endif

	/** @brief type of AAD request
	 *
	 *  Each type names the arguments \ref freerdp_client_get_aad_url expects after \b type.
	 *  Every one of them is a \b const \b char* and none of them may be \b nullptr.
	 *  @since version 3.16.0
	 */
	typedef enum
	{
		/** Authorization request for an AAD scope, arguments: \c scope */
		FREERDP_CLIENT_AAD_AUTH_REQUEST,
		/** Token request redeeming a \ref FREERDP_CLIENT_AAD_AUTH_REQUEST,
		 *  arguments: \c scope, \c code, \c req_cnf */
		FREERDP_CLIENT_AAD_TOKEN_REQUEST,
		/** Authorization request for Azure Virtual Desktop, no arguments */
		FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST,
		/** Token request redeeming a \ref FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST,
		 *  arguments: \c code */
		FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST,
	} freerdp_client_aad_type;

	/** @brief helper function to construct a connection URL for AAD authentication
	 *
	 *  \ref FREERDP_CLIENT_AAD_AUTH_REQUEST and \ref FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST
	 *  start a new OAuth transaction on \b cctx: a fresh \c state value and a PKCE code
	 *  verifier are generated, \c state, \c code_challenge and \c code_challenge_method are
	 *  appended to the authorization URL and the redirect URI the response has to arrive at is
	 *  remembered. The matching \ref FREERDP_CLIENT_AAD_TOKEN_REQUEST or
	 *  \ref FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST appends the \c code_verifier of that
	 *  transaction and releases it once the request was built, so a verifier is sent with
	 *  exactly one token request and a request that could not be built can be retried. A
	 *  token request built without a preceding authorization request carries no
	 *  \c code_verifier and logs a warning, so callers that construct the authorization URL
	 *  themselves keep working unchanged.
	 *
	 *  Only one OAuth transaction can be in flight per client context: a new authorization
	 *  request discards the state of the previous one, and no locking is done, so a context
	 *  must not run transactions from multiple threads at the same time.
	 *
	 *  The \c code argument of the token requests is the percent \b decoded authorization
	 *  code, as \ref freerdp_client_aad_parse_callback returns it: it is percent encoded again
	 *  when it goes into the request body. Before version 3.32.0 it was copied verbatim, so
	 *  a caller that keeps passing the value as it appears in the callback query now sends it
	 *  encoded twice.
	 *
	 *  @param cctx The client context to use
	 *  @param type The kind of request to build
	 *  @param ... The arguments of \b type, see \ref freerdp_client_aad_type
	 *  @return An allocated string that can be used to connect
	 *  @since version 3.16.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API char* freerdp_client_get_aad_url(rdpClientContext* cctx,
	                                             freerdp_client_aad_type type, ...);

	/** @brief Discard the OAuth transaction of a client context
	 *
	 *  Releases the \c state value, the PKCE code verifier and the expected redirect URI the
	 *  last authorization request generated. A token request built afterwards carries no
	 *  \c code_verifier. Called automatically when a new authorization request is built,
	 *  when the matching token request has been built and when the client context is
	 *  freed.
	 *
	 *  @param cctx The client context to reset, may be \b nullptr
	 *  @since version 3.32.0
	 */
	FREERDP_API void freerdp_client_aad_reset(rdpClientContext* cctx);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_H */
