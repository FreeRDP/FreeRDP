/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Message)
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

#include <winpr/config.h>

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_message.h"

#include "../../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

static const char NTLM_SIGNATURE[8] = { 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0' };

static void ntlm_free_message_fields_buffer(NTLM_MESSAGE_FIELDS* fields);

const char* ntlm_get_negotiate_string(UINT32 flag)
{
	if (flag & NTLMSSP_NEGOTIATE_56)
		return "NTLMSSP_NEGOTIATE_56";
	if (flag & NTLMSSP_NEGOTIATE_KEY_EXCH)
		return "NTLMSSP_NEGOTIATE_KEY_EXCH";
	if (flag & NTLMSSP_NEGOTIATE_128)
		return "NTLMSSP_NEGOTIATE_128";
	if (flag & NTLMSSP_RESERVED1)
		return "NTLMSSP_RESERVED1";
	if (flag & NTLMSSP_RESERVED2)
		return "NTLMSSP_RESERVED2";
	if (flag & NTLMSSP_RESERVED3)
		return "NTLMSSP_RESERVED3";
	if (flag & NTLMSSP_NEGOTIATE_VERSION)
		return "NTLMSSP_NEGOTIATE_VERSION";
	if (flag & NTLMSSP_RESERVED4)
		return "NTLMSSP_RESERVED4";
	if (flag & NTLMSSP_NEGOTIATE_TARGET_INFO)
		return "NTLMSSP_NEGOTIATE_TARGET_INFO";
	if (flag & NTLMSSP_REQUEST_NON_NT_SESSION_KEY)
		return "NTLMSSP_REQUEST_NON_NT_SESSION_KEY";
	if (flag & NTLMSSP_RESERVED5)
		return "NTLMSSP_RESERVED5";
	if (flag & NTLMSSP_NEGOTIATE_IDENTIFY)
		return "NTLMSSP_NEGOTIATE_IDENTIFY";
	if (flag & NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY)
		return "NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY";
	if (flag & NTLMSSP_RESERVED6)
		return "NTLMSSP_RESERVED6";
	if (flag & NTLMSSP_TARGET_TYPE_SERVER)
		return "NTLMSSP_TARGET_TYPE_SERVER";
	if (flag & NTLMSSP_TARGET_TYPE_DOMAIN)
		return "NTLMSSP_TARGET_TYPE_DOMAIN";
	if (flag & NTLMSSP_NEGOTIATE_ALWAYS_SIGN)
		return "NTLMSSP_NEGOTIATE_ALWAYS_SIGN";
	if (flag & NTLMSSP_RESERVED7)
		return "NTLMSSP_RESERVED7";
	if (flag & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
		return "NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED";
	if (flag & NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED)
		return "NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED";
	if (flag & NTLMSSP_NEGOTIATE_ANONYMOUS)
		return "NTLMSSP_NEGOTIATE_ANONYMOUS";
	if (flag & NTLMSSP_RESERVED8)
		return "NTLMSSP_RESERVED8";
	if (flag & NTLMSSP_NEGOTIATE_NTLM)
		return "NTLMSSP_NEGOTIATE_NTLM";
	if (flag & NTLMSSP_RESERVED9)
		return "NTLMSSP_RESERVED9";
	if (flag & NTLMSSP_NEGOTIATE_LM_KEY)
		return "NTLMSSP_NEGOTIATE_LM_KEY";
	if (flag & NTLMSSP_NEGOTIATE_DATAGRAM)
		return "NTLMSSP_NEGOTIATE_DATAGRAM";
	if (flag & NTLMSSP_NEGOTIATE_SEAL)
		return "NTLMSSP_NEGOTIATE_SEAL";
	if (flag & NTLMSSP_NEGOTIATE_SIGN)
		return "NTLMSSP_NEGOTIATE_SIGN";
	if (flag & NTLMSSP_RESERVED10)
		return "NTLMSSP_RESERVED10";
	if (flag & NTLMSSP_REQUEST_TARGET)
		return "NTLMSSP_REQUEST_TARGET";
	if (flag & NTLMSSP_NEGOTIATE_OEM)
		return "NTLMSSP_NEGOTIATE_OEM";
	if (flag & NTLMSSP_NEGOTIATE_UNICODE)
		return "NTLMSSP_NEGOTIATE_UNICODE";
	return "NTLMSSP_NEGOTIATE_UNKNOWN";
}

#if defined(WITH_DEBUG_NTLM)
static void ntlm_print_message_fields(const NTLM_MESSAGE_FIELDS* fields, const char* name)
{
	WINPR_ASSERT(fields);
	WINPR_ASSERT(name);

	WLog_VRB(TAG, "%s (Len: %" PRIu16 " MaxLen: %" PRIu16 " BufferOffset: %" PRIu32 ")", name,
	         fields->Len, fields->MaxLen, fields->BufferOffset);

	if (fields->Len > 0)
		winpr_HexDump(TAG, WLOG_TRACE, fields->Buffer, fields->Len);
}

static void ntlm_print_negotiate_flags(UINT32 flags)
{
	int i;

	WLog_VRB(TAG, "negotiateFlags \"0x%08" PRIX32 "\"", flags);

	for (i = 31; i >= 0; i--)
	{
		if ((flags >> i) & 1)
		{
			const char* str = ntlm_get_negotiate_string(1 << i);
			WLog_VRB(TAG, "\t%s (%d),", str, (31 - i));
		}
	}
}

static void ntlm_print_negotiate_message(const SecBuffer* NegotiateMessage,
                                         const NTLM_NEGOTIATE_MESSAGE* message)
{
	WINPR_ASSERT(NegotiateMessage);
	WINPR_ASSERT(message);

	WLog_VRB(TAG, "NEGOTIATE_MESSAGE (length = %" PRIu32 ")", NegotiateMessage->cbBuffer);
	winpr_HexDump(TAG, WLOG_TRACE, NegotiateMessage->pvBuffer, NegotiateMessage->cbBuffer);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));
}

