/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_SSPI_NTLM_PRIVATE_H
#define FREERDP_SSPI_NTLM_PRIVATE_H

#include <freerdp/sspi/sspi.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/utils/unicode.h>

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
	uint16 length;
	uint8* value;
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
	uint32 Flags;
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

struct _NTLM_CONTEXT
{
	boolean server;
	boolean ntlm_v2;
	NTLM_STATE state;
	UNICONV* uniconv;
	int SendSeqNum;
	int RecvSeqNum;
	CryptoRc4 SendRc4Seal;
	CryptoRc4 RecvRc4Seal;
	uint8* SendSigningKey;
	uint8* RecvSigningKey;
	uint8* SendSealingKey;
	uint8* RecvSealingKey;
	AV_PAIRS* av_pairs;
	uint32 NegotiateFlags;
	uint16* Workstation;
	uint32 WorkstationLength;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecBuffer NegotiateMessage;
	SecBuffer ChallengeMessage;
	SecBuffer AuthenticateMessage;
	SecBuffer TargetInfo;
	SecBuffer TargetName;
	SecBuffer NtChallengeResponse;
	SecBuffer LmChallengeResponse;
	uint8 Timestamp[8];
	uint8 ServerChallenge[8];
	uint8 ClientChallenge[8];
	uint8 SessionBaseKey[16];
	uint8 KeyExchangeKey[16];
	uint8 RandomSessionKey[16];
	uint8 ExportedSessionKey[16];
	uint8 EncryptedRandomSessionKey[16];
	uint8 ClientSigningKey[16];
	uint8 ClientSealingKey[16];
	uint8 ServerSigningKey[16];
	uint8 ServerSealingKey[16];
	uint8 MessageIntegrityCheck[16];
};
typedef struct _NTLM_CONTEXT NTLM_CONTEXT;

NTLM_CONTEXT* ntlm_ContextNew();
void ntlm_ContextFree(NTLM_CONTEXT* context);

#ifdef WITH_DEBUG_NLA
#define WITH_DEBUG_NTLM
#endif

#endif /* FREERDP_SSPI_NTLM_PRIVATE_H */
