/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include "wlog.h"

#include "PacketMessage.h"

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include "../../log.h"
#define TAG WINPR_TAG("utils.wlog")

static BOOL Pcap_Read_Header(wPcap* pcap, wPcapHeader* header)
{
	if (pcap && pcap->fp && fread((void*)header, sizeof(wPcapHeader), 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Write_Header(wPcap* pcap, wPcapHeader* header)
{
	if (pcap && pcap->fp && fwrite((void*)header, sizeof(wPcapHeader), 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Write_RecordHeader(wPcap* pcap, wPcapRecordHeader* record)
{
	if (pcap && pcap->fp && fwrite((void*)record, sizeof(wPcapRecordHeader), 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Write_RecordContent(wPcap* pcap, wPcapRecord* record)
{
	if (pcap && pcap->fp && fwrite(record->data, record->length, 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Write_Record(wPcap* pcap, wPcapRecord* record)
{
	return Pcap_Write_RecordHeader(pcap, &record->header) && Pcap_Write_RecordContent(pcap, record);
}

wPcap* Pcap_Open(char* name, BOOL write)
{
	wPcap* pcap = NULL;
	FILE* pcap_fp = winpr_fopen(name, write ? "w+b" : "rb");

	if (!pcap_fp)
	{
		WLog_ERR(TAG, "opening pcap file");
		return NULL;
	}

	pcap = (wPcap*)calloc(1, sizeof(wPcap));

	if (!pcap)
		goto out_fail;

	pcap->name = name;
	pcap->write = write;
	pcap->record_count = 0;
	pcap->fp = pcap_fp;

	if (write)
	{
		pcap->header.magic_number = PCAP_MAGIC_NUMBER;
		pcap->header.version_major = 2;
		pcap->header.version_minor = 4;
		pcap->header.thiszone = 0;
		pcap->header.sigfigs = 0;
		pcap->header.snaplen = 0xFFFFFFFF;
		pcap->header.network = 1; /* ethernet */
		if (!Pcap_Write_Header(pcap, &pcap->header))
			goto out_fail;
	}
	else
	{
		if (_fseeki64(pcap->fp, 0, SEEK_END) < 0)
			goto out_fail;
		pcap->file_size = (SSIZE_T)_ftelli64(pcap->fp);
		if (pcap->file_size < 0)
			goto out_fail;
		if (_fseeki64(pcap->fp, 0, SEEK_SET) < 0)
			goto out_fail;
		if (!Pcap_Read_Header(pcap, &pcap->header))
			goto out_fail;
	}

	return pcap;

out_fail:
	if (pcap_fp)
		(void)fclose(pcap_fp);
	free(pcap);
	return NULL;
}

void Pcap_Flush(wPcap* pcap)
{
	if (!pcap || !pcap->fp)
		return;

	while (pcap->record)
	{
		if (!Pcap_Write_Record(pcap, pcap->record))
			return;
		pcap->record = pcap->record->next;
	}

	(void)fflush(pcap->fp);
}

void Pcap_Close(wPcap* pcap)
{
	if (!pcap || !pcap->fp)
		return;

	Pcap_Flush(pcap);
	(void)fclose(pcap->fp);
	free(pcap);
}

static BOOL WLog_PacketMessage_Write_EthernetHeader(wPcap* pcap, wEthernetHeader* ethernet)
{
	wStream* s = NULL;
	wStream sbuffer = { 0 };
	BYTE buffer[14] = { 0 };
	BOOL ret = TRUE;

	if (!pcap || !pcap->fp || !ethernet)
		return FALSE;

	s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	if (!s)
		return FALSE;
	Stream_Write(s, ethernet->Destination, 6);
	Stream_Write(s, ethernet->Source, 6);
	Stream_Write_UINT16_BE(s, ethernet->Type);
	if (fwrite(buffer, sizeof(buffer), 1, pcap->fp) != 1)
		ret = FALSE;

	return ret;
}

static UINT16 IPv4Checksum(const BYTE* ipv4, int length)
{
	long checksum = 0;

	while (length > 1)
	{
		const UINT16 tmp16 = *((const UINT16*)ipv4);
		checksum += tmp16;
		length -= 2;
		ipv4 += 2;
	}

	if (length > 0)
		checksum += *ipv4;

	while (checksum >> 16)
		checksum = (checksum & 0xFFFF) + (checksum >> 16);

	return (UINT16)(~checksum);
}

static BOOL WLog_PacketMessage_Write_IPv4Header(wPcap* pcap, wIPv4Header* ipv4)
{
	wStream* s = NULL;
	wStream sbuffer = { 0 };
	BYTE buffer[20] = { 0 };
	int ret = TRUE;

	if (!pcap || !pcap->fp || !ipv4)
		return FALSE;

	s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	if (!s)
		return FALSE;
	Stream_Write_UINT8(s, (BYTE)((ipv4->Version << 4) | ipv4->InternetHeaderLength));
	Stream_Write_UINT8(s, ipv4->TypeOfService);
	Stream_Write_UINT16_BE(s, ipv4->TotalLength);
	Stream_Write_UINT16_BE(s, ipv4->Identification);
	Stream_Write_UINT16_BE(s, (UINT16)((ipv4->InternetProtocolFlags << 13) | ipv4->FragmentOffset));
	Stream_Write_UINT8(s, ipv4->TimeToLive);
	Stream_Write_UINT8(s, ipv4->Protocol);
	Stream_Write_UINT16(s, ipv4->HeaderChecksum);
	Stream_Write_UINT32_BE(s, ipv4->SourceAddress);
	Stream_Write_UINT32_BE(s, ipv4->DestinationAddress);
	ipv4->HeaderChecksum = IPv4Checksum((BYTE*)buffer, 20);
	Stream_Rewind(s, 10);
	Stream_Write_UINT16(s, ipv4->HeaderChecksum);

	if (fwrite(buffer, sizeof(buffer), 1, pcap->fp) != 1)
		ret = FALSE;

	return ret;
}

static BOOL WLog_PacketMessage_Write_TcpHeader(wPcap* pcap, wTcpHeader* tcp)
{
	wStream* s = NULL;
	wStream sbuffer = { 0 };
	BYTE buffer[20] = { 0 };
	BOOL ret = TRUE;

	if (!pcap || !pcap->fp || !tcp)
		return FALSE;

	s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	if (!s)
		return FALSE;
	Stream_Write_UINT16_BE(s, tcp->SourcePort);
	Stream_Write_UINT16_BE(s, tcp->DestinationPort);
	Stream_Write_UINT32_BE(s, tcp->SequenceNumber);
	Stream_Write_UINT32_BE(s, tcp->AcknowledgementNumber);
	Stream_Write_UINT8(s, (UINT8)((tcp->Offset << 4) | (tcp->Reserved & 0xF)));
	Stream_Write_UINT8(s, tcp->TcpFlags);
	Stream_Write_UINT16_BE(s, tcp->Window);
	Stream_Write_UINT16_BE(s, tcp->Checksum);
	Stream_Write_UINT16_BE(s, tcp->UrgentPointer);

	if (pcap->fp)
	{
		if (fwrite(buffer, sizeof(buffer), 1, pcap->fp) != 1)
			ret = FALSE;
	}

	return ret;
}

static UINT32 g_InboundSequenceNumber = 0;
static UINT32 g_OutboundSequenceNumber = 0;

BOOL WLog_PacketMessage_Write(wPcap* pcap, void* data, size_t length, DWORD flags)
{
	wTcpHeader tcp;
	wIPv4Header ipv4;
	wPcapRecord record;
	wEthernetHeader ethernet;
	ethernet.Type = 0x0800;

	if (!pcap || !pcap->fp)
		return FALSE;

	if (flags & WLOG_PACKET_OUTBOUND)
	{
		/* 00:15:5D:01:64:04 */
		ethernet.Source[0] = 0x00;
		ethernet.Source[1] = 0x15;
		ethernet.Source[2] = 0x5D;
		ethernet.Source[3] = 0x01;
		ethernet.Source[4] = 0x64;
		ethernet.Source[5] = 0x04;
		/* 00:15:5D:01:64:01 */
		ethernet.Destination[0] = 0x00;
		ethernet.Destination[1] = 0x15;
		ethernet.Destination[2] = 0x5D;
		ethernet.Destination[3] = 0x01;
		ethernet.Destination[4] = 0x64;
		ethernet.Destination[5] = 0x01;
	}
	else
	{
		/* 00:15:5D:01:64:01 */
		ethernet.Source[0] = 0x00;
		ethernet.Source[1] = 0x15;
		ethernet.Source[2] = 0x5D;
		ethernet.Source[3] = 0x01;
		ethernet.Source[4] = 0x64;
		ethernet.Source[5] = 0x01;
		/* 00:15:5D:01:64:04 */
		ethernet.Destination[0] = 0x00;
		ethernet.Destination[1] = 0x15;
		ethernet.Destination[2] = 0x5D;
		ethernet.Destination[3] = 0x01;
		ethernet.Destination[4] = 0x64;
		ethernet.Destination[5] = 0x04;
	}

	ipv4.Version = 4;
	ipv4.InternetHeaderLength = 5;
	ipv4.TypeOfService = 0;
	ipv4.TotalLength = (UINT16)(length + 20 + 20);
	ipv4.Identification = 0;
	ipv4.InternetProtocolFlags = 0x02;
	ipv4.FragmentOffset = 0;
	ipv4.TimeToLive = 128;
	ipv4.Protocol = 6; /* TCP */
	ipv4.HeaderChecksum = 0;

	if (flags & WLOG_PACKET_OUTBOUND)
	{
		ipv4.SourceAddress = 0xC0A80196;      /* 192.168.1.150 */
		ipv4.DestinationAddress = 0x4A7D64C8; /* 74.125.100.200 */
	}
	else
	{
		ipv4.SourceAddress = 0x4A7D64C8;      /* 74.125.100.200 */
		ipv4.DestinationAddress = 0xC0A80196; /* 192.168.1.150 */
	}

	tcp.SourcePort = 3389;
	tcp.DestinationPort = 3389;

	if (flags & WLOG_PACKET_OUTBOUND)
	{
		tcp.SequenceNumber = g_OutboundSequenceNumber;
		tcp.AcknowledgementNumber = g_InboundSequenceNumber;
		WINPR_ASSERT(length + g_OutboundSequenceNumber <= UINT32_MAX);
		g_OutboundSequenceNumber += WINPR_ASSERTING_INT_CAST(uint32_t, length);
	}
	else
	{
		tcp.SequenceNumber = g_InboundSequenceNumber;
		tcp.AcknowledgementNumber = g_OutboundSequenceNumber;

		WINPR_ASSERT(length + g_InboundSequenceNumber <= UINT32_MAX);
		g_InboundSequenceNumber += WINPR_ASSERTING_INT_CAST(uint32_t, length);
	}

	tcp.Offset = 5;
	tcp.Reserved = 0;
	tcp.TcpFlags = 0x0018;
	tcp.Window = 0x7FFF;
	tcp.Checksum = 0;
	tcp.UrgentPointer = 0;
	record.data = data;
	record.length = length;
	const size_t offset = 14 + 20 + 20;
	WINPR_ASSERT(record.length <= UINT32_MAX - offset);
	const uint32_t rloff = WINPR_ASSERTING_INT_CAST(uint32_t, record.length + offset);
	record.header.incl_len = rloff;
	record.header.orig_len = rloff;
	record.next = NULL;

	UINT64 ns = winpr_GetUnixTimeNS();
	record.header.ts_sec = (UINT32)WINPR_TIME_NS_TO_S(ns);
	record.header.ts_usec = WINPR_TIME_NS_REM_US(ns);

	if (!Pcap_Write_RecordHeader(pcap, &record.header) ||
	    !WLog_PacketMessage_Write_EthernetHeader(pcap, &ethernet) ||
	    !WLog_PacketMessage_Write_IPv4Header(pcap, &ipv4) ||
	    !WLog_PacketMessage_Write_TcpHeader(pcap, &tcp) || !Pcap_Write_RecordContent(pcap, &record))
		return FALSE;
	(void)fflush(pcap->fp);
	return TRUE;
}
