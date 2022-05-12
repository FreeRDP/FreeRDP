/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (AV_PAIRs)
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

#include <winpr/assert.h>

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>
#include <winpr/tchar.h>
#include <winpr/crypto.h>

#include "ntlm_compute.h"

#include "ntlm_av_pairs.h"

#include "../../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

static BOOL ntlm_av_pair_get_next_offset(const NTLM_AV_PAIR* pAvPair, size_t size, size_t* pOffset);

static BOOL ntlm_av_pair_check_data(const NTLM_AV_PAIR* pAvPair, size_t cbAvPair, size_t size)
{
	size_t offset;
	if (!pAvPair || cbAvPair < sizeof(NTLM_AV_PAIR) + size)
		return FALSE;
	if (!ntlm_av_pair_get_next_offset(pAvPair, cbAvPair, &offset))
		return FALSE;
	return cbAvPair >= offset;
}

static const char* get_av_pair_string(UINT16 pair)
{
	switch (pair)
	{
		case MsvAvEOL:
			return "MsvAvEOL";
		case MsvAvNbComputerName:
			return "MsvAvNbComputerName";
		case MsvAvNbDomainName:
			return "MsvAvNbDomainName";
		case MsvAvDnsComputerName:
			return "MsvAvDnsComputerName";
		case MsvAvDnsDomainName:
			return "MsvAvDnsDomainName";
		case MsvAvDnsTreeName:
			return "MsvAvDnsTreeName";
		case MsvAvFlags:
			return "MsvAvFlags";
		case MsvAvTimestamp:
			return "MsvAvTimestamp";
		case MsvAvSingleHost:
			return "MsvAvSingleHost";
		case MsvAvTargetName:
			return "MsvAvTargetName";
		case MsvAvChannelBindings:
			return "MsvAvChannelBindings";
		default:
			return "UNKNOWN";
	}
}

static BOOL ntlm_av_pair_check(const NTLM_AV_PAIR* pAvPair, size_t cbAvPair);
static NTLM_AV_PAIR* ntlm_av_pair_next(NTLM_AV_PAIR* pAvPairList, size_t* pcbAvPairList);

static INLINE void ntlm_av_pair_set_id(NTLM_AV_PAIR* pAvPair, UINT16 id)
{
	WINPR_ASSERT(pAvPair);
	Data_Write_UINT16(&pAvPair->AvId, id);
}

static INLINE void ntlm_av_pair_set_len(NTLM_AV_PAIR* pAvPair, UINT16 len)
{
	WINPR_ASSERT(pAvPair);
	Data_Write_UINT16(&pAvPair->AvLen, len);
}

static BOOL ntlm_av_pair_list_init(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList)
{
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!pAvPair || (cbAvPairList < sizeof(NTLM_AV_PAIR)))
		return FALSE;

	ntlm_av_pair_set_id(pAvPair, MsvAvEOL);
	ntlm_av_pair_set_len(pAvPair, 0);
	return TRUE;
}

static INLINE BOOL ntlm_av_pair_get_id(const NTLM_AV_PAIR* pAvPair, size_t size, UINT16* pair)
{
	UINT16 AvId;
	if (!pAvPair || !pair)
		return FALSE;

	if (size < sizeof(NTLM_AV_PAIR))
		return FALSE;

	Data_Read_UINT16(&pAvPair->AvId, AvId);

	*pair = AvId;
	return TRUE;
}

ULONG ntlm_av_pair_list_length(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList)
{
	size_t size;
	size_t cbAvPair;
	NTLM_AV_PAIR* pAvPair;

	pAvPair = ntlm_av_pair_get(pAvPairList, cbAvPairList, MsvAvEOL, &cbAvPair);
	if (!pAvPair)
		return 0;

	size = ((PBYTE)pAvPair - (PBYTE)pAvPairList) + sizeof(NTLM_AV_PAIR);
	WINPR_ASSERT(size <= ULONG_MAX);
	return (ULONG)size;
}

