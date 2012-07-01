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

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "ntlm_av_pairs.h"

const char* const AV_PAIRS_STRINGS[] =
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

ULONG ntlm_av_pair_list_size(ULONG AvPairsCount, ULONG AvPairsValueLength)
{
	/* size of headers + value lengths + terminating MsvAvEOL AV_PAIR */
	return (AvPairsCount + 1) * sizeof(NTLM_AV_PAIR) + AvPairsValueLength;
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

NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, AV_ID AvId, LONG AvPairListSize)
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

		AvPairListSize -= ntlm_av_pair_get_next_offset(pAvPair);

		if (AvPairListSize <= 0)
			return NULL;

		pAvPair = ntlm_av_pair_get_next_pointer(pAvPair);
	}

	return NULL;
}

NTLM_AV_PAIR* ntlm_av_pair_add(NTLM_AV_PAIR* pAvPairList, AV_ID AvId, PUNICODE_STRING pValue, LONG AvPairListSize)
{
	NTLM_AV_PAIR* pAvPair;

	pAvPair = ntlm_av_pair_get(pAvPairList, MsvAvEOL, AvPairListSize);

	if (!pAvPair)
		return NULL;

	pAvPair->AvId = AvId;
	pAvPair->AvLen = pValue->Length;

	CopyMemory(ntlm_av_pair_get_value_pointer(pAvPair), pValue->Buffer, pValue->Length);

	return pAvPair;
}

/**
 * Input array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 * @param s
 */

void ntlm_input_av_pairs(NTLM_CONTEXT* context, PStream s)
{
	AV_ID AvId;
	UINT16 AvLen;
	BYTE* value;
	AV_PAIRS* av_pairs = context->av_pairs;

#ifdef WITH_DEBUG_NTLM
	printf("AV_PAIRS = {\n");
#endif

	do
	{
		value = NULL;
		StreamRead_UINT16(s, AvId);
		StreamRead_UINT16(s, AvLen);

		if (AvLen > 0)
		{
			if (AvId != MsvAvFlags)
			{
				value = malloc(AvLen);
				StreamRead(s, value, AvLen);
			}
			else
			{
				StreamRead_UINT32(s, av_pairs->Flags);
			}
		}

		switch (AvId)
		{
			case MsvAvNbComputerName:
				av_pairs->NbComputerName.length = AvLen;
				av_pairs->NbComputerName.value = value;
				break;

			case MsvAvNbDomainName:
				av_pairs->NbDomainName.length = AvLen;
				av_pairs->NbDomainName.value = value;
				break;

			case MsvAvDnsComputerName:
				av_pairs->DnsComputerName.length = AvLen;
				av_pairs->DnsComputerName.value = value;
				break;

			case MsvAvDnsDomainName:
				av_pairs->DnsDomainName.length = AvLen;
				av_pairs->DnsDomainName.value = value;
				break;

			case MsvAvDnsTreeName:
				av_pairs->DnsTreeName.length = AvLen;
				av_pairs->DnsTreeName.value = value;
				break;

			case MsvAvTimestamp:
				av_pairs->Timestamp.length = AvLen;
				av_pairs->Timestamp.value = value;
				break;

			case MsvAvRestrictions:
				av_pairs->Restrictions.length = AvLen;
				av_pairs->Restrictions.value = value;
				break;

			case MsvAvTargetName:
				av_pairs->TargetName.length = AvLen;
				av_pairs->TargetName.value = value;
				break;

			case MsvChannelBindings:
				av_pairs->ChannelBindings.length = AvLen;
				av_pairs->ChannelBindings.value = value;
				break;

			default:
				if (value != NULL)
					free(value);
				break;
		}

#ifdef WITH_DEBUG_NTLM
		if (AvId < 10)
			printf("\tAvId: %s, AvLen: %d\n", AV_PAIRS_STRINGS[AvId], AvLen);
		else
			printf("\tAvId: %s, AvLen: %d\n", "Unknown", AvLen);

		winpr_HexDump(value, AvLen);
#endif
	}
	while (AvId != MsvAvEOL);

#ifdef WITH_DEBUG_NTLM
	printf("}\n");
#endif
}

/**
 * Output array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 * @param s
 */

