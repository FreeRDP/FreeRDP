/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/utils/blob.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/string.h>

/* Redirection Flags */
#define LB_TARGET_NET_ADDRESS		0x00000001
#define LB_LOAD_BALANCE_INFO		0x00000002
#define LB_USERNAME			0x00000004
#define LB_DOMAIN			0x00000008
#define LB_PASSWORD			0x00000010
#define LB_DONTSTOREUSERNAME		0x00000020
#define LB_SMARTCARD_LOGON		0x00000040
#define LB_NOREDIRECT			0x00000080
#define LB_TARGET_FQDN			0x00000100
#define LB_TARGET_NETBIOS_NAME		0x00000200
#define LB_TARGET_NET_ADDRESSES		0x00000800
#define LB_CLIENT_TSV_URL		0x00001000
#define LB_SERVER_TSV_CAPABLE		0x00002000

struct rdp_redirection
{
	uint32 flags;
	uint32 sessionID;
	rdpString tsvUrl;
	rdpString username;
	rdpString domain;
	rdpBlob password_cookie;
	rdpString targetFQDN;
	rdpBlob loadBalanceInfo;
	rdpString targetNetBiosName;
	rdpString targetNetAddress;
	uint32 targetNetAddressesCount;
	rdpString* targetNetAddresses;
};
typedef struct rdp_redirection rdpRedirection;

boolean rdp_recv_redirection_packet(rdpRdp* rdp, STREAM* s);
boolean rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, STREAM* s);

rdpRedirection* redirection_new();
void redirection_free(rdpRedirection* redirection);

#ifdef WITH_DEBUG_REDIR
#define DEBUG_REDIR(fmt, ...) DEBUG_CLASS(REDIR, fmt, ## __VA_ARGS__)
#else
#define DEBUG_REDIR(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __REDIRECTION_H */