static INLINE BOOL ntlm_av_pair_get_len(const NTLM_AV_PAIR* pAvPair, size_t size, size_t* pAvLen)
{
	UINT16 AvLen;
	if (!pAvPair)
		return FALSE;

	if (size < sizeof(NTLM_AV_PAIR))
		return FALSE;

	Data_Read_UINT16(&pAvPair->AvLen, AvLen);

	*pAvLen = AvLen;
	return TRUE;
}

#ifdef WITH_DEBUG_NTLM
void ntlm_print_av_pair_list(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList)
{
	UINT16 pair;
	size_t cbAvPair = cbAvPairList;
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!ntlm_av_pair_check(pAvPair, cbAvPair))
		return;

	WLog_VRB(TAG, "AV_PAIRs =");

	while (pAvPair && ntlm_av_pair_get_id(pAvPair, cbAvPair, &pair) && (pair != MsvAvEOL))
	{
		size_t cbLen = 0;
		ntlm_av_pair_get_len(pAvPair, cbAvPair, &cbLen);

		WLog_VRB(TAG, "\t%s AvId: %" PRIu16 " AvLen: %" PRIu16 "", get_av_pair_string(pair), pair);
		winpr_HexDump(TAG, WLOG_TRACE, ntlm_av_pair_get_value_pointer(pAvPair), cbLen);

		pAvPair = ntlm_av_pair_next(pAvPair, &cbAvPair);
	}
}
#endif

static ULONG ntlm_av_pair_list_size(ULONG AvPairsCount, ULONG AvPairsValueLength)
{
	/* size of headers + value lengths + terminating MsvAvEOL AV_PAIR */
	return ((AvPairsCount + 1) * 4) + AvPairsValueLength;
}

PBYTE ntlm_av_pair_get_value_pointer(NTLM_AV_PAIR* pAvPair)
{
	WINPR_ASSERT(pAvPair);
	return (PBYTE)pAvPair + sizeof(NTLM_AV_PAIR);
}

static BOOL ntlm_av_pair_get_next_offset(const NTLM_AV_PAIR* pAvPair, size_t size, size_t* pOffset)
{
	size_t avLen;
	if (!pOffset)
		return FALSE;

	if (!ntlm_av_pair_get_len(pAvPair, size, &avLen))
		return FALSE;
	*pOffset = avLen + sizeof(NTLM_AV_PAIR);
	return TRUE;
}

static BOOL ntlm_av_pair_check(const NTLM_AV_PAIR* pAvPair, size_t cbAvPair)
{
	return ntlm_av_pair_check_data(pAvPair, cbAvPair, 0);
}

static NTLM_AV_PAIR* ntlm_av_pair_next(NTLM_AV_PAIR* pAvPair, size_t* pcbAvPair)
{
	size_t offset;

	if (!pcbAvPair)
		return NULL;
	if (!ntlm_av_pair_check(pAvPair, *pcbAvPair))
		return NULL;

	if (!ntlm_av_pair_get_next_offset(pAvPair, *pcbAvPair, &offset))
		return NULL;

	*pcbAvPair -= offset;
	return (NTLM_AV_PAIR*)((PBYTE)pAvPair + offset);
}

NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList, NTLM_AV_ID AvId,
                               size_t* pcbAvPairListRemaining)
{
	UINT16 id;
	size_t cbAvPair = cbAvPairList;
	NTLM_AV_PAIR* pAvPair = pAvPairList;

	if (!ntlm_av_pair_check(pAvPair, cbAvPair))
		pAvPair = NULL;

	while (pAvPair && ntlm_av_pair_get_id(pAvPair, cbAvPair, &id))
	{
		if (id == AvId)
			break;
		if (id == MsvAvEOL)
		{
			pAvPair = NULL;
			break;
		}

		pAvPair = ntlm_av_pair_next(pAvPair, &cbAvPair);
	}

	if (!pAvPair)
		cbAvPair = 0;
	if (pcbAvPairListRemaining)
		*pcbAvPairListRemaining = cbAvPair;

	return pAvPair;
}

