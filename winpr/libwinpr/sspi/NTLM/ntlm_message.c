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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_message.h"

#include "../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

static const char NTLM_SIGNATURE[8] = { 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0' };

static const char* const NTLM_NEGOTIATE_STRINGS[] =
{
	"NTLMSSP_NEGOTIATE_56",
	"NTLMSSP_NEGOTIATE_KEY_EXCH",
	"NTLMSSP_NEGOTIATE_128",
	"NTLMSSP_RESERVED1",
	"NTLMSSP_RESERVED2",
	"NTLMSSP_RESERVED3",
	"NTLMSSP_NEGOTIATE_VERSION",
	"NTLMSSP_RESERVED4",
	"NTLMSSP_NEGOTIATE_TARGET_INFO",
	"NTLMSSP_REQUEST_NON_NT_SESSION_KEY",
	"NTLMSSP_RESERVED5",
	"NTLMSSP_NEGOTIATE_IDENTIFY",
	"NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY",
	"NTLMSSP_RESERVED6",
	"NTLMSSP_TARGET_TYPE_SERVER",
	"NTLMSSP_TARGET_TYPE_DOMAIN",
	"NTLMSSP_NEGOTIATE_ALWAYS_SIGN",
	"NTLMSSP_RESERVED7",
	"NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED",
	"NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED",
	"NTLMSSP_NEGOTIATE_ANONYMOUS",
	"NTLMSSP_RESERVED8",
	"NTLMSSP_NEGOTIATE_NTLM",
	"NTLMSSP_RESERVED9",
	"NTLMSSP_NEGOTIATE_LM_KEY",
	"NTLMSSP_NEGOTIATE_DATAGRAM",
	"NTLMSSP_NEGOTIATE_SEAL",
	"NTLMSSP_NEGOTIATE_SIGN",
	"NTLMSSP_RESERVED10",
	"NTLMSSP_REQUEST_TARGET",
	"NTLMSSP_NEGOTIATE_OEM",
	"NTLMSSP_NEGOTIATE_UNICODE"
};

void ntlm_print_negotiate_flags(UINT32 flags)
{
	int i;
	const char* str;
	WLog_INFO(TAG, "negotiateFlags \"0x%08"PRIX32"\"", flags);

	for (i = 31; i >= 0; i--)
	{
		if ((flags >> i) & 1)
		{
			str = NTLM_NEGOTIATE_STRINGS[(31 - i)];
			WLog_INFO(TAG, "\t%s (%d),", str, (31 - i));
		}
	}
}

int ntlm_read_message_header(wStream* s, NTLM_MESSAGE_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read(s, header->Signature, 8);
	Stream_Read_UINT32(s, header->MessageType);

	if (strncmp((char*) header->Signature, NTLM_SIGNATURE, 8) != 0)
		return -1;

	return 1;
}

void ntlm_write_message_header(wStream* s, NTLM_MESSAGE_HEADER* header)
{
	Stream_Write(s, header->Signature, sizeof(NTLM_SIGNATURE));
	Stream_Write_UINT32(s, header->MessageType);
}

void ntlm_populate_message_header(NTLM_MESSAGE_HEADER* header, UINT32 MessageType)
{
	CopyMemory(header->Signature, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));
	header->MessageType = MessageType;
}

int ntlm_read_message_fields(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT16(s, fields->Len); /* Len (2 bytes) */
	Stream_Read_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	Stream_Read_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
	return 1;
}

void ntlm_write_message_fields(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->MaxLen < 1)
		fields->MaxLen = fields->Len;

	Stream_Write_UINT16(s, fields->Len); /* Len (2 bytes) */
	Stream_Write_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	Stream_Write_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
}

int ntlm_read_message_fields_buffer(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->Len > 0)
	{
		if ((fields->BufferOffset + fields->Len) > Stream_Length(s))
			return -1;

		fields->Buffer = (PBYTE) malloc(fields->Len);

		if (!fields->Buffer)
			return -1;

		Stream_SetPosition(s, fields->BufferOffset);
		Stream_Read(s, fields->Buffer, fields->Len);
	}

	return 1;
}

