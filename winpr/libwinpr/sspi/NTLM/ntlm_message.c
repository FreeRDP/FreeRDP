/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Message)
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

static const char NTLM_SIGNATURE[8] = "NTLMSSP\0";

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

	fprintf(stderr, "negotiateFlags \"0x%08X\"{\n", flags);

	for (i = 31; i >= 0; i--)
	{
		if ((flags >> i) & 1)
		{
			str = NTLM_NEGOTIATE_STRINGS[(31 - i)];
			fprintf(stderr, "\t%s (%d),\n", str, (31 - i));
		}
	}

	fprintf(stderr, "}\n");
}

void ntlm_read_message_header(wStream* s, NTLM_MESSAGE_HEADER* header)
{
	Stream_Read(s, header->Signature, sizeof(NTLM_SIGNATURE));
	Stream_Read_UINT32(s, header->MessageType);
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

BOOL ntlm_validate_message_header(wStream* s, NTLM_MESSAGE_HEADER* header, UINT32 MessageType)
{
	if (memcmp(header->Signature, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
	{
		fprintf(stderr, "Unexpected NTLM signature: %s, expected:%s\n", header->Signature, NTLM_SIGNATURE);
		return FALSE;
	}

	if (header->MessageType != MessageType)
	{
		fprintf(stderr, "Unexpected NTLM message type: %d, expected: %d\n", header->MessageType, MessageType);
		return FALSE;
	}

	return TRUE;
}

void ntlm_read_message_fields(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	Stream_Read_UINT16(s, fields->Len); /* Len (2 bytes) */
	Stream_Read_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	Stream_Read_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
}

void ntlm_write_message_fields(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->MaxLen < 1)
		fields->MaxLen = fields->Len;

	Stream_Write_UINT16(s, fields->Len); /* Len (2 bytes) */
	Stream_Write_UINT16(s, fields->MaxLen); /* MaxLen (2 bytes) */
	Stream_Write_UINT32(s, fields->BufferOffset); /* BufferOffset (4 bytes) */
}

void ntlm_read_message_fields_buffer(wStream* s, NTLM_MESSAGE_FIELDS* fields)
{
	if (fields->Len > 0)
	{
		fields->Buffer = malloc(fields->Len);
		Stream_SetPosition(s, fields->BufferOffset);
		Stream_Read(s, fields->Buffer, fields->Len);
	}
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
	if (fields != NULL)
	{
		if (fields->Buffer != NULL)
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
	fprintf(stderr, "%s (Len: %d MaxLen: %d BufferOffset: %d)\n",
			name, fields->Len, fields->MaxLen, fields->BufferOffset);

	if (fields->Len > 0)
		winpr_HexDump(fields->Buffer, fields->Len);

	fprintf(stderr, "\n");
}

SECURITY_STATUS ntlm_read_NegotiateMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	int length;
	NTLM_NEGOTIATE_MESSAGE* message;

	message = &context->NEGOTIATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_NEGOTIATE_MESSAGE));

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_NEGOTIATE))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (!((message->NegotiateFlags & NTLMSSP_REQUEST_TARGET) &&
			(message->NegotiateFlags & NTLMSSP_NEGOTIATE_NTLM) &&
			(message->NegotiateFlags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN) &&
			(message->NegotiateFlags & NTLMSSP_NEGOTIATE_UNICODE)))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	context->NegotiateFlags = message->NegotiateFlags;

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message->DomainName));

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	ntlm_read_message_fields(s, &(message->Workstation));

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message->Version)); /* Version (8 bytes) */

	length = Stream_GetPosition(s);
	buffer->cbBuffer = length;

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "NEGOTIATE_MESSAGE (length = %d)\n", (int) context->NegotiateMessage.cbBuffer);
	winpr_HexDump(context->NegotiateMessage.pvBuffer, context->NegotiateMessage.cbBuffer);
	fprintf(stderr, "\n");

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
	int length;
	NTLM_NEGOTIATE_MESSAGE* message;

	message = &context->NEGOTIATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_NEGOTIATE_MESSAGE));

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

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

	sspi_SecBufferAlloc(&context->NegotiateMessage, length);
	CopyMemory(context->NegotiateMessage.pvBuffer, buffer->pvBuffer, buffer->cbBuffer);
	context->NegotiateMessage.BufferType = buffer->BufferType;

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "NEGOTIATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(Stream_Buffer(s), length);
	fprintf(stderr, "\n");

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

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

	StartOffset = Stream_Pointer(s);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_CHALLENGE))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	/* TargetNameFields (8 bytes) */
	ntlm_read_message_fields(s, &(message->TargetName));

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	context->NegotiateFlags = message->NegotiateFlags;

	Stream_Read(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	CopyMemory(context->ServerChallenge, message->ServerChallenge, 8);

	Stream_Read(s, message->Reserved, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	ntlm_read_message_fields(s, &(message->TargetInfo));

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message->Version)); /* Version (8 bytes) */

	/* Payload (variable) */
	PayloadOffset = Stream_Pointer(s);

	if (message->TargetName.Len > 0)
		ntlm_read_message_fields_buffer(s, &(message->TargetName));

	if (message->TargetInfo.Len > 0)
	{
		ntlm_read_message_fields_buffer(s, &(message->TargetInfo));

		context->ChallengeTargetInfo.pvBuffer = message->TargetInfo.Buffer;
		context->ChallengeTargetInfo.cbBuffer = message->TargetInfo.Len;

		AvTimestamp = ntlm_av_pair_get((NTLM_AV_PAIR*) message->TargetInfo.Buffer, MsvAvTimestamp);

		if (AvTimestamp != NULL)
		{
			if (context->NTLMv2)
				context->UseMIC = TRUE;

			CopyMemory(context->ChallengeTimestamp, ntlm_av_pair_get_value_pointer(AvTimestamp), 8);
		}
	}

	length = (PayloadOffset - StartOffset) + message->TargetName.Len + message->TargetInfo.Len;

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, StartOffset, length);

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	fprintf(stderr, "\n");

	ntlm_print_negotiate_flags(context->NegotiateFlags);

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->TargetName), "TargetName");
	ntlm_print_message_fields(&(message->TargetInfo), "TargetInfo");

	if (context->ChallengeTargetInfo.cbBuffer > 0)
	{
		fprintf(stderr, "ChallengeTargetInfo (%d):\n", (int) context->ChallengeTargetInfo.cbBuffer);
		ntlm_print_av_pair_list(context->ChallengeTargetInfo.pvBuffer);
	}