static BOOL ntlm_av_pair_add(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList, NTLM_AV_ID AvId,
                             PBYTE Value, UINT16 AvLen)
{
	size_t cbAvPair;
	NTLM_AV_PAIR* pAvPair;

	pAvPair = ntlm_av_pair_get(pAvPairList, cbAvPairList, MsvAvEOL, &cbAvPair);

	/* size of header + value length + terminating MsvAvEOL AV_PAIR */
	if (!pAvPair || cbAvPair < 2 * sizeof(NTLM_AV_PAIR) + AvLen)
		return FALSE;

	ntlm_av_pair_set_id(pAvPair, (UINT16)AvId);
	ntlm_av_pair_set_len(pAvPair, AvLen);
	if (AvLen)
	{
		WINPR_ASSERT(Value != NULL);
		CopyMemory(ntlm_av_pair_get_value_pointer(pAvPair), Value, AvLen);
	}

	pAvPair = ntlm_av_pair_next(pAvPair, &cbAvPair);
	return ntlm_av_pair_list_init(pAvPair, cbAvPair);
}

static BOOL ntlm_av_pair_add_copy(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList,
                                  NTLM_AV_PAIR* pAvPair, size_t cbAvPair)
{
	UINT16 pair;
	size_t avLen;

	if (!ntlm_av_pair_check(pAvPair, cbAvPair))
		return FALSE;

	if (!ntlm_av_pair_get_id(pAvPair, cbAvPair, &pair))
		return FALSE;

	if (!ntlm_av_pair_get_len(pAvPair, cbAvPair, &avLen))
		return FALSE;

	WINPR_ASSERT(avLen <= UINT16_MAX);
	return ntlm_av_pair_add(pAvPairList, cbAvPairList, pair,
	                        ntlm_av_pair_get_value_pointer(pAvPair), (UINT16)avLen);
}

static int ntlm_get_target_computer_name(PUNICODE_STRING pName, COMPUTER_NAME_FORMAT type)
{
	char* name;
	int status;
	DWORD nSize = 0;
	CHAR* computerName;

	WINPR_ASSERT(pName);

	if (GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize) || GetLastError() != ERROR_MORE_DATA)
		return -1;

	computerName = calloc(nSize, sizeof(CHAR));

	if (!computerName)
		return -1;

	if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
	{
		free(computerName);
		return -1;
	}

	if (nSize > MAX_COMPUTERNAME_LENGTH)
		computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

	name = computerName;

	if (!name)
		return -1;

	if (type == ComputerNameNetBIOS)
		CharUpperA(name);

	status = ConvertToUnicode(CP_UTF8, 0, name, -1, &pName->Buffer, 0);

	if (status <= 0)
	{
		free(name);
		return status;
	}

	pName->Length = (USHORT)((status - 1) * 2);
	pName->MaximumLength = pName->Length;
	free(name);
	return 1;
}

