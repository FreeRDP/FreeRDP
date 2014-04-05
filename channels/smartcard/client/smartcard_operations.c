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

const char* smartcard_get_ioctl_string(UINT32 ioControlCode)
{
	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			return "SCARD_IOCTL_ESTABLISHCONTEXT";
		case SCARD_IOCTL_RELEASECONTEXT:
			return "SCARD_IOCTL_RELEASECONTEXT";
		case SCARD_IOCTL_ISVALIDCONTEXT:
			return "SCARD_IOCTL_ISVALIDCONTEXT";
		case SCARD_IOCTL_LISTREADERGROUPSA:
			return "SCARD_IOCTL_LISTREADERGROUPSA";
		case SCARD_IOCTL_LISTREADERGROUPSW:
			return "SCARD_IOCTL_LISTREADERGROUPSW";
		case SCARD_IOCTL_LISTREADERSA:
			return "SCARD_IOCTL_LISTREADERSA";
		case SCARD_IOCTL_LISTREADERSW:
			return "SCARD_IOCTL_LISTREADERSW";
		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			return "SCARD_IOCTL_INTRODUCEREADERGROUPA";
		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			return "SCARD_IOCTL_INTRODUCEREADERGROUPW";
		case SCARD_IOCTL_FORGETREADERGROUPA:
			return "SCARD_IOCTL_FORGETREADERGROUPA";
		case SCARD_IOCTL_FORGETREADERGROUPW:
			return "SCARD_IOCTL_FORGETREADERGROUPW";
		case SCARD_IOCTL_INTRODUCEREADERA:
			return "SCARD_IOCTL_INTRODUCEREADERA";
		case SCARD_IOCTL_INTRODUCEREADERW:
			return "SCARD_IOCTL_INTRODUCEREADERW";
		case SCARD_IOCTL_FORGETREADERA:
			return "SCARD_IOCTL_FORGETREADERA";
		case SCARD_IOCTL_FORGETREADERW:
			return "SCARD_IOCTL_FORGETREADERW";
		case SCARD_IOCTL_ADDREADERTOGROUPA:
			return "SCARD_IOCTL_ADDREADERTOGROUPA";
		case SCARD_IOCTL_ADDREADERTOGROUPW:
			return "SCARD_IOCTL_ADDREADERTOGROUPW";
		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			return "SCARD_IOCTL_REMOVEREADERFROMGROUPA";
		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			return "SCARD_IOCTL_REMOVEREADERFROMGROUPW";
		case SCARD_IOCTL_LOCATECARDSA:
			return "SCARD_IOCTL_LOCATECARDSA";
		case SCARD_IOCTL_LOCATECARDSW:
			return "SCARD_IOCTL_LOCATECARDSW";
		case SCARD_IOCTL_GETSTATUSCHANGEA:
			return "SCARD_IOCTL_GETSTATUSCHANGEA";
		case SCARD_IOCTL_GETSTATUSCHANGEW:
			return "SCARD_IOCTL_GETSTATUSCHANGEW";
		case SCARD_IOCTL_CANCEL:
			return "SCARD_IOCTL_CANCEL";
		case SCARD_IOCTL_CONNECTA:
			return "SCARD_IOCTL_CONNECTA";
		case SCARD_IOCTL_CONNECTW:
			return "SCARD_IOCTL_CONNECTW";
		case SCARD_IOCTL_RECONNECT:
			return "SCARD_IOCTL_RECONNECT";
		case SCARD_IOCTL_DISCONNECT:
			return "SCARD_IOCTL_DISCONNECT";
		case SCARD_IOCTL_BEGINTRANSACTION:
			return "SCARD_IOCTL_BEGINTRANSACTION";
		case SCARD_IOCTL_ENDTRANSACTION:
			return "SCARD_IOCTL_ENDTRANSACTION";
		case SCARD_IOCTL_STATE:
			return "SCARD_IOCTL_STATE";
		case SCARD_IOCTL_STATUSA:
			return "SCARD_IOCTL_STATUSA";
		case SCARD_IOCTL_STATUSW:
			return "SCARD_IOCTL_STATUSW";
		case SCARD_IOCTL_TRANSMIT:
			return "SCARD_IOCTL_TRANSMIT";
		case SCARD_IOCTL_CONTROL:
			return "SCARD_IOCTL_CONTROL";
		case SCARD_IOCTL_GETATTRIB:
			return "SCARD_IOCTL_GETATTRIB";
		case SCARD_IOCTL_SETATTRIB:
			return "SCARD_IOCTL_SETATTRIB";
		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			return "SCARD_IOCTL_ACCESSSTARTEDEVENT";
		case SCARD_IOCTL_LOCATECARDSBYATRA:
			return "SCARD_IOCTL_LOCATECARDSBYATRA";
		case SCARD_IOCTL_LOCATECARDSBYATRW:
			return "SCARD_IOCTL_LOCATECARDSBYATRW";
		case SCARD_IOCTL_READCACHEA:
			return "SCARD_IOCTL_READCACHEA";
		case SCARD_IOCTL_READCACHEW:
			return "SCARD_IOCTL_READCACHEW";
		case SCARD_IOCTL_WRITECACHEA:
			return "SCARD_IOCTL_WRITECACHEA";
		case SCARD_IOCTL_WRITECACHEW:
			return "SCARD_IOCTL_WRITECACHEW";
		case SCARD_IOCTL_GETTRANSMITCOUNT:
			return "SCARD_IOCTL_GETTRANSMITCOUNT";
		case SCARD_IOCTL_RELEASESTARTEDEVENT:
			return "SCARD_IOCTL_RELEASESTARTEDEVENT";
		case SCARD_IOCTL_GETREADERICON:
			return "SCARD_IOCTL_GETREADERICON";
		case SCARD_IOCTL_GETDEVICETYPEID:
			return "SCARD_IOCTL_GETDEVICETYPEID";
		default:
			return "SCARD_IOCTL_UNKNOWN";
	}

	return "SCARD_IOCTL_UNKNOWN";
}