void ntlm_write_message_fields_buffer(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->Len > 0)
	{
		Stream_SetPosition(s, fields->BufferOffset);
		Stream_Write(s, fields->Buffer, fields->Len);
	}
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

void ntlm_print_message_fields(NTLM_MESSAGE_FIELDS* fields, const char* name)
{
	WLog_DBG(TAG, "%s (Len: %"PRIu16" MaxLen: %"PRIu16" BufferOffset: %"PRIu32")",
	         name, fields->Len, fields->MaxLen, fields->BufferOffset);

	if (fields->Len > 0)
		winpr_HexDump(TAG, WLOG_DEBUG, fields->Buffer, fields->Len);
}

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	size_t length;
	NTLM_NEGOTIATE_MESSAGE* message;
	message = &context->NEGOTIATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_NEGOTIATE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message) < 0)
		return SEC_E_INVALID_TOKEN;

	if (message->MessageType != MESSAGE_TYPE_NEGOTIATE)
		return SEC_E_INVALID_TOKEN;

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (!((message->NegotiateFlags & NTLMSSP_REQUEST_TARGET) &&
	      (message->NegotiateFlags & NTLMSSP_NEGOTIATE_NTLM) &&
	      (message->NegotiateFlags & NTLMSSP_NEGOTIATE_UNICODE)))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	context->NegotiateFlags = message->NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	if (ntlm_read_message_fields(s, &(message->DomainName)) < 0) /* DomainNameFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	if (ntlm_read_message_fields(s, &(message->Workstation)) < 0) /* WorkstationFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (ntlm_read_version_info(s, &(message->Version)) < 0) /* Version (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	length = Stream_GetPosition(s);
	buffer->cbBuffer = length;

	if (!sspi_SecBufferAlloc(&context->NegotiateMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "NEGOTIATE_MESSAGE (length = %"PRIu32")", context->NegotiateMessage.cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, context->NegotiateMessage.pvBuffer,
	              context->NegotiateMessage.cbBuffer);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

#endif
	context->state = NTLM_STATE_CHALLENGE;
	Stream_Free(s, FALSE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	size_t length;
	NTLM_NEGOTIATE_MESSAGE* message;
	message = &context->NEGOTIATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_NEGOTIATE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_NEGOTIATE);

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
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) message);
	Stream_Write_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */
	/* DomainNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->DomainName));
	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */
	/* WorkstationFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->Workstation));

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message->Version));

	length = Stream_GetPosition(s);
	buffer->cbBuffer = length;

	if (!sspi_SecBufferAlloc(&context->NegotiateMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "NEGOTIATE_MESSAGE (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), length);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

#endif
	context->state = NTLM_STATE_CHALLENGE;
	Stream_Free(s, FALSE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	int length;
	PBYTE StartOffset;
	PBYTE PayloadOffset;
	NTLM_AV_PAIR* AvTimestamp;
	NTLM_CHALLENGE_MESSAGE* message;
	ntlm_generate_client_challenge(context);
	message = &context->CHALLENGE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_CHALLENGE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	StartOffset = Stream_Pointer(s);

	if (ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message) < 0)
		return SEC_E_INVALID_TOKEN;

	if (message->MessageType != MESSAGE_TYPE_CHALLENGE)
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s, &(message->TargetName)) < 0) /* TargetNameFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (Stream_GetRemainingLength(s) < 4)
		return SEC_E_INVALID_TOKEN;

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	context->NegotiateFlags = message->NegotiateFlags;

	if (Stream_GetRemainingLength(s) < 8)
		return SEC_E_INVALID_TOKEN;

	Stream_Read(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	CopyMemory(context->ServerChallenge, message->ServerChallenge, 8);

	if (Stream_GetRemainingLength(s) < 8)
		return SEC_E_INVALID_TOKEN;

	Stream_Read(s, message->Reserved, 8); /* Reserved (8 bytes), should be ignored */

	if (ntlm_read_message_fields(s, &(message->TargetInfo)) < 0) /* TargetInfoFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (ntlm_read_version_info(s, &(message->Version)) < 0) /* Version (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	/* Payload (variable) */
	PayloadOffset = Stream_Pointer(s);

	if (message->TargetName.Len > 0)
	{
		if (ntlm_read_message_fields_buffer(s, &(message->TargetName)) < 0)
			return SEC_E_INTERNAL_ERROR;
	}

	if (message->TargetInfo.Len > 0)
	{
		if (ntlm_read_message_fields_buffer(s, &(message->TargetInfo)) < 0)
			return SEC_E_INTERNAL_ERROR;

		context->ChallengeTargetInfo.pvBuffer = message->TargetInfo.Buffer;
		context->ChallengeTargetInfo.cbBuffer = message->TargetInfo.Len;
		AvTimestamp = ntlm_av_pair_get((NTLM_AV_PAIR*) message->TargetInfo.Buffer, MsvAvTimestamp);

		if (AvTimestamp)
		{
			if (context->NTLMv2)
				context->UseMIC = TRUE;

			CopyMemory(context->ChallengeTimestamp, ntlm_av_pair_get_value_pointer(AvTimestamp), 8);
		}
	}

	length = (PayloadOffset - StartOffset) + message->TargetName.Len + message->TargetInfo.Len;

	if (!sspi_SecBufferAlloc(&context->ChallengeMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->ChallengeMessage.pvBuffer, StartOffset, length);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "CHALLENGE_MESSAGE (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, context->ChallengeMessage.pvBuffer,
	              context->ChallengeMessage.cbBuffer);
	ntlm_print_negotiate_flags(context->NegotiateFlags);

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->TargetName), "TargetName");
	ntlm_print_message_fields(&(message->TargetInfo), "TargetInfo");

	if (context->ChallengeTargetInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "ChallengeTargetInfo (%"PRIu32"):", context->ChallengeTargetInfo.cbBuffer);
		ntlm_print_av_pair_list(context->ChallengeTargetInfo.pvBuffer);
	}