static void ntlm_free_unicode_string(PUNICODE_STRING string)
{
	if (string)
	{
		if (string->Length > 0)
		{
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

static BOOL ntlm_md5_update_uint32_be(WINPR_DIGEST_CTX* md5, UINT32 num)
{
	BYTE be32[4];
	be32[0] = (num >> 0) & 0xFF;
	be32[1] = (num >> 8) & 0xFF;
	be32[2] = (num >> 16) & 0xFF;
	be32[3] = (num >> 24) & 0xFF;
	return winpr_Digest_Update(md5, be32, 4);
}

static void ntlm_compute_channel_bindings(NTLM_CONTEXT* context)
{
	WINPR_DIGEST_CTX* md5;
	BYTE* ChannelBindingToken;
	UINT32 ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;

	WINPR_ASSERT(context);

	ZeroMemory(context->ChannelBindingsHash, WINPR_MD5_DIGEST_LENGTH);
	ChannelBindings = context->Bindings.Bindings;

	if (!ChannelBindings)
		return;

	if (!(md5 = winpr_Digest_New()))
		return;

	if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
		goto out;

	ChannelBindingTokenLength = context->Bindings.BindingsLength - sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*)ChannelBindings)[ChannelBindings->dwApplicationDataOffset];

	if (!ntlm_md5_update_uint32_be(md5, ChannelBindings->dwInitiatorAddrType))
		goto out;

	if (!ntlm_md5_update_uint32_be(md5, ChannelBindings->cbInitiatorLength))
		goto out;

	if (!ntlm_md5_update_uint32_be(md5, ChannelBindings->dwAcceptorAddrType))
		goto out;

	if (!ntlm_md5_update_uint32_be(md5, ChannelBindings->cbAcceptorLength))
		goto out;

	if (!ntlm_md5_update_uint32_be(md5, ChannelBindings->cbApplicationDataLength))
		goto out;

	if (!winpr_Digest_Update(md5, (void*)ChannelBindingToken, ChannelBindingTokenLength))
		goto out;

	if (!winpr_Digest_Final(md5, context->ChannelBindingsHash, WINPR_MD5_DIGEST_LENGTH))
		goto out;

out:
	winpr_Digest_Free(md5);
}

static void ntlm_compute_single_host_data(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);
	/**
	 * The Single_Host_Data structure allows a client to send machine-specific information
	 * within an authentication exchange to services on the same machine. The client can
	 * produce additional information to be processed in an implementation-specific way when
	 * the client and server are on the same host. If the server and client platforms are
	 * different or if they are on different hosts, then the information MUST be ignored.
	 * Any fields after the MachineID field MUST be ignored on receipt.
	 */
	Data_Write_UINT32(&context->SingleHostData.Size, 48);
	Data_Write_UINT32(&context->SingleHostData.Z4, 0);
	Data_Write_UINT32(&context->SingleHostData.DataPresent, 1);
	Data_Write_UINT32(&context->SingleHostData.CustomData, SECURITY_MANDATORY_MEDIUM_RID);
	FillMemory(context->SingleHostData.MachineID, 32, 0xAA);
}

BOOL ntlm_construct_challenge_target_info(NTLM_CONTEXT* context)
{
	BOOL rc = FALSE;
	ULONG length;
	ULONG AvPairsCount;
	ULONG AvPairsLength;
	NTLM_AV_PAIR* pAvPairList;
	size_t cbAvPairList;
	UNICODE_STRING NbDomainName = { 0 };
	UNICODE_STRING NbComputerName = { 0 };
	UNICODE_STRING DnsDomainName = { 0 };
	UNICODE_STRING DnsComputerName = { 0 };

	WINPR_ASSERT(context);

	if (ntlm_get_target_computer_name(&NbDomainName, ComputerNameNetBIOS) < 0)
		goto fail;

	NbComputerName.Buffer = NULL;

	if (ntlm_get_target_computer_name(&NbComputerName, ComputerNameNetBIOS) < 0)
		goto fail;

	DnsDomainName.Buffer = NULL;

	if (ntlm_get_target_computer_name(&DnsDomainName, ComputerNameDnsDomain) < 0)
		goto fail;

	DnsComputerName.Buffer = NULL;

	if (ntlm_get_target_computer_name(&DnsComputerName, ComputerNameDnsHostname) < 0)
		goto fail;

	AvPairsCount = 5;
	AvPairsLength = NbDomainName.Length + NbComputerName.Length + DnsDomainName.Length +
	                DnsComputerName.Length + 8;
	length = ntlm_av_pair_list_size(AvPairsCount, AvPairsLength);

	if (!sspi_SecBufferAlloc(&context->ChallengeTargetInfo, length))
		goto fail;

	pAvPairList = (NTLM_AV_PAIR*)context->ChallengeTargetInfo.pvBuffer;
	cbAvPairList = context->ChallengeTargetInfo.cbBuffer;

	if (!ntlm_av_pair_list_init(pAvPairList, cbAvPairList))
		goto fail;

	if (!ntlm_av_pair_add(pAvPairList, cbAvPairList, MsvAvNbDomainName, (PBYTE)NbDomainName.Buffer,
	                      NbDomainName.Length))
		goto fail;

	if (!ntlm_av_pair_add(pAvPairList, cbAvPairList, MsvAvNbComputerName,
	                      (PBYTE)NbComputerName.Buffer, NbComputerName.Length))
		goto fail;

	if (!ntlm_av_pair_add(pAvPairList, cbAvPairList, MsvAvDnsDomainName,
	                      (PBYTE)DnsDomainName.Buffer, DnsDomainName.Length))
		goto fail;

	if (!ntlm_av_pair_add(pAvPairList, cbAvPairList, MsvAvDnsComputerName,
	                      (PBYTE)DnsComputerName.Buffer, DnsComputerName.Length))
		goto fail;

	if (!ntlm_av_pair_add(pAvPairList, cbAvPairList, MsvAvTimestamp, context->Timestamp,
	                      sizeof(context->Timestamp)))
		goto fail;

	rc = TRUE;
fail:
	ntlm_free_unicode_string(&NbDomainName);
	ntlm_free_unicode_string(&NbComputerName);
	ntlm_free_unicode_string(&DnsDomainName);
	ntlm_free_unicode_string(&DnsComputerName);
	return rc;
}

