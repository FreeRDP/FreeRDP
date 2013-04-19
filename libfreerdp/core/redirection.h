/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Redirection
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __REDIRECTION_H
#define __REDIRECTION_H

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/debug.h>

#include <winpr/stream.h>

struct rdp_string
{
	char* ascii;
	char* unicode;
	UINT32 length;
};
typedef struct rdp_string rdpString;

struct rdp_redirection
{
	UINT32 flags;
	UINT32 sessionID;
	rdpString tsvUrl;
	rdpString username;
	rdpString domain;
	BYTE* PasswordCookie;
	DWORD PasswordCookieLength;
	rdpString targetFQDN;
	BYTE* LoadBalanceInfo;
	DWORD LoadBalanceInfoLength;
	rdpString targetNetBiosName;
	rdpString targetNetAddress;
	UINT32 targetNetAddressesCount;
	rdpString* targetNetAddresses;
};
typedef struct rdp_redirection rdpRedirection;

BOOL rdp_recv_redirection_packet(rdpRdp* rdp, wStream* s);
BOOL rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s);

rdpRedirection* redirection_new(void);
void redirection_free(rdpRedirection* redirection);

#ifdef WITH_DEBUG_REDIR
#define DEBUG_REDIR(fmt, ...) DEBUG_CLASS(REDIR, fmt, ## __VA_ARGS__)
#else
#define DEBUG_REDIR(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __REDIRECTION_H */
