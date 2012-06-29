/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_NTLM_PRIVATE_H
#define WINPR_SSPI_NTLM_PRIVATE_H

#include <winpr/sspi.h>
#include <winpr/windows.h>

#include <time.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>

#include "../sspi.h"

enum _NTLM_STATE
{
	NTLM_STATE_INITIAL,
	NTLM_STATE_NEGOTIATE,
	NTLM_STATE_CHALLENGE,
	NTLM_STATE_AUTHENTICATE,
	NTLM_STATE_FINAL
};
typedef enum _NTLM_STATE NTLM_STATE;

struct _AV_PAIR
{
	UINT16 length;
	BYTE* value;
};
typedef struct _AV_PAIR AV_PAIR;

struct _AV_PAIRS
{
	AV_PAIR NbComputerName;
	AV_PAIR NbDomainName;
	AV_PAIR DnsComputerName;
	AV_PAIR DnsDomainName;
	AV_PAIR DnsTreeName;
	AV_PAIR Timestamp;
	AV_PAIR Restrictions;
	AV_PAIR TargetName;
	AV_PAIR ChannelBindings;
	UINT32 Flags;
};
typedef struct _AV_PAIRS AV_PAIRS;

enum _AV_ID
{
	MsvAvEOL,
	MsvAvNbComputerName,
	MsvAvNbDomainName,
	MsvAvDnsComputerName,
	MsvAvDnsDomainName,
	MsvAvDnsTreeName,
	MsvAvFlags,
	MsvAvTimestamp,
	MsvAvRestrictions,
	MsvAvTargetName,
	MsvChannelBindings
};
typedef enum _AV_ID AV_ID;

struct _NTLM_MESSAGE_FIELD
{
	UINT16 Len;
	UINT16 MaxLen;
	PBYTE Buffer;
	UINT32 BufferOffset;
};
typedef struct _NTLM_MESSAGE_FIELD NTLM_MESSAGE_FIELD;

struct _NTLM_CONTEXT
{
	BOOL server;
	BOOL ntlm_v2;
	NTLM_STATE state;
	int SendSeqNum;
	int RecvSeqNum;
	int SendVersionInfo;
	BOOL confidentiality;
	RC4_KEY SendRc4Seal;
	RC4_KEY RecvRc4Seal;
	BYTE* SendSigningKey;
	BYTE* RecvSigningKey;
	BYTE* SendSealingKey;
	BYTE* RecvSealingKey;
	AV_PAIRS* av_pairs;
	UINT32 NegotiateFlags;
	UINT16* Workstation;
	UINT32 WorkstationLength;
	int LmCompatibilityLevel;
	int SuppressExtendedProtection;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecBuffer NegotiateMessage;
	SecBuffer ChallengeMessage;
	SecBuffer AuthenticateMessage;
	SecBuffer TargetInfo;
	SecBuffer TargetName;
	SecBuffer NtChallengeResponse;
	SecBuffer LmChallengeResponse;
	BYTE Timestamp[8];
	BYTE ServerChallenge[8];
	BYTE ClientChallenge[8];
	BYTE SessionBaseKey[16];
	BYTE KeyExchangeKey[16];
	BYTE RandomSessionKey[16];
	BYTE ExportedSessionKey[16];
	BYTE EncryptedRandomSessionKey[16];
	BYTE ClientSigningKey[16];
	BYTE ClientSealingKey[16];
	BYTE ServerSigningKey[16];
	BYTE ServerSealingKey[16];
	BYTE MessageIntegrityCheck[16];
};
typedef struct _NTLM_CONTEXT NTLM_CONTEXT;

NTLM_CONTEXT* ntlm_ContextNew();
void ntlm_ContextFree(NTLM_CONTEXT* context);

#ifdef WITH_DEBUG_NLA
#define WITH_DEBUG_NTLM
#endif

#endif /* FREERDP_SSPI_NTLM_PRIVATE_H */
