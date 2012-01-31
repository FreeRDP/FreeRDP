/**
 * FreeRDP: A Remote Desktop Protocol Client
 * pcap File Format Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#else
#include <time.h>
#include <sys/timeb.h>

#if !defined(__MINGW32__) && !defined(__MINGW64__)
struct timeval
{
	long tv_sec;
	long tv_usec;
};
#endif

int gettimeofday(struct timeval* tp, void* tz)
{
	struct _timeb timebuffer;
	_ftime (&timebuffer);
	tp->tv_sec = (long) timebuffer.time;
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#endif

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>

#include <freerdp/utils/pcap.h>

#define PCAP_MAGIC	0xA1B2C3D4

void pcap_read_header(rdpPcap* pcap, pcap_header* header)
{
	fread((void*) header, sizeof(pcap_header), 1, pcap->fp);
}

void pcap_write_header(rdpPcap* pcap, pcap_header* header)
{
	fwrite((void*) header, sizeof(pcap_header), 1, pcap->fp);
}

void pcap_read_record_header(rdpPcap* pcap, pcap_record_header* record)
{
	fread((void*) record, sizeof(pcap_record_header), 1, pcap->fp);
}

void pcap_write_record_header(rdpPcap* pcap, pcap_record_header* record)
{
	fwrite((void*) record, sizeof(pcap_record_header), 1, pcap->fp);
}

void pcap_read_record(rdpPcap* pcap, pcap_record* record)
{
	pcap_read_record_header(pcap, &record->header);
	record->length = record->header.incl_len;
	record->data = xmalloc(record->length);
	fread(record->data, record->length, 1, pcap->fp);
}

void pcap_write_record(rdpPcap* pcap, pcap_record* record)
{
	pcap_write_record_header(pcap, &record->header);
	fwrite(record->data, record->length, 1, pcap->fp);
}

void pcap_add_record(rdpPcap* pcap, void* data, uint32 length)
{
	pcap_record* record;
	struct timeval tp;

	if (pcap->tail == NULL)
	{
		pcap->tail = (pcap_record*) xzalloc(sizeof(pcap_record));
		pcap->head = pcap->tail;
		pcap->record = pcap->head;
		record = pcap->tail;
	}
	else
	{
		record = (pcap_record*) xzalloc(sizeof(pcap_record));
		pcap->tail->next = record;
		pcap->tail = record;
	}

	if (pcap->record == NULL)
		pcap->record = record;

	record->data = data;
	record->length = length;
	record->header.incl_len = length;
	record->header.orig_len = length;

	gettimeofday(&tp, 0);
	record->header.ts_sec = tp.tv_sec;
	record->header.ts_usec = tp.tv_usec;
}

boolean pcap_has_next_record(rdpPcap* pcap)
{
	if (pcap->file_size - (ftell(pcap->fp)) <= 16)
		return false;

	return true;
}

boolean pcap_get_next_record_header(rdpPcap* pcap, pcap_record* record)
{
	if (pcap_has_next_record(pcap) != true)
		return false;

	pcap_read_record_header(pcap, &record->header);
	record->length = record->header.incl_len;
	record->data = xmalloc(record->length);

	return true;
}

boolean pcap_get_next_record_content(rdpPcap* pcap, pcap_record* record)
{
	fread(record->data, record->length, 1, pcap->fp);
	return true;
}

boolean pcap_get_next_record(rdpPcap* pcap, pcap_record* record)
{
	if (pcap_has_next_record(pcap) != true)
		return false;

	pcap_read_record(pcap, record);

	return true;
}

rdpPcap* pcap_open(char* name, boolean write)
{
	rdpPcap* pcap;

	FILE *pcap_fp = fopen(name, write ? "w+" : "r");
	if (pcap_fp == NULL)
	{
		perror("opening pcap dump");
		return NULL;
	}

	pcap = (rdpPcap*) xzalloc(sizeof(rdpPcap));

	if (pcap != NULL)
	{
		pcap->name = name;
		pcap->write = write;
		pcap->record_count = 0;
		pcap->fp = pcap_fp;

		if (write)
		{
			pcap->header.magic_number = 0xA1B2C3D4;
			pcap->header.version_major = 2;
			pcap->header.version_minor = 4;
			pcap->header.thiszone = 0;
			pcap->header.sigfigs = 0;
			pcap->header.snaplen = 0xFFFFFFFF;
			pcap->header.network = 0;
			pcap_write_header(pcap, &pcap->header);
		}
		else
		{
			fseek(pcap->fp, 0, SEEK_END);
			pcap->file_size = (int) ftell(pcap->fp);
			fseek(pcap->fp, 0, SEEK_SET);
			pcap_read_header(pcap, &pcap->header);
		}
	}

	return pcap;
}

void pcap_flush(rdpPcap* pcap)
{
	while (pcap->record != NULL)
	{
		pcap_write_record(pcap, pcap->record);
		pcap->record = pcap->record->next;
	}

	if (pcap->fp != NULL)
		fflush(pcap->fp);
}

void pcap_close(rdpPcap* pcap)
{
	pcap_flush(pcap);

	if (pcap->fp != NULL)
		fclose(pcap->fp);
}
