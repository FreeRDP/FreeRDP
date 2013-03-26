/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (AV_PAIRs)
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
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_av_pairs.h"

const char* const AV_PAIR_STRINGS[] =
{
	"MsvAvEOL",
	"MsvAvNbComputerName",
	"MsvAvNbDomainName",
	"MsvAvDnsComputerName",
	"MsvAvDnsDomainName",
	"MsvAvDnsTreeName",
	"MsvAvFlags",
	"MsvAvTimestamp",
	"MsvAvRestrictions",
	"MsvAvTargetName",
	"MsvChannelBindings"
};

void ntlm_av_pair_list_init(NTLM_AV_PAIR* pAvPairList)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	pAvPair->AvId = MsvAvEOL;
	pAvPair->AvLen = 0;
}

ULONG ntlm_av_pair_list_length(NTLM_AV_PAIR* pAvPairList)
{
	ULONG length;
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return 0;

	while (pAvPair->AvId != MsvAvEOL)
	{
		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	length = (pAvPair - pAvPairList) + sizeof(NTLM_AV_PAIR);

	return length;
}

void ntlm_print_av_pair_list(NTLM_AV_PAIR* pAvPairList)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return;

	fprintf(stderr, "AV_PAIRs =\n{\n");

	while (pAvPair->AvId != MsvAvEOL)
	{
		fprintf(stderr, "\t%s AvId: %d AvLen: %d\n",
				AV_PAIR_STRINGS[pAvPair->AvId],
				pAvPair->AvId, pAvPair->AvLen);

		winpr_HexDump(ntlm_av_pair_get_value_pointer(pAvPair), pAvPair->AvLen);

		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	fprintf(stderr, "}\n");
}

ULONG ntlm_av_pair_list_size(ULONG AvPairsCount, ULONG AvPairsValueLength)
{
	/* size of headers + value lengths + terminating MsvAvEOL AV_PAIR */
	return ((AvPairsCount + 1) * 4) + AvPairsValueLength;
}

PBYTE ntlm_av_pair_get_value_pointer(NTLM_AV_PAIR* pAvPair)
{
	return &((PBYTE) pAvPair)[sizeof(NTLM_AV_PAIR)];
}

int ntlm_av_pair_get_next_offset(NTLM_AV_PAIR* pAvPair)
{
	return pAvPair->AvLen + sizeof(NTLM_AV_PAIR);
}

NTLM_AV_PAIR* ntlm_av_pair_get_next_pointer(NTLM_AV_PAIR* pAvPair)
{
	return (NTLM_AV_PAIR*) ((PBYTE) pAvPair + ntlm_av_pair_get_next_offset(pAvPair));
}

NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair)
		return NULL;

	while (1)
	{
		if (pAvPair->AvId == AvId)
			return pAvPair;

		if (pAvPair->AvId == MsvAvEOL)
			return NULL;

		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	return NULL;
}

NTLM_AV_PAIR* ntlm_av_pair_add(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId, PBYTE Value, UINT16 AvLen)
{
	NTLM_AV_PAIR* pAvPair;

	pAvPair = ntlm_av_pair_get(pAvPairList, MsvAvEOL);

	if (!pAvPair)
		return NULL;

	pAvPair->AvId = AvId;
	pAvPair->AvLen = AvLen;

	CopyMemory(ntlm_av_pair_get_value_pointer(pAvPair), Value, AvLen);

	return pAvPair;
}

NTLM_AV_PAIR* ntlm_av_pair_add_copy(NTLM_AV_PAIR* pAvPairList, NTLM_AV_PAIR* pAvPair)
{
	NTLM_AV_PAIR* pAvPairCopy;

	pAvPairCopy = ntlm_av_pair_get(pAvPairList, MsvAvEOL);

	if (!pAvPairCopy)
		return NULL;

	pAvPairCopy->AvId = pAvPair->AvId;
	pAvPairCopy->AvLen = pAvPair->AvLen;

	CopyMemory(ntlm_av_pair_get_value_pointer(pAvPairCopy),
			ntlm_av_pair_get_value_pointer(pAvPair), pAvPair->AvLen);

	return pAvPairCopy;
}

