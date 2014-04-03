/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright (C) Alexi Volkov <alexi@myrealbox.com> 2006
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
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
#include <assert.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

static UINT32 handle_CommonTypeHeader(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT8 version;
	UINT8 endianess;
	UINT16 header_length;

	if (Stream_GetRemainingLength(irp->input) < 8)
	{
		DEBUG_WARN("length violation %d [%d]", 8,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	/* Process CommonTypeHeader */
	Stream_Read_UINT8(irp->input, version);
	Stream_Read_UINT8(irp->input, endianess);
	Stream_Read_UINT16(irp->input, header_length);
	Stream_Seek(irp->input, 4);

	if (0x01 != version)
	{
		DEBUG_WARN("unsupported header version %d", version);	
		return SCARD_F_INTERNAL_ERROR;
	}
	if (0x10 != endianess)
	{
		DEBUG_WARN("unsupported endianess %d", endianess);
		return SCARD_F_INTERNAL_ERROR;
	}
	if (0x08 != header_length)
	{
		DEBUG_WARN("unsupported header length %d", header_length);
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
}

static UINT32 handle_PrivateTypeHeader(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 len;

	if (Stream_GetRemainingLength(irp->input) < 8)
	{
		DEBUG_WARN("length violation %d [%d]", 8,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	/* Process PrivateTypeHeader */
	Stream_Read_UINT32(irp->input, len);
	Stream_Seek_UINT32(irp->input);

	/* Assure the remaining length is as expected. */
	if (len < Stream_GetRemainingLength(irp->input))
	{
		DEBUG_WARN("missing payload %d [%d]",
				len, Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
}

static UINT32 handle_Context(SMARTCARD_DEVICE* smartcard, IRP* irp, int *redirect)
{
	UINT32 len;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	/* Process PrivateTypeHeader */
	Stream_Read_UINT32(irp->input, len);
	if (Stream_GetRemainingLength(irp->input) < len)
	{
		DEBUG_WARN("length violation %d [%d]", len,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}
	Stream_Seek(irp->input, len);
	if (!len)
	{
		DEBUG_WARN("Context handle is NULL, using from cache.");
		*redirect |= 0x01;
	}

	if (len > Stream_GetRemainingLength(irp->input))
	{
		DEBUG_WARN("length violation %d [%d]", len,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
}

static UINT32 handle_CardHandle(SMARTCARD_DEVICE* smartcard, IRP* irp, int* redirect)
{
	UINT32 status;
	UINT32 len;

	status = handle_Context(smartcard, irp, redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, len);
	if (Stream_GetRemainingLength(irp->input) < len)
	{
		DEBUG_WARN("length violation %d [%d]", len,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}
	Stream_Seek(irp->input, len);

	if (!len)
	{
		DEBUG_WARN("Card handle is NULL, using from cache.");
		*redirect |= 0x02;
	}

	return 0;
}

static UINT32 handle_RedirContextRef(SMARTCARD_DEVICE* smartcard, IRP* irp,
		int redirect, SCARDCONTEXT* hContext)
{
	UINT32 len;

	/* No context provided, use stored. */
	if (redirect & 0x01)
	{
		DEBUG_WARN("No context provided, using stored context.");
		*hContext = smartcard->hContext;
		return 0;
	}

	/* Extract context handle. */
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, len);
	if (len > Stream_GetRemainingLength(irp->input))
	{
		DEBUG_WARN("length violation %d [%d]", len,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	ZeroMemory(hContext, sizeof(SCARDCONTEXT));
	Stream_Read(irp->input, hContext, len);

	return 0;
}

static UINT32 handle_RedirHandleRef(SMARTCARD_DEVICE* smartcard, IRP* irp,
		int redirect, SCARDCONTEXT* hContext, SCARDHANDLE* hHandle)
{
	UINT32 len, status;

	status = handle_RedirContextRef(smartcard, irp, redirect, hContext);

	if (status)
		return status;

	/* Use stored card handle. */
	if (redirect & 0x02)
	{
		DEBUG_WARN("No card handle provided, using stored handle.");
		*hHandle = smartcard->hCard;
		return 0;
	}

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, len);
	if (len != 4)
	{
		DEBUG_WARN("length violation %d [%d]", len, 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(irp->input) < len)
	{
		DEBUG_WARN("length violation %d [%d]", len,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	ZeroMemory(hHandle, sizeof(SCARDHANDLE));
	Stream_Read_UINT32(irp->input, *hHandle);

	return 0;
}

static BOOL check_reader_is_forwarded(SMARTCARD_DEVICE* smartcard, const char* readerName)
{
	BOOL rc = TRUE;
	char *name = _strdup(readerName);
	char *str, *strpos=NULL, *strstatus=NULL;
	long pos, cpos, ret;

	/* Extract the name, position and status from the data provided. */
	str = strtok(name, " ");
	while(str)	
	{
		strpos = strstatus;
		strstatus = str;
		str = strtok(NULL, " ");
	} 

	if (!strpos)
		goto finally;

	pos = strtol(strpos, NULL, 10);
	
	if (strpos && strstatus)
	{
		/* Check, if the name of the reader matches. */
		if (smartcard->name &&  strncmp(smartcard->name, readerName, strlen(smartcard->name)))
			rc = FALSE;

		/* Check, if the position matches. */
		if (smartcard->path)
		{
			ret = sscanf(smartcard->path, "%ld", &cpos);
			if ((1 == ret) && (cpos != pos))
				rc = FALSE;
		}
	}
	else
	{
		DEBUG_WARN("unknown reader format '%s'", readerName);
	}

finally:
	free(name);

	if (!rc)
		DEBUG_WARN("reader '%s' not forwarded", readerName);
	
	return rc;
}

static BOOL check_handle_is_forwarded(SMARTCARD_DEVICE* smartcard, SCARDHANDLE hCard, SCARDCONTEXT hContext)
{
	BOOL rc = FALSE;
	LONG status;
	DWORD state = 0, protocol = 0;
	DWORD readerLen;
	DWORD atrLen = SCARD_ATR_LENGTH;
	char* readerName = NULL;
	BYTE pbAtr[SCARD_ATR_LENGTH];

	readerLen = SCARD_AUTOALLOCATE;

	status = SCardStatus(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);

	if (status == SCARD_S_SUCCESS)
	{
		rc = check_reader_is_forwarded(smartcard, readerName);
		if (!rc)
			DEBUG_WARN("Reader '%s' not forwarded!", readerName);
	}

	SCardFreeMemory(hContext, readerName);

	return rc;
}

static UINT32 smartcard_output_string(IRP* irp, char* src, BOOL wide)
{
	BYTE* p;
	UINT32 len;

	p = Stream_Pointer(irp->output);
	len = strlen(src) + 1;

	if (wide)
	{
		int i;

		for (i = 0; i < len; i++ )
		{
			p[2 * i] = src[i] < 0 ? '?' : src[i];
			p[2 * i + 1] = '\0';
		}

		len *= 2;
	}
	else
	{
		memcpy(p, src, len);
	}

	Stream_Seek(irp->output, len);
	return len;
}

static void smartcard_output_alignment(IRP* irp, UINT32 seed)
{
	const UINT32 field_lengths = 20;/* Remove the lengths of the fields
					 * RDPDR_HEADER, DeviceID,
					 * CompletionID, and IoStatus
					 * of Section 2.2.1.5.5 of MS-RDPEFS.
					 */
	UINT32 size = Stream_GetPosition(irp->output) - field_lengths;
	UINT32 add = (seed - (size % seed)) % seed;

	if (add > 0)
		Stream_Zero(irp->output, add);
}

static void smartcard_output_repos(IRP* irp, UINT32 written)
{
	UINT32 add = (4 - (written % 4)) % 4;

	if (add > 0)
		Stream_Zero(irp->output, add);
}

static UINT32 smartcard_output_return(IRP* irp, UINT32 status)
{
	Stream_Zero(irp->output, 256);
	return status;
}

static void smartcard_output_buffer_limit(IRP* irp, char* buffer, unsigned int length, unsigned int highLimit)
{
	int header = (length < 0) ? (0) : ((length > highLimit) ? (highLimit) : (length));

	Stream_Write_UINT32(irp->output, header);

	if (length <= 0)
	{
		Stream_Write_UINT32(irp->output, 0);
	}
	else
	{
		assert(NULL != buffer);
		if (header < length)
			length = header;

		Stream_Write(irp->output, buffer, length);
		smartcard_output_repos(irp, length);
	}
}

static void smartcard_output_buffer(IRP* irp, char* buffer, unsigned int length)
{
	smartcard_output_buffer_limit(irp, buffer, length, 0x7FFFFFFF);
}

static void smartcard_output_buffer_start_limit(IRP* irp, int length, int highLimit)
{
	int header = (length < 0) ? (0) : ((length > highLimit) ? (highLimit) : (length));

	Stream_Write_UINT32(irp->output, header);
	Stream_Write_UINT32(irp->output, 0x00000001);	/* Magic DWORD - any non zero */
}

static void smartcard_output_buffer_start(IRP* irp, int length)
{
	smartcard_output_buffer_start_limit(irp, length, 0x7FFFFFFF);
}

static UINT32 smartcard_input_string(IRP* irp, char** dest, UINT32 dataLength, BOOL wide)
{
	char* buffer;
	int bufferSize;

	bufferSize = wide ? (2 * dataLength) : dataLength;
	buffer = malloc(bufferSize + 2); /* reserve 2 bytes for the '\0' */

	Stream_Read(irp->input, buffer, bufferSize);

	if (wide)
	{
		int i;
		for (i = 0; i < dataLength; i++)
		{
			if ((buffer[2 * i] < 0) || (buffer[2 * i + 1] != 0))
				buffer[i] = '?';
			else
				buffer[i] = buffer[2 * i];
		}
	}

	buffer[dataLength] = '\0';
	*dest = buffer;

	return bufferSize;
}

static void smartcard_input_repos(IRP* irp, UINT32 read)
{
	UINT32 add = 4 - (read % 4);

	if (add < 4 && add > 0)
		Stream_Seek(irp->input, add);
}

static UINT32 smartcard_input_reader_name(IRP* irp, char** dest, BOOL wide)
{
	UINT32 dataLength;

	assert(irp);
	assert(dest);

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 8);
	Stream_Read_UINT32(irp->input, dataLength);

	if (Stream_GetRemainingLength(irp->input) < dataLength)
	{
		DEBUG_WARN("length violation %d [%d]", dataLength,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	smartcard_input_repos(irp, smartcard_input_string(irp, dest, dataLength, wide));

	return 0;
}

static UINT32 handle_EstablishContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	UINT32 scope;
	SCARDCONTEXT hContext = -1;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	/* Ensure, that the capacity expected is actually available. */
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	/* Read the scope from the stream. */
	Stream_Read_UINT32(irp->input, scope);

	status = SCardEstablishContext(scope, NULL, NULL, &hContext);

	Stream_Write_UINT32(irp->output, 4);	// cbContext
	Stream_Write_UINT32(irp->output, -1);	// ReferentID

	Stream_Write_UINT32(irp->output, 4);
	Stream_Write_UINT32(irp->output, hContext);

	/* store hContext in allowed context list */
	smartcard->hContext = hContext;

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_ReleaseContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	UINT32 status;
	SCARDCONTEXT hContext = -1;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		return status;

	status = SCardReleaseContext(hContext);
	ZeroMemory(&smartcard->hContext, sizeof(smartcard->hContext));

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_IsValidContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	UINT32 status;
	SCARDCONTEXT hContext;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		return status;

	status = SCardIsValidContext(hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_ListReaders(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	int redirect = 0;
	UINT32 status;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	UINT32 cBytes;
	INT32 fmszReadersIsNULL;
	UINT32 cchReaders;
	LPTSTR mszGroups = NULL;
	char *readerList = NULL, *walker;
	int elemLength, dataLength;
	int pos, poslen1, poslen2, allowed_pos;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Read_UINT32(irp->input, cBytes);

	/* Ensure, that the capacity expected is actually available. */
	if (Stream_GetRemainingLength(irp->input) < cBytes)
	{
		DEBUG_WARN("length violation %d [%d]", cBytes,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	if (cBytes)
	{
		mszGroups = malloc(cBytes);
		Stream_Read(irp->input, mszGroups, cBytes);
	}

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Read_UINT32(irp->input, fmszReadersIsNULL);

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Read_UINT32(irp->input, cchReaders);
	
	Stream_Seek(irp->input, 4);

	/* Read RedirScardcontextRef */
	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);
	if (status)
		goto finish;

	/* ignore rest of [MS-RDPESC] 2.2.2.4 ListReaders_Call */

	dwReaders = SCARD_AUTOALLOCATE;
	status = SCardListReaders(hContext, mszGroups, (LPSTR) &readerList, &dwReaders);

	if (status != SCARD_S_SUCCESS)
	{
		goto finish;
	}

	poslen1 = Stream_GetPosition(irp->output);
	Stream_Seek_UINT32(irp->output);

	Stream_Write_UINT32(irp->output, 0x01760650);

	poslen2 = Stream_GetPosition(irp->output);
	Stream_Seek_UINT32(irp->output);

	walker = readerList;
	dataLength = 0;

	/* Smartcards can be forwarded by position and name. */
	allowed_pos = -1;
	if (smartcard->path)
	{
		if (1 != sscanf(smartcard->path, "%d", &allowed_pos))
			allowed_pos = -1;
	}

	pos = 0;
	while (1)
	{
		elemLength = strlen(walker);
		if (elemLength == 0)
			break;

		/* Ignore readers not forwarded. */
		if ((allowed_pos < 0) || (pos == allowed_pos))
		{
			if (!smartcard->name || strstr(walker, smartcard->name))
				dataLength += smartcard_output_string(irp, walker, wide);
		}
		walker += elemLength + 1;
		pos ++;
	}

	dataLength += smartcard_output_string(irp, "\0", wide);

	pos = Stream_GetPosition(irp->output);

	Stream_SetPosition(irp->output, poslen1);
	Stream_Write_UINT32(irp->output, dataLength);
	Stream_SetPosition(irp->output, poslen2);
	Stream_Write_UINT32(irp->output, dataLength);

	Stream_SetPosition(irp->output, pos);

	smartcard_output_repos(irp, dataLength);
	smartcard_output_alignment(irp, 8);

finish:
	if (readerList)
	{
		SCardFreeMemory(hContext, readerList);
	}

	if (mszGroups)
		free(mszGroups);

	return status;
}

static UINT32 handle_GetStatusChange(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	int i;
	LONG status;
	int redirect = 0;
	SCARDCONTEXT hContext;
	DWORD dwTimeout = 0;
	DWORD readerCount = 0;
	SCARD_READERSTATE *readerStates = NULL, *cur;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		goto finish;

	/* Ensure, that the capacity expected is actually available. */
	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Read_UINT32(irp->input, dwTimeout);
	Stream_Read_UINT32(irp->input, readerCount);

	/* Skip reader state */
	Stream_Seek(irp->input, 4);

	/* Get context */
	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		goto finish;

	/* Skip ReaderStateConformant */
	if (Stream_GetRemainingLength(irp->input) < 4 )
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Seek(irp->input, 4);

	if (readerCount > 0)
	{
		readerStates = malloc(readerCount * sizeof(SCARD_READERSTATE));
		ZeroMemory(readerStates, readerCount * sizeof(SCARD_READERSTATE));

		for (i = 0; i < readerCount; i++)
		{
			cur = &readerStates[i];

			if (Stream_GetRemainingLength(irp->input) < 52 )
			{
				DEBUG_WARN("length violation %d [%d]", 52,
					Stream_GetRemainingLength(irp->input));
				status = SCARD_F_INTERNAL_ERROR;
				goto finish;
			}

			Stream_Seek(irp->input, 4);

			/*
			 * TODO: on-wire is little endian; need to either
			 * convert to host endian or fix the headers to
			 * request the order we want
			 */
			Stream_Read_UINT32(irp->input, cur->dwCurrentState);
			Stream_Read_UINT32(irp->input, cur->dwEventState);
			Stream_Read_UINT32(irp->input, cur->cbAtr);
			Stream_Read(irp->input, cur->rgbAtr, 32);

			Stream_Seek(irp->input, 4);

			/* reset high bytes? */
			cur->dwCurrentState &= 0x0000FFFF;
			cur->dwEventState = 0;
		}

		for (i = 0; i < readerCount; i++)
		{
			cur = &readerStates[i];
			UINT32 dataLength;

			if (Stream_GetRemainingLength(irp->input) < 12 )
			{
				DEBUG_WARN("length violation %d [%d]", 12,
					Stream_GetRemainingLength(irp->input));
				status = SCARD_F_INTERNAL_ERROR;
				goto finish;
			}

			Stream_Seek(irp->input, 8);
			Stream_Read_UINT32(irp->input, dataLength);

			if (Stream_GetRemainingLength(irp->input) < dataLength )
			{
				DEBUG_WARN("length violation %d [%d]", dataLength,
					Stream_GetRemainingLength(irp->input));
				status = SCARD_F_INTERNAL_ERROR;
				goto finish;
			}
			smartcard_input_repos(irp, smartcard_input_string(irp,
						(char **) &cur->szReader, dataLength, wide));

			if (!cur->szReader)
			{
				DEBUG_WARN("cur->szReader=%p", cur->szReader);
				continue;
			}
			if (strcmp(cur->szReader, "\\\\?PnP?\\Notification") == 0)
				cur->dwCurrentState |= SCARD_STATE_IGNORE;
		}
	}
	else
	{
		readerStates = NULL;
	}

	status = SCardGetStatusChange(hContext, (DWORD) dwTimeout, readerStates, (DWORD) readerCount);

	Stream_Write_UINT32(irp->output, readerCount);
	Stream_Write_UINT32(irp->output, 0x00084dd8);
	Stream_Write_UINT32(irp->output, readerCount);

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];

		/* TODO: do byte conversions if necessary */
		Stream_Write_UINT32(irp->output, cur->dwCurrentState);
		Stream_Write_UINT32(irp->output, cur->dwEventState);
		Stream_Write_UINT32(irp->output, cur->cbAtr);
		Stream_Write(irp->output, cur->rgbAtr, 32);

		Stream_Zero(irp->output, 4);
	}

	smartcard_output_alignment(irp, 8);

finish:
	if (readerStates)
	{
		for (i = 0; i < readerCount; i++)
		{
			cur = &readerStates[i];

			if (cur->szReader)
				free((void*) cur->szReader);
			cur->szReader = NULL;
		}
		free(readerStates);
	}

	return status;
}

static UINT32 handle_Cancel(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDCONTEXT hContext;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		return status;

	status = SCardCancel(hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_Connect(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	int redirect = 0;
	LONG status;
	SCARDCONTEXT hContext;
	char* readerName = NULL;
	DWORD dwShareMode = 0;
	DWORD dwPreferredProtocol = 0;
	DWORD dwActiveProtocol = 0;
	SCARDHANDLE hCard;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	/* Skip ptrReader */
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("Length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Seek(irp->input, 4);

	/* Read common data */
	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		goto finish;
	
	if (Stream_GetRemainingLength(irp->input) < 8)
	{
		DEBUG_WARN("Length violadion %d [%d]", 8,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Read_UINT32(irp->input, dwShareMode);
	Stream_Read_UINT32(irp->input, dwPreferredProtocol);

	status = smartcard_input_reader_name(irp, &readerName, wide);

	if (status)
		goto finish;
	
	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		goto finish;

	if (!check_reader_is_forwarded(smartcard, readerName))
	{
		DEBUG_WARN("Reader '%s' not forwarded!", readerName);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	status = SCardConnect(hContext, readerName, (DWORD) dwShareMode,
		(DWORD) dwPreferredProtocol, &hCard, (DWORD *) &dwActiveProtocol);

	smartcard->hCard = hCard;

	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, 0x016Cff34);
	Stream_Write_UINT32(irp->output, dwActiveProtocol);
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, hCard);

	smartcard_output_alignment(irp, 8);

finish:
	if (readerName)
		free(readerName);

	return status;
}

static UINT32 handle_Reconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwShareMode = 0;
	DWORD dwPreferredProtocol = 0;
	DWORD dwInitialization = 0;
	DWORD dwActiveProtocol = 0;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, dwShareMode);
	Stream_Read_UINT32(irp->input, dwPreferredProtocol);
	Stream_Read_UINT32(irp->input, dwInitialization);

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardReconnect(hCard, (DWORD) dwShareMode, (DWORD) dwPreferredProtocol,
			(DWORD) dwInitialization, (LPDWORD) &dwActiveProtocol);

	Stream_Write_UINT32(irp->output, dwActiveProtocol);
	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_Disconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwDisposition = 0;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, dwDisposition);
	
	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardDisconnect(hCard, (DWORD) dwDisposition);

	ZeroMemory(&smartcard->hCard, sizeof(smartcard->hCard));

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_BeginTransaction(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}
	Stream_Seek(irp->input, 4);

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardBeginTransaction(hCard);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_EndTransaction(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	DWORD dwDisposition = 0;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}
	Stream_Read_UINT32(irp->input, dwDisposition);

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardEndTransaction(hCard, dwDisposition);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_State(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	DWORD state = 0, protocol = 0;
	DWORD readerLen;
	DWORD atrLen = SCARD_ATR_LENGTH;
	char* readerName = NULL;
	BYTE pbAtr[SCARD_ATR_LENGTH];

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 8)
	{
		DEBUG_WARN("length violation %d [%d]", 8,
			Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Seek(irp->input, 4);
	Stream_Seek_UINT32(irp->input);	/* atrLen */

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		goto finish;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	readerLen = SCARD_AUTOALLOCATE;
	status = SCardStatus(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}

	Stream_Write_UINT32(irp->output, state);
	Stream_Write_UINT32(irp->output, protocol);
	Stream_Write_UINT32(irp->output, atrLen);
	Stream_Write_UINT32(irp->output, 0x00000001);
	Stream_Write_UINT32(irp->output, atrLen);
	Stream_Write(irp->output, pbAtr, atrLen);

	smartcard_output_repos(irp, atrLen);
	smartcard_output_alignment(irp, 8);

finish:
	if (readerName)
	{
		SCardFreeMemory(hContext, readerName);
	}

	return status;
}

static DWORD handle_Status(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	DWORD state, protocol;
	DWORD readerLen = 0;
	DWORD atrLen = SCARD_ATR_LENGTH;
	char* readerName = NULL;
	BYTE *pbAtr = NULL;
	UINT32 dataLength = 0;
	int pos, poslen1, poslen2;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Seek(irp->input, 4);
	Stream_Read_UINT32(irp->input, readerLen);
	Stream_Read_UINT32(irp->input, atrLen);
	
	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		goto finish;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	pbAtr = malloc(sizeof(BYTE) * atrLen);

	readerLen = SCARD_AUTOALLOCATE;
	status = SCardStatus(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}

	poslen1 = Stream_GetPosition(irp->output);
	Stream_Write_UINT32(irp->output, readerLen);
	Stream_Write_UINT32(irp->output, 0x00020000);
	Stream_Write_UINT32(irp->output, state);
	Stream_Write_UINT32(irp->output, protocol);
	Stream_Write(irp->output, pbAtr, atrLen);

	if (atrLen < 32)
		Stream_Zero(irp->output, 32 - atrLen);
	Stream_Write_UINT32(irp->output, atrLen);

	poslen2 = Stream_GetPosition(irp->output);
	Stream_Write_UINT32(irp->output, readerLen);

	if (readerName)
		dataLength += smartcard_output_string(irp, readerName, wide);
	dataLength += smartcard_output_string(irp, "\0", wide);
	smartcard_output_repos(irp, dataLength);

	pos = Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, poslen1);
	Stream_Write_UINT32(irp->output,dataLength);
	Stream_SetPosition(irp->output, poslen2);
	Stream_Write_UINT32(irp->output,dataLength);
	Stream_SetPosition(irp->output, pos);

	smartcard_output_alignment(irp, 8);

finish:
	if (readerName)
	{
		SCardFreeMemory(hContext, readerName);
	}

	if (pbAtr)
		free(pbAtr);

	return status;
}

static UINT32 handle_Transmit(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	UINT32 pioSendPciBufferPtr;
	UINT32 ptrSendBuffer;
	UINT32 ptrIoRecvPciBuffer;
	UINT32 recvBufferIsNULL;
	UINT32 linkedLen;
	void *tmp;
	union
	{
		SCARD_IO_REQUEST *rq;
		UINT32 *u32;
		void *v;
	} ioSendPci, ioRecvPci;

	SCARD_IO_REQUEST *pPioRecvPci = NULL;
	DWORD cbSendLength = 0, cbRecvLength = 0;
	BYTE *sendBuf = NULL, *recvBuf = NULL;

	ioSendPci.v = NULL;
	ioRecvPci.v = NULL;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		DEBUG_WARN("length violation %d [%d]", 32,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	ioSendPci.v = malloc(sizeof(SCARD_IO_REQUEST));
	ioRecvPci.v = malloc(sizeof(SCARD_IO_REQUEST));

	Stream_Read_UINT32(irp->input, ioSendPci.rq->dwProtocol);
	Stream_Read_UINT32(irp->input, ioSendPci.rq->cbPciLength);
	Stream_Read_UINT32(irp->input, pioSendPciBufferPtr);

	Stream_Read_UINT32(irp->input, cbSendLength);
	Stream_Read_UINT32(irp->input, ptrSendBuffer);
	Stream_Read_UINT32(irp->input, ptrIoRecvPciBuffer);
	Stream_Read_UINT32(irp->input, recvBufferIsNULL);
	Stream_Read_UINT32(irp->input, cbRecvLength);

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);
	if (status)
		goto finish;
	
	/* Check, if there is data available from the ipSendPci element */
	if (pioSendPciBufferPtr)
	{
		if (Stream_GetRemainingLength(irp->input) < 8)
		{
			DEBUG_WARN("length violation %d [%d]", 8,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		Stream_Read_UINT32(irp->input, linkedLen);

		if (Stream_GetRemainingLength(irp->input) < ioSendPci.rq->cbPciLength)
		{
			DEBUG_WARN("length violation %d [%d]", ioSendPci.rq->cbPciLength,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		
		/* For details see 2.2.1.8 SCardIO_Request in MS-RDPESC and
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa379807%28v=vs.85%29.aspx
		 */
		if (linkedLen < ioSendPci.rq->cbPciLength - sizeof(SCARD_IO_REQUEST))
		{
			DEBUG_WARN("SCARD_IO_REQUEST with invalid extra byte length %d [%d]",
					ioSendPci.rq->cbPciLength - sizeof(SCARD_IO_REQUEST), linkedLen);
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}

		/* Invalid length received. */
		if (!ioSendPci.rq->cbPciLength)
		{
			if (ioSendPci.v)
				free(ioSendPci.v);
			ioSendPci.v = NULL;
		}
		else
		{
			tmp = realloc(ioSendPci.v, ioSendPci.rq->cbPciLength);
			ioSendPci.v = tmp;
		
			Stream_Read(irp->input, &ioSendPci.rq[1], ioSendPci.rq->cbPciLength);
		}
	}
	else
		ioSendPci.rq->cbPciLength = sizeof(SCARD_IO_REQUEST);

	/* Check, if there is data available from the SendBufferPointer */
	if (ptrSendBuffer)
	{
		if (Stream_GetRemainingLength(irp->input) < 4)
		{
			DEBUG_WARN("length violation %d [%d]", 4,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		Stream_Read_UINT32(irp->input, linkedLen);

		/* Just check for too few bytes, there may be more actual
		 * data than is used due to padding. */
		if (linkedLen < cbSendLength)
		{
			DEBUG_WARN("SendBuffer invalid byte length %d [%d]",
					cbSendLength, linkedLen);
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		if (Stream_GetRemainingLength(irp->input) < cbSendLength)
		{
			DEBUG_WARN("length violation %d [%d]", cbSendLength,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		sendBuf = malloc(cbSendLength);
		Stream_Read(irp->input, sendBuf, cbSendLength);
	}

	/* Check, if a response is desired. */
	if (cbRecvLength && !recvBufferIsNULL)
		recvBuf = malloc(cbRecvLength);
	else
		cbRecvLength = 0;

	if (ptrIoRecvPciBuffer)
	{
		if (Stream_GetRemainingLength(irp->input) < 8)
		{
			DEBUG_WARN("length violation %d [%d]", 8,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		/* recvPci */
		Stream_Read_UINT32(irp->input, linkedLen);
		Stream_Read_UINT16(irp->input, ioRecvPci.rq->dwProtocol);
		Stream_Read_UINT16(irp->input, ioRecvPci.rq->cbPciLength);
	
		/* Just check for too few bytes, there may be more actual
		 * data than is used due to padding. */
		if (linkedLen < ioRecvPci.rq->cbPciLength)
		{
			DEBUG_WARN("SCARD_IO_REQUEST with invalid extra byte length %d [%d]",
					ioRecvPci.rq->cbPciLength - sizeof(SCARD_IO_REQUEST), linkedLen);
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}

		if (Stream_GetRemainingLength(irp->input) < ioRecvPci.rq->cbPciLength)
		{
			DEBUG_WARN("length violation %d [%d]", ioRecvPci.rq->cbPciLength,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}

		/* Read data, see
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa379807%28v=vs.85%29.aspx
		 */
		if (!ioRecvPci.rq->cbPciLength)
		{
			if (ioRecvPci.v)
				free(ioRecvPci.v);
			ioRecvPci.v = NULL;
		}
		else
		{
			tmp = realloc(ioRecvPci.v, ioRecvPci.rq->cbPciLength);
			ioRecvPci.v = tmp;
		
			Stream_Read(irp->input, &ioRecvPci.rq[1], ioRecvPci.rq->cbPciLength);
		}

		pPioRecvPci = ioRecvPci.rq;
	}
	else
	{
		pPioRecvPci = NULL;
	}

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	status = SCardTransmit(hCard, ioSendPci.rq, sendBuf, cbSendLength,
			   pPioRecvPci, recvBuf, &cbRecvLength);

	if (status != SCARD_S_SUCCESS)
	{

	}
	else
	{
		Stream_Write_UINT32(irp->output, 0); 	/* pioRecvPci 0x00; */

		if (recvBuf)
		{
			smartcard_output_buffer_start(irp, cbRecvLength);	/* start of recvBuf output */
			smartcard_output_buffer(irp, (char*) recvBuf, cbRecvLength);
		}
	}

	smartcard_output_alignment(irp, 8);

finish:
	if (sendBuf)
		free(sendBuf);
	if (recvBuf)
		free(recvBuf);
	if (ioSendPci.v)
		free(ioSendPci.v);
	if (ioRecvPci.v)
		free(ioRecvPci.v);

	return status;
}

static UINT32 handle_Control(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	UINT32 pvInBuffer, fpvOutBufferIsNULL;
	UINT32 controlCode;
	UINT32 controlFunction;
	BYTE* recvBuffer = NULL;
	BYTE* sendBuffer = NULL;
	UINT32 recvLength;
	DWORD nBytesReturned;
	DWORD outBufferSize;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		goto finish;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 20)
	{
		DEBUG_WARN("length violation %d [%d]", 20,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Read_UINT32(irp->input, controlCode);
	Stream_Read_UINT32(irp->input, recvLength);
	Stream_Read_UINT32(irp->input, pvInBuffer);
	Stream_Read_UINT32(irp->input, fpvOutBufferIsNULL);
	Stream_Read_UINT32(irp->input, outBufferSize);

	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		goto finish;

	/* Translate Windows SCARD_CTL_CODE's to corresponding local code */
	if (DEVICE_TYPE_FROM_CTL_CODE(controlCode) == FILE_DEVICE_SMARTCARD)
	{
		controlFunction = FUNCTION_FROM_CTL_CODE(controlCode);
		controlCode = SCARD_CTL_CODE(controlFunction);
	}

	if (pvInBuffer)
	{
		/* Get the size of the linked data. */
		if (Stream_GetRemainingLength(irp->input) < 4)
		{
			DEBUG_WARN("length violation %d [%d]", 4,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		Stream_Read_UINT32(irp->input, recvLength);

		/* Check, if there is actually enough data... */
		if (Stream_GetRemainingLength(irp->input) < recvLength)
		{
			DEBUG_WARN("length violation %d [%d]", recvLength,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}
		recvBuffer = malloc(recvLength);

		Stream_Read(irp->input, recvBuffer, recvLength);
	}

	nBytesReturned = outBufferSize;
	sendBuffer = malloc(outBufferSize);

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	status = SCardControl(hCard, (DWORD) controlCode, recvBuffer, (DWORD) recvLength,
		sendBuffer, (DWORD) outBufferSize, &nBytesReturned);

	Stream_Write_UINT32(irp->output, (UINT32) nBytesReturned);
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, nBytesReturned);

	if (nBytesReturned > 0)
	{
		Stream_Write(irp->output, sendBuffer, nBytesReturned);
		smartcard_output_repos(irp, nBytesReturned);
	}

	smartcard_output_alignment(irp, 8);

finish:
	if (recvBuffer)
		free(recvBuffer);
	if (sendBuffer)
		free(sendBuffer);

	return status;
}

static UINT32 handle_GetAttrib(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	int redirect = 0;
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	DWORD dwAttrId = 0;
	DWORD dwAttrLen = 0;
	DWORD attrLen = 0;
	BYTE* pbAttr = NULL;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_CardHandle(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, dwAttrId);
	Stream_Seek(irp->input, 0x4);
	Stream_Read_UINT32(irp->input, dwAttrLen);
	
	status = handle_RedirHandleRef(smartcard, irp, redirect, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	attrLen = (dwAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
	status = SCardGetAttrib(hCard, dwAttrId, attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

	if (status != SCARD_S_SUCCESS)
	{
		attrLen = (dwAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
	}

	if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
			attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

		if (status != SCARD_S_SUCCESS)
		{
			attrLen = (dwAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
		}
	}

	if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
			attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

		if (status != SCARD_S_SUCCESS)
		{
			attrLen = (dwAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
		}
	}
	if (attrLen > dwAttrLen && pbAttr != NULL)
	{
		status = SCARD_E_INSUFFICIENT_BUFFER;
	}
	dwAttrLen = attrLen;

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}
	else
	{
		Stream_Write_UINT32(irp->output, dwAttrLen);
		Stream_Write_UINT32(irp->output, 0x00000200);
		Stream_Write_UINT32(irp->output, dwAttrLen);

		if (!pbAttr)
		{
			Stream_Zero(irp->output, dwAttrLen);
		}
		else
		{
			Stream_Write(irp->output, pbAttr, dwAttrLen);
		}
		smartcard_output_repos(irp, dwAttrLen);
		/* align to multiple of 4 */
		Stream_Write_UINT32(irp->output, 0);
	}
	smartcard_output_alignment(irp, 8);

finish:
	SCardFreeMemory(hContext, pbAttr);

	return status;
}

static UINT32 handle_AccessStartedEvent(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4, Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 4);

	smartcard_output_alignment(irp, 8);
	
	return SCARD_S_SUCCESS;
}

void scard_error(SMARTCARD_DEVICE* smartcard, IRP* irp, UINT32 ntstatus)
{
	/* [MS-RDPESC] 3.1.4.4 */
	DEBUG_WARN("scard processing error %x", ntstatus);

	Stream_SetPosition(irp->output, 0);	/* CHECKME */
	irp->IoStatus = ntstatus;
	irp->Complete(irp);
}

static UINT32 handle_LocateCardsByATR(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	int redirect = 0;
	LONG status;
	int i, j, k;
	SCARDCONTEXT hContext;
	UINT32 atrMaskCount = 0;
	UINT32 readerCount = 0;
	SCARD_READERSTATE* cur = NULL;
	SCARD_READERSTATE* rsCur = NULL;
	SCARD_READERSTATE* readerStates = NULL;
	SCARD_ATRMASK* curAtr = NULL;
	SCARD_ATRMASK* pAtrMasks = NULL;

	status = handle_CommonTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_PrivateTypeHeader(smartcard, irp);

	if (status)
		return status;

	status = handle_Context(smartcard, irp, &redirect);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 4);
	status = handle_RedirContextRef(smartcard, irp, redirect, &hContext);

	if (status)
		return status;

	Stream_Seek(irp->input, 0x2C);
	Stream_Read_UINT32(irp->input, hContext);
	Stream_Read_UINT32(irp->input, atrMaskCount);

	pAtrMasks = malloc(atrMaskCount * sizeof(SCARD_ATRMASK));

	if (!pAtrMasks)
		return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

	for (i = 0; i < atrMaskCount; i++)
	{
		Stream_Read_UINT32(irp->input, pAtrMasks[i].cbAtr);
		Stream_Read(irp->input, pAtrMasks[i].rgbAtr, 36);
		Stream_Read(irp->input, pAtrMasks[i].rgbMask, 36);
	}

	Stream_Read_UINT32(irp->input, readerCount);

	readerStates = malloc(readerCount * sizeof(SCARD_READERSTATE));
	ZeroMemory(readerStates, readerCount * sizeof(SCARD_READERSTATE));

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];

		Stream_Seek(irp->input, 4);

		/*
		 * TODO: on-wire is little endian; need to either
		 * convert to host endian or fix the headers to
		 * request the order we want
		 */
		Stream_Read_UINT32(irp->input, cur->dwCurrentState);
		Stream_Read_UINT32(irp->input, cur->dwEventState);
		Stream_Read_UINT32(irp->input, cur->cbAtr);
		Stream_Read(irp->input, cur->rgbAtr, 32);

		Stream_Seek(irp->input, 4);

		/* reset high bytes? */
		cur->dwCurrentState &= 0x0000FFFF;
		cur->dwEventState &= 0x0000FFFF;
		cur->dwEventState = 0;
	}

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];
		UINT32 dataLength;

		Stream_Seek(irp->input, 8);
		Stream_Read_UINT32(irp->input, dataLength);
		smartcard_input_repos(irp, smartcard_input_string(irp, (char **) &cur->szReader, dataLength, wide));

		if (!cur->szReader)
		{
			DEBUG_WARN("cur->szReader=%p", cur->szReader);
			continue;
		}
		if (strcmp(cur->szReader, "\\\\?PnP?\\Notification") == 0)
			cur->dwCurrentState |= SCARD_STATE_IGNORE;
	}

	status = SCardGetStatusChange(hContext, 0x00000001, readerStates, readerCount);

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}

	for (i = 0, curAtr = pAtrMasks; i < atrMaskCount; i++, curAtr++)
	{
		for (j = 0, rsCur = readerStates; j < readerCount; j++, rsCur++)
		{
			BOOL equal = 1;
			for (k = 0; k < cur->cbAtr; k++)
			{
				if ((curAtr->rgbAtr[k] & curAtr->rgbMask[k]) !=
				    (rsCur->rgbAtr[k] & curAtr->rgbMask[k]))
				{
					equal = 0;
					break;
				}
			}
			if (equal)
			{
				rsCur->dwEventState |= 0x00000040;	/* SCARD_STATE_ATRMATCH 0x00000040 */
			}
		}
	}

	Stream_Write_UINT32(irp->output, readerCount);
	Stream_Write_UINT32(irp->output, 0x00084dd8);
	Stream_Write_UINT32(irp->output, readerCount);

	for (i = 0, cur = readerStates; i < readerCount; i++, cur++)
	{
		Stream_Write_UINT32(irp->output, cur->dwCurrentState);
		Stream_Write_UINT32(irp->output, cur->dwEventState);
		Stream_Write_UINT32(irp->output, cur->cbAtr);
		Stream_Write(irp->output, cur->rgbAtr, 32);

		Stream_Zero(irp->output, 4);
	}

	smartcard_output_alignment(irp, 8);

finish:
	for (i = 0, cur = readerStates; i < readerCount; i++, cur++)
	{
		if (cur->szReader)
			free((void*) cur->szReader);
		cur->szReader = NULL;
	}

	if (readerStates)
		free(readerStates);

	if (pAtrMasks)
		free(pAtrMasks);

	return status;
}

BOOL smartcard_async_op(IRP* irp)
{
	UINT32 ioctl_code;

	/* peek ahead */
	Stream_Seek(irp->input, 8);
	Stream_Read_UINT32(irp->input, ioctl_code);
	Stream_Rewind(irp->input, 12);

	switch (ioctl_code)
	{
		/* non-blocking events */
		case SCARD_IOCTL_ACCESS_STARTED_EVENT:

		case SCARD_IOCTL_ESTABLISH_CONTEXT:
		case SCARD_IOCTL_RELEASE_CONTEXT:
		case SCARD_IOCTL_IS_VALID_CONTEXT:

			return FALSE;
			break;

		/* async events */
		case SCARD_IOCTL_GET_STATUS_CHANGE:
		case SCARD_IOCTL_GET_STATUS_CHANGE + 4:

		case SCARD_IOCTL_TRANSMIT:

		case SCARD_IOCTL_STATUS:
		case SCARD_IOCTL_STATUS + 4:
			return TRUE;
			break;

		default:
			break;
	}	

	/* default to async */
	return TRUE;
}

void smartcard_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 pos;
	UINT32 result;
	UINT32 result_pos;
	UINT32 output_len;
	UINT32 input_len;
	UINT32 ioctl_code;
	UINT32 stream_len;
	UINT32 irp_result_pos;
	UINT32 output_len_pos;
	const UINT32 header_lengths = 16;

	/* MS-RPCE, Sections 2.2.6.1 and 2.2.6.2. */
	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		DEBUG_WARN("Invalid IRP of length %d received, ignoring.",
				Stream_GetRemainingLength(irp->input));
		return;
	}

	Stream_Read_UINT32(irp->input, output_len);
	Stream_Read_UINT32(irp->input, input_len);
	Stream_Read_UINT32(irp->input, ioctl_code);

	Stream_Seek(irp->input, 20);	/* padding */

	// Stream_Seek(irp->input, 4);	/* TODO: parse len, le, v1 */
	// Stream_Seek(irp->input, 4);	/* 0xcccccccc */
	// Stream_Seek(irp->input, 4);	/* rpce len */

	/* [MS-RDPESC] 3.2.5.1 Sending Outgoing Messages */
	Stream_EnsureRemainingCapacity(irp->output, 2048);

	irp_result_pos = Stream_GetPosition(irp->output);

	Stream_Write_UINT32(irp->output, 0x00000000); 	/* MS-RDPEFS 
							 * OutputBufferLength
							 * will be updated 
							 * later in this 
							 * function.
							 */
	/* [MS-RPCE] 2.2.6.1 */
	Stream_Write_UINT32(irp->output, 0x00081001); /* len 8, LE, v1 */
	Stream_Write_UINT32(irp->output, 0xcccccccc); /* filler */

	output_len_pos = Stream_GetPosition(irp->output);
	Stream_Seek(irp->output, 4);		/* size */

	Stream_Write_UINT32(irp->output, 0x0);	/* filler */

	result_pos = Stream_GetPosition(irp->output);
	Stream_Seek(irp->output, 4);		/* result */

	/* Ensure, that this package is fully available. */
	if (Stream_GetRemainingLength(irp->input) < input_len)
	{
		DEBUG_WARN("Invalid IRP of length %d received, expected %d, ignoring.",
				Stream_GetRemainingLength(irp->input), input_len);
		return;
	}

	/* body. input_len contains the length of the remaining data
	 * that can be read from the current position of irp->input,
	 * so pass it on ;) */

	switch (ioctl_code)
	{
		case SCARD_IOCTL_ESTABLISH_CONTEXT:
			result = handle_EstablishContext(smartcard, irp);
			break;

		case SCARD_IOCTL_IS_VALID_CONTEXT:
			result = handle_IsValidContext(smartcard, irp);
			break;

		case SCARD_IOCTL_RELEASE_CONTEXT:
			result = handle_ReleaseContext(smartcard, irp);
			break;

		case SCARD_IOCTL_LIST_READERS:
			result = handle_ListReaders(smartcard, irp, 0);
			break;
		case SCARD_IOCTL_LIST_READERS + 4:
			result = handle_ListReaders(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_LIST_READER_GROUPS:
		case SCARD_IOCTL_LIST_READER_GROUPS + 4:
			/* typically not used unless list_readers fail */
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GET_STATUS_CHANGE:
			result = handle_GetStatusChange(smartcard, irp, 0);
			break;
		case SCARD_IOCTL_GET_STATUS_CHANGE + 4:
			result = handle_GetStatusChange(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_CANCEL:
			result = handle_Cancel(smartcard, irp);
			break;

		case SCARD_IOCTL_CONNECT:
			result = handle_Connect(smartcard, irp, 0);
			break;
		case SCARD_IOCTL_CONNECT + 4:
			result = handle_Connect(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_RECONNECT:
			result = handle_Reconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_DISCONNECT:
			result = handle_Disconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_BEGIN_TRANSACTION:
			result = handle_BeginTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_END_TRANSACTION:
			result = handle_EndTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_STATE:
			result = handle_State(smartcard, irp);
			break;

		case SCARD_IOCTL_STATUS:
			result = handle_Status(smartcard, irp, 0);
			break;
		case SCARD_IOCTL_STATUS + 4:
			result = handle_Status(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_TRANSMIT:
			result = handle_Transmit(smartcard, irp);
			break;

		case SCARD_IOCTL_CONTROL:
			result = handle_Control(smartcard, irp);
			break;

		case SCARD_IOCTL_GETATTRIB:
			result = handle_GetAttrib(smartcard, irp);
			break;

		case SCARD_IOCTL_ACCESS_STARTED_EVENT:
			result = handle_AccessStartedEvent(smartcard, irp);
			break;

		case SCARD_IOCTL_LOCATE_CARDS_BY_ATR:
			result = handle_LocateCardsByATR(smartcard, irp, 0);
			break;
		case SCARD_IOCTL_LOCATE_CARDS_BY_ATR + 4:
			result = handle_LocateCardsByATR(smartcard, irp, 1);
			break;

		default:
			result = 0xc0000001;
			DEBUG_WARN("scard unknown ioctl 0x%x [%d]\n",
					ioctl_code, input_len);
			break;
	}

	/* look for NTSTATUS errors */
	if ((result & 0xc0000000) == 0xc0000000)
		return scard_error(smartcard, irp, result);

	/* per Ludovic Rousseau, map different usage of this particular
  	 * error code between pcsc-lite & windows */
	if (result == 0x8010001F)
		result = 0x80100022;

	/* handle response packet */
	pos = Stream_GetPosition(irp->output);
	stream_len = pos - irp_result_pos - 4;	/* Value of OutputBufferLength */
	Stream_SetPosition(irp->output, irp_result_pos);
	Stream_Write_UINT32(irp->output, stream_len);

	Stream_SetPosition(irp->output, output_len_pos);
	/* Remove the effect of the MS-RPCE Common Type Header and Private
	 * Header (Sections 2.2.6.1 and 2.2.6.2).
	 */
	Stream_Write_UINT32(irp->output, stream_len - header_lengths);

	Stream_SetPosition(irp->output, result_pos);
	Stream_Write_UINT32(irp->output, result);

	Stream_SetPosition(irp->output, pos);

	irp->IoStatus = 0;

	irp->Complete(irp);

}