#endif
	/* AV_PAIRs */

	if (context->NTLMv2)
	{
		if (ntlm_construct_authenticate_target_info(context) < 0)
			return SEC_E_INTERNAL_ERROR;

		sspi_SecBufferFree(&context->ChallengeTargetInfo);
		context->ChallengeTargetInfo.pvBuffer = context->AuthenticateTargetInfo.pvBuffer;
		context->ChallengeTargetInfo.cbBuffer = context->AuthenticateTargetInfo.cbBuffer;
	}

	ntlm_generate_timestamp(context); /* Timestamp */

	if (ntlm_compute_lm_v2_response(context) < 0) /* LmChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_compute_ntlm_v2_response(context) < 0) /* NtChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	ntlm_generate_key_exchange_key(context); /* KeyExchangeKey */
	ntlm_generate_random_session_key(context); /* RandomSessionKey */
	ntlm_generate_exported_session_key(context); /* ExportedSessionKey */
	ntlm_encrypt_random_session_key(context); /* EncryptedRandomSessionKey */
	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);
	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);
	/* Initialize RC4 seal state using client sealing key */
	ntlm_init_rc4_seal_states(context);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "ClientChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientChallenge, 8);
	WLog_DBG(TAG, "ServerChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerChallenge, 8);
	WLog_DBG(TAG, "SessionBaseKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->SessionBaseKey, 16);
	WLog_DBG(TAG, "KeyExchangeKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->KeyExchangeKey, 16);
	WLog_DBG(TAG, "ExportedSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ExportedSessionKey, 16);
	WLog_DBG(TAG, "RandomSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->RandomSessionKey, 16);
	WLog_DBG(TAG, "ClientSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSigningKey, 16);
	WLog_DBG(TAG, "ClientSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSealingKey, 16);
	WLog_DBG(TAG, "ServerSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSigningKey, 16);
	WLog_DBG(TAG, "ServerSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSealingKey, 16);
	WLog_DBG(TAG, "Timestamp");
	winpr_HexDump(TAG, WLOG_DEBUG, context->Timestamp, 8);
#endif
	context->state = NTLM_STATE_AUTHENTICATE;
	ntlm_free_message_fields_buffer(&(message->TargetName));
	Stream_Free(s, FALSE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	size_t length;
	UINT32 PayloadOffset;
	NTLM_CHALLENGE_MESSAGE* message;
	message = &context->CHALLENGE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_CHALLENGE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	ntlm_get_version_info(&(message->Version)); /* Version */
	ntlm_generate_server_challenge(context); /* Server Challenge */
	ntlm_generate_timestamp(context); /* Timestamp */

	if (ntlm_construct_challenge_target_info(context) < 0) /* TargetInfo */
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(message->ServerChallenge, context->ServerChallenge, 8); /* ServerChallenge */
	message->NegotiateFlags = context->NegotiateFlags;
	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_CHALLENGE);
	/* Message Header (12 bytes) */
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) message);

	if (message->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		message->TargetName.Len = (UINT16) context->TargetName.cbBuffer;
		message->TargetName.Buffer = (PBYTE) context->TargetName.pvBuffer;
	}

	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		message->TargetInfo.Len = (UINT16) context->ChallengeTargetInfo.cbBuffer;
		message->TargetInfo.Buffer = (PBYTE) context->ChallengeTargetInfo.pvBuffer;
	}

	PayloadOffset = 48;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadOffset += 8;

	message->TargetName.BufferOffset = PayloadOffset;
	message->TargetInfo.BufferOffset = message->TargetName.BufferOffset + message->TargetName.Len;
	/* TargetNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->TargetName));
	Stream_Write_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	Stream_Write(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	Stream_Write(s, message->Reserved, 8); /* Reserved (8 bytes), should be ignored */
	/* TargetInfoFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->TargetInfo));

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message->Version)); /* Version (8 bytes) */

	/* Payload (variable) */

	if (message->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
		ntlm_write_message_fields_buffer(s, &(message->TargetName));

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
		ntlm_write_message_fields_buffer(s, &(message->TargetInfo));

	length = Stream_GetPosition(s);
	buffer->cbBuffer = length;

	if (!sspi_SecBufferAlloc(&context->ChallengeMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->ChallengeMessage.pvBuffer, Stream_Buffer(s), length);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "CHALLENGE_MESSAGE (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, context->ChallengeMessage.pvBuffer,
	              context->ChallengeMessage.cbBuffer);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->TargetName), "TargetName");
	ntlm_print_message_fields(&(message->TargetInfo), "TargetInfo");
#endif
	context->state = NTLM_STATE_AUTHENTICATE;
	Stream_Free(s, FALSE);
	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_read_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	size_t length;
	UINT32 flags;
	NTLM_AV_PAIR* AvFlags;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE* message;
	SSPI_CREDENTIALS* credentials = context->credentials;
	flags = 0;
	AvFlags = NULL;
	message = &context->AUTHENTICATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_AUTHENTICATE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message) < 0)
		return SEC_E_INVALID_TOKEN;

	if (message->MessageType != MESSAGE_TYPE_AUTHENTICATE)
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s,
	                             &(message->LmChallengeResponse)) < 0) /* LmChallengeResponseFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s,
	                             &(message->NtChallengeResponse)) < 0) /* NtChallengeResponseFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s, &(message->DomainName)) < 0) /* DomainNameFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s, &(message->UserName)) < 0) /* UserNameFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s, &(message->Workstation)) < 0) /* WorkstationFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	if (ntlm_read_message_fields(s,
	                             &(message->EncryptedRandomSessionKey)) < 0) /* EncryptedRandomSessionKeyFields (8 bytes) */
		return SEC_E_INVALID_TOKEN;

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	context->NegotiateKeyExchange = (message->NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH) ? TRUE :
	                                FALSE;

	if ((context->NegotiateKeyExchange && !message->EncryptedRandomSessionKey.Len) ||
	    (!context->NegotiateKeyExchange && message->EncryptedRandomSessionKey.Len))
		return SEC_E_INVALID_TOKEN;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (ntlm_read_version_info(s, &(message->Version)) < 0) /* Version (8 bytes) */
			return SEC_E_INVALID_TOKEN;
	}

	PayloadBufferOffset = Stream_GetPosition(s);

	if (ntlm_read_message_fields_buffer(s, &(message->DomainName)) < 0) /* DomainName */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_fields_buffer(s, &(message->UserName)) < 0) /* UserName */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_fields_buffer(s, &(message->Workstation)) < 0) /* Workstation */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_fields_buffer(s,
	                                    &(message->LmChallengeResponse)) < 0) /* LmChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_read_message_fields_buffer(s,
	                                    &(message->NtChallengeResponse)) < 0) /* NtChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	if (message->NtChallengeResponse.Len > 0)
	{
		wStream* snt = Stream_New(message->NtChallengeResponse.Buffer, message->NtChallengeResponse.Len);

		if (!snt)
			return SEC_E_INTERNAL_ERROR;

		if (ntlm_read_ntlm_v2_response(snt, &(context->NTLMv2Response)) < 0)
			return SEC_E_INVALID_TOKEN;

		Stream_Free(snt, FALSE);
		context->NtChallengeResponse.pvBuffer = message->NtChallengeResponse.Buffer;
		context->NtChallengeResponse.cbBuffer = message->NtChallengeResponse.Len;
		sspi_SecBufferFree(&(context->ChallengeTargetInfo));
		context->ChallengeTargetInfo.pvBuffer = (void*) context->NTLMv2Response.Challenge.AvPairs;
		context->ChallengeTargetInfo.cbBuffer = message->NtChallengeResponse.Len - (28 + 16);
		CopyMemory(context->ClientChallenge, context->NTLMv2Response.Challenge.ClientChallenge, 8);
		AvFlags = ntlm_av_pair_get(context->NTLMv2Response.Challenge.AvPairs, MsvAvFlags);

		if (AvFlags)
			Data_Read_UINT32(ntlm_av_pair_get_value_pointer(AvFlags), flags);
	}

	if (ntlm_read_message_fields_buffer(s,
	                                    &(message->EncryptedRandomSessionKey)) < 0) /* EncryptedRandomSessionKey */
		return SEC_E_INTERNAL_ERROR;

	if (message->EncryptedRandomSessionKey.Len > 0)
	{
		if (message->EncryptedRandomSessionKey.Len != 16)
			return SEC_E_INVALID_TOKEN;

		CopyMemory(context->EncryptedRandomSessionKey, message->EncryptedRandomSessionKey.Buffer, 16);
	}

	length = Stream_GetPosition(s);

	if (!sspi_SecBufferAlloc(&context->AuthenticateMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = length;
	Stream_SetPosition(s, PayloadBufferOffset);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		context->MessageIntegrityCheckOffset = (UINT32) Stream_GetPosition(s);

		if (Stream_GetRemainingLength(s) < 16)
			return SEC_E_INVALID_TOKEN;

		Stream_Read(s, message->MessageIntegrityCheck, 16);
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "AUTHENTICATE_MESSAGE (length = %"PRIu32")", context->AuthenticateMessage.cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, context->AuthenticateMessage.pvBuffer,
	              context->AuthenticateMessage.cbBuffer);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->DomainName), "DomainName");
	ntlm_print_message_fields(&(message->UserName), "UserName");
	ntlm_print_message_fields(&(message->Workstation), "Workstation");
	ntlm_print_message_fields(&(message->LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message->NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message->EncryptedRandomSessionKey), "EncryptedRandomSessionKey");
	ntlm_print_av_pair_list(context->NTLMv2Response.Challenge.AvPairs);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		WLog_DBG(TAG, "MessageIntegrityCheck:");
		winpr_HexDump(TAG, WLOG_DEBUG, message->MessageIntegrityCheck, 16);
	}