void ntlm_get_target_computer_name(PUNICODE_STRING pName, COMPUTER_NAME_FORMAT type)
{
	char* name;
	int length;
	DWORD nSize = 0;

	GetComputerNameExA(type, NULL, &nSize);
	name = malloc(nSize);
	GetComputerNameExA(type, name, &nSize);

	if (type == ComputerNameNetBIOS)
		CharUpperA(name);

	length = ConvertToUnicode(CP_UTF8, 0, name, -1, &pName->Buffer, 0);

	pName->Length = (length - 1) / 2;
	pName->MaximumLength = pName->Length;

	free(name);
}

void ntlm_free_unicode_string(PUNICODE_STRING string)
{
	if (string != NULL)
	{
		if (string->Length > 0)
		{
			if (string->Buffer != NULL)
				free(string->Buffer);

			string->Buffer = NULL;
			string->Length = 0;
			string->MaximumLength = 0;
		}
	}
}

/**
 * From http://www.ietf.org/proceedings/72/slides/sasl-2.pdf:
 *
 * tls-server-end-point:
 *
 * The hash of the TLS server's end entity certificate as it appears, octet for octet,
 * in the server's Certificate message (note that the Certificate message contains a
 * certificate_list, the first element of which is the server's end entity certificate.)
 * The hash function to be selected is as follows: if the certificate's signature hash
 * algorithm is either MD5 or SHA-1, then use SHA-256, otherwise use the certificate's
 * signature hash algorithm.
 */

/**
 * Channel Bindings sample usage:
 * https://raw.github.com/mozilla/mozilla-central/master/extensions/auth/nsAuthSSPI.cpp
 */

/*
typedef struct gss_channel_bindings_struct {
	OM_uint32 initiator_addrtype;
	gss_buffer_desc initiator_address;
	OM_uint32 acceptor_addrtype;
	gss_buffer_desc acceptor_address;
	gss_buffer_desc application_data;
} *gss_channel_bindings_t;
 */

static void ntlm_md5_update_uint32_be(MD5_CTX* md5, UINT32 num)
{
	BYTE be32[4];

	be32[0] = (num >> 0) & 0xFF;
	be32[1] = (num >> 8) & 0xFF;
	be32[2] = (num >> 16) & 0xFF;
	be32[3] = (num >> 24) & 0xFF;

	MD5_Update(md5, be32, 4);
}

void ntlm_compute_channel_bindings(NTLM_CONTEXT* context)
{
	MD5_CTX md5;
	BYTE* ChannelBindingToken;
	UINT32 ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;

	ZeroMemory(context->ChannelBindingsHash, 16);
	ChannelBindings = context->Bindings.Bindings;

	if (!ChannelBindings)
		return;

	ChannelBindingTokenLength = context->Bindings.BindingsLength - sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*) ChannelBindings)[ChannelBindings->dwApplicationDataOffset];

	MD5_Init(&md5);

	ntlm_md5_update_uint32_be(&md5, ChannelBindings->dwInitiatorAddrType);
	ntlm_md5_update_uint32_be(&md5, ChannelBindings->cbInitiatorLength);
	ntlm_md5_update_uint32_be(&md5, ChannelBindings->dwAcceptorAddrType);
	ntlm_md5_update_uint32_be(&md5, ChannelBindings->cbAcceptorLength);
	ntlm_md5_update_uint32_be(&md5, ChannelBindings->cbApplicationDataLength);

	MD5_Update(&md5, (void*) ChannelBindingToken, ChannelBindingTokenLength);

	MD5_Final(context->ChannelBindingsHash, &md5);
}