static void ntlm_print_challenge_message(const SecBuffer* ChallengeMessage,
                                         const NTLM_CHALLENGE_MESSAGE* message,
                                         const SecBuffer* ChallengeTargetInfo)
{
	WINPR_ASSERT(ChallengeMessage);
	WINPR_ASSERT(message);

	WLog_VRB(TAG, "CHALLENGE_MESSAGE (length = %" PRIu32 ")", ChallengeMessage->cbBuffer);
	winpr_HexDump(TAG, WLOG_TRACE, ChallengeMessage->pvBuffer, ChallengeMessage->cbBuffer);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->TargetName), "TargetName");
	ntlm_print_message_fields(&(message->TargetInfo), "TargetInfo");

	if (ChallengeTargetInfo && (ChallengeTargetInfo->cbBuffer > 0))
	{
		WLog_VRB(TAG, "ChallengeTargetInfo (%" PRIu32 "):", ChallengeTargetInfo->cbBuffer);
		ntlm_print_av_pair_list(ChallengeTargetInfo->pvBuffer, ChallengeTargetInfo->cbBuffer);
	}
}

static void ntlm_print_authenticate_message(const SecBuffer* AuthenticateMessage,
                                            const NTLM_AUTHENTICATE_MESSAGE* message, UINT32 flags,
                                            const SecBuffer* AuthenticateTargetInfo)
{
	WINPR_ASSERT(AuthenticateMessage);
	WINPR_ASSERT(message);

	WLog_VRB(TAG, "AUTHENTICATE_MESSAGE (length = %" PRIu32 ")", AuthenticateMessage->cbBuffer);
	winpr_HexDump(TAG, WLOG_TRACE, AuthenticateMessage->pvBuffer, AuthenticateMessage->cbBuffer);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	if (AuthenticateTargetInfo && (AuthenticateTargetInfo->cbBuffer > 0))
	{
		WLog_VRB(TAG, "AuthenticateTargetInfo (%" PRIu32 "):", AuthenticateTargetInfo->cbBuffer);
		ntlm_print_av_pair_list(AuthenticateTargetInfo->pvBuffer, AuthenticateTargetInfo->cbBuffer);
	}

	ntlm_print_message_fields(&(message->DomainName), "DomainName");
	ntlm_print_message_fields(&(message->UserName), "UserName");
	ntlm_print_message_fields(&(message->Workstation), "Workstation");
	ntlm_print_message_fields(&(message->LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message->NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message->EncryptedRandomSessionKey), "EncryptedRandomSessionKey");

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		WLog_VRB(TAG, "MessageIntegrityCheck (length = 16)");
		winpr_HexDump(TAG, WLOG_TRACE, message->MessageIntegrityCheck,
		              sizeof(message->MessageIntegrityCheck));
	}
}

