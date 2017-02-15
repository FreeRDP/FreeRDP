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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wlog.h"

#include "wlog/PacketMessage.h"

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "../../log.h"
#define TAG WINPR_TAG("utils.wlog")

#ifndef _WIN32
#include <sys/time.h>
#else
#include <time.h>
#include <sys/timeb.h>
#include <winpr/windows.h>

static int gettimeofday(struct timeval* tp, void* tz)
{
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	tp->tv_sec = (long) timebuffer.time;
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#endif

static BOOL Pcap_Read_Header(wPcap* pcap, wPcapHeader* header)
{
	if (pcap && pcap->fp && fread((void*) header, sizeof(wPcapHeader), 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

/* currently unused code */
# if 0
static BOOL Pcap_Read_RecordHeader(wPcap* pcap, wPcapRecordHeader* record)
{
	if (pcap && pcap->fp && (fread((void*) record, sizeof(wPcapRecordHeader), 1, pcap->fp) == 1))
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Read_Record(wPcap* pcap, wPcapRecord* record)
{
	if (pcap && pcap->fp)
	{
		if (!Pcap_Read_RecordHeader(pcap, &record->header))
			return FALSE;
		record->length = record->header.incl_len;
		record->data = malloc(record->length);
		if (!record->data)
			return FALSE;
		if (fread(record->data, record->length, 1, pcap->fp) != 1)
		{
			free(record->data);
			record->length = 0;
			record->data = NULL;
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL Pcap_Add_Record(wPcap* pcap, void* data, UINT32 length)
{
	wPcapRecord* record;
	struct timeval tp;

	if (!pcap->tail)
	{
		pcap->tail = (wPcapRecord*) calloc(1, sizeof(wPcapRecord));
		if (!pcap->tail)
			return FALSE;
		pcap->head = pcap->tail;
		pcap->record = pcap->head;
		record = pcap->tail;
	}
	else
	{
		record = (wPcapRecord*) calloc(1, sizeof(wPcapRecord));
		if (!record)
			return FALSE;
		pcap->tail->next = record;
		pcap->tail = record;
	}

	if (!pcap->record)
		pcap->record = record;

	record->data = data;
	record->length = length;
	record->header.incl_len = length;
	record->header.orig_len = length;
	gettimeofday(&tp, 0);
	record->header.ts_sec = tp.tv_sec;
	record->header.ts_usec = tp.tv_usec;
	return TRUE;
}

static BOOL Pcap_HasNext_Record(wPcap* pcap)
{
	if (pcap->file_size - (ftell(pcap->fp)) <= 16)
		return FALSE;

	return TRUE;
}

static BOOL Pcap_GetNext_RecordHeader(wPcap* pcap, wPcapRecord* record)
{
	if (!Pcap_HasNext_Record(pcap) || !Pcap_Read_RecordHeader(pcap, &record->header))
		return FALSE;

	record->length = record->header.incl_len;
	return TRUE;
}

static BOOL Pcap_GetNext_RecordContent(wPcap* pcap, wPcapRecord* record)
{
	if (pcap && pcap->fp && fread(record->data, record->length, 1, pcap->fp) == 1)
		return TRUE;

	return FALSE;
}

static BOOL Pcap_GetNext_Record(wPcap* pcap, wPcapRecord* record)
{
	if (!Pcap_HasNext_Record(pcap))
		return FALSE;

	return Pcap_Read_Record(pcap, record);
}
#endif


static BOOL Pcap_Write_Header(wPcap* pcap, wPcapHeader* header)
{
	if (pcap && pcap->fp && fwrite((void*) header, sizeof(wPcapHeader), 1, pcap->fp) == 1)
		return TRUE;
	return FALSE;
}

static BOOL Pcap_Write_RecordHeader(wPcap* pcap, wPcapRecordHeader* record)
{
	if (pcap && pcap->fp && fwrite((void *) record, sizeof(wPcapRecordHeader), 1, pcap->fp) == 1)
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
	return Pcap_Write_RecordHeader(pcap, &record->header) &&
		Pcap_Write_RecordContent(pcap, record);
}

wPcap* Pcap_Open(char* name, BOOL write)
{
	wPcap* pcap = NULL;
	FILE* pcap_fp = fopen(name, write ? "w+b" : "rb");

	if (!pcap_fp)
	{
		WLog_ERR(TAG,"opening pcap file");
		return NULL;
	}

	pcap = (wPcap*) calloc(1, sizeof(wPcap));

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
		if (fseek(pcap->fp, 0, SEEK_END) < 0)
			goto out_fail;
		pcap->file_size = (int) ftell(pcap->fp);
		if (pcap->file_size < 0)
			goto out_fail;
		if (fseek(pcap->fp, 0, SEEK_SET) < 0)
			goto out_fail;
		if (!Pcap_Read_Header(pcap, &pcap->header))
			goto out_fail;
	}

	return pcap;

out_fail:
	fclose(pcap_fp);
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

	fflush(pcap->fp);
	return;
}

void Pcap_Close(wPcap* pcap)
{
	if (!pcap || !pcap->fp)
		return;

	Pcap_Flush(pcap);
	fclose(pcap->fp);
	free(pcap);
}

static BOOL WLog_PacketMessage_Write_EthernetHeader(wPcap* pcap, wEthernetHeader* ethernet)
{
	wStream* s;
	BYTE buffer[14];
	BOOL ret = TRUE;

	if (!pcap || !pcap->fp || !ethernet)
		return FALSE;

	s = Stream_New(buffer, 14);
	if (!s)
		return FALSE;
	Stream_Write(s, ethernet->Destination, 6);
	Stream_Write(s, ethernet->Source, 6);
	Stream_Write_UINT16_BE(s, ethernet->Type);
	if (fwrite(buffer, 14, 1, pcap->fp) != 1)
		ret = FALSE;
	Stream_Free(s, FALSE);
	return ret;
}

static UINT16 IPv4Checksum(BYTE* ipv4, int length)
{
	UINT16 tmp16;
	long checksum = 0;

	while (length > 1)
	{
		tmp16 = *((UINT16*) ipv4);
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
	wStream* s;
	BYTE buffer[20];
	int ret = TRUE;

	if (!pcap || !pcap->fp || !ipv4)
		return FALSE;

	s = Stream_New(buffer, 20);
	if (!s)
		return FALSE;
	Stream_Write_UINT8(s, (ipv4->Version << 4) | ipv4->InternetHeaderLength);
	Stream_Write_UINT8(s, ipv4->TypeOfService);
	Stream_Write_UINT16_BE(s, ipv4->TotalLength);
	Stream_Write_UINT16_BE(s, ipv4->Identification);
	Stream_Write_UINT16_BE(s, (ipv4->InternetProtocolFlags << 13) | ipv4->FragmentOffset);
	Stream_Write_UINT8(s, ipv4->TimeToLive);
	Stream_Write_UINT8(s, ipv4->Protocol);
	Stream_Write_UINT16(s, ipv4->HeaderChecksum);
	Stream_Write_UINT32_BE(s, ipv4->SourceAddress);
	Stream_Write_UINT32_BE(s, ipv4->DestinationAddress);
	ipv4->HeaderChecksum = IPv4Checksum((BYTE*) buffer, 20);
	Stream_Rewind(s, 10);
	Stream_Write_UINT16(s, ipv4->HeaderChecksum);
	Stream_Seek(s, 8);
	if (fwrite(buffer, 20, 1, pcap->fp) != 1)
		ret = FALSE;
	Stream_Free(s, FALSE);
	return ret;
}

static BOOL WLog_PacketMessage_Write_TcpHeader(wPcap* pcap, wTcpHeader* tcp)
{
	wStream* s;
	BYTE buffer[20];
	BOOL ret = TRUE;

	if (!pcap || !pcap->fp || !tcp)
		return FALSE;

	s = Stream_New(buffer, 20);
	if (!s)
		return FALSE;
	Stream_Write_UINT16_BE(s, tcp->SourcePort);
	Stream_Write_UINT16_BE(s, tcp->DestinationPort);
	Stream_Write_UINT32_BE(s, tcp->SequenceNumber);
	Stream_Write_UINT32_BE(s, tcp->AcknowledgementNumber);
	Stream_Write_UINT8(s, (tcp->Offset << 4) | tcp->Reserved);
	Stream_Write_UINT8(s, tcp->TcpFlags);
	Stream_Write_UINT16_BE(s, tcp->Window);
	Stream_Write_UINT16_BE(s, tcp->Checksum);
	Stream_Write_UINT16_BE(s, tcp->UrgentPointer);

	if (pcap->fp)
	{
		if (fwrite(buffer, 20, 1, pcap->fp) != 1)
			ret = FALSE;
	}

	Stream_Free(s, FALSE);
	return ret;
}

static UINT32 g_InboundSequenceNumber = 0;
static UINT32 g_OutboundSequenceNumber = 0;

BOOL WLog_PacketMessage_Write(wPcap* pcap, void* data, DWORD length, DWORD flags)
{
	wTcpHeader tcp;
	wIPv4Header ipv4;
	struct timeval tp;
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
		ipv4.SourceAddress = 0xC0A80196; /* 192.168.1.150 */
		ipv4.DestinationAddress = 0x4A7D64C8; /* 74.125.100.200 */
	}
	else
	{
		ipv4.SourceAddress = 0x4A7D64C8; /* 74.125.100.200 */
		ipv4.DestinationAddress = 0xC0A80196; /* 192.168.1.150 */
	}

	tcp.SourcePort = 3389;
	tcp.DestinationPort = 3389;

	if (flags & WLOG_PACKET_OUTBOUND)
	{
		tcp.SequenceNumber = g_OutboundSequenceNumber;
		tcp.AcknowledgementNumber = g_InboundSequenceNumber;
		g_OutboundSequenceNumber += length;
	}
	else
	{
		tcp.SequenceNumber = g_InboundSequenceNumber;
		tcp.AcknowledgementNumber = g_OutboundSequenceNumber;
		g_InboundSequenceNumber += length;
	}

	tcp.Offset = 5;
	tcp.Reserved = 0;
	tcp.TcpFlags = 0x0018;
	tcp.Window = 0x7FFF;
	tcp.Checksum = 0;
	tcp.UrgentPointer = 0;
	record.data = data;
	record.length = length;
	record.header.incl_len = record.length + 14 + 20 + 20;
	record.header.orig_len = record.length + 14 + 20 + 20;
	record.next = NULL;
	gettimeofday(&tp, 0);
	record.header.ts_sec = tp.tv_sec;
	record.header.ts_usec = tp.tv_usec;
	if (!Pcap_Write_RecordHeader(pcap, &record.header) ||
		!WLog_PacketMessage_Write_EthernetHeader(pcap, &ethernet) ||
		!WLog_PacketMessage_Write_IPv4Header(pcap, &ipv4) ||
		!WLog_PacketMessage_Write_TcpHeader(pcap, &tcp) ||
		!Pcap_Write_RecordContent(pcap, &record))
		return FALSE;
	fflush(pcap->fp);
	return TRUE;
}