#endif

	if (message->UserName.Len > 0)
	{
		credentials->identity.User = (UINT16*) malloc(message->UserName.Len);

		if (!credentials->identity.User)
			return SEC_E_INTERNAL_ERROR;

		CopyMemory(credentials->identity.User, message->UserName.Buffer, message->UserName.Len);
		credentials->identity.UserLength = message->UserName.Len / 2;
	}

	if (message->DomainName.Len > 0)
	{
		credentials->identity.Domain = (UINT16*) malloc(message->DomainName.Len);

		if (!credentials->identity.Domain)
			return SEC_E_INTERNAL_ERROR;

		CopyMemory(credentials->identity.Domain, message->DomainName.Buffer, message->DomainName.Len);
		credentials->identity.DomainLength = message->DomainName.Len / 2;
	}

	Stream_Free(s, FALSE);
	/* Computations beyond this point require the NTLM hash of the password */
	context->state = NTLM_STATE_COMPLETION;
	return SEC_I_COMPLETE_NEEDED;
}

/**
 * Send NTLMSSP AUTHENTICATE_MESSAGE.\n
 * AUTHENTICATE_MESSAGE @msdn{cc236643}
 * @param NTLM context
 * @param buffer
 */

SECURITY_STATUS ntlm_write_AuthenticateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	size_t length;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE* message;
	SSPI_CREDENTIALS* credentials = context->credentials;
	message = &context->AUTHENTICATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_AUTHENTICATE_MESSAGE));
	s = Stream_New((BYTE*) buffer->pvBuffer, buffer->cbBuffer);

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
		message->Workstation.Buffer = (BYTE*) context->Workstation.Buffer;
	}

	if (credentials->identity.DomainLength > 0)
	{
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		message->DomainName.Len = (UINT16) credentials->identity.DomainLength * 2;
		message->DomainName.Buffer = (BYTE*) credentials->identity.Domain;
	}

	message->UserName.Len = (UINT16) credentials->identity.UserLength * 2;
	message->UserName.Buffer = (BYTE*) credentials->identity.User;
	message->LmChallengeResponse.Len = (UINT16) context->LmChallengeResponse.cbBuffer;
	message->LmChallengeResponse.Buffer = (BYTE*) context->LmChallengeResponse.pvBuffer;
	message->NtChallengeResponse.Len = (UINT16) context->NtChallengeResponse.cbBuffer;
	message->NtChallengeResponse.Buffer = (BYTE*) context->NtChallengeResponse.pvBuffer;

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
	message->LmChallengeResponse.BufferOffset = message->Workstation.BufferOffset +
	        message->Workstation.Len;
	message->NtChallengeResponse.BufferOffset = message->LmChallengeResponse.BufferOffset +
	        message->LmChallengeResponse.Len;
	message->EncryptedRandomSessionKey.BufferOffset = message->NtChallengeResponse.BufferOffset +
	        message->NtChallengeResponse.Len;
	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_AUTHENTICATE);
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) message);  /* Message Header (12 bytes) */
	ntlm_write_message_fields(s, &
	                          (message->LmChallengeResponse)); /* LmChallengeResponseFields (8 bytes) */
	ntlm_write_message_fields(s, &
	                          (message->NtChallengeResponse)); /* NtChallengeResponseFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->DomainName)); /* DomainNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->UserName)); /* UserNameFields (8 bytes) */
	ntlm_write_message_fields(s, &(message->Workstation)); /* WorkstationFields (8 bytes) */
	ntlm_write_message_fields(s, &
	                          (message->EncryptedRandomSessionKey)); /* EncryptedRandomSessionKeyFields (8 bytes) */
	Stream_Write_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message->Version)); /* Version (8 bytes) */

	if (context->UseMIC)
	{
		context->MessageIntegrityCheckOffset = (UINT32) Stream_GetPosition(s);
		Stream_Zero(s, 16); /* Message Integrity Check (16 bytes) */
	}

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED)
		ntlm_write_message_fields_buffer(s, &(message->DomainName)); /* DomainName */

	ntlm_write_message_fields_buffer(s, &(message->UserName)); /* UserName */

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
		ntlm_write_message_fields_buffer(s, &(message->Workstation)); /* Workstation */

	ntlm_write_message_fields_buffer(s, &(message->LmChallengeResponse)); /* LmChallengeResponse */
	ntlm_write_message_fields_buffer(s, &(message->NtChallengeResponse)); /* NtChallengeResponse */

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
		ntlm_write_message_fields_buffer(s,
		                                 &(message->EncryptedRandomSessionKey)); /* EncryptedRandomSessionKey */

	length = Stream_GetPosition(s);

	if (!sspi_SecBufferAlloc(&context->AuthenticateMessage, length))
		return SEC_E_INTERNAL_ERROR;

	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = length;

	if (context->UseMIC)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context);
		Stream_SetPosition(s, context->MessageIntegrityCheckOffset);
		Stream_Write(s, context->MessageIntegrityCheck, 16);
		Stream_SetPosition(s, length);
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "AUTHENTICATE_MESSAGE (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), length);
	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	if (context->AuthenticateTargetInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "AuthenticateTargetInfo (%"PRIu32"):", context->AuthenticateTargetInfo.cbBuffer);
		ntlm_print_av_pair_list(context->AuthenticateTargetInfo.pvBuffer);
	}

	ntlm_print_message_fields(&(message->DomainName), "DomainName");
	ntlm_print_message_fields(&(message->UserName), "UserName");
	ntlm_print_message_fields(&(message->Workstation), "Workstation");
	ntlm_print_message_fields(&(message->LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message->NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message->EncryptedRandomSessionKey), "EncryptedRandomSessionKey");

	if (context->UseMIC)
	{
		WLog_DBG(TAG, "MessageIntegrityCheck (length = 16)");
		winpr_HexDump(TAG, WLOG_DEBUG, context->MessageIntegrityCheck, 16);
	}