static void ntlm_print_authentication_complete(const NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	WLog_VRB(TAG, "ClientChallenge");
	winpr_HexDump(TAG, WLOG_TRACE, context->ClientChallenge, 8);
	WLog_VRB(TAG, "ServerChallenge");
	winpr_HexDump(TAG, WLOG_TRACE, context->ServerChallenge, 8);
	WLog_VRB(TAG, "SessionBaseKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->SessionBaseKey, 16);
	WLog_VRB(TAG, "KeyExchangeKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->KeyExchangeKey, 16);
	WLog_VRB(TAG, "ExportedSessionKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->ExportedSessionKey, 16);
	WLog_VRB(TAG, "RandomSessionKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->RandomSessionKey, 16);
	WLog_VRB(TAG, "ClientSigningKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->ClientSigningKey, 16);
	WLog_VRB(TAG, "ClientSealingKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->ClientSealingKey, 16);
	WLog_VRB(TAG, "ServerSigningKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->ServerSigningKey, 16);
	WLog_VRB(TAG, "ServerSealingKey");
	winpr_HexDump(TAG, WLOG_TRACE, context->ServerSealingKey, 16);
	WLog_VRB(TAG, "Timestamp");
	winpr_HexDump(TAG, WLOG_TRACE, context->Timestamp, 8);
}
#endif

static BOOL ntlm_read_message_header(wStream* s, NTLM_MESSAGE_HEADER* header, UINT32 expected)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	Stream_Read(s, header->Signature, 8);
	Stream_Read_UINT32(s, header->MessageType);

	if (strncmp((char*)header->Signature, NTLM_SIGNATURE, 8) != 0)
	{
		WLog_ERR(TAG, "NTLM_MESSAGE_HEADER Invalid signature, got %s, expected %s",
		         header->Signature, NTLM_SIGNATURE);
		return FALSE;
	}

	if (header->MessageType != expected)
	{
		WLog_ERR(TAG, "NTLM_MESSAGE_HEADER Invalid message tyep, got %s, expected %s",
		         ntlm_message_type_string(header->MessageType), ntlm_message_type_string(expected));
		return FALSE;
	}

	return TRUE;
}

static BOOL ntlm_write_message_header(wStream* s, const NTLM_MESSAGE_HEADER* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (Stream_GetRemainingCapacity(s) < sizeof(NTLM_SIGNATURE) + 4)
	{
		WLog_ERR(TAG, "Short NTLM_MESSAGE_HEADER::header %" PRIuz ", expected 12",
		         Stream_GetRemainingCapacity(s));
		return FALSE;
	}

	Stream_Write(s, header->Signature, sizeof(NTLM_SIGNATURE));
	Stream_Write_UINT32(s, header->MessageType);

	return TRUE;
}

static BOOL ntlm_populate_message_header(NTLM_MESSAGE_HEADER* header, UINT32 MessageType)
{
	WINPR_ASSERT(header);

	CopyMemory(header->Signature, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));
	header->MessageType = MessageType;
	return TRUE;
}

static BOOL ntlm_read_message_fields(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(fields);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	ntlm_free_message_fields_buffer(fields);

	Stream_Read_UINT16(s, fields->Len);          /* Len (2 bytes) */
	Stream_Read_UINT16(s, fields->MaxLen);       /* MaxLen (2 bytes) */
	Stream_Read_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
	return TRUE;
}

static BOOL ntlm_write_message_fields(wStream* s, const NTLM_MESSAGE_FIELDS* fields)
{
	UINT16 MaxLen;
	WINPR_ASSERT(s);
	WINPR_ASSERT(fields);

	MaxLen = fields->MaxLen;
	if (fields->MaxLen < 1)
		MaxLen = fields->Len;

	if (Stream_GetRemainingCapacity(s) < 8)
	{
		WLog_ERR(TAG, "Short NTLM_MESSAGE_FIELDS::header %" PRIuz ", expected %" PRIuz,
		         Stream_GetRemainingCapacity(s), 8);
		return FALSE;
	}
	Stream_Write_UINT16(s, fields->Len);          /* Len (2 bytes) */
	Stream_Write_UINT16(s, MaxLen);               /* MaxLen (2 bytes) */
	Stream_Write_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
	return TRUE;
}

static BOOL ntlm_read_message_fields_buffer(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(fields);

	if (fields->Len > 0)
	{
		const UINT32 offset = fields->BufferOffset + fields->Len;

		if (fields->BufferOffset > UINT32_MAX - fields->Len)
		{
			WLog_ERR(TAG,
			         "NTLM_MESSAGE_FIELDS::BufferOffset %" PRIu32
			         " too large, maximum allowed is %" PRIu32,
			         fields->BufferOffset, UINT32_MAX - fields->Len);
			return FALSE;
		}

		if (offset > Stream_Length(s))
		{
			WLog_ERR(TAG,
			         "NTLM_MESSAGE_FIELDS::Buffer offset %" PRIu32 " beyond received data %" PRIuz,
			         offset, Stream_Length(s));
			return FALSE;
		}

		fields->Buffer = (PBYTE)malloc(fields->Len);

		if (!fields->Buffer)
		{
			WLog_ERR(TAG, "NTLM_MESSAGE_FIELDS::Buffer allocation of %" PRIu16 "bytes failed",
			         fields->Len);
			return FALSE;
		}

		Stream_SetPosition(s, fields->BufferOffset);
		Stream_Read(s, fields->Buffer, fields->Len);
	}

	return TRUE;
}

static BOOL ntlm_write_message_fields_buffer(wStream* s, const NTLM_MESSAGE_FIELDS* fields)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(fields);

	if (fields->Len > 0)
	{
		Stream_SetPosition(s, fields->BufferOffset);
		if (Stream_GetRemainingCapacity(s) < fields->Len)
		{
			WLog_ERR(TAG, "Short NTLM_MESSAGE_FIELDS::Len %" PRIuz ", expected %" PRIu16,
			         Stream_GetRemainingCapacity(s), fields->Len);
			return FALSE;
		}
		Stream_Write(s, fields->Buffer, fields->Len);
	}
	return TRUE;
}

void ntlm_free_message_fields_buffer(NTLM_MESSAGE_FIELDS* fields)
{
	if (fields)
	{
		if (fields->Buffer)
		{
			free(fields->Buffer);
			fields->Len = 0;
			fields->MaxLen = 0;
			fields->Buffer = NULL;
			fields->BufferOffset = 0;
		}
	}
}

static BOOL ntlm_read_negotiate_flags(wStream* s, UINT32* flags, UINT32 required, const char* name)
{
	UINT32 NegotiateFlags = 0;
	char buffer[1024] = { 0 };
	WINPR_ASSERT(s);
	WINPR_ASSERT(flags);
	WINPR_ASSERT(name);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if ((NegotiateFlags & required) != required)
	{
		WLog_ERR(TAG, "%s::NegotiateFlags invalid flags 0x08%" PRIx32 ", 0x%08" PRIx32 " required",
		         name, NegotiateFlags, required);
		return FALSE;
	}

	WLog_DBG(TAG, "Read flags %s",
	         ntlm_negotiate_flags_string(buffer, ARRAYSIZE(buffer), NegotiateFlags));
	*flags = NegotiateFlags;
	return TRUE;
}

static BOOL ntlm_write_negotiate_flags(wStream* s, UINT32 flags, const char* name)
{
	char buffer[1024] = { 0 };
	WINPR_ASSERT(s);
	WINPR_ASSERT(name);

	if (Stream_GetRemainingCapacity(s) < 4)
	{
		WLog_ERR(TAG, "%s::NegotiateFlags expected 4bytes, have %" PRIuz "bytes", name,
		         Stream_GetRemainingCapacity(s));
		return FALSE;
	}

	WLog_DBG(TAG, "Write flags %s", ntlm_negotiate_flags_string(buffer, ARRAYSIZE(buffer), flags));
	Stream_Write_UINT32(s, flags); /* NegotiateFlags (4 bytes) */
	return TRUE;
}

static BOOL ntlm_read_message_integrity_check(wStream* s, size_t* offset, BYTE* data, size_t size,
                                              const char* name)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(offset);
	WINPR_ASSERT(data);
	WINPR_ASSERT(size == WINPR_MD5_DIGEST_LENGTH);
	WINPR_ASSERT(name);

	*offset = Stream_GetPosition(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, size))
		return FALSE;

	Stream_Read(s, data, size);
	return TRUE;
}

static BOOL ntlm_write_message_integrity_check(wStream* s, size_t offset, const BYTE* data,
                                               size_t size, const char* name)
{
	size_t pos;

	WINPR_ASSERT(s);
	WINPR_ASSERT(data);
	WINPR_ASSERT(size == WINPR_MD5_DIGEST_LENGTH);
	WINPR_ASSERT(name);

	pos = Stream_GetPosition(s);

	if (offset + size > Stream_Capacity(s))
	{
		WLog_ERR(TAG,
		         "%s::MessageIntegrityCheck invalid offset[length] %" PRIuz "[%" PRIuz
		         "], got %" PRIuz,
		         name, offset, size, Stream_GetRemainingCapacity(s));
		return FALSE;
	}
	Stream_SetPosition(s, offset);
	if (Stream_GetRemainingCapacity(s) < size)
	{
		WLog_ERR(TAG, "%s::MessageIntegrityCheck expected %" PRIuz "bytes, got %" PRIuz "bytes",
		         name, size, Stream_GetRemainingCapacity(s));
		return FALSE;
	}

	Stream_Write(s, data, size);
	Stream_SetPosition(s, pos);
	return TRUE;
}

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream sbuffer;
	wStream* s;
	size_t length;
	const NTLM_NEGOTIATE_MESSAGE empty = { 0 };
	NTLM_NEGOTIATE_MESSAGE* message;

	WINPR_ASSERT(context);
	WINPR_ASSERT(buffer);

	message = &context->NEGOTIATE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticConstInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (!ntlm_read_message_header(s, &message->header, MESSAGE_TYPE_NEGOTIATE))
		return SEC_E_INVALID_TOKEN;

	if (!ntlm_read_negotiate_flags(s, &message->NegotiateFlags,
	                               NTLMSSP_REQUEST_TARGET | NTLMSSP_NEGOTIATE_NTLM |
	                                   NTLMSSP_NEGOTIATE_UNICODE,
	                               "NTLM_NEGOTIATE_MESSAGE"))
		return SEC_E_INVALID_TOKEN;

	context->NegotiateFlags = message->NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */
	// if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED)
	{
		if (!ntlm_read_message_fields(s, &(message->DomainName))) /* DomainNameFields (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */
	// if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
	{
		if (!ntlm_read_message_fields(s, &(message->Workstation))) /* WorkstationFields (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_read_version_info(s, &(message->Version))) /* Version (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	if (!ntlm_read_message_fields_buffer(s, &message->DomainName))
		return SEC_E_INVALID_TOKEN;

	if (!ntlm_read_message_fields_buffer(s, &message->Workstation))
		return SEC_E_INVALID_TOKEN;

	length = Stream_GetPosition(s);
	WINPR_ASSERT(length <= ULONG_MAX);
	buffer->cbBuffer = (ULONG)length;

	if (!sspi_SecBufferAlloc(&context->NegotiateMessage, (ULONG)length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_negotiate_message(&context->NegotiateMessage, message);
#endif
	ntlm_change_state(context, NTLM_STATE_CHALLENGE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream sbuffer;
	wStream* s;
	size_t length;
	const NTLM_NEGOTIATE_MESSAGE empty = { 0 };
	NTLM_NEGOTIATE_MESSAGE* message;

	WINPR_ASSERT(context);
	WINPR_ASSERT(buffer);

	message = &context->NEGOTIATE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (!ntlm_populate_message_header(&message->header, MESSAGE_TYPE_NEGOTIATE))
		return SEC_E_INTERNAL_ERROR;

	if (context->NTLMv2)
	{
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_56;
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
	}

	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
	message->NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;

	if (context->confidentiality)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->SendVersionInfo)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_get_version_info(&(message->Version));

	context->NegotiateFlags = message->NegotiateFlags;
	/* Message Header (12 bytes) */
	if (!ntlm_write_message_header(s, &message->header))
		return SEC_E_INTERNAL_ERROR;

	if (!ntlm_write_negotiate_flags(s, message->NegotiateFlags, "NTLM_NEGOTIATE_MESSAGE"))
		return SEC_E_INTERNAL_ERROR;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */
	/* DomainNameFields (8 bytes) */
	if (!ntlm_write_message_fields(s, &(message->DomainName)))
		return SEC_E_INTERNAL_ERROR;

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */
	/* WorkstationFields (8 bytes) */
	if (!ntlm_write_message_fields(s, &(message->Workstation)))
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_write_version_info(s, &(message->Version)))
			return SEC_E_INTERNAL_ERROR;
	}

	length = Stream_GetPosition(s);
	WINPR_ASSERT(length <= ULONG_MAX);
	buffer->cbBuffer = (ULONG)length;

	if (!sspi_SecBufferAlloc(&context->NegotiateMessage, (ULONG)length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_negotiate_message(&context->NegotiateMessage, message);
#endif
	ntlm_change_state(context, NTLM_STATE_CHALLENGE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;
	wStream sbuffer;
	wStream* s;
	size_t length;
	size_t StartOffset;
	size_t PayloadOffset;
	NTLM_AV_PAIR* AvTimestamp;
	const NTLM_CHALLENGE_MESSAGE empty = { 0 };
	NTLM_CHALLENGE_MESSAGE* message;

	if (!context || !buffer)
		return SEC_E_INTERNAL_ERROR;

	ntlm_generate_client_challenge(context);
	message = &context->CHALLENGE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticConstInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	StartOffset = Stream_GetPosition(s);

	if (!ntlm_read_message_header(s, &message->header, MESSAGE_TYPE_CHALLENGE))
		goto fail;

	if (!ntlm_read_message_fields(s, &(message->TargetName))) /* TargetNameFields (8 bytes) */
		goto fail;

	if (!ntlm_read_negotiate_flags(s, &message->NegotiateFlags, 0, "NTLM_CHALLENGE_MESSAGE"))
		goto fail;

	context->NegotiateFlags = message->NegotiateFlags;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		goto fail;

	Stream_Read(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	CopyMemory(context->ServerChallenge, message->ServerChallenge, 8);
	Stream_Read(s, message->Reserved, 8); /* Reserved (8 bytes), should be ignored */

	if (!ntlm_read_message_fields(s, &(message->TargetInfo))) /* TargetInfoFields (8 bytes) */
		goto fail;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_read_version_info(s, &(message->Version))) /* Version (8 bytes) */
			goto fail;
	}

	/* Payload (variable) */
	PayloadOffset = Stream_GetPosition(s);

	status = SEC_E_INTERNAL_ERROR;
	if (message->TargetName.Len > 0)
	{
		if (!ntlm_read_message_fields_buffer(s, &(message->TargetName)))
			goto fail;
	}

	if (message->TargetInfo.Len > 0)
	{
		size_t cbAvTimestamp;

		if (!ntlm_read_message_fields_buffer(s, &(message->TargetInfo)))
			goto fail;

		context->ChallengeTargetInfo.pvBuffer = message->TargetInfo.Buffer;
		context->ChallengeTargetInfo.cbBuffer = message->TargetInfo.Len;
		AvTimestamp = ntlm_av_pair_get((NTLM_AV_PAIR*)message->TargetInfo.Buffer,
		                               message->TargetInfo.Len, MsvAvTimestamp, &cbAvTimestamp);

		if (AvTimestamp)
		{
			PBYTE ptr = ntlm_av_pair_get_value_pointer(AvTimestamp);

			if (!ptr)
				goto fail;

			if (context->NTLMv2)
				context->UseMIC = TRUE;

			CopyMemory(context->ChallengeTimestamp, ptr, 8);
		}
	}

	length = (PayloadOffset - StartOffset) + message->TargetName.Len + message->TargetInfo.Len;
	if (length > buffer->cbBuffer)
		goto fail;

	if (!sspi_SecBufferAlloc(&context->ChallengeMessage, (ULONG)length))
		goto fail;

	if (context->ChallengeMessage.pvBuffer)
		CopyMemory(context->ChallengeMessage.pvBuffer, Stream_Buffer(s) + StartOffset, length);
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_challenge_message(&context->ChallengeMessage, message, NULL);
#endif
	/* AV_PAIRs */

	if (context->NTLMv2)
	{
		if (!ntlm_construct_authenticate_target_info(context))
			goto fail;

		sspi_SecBufferFree(&context->ChallengeTargetInfo);
		context->ChallengeTargetInfo.pvBuffer = context->AuthenticateTargetInfo.pvBuffer;
		context->ChallengeTargetInfo.cbBuffer = context->AuthenticateTargetInfo.cbBuffer;
	}

	ntlm_generate_timestamp(context); /* Timestamp */

	if (!ntlm_compute_lm_v2_response(context)) /* LmChallengeResponse */
		goto fail;

	if (!ntlm_compute_ntlm_v2_response(context)) /* NtChallengeResponse */
		goto fail;

	ntlm_generate_key_exchange_key(context);     /* KeyExchangeKey */
	ntlm_generate_random_session_key(context);   /* RandomSessionKey */
	ntlm_generate_exported_session_key(context); /* ExportedSessionKey */
	ntlm_encrypt_random_session_key(context);    /* EncryptedRandomSessionKey */
	/* Generate signing keys */
	if (!ntlm_generate_client_signing_key(context))
		goto fail;
	if (!ntlm_generate_server_signing_key(context))
		goto fail;
	/* Generate sealing keys */
	if (!ntlm_generate_client_sealing_key(context))
		goto fail;
	if (!ntlm_generate_server_sealing_key(context))
		goto fail;
	/* Initialize RC4 seal state using client sealing key */
	ntlm_init_rc4_seal_states(context);
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_authentication_complete(context);
#endif
	ntlm_change_state(context, NTLM_STATE_AUTHENTICATE);
	status = SEC_I_CONTINUE_NEEDED;
fail:
	ntlm_free_message_fields_buffer(&(message->TargetName));
	return status;
}

SECURITY_STATUS ntlm_write_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream sbuffer;
	wStream* s;
	size_t length;
	UINT32 PayloadOffset;
	const NTLM_CHALLENGE_MESSAGE empty = { 0 };
	NTLM_CHALLENGE_MESSAGE* message;

	WINPR_ASSERT(context);
	WINPR_ASSERT(buffer);

	message = &context->CHALLENGE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	ntlm_get_version_info(&(message->Version)); /* Version */
	ntlm_generate_server_challenge(context);    /* Server Challenge */
	ntlm_generate_timestamp(context);           /* Timestamp */

	if (!ntlm_construct_challenge_target_info(context)) /* TargetInfo */
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(message->ServerChallenge, context->ServerChallenge, 8); /* ServerChallenge */
	message->NegotiateFlags = context->NegotiateFlags;
	if (!ntlm_populate_message_header(&message->header, MESSAGE_TYPE_CHALLENGE))
		return SEC_E_INTERNAL_ERROR;

	/* Message Header (12 bytes) */
	if (!ntlm_write_message_header(s, &message->header))
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		message->TargetName.Len = (UINT16)context->TargetName.cbBuffer;
		message->TargetName.Buffer = (PBYTE)context->TargetName.pvBuffer;
	}

	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		message->TargetInfo.Len = (UINT16)context->ChallengeTargetInfo.cbBuffer;
		message->TargetInfo.Buffer = (PBYTE)context->ChallengeTargetInfo.pvBuffer;
	}

	PayloadOffset = 48;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadOffset += 8;

	message->TargetName.BufferOffset = PayloadOffset;
	message->TargetInfo.BufferOffset = message->TargetName.BufferOffset + message->TargetName.Len;
	/* TargetNameFields (8 bytes) */
	if (!ntlm_write_message_fields(s, &(message->TargetName)))
		return SEC_E_INTERNAL_ERROR;

	if (!ntlm_write_negotiate_flags(s, message->NegotiateFlags, "NTLM_CHALLENGE_MESSAGE"))
		return SEC_E_INTERNAL_ERROR;

	if (Stream_GetRemainingCapacity(s) < 16)
	{
		WLog_ERR(TAG,
		         "NTLM_CHALLENGE_MESSAGE::ServerChallenge expected 16bytes, got %" PRIuz "bytes",
		         Stream_GetRemainingCapacity(s));
		return SEC_E_INTERNAL_ERROR;
	}

	Stream_Write(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	Stream_Write(s, message->Reserved, 8);        /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	if (!ntlm_write_message_fields(s, &(message->TargetInfo)))
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_write_version_info(s, &(message->Version))) /* Version (8 bytes) */
			return SEC_E_INTERNAL_ERROR;
	}

	/* Payload (variable) */
	if (message->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		if (!ntlm_write_message_fields_buffer(s, &(message->TargetName)))
			return SEC_E_INTERNAL_ERROR;
	}

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		if (!ntlm_write_message_fields_buffer(s, &(message->TargetInfo)))
			return SEC_E_INTERNAL_ERROR;
	}

	length = Stream_GetPosition(s);
	WINPR_ASSERT(length <= ULONG_MAX);
	buffer->cbBuffer = (ULONG)length;

	if (!sspi_SecBufferAlloc(&context->ChallengeMessage, (ULONG)length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->ChallengeMessage.pvBuffer, Stream_Buffer(s), length);
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_challenge_message(&context->ChallengeMessage, message,
	                             &context->ChallengeTargetInfo);
#endif
	ntlm_change_state(context, NTLM_STATE_AUTHENTICATE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;
	wStream sbuffer;
	wStream* s;
	size_t length;
	UINT32 flags = 0;
	NTLM_AV_PAIR* AvFlags = NULL;
	size_t PayloadBufferOffset;
	const NTLM_AUTHENTICATE_MESSAGE empty = { 0 };
	NTLM_AUTHENTICATE_MESSAGE* message;
	SSPI_CREDENTIALS* credentials;

	WINPR_ASSERT(context);
	WINPR_ASSERT(buffer);

	credentials = context->credentials;
	WINPR_ASSERT(credentials);

	message = &context->AUTHENTICATE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticConstInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (!ntlm_read_message_header(s, &message->header, MESSAGE_TYPE_AUTHENTICATE))
		goto fail;

	if (!ntlm_read_message_fields(
	        s, &(message->LmChallengeResponse))) /* LmChallengeResponseFields (8 bytes) */
		goto fail;

	if (!ntlm_read_message_fields(
	        s, &(message->NtChallengeResponse))) /* NtChallengeResponseFields (8 bytes) */
		goto fail;

	if (!ntlm_read_message_fields(s, &(message->DomainName))) /* DomainNameFields (8 bytes) */
		goto fail;

	if (!ntlm_read_message_fields(s, &(message->UserName))) /* UserNameFields (8 bytes) */
		goto fail;

	if (!ntlm_read_message_fields(s, &(message->Workstation))) /* WorkstationFields (8 bytes) */
		goto fail;

	if (!ntlm_read_message_fields(
	        s,
	        &(message->EncryptedRandomSessionKey))) /* EncryptedRandomSessionKeyFields (8 bytes) */
		goto fail;

	if (!ntlm_read_negotiate_flags(s, &message->NegotiateFlags, 0, "NTLM_AUTHENTICATE_MESSAGE"))
		goto fail;

	context->NegotiateKeyExchange =
	    (message->NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH) ? TRUE : FALSE;

	if ((context->NegotiateKeyExchange && !message->EncryptedRandomSessionKey.Len) ||
	    (!context->NegotiateKeyExchange && message->EncryptedRandomSessionKey.Len))
		goto fail;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_read_version_info(s, &(message->Version))) /* Version (8 bytes) */
			goto fail;
	}

	PayloadBufferOffset = Stream_GetPosition(s);

	status = SEC_E_INTERNAL_ERROR;
	if (!ntlm_read_message_fields_buffer(s, &(message->DomainName))) /* DomainName */
		goto fail;

	if (!ntlm_read_message_fields_buffer(s, &(message->UserName))) /* UserName */
		goto fail;

	if (!ntlm_read_message_fields_buffer(s, &(message->Workstation))) /* Workstation */
		goto fail;

	if (!ntlm_read_message_fields_buffer(s,
	                                     &(message->LmChallengeResponse))) /* LmChallengeResponse */
		goto fail;

	if (!ntlm_read_message_fields_buffer(s,
	                                     &(message->NtChallengeResponse))) /* NtChallengeResponse */
		goto fail;

	if (message->NtChallengeResponse.Len > 0)
	{
		size_t cbAvFlags;
		wStream ssbuffer;
		wStream* snt = Stream_StaticConstInit(&ssbuffer, message->NtChallengeResponse.Buffer,
		                                      message->NtChallengeResponse.Len);

		if (!snt)
			goto fail;

		status = SEC_E_INVALID_TOKEN;
		if (!ntlm_read_ntlm_v2_response(snt, &(context->NTLMv2Response)))
			goto fail;
		status = SEC_E_INTERNAL_ERROR;

		context->NtChallengeResponse.pvBuffer = message->NtChallengeResponse.Buffer;
		context->NtChallengeResponse.cbBuffer = message->NtChallengeResponse.Len;
		sspi_SecBufferFree(&(context->ChallengeTargetInfo));
		context->ChallengeTargetInfo.pvBuffer = (void*)context->NTLMv2Response.Challenge.AvPairs;
		context->ChallengeTargetInfo.cbBuffer = message->NtChallengeResponse.Len - (28 + 16);
		CopyMemory(context->ClientChallenge, context->NTLMv2Response.Challenge.ClientChallenge, 8);
		AvFlags =
		    ntlm_av_pair_get(context->NTLMv2Response.Challenge.AvPairs,
		                     context->NTLMv2Response.Challenge.cbAvPairs, MsvAvFlags, &cbAvFlags);

		if (AvFlags)
			Data_Read_UINT32(ntlm_av_pair_get_value_pointer(AvFlags), flags);
	}

	if (!ntlm_read_message_fields_buffer(
	        s, &(message->EncryptedRandomSessionKey))) /* EncryptedRandomSessionKey */
		goto fail;

	if (message->EncryptedRandomSessionKey.Len > 0)
	{
		if (message->EncryptedRandomSessionKey.Len != 16)
			goto fail;

		CopyMemory(context->EncryptedRandomSessionKey, message->EncryptedRandomSessionKey.Buffer,
		           16);
	}

	length = Stream_GetPosition(s);
	WINPR_ASSERT(length <= ULONG_MAX);

	if (!sspi_SecBufferAlloc(&context->AuthenticateMessage, (ULONG)length))
		goto fail;

	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = (ULONG)length;
	Stream_SetPosition(s, PayloadBufferOffset);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		status = SEC_E_INVALID_TOKEN;
		if (!ntlm_read_message_integrity_check(
		        s, &context->MessageIntegrityCheckOffset, message->MessageIntegrityCheck,
		        sizeof(message->MessageIntegrityCheck), "NTLM_AUTHENTICATE_MESSAGE"))
			goto fail;
	}

	status = SEC_E_INTERNAL_ERROR;