void ntlm_compute_single_host_data(NTLM_CONTEXT* context)
{
	/**
	 * The Single_Host_Data structure allows a client to send machine-specific information
	 * within an authentication exchange to services on the same machine. The client can
	 * produce additional information to be processed in an implementation-specific way when
	 * the client and server are on the same host. If the server and client platforms are
	 * different or if they are on different hosts, then the information MUST be ignored.
	 * Any fields after the MachineID field MUST be ignored on receipt.
	 */

	context->SingleHostData.Size = 48;
	context->SingleHostData.Z4 = 0;
	context->SingleHostData.DataPresent = 1;
	context->SingleHostData.CustomData = SECURITY_MANDATORY_MEDIUM_RID;
	FillMemory(context->SingleHostData.MachineID, 32, 0xAA);
}

void ntlm_construct_challenge_target_info(NTLM_CONTEXT* context)
{
	int length;
	ULONG AvPairsCount;
	ULONG AvPairsLength;
	LONG AvPairListSize;
	NTLM_AV_PAIR* pAvPairList;
	UNICODE_STRING NbDomainName;
	UNICODE_STRING NbComputerName;
	UNICODE_STRING DnsDomainName;
	UNICODE_STRING DnsComputerName;

	NbDomainName.Buffer = NULL;
	ntlm_get_target_computer_name(&NbDomainName, ComputerNameNetBIOS);

	NbComputerName.Buffer = NULL;
	ntlm_get_target_computer_name(&NbComputerName, ComputerNameNetBIOS);

	DnsDomainName.Buffer = NULL;
	ntlm_get_target_computer_name(&DnsDomainName, ComputerNameDnsDomain);

	DnsComputerName.Buffer = NULL;
	ntlm_get_target_computer_name(&DnsComputerName, ComputerNameDnsHostname);

	AvPairsCount = 5;
	AvPairsLength = NbDomainName.Length + NbComputerName.Length +
			DnsDomainName.Length + DnsComputerName.Length + 8;

	length = ntlm_av_pair_list_size(AvPairsCount, AvPairsLength);
	sspi_SecBufferAlloc(&context->ChallengeTargetInfo, length);

	pAvPairList = (NTLM_AV_PAIR*) context->ChallengeTargetInfo.pvBuffer;
	AvPairListSize = (ULONG) context->ChallengeTargetInfo.cbBuffer;

	ntlm_av_pair_list_init(pAvPairList);
	ntlm_av_pair_add(pAvPairList, MsvAvNbDomainName, (PBYTE) NbDomainName.Buffer, NbDomainName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvNbComputerName, (PBYTE) NbComputerName.Buffer, NbComputerName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsDomainName, (PBYTE) DnsDomainName.Buffer, DnsDomainName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsComputerName, (PBYTE) DnsComputerName.Buffer, DnsComputerName.Length);
	ntlm_av_pair_add(pAvPairList, MsvAvTimestamp, context->Timestamp, sizeof(context->Timestamp));

	ntlm_free_unicode_string(&NbDomainName);
	ntlm_free_unicode_string(&NbComputerName);
	ntlm_free_unicode_string(&DnsDomainName);
	ntlm_free_unicode_string(&DnsComputerName);
}

