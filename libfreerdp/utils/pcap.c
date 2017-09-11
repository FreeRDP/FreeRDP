/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("utils")

#ifndef _WIN32
#include <sys/time.h>
#else
#include <time.h>
#include <sys/timeb.h>
#include <winpr/windows.h>

int gettimeofday(struct timeval* tp, void* tz)
{
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	tp->tv_sec = (long) timebuffer.time;
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#endif

#include <freerdp/types.h>
#include <freerdp/utils/pcap.h>

#define PCAP_MAGIC	0xA1B2C3D4

static BOOL pcap_read_header(rdpPcap* pcap, pcap_header* header)
{
	return fread((void*) header, sizeof(pcap_header), 1, pcap->fp) == 1;
}

static BOOL pcap_write_header(rdpPcap* pcap, pcap_header* header)
{
	return fwrite((void*) header, sizeof(pcap_header), 1, pcap->fp) == 1;
}

static BOOL pcap_read_record_header(rdpPcap* pcap, pcap_record_header* record)
{
	return fread((void*) record, sizeof(pcap_record_header), 1, pcap->fp) == 1;
}

static BOOL pcap_write_record_header(rdpPcap* pcap, pcap_record_header* record)
{
	return fwrite((void*) record, sizeof(pcap_record_header), 1, pcap->fp) == 1;
}

static BOOL pcap_read_record(rdpPcap* pcap, pcap_record* record)
{
	if (!pcap_read_record_header(pcap, &record->header))
		return FALSE;

	record->length = record->header.incl_len;
	record->data = malloc(record->length);
	if (!record->data)
		return FALSE;

	if (fread(record->data, record->length, 1, pcap->fp) != 1)
	{
		free(record->data);
		record->data = NULL;
		return FALSE;
	}
	return TRUE;
}

static BOOL pcap_write_record(rdpPcap* pcap, pcap_record* record)
{
	return pcap_write_record_header(pcap, &record->header) &&
			(fwrite(record->data, record->length, 1, pcap->fp) == 1);
}

BOOL pcap_add_record(rdpPcap* pcap, void* data, UINT32 length)
{
	pcap_record* record;
	struct timeval tp;

	if (pcap->tail == NULL)
	{
		pcap->tail = (pcap_record*) calloc(1, sizeof(pcap_record));
		if (!pcap->tail)
			return FALSE;

		pcap->head = pcap->tail;
		pcap->record = pcap->head;
		record = pcap->tail;
	}
	else
	{
		record = (pcap_record*) calloc(1, sizeof(pcap_record));
		if (!record)
			return FALSE;

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
	return TRUE;
}

BOOL pcap_has_next_record(rdpPcap* pcap)
{
	if (pcap->file_size - (_ftelli64(pcap->fp)) <= 16)
		return FALSE;

	return TRUE;
}

BOOL pcap_get_next_record_header(rdpPcap* pcap, pcap_record* record)
{
	if (pcap_has_next_record(pcap) != TRUE)
		return FALSE;

	pcap_read_record_header(pcap, &record->header);
	record->length = record->header.incl_len;

	return TRUE;
}

BOOL pcap_get_next_record_content(rdpPcap* pcap, pcap_record* record)
{
	return fread(record->data, record->length, 1, pcap->fp) == 1;
}

BOOL pcap_get_next_record(rdpPcap* pcap, pcap_record* record)
{
	return pcap_has_next_record(pcap) &&
			pcap_read_record(pcap, record);
}

rdpPcap* pcap_open(char* name, BOOL write)
{
	rdpPcap* pcap;

	FILE* pcap_fp = fopen(name, write ? "w+b" : "rb");

	if (pcap_fp == NULL)
	{
		WLog_ERR(TAG, "opening pcap dump");
		return NULL;
	}

	pcap = (rdpPcap*) calloc(1, sizeof(rdpPcap));
	if (!pcap)
		goto fail_close;

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
		if (!pcap_write_header(pcap, &pcap->header))
			goto fail;
	}
	else
	{
		_fseeki64(pcap->fp, 0, SEEK_END);
		pcap->file_size = _ftelli64(pcap->fp);
		_fseeki64(pcap->fp, 0, SEEK_SET);
		if (!pcap_read_header(pcap, &pcap->header))
			goto fail;
	}

	return pcap;

fail:
	free(pcap);
fail_close:
	fclose(pcap_fp);
	return NULL;
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

	free(pcap);
}