#if defined(WITH_DEBUG_NTLM)
	ntlm_print_authenticate_message(&context->AuthenticateMessage, message, flags, NULL);
#endif

	if (message->UserName.Len > 0)
	{
		credentials->identity.User = (UINT16*)malloc(message->UserName.Len);

		if (!credentials->identity.User)
			goto fail;

		CopyMemory(credentials->identity.User, message->UserName.Buffer, message->UserName.Len);
		credentials->identity.UserLength = message->UserName.Len / 2;
	}

	if (message->DomainName.Len > 0)
	{
		credentials->identity.Domain = (UINT16*)malloc(message->DomainName.Len);

		if (!credentials->identity.Domain)
			goto fail;

		CopyMemory(credentials->identity.Domain, message->DomainName.Buffer,
		           message->DomainName.Len);
		credentials->identity.DomainLength = message->DomainName.Len / 2;
	}

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_LM_KEY)
	{
		if (!ntlm_compute_lm_v2_response(context)) /* LmChallengeResponse */
			return SEC_E_INTERNAL_ERROR;
	}

	if (!ntlm_compute_ntlm_v2_response(context)) /* NtChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);
	/* EncryptedRandomSessionKey */
	ntlm_decrypt_random_session_key(context);
	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		BYTE messageIntegrityCheck[16] = { 0 };

		ntlm_compute_message_integrity_check(context, messageIntegrityCheck,
		                                     sizeof(messageIntegrityCheck));
		CopyMemory(
		    &((PBYTE)context->AuthenticateMessage.pvBuffer)[context->MessageIntegrityCheckOffset],
		    message->MessageIntegrityCheck, sizeof(message->MessageIntegrityCheck));

		if (memcmp(messageIntegrityCheck, message->MessageIntegrityCheck,
		           sizeof(message->MessageIntegrityCheck)) != 0)
		{
			WLog_ERR(TAG, "Message Integrity Check (MIC) verification failed!");
#ifdef WITH_DEBUG_NTLM
			WLog_ERR(TAG, "Expected MIC:");
			winpr_HexDump(TAG, WLOG_ERROR, messageIntegrityCheck, sizeof(messageIntegrityCheck));
			WLog_ERR(TAG, "Actual MIC:");
			winpr_HexDump(TAG, WLOG_ERROR, message->MessageIntegrityCheck,
			              sizeof(message->MessageIntegrityCheck));
#endif
			return SEC_E_MESSAGE_ALTERED;
		}
	}
	else
	{
		/* no mic message was present

		   https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-nlmp/f9e6fbc4-a953-4f24-b229-ccdcc213b9ec
		   the mic is optional, as not supported in Windows NT, Windows 2000, Windows XP, and
		   Windows Server 2003 and, as it seems, in the NTLMv2 implementation of Qt5.

		   now check the NtProofString, to detect if the entered client password matches the
		   expected password.
		   */

#ifdef WITH_DEBUG_NTLM
		WLog_VRB(TAG, "No MIC present, using NtProofString for verification.");
#endif

		if (memcmp(context->NTLMv2Response.Response, context->NtProofString, 16) != 0)
		{
			WLog_ERR(TAG, "NtProofString verification failed!");
#ifdef WITH_DEBUG_NTLM
			WLog_ERR(TAG, "Expected NtProofString:");
			winpr_HexDump(TAG, WLOG_ERROR, context->NtProofString, sizeof(context->NtProofString));
			WLog_ERR(TAG, "Actual NtProofString:");
			winpr_HexDump(TAG, WLOG_ERROR, context->NTLMv2Response.Response,
			              sizeof(context->NTLMv2Response));
#endif
			return SEC_E_LOGON_DENIED;
		}
	}

	/* Generate signing keys */
	if (!ntlm_generate_client_signing_key(context))
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_generate_server_signing_key(context))
		return SEC_E_INTERNAL_ERROR;
	/* Generate sealing keys */
	if (!ntlm_generate_client_sealing_key(context))
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_generate_server_sealing_key(context))
		return SEC_E_INTERNAL_ERROR;
	/* Initialize RC4 seal state */
	ntlm_init_rc4_seal_states(context);