#endif
	/* AV_PAIRs */

	if (context->NTLMv2)
	{
		ntlm_construct_authenticate_target_info(context);
		sspi_SecBufferFree(&context->ChallengeTargetInfo);
		context->ChallengeTargetInfo.pvBuffer = context->AuthenticateTargetInfo.pvBuffer;
		context->ChallengeTargetInfo.cbBuffer = context->AuthenticateTargetInfo.cbBuffer;
	}

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* LmChallengeResponse */
	ntlm_compute_lm_v2_response(context);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

	/* RandomSessionKey */
	ntlm_generate_random_session_key(context);

	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	/* EncryptedRandomSessionKey */
	ntlm_encrypt_random_session_key(context);

	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);

	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);

	/* Initialize RC4 seal state using client sealing key */
	ntlm_init_rc4_seal_states(context);

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "ClientChallenge\n");
	winpr_HexDump(context->ClientChallenge, 8);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerChallenge\n");
	winpr_HexDump(context->ServerChallenge, 8);
	fprintf(stderr, "\n");

	fprintf(stderr, "SessionBaseKey\n");
	winpr_HexDump(context->SessionBaseKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "KeyExchangeKey\n");
	winpr_HexDump(context->KeyExchangeKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ExportedSessionKey\n");
	winpr_HexDump(context->ExportedSessionKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "RandomSessionKey\n");
	winpr_HexDump(context->RandomSessionKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ClientSigningKey\n");
	winpr_HexDump(context->ClientSigningKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ClientSealingKey\n");
	winpr_HexDump(context->ClientSealingKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerSigningKey\n");
	winpr_HexDump(context->ServerSigningKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerSealingKey\n");
	winpr_HexDump(context->ServerSealingKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "Timestamp\n");
	winpr_HexDump(context->Timestamp, 8);
	fprintf(stderr, "\n");
#endif

	context->state = NTLM_STATE_AUTHENTICATE;

	ntlm_free_message_fields_buffer(&(message->TargetName));

	Stream_Free(s, FALSE);

	return SEC_I_CONTINUE_NEEDED;
}

SECURITY_STATUS ntlm_write_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	int length;
	UINT32 PayloadOffset;
	NTLM_CHALLENGE_MESSAGE* message;

	message = &context->CHALLENGE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_CHALLENGE_MESSAGE));

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

	/* Version */
	ntlm_get_version_info(&(message->Version));

	/* Server Challenge */
	ntlm_generate_server_challenge(context);

	/* Timestamp */
	ntlm_generate_timestamp(context);

	/* TargetInfo */
	ntlm_construct_challenge_target_info(context);

	/* ServerChallenge */
	CopyMemory(message->ServerChallenge, context->ServerChallenge, 8);

	message->NegotiateFlags = context->NegotiateFlags;

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_CHALLENGE);

	/* Message Header (12 bytes) */
	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) message);

	if (message->NegotiateFlags & NTLMSSP_REQUEST_TARGET)
	{
		message->TargetName.Len = (UINT16) context->TargetName.cbBuffer;
		message->TargetName.Buffer = context->TargetName.pvBuffer;
	}

	message->NegotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_TARGET_INFO)
	{
		message->TargetInfo.Len = (UINT16) context->ChallengeTargetInfo.cbBuffer;
		message->TargetInfo.Buffer = context->ChallengeTargetInfo.pvBuffer;
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

	sspi_SecBufferAlloc(&context->ChallengeMessage, length);
	CopyMemory(context->ChallengeMessage.pvBuffer, Stream_Buffer(s), length);

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "CHALLENGE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	fprintf(stderr, "\n");

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
	int length;
	UINT32 flags;
	UINT32 MicOffset;
	NTLM_AV_PAIR* AvFlags;
	NTLMv2_RESPONSE response;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE* message;

	flags = 0;
	MicOffset = 0;
	AvFlags = NULL;

	message = &context->AUTHENTICATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_AUTHENTICATE_MESSAGE));

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

	ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*) message);

	if (!ntlm_validate_message_header(s, (NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_AUTHENTICATE))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	ntlm_read_message_fields(s, &(message->LmChallengeResponse)); /* LmChallengeResponseFields (8 bytes) */

	ntlm_read_message_fields(s, &(message->NtChallengeResponse)); /* NtChallengeResponseFields (8 bytes) */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	ntlm_read_message_fields(s, &(message->DomainName)); /* DomainNameFields (8 bytes) */

	ntlm_read_message_fields(s, &(message->UserName)); /* UserNameFields (8 bytes) */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	ntlm_read_message_fields(s, &(message->Workstation)); /* WorkstationFields (8 bytes) */

	ntlm_read_message_fields(s, &(message->EncryptedRandomSessionKey)); /* EncryptedRandomSessionKeyFields (8 bytes) */

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_read_version_info(s, &(message->Version)); /* Version (8 bytes) */

	PayloadBufferOffset = Stream_GetPosition(s);

	ntlm_read_message_fields_buffer(s, &(message->DomainName)); /* DomainName */

	ntlm_read_message_fields_buffer(s, &(message->UserName)); /* UserName */

	ntlm_read_message_fields_buffer(s, &(message->Workstation)); /* Workstation */

	ntlm_read_message_fields_buffer(s, &(message->LmChallengeResponse)); /* LmChallengeResponse */

	ntlm_read_message_fields_buffer(s, &(message->NtChallengeResponse)); /* NtChallengeResponse */

	if (message->NtChallengeResponse.Len > 0)
	{
		wStream* s = Stream_New(message->NtChallengeResponse.Buffer, message->NtChallengeResponse.Len);
		ntlm_read_ntlm_v2_response(s, &response);
		Stream_Free(s, FALSE);

		context->NtChallengeResponse.pvBuffer = message->NtChallengeResponse.Buffer;
		context->NtChallengeResponse.cbBuffer = message->NtChallengeResponse.Len;

		context->ChallengeTargetInfo.pvBuffer = (void*) response.Challenge.AvPairs;
		context->ChallengeTargetInfo.cbBuffer = message->NtChallengeResponse.Len - (28 + 16);

		CopyMemory(context->ClientChallenge, response.Challenge.ClientChallenge, 8);

		AvFlags = ntlm_av_pair_get(response.Challenge.AvPairs, MsvAvFlags);

		if (AvFlags != NULL)
			flags = *((UINT32*) ntlm_av_pair_get_value_pointer(AvFlags));
	}

	/* EncryptedRandomSessionKey */
	ntlm_read_message_fields_buffer(s, &(message->EncryptedRandomSessionKey));
	CopyMemory(context->EncryptedRandomSessionKey, message->EncryptedRandomSessionKey.Buffer, 16);

	length = Stream_GetPosition(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = length;

	Stream_SetPosition(s, PayloadBufferOffset);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		MicOffset = Stream_GetPosition(s);
		Stream_Read(s, message->MessageIntegrityCheck, 16);
		PayloadBufferOffset += 16;
	}

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "AUTHENTICATE_MESSAGE (length = %d)\n", (int) context->AuthenticateMessage.cbBuffer);
	winpr_HexDump(context->AuthenticateMessage.pvBuffer, context->AuthenticateMessage.cbBuffer);
	fprintf(stderr, "\n");

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->DomainName), "DomainName");
	ntlm_print_message_fields(&(message->UserName), "UserName");
	ntlm_print_message_fields(&(message->Workstation), "Workstation");
	ntlm_print_message_fields(&(message->LmChallengeResponse), "LmChallengeResponse");
	ntlm_print_message_fields(&(message->NtChallengeResponse), "NtChallengeResponse");
	ntlm_print_message_fields(&(message->EncryptedRandomSessionKey), "EncryptedRandomSessionKey");

	ntlm_print_av_pair_list(response.Challenge.AvPairs);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		fprintf(stderr, "MessageIntegrityCheck:\n");
		winpr_HexDump(message->MessageIntegrityCheck, 16);
	}