#endif
	context->state = NTLM_STATE_FINAL;
	Stream_Free(s, FALSE);
	return SEC_I_COMPLETE_NEEDED;
}

SECURITY_STATUS ntlm_server_AuthenticateComplete(NTLM_CONTEXT* context)
{
	UINT32 flags = 0;
	NTLM_AV_PAIR* AvFlags = NULL;
	NTLM_AUTHENTICATE_MESSAGE* message;

	if (context->state != NTLM_STATE_COMPLETION)
		return SEC_E_OUT_OF_SEQUENCE;

	message = &context->AUTHENTICATE_MESSAGE;
	AvFlags = ntlm_av_pair_get(context->NTLMv2Response.Challenge.AvPairs, MsvAvFlags);

	if (AvFlags)
		Data_Read_UINT32(ntlm_av_pair_get_value_pointer(AvFlags), flags);

	if (ntlm_compute_lm_v2_response(context) < 0) /* LmChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	if (ntlm_compute_ntlm_v2_response(context) < 0) /* NtChallengeResponse */
		return SEC_E_INTERNAL_ERROR;

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);
	/* EncryptedRandomSessionKey */
	ntlm_decrypt_random_session_key(context);
	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		ZeroMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[context->MessageIntegrityCheckOffset],
		           16);
		ntlm_compute_message_integrity_check(context);
		CopyMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[context->MessageIntegrityCheckOffset],
		           message->MessageIntegrityCheck, 16);

		if (memcmp(context->MessageIntegrityCheck, message->MessageIntegrityCheck, 16) != 0)
		{
			WLog_ERR(TAG, "Message Integrity Check (MIC) verification failed!");
			WLog_ERR(TAG, "Expected MIC:");
			winpr_HexDump(TAG, WLOG_ERROR, context->MessageIntegrityCheck, 16);
			WLog_ERR(TAG, "Actual MIC:");
			winpr_HexDump(TAG, WLOG_ERROR, message->MessageIntegrityCheck, 16);
			return SEC_E_MESSAGE_ALTERED;
		}
	}

	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);
	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);
	/* Initialize RC4 seal state */
	ntlm_init_rc4_seal_states(context);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "ClientChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientChallenge, 8);
	WLog_DBG(TAG, "ServerChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerChallenge, 8);
	WLog_DBG(TAG, "SessionBaseKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->SessionBaseKey, 16);
	WLog_DBG(TAG, "KeyExchangeKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->KeyExchangeKey, 16);
	WLog_DBG(TAG, "ExportedSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ExportedSessionKey, 16);
	WLog_DBG(TAG, "RandomSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->RandomSessionKey, 16);
	WLog_DBG(TAG, "ClientSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSigningKey, 16);
	WLog_DBG(TAG, "ClientSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSealingKey, 16);
	WLog_DBG(TAG, "ServerSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSigningKey, 16);
	WLog_DBG(TAG, "ServerSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSealingKey, 16);
	WLog_DBG(TAG, "Timestamp");
	winpr_HexDump(TAG, WLOG_DEBUG, context->Timestamp, 8);
#endif
	context->state = NTLM_STATE_FINAL;
	ntlm_free_message_fields_buffer(&(message->DomainName));
	ntlm_free_message_fields_buffer(&(message->UserName));
	ntlm_free_message_fields_buffer(&(message->Workstation));
	ntlm_free_message_fields_buffer(&(message->LmChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->NtChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->EncryptedRandomSessionKey));
	return SEC_E_OK;
}