void ntlm_output_av_pairs(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	PStream s;
	AV_PAIRS* av_pairs = context->av_pairs;

	s = PStreamAllocAttach(buffer->pvBuffer, buffer->cbBuffer);

	if (av_pairs->NbDomainName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvNbDomainName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->NbDomainName.length); /* AvLen */
		StreamWrite(s, av_pairs->NbDomainName.value, av_pairs->NbDomainName.length); /* Value */
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvNbComputerName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->NbComputerName.length); /* AvLen */
		StreamWrite(s, av_pairs->NbComputerName.value, av_pairs->NbComputerName.length); /* Value */
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvDnsDomainName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->DnsDomainName.length); /* AvLen */
		StreamWrite(s, av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length); /* Value */
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvDnsComputerName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->DnsComputerName.length); /* AvLen */
		StreamWrite(s, av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length); /* Value */
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvDnsTreeName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->DnsTreeName.length); /* AvLen */
		StreamWrite(s, av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length); /* Value */
	}

	if (av_pairs->Timestamp.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvTimestamp); /* AvId */
		StreamWrite_UINT16(s, av_pairs->Timestamp.length); /* AvLen */
		StreamWrite(s, av_pairs->Timestamp.value, av_pairs->Timestamp.length); /* Value */
	}

	if (av_pairs->Flags > 0)
	{
		StreamWrite_UINT16(s, MsvAvFlags); /* AvId */
		StreamWrite_UINT16(s, 4); /* AvLen */
		StreamWrite_UINT32(s, av_pairs->Flags); /* Value */
	}

	if (av_pairs->Restrictions.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvRestrictions); /* AvId */
		StreamWrite_UINT16(s, av_pairs->Restrictions.length); /* AvLen */
		StreamWrite(s, av_pairs->Restrictions.value, av_pairs->Restrictions.length); /* Value */
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		StreamWrite_UINT16(s, MsvChannelBindings); /* AvId */
		StreamWrite_UINT16(s, av_pairs->ChannelBindings.length); /* AvLen */
		StreamWrite(s, av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length); /* Value */
	}

	if (av_pairs->TargetName.length > 0)
	{
		StreamWrite_UINT16(s, MsvAvTargetName); /* AvId */
		StreamWrite_UINT16(s, av_pairs->TargetName.length); /* AvLen */
		StreamWrite(s, av_pairs->TargetName.value, av_pairs->TargetName.length); /* Value */
	}

	/* This indicates the end of the AV_PAIR array */
	StreamWrite_UINT16(s, MsvAvEOL); /* AvId */
	StreamWrite_UINT16(s, 0); /* AvLen */

	if (context->ntlm_v2)
	{
		StreamZero(s, 8);
	}

	free(s);
}

/**
 * Compute AV_PAIRs length.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

int ntlm_compute_av_pairs_length(NTLM_CONTEXT* context)
{
	int length = 0;
	AV_PAIRS* av_pairs = context->av_pairs;

	if (av_pairs->NbDomainName.length > 0)
		length += av_pairs->NbDomainName.length + 4;

	if (av_pairs->NbComputerName.length > 0)
		length += av_pairs->NbComputerName.length + 4;

	if (av_pairs->DnsDomainName.length > 0)
		length += av_pairs->DnsDomainName.length + 4;

	if (av_pairs->DnsComputerName.length > 0)
		length += av_pairs->DnsComputerName.length + 4;

	if (av_pairs->DnsTreeName.length > 0)
		length += av_pairs->DnsTreeName.length + 4;

	if (av_pairs->Timestamp.length > 0)
		length += av_pairs->Timestamp.length;

	if (av_pairs->Flags > 0)
		length += 4 + 4;

	if (av_pairs->Restrictions.length > 0)
		length += av_pairs->Restrictions.length + 4;

	if (av_pairs->ChannelBindings.length > 0)
		length += av_pairs->ChannelBindings.length + 4;

	if (av_pairs->TargetName.length > 0)
		length += av_pairs->TargetName.length + 4;

	length += 4;

	if (context->ntlm_v2)
		length += 8;

	return length;
}

/**
 * Populate array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_populate_av_pairs(NTLM_CONTEXT* context)
{
	int length;
	AV_PAIRS* av_pairs = context->av_pairs;

	/* MsvAvFlags */
	av_pairs->Flags = 0x00000002; /* Indicates the present of a Message Integrity Check (MIC) */

	/* Restriction_Encoding */
	ntlm_output_restriction_encoding(context);

	/* TargetName */
	ntlm_output_target_name(context);

	/* ChannelBindings */
	ntlm_output_channel_bindings(context);

	length = ntlm_compute_av_pairs_length(context);
	sspi_SecBufferAlloc(&context->TargetInfo, length);
	ntlm_output_av_pairs(context, &context->TargetInfo);
}

void ntlm_get_target_computer_name(PUNICODE_STRING pName, COMPUTER_NAME_FORMAT type)
{
	char* name;
	DWORD nSize = 0;

	GetComputerNameExA(type, NULL, &nSize);
	name = malloc(nSize);
	GetComputerNameExA(type, name, &nSize);

	if (type == ComputerNameNetBIOS)
		CharUpperA(name);

	pName->Length = strlen(name) * 2;
	pName->Buffer = (PWSTR) malloc(pName->Length);
	MultiByteToWideChar(CP_ACP, 0, name, strlen(name),
			(LPWSTR) pName->Buffer, pName->Length / 2);

	pName->MaximumLength = pName->Length;

	free(name);
}

