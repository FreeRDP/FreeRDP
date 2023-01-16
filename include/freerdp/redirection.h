/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Redirection
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_REDIRECTION_H
#define FREERDP_REDIRECTION_H

#include <freerdp/api.h>

/* Redirection Flags */
#define LB_TARGET_NET_ADDRESS 0x00000001
#define LB_LOAD_BALANCE_INFO 0x00000002
#define LB_USERNAME 0x00000004
#define LB_DOMAIN 0x00000008
#define LB_PASSWORD 0x00000010
#define LB_DONTSTOREUSERNAME 0x00000020
#define LB_SMARTCARD_LOGON 0x00000040
#define LB_NOREDIRECT 0x00000080
#define LB_TARGET_FQDN 0x00000100
#define LB_TARGET_NETBIOS_NAME 0x00000200
#define LB_TARGET_NET_ADDRESSES 0x00000800
#define LB_CLIENT_TSV_URL 0x00001000
#define LB_SERVER_TSV_CAPABLE 0x00002000
#define LB_PASSWORD_IS_PK_ENCRYPTED 0x00004000
#define LB_REDIRECTION_GUID 0x00008000
#define LB_TARGET_CERTIFICATE 0x00010000

#define LB_PASSWORD_MAX_LENGTH 512

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_redirection rdpRedirection;

	FREERDP_API rdpRedirection* redirection_new(void);
	FREERDP_API void redirection_free(rdpRedirection* redirection);

	/** \brief This function checks if all necessary settings for a given \b rdpRedirection are
	 * available.
	 *
	 *  \param redirection The redirection settings to check
	 *  \param pFlags An (optional) pointer to UINT32. Is set to the flags that do not have
	 * necessary data available.
	 *
	 *  \return \b TRUE if the redirection settings are ready to use, \b FALSE otherwise.
	 */
	FREERDP_API BOOL redirection_settings_are_valid(rdpRedirection* redirection, UINT32* pFlags);

	FREERDP_API BOOL redirection_set_flags(rdpRedirection* redirection, UINT32 flags);
	FREERDP_API BOOL redirection_set_session_id(rdpRedirection* redirection, UINT32 session_id);
	FREERDP_API BOOL redirection_set_byte_option(rdpRedirection* redirection, UINT32 flag,
	                                             const BYTE* data, size_t length);
	FREERDP_API BOOL redirection_set_string_option(rdpRedirection* redirection, UINT32 flag,
	                                               const char* str);
	FREERDP_API BOOL redirection_set_array_option(rdpRedirection* redirection, UINT32 flag,
	                                              const char** str, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_REDIRECTION_H */
