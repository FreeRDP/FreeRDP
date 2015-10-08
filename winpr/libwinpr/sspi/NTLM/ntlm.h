/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/nt.h>
#include <winpr/crypto.h>

#include "../sspi.h"

#define MESSAGE_TYPE_NEGOTIATE					1
#define MESSAGE_TYPE_CHALLENGE					2
#define MESSAGE_TYPE_AUTHENTICATE				3

#define NTLMSSP_NEGOTIATE_56					0x80000000 /* W   (0) */
#define NTLMSSP_NEGOTIATE_KEY_EXCH				0x40000000 /* V   (1) */
#define NTLMSSP_NEGOTIATE_128					0x20000000 /* U   (2) */
#define NTLMSSP_RESERVED1					0x10000000 /* r1  (3) */
#define NTLMSSP_RESERVED2					0x08000000 /* r2  (4) */
#define NTLMSSP_RESERVED3					0x04000000 /* r3  (5) */
#define NTLMSSP_NEGOTIATE_VERSION				0x02000000 /* T   (6) */
#define NTLMSSP_RESERVED4					0x01000000 /* r4  (7) */
#define NTLMSSP_NEGOTIATE_TARGET_INFO				0x00800000 /* S   (8) */
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY			0x00400000 /* R   (9) */
#define NTLMSSP_RESERVED5					0x00200000 /* r5  (10) */
#define NTLMSSP_NEGOTIATE_IDENTIFY				0x00100000 /* Q   (11) */
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY		0x00080000 /* P   (12) */
#define NTLMSSP_RESERVED6					0x00040000 /* r6  (13) */
#define NTLMSSP_TARGET_TYPE_SERVER				0x00020000 /* O   (14) */
#define NTLMSSP_TARGET_TYPE_DOMAIN				0x00010000 /* N   (15) */
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN				0x00008000 /* M   (16) */
#define NTLMSSP_RESERVED7					0x00004000 /* r7  (17) */
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED			0x00002000 /* L   (18) */
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED			0x00001000 /* K   (19) */
#define NTLMSSP_NEGOTIATE_ANONYMOUS				0x00000800 /* J   (20) */
#define NTLMSSP_RESERVED8					0x00000400 /* r8  (21) */
#define NTLMSSP_NEGOTIATE_NTLM					0x00000200 /* H   (22) */
#define NTLMSSP_RESERVED9					0x00000100 /* r9  (23) */
#define NTLMSSP_NEGOTIATE_LM_KEY				0x00000080 /* G   (24) */
#define NTLMSSP_NEGOTIATE_DATAGRAM				0x00000040 /* F   (25) */
#define NTLMSSP_NEGOTIATE_SEAL					0x00000020 /* E   (26) */
#define NTLMSSP_NEGOTIATE_SIGN					0x00000010 /* D   (27) */
#define NTLMSSP_RESERVED10					0x00000008 /* r10 (28) */
#define NTLMSSP_REQUEST_TARGET					0x00000004 /* C   (29) */
#define NTLMSSP_NEGOTIATE_OEM					0x00000002 /* B   (30) */
#define NTLMSSP_NEGOTIATE_UNICODE				0x00000001 /* A   (31) */

enum _NTLM_STATE
{
	NTLM_STATE_INITIAL,
	NTLM_STATE_NEGOTIATE,
	NTLM_STATE_CHALLENGE,
	NTLM_STATE_AUTHENTICATE,
	NTLM_STATE_COMPLETION,
	NTLM_STATE_FINAL
};
typedef enum _NTLM_STATE NTLM_STATE;

enum _NTLM_AV_ID
{
	MsvAvEOL,
	MsvAvNbComputerName,
	MsvAvNbDomainName,
	MsvAvDnsComputerName,
	MsvAvDnsDomainName,
	MsvAvDnsTreeName,
	MsvAvFlags,
	MsvAvTimestamp,
	MsvAvSingleHost,
	MsvAvTargetName,
	MsvChannelBindings
};
typedef enum _NTLM_AV_ID NTLM_AV_ID;

struct _NTLM_AV_PAIR
{
	UINT16 AvId;
	UINT16 AvLen;
};
typedef struct _NTLM_AV_PAIR NTLM_AV_PAIR;

#define MSV_AV_FLAGS_AUTHENTICATION_CONSTRAINED		0x00000001
#define MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK		0x00000002
#define MSV_AV_FLAGS_TARGET_SPN_UNTRUSTED_SOURCE	0x00000004

#define WINDOWS_MAJOR_VERSION_5				0x05
#define WINDOWS_MAJOR_VERSION_6				0x06
#define WINDOWS_MINOR_VERSION_0				0x00
#define WINDOWS_MINOR_VERSION_1				0x01
#define WINDOWS_MINOR_VERSION_2				0x02
#define NTLMSSP_REVISION_W2K3				0x0F

struct _NTLM_VERSION_INFO
{
	UINT8 ProductMajorVersion;
	UINT8 ProductMinorVersion;
	UINT16 ProductBuild;
	BYTE Reserved[3];
	UINT8 NTLMRevisionCurrent;
};
typedef struct _NTLM_VERSION_INFO NTLM_VERSION_INFO;

struct _NTLM_SINGLE_HOST_DATA
{
	UINT32 Size;
	UINT32 Z4;
	UINT32 DataPresent;
	UINT32 CustomData;
	BYTE MachineID[32];
};
typedef struct _NTLM_SINGLE_HOST_DATA NTLM_SINGLE_HOST_DATA;

struct _NTLM_RESPONSE
{
	BYTE Response[24];
};
typedef struct _NTLM_RESPONSE NTLM_RESPONSE;