static UINT32 handle_Context(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 length;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Context is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, length); /* Length (4 bytes) */

	if ((Stream_GetRemainingLength(irp->input) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Context is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(irp->input), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, length);

	if (length > Stream_GetRemainingLength(irp->input))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Context is too long: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(irp->input), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
}

static UINT32 handle_CardHandle(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	UINT32 length;

	status = handle_Context(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "CardHandle is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, length); /* Length (4 bytes) */

	if ((Stream_GetRemainingLength(irp->input) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "CardHandle is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(irp->input), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, length); /* Length (4 bytes) */

	return 0;
}

static UINT32 handle_RedirContextRef(SMARTCARD_DEVICE* smartcard, IRP* irp, SCARDCONTEXT* hContext)
{
	UINT32 length;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirContextRef is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(irp->input), 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, length); /* Length (4 bytes) */

	if ((length != 4) && (length != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirContextRef length is not 4 or 8: %d\n", length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if ((Stream_GetRemainingLength(irp->input) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirContextRef is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(irp->input), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (length > 4)
		Stream_Read_UINT64(irp->input, *hContext);
	else
		Stream_Read_UINT32(irp->input, *hContext);

	return 0;
}

static UINT32 handle_RedirHandleRef(SMARTCARD_DEVICE* smartcard, IRP* irp, SCARDCONTEXT* hContext, SCARDHANDLE* hHandle)
{
	UINT32 length;
	UINT32 status;

	status = handle_RedirContextRef(smartcard, irp, hContext);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirHandleRef is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(irp->input), 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, length); /* Length (4 bytes) */

	if ((length != 4) && (length != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirHandleRef length is not 4 or 8: %d\n", length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if ((Stream_GetRemainingLength(irp->input) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "RedirHandleRef is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(irp->input), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (length > 4)
		Stream_Read_UINT64(irp->input, *hHandle);
	else
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

	status = SCardStatusA(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);

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
		UINT32 i;

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
	UINT32 header = (length < 0) ? (0) : ((length > highLimit) ? (highLimit) : (length));

	Stream_Write_UINT32(irp->output, header);

	if (length <= 0)
	{
		Stream_Write_UINT32(irp->output, 0);
	}
	else
	{
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
		UINT32 i;

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

static UINT32 handle_EstablishContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	EstablishContext_Call call;
	SCARDCONTEXT hContext = -1;

	status = smartcard_unpack_establish_context_call(smartcard, irp->input, &call);

	if (status)
		return status;

	status = SCardEstablishContext(call.dwScope, NULL, NULL, &hContext);

	Stream_Write_UINT32(irp->output, 4); /* cbContext (4 bytes) */
	Stream_Write_UINT32(irp->output, -1); /* ReferentID (4 bytes) */

	Stream_Write_UINT32(irp->output, 4);
	Stream_Write_UINT32(irp->output, hContext);

	/* store hContext in allowed context list */
	smartcard->hContext = hContext;

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_ReleaseContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext = -1;

	status = handle_Context(smartcard, irp);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		return status;

	status = SCardReleaseContext(hContext);
	ZeroMemory(&smartcard->hContext, sizeof(smartcard->hContext));

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_IsValidContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext;

	status = handle_Context(smartcard, irp);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		return status;

	status = SCardIsValidContext(hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_ListReaders(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	UINT32 status;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	ListReaders_Call call;
	char *readerList = NULL, *walker;
	int elemLength, dataLength;
	int pos, poslen1, poslen2, allowed_pos;

	status = handle_Context(smartcard, irp);

	if (status)
		goto finish;

	status = smartcard_unpack_list_readers_call(smartcard, irp->input, &call);

	if (status)
		goto finish;

	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		goto finish;

	dwReaders = SCARD_AUTOALLOCATE;
	status = SCardListReadersA(hContext, (LPCSTR) call.mszGroups, (LPSTR) &readerList, &dwReaders);

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

	if (call.mszGroups)
		free(call.mszGroups);

	return status;
}

static UINT32 handle_GetStatusChange(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	UINT32 i;
	LONG status;
	SCARDCONTEXT hContext;
	GetStatusChangeA_Call call;
	ReaderStateA* readerState = NULL;
	LPSCARD_READERSTATEA rgReaderState = NULL;
	LPSCARD_READERSTATEA rgReaderStates = NULL;

	status = handle_Context(smartcard, irp);

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

	Stream_Read_UINT32(irp->input, call.dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(irp->input, call.cReaders); /* cReaders (4 bytes) */
	Stream_Seek_UINT32(irp->input); /* rgReaderStatesNdrPtr (4 bytes) */

	/* Get context */
	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		goto finish;

	/* Skip ReaderStateConformant */
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}
	Stream_Seek(irp->input, 4);

	if (call.cReaders > 0)
	{
		call.rgReaderStates = (ReaderStateA*) calloc(1, call.cReaders * sizeof(ReaderStateA));

		for (i = 0; i < call.cReaders; i++)
		{
			readerState = &call.rgReaderStates[i];

			if (Stream_GetRemainingLength(irp->input) < 52 )
			{
				DEBUG_WARN("length violation %d [%d]", 52,
					Stream_GetRemainingLength(irp->input));
				status = SCARD_F_INTERNAL_ERROR;
				goto finish;
			}

			Stream_Seek(irp->input, 4);

			Stream_Read_UINT32(irp->input, readerState->Common.dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(irp->input, readerState->Common.dwEventState); /* dwEventState (4 bytes) */
			Stream_Read_UINT32(irp->input, readerState->Common.cbAtr); /* cbAtr (4 bytes) */
			Stream_Read(irp->input, readerState->Common.rgbAtr, 32);
			Stream_Seek(irp->input, 4);

			/* reset high bytes? */
			readerState->Common.dwCurrentState &= 0x0000FFFF;
			readerState->Common.dwEventState = 0;
		}

		for (i = 0; i < call.cReaders; i++)
		{
			UINT32 dataLength;

			readerState = &call.rgReaderStates[i];

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
						(char**) &readerState->szReader, dataLength, wide));

			if (!readerState->szReader)
			{
				DEBUG_WARN("cur->szReader=%p", readerState->szReader);
				continue;
			}

			if (strcmp((char*) readerState->szReader, "\\\\?PnP?\\Notification") == 0)
				readerState->Common.dwCurrentState |= SCARD_STATE_IGNORE;
		}
	}
	else
	{
		call.rgReaderStates = NULL;
	}

	rgReaderStates = (SCARD_READERSTATEA*) calloc(1, call.cReaders * sizeof(SCARD_READERSTATEA));

	for (i = 0; i < call.cReaders; i++)
	{
		rgReaderStates[i].szReader = (LPCSTR) call.rgReaderStates[i].szReader;
		rgReaderStates[i].dwCurrentState = call.rgReaderStates[i].Common.dwCurrentState;
		rgReaderStates[i].dwEventState = call.rgReaderStates[i].Common.dwEventState;
		rgReaderStates[i].cbAtr = call.rgReaderStates[i].Common.cbAtr;
		CopyMemory(&(rgReaderStates[i].rgbAtr), &(call.rgReaderStates[i].Common.rgbAtr), 36);
	}

	status = SCardGetStatusChangeA(hContext, (DWORD) call.dwTimeOut, rgReaderStates, (DWORD) call.cReaders);

	Stream_Write_UINT32(irp->output, call.cReaders);
	Stream_Write_UINT32(irp->output, 0x00084dd8);
	Stream_Write_UINT32(irp->output, call.cReaders);

	for (i = 0; i < call.cReaders; i++)
	{
		rgReaderState = &rgReaderStates[i];

		Stream_Write_UINT32(irp->output, rgReaderState->dwCurrentState);
		Stream_Write_UINT32(irp->output, rgReaderState->dwEventState);
		Stream_Write_UINT32(irp->output, rgReaderState->cbAtr);
		Stream_Write(irp->output, rgReaderState->rgbAtr, 32);
		Stream_Zero(irp->output, 4);
	}

	smartcard_output_alignment(irp, 8);

finish:
	if (call.rgReaderStates)
	{
		for (i = 0; i < call.cReaders; i++)
		{
			readerState = &call.rgReaderStates[i];

			if (readerState->szReader)
				free((void*) readerState->szReader);
			readerState->szReader = NULL;
		}
		free(call.rgReaderStates);
		free(rgReaderStates);
	}

	return status;
}

static UINT32 handle_Cancel(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;

	status = handle_Context(smartcard, irp);

	if (status)
		return status;

	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		return status;

	status = SCardCancel(hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

UINT32 handle_ConnectA(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	ConnectA_Call call;
	Connect_Return ret;

	call.szReader = NULL;
	call.Common.Context.pbContext = (BYTE*) &hContext;

	status = smartcard_unpack_connect_a_call(smartcard, irp->input, &call);

	if (status)
		goto finish;

	if (!check_reader_is_forwarded(smartcard, (char*) call.szReader))
	{
		DEBUG_WARN("Reader '%s' not forwarded!", call.szReader);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	status = SCardConnectA(hContext, (char*) call.szReader, (DWORD) call.Common.dwShareMode,
		(DWORD) call.Common.dwPreferredProtocols, &hCard, (DWORD*) &ret.dwActiveProtocol);

	smartcard->hCard = hCard;

	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, 0x016Cff34);
	Stream_Write_UINT32(irp->output, ret.dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, hCard);

	smartcard_output_alignment(irp, 8);

finish:
	if (call.szReader)
		free(call.szReader);

	return status;
}

UINT32 handle_ConnectW(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	ConnectW_Call call;
	Connect_Return ret;

	call.szReader = NULL;
	call.Common.Context.pbContext = (BYTE*) &hContext;

	status = smartcard_unpack_connect_w_call(smartcard, irp->input, &call);

	if (status)
		goto finish;

	status = SCardConnectW(hContext, (WCHAR*) call.szReader, (DWORD) call.Common.dwShareMode,
		(DWORD) call.Common.dwPreferredProtocols, &hCard, (DWORD*) &ret.dwActiveProtocol);

	smartcard->hCard = hCard;

	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000000);
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, 0x016Cff34);
	Stream_Write_UINT32(irp->output, ret.dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	Stream_Write_UINT32(irp->output, 0x00000004);
	Stream_Write_UINT32(irp->output, hCard);

	smartcard_output_alignment(irp, 8);

finish:
	if (call.szReader)
		free(call.szReader);

	return status;
}

static UINT32 handle_Reconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	Reconnect_Call call;
	Reconnect_Return ret;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call.dwShareMode); /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(irp->input, call.dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	Stream_Read_UINT32(irp->input, call.dwInitialization); /* dwInitialization (4 bytes) */

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardReconnect(hCard, (DWORD) call.dwShareMode, (DWORD) call.dwPreferredProtocols,
			(DWORD) call.dwInitialization, (LPDWORD) &ret.dwActiveProtocol);

	Stream_Write_UINT32(irp->output, ret.dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_Disconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	HCardAndDisposition_Call call;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call.dwDisposition); /* dwDisposition (4 bytes) */
	
	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardDisconnect(hCard, (DWORD) call.dwDisposition);

	ZeroMemory(&smartcard->hCard, sizeof(smartcard->hCard));

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_BeginTransaction(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	HCardAndDisposition_Call call;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call.dwDisposition); /* dwDisposition (4 bytes) */

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

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
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	HCardAndDisposition_Call call;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
			Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call.dwDisposition); /* dwDisposition (4 bytes) */

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	status = SCardEndTransaction(hCard, call.dwDisposition);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_State(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	State_Call call;
	State_Return ret;
	DWORD readerLen;
	char* readerName = NULL;
	BYTE atr[SCARD_ATR_LENGTH];

	status = handle_CardHandle(smartcard, irp);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 8)
	{
		DEBUG_WARN("length violation %d [%d]", 8,
			Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	Stream_Read_UINT32(irp->input, call.fpbAtrIsNULL); /* fpbAtrIsNULL (4 bytes) */
	Stream_Read_UINT32(irp->input, call.cbAtrLen); /* cbAtrLen (4 bytes) */

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		goto finish;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	readerLen = SCARD_AUTOALLOCATE;

	ret.rgAtr = atr;
	ret.cbAtrLen = SCARD_ATR_LENGTH;

	status = SCardStatusA(hCard, (LPSTR) &readerName, &readerLen,
			&ret.dwState, &ret.dwProtocol, ret.rgAtr, &ret.cbAtrLen);

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}

	Stream_Write_UINT32(irp->output, ret.dwState); /* dwState (4 bytes) */
	Stream_Write_UINT32(irp->output, ret.dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Write_UINT32(irp->output, ret.cbAtrLen); /* cbAtrLen (4 bytes) */
	Stream_Write_UINT32(irp->output, 0x00000001); /* rgAtrPointer (4 bytes) */
	Stream_Write_UINT32(irp->output, ret.cbAtrLen); /* rgAtrLength (4 bytes) */
	Stream_Write(irp->output, ret.rgAtr, ret.cbAtrLen); /* rgAtr */

	smartcard_output_repos(irp, ret.cbAtrLen);
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

	status = handle_CardHandle(smartcard, irp);

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
	
	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

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
	status = SCardStatusA(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);

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

	status = handle_CardHandle(smartcard, irp);

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

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

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
	LONG status;
	UINT32 length;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	UINT32 controlFunction;
	Control_Call call;
	Control_Return ret;
	UINT32 pvInBufferPointer;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		goto finish;

	if (Stream_GetRemainingLength(irp->input) < 20)
	{
		DEBUG_WARN("length violation %d [%d]", 20,
				Stream_GetRemainingLength(irp->input));
		status = SCARD_F_INTERNAL_ERROR;
		goto finish;
	}

	call.pvInBuffer = NULL;
	Stream_Read_UINT32(irp->input, call.dwControlCode); /* dwControlCode (4 bytes) */
	Stream_Read_UINT32(irp->input, call.cbInBufferSize); /* cbInBufferSize (4 bytes) */
	Stream_Read_UINT32(irp->input, pvInBufferPointer); /* pvInBufferPointer (4 bytes) */
	Stream_Read_UINT32(irp->input, call.fpvOutBufferIsNULL); /* fpvOutBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(irp->input, call.cbOutBufferSize); /* cbOutBufferSize (4 bytes) */

	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		goto finish;

	/* Translate Windows SCARD_CTL_CODE's to corresponding local code */
	if (DEVICE_TYPE_FROM_CTL_CODE(call.dwControlCode) == FILE_DEVICE_SMARTCARD)
	{
		controlFunction = FUNCTION_FROM_CTL_CODE(call.dwControlCode);
		call.dwControlCode = SCARD_CTL_CODE(controlFunction);
	}

	if (pvInBufferPointer)
	{
		/* Get the size of the linked data. */
		if (Stream_GetRemainingLength(irp->input) < 4)
		{
			DEBUG_WARN("length violation %d [%d]", 4,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}

		Stream_Read_UINT32(irp->input, length); /* Length (4 bytes) */

		/* Check, if there is actually enough data... */
		if (Stream_GetRemainingLength(irp->input) < length)
		{
			DEBUG_WARN("length violation %d [%d]", length,
					Stream_GetRemainingLength(irp->input));
			status = SCARD_F_INTERNAL_ERROR;
			goto finish;
		}

		call.pvInBuffer = malloc(length);
		call.cbInBufferSize = length;

		Stream_Read(irp->input, call.pvInBuffer, length);
	}

	ret.cbOutBufferSize = call.cbOutBufferSize;
	ret.pvOutBuffer = malloc(call.cbOutBufferSize);

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		status = SCARD_E_INVALID_TARGET;
		goto finish;
	}

	status = SCardControl(hCard, (DWORD) call.dwControlCode,
			call.pvInBuffer, (DWORD) call.cbInBufferSize,
			ret.pvOutBuffer, (DWORD) call.cbOutBufferSize, &ret.cbOutBufferSize);

	Stream_Write_UINT32(irp->output, (UINT32) ret.cbOutBufferSize); /* cbOutBufferSize (4 bytes) */
	Stream_Write_UINT32(irp->output, 0x00000004); /* pvOutBufferPointer (4 bytes) */
	Stream_Write_UINT32(irp->output, ret.cbOutBufferSize); /* pvOutBufferLength (4 bytes) */

	if (ret.cbOutBufferSize > 0)
	{
		Stream_Write(irp->output, ret.pvOutBuffer, ret.cbOutBufferSize); /* pvOutBuffer */
		smartcard_output_repos(irp, ret.cbOutBufferSize);
	}

	smartcard_output_alignment(irp, 8);

finish:
	if (call.pvInBuffer)
		free(call.pvInBuffer);
	if (ret.pvOutBuffer)
		free(ret.pvOutBuffer);

	return status;
}

static UINT32 handle_GetAttrib(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	DWORD cbAttrLen;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	GetAttrib_Call call;
	GetAttrib_Return ret;

	status = handle_CardHandle(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 12)
	{
		DEBUG_WARN("length violation %d [%d]", 12,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call.dwAttrId); /* dwAttrId (4 bytes) */
	Stream_Read_UINT32(irp->input, call.fpbAttrIsNULL); /* fpbAttrIsNULL (4 bytes) */
	Stream_Read_UINT32(irp->input, call.cbAttrLen); /* cbAttrLen (4 bytes) */
	
	status = handle_RedirHandleRef(smartcard, irp, &hContext, &hCard);

	if (status)
		return status;

	if (!check_handle_is_forwarded(smartcard, hCard, hContext))
	{
		DEBUG_WARN("invalid handle %p [%p]", hCard, hContext);
		return SCARD_E_INVALID_TARGET;
	}

	ret.pbAttr = NULL;

	cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;

	status = SCardGetAttrib(hCard, call.dwAttrId, (cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

	if (status != SCARD_S_SUCCESS)
	{
		cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
	}

	if (call.dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
				(cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

		if (status != SCARD_S_SUCCESS)
		{
			cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
		}
	}

	if (call.dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
				(cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

		if (status != SCARD_S_SUCCESS)
		{
			cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
		}
	}

	if ((cbAttrLen > call.cbAttrLen) && (ret.pbAttr != NULL))
	{
		status = SCARD_E_INSUFFICIENT_BUFFER;
	}
	call.cbAttrLen = cbAttrLen;

	if (status != SCARD_S_SUCCESS)
	{
		status = smartcard_output_return(irp, status);
		goto finish;
	}
	else
	{
		ret.cbAttrLen = call.cbAttrLen;

		Stream_Write_UINT32(irp->output, ret.cbAttrLen); /* cbAttrLen (4 bytes) */
		Stream_Write_UINT32(irp->output, 0x00000200); /* pbAttrPointer (4 bytes) */
		Stream_Write_UINT32(irp->output, ret.cbAttrLen); /* pbAttrLength (4 bytes) */

		if (!ret.pbAttr)
			Stream_Zero(irp->output, ret.cbAttrLen); /* pbAttr */
		else
			Stream_Write(irp->output, ret.pbAttr, ret.cbAttrLen); /* pbAttr */

		smartcard_output_repos(irp, ret.cbAttrLen);
		/* align to multiple of 4 */
		Stream_Write_UINT32(irp->output, 0);
	}
	smartcard_output_alignment(irp, 8);

finish:
	SCardFreeMemory(hContext, ret.pbAttr);

	return status;
}

static UINT32 handle_AccessStartedEvent(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4, Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 4); /* Unused (4 bytes) */

	smartcard_output_alignment(irp, 8);
	
	return SCARD_S_SUCCESS;
}

static UINT32 handle_LocateCardsByATR(SMARTCARD_DEVICE* smartcard, IRP* irp, BOOL wide)
{
	LONG status;
	UINT32 i, j, k;
	SCARDCONTEXT hContext;
	UINT32 atrMaskCount = 0;
	UINT32 readerCount = 0;
	SCARD_READERSTATEA* cur = NULL;
	SCARD_READERSTATEA* rsCur = NULL;
	SCARD_READERSTATEA* readerStates = NULL;
	SCARD_ATRMASK* curAtr = NULL;
	SCARD_ATRMASK* pAtrMasks = NULL;

	status = handle_Context(smartcard, irp);

	if (status)
		return status;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		DEBUG_WARN("length violation %d [%d]", 4,
				Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 4);
	status = handle_RedirContextRef(smartcard, irp, &hContext);

	if (status)
		return status;

	Stream_Seek(irp->input, 44);
	Stream_Read_UINT32(irp->input, hContext);
	Stream_Read_UINT32(irp->input, atrMaskCount);

	pAtrMasks = calloc(atrMaskCount, sizeof(SCARD_ATRMASK));

	if (!pAtrMasks)
		return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

	for (i = 0; i < atrMaskCount; i++)
	{
		Stream_Read_UINT32(irp->input, pAtrMasks[i].cbAtr);
		Stream_Read(irp->input, pAtrMasks[i].rgbAtr, 36);
		Stream_Read(irp->input, pAtrMasks[i].rgbMask, 36);
	}

	Stream_Read_UINT32(irp->input, readerCount);

	readerStates = calloc(readerCount, sizeof(SCARD_READERSTATE));

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];

		Stream_Seek(irp->input, 4);

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
		UINT32 dataLength;

		cur = &readerStates[i];

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

	status = SCardGetStatusChangeA(hContext, 0x00000001, readerStates, readerCount);

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
				rsCur->dwEventState |= SCARD_STATE_ATRMATCH;
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

void smartcard_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 result;
	UINT32 status;
	UINT32 ioControlCode;
	UINT32 outputBufferLength;
	UINT32 inputBufferLength;
	UINT32 objectBufferLength;

	/* Device Control Request */

	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Device Control Request is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return;
	}

	Stream_Read_UINT32(irp->input, outputBufferLength); /* OutputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, inputBufferLength); /* InputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, ioControlCode); /* IoControlCode (4 bytes) */
	Stream_Seek(irp->input, 20); /* Padding (20 bytes) */

	if (Stream_Length(irp->input) != (Stream_GetPosition(irp->input) + inputBufferLength))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
				"InputBufferLength mismatch: Actual: %d Expected: %d\n",
				Stream_Length(irp->input), Stream_GetPosition(irp->input) + inputBufferLength);
		return;
	}

	WLog_Print(smartcard->log, WLOG_WARN, "ioControlCode: %s (0x%08X)",
			smartcard_get_ioctl_string(ioControlCode), ioControlCode);

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		status = smartcard_unpack_common_type_header(smartcard, irp->input);

		if (status)
			return;

		status = smartcard_unpack_private_type_header(smartcard, irp->input);

		if (status)
			return;
	}

	/**
	 * [MS-RDPESC] 3.2.5.1: Sending Outgoing Messages:
	 * the output buffer length SHOULD be set to 2048
	 *
	 * Since it's a SHOULD and not a MUST, we don't care
	 * about it, but we still reserve at least 2048 bytes.
	 */
	Stream_EnsureRemainingCapacity(irp->output, 2048);

	/* Device Control Response */
	Stream_Seek_UINT32(irp->output); /* OutputBufferLength (4 bytes) */

	Stream_Seek(irp->output, SMARTCARD_COMMON_TYPE_HEADER_LENGTH); /* CommonTypeHeader (8 bytes) */
	Stream_Seek(irp->output, SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH); /* PrivateTypeHeader (8 bytes) */

	Stream_Seek_UINT32(irp->output); /* Result (4 bytes) */

	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			result = handle_EstablishContext(smartcard, irp);
			break;

		case SCARD_IOCTL_RELEASECONTEXT:
			result = handle_ReleaseContext(smartcard, irp);
			break;

		case SCARD_IOCTL_ISVALIDCONTEXT:
			result = handle_IsValidContext(smartcard, irp);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERGROUPSW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERSA:
			result = handle_ListReaders(smartcard, irp, 0);
			break;

		case SCARD_IOCTL_LISTREADERSW:
			result = handle_ListReaders(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEA:
			result = handle_GetStatusChange(smartcard, irp, 0);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			result = handle_GetStatusChange(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_CANCEL:
			result = handle_Cancel(smartcard, irp);
			break;

		case SCARD_IOCTL_CONNECTA:
			result = handle_ConnectA(smartcard, irp);
			break;

		case SCARD_IOCTL_CONNECTW:
			result = handle_ConnectW(smartcard, irp);
			break;

		case SCARD_IOCTL_RECONNECT:
			result = handle_Reconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_DISCONNECT:
			result = handle_Disconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_BEGINTRANSACTION:
			result = handle_BeginTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_ENDTRANSACTION:
			result = handle_EndTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_STATE:
			result = handle_State(smartcard, irp);
			break;

		case SCARD_IOCTL_STATUSA:
			result = handle_Status(smartcard, irp, 0);
			break;

		case SCARD_IOCTL_STATUSW:
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

		case SCARD_IOCTL_SETATTRIB:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			result = handle_AccessStartedEvent(smartcard, irp);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			result = handle_LocateCardsByATR(smartcard, irp, 0);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRW:
			result = handle_LocateCardsByATR(smartcard, irp, 1);
			break;

		case SCARD_IOCTL_READCACHEA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_READCACHEW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETTRANSMITCOUNT:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_RELEASESTARTEDEVENT:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETREADERICON:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETDEVICETYPEID:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		default:
			result = STATUS_UNSUCCESSFUL;
			break;
	}

	if ((result != SCARD_S_SUCCESS) && (result != SCARD_E_TIMEOUT))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
			"IRP failure: ioControlCode: %s (0x%08X), status: %s (0x%08X)",
			smartcard_get_ioctl_string(ioControlCode), ioControlCode,
			SCardGetErrorString(result), result);
	}

	Stream_SealLength(irp->output);
	outputBufferLength = Stream_Length(irp->output) - RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4;
	objectBufferLength = outputBufferLength - RDPDR_DEVICE_IO_RESPONSE_LENGTH;
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH);

	/* Device Control Response */
	Stream_Write_UINT32(irp->output, outputBufferLength); /* OutputBufferLength (4 bytes) */

	smartcard_pack_common_type_header(smartcard, irp->output); /* CommonTypeHeader (8 bytes) */
	smartcard_pack_private_type_header(smartcard, irp->output, objectBufferLength); /* PrivateTypeHeader (8 bytes) */

	Stream_Write_UINT32(irp->output, result); /* Result (4 bytes) */

	Stream_SetPosition(irp->output, Stream_Length(irp->output));

	irp->IoStatus = 0;
	irp->Complete(irp);
}
