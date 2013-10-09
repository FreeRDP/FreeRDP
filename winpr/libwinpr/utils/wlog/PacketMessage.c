/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/wlog.h>

#include "wlog/PacketMessage.h"

#include <winpr/crt.h>

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

void Pcap_Read_Header(wPcap* pcap, wPcapHeader* header)
{
	fread((void*) header, sizeof(wPcapHeader), 1, pcap->fp);
}

void Pcap_Write_Header(wPcap* pcap, wPcapHeader* header)
{
	fwrite((void*) header, sizeof(wPcapHeader), 1, pcap->fp);
}

void Pcap_Read_RecordHeader(wPcap* pcap, wPcapRecordHeader* record)
{
	fread((void*) record, sizeof(wPcapRecordHeader), 1, pcap->fp);
}

void Pcap_Write_RecordHeader(wPcap* pcap, wPcapRecordHeader* record)
{
	fwrite((void*) record, sizeof(wPcapRecordHeader), 1, pcap->fp);
}

void Pcap_Read_Record(wPcap* pcap, wPcapRecord* record)
{
	Pcap_Read_RecordHeader(pcap, &record->header);
	record->length = record->header.incl_len;
	record->data = malloc(record->length);
	fread(record->data, record->length, 1, pcap->fp);
}

void Pcap_Write_Record(wPcap* pcap, wPcapRecord* record)
{
	Pcap_Write_RecordHeader(pcap, &record->header);
	fwrite(record->data, record->length, 1, pcap->fp);
}

void Pcap_Add_Record(wPcap* pcap, void* data, UINT32 length)
{
	wPcapRecord* record;
	struct timeval tp;

	if (!pcap->tail)
	{
		pcap->tail = (wPcapRecord*) malloc(sizeof(wPcapRecord));
		ZeroMemory(pcap->tail, sizeof(wPcapRecord));

		pcap->head = pcap->tail;
		pcap->record = pcap->head;
		record = pcap->tail;
	}
	else
	{
		record = (wPcapRecord*) malloc(sizeof(wPcapRecord));
		ZeroMemory(record, sizeof(wPcapRecord));

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
}

BOOL Pcap_HasNext_Record(wPcap* pcap)
{
	if (pcap->file_size - (ftell(pcap->fp)) <= 16)
		return FALSE;

	return TRUE;
}

BOOL Pcap_GetNext_RecordHeader(wPcap* pcap, wPcapRecord* record)
{
	if (Pcap_HasNext_Record(pcap) != TRUE)
		return FALSE;

	Pcap_Read_RecordHeader(pcap, &record->header);
	record->length = record->header.incl_len;

	return TRUE;
}

BOOL Pcap_GetNext_RecordContent(wPcap* pcap, wPcapRecord* record)
{
	fread(record->data, record->length, 1, pcap->fp);
	return TRUE;
}

BOOL Pcap_GetNext_Record(wPcap* pcap, wPcapRecord* record)
{
	if (Pcap_HasNext_Record(pcap) != TRUE)
		return FALSE;

	Pcap_Read_Record(pcap, record);

	return TRUE;
}

wPcap* Pcap_Open(char* name, BOOL write)
{
	wPcap* pcap;

	FILE* pcap_fp = fopen(name, write ? "w+b" : "rb");

	if (!pcap_fp)
	{
		perror("opening pcap file");
		return NULL;
	}

	pcap = (wPcap*) malloc(sizeof(wPcap));

	if (pcap)
	{
		ZeroMemory(pcap, sizeof(wPcap));

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
			pcap->header.network = 0;
			Pcap_Write_Header(pcap, &pcap->header);
		}
		else
		{
			fseek(pcap->fp, 0, SEEK_END);
			pcap->file_size = (int) ftell(pcap->fp);
			fseek(pcap->fp, 0, SEEK_SET);
			Pcap_Read_Header(pcap, &pcap->header);
		}
	}

	return pcap;
}

void Pcap_Flush(wPcap* pcap)
{
	while (pcap->record)
	{
		Pcap_Write_Record(pcap, pcap->record);
		pcap->record = pcap->record->next;
	}

	if (pcap->fp)
		fflush(pcap->fp);
}

void Pcap_Close(wPcap* pcap)
{
	Pcap_Flush(pcap);

	if (pcap->fp)
		fclose(pcap->fp);

	free(pcap);
}

int WLog_PacketMessage_Write(wPcap* pcap, void* data, DWORD length, DWORD flags)
{
	struct timeval tp;
	wPcapRecord record;

	record.data = data;
	record.length = length;
	record.next = NULL;
	record.header.incl_len = length;
	record.header.orig_len = length;

	gettimeofday(&tp, 0);
	record.header.ts_sec = tp.tv_sec;
	record.header.ts_usec = tp.tv_usec;

	Pcap_Write_Record(pcap, &record);

	return 0;
}