struct _NTLMv2_CLIENT_CHALLENGE
{
	UINT8 RespType;
	UINT8 HiRespType;
	UINT16 Reserved1;
	UINT32 Reserved2;
	BYTE Timestamp[8];
	BYTE ClientChallenge[8];
	UINT32 Reserved3;
	NTLM_AV_PAIR* AvPairs;
};
typedef struct _NTLMv2_CLIENT_CHALLENGE NTLMv2_CLIENT_CHALLENGE;

struct _NTLMv2_RESPONSE
{
	BYTE Response[16];
	NTLMv2_CLIENT_CHALLENGE Challenge;
};
typedef struct _NTLMv2_RESPONSE NTLMv2_RESPONSE;

struct _NTLM_MESSAGE_FIELDS
{
	UINT16 Len;
	UINT16 MaxLen;
	PBYTE Buffer;
	UINT32 BufferOffset;
};
typedef struct _NTLM_MESSAGE_FIELDS NTLM_MESSAGE_FIELDS;

struct _NTLM_MESSAGE_HEADER
{
	BYTE Signature[8];
	UINT32 MessageType;
};
typedef struct _NTLM_MESSAGE_HEADER NTLM_MESSAGE_HEADER;

struct _NTLM_NEGOTIATE_MESSAGE
{
	BYTE Signature[8];
	UINT32 MessageType;
	UINT32 NegotiateFlags;
	NTLM_VERSION_INFO Version;
	NTLM_MESSAGE_FIELDS DomainName;
	NTLM_MESSAGE_FIELDS Workstation;
};
typedef struct _NTLM_NEGOTIATE_MESSAGE NTLM_NEGOTIATE_MESSAGE;

struct _NTLM_CHALLENGE_MESSAGE
{
	BYTE Signature[8];
	UINT32 MessageType;
	UINT32 NegotiateFlags;
	BYTE ServerChallenge[8];
	BYTE Reserved[8];
	NTLM_VERSION_INFO Version;
	NTLM_MESSAGE_FIELDS TargetName;
	NTLM_MESSAGE_FIELDS TargetInfo;
};
typedef struct _NTLM_CHALLENGE_MESSAGE NTLM_CHALLENGE_MESSAGE;

struct _NTLM_AUTHENTICATE_MESSAGE
{
	BYTE Signature[8];
	UINT32 MessageType;
	UINT32 NegotiateFlags;
	NTLM_VERSION_INFO Version;
	NTLM_MESSAGE_FIELDS DomainName;
	NTLM_MESSAGE_FIELDS UserName;
	NTLM_MESSAGE_FIELDS Workstation;
	NTLM_MESSAGE_FIELDS LmChallengeResponse;
	NTLM_MESSAGE_FIELDS NtChallengeResponse;
	NTLM_MESSAGE_FIELDS EncryptedRandomSessionKey;
	BYTE MessageIntegrityCheck[16];
};
typedef struct _NTLM_AUTHENTICATE_MESSAGE NTLM_AUTHENTICATE_MESSAGE;

struct _NTLM_CONTEXT
{
	BOOL server;
	BOOL NTLMv2;
	BOOL UseMIC;
	NTLM_STATE state;
	int SendSeqNum;
	int RecvSeqNum;
	BYTE NtlmHash[16];
	BYTE NtlmV2Hash[16];
	BYTE MachineID[32];
	BOOL SendVersionInfo;
	BOOL confidentiality;
	WINPR_RC4_CTX SendRc4Seal;
	WINPR_RC4_CTX RecvRc4Seal;
	BYTE* SendSigningKey;
	BYTE* RecvSigningKey;
	BYTE* SendSealingKey;
	BYTE* RecvSealingKey;
	UINT32 NegotiateFlags;
	BOOL UseSamFileDatabase;
	int LmCompatibilityLevel;
	int SuppressExtendedProtection;
	BOOL SendWorkstationName;
	UNICODE_STRING Workstation;
	UNICODE_STRING ServicePrincipalName;
	SSPI_CREDENTIALS* credentials;
	BYTE* ChannelBindingToken;
	BYTE ChannelBindingsHash[16];
	SecPkgContext_Bindings Bindings;
	BOOL SendSingleHostData;
	BOOL NegotiateKeyExchange;
	NTLM_SINGLE_HOST_DATA SingleHostData;
	NTLM_NEGOTIATE_MESSAGE NEGOTIATE_MESSAGE;
	NTLM_CHALLENGE_MESSAGE CHALLENGE_MESSAGE;
	NTLM_AUTHENTICATE_MESSAGE AUTHENTICATE_MESSAGE;
	SecBuffer NegotiateMessage;
	SecBuffer ChallengeMessage;
	SecBuffer AuthenticateMessage;
	SecBuffer ChallengeTargetInfo;
	SecBuffer AuthenticateTargetInfo;
	SecBuffer TargetName;
	SecBuffer NtChallengeResponse;
	SecBuffer LmChallengeResponse;
	NTLMv2_RESPONSE NTLMv2Response;
	BYTE Timestamp[8];
	BYTE ChallengeTimestamp[8];
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
	UINT32 MessageIntegrityCheckOffset;
};
typedef struct _NTLM_CONTEXT NTLM_CONTEXT;

NTLM_CONTEXT* ntlm_ContextNew(void);
void ntlm_ContextFree(NTLM_CONTEXT* context);

#ifdef WITH_DEBUG_NLA
#define WITH_DEBUG_NTLM
#endif

#endif /* FREERDP_SSPI_NTLM_PRIVATE_H */