void ntlm_construct_server_target_info(NTLM_CONTEXT* context)
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
	UNICODE_STRING Timestamp;

	ntlm_get_target_computer_name(&NbDomainName, ComputerNameNetBIOS);
	ntlm_get_target_computer_name(&NbComputerName, ComputerNameNetBIOS);
	ntlm_get_target_computer_name(&DnsDomainName, ComputerNameDnsDomain);
	ntlm_get_target_computer_name(&DnsComputerName, ComputerNameDnsHostname);

	Timestamp.Buffer = (PWSTR) context->Timestamp;
	Timestamp.Length = sizeof(context->Timestamp);

	AvPairsCount = 5;
	AvPairsLength = NbDomainName.Length + NbComputerName.Length +
			DnsDomainName.Length + DnsComputerName.Length + 8;

	length = ntlm_av_pair_list_size(AvPairsCount, AvPairsLength);
	sspi_SecBufferAlloc(&context->TargetInfo, length);

	pAvPairList = (NTLM_AV_PAIR*) context->TargetInfo.pvBuffer;
	AvPairListSize = (ULONG) context->TargetInfo.cbBuffer;

	ntlm_av_pair_list_init(pAvPairList);
	ntlm_av_pair_add(pAvPairList, MsvAvNbDomainName, &NbDomainName, AvPairListSize);
	ntlm_av_pair_add(pAvPairList, MsvAvNbComputerName, &NbComputerName, AvPairListSize);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsDomainName, &DnsDomainName, AvPairListSize);
	ntlm_av_pair_add(pAvPairList, MsvAvDnsComputerName, &DnsComputerName, AvPairListSize);
	ntlm_av_pair_add(pAvPairList, MsvAvTimestamp, &Timestamp, AvPairListSize);
}

/**
 * Print array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_print_av_pairs(NTLM_CONTEXT* context)
{
	AV_PAIRS* av_pairs = context->av_pairs;

	printf("AV_PAIRS = {\n");

	if (av_pairs->NbDomainName.length > 0)
	{
		printf("\tAvId: MsvAvNbDomainName AvLen: %d\n", av_pairs->NbDomainName.length);
		winpr_HexDump(av_pairs->NbDomainName.value, av_pairs->NbDomainName.length);
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		printf("\tAvId: MsvAvNbComputerName AvLen: %d\n", av_pairs->NbComputerName.length);
		winpr_HexDump(av_pairs->NbComputerName.value, av_pairs->NbComputerName.length);
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		printf("\tAvId: MsvAvDnsDomainName AvLen: %d\n", av_pairs->DnsDomainName.length);
		winpr_HexDump(av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length);
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		printf("\tAvId: MsvAvDnsComputerName AvLen: %d\n", av_pairs->DnsComputerName.length);
		winpr_HexDump(av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length);
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		printf("\tAvId: MsvAvDnsTreeName AvLen: %d\n", av_pairs->DnsTreeName.length);
		winpr_HexDump(av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length);
	}

	if (av_pairs->Timestamp.length > 0)
	{
		printf("\tAvId: MsvAvTimestamp AvLen: %d\n", av_pairs->Timestamp.length);
		winpr_HexDump(av_pairs->Timestamp.value, av_pairs->Timestamp.length);
	}

	if (av_pairs->Flags > 0)
	{
		printf("\tAvId: MsvAvFlags AvLen: %d\n", 4);
		printf("0x%08X\n", av_pairs->Flags);
	}

	if (av_pairs->Restrictions.length > 0)
	{
		printf("\tAvId: MsvAvRestrictions AvLen: %d\n", av_pairs->Restrictions.length);
		winpr_HexDump(av_pairs->Restrictions.value, av_pairs->Restrictions.length);
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		printf("\tAvId: MsvChannelBindings AvLen: %d\n", av_pairs->ChannelBindings.length);
		winpr_HexDump(av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length);
	}

	if (av_pairs->TargetName.length > 0)
	{
		printf("\tAvId: MsvAvTargetName AvLen: %d\n", av_pairs->TargetName.length);
		winpr_HexDump(av_pairs->TargetName.value, av_pairs->TargetName.length);
	}

	printf("}\n");
}

/**
 * Free array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param NTLM context
 */

void ntlm_free_av_pairs(NTLM_CONTEXT* context)
{
	AV_PAIRS* av_pairs = context->av_pairs;

	if (av_pairs != NULL)
	{
		if (av_pairs->NbComputerName.value != NULL)
			free(av_pairs->NbComputerName.value);
		if (av_pairs->NbDomainName.value != NULL)
			free(av_pairs->NbDomainName.value);
		if (av_pairs->DnsComputerName.value != NULL)
			free(av_pairs->DnsComputerName.value);
		if (av_pairs->DnsDomainName.value != NULL)
			free(av_pairs->DnsDomainName.value);
		if (av_pairs->DnsTreeName.value != NULL)
			free(av_pairs->DnsTreeName.value);
		if (av_pairs->Timestamp.value != NULL)
			free(av_pairs->Timestamp.value);
		if (av_pairs->Restrictions.value != NULL)
			free(av_pairs->Restrictions.value);
		if (av_pairs->TargetName.value != NULL)
			free(av_pairs->TargetName.value);
		if (av_pairs->ChannelBindings.value != NULL)
			free(av_pairs->ChannelBindings.value);

		free(av_pairs);
	}

	context->av_pairs = NULL;
}