#if defined(WITH_DEBUG_NTLM)
	ntlm_print_authentication_complete(context);
#endif
	ntlm_change_state(context, NTLM_STATE_FINAL);
	ntlm_free_message_fields_buffer(&(message->DomainName));
	ntlm_free_message_fields_buffer(&(message->UserName));
	ntlm_free_message_fields_buffer(&(message->Workstation));
	ntlm_free_message_fields_buffer(&(message->LmChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->NtChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->EncryptedRandomSessionKey));
	return SEC_E_OK;

fail:
	return status;
}

/**
 * Send NTLMSSP AUTHENTICATE_MESSAGE. msdn{cc236643}
 *
 * @param context Pointer to the NTLM context
 * @param buffer The buffer to write
 */

SECURITY_STATUS ntlm_write_AuthenticateMessage(NTLM_CONTEXT* context, const PSecBuffer buffer)
{
	wStream sbuffer;
	wStream* s;
	size_t length;
	UINT32 PayloadBufferOffset;
	const NTLM_AUTHENTICATE_MESSAGE empty = { 0 };
	NTLM_AUTHENTICATE_MESSAGE* message;
	SSPI_CREDENTIALS* credentials;

	WINPR_ASSERT(context);
	WINPR_ASSERT(buffer);

	credentials = context->credentials;
	WINPR_ASSERT(credentials);

	message = &context->AUTHENTICATE_MESSAGE;
	WINPR_ASSERT(message);

	*message = empty;

	s = Stream_StaticInit(&sbuffer, buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (context->NTLMv2)
	{
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_56;

		if (context->SendVersionInfo)
			message->NegotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
	}

	if (context->UseMIC)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (context->SendWorkstationName)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED;

	if (context->confidentiality)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;

	if (context->CHALLENGE_MESSAGE.NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;

	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_128;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
	message->NegotiateFlags |= NTLMSSP_REQUEST_TARGET;
	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_get_version_info(&(message->Version));

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
	{
		message->Workstation.Len = context->Workstation.Length;
		message->Workstation.Buffer = (BYTE*)context->Workstation.Buffer;
	}

	if (credentials->identity.DomainLength > 0)
	{
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		message->DomainName.Len = (UINT16)credentials->identity.DomainLength * 2;
		message->DomainName.Buffer = (BYTE*)credentials->identity.Domain;
	}

	message->UserName.Len = (UINT16)credentials->identity.UserLength * 2;
	message->UserName.Buffer = (BYTE*)credentials->identity.User;
	message->LmChallengeResponse.Len = (UINT16)context->LmChallengeResponse.cbBuffer;
	message->LmChallengeResponse.Buffer = (BYTE*)context->LmChallengeResponse.pvBuffer;
	message->NtChallengeResponse.Len = (UINT16)context->NtChallengeResponse.cbBuffer;
	message->NtChallengeResponse.Buffer = (BYTE*)context->NtChallengeResponse.pvBuffer;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
	{
		message->EncryptedRandomSessionKey.Len = 16;
		message->EncryptedRandomSessionKey.Buffer = context->EncryptedRandomSessionKey;
	}

	PayloadBufferOffset = 64;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8; /* Version (8 bytes) */

	if (context->UseMIC)
		PayloadBufferOffset += 16; /* Message Integrity Check (16 bytes) */

	message->DomainName.BufferOffset = PayloadBufferOffset;
	message->UserName.BufferOffset = message->DomainName.BufferOffset + message->DomainName.Len;
	message->Workstation.BufferOffset = message->UserName.BufferOffset + message->UserName.Len;
	message->LmChallengeResponse.BufferOffset =
	    message->Workstation.BufferOffset + message->Workstation.Len;
	message->NtChallengeResponse.BufferOffset =
	    message->LmChallengeResponse.BufferOffset + message->LmChallengeResponse.Len;
	message->EncryptedRandomSessionKey.BufferOffset =
	    message->NtChallengeResponse.BufferOffset + message->NtChallengeResponse.Len;
	if (!ntlm_populate_message_header(&message->header, MESSAGE_TYPE_AUTHENTICATE))
		return SEC_E_INVALID_TOKEN;
	if (!ntlm_write_message_header(s, &message->header)) /* Message Header (12 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(
	        s, &(message->LmChallengeResponse))) /* LmChallengeResponseFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(
	        s, &(message->NtChallengeResponse))) /* NtChallengeResponseFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(s, &(message->DomainName))) /* DomainNameFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(s, &(message->UserName))) /* UserNameFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(s, &(message->Workstation))) /* WorkstationFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_message_fields(
	        s,
	        &(message->EncryptedRandomSessionKey))) /* EncryptedRandomSessionKeyFields (8 bytes) */
		return SEC_E_INTERNAL_ERROR;
	if (!ntlm_write_negotiate_flags(s, message->NegotiateFlags, "NTLM_AUTHENTICATE_MESSAGE"))
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (!ntlm_write_version_info(s, &(message->Version))) /* Version (8 bytes) */
			return SEC_E_INTERNAL_ERROR;
	}

	if (context->UseMIC)
	{
		const BYTE data[WINPR_MD5_DIGEST_LENGTH] = { 0 };

		context->MessageIntegrityCheckOffset = Stream_GetPosition(s);
		if (!ntlm_write_message_integrity_check(s, Stream_GetPosition(s), data, sizeof(data),
		                                        "NTLM_AUTHENTICATE_MESSAGE"))
			return SEC_E_INTERNAL_ERROR;
	}

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED)
	{
		if (!ntlm_write_message_fields_buffer(s, &(message->DomainName))) /* DomainName */
			return SEC_E_INTERNAL_ERROR;
	}

	if (!ntlm_write_message_fields_buffer(s, &(message->UserName))) /* UserName */
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
	{
		if (!ntlm_write_message_fields_buffer(s, &(message->Workstation))) /* Workstation */
			return SEC_E_INTERNAL_ERROR;
	}

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_LM_KEY)
	{
		if (!ntlm_write_message_fields_buffer(
		        s, &(message->LmChallengeResponse))) /* LmChallengeResponse */
			return SEC_E_INTERNAL_ERROR;
	}
	if (!ntlm_write_message_fields_buffer(
	        s, &(message->NtChallengeResponse))) /* NtChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
	{
		if (!ntlm_write_message_fields_buffer(
		        s, &(message->EncryptedRandomSessionKey))) /* EncryptedRandomSessionKey */
			return SEC_E_INTERNAL_ERROR;
	}

	length = Stream_GetPosition(s);
	WINPR_ASSERT(length <= ULONG_MAX);

	if (!sspi_SecBufferAlloc(&context->AuthenticateMessage, (ULONG)length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = (ULONG)length;

	if (context->UseMIC)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context, message->MessageIntegrityCheck,
		                                     sizeof(message->MessageIntegrityCheck));
		if (!ntlm_write_message_integrity_check(
		        s, context->MessageIntegrityCheckOffset, message->MessageIntegrityCheck,
		        sizeof(message->MessageIntegrityCheck), "NTLM_AUTHENTICATE_MESSAGE"))
			return SEC_E_INTERNAL_ERROR;
	}

#if defined(WITH_DEBUG_NTLM)
	ntlm_print_authenticate_message(&context->AuthenticateMessage, message,
	                                context->UseMIC ? MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK : 0,
	                                &context->AuthenticateTargetInfo);
#endif
	ntlm_change_state(context, NTLM_STATE_FINAL);
	return SEC_E_OK;
}