BOOL ntlm_construct_authenticate_target_info(NTLM_CONTEXT* context)
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
	size_t cbAvTimestamp;
	size_t cbAvNbDomainName;
	size_t cbAvNbComputerName;
	size_t cbAvDnsDomainName;
	size_t cbAvDnsComputerName;
	size_t cbAvDnsTreeName;
	size_t cbChallengeTargetInfo;
	size_t cbAuthenticateTargetInfo;

	WINPR_ASSERT(context);

	AvPairsCount = 1;
	AvPairsValueLength = 0;
	ChallengeTargetInfo = (NTLM_AV_PAIR*)context->ChallengeTargetInfo.pvBuffer;
	cbChallengeTargetInfo = context->ChallengeTargetInfo.cbBuffer;
	AvNbDomainName = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo, MsvAvNbDomainName,
	                                  &cbAvNbDomainName);
	AvNbComputerName = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo,
	                                    MsvAvNbComputerName, &cbAvNbComputerName);
	AvDnsDomainName = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo,
	                                   MsvAvDnsDomainName, &cbAvDnsDomainName);
	AvDnsComputerName = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo,
	                                     MsvAvDnsComputerName, &cbAvDnsComputerName);
	AvDnsTreeName = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo, MsvAvDnsTreeName,
	                                 &cbAvDnsTreeName);
	AvTimestamp = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo, MsvAvTimestamp,
	                               &cbAvTimestamp);

	if (AvNbDomainName)
	{
		size_t avLen;
		if (!ntlm_av_pair_get_len(AvNbDomainName, cbAvNbDomainName, &avLen))
			goto fail;
		AvPairsCount++; /* MsvAvNbDomainName */
		AvPairsValueLength += avLen;
	}

	if (AvNbComputerName)
	{
		size_t avLen;
		if (!ntlm_av_pair_get_len(AvNbComputerName, cbAvNbComputerName, &avLen))
			goto fail;
		AvPairsCount++; /* MsvAvNbComputerName */
		AvPairsValueLength += avLen;
	}

	if (AvDnsDomainName)
	{
		size_t avLen;
		if (!ntlm_av_pair_get_len(AvDnsDomainName, cbAvDnsDomainName, &avLen))
			goto fail;
		AvPairsCount++; /* MsvAvDnsDomainName */
		AvPairsValueLength += avLen;
	}

	if (AvDnsComputerName)
	{
		size_t avLen;
		if (!ntlm_av_pair_get_len(AvDnsComputerName, cbAvDnsComputerName, &avLen))
			goto fail;
		AvPairsCount++; /* MsvAvDnsComputerName */
		AvPairsValueLength += avLen;
	}

	if (AvDnsTreeName)
	{
		size_t avLen;
		if (!ntlm_av_pair_get_len(AvDnsTreeName, cbAvDnsTreeName, &avLen))
			goto fail;
		AvPairsCount++; /* MsvAvDnsTreeName */
		AvPairsValueLength += avLen;
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
		AvPairsCount++; /* MsvAvChannelBindings */
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

	if (!sspi_SecBufferAlloc(&context->AuthenticateTargetInfo, size))
		goto fail;

	AuthenticateTargetInfo = (NTLM_AV_PAIR*)context->AuthenticateTargetInfo.pvBuffer;
	cbAuthenticateTargetInfo = context->AuthenticateTargetInfo.cbBuffer;

	if (!ntlm_av_pair_list_init(AuthenticateTargetInfo, cbAuthenticateTargetInfo))
		goto fail;

	if (AvNbDomainName)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo, AvNbDomainName,
		                           cbAvNbDomainName))
			goto fail;
	}

	if (AvNbComputerName)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo,
		                           AvNbComputerName, cbAvNbComputerName))
			goto fail;
	}

	if (AvDnsDomainName)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo,
		                           AvDnsDomainName, cbAvDnsDomainName))
			goto fail;
	}

	if (AvDnsComputerName)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo,
		                           AvDnsComputerName, cbAvDnsComputerName))
			goto fail;
	}

	if (AvDnsTreeName)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo, AvDnsTreeName,
		                           cbAvDnsTreeName))
			goto fail;
	}

	if (AvTimestamp)
	{
		if (!ntlm_av_pair_add_copy(AuthenticateTargetInfo, cbAuthenticateTargetInfo, AvTimestamp,
		                           cbAvTimestamp))
			goto fail;
	}

	if (context->UseMIC)
	{
		UINT32 flags;
		Data_Write_UINT32(&flags, MSV_AV_FLAGS_MESSAGE_INTEGRITY_CHECK);

		if (!ntlm_av_pair_add(AuthenticateTargetInfo, cbAuthenticateTargetInfo, MsvAvFlags,
		                      (PBYTE)&flags, 4))
			goto fail;
	}

	if (context->SendSingleHostData)
	{
		WINPR_ASSERT(context->SingleHostData.Size <= UINT16_MAX);
		if (!ntlm_av_pair_add(AuthenticateTargetInfo, cbAuthenticateTargetInfo, MsvAvSingleHost,
		                      (PBYTE)&context->SingleHostData,
		                      (UINT16)context->SingleHostData.Size))
			goto fail;
	}

	if (!context->SuppressExtendedProtection)
	{
		if (!ntlm_av_pair_add(AuthenticateTargetInfo, cbAuthenticateTargetInfo,
		                      MsvAvChannelBindings, context->ChannelBindingsHash, 16))
			goto fail;

		if (context->ServicePrincipalName.Length > 0)
		{
			if (!ntlm_av_pair_add(AuthenticateTargetInfo, cbAuthenticateTargetInfo, MsvAvTargetName,
			                      (PBYTE)context->ServicePrincipalName.Buffer,
			                      context->ServicePrincipalName.Length))
				goto fail;
		}
	}

	if (context->NTLMv2)
	{
		NTLM_AV_PAIR* AvEOL;
		AvEOL = ntlm_av_pair_get(ChallengeTargetInfo, cbChallengeTargetInfo, MsvAvEOL, NULL);

		if (!AvEOL)
			goto fail;

		ZeroMemory(AvEOL, sizeof(NTLM_AV_PAIR));
	}

	return TRUE;
fail:
	sspi_SecBufferFree(&context->AuthenticateTargetInfo);
	return FALSE;
}