void ntlm_construct_authenticate_target_info(NTLM_CONTEXT* context)
{
	ULONG size;
	ULONG AvPairsCount;
	ULONG AvPairsValueLength;
	NTLM_AV_PAIR* AvTimestamp;
	NTLM_AV_PAIR* AvNbDomainName;
	NTLM_AV_PAIR* AvNbComputerName;
	NTLM_AV_PAIR* AvDnsDomainName;
	NTLM_AV_PAIR* AvDnsComputerName;
	NTLM_AV_PAIR* AvDnsTreeName;
	NTLM_AV_PAIR* ChallengeTargetInfo;
	NTLM_AV_PAIR* AuthenticateTargetInfo;

	AvPairsCount = 1;
	AvPairsValueLength = 0;
	ChallengeTargetInfo = (NTLM_AV_PAIR*) context->ChallengeTargetInfo.pvBuffer;

	AvNbDomainName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvNbDomainName);
	AvNbComputerName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvNbComputerName);
	AvDnsDomainName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsDomainName);
	AvDnsComputerName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsComputerName);
	AvDnsTreeName = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvDnsTreeName);
	AvTimestamp = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvTimestamp);

	if (AvNbDomainName != NULL)
	{
		AvPairsCount++; /* MsvAvNbDomainName */
		AvPairsValueLength += AvNbDomainName->AvLen;
	}

	if (AvNbComputerName != NULL)
	{
		AvPairsCount++; /* MsvAvNbComputerName */
		AvPairsValueLength += AvNbComputerName->AvLen;
	}

	if (AvDnsDomainName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsDomainName */
		AvPairsValueLength += AvDnsDomainName->AvLen;
	}

	if (AvDnsComputerName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsComputerName */
		AvPairsValueLength += AvDnsComputerName->AvLen;
	}

	if (AvDnsTreeName != NULL)
	{
		AvPairsCount++; /* MsvAvDnsTreeName */
		AvPairsValueLength += AvDnsTreeName->AvLen;
	}

	AvPairsCount++; /* MsvAvTimestamp */
	AvPairsValueLength += 8;

	if (context->UseMIC)
	{
		AvPairsCount++; /* MsvAvFlags */
		AvPairsValueLength += 4;
	}

	if (context->SendSingleHostData)
	{
		AvPairsCount++; /* MsvAvSingleHost */
		ntlm_compute_single_host_data(context);
		AvPairsValueLength += context->SingleHostData.Size;
	}

	/**
	 * Extended Protection for Authentication:
	 * http://blogs.technet.com/b/srd/archive/2009/12/08/extended-protection-for-authentication.aspx
	 */

	if (!context->SuppressExtendedProtection)
	{
		/**
		 * SEC_CHANNEL_BINDINGS structure
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd919963/
		 */

		AvPairsCount++; /* MsvChannelBindings */
		AvPairsValueLength += 16;
		ntlm_compute_channel_bindings(context);

		if (context->ServicePrincipalName.Length > 0)
		{
			AvPairsCount++; /* MsvAvTargetName */
			AvPairsValueLength += context->ServicePrincipalName.Length;
		}
	}

	size = ntlm_av_pair_list_size(AvPairsCount, AvPairsValueLength);

	if (context->NTLMv2)
		size += 8; /* unknown 8-byte padding */

	sspi_SecBufferAlloc(&context->AuthenticateTargetInfo, size);
	AuthenticateTargetInfo = (NTLM_AV_PAIR*) context->AuthenticateTargetInfo.pvBuffer;

	ntlm_av_pair_list_init(AuthenticateTargetInfo);

	if (AvNbDomainName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvNbDomainName);

	if (AvNbComputerName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvNbComputerName);

	if (AvDnsDomainName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsDomainName);

	if (AvDnsComputerName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsComputerName);

	if (AvDnsTreeName != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvDnsTreeName);

	if (AvTimestamp != NULL)
		ntlm_av_pair_add_copy(AuthenticateTargetInfo, AvTimestamp);

	if (context->UseMIC)
	{
		UINT32 flags = MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK;
		ntlm_av_pair_add(AuthenticateTargetInfo, MsvAvFlags, (PBYTE) &flags, 4);
	}

	if (context->SendSingleHostData)
	{
		ntlm_av_pair_add(AuthenticateTargetInfo, MsvAvSingleHost,
				(PBYTE) &context->SingleHostData, context->SingleHostData.Size);
	}

	if (!context->SuppressExtendedProtection)
	{
		ntlm_av_pair_add(AuthenticateTargetInfo, MsvChannelBindings, context->ChannelBindingsHash, 16);

		if (context->ServicePrincipalName.Length > 0)
		{
			ntlm_av_pair_add(AuthenticateTargetInfo, MsvAvTargetName,
					(PBYTE) context->ServicePrincipalName.Buffer,
					context->ServicePrincipalName.Length);
		}
	}

	if (context->NTLMv2)
	{
		NTLM_AV_PAIR* AvEOL;

		AvEOL = ntlm_av_pair_get(ChallengeTargetInfo, MsvAvEOL);
		ZeroMemory((void*) AvEOL, 4);
	}
}