#endif

	if (message->UserName.Len > 0)
	{
		context->identity.User = (UINT16*) malloc(message->UserName.Len);
		CopyMemory(context->identity.User, message->UserName.Buffer, message->UserName.Len);
		context->identity.UserLength = message->UserName.Len / 2;
	}

	if (message->DomainName.Len > 0)
	{
		context->identity.Domain = (UINT16*) malloc(message->DomainName.Len);
		CopyMemory(context->identity.Domain, message->DomainName.Buffer, message->DomainName.Len);
		context->identity.DomainLength = message->DomainName.Len / 2;
	}

	/* LmChallengeResponse */
	ntlm_compute_lm_v2_response(context);

	/* NtChallengeResponse */
	ntlm_compute_ntlm_v2_response(context);

	/* KeyExchangeKey */
	ntlm_generate_key_exchange_key(context);

	/* EncryptedRandomSessionKey */
	ntlm_decrypt_random_session_key(context);

	/* ExportedSessionKey */
	ntlm_generate_exported_session_key(context);

	if (flags & MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK)
	{
		ZeroMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[MicOffset], 16);
		ntlm_compute_message_integrity_check(context);
		CopyMemory(&((PBYTE) context->AuthenticateMessage.pvBuffer)[MicOffset], message->MessageIntegrityCheck, 16);

		if (memcmp(context->MessageIntegrityCheck, message->MessageIntegrityCheck, 16) != 0)
		{
			fprintf(stderr, "Message Integrity Check (MIC) verification failed!\n");

			fprintf(stderr, "Expected MIC:\n");
			winpr_HexDump(context->MessageIntegrityCheck, 16);
			fprintf(stderr, "Actual MIC:\n");
			winpr_HexDump(message->MessageIntegrityCheck, 16);

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
	fprintf(stderr, "ClientChallenge\n");
	winpr_HexDump(context->ClientChallenge, 8);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerChallenge\n");
	winpr_HexDump(context->ServerChallenge, 8);
	fprintf(stderr, "\n");

	fprintf(stderr, "SessionBaseKey\n");
	winpr_HexDump(context->SessionBaseKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "KeyExchangeKey\n");
	winpr_HexDump(context->KeyExchangeKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ExportedSessionKey\n");
	winpr_HexDump(context->ExportedSessionKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "RandomSessionKey\n");
	winpr_HexDump(context->RandomSessionKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ClientSigningKey\n");
	winpr_HexDump(context->ClientSigningKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ClientSealingKey\n");
	winpr_HexDump(context->ClientSealingKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerSigningKey\n");
	winpr_HexDump(context->ServerSigningKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "ServerSealingKey\n");
	winpr_HexDump(context->ServerSealingKey, 16);
	fprintf(stderr, "\n");

	fprintf(stderr, "Timestamp\n");
	winpr_HexDump(context->Timestamp, 8);
	fprintf(stderr, "\n");
#endif

	context->state = NTLM_STATE_FINAL;

	Stream_Free(s, FALSE);

	ntlm_free_message_fields_buffer(&(message->DomainName));
	ntlm_free_message_fields_buffer(&(message->UserName));
	ntlm_free_message_fields_buffer(&(message->Workstation));
	ntlm_free_message_fields_buffer(&(message->LmChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->NtChallengeResponse));
	ntlm_free_message_fields_buffer(&(message->EncryptedRandomSessionKey));

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
	int length;
	UINT32 MicOffset = 0;
	UINT32 PayloadBufferOffset;
	NTLM_AUTHENTICATE_MESSAGE* message;

	message = &context->AUTHENTICATE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_AUTHENTICATE_MESSAGE));

	s = Stream_New(buffer->pvBuffer, buffer->cbBuffer);

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

	if (context->identity.DomainLength > 0)
	{
		message->NegotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		message->DomainName.Len = (UINT16) context->identity.DomainLength * 2;
		message->DomainName.Buffer = (BYTE*) context->identity.Domain;
	}

	message->UserName.Len = (UINT16) context->identity.UserLength * 2;
	message->UserName.Buffer = (BYTE*) context->identity.User;

	message->LmChallengeResponse.Len = (UINT16) context->LmChallengeResponse.cbBuffer;
	message->LmChallengeResponse.Buffer = (BYTE*) context->LmChallengeResponse.pvBuffer;

	//if (context->NTLMv2)
	//	ZeroMemory(message->LmChallengeResponse.Buffer, message->LmChallengeResponse.Len);

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
	message->LmChallengeResponse.BufferOffset = message->Workstation.BufferOffset + message->Workstation.Len;
	message->NtChallengeResponse.BufferOffset = message->LmChallengeResponse.BufferOffset + message->LmChallengeResponse.Len;
	message->EncryptedRandomSessionKey.BufferOffset = message->NtChallengeResponse.BufferOffset + message->NtChallengeResponse.Len;

	ntlm_populate_message_header((NTLM_MESSAGE_HEADER*) message, MESSAGE_TYPE_AUTHENTICATE);

	ntlm_write_message_header(s, (NTLM_MESSAGE_HEADER*) message); /* Message Header (12 bytes) */

	ntlm_write_message_fields(s, &(message->LmChallengeResponse)); /* LmChallengeResponseFields (8 bytes) */

	ntlm_write_message_fields(s, &(message->NtChallengeResponse)); /* NtChallengeResponseFields (8 bytes) */

	ntlm_write_message_fields(s, &(message->DomainName)); /* DomainNameFields (8 bytes) */

	ntlm_write_message_fields(s, &(message->UserName)); /* UserNameFields (8 bytes) */

	ntlm_write_message_fields(s, &(message->Workstation)); /* WorkstationFields (8 bytes) */

	ntlm_write_message_fields(s, &(message->EncryptedRandomSessionKey)); /* EncryptedRandomSessionKeyFields (8 bytes) */

	Stream_Write_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_write_version_info(s, &(message->Version)); /* Version (8 bytes) */

	if (context->UseMIC)
	{
		MicOffset = Stream_GetPosition(s);
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
		ntlm_write_message_fields_buffer(s, &(message->EncryptedRandomSessionKey)); /* EncryptedRandomSessionKey */

	length = Stream_GetPosition(s);
	sspi_SecBufferAlloc(&context->AuthenticateMessage, length);
	CopyMemory(context->AuthenticateMessage.pvBuffer, Stream_Buffer(s), length);
	buffer->cbBuffer = length;

	if (context->UseMIC)
	{
		/* Message Integrity Check */
		ntlm_compute_message_integrity_check(context);

		Stream_SetPosition(s, MicOffset);
		Stream_Write(s, context->MessageIntegrityCheck, 16);
		Stream_SetPosition(s, length);
	}

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "AUTHENTICATE_MESSAGE (length = %d)\n", length);
	winpr_HexDump(Stream_Buffer(s), length);
	fprintf(stderr, "\n");

	ntlm_print_negotiate_flags(message->NegotiateFlags);

	if (message->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	if (context->AuthenticateTargetInfo.cbBuffer > 0)
	{
		fprintf(stderr, "AuthenticateTargetInfo (%d):\n", (int) context->AuthenticateTargetInfo.cbBuffer);
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
		fprintf(stderr, "MessageIntegrityCheck (length = 16)\n");
		winpr_HexDump(context->MessageIntegrityCheck, 16);
		fprintf(stderr, "\n");
	}
#endif

	context->state = NTLM_STATE_FINAL;

	Stream_Free(s, FALSE);

	return SEC_I_COMPLETE_NEEDED;
}
