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
#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <semaphore.h>

#define BOOL PCSC_BOOL
#include <PCSC/pcsclite.h>
#include <PCSC/reader.h>
#include <PCSC/winscard.h>
#undef BOOL

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/svc_plugin.h>

#include "smartcard_main.h"

/* [MS-RDPESC] 3.1.4 */
#define SCARD_IOCTL_ESTABLISH_CONTEXT		0x00090014	/* EstablishContext */
#define SCARD_IOCTL_RELEASE_CONTEXT		0x00090018	/* ReleaseContext */
#define SCARD_IOCTL_IS_VALID_CONTEXT		0x0009001C	/* IsValidContext */
#define SCARD_IOCTL_LIST_READER_GROUPS		0x00090020	/* ListReaderGroups */
#define SCARD_IOCTL_LIST_READERS		0x00090028	/* ListReadersA */
#define SCARD_IOCTL_INTRODUCE_READER_GROUP	0x00090050	/* IntroduceReaderGroup */
#define SCARD_IOCTL_FORGET_READER_GROUP		0x00090058	/* ForgetReader */
#define SCARD_IOCTL_INTRODUCE_READER		0x00090060	/* IntroduceReader */
#define SCARD_IOCTL_FORGET_READER		0x00090068	/* IntroduceReader */
#define SCARD_IOCTL_ADD_READER_TO_GROUP		0x00090070	/* AddReaderToGroup */
#define SCARD_IOCTL_REMOVE_READER_FROM_GROUP	0x00090078	/* RemoveReaderFromGroup */
#define SCARD_IOCTL_GET_STATUS_CHANGE		0x000900A0	/* GetStatusChangeA */
#define SCARD_IOCTL_CANCEL			0x000900A8	/* Cancel */
#define SCARD_IOCTL_CONNECT			0x000900AC	/* ConnectA */
#define SCARD_IOCTL_RECONNECT			0x000900B4	/* Reconnect */
#define SCARD_IOCTL_DISCONNECT			0x000900B8	/* Disconnect */
#define SCARD_IOCTL_BEGIN_TRANSACTION		0x000900BC	/* BeginTransaction */
#define SCARD_IOCTL_END_TRANSACTION		0x000900C0	/* EndTransaction */
#define SCARD_IOCTL_STATE			0x000900C4	/* State */
#define SCARD_IOCTL_STATUS			0x000900C8	/* StatusA */
#define SCARD_IOCTL_TRANSMIT			0x000900D0	/* Transmit */
#define SCARD_IOCTL_CONTROL			0x000900D4	/* Control */
#define SCARD_IOCTL_GETATTRIB			0x000900D8	/* GetAttrib */
#define SCARD_IOCTL_SETATTRIB			0x000900DC	/* SetAttrib */
#define SCARD_IOCTL_ACCESS_STARTED_EVENT	0x000900E0	/* SCardAccessStartedEvent */
#define SCARD_IOCTL_LOCATE_CARDS_BY_ATR		0x000900E8	/* LocateCardsByATR */

#define SCARD_INPUT_LINKED			0xFFFFFFFF

/* Decode Win CTL_CODE values */
#define WIN_CTL_FUNCTION(ctl_code)		((ctl_code & 0x3FFC) >> 2)
#define WIN_CTL_DEVICE_TYPE(ctl_code)		(ctl_code >> 16)

#define WIN_FILE_DEVICE_SMARTCARD		0x00000031


static UINT32 smartcard_output_string(IRP* irp, char* src, BOOL wide)
{
	BYTE* p;
	UINT32 len;

	p = stream_get_tail(irp->output);
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

	stream_seek(irp->output, len);
	return len;
}

static void smartcard_output_alignment(IRP* irp, UINT32 seed)
{
	const UINT32 field_lengths = 20;/* Remove the lengths of the fields
					 * RDPDR_HEADER, DeviceID,
					 * CompletionID, and IoStatus
					 * of Section 2.2.1.5.5 of MS-RDPEFS.
					 */
	UINT32 size = stream_get_length(irp->output) - field_lengths;
	UINT32 add = (seed - (size % seed)) % seed;

	if (add > 0)
		stream_write_zero(irp->output, add);
}

static void smartcard_output_repos(IRP* irp, UINT32 written)
{
	UINT32 add = (4 - (written % 4)) % 4;

	if (add > 0)
		stream_write_zero(irp->output, add);
}

static UINT32 smartcard_output_return(IRP* irp, UINT32 status)
{
	stream_write_zero(irp->output, 256);
	return status;
}

static void smartcard_output_buffer_limit(IRP* irp, char* buffer, unsigned int length, unsigned int highLimit)
{
	int header = (length < 0) ? (0) : ((length > highLimit) ? (highLimit) : (length));

	stream_write_UINT32(irp->output, header);

	if (length <= 0)
	{
		stream_write_UINT32(irp->output, 0);
	}
	else
	{
		if (header < length)
			length = header;

		stream_write(irp->output, buffer, length);
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

	stream_write_UINT32(irp->output, header);
	stream_write_UINT32(irp->output, 0x00000001);	/* Magic DWORD - any non zero */
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

	stream_read(irp->input, buffer, bufferSize);

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
		stream_seek(irp->input, add);
}

static void smartcard_input_reader_name(IRP* irp, char** dest, BOOL wide)
{
	UINT32 dataLength;

	stream_seek(irp->input, 8);
	stream_read_UINT32(irp->input, dataLength);

	DEBUG_SCARD("datalength %d", dataLength);
	smartcard_input_repos(irp, smartcard_input_string(irp, dest, dataLength, wide));
}

static void smartcard_input_skip_linked(IRP* irp)
{
	UINT32 len;
	stream_read_UINT32(irp->input, len);

	if (len > 0)
	{
		stream_seek(irp->input, len);
		smartcard_input_repos(irp, len);
	}
}

static UINT32 smartcard_map_state(UINT32 state)
{
	/* is this mapping still needed? */

	if (state & SCARD_SPECIFIC)
		state = 0x00000006;
	else if (state & SCARD_NEGOTIABLE)
		state = 0x00000006;
	else if (state & SCARD_POWERED)
		state = 0x00000004;
	else if (state & SCARD_SWALLOWED)
		state = 0x00000003;
	else if (state & SCARD_PRESENT)
		state = 0x00000002;
	else if (state & SCARD_ABSENT)
		state = 0x00000001;
	else
		state = 0x00000000;

	return state;
}

static UINT32 handle_EstablishContext(IRP* irp)
{
	UINT32 len;
	UINT32 status;
	UINT32 scope;
	SCARDCONTEXT hContext = -1;

	stream_seek(irp->input, 8);
	stream_read_UINT32(irp->input, len);

	if (len != 8)
		return SCARD_F_INTERNAL_ERROR;

	stream_seek_UINT32(irp->input);
	stream_read_UINT32(irp->input, scope);

	status = SCardEstablishContext(scope, NULL, NULL, &hContext);

	stream_write_UINT32(irp->output, 4);	// cbContext
	stream_write_UINT32(irp->output, -1);	// ReferentID

	stream_write_UINT32(irp->output, 4);
	stream_write_UINT32(irp->output, hContext);

	/* TODO: store hContext in allowed context list */

	smartcard_output_alignment(irp, 8);
	return SCARD_S_SUCCESS;
}

static UINT32 handle_ReleaseContext(IRP* irp)
{
	UINT32 len, status;
	SCARDCONTEXT hContext = -1;

	stream_seek(irp->input, 8);
	stream_read_UINT32(irp->input, len);

	stream_seek(irp->input, 0x10);
	stream_read_UINT32(irp->input, hContext);

	status = SCardReleaseContext(hContext);

	if (status)
		DEBUG_SCARD("%s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("success 0x%08lx", hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_IsValidContext(IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext;

	stream_seek(irp->input, 0x1C);
	stream_read_UINT32(irp->input, hContext);

	status = SCardIsValidContext(hContext);

	if (status)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success context: 0x%08x", (unsigned) hContext);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_ListReaders(IRP* irp, BOOL wide)
{
	UINT32 len, status;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	char *readerList = NULL, *walker;
	int elemLength, dataLength;
	int pos, poslen1, poslen2;

	stream_seek(irp->input, 8);
	stream_read_UINT32(irp->input, len);

	stream_seek(irp->input, 0x1c);
	stream_read_UINT32(irp->input, len);

	if (len != 4)
		return SCARD_F_INTERNAL_ERROR;

	stream_read_UINT32(irp->input, hContext);

	/* ignore rest of [MS-RDPESC] 2.2.2.4 ListReaders_Call */

	status = SCARD_S_SUCCESS;
#ifdef SCARD_AUTOALLOCATE
	dwReaders = SCARD_AUTOALLOCATE;
	status = SCardListReaders(hContext, NULL, (LPSTR) &readerList, &dwReaders);
#else
	status = SCardListReaders(hContext, NULL, NULL, &dwReaders);

	readerList = malloc(dwReaders);
	status = SCardListReaders(hContext, NULL, readerList, &dwReaders);
#endif
	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
		return status;
	}

/*	DEBUG_SCARD("Success 0x%08x %d %d", (unsigned) hContext, (unsigned) cchReaders, (int) strlen(readerList));*/

	poslen1 = stream_get_pos(irp->output);
	stream_seek_UINT32(irp->output);

	stream_write_UINT32(irp->output, 0x01760650);

	poslen2 = stream_get_pos(irp->output);
	stream_seek_UINT32(irp->output);

	walker = readerList;
	dataLength = 0;

	while (1)
	{
		elemLength = strlen(walker);
		if (elemLength == 0)
			break;

		dataLength += smartcard_output_string(irp, walker, wide);
		walker += elemLength + 1;
		elemLength = strlen(walker);
	}

	dataLength += smartcard_output_string(irp, "\0", wide);

	pos = stream_get_pos(irp->output);

	stream_set_pos(irp->output, poslen1);
	stream_write_UINT32(irp->output, dataLength);
	stream_set_pos(irp->output, poslen2);
	stream_write_UINT32(irp->output, dataLength);

	stream_set_pos(irp->output, pos);

	smartcard_output_repos(irp, dataLength);
	smartcard_output_alignment(irp, 8);

#ifdef SCARD_AUTOALLOCATE
	SCardFreeMemory(hContext, readerList);
#else
	free(readerList);
#endif

	return status;
}

static UINT32 handle_GetStatusChange(IRP* irp, BOOL wide)
{
	int i;
	LONG status;
	SCARDCONTEXT hContext;
	DWORD dwTimeout = 0;
	DWORD readerCount = 0;
	SCARD_READERSTATE *readerStates, *cur;

	stream_seek(irp->input, 0x18);
	stream_read_UINT32(irp->input, dwTimeout);
	stream_read_UINT32(irp->input, readerCount);

	stream_seek(irp->input, 8);

	stream_read_UINT32(irp->input, hContext);

	stream_seek(irp->input, 4);

	DEBUG_SCARD("context: 0x%08x, timeout: 0x%08x, count: %d",
		     (unsigned) hContext, (unsigned) dwTimeout, (int) readerCount);

	if (readerCount > 0)
	{
		readerStates = malloc(readerCount * sizeof(SCARD_READERSTATE));
		ZeroMemory(readerStates, readerCount * sizeof(SCARD_READERSTATE));

		if (!readerStates)
			return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

		for (i = 0; i < readerCount; i++)
		{
			cur = &readerStates[i];

			stream_seek(irp->input, 4);

			/*
			 * TODO: on-wire is little endian; need to either
			 * convert to host endian or fix the headers to
			 * request the order we want
			 */
			stream_read_UINT32(irp->input, cur->dwCurrentState);
			stream_read_UINT32(irp->input, cur->dwEventState);
			stream_read_UINT32(irp->input, cur->cbAtr);
			stream_read(irp->input, cur->rgbAtr, 32);

			stream_seek(irp->input, 4);

			/* reset high bytes? */
			cur->dwCurrentState &= 0x0000FFFF;
			cur->dwEventState = 0;
		}

		for (i = 0; i < readerCount; i++)
		{
			cur = &readerStates[i];
			UINT32 dataLength;

			stream_seek(irp->input, 8);
			stream_read_UINT32(irp->input, dataLength);
			smartcard_input_repos(irp, smartcard_input_string(irp, (char **) &cur->szReader, dataLength, wide));

			DEBUG_SCARD("   \"%s\"", cur->szReader ? cur->szReader : "NULL");
			DEBUG_SCARD("       user: 0x%08x, state: 0x%08x, event: 0x%08x",
				(unsigned) cur->pvUserData, (unsigned) cur->dwCurrentState,
				(unsigned) cur->dwEventState);

			if (strcmp(cur->szReader, "\\\\?PnP?\\Notification") == 0)
				cur->dwCurrentState |= SCARD_STATE_IGNORE;
		}
	}
	else
	{
		readerStates = NULL;
	}

	status = SCardGetStatusChange(hContext, (DWORD) dwTimeout, readerStates, (DWORD) readerCount);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success");

	stream_write_UINT32(irp->output, readerCount);
	stream_write_UINT32(irp->output, 0x00084dd8);
	stream_write_UINT32(irp->output, readerCount);

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];

		DEBUG_SCARD("   \"%s\"", cur->szReader ? cur->szReader : "NULL");
		DEBUG_SCARD("       user: 0x%08x, state: 0x%08x, event: 0x%08x",
			(unsigned) cur->pvUserData, (unsigned) cur->dwCurrentState,
			(unsigned) cur->dwEventState);

		/* TODO: do byte conversions if necessary */
		stream_write_UINT32(irp->output, cur->dwCurrentState);
		stream_write_UINT32(irp->output, cur->dwEventState);
		stream_write_UINT32(irp->output, cur->cbAtr);
		stream_write(irp->output, cur->rgbAtr, 32);

		stream_write_zero(irp->output, 4);

		free((void *)cur->szReader);
	}

	smartcard_output_alignment(irp, 8);

	free(readerStates);
	return status;
}

static UINT32 handle_Cancel(IRP *irp)
{
	LONG status;
	SCARDCONTEXT hContext;

	stream_seek(irp->input, 0x1C);
	stream_read_UINT32(irp->input, hContext);

	status = SCardCancel(hContext);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success context: 0x%08x %s", (unsigned) hContext, pcsc_stringify_error(status));

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_Connect(IRP* irp, BOOL wide)
{
	LONG status;
	SCARDCONTEXT hContext;
	char* readerName = NULL;
	DWORD dwShareMode = 0;
	DWORD dwPreferredProtocol = 0;
	DWORD dwActiveProtocol = 0;
	SCARDHANDLE hCard;

	stream_seek(irp->input, 0x1c);
	stream_read_UINT32(irp->input, dwShareMode);
	stream_read_UINT32(irp->input, dwPreferredProtocol);

	smartcard_input_reader_name(irp, &readerName, wide);

	stream_seek(irp->input, 4);
	stream_read_UINT32(irp->input, hContext);

	DEBUG_SCARD("(context: 0x%08x, share: 0x%08x, proto: 0x%08x, reader: \"%s\")",
		(unsigned) hContext, (unsigned) dwShareMode,
		(unsigned) dwPreferredProtocol, readerName ? readerName : "NULL");

	status = SCardConnect(hContext, readerName, (DWORD) dwShareMode,
		(DWORD) dwPreferredProtocol, &hCard, (DWORD *) &dwActiveProtocol);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s 0x%08x", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success 0x%08x", (unsigned) hCard);

	stream_write_UINT32(irp->output, 0x00000000);
	stream_write_UINT32(irp->output, 0x00000000);
	stream_write_UINT32(irp->output, 0x00000004);
	stream_write_UINT32(irp->output, 0x016Cff34);
	stream_write_UINT32(irp->output, dwActiveProtocol);
	stream_write_UINT32(irp->output, 0x00000004);
	stream_write_UINT32(irp->output, hCard);

	smartcard_output_alignment(irp, 8);

	free(readerName);
	return status;
}

static UINT32 handle_Reconnect(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwShareMode = 0;
	DWORD dwPreferredProtocol = 0;
	DWORD dwInitialization = 0;
	DWORD dwActiveProtocol = 0;

	stream_seek(irp->input, 0x20);
	stream_read_UINT32(irp->input, dwShareMode);
	stream_read_UINT32(irp->input, dwPreferredProtocol);
	stream_read_UINT32(irp->input, dwInitialization);

	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, hContext);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, hCard);

	DEBUG_SCARD("(context: 0x%08x, hcard: 0x%08x, share: 0x%08x, proto: 0x%08x, init: 0x%08x)",
		(unsigned) hContext, (unsigned) hCard,
		(unsigned) dwShareMode, (unsigned) dwPreferredProtocol, (unsigned) dwInitialization);

	status = SCardReconnect(hCard, (DWORD) dwShareMode, (DWORD) dwPreferredProtocol,
	    (DWORD) dwInitialization, (LPDWORD) &dwActiveProtocol);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success (proto: 0x%08x)", (unsigned) dwActiveProtocol);

	stream_write_UINT32(irp->output, dwActiveProtocol);
	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_Disconnect(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwDisposition = 0;

	stream_seek(irp->input, 0x20);
	stream_read_UINT32(irp->input, dwDisposition);
	stream_seek(irp->input, 4);
	stream_read_UINT32(irp->input, hContext);
	stream_seek(irp->input, 4);
	stream_read_UINT32(irp->input, hCard);

	DEBUG_SCARD("(context: 0x%08x, hcard: 0x%08x, disposition: 0x%08x)",
		(unsigned) hContext, (unsigned) hCard, (unsigned) dwDisposition);

	status = SCardDisconnect(hCard, (DWORD) dwDisposition);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success");

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_BeginTransaction(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hCard;

	stream_seek(irp->input, 0x30);
	stream_read_UINT32(irp->input, hCard);

	status = SCardBeginTransaction(hCard);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success hcard: 0x%08x", (unsigned) hCard);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_EndTransaction(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hCard;
	DWORD dwDisposition = 0;

	stream_seek(irp->input, 0x20);
	stream_read_UINT32(irp->input, dwDisposition);

	stream_seek(irp->input, 0x0C);
	stream_read_UINT32(irp->input, hCard);

	status = SCardEndTransaction(hCard, dwDisposition);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success hcard: 0x%08x", (unsigned) hCard);

	smartcard_output_alignment(irp, 8);

	return status;
}

static UINT32 handle_State(IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	DWORD state = 0, protocol = 0;
	DWORD readerLen;
	DWORD atrLen = MAX_ATR_SIZE;
	char* readerName;
	BYTE pbAtr[MAX_ATR_SIZE];

#ifdef WITH_DEBUG_SCARD
	int i;
#endif

	stream_seek(irp->input, 0x24);
	stream_seek_UINT32(irp->input);	/* atrLen */

	stream_seek(irp->input, 0x0c);
	stream_read_UINT32(irp->input, hCard);
	stream_seek(irp->input, 0x04);

#ifdef SCARD_AUTOALLOCATE
	readerLen = SCARD_AUTOALLOCATE;

	status = SCardStatus(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);
#else
	readerLen = 256;
	readerName = malloc(readerLen);

	status = SCardStatus(hCard, (LPSTR) readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);
#endif

	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
		return smartcard_output_return(irp, status);
	}

	DEBUG_SCARD("Success (hcard: 0x%08x len: %d state: 0x%08x, proto: 0x%08x)",
		(unsigned) hCard, (int) atrLen, (unsigned) state, (unsigned) protocol);

#ifdef WITH_DEBUG_SCARD
	fprintf(stderr, "       ATR: ");
	for (i = 0; i < atrLen; i++)
		fprintf(stderr, "%02x%c", pbAtr[i], (i == atrLen - 1) ? ' ' : ':');
	fprintf(stderr, "\n");
#endif

	state = smartcard_map_state(state);

	stream_write_UINT32(irp->output, state);
	stream_write_UINT32(irp->output, protocol);
	stream_write_UINT32(irp->output, atrLen);
	stream_write_UINT32(irp->output, 0x00000001);
	stream_write_UINT32(irp->output, atrLen);
	stream_write(irp->output, pbAtr, atrLen);

	smartcard_output_repos(irp, atrLen);
	smartcard_output_alignment(irp, 8);

#ifdef SCARD_AUTOALLOCATE
	free(readerName);
#else
	free(readerName);
#endif

	return status;
}

static DWORD handle_Status(IRP *irp, BOOL wide)
{
	LONG status;
	SCARDHANDLE hCard;
	DWORD state, protocol;
	DWORD readerLen = 0;
	DWORD atrLen = 0;
	char* readerName;
	BYTE pbAtr[MAX_ATR_SIZE];
	UINT32 dataLength;
	int pos, poslen1, poslen2;

#ifdef WITH_DEBUG_SCARD
	int i;
#endif

	stream_seek(irp->input, 0x24);
	stream_read_UINT32(irp->input, readerLen);
	stream_read_UINT32(irp->input, atrLen);
	stream_seek(irp->input, 0x0c);
	stream_read_UINT32(irp->input, hCard);
	stream_seek(irp->input, 0x4);

	atrLen = MAX_ATR_SIZE;

#ifdef SCARD_AUTOALLOCATE
	readerLen = SCARD_AUTOALLOCATE;

	status = SCardStatus(hCard, (LPSTR) &readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);
#else
	readerLen = 256;
	readerName = malloc(readerLen);

	status = SCardStatus(hCard, (LPSTR) readerName, &readerLen, &state, &protocol, pbAtr, &atrLen);
#endif

	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
		return smartcard_output_return(irp, status);
	}

	DEBUG_SCARD("Success (state: 0x%08x, proto: 0x%08x)", (unsigned) state, (unsigned) protocol);
	DEBUG_SCARD("       Reader: \"%s\"", readerName ? readerName : "NULL");

#ifdef WITH_DEBUG_SCARD
	fprintf(stderr, "       ATR: ");
	for (i = 0; i < atrLen; i++)
		fprintf(stderr, "%02x%c", pbAtr[i], (i == atrLen - 1) ? ' ' : ':');
	fprintf(stderr, "\n");
#endif

	state = smartcard_map_state(state);

	poslen1 = stream_get_pos(irp->output);
	stream_write_UINT32(irp->output, readerLen);
	stream_write_UINT32(irp->output, 0x00020000);
	stream_write_UINT32(irp->output, state);
	stream_write_UINT32(irp->output, protocol);
	stream_write(irp->output, pbAtr, atrLen);

	if (atrLen < 32)
		stream_write_zero(irp->output, 32 - atrLen);
	stream_write_UINT32(irp->output, atrLen);

	poslen2 = stream_get_pos(irp->output);
	stream_write_UINT32(irp->output, readerLen);

	dataLength = smartcard_output_string(irp, readerName, wide);
	dataLength += smartcard_output_string(irp, "\0", wide);
	smartcard_output_repos(irp, dataLength);

	pos = stream_get_pos(irp->output);
	stream_set_pos(irp->output, poslen1);
	stream_write_UINT32(irp->output,dataLength);
	stream_set_pos(irp->output, poslen2);
	stream_write_UINT32(irp->output,dataLength);
	stream_set_pos(irp->output, pos);

	smartcard_output_alignment(irp, 8);

#ifdef SCARD_AUTOALLOCATE
	/* SCardFreeMemory(NULL, readerName); */
	free(readerName);
#else
	free(readerName);
#endif

	return status;
}

static UINT32 handle_Transmit(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hCard;
	UINT32 map[7], linkedLen;
	SCARD_IO_REQUEST pioSendPci, pioRecvPci, *pPioRecvPci;
	DWORD cbSendLength = 0, cbRecvLength = 0;
	BYTE *sendBuf = NULL, *recvBuf = NULL;

	stream_seek(irp->input, 0x14);
	stream_read_UINT32(irp->input, map[0]);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, map[1]);

	stream_read_UINT32(irp->input, pioSendPci.dwProtocol);
	stream_read_UINT32(irp->input, pioSendPci.cbPciLength);

	stream_read_UINT32(irp->input, map[2]);
	stream_read_UINT32(irp->input, cbSendLength);
	stream_read_UINT32(irp->input, map[3]);
	stream_read_UINT32(irp->input, map[4]);
	stream_read_UINT32(irp->input, map[5]);
	stream_read_UINT32(irp->input, cbRecvLength);

	if (map[0] & SCARD_INPUT_LINKED)
		smartcard_input_skip_linked(irp);

	stream_seek(irp->input, 4);
	stream_read_UINT32(irp->input, hCard);

	if (map[2] & SCARD_INPUT_LINKED)
	{
		/* sendPci */
		stream_read_UINT32(irp->input, linkedLen);

		stream_read_UINT32(irp->input, pioSendPci.dwProtocol);
		stream_seek(irp->input, linkedLen - 4);

		smartcard_input_repos(irp, linkedLen);
	}
	pioSendPci.cbPciLength = sizeof(SCARD_IO_REQUEST);

	if (map[3] & SCARD_INPUT_LINKED)
	{
		/* send buffer */
		stream_read_UINT32(irp->input, linkedLen);

		sendBuf = malloc(linkedLen);
		stream_read(irp->input, sendBuf, linkedLen);
		smartcard_input_repos(irp, linkedLen);
	}

	if (cbRecvLength)
		recvBuf = malloc(cbRecvLength);

	if (map[4] & SCARD_INPUT_LINKED)
	{
		/* recvPci */
		stream_read_UINT32(irp->input, linkedLen);

		stream_read_UINT32(irp->input, pioRecvPci.dwProtocol);
		stream_seek(irp->input, linkedLen - 4);

		smartcard_input_repos(irp, linkedLen);

		stream_read_UINT32(irp->input, map[6]);
		if (map[6] & SCARD_INPUT_LINKED)
		{
			/* not sure what this is */
			stream_read_UINT32(irp->input, linkedLen);
			stream_seek(irp->input, linkedLen);

			smartcard_input_repos(irp, linkedLen);
		}
		pioRecvPci.cbPciLength = sizeof(SCARD_IO_REQUEST);
		pPioRecvPci = &pioRecvPci;
	}
	else
	{
		pPioRecvPci = NULL;
	}
	pPioRecvPci = NULL;

	DEBUG_SCARD("SCardTransmit(hcard: 0x%08lx, send: %d bytes, recv: %d bytes)",
		(long unsigned) hCard, (int) cbSendLength, (int) cbRecvLength);

	status = SCardTransmit(hCard, &pioSendPci, sendBuf, cbSendLength,
			   pPioRecvPci, recvBuf, &cbRecvLength);

	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	}
	else
	{
		DEBUG_SCARD("Success (%d bytes)", (int) cbRecvLength);

		stream_write_UINT32(irp->output, 0); 	/* pioRecvPci 0x00; */

		smartcard_output_buffer_start(irp, cbRecvLength);	/* start of recvBuf output */

		smartcard_output_buffer(irp, (char*) recvBuf, cbRecvLength);
	}

	smartcard_output_alignment(irp, 8);

	free(sendBuf);
	free(recvBuf);

	return status;
}

static UINT32 handle_Control(IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	UINT32 map[3];
	UINT32 controlCode;
	UINT32 controlFunction;
	BYTE* recvBuffer = NULL;
	BYTE* sendBuffer = NULL;
	UINT32 recvLength;
	DWORD nBytesReturned;
	DWORD outBufferSize;

	stream_seek(irp->input, 0x14);
	stream_read_UINT32(irp->input, map[0]);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, map[1]);
	stream_read_UINT32(irp->input, controlCode);
	stream_read_UINT32(irp->input, recvLength);
	stream_read_UINT32(irp->input, map[2]);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, outBufferSize);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, hContext);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, hCard);

	/* Translate Windows SCARD_CTL_CODE's to corresponding local code */
	if (WIN_CTL_DEVICE_TYPE(controlCode) == WIN_FILE_DEVICE_SMARTCARD)
	{
		controlFunction = WIN_CTL_FUNCTION(controlCode);
		controlCode = SCARD_CTL_CODE(controlFunction);
	}
	DEBUG_SCARD("controlCode: 0x%08x", (unsigned) controlCode);

	if (map[2] & SCARD_INPUT_LINKED)
	{
		/* read real input size */
		stream_read_UINT32(irp->input, recvLength);

		recvBuffer = malloc(recvLength);

		if (!recvBuffer)
			return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

		stream_read(irp->input, recvBuffer, recvLength);
	}

	nBytesReturned = outBufferSize;
	sendBuffer = malloc(outBufferSize);

	if (!sendBuffer)
		return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

	status = SCardControl(hCard, (DWORD) controlCode, recvBuffer, (DWORD) recvLength,
		sendBuffer, (DWORD) outBufferSize, &nBytesReturned);

	if (status != SCARD_S_SUCCESS)
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned) status);
	else
		DEBUG_SCARD("Success (out: %u bytes)", (unsigned) nBytesReturned);

	stream_write_UINT32(irp->output, (UINT32) nBytesReturned);
	stream_write_UINT32(irp->output, 0x00000004);
	stream_write_UINT32(irp->output, nBytesReturned);

	if (nBytesReturned > 0)
	{
		stream_write(irp->output, sendBuffer, nBytesReturned);
		smartcard_output_repos(irp, nBytesReturned);
	}

	smartcard_output_alignment(irp, 8);

	free(recvBuffer);
	free(sendBuffer);

	return status;
}

static UINT32 handle_GetAttrib(IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	DWORD dwAttrId = 0;
	DWORD dwAttrLen = 0;
	DWORD attrLen = 0;
	BYTE* pbAttr = NULL;

	stream_seek(irp->input, 0x20);
	stream_read_UINT32(irp->input, dwAttrId);
	stream_seek(irp->input, 0x4);
	stream_read_UINT32(irp->input, dwAttrLen);
	stream_seek(irp->input, 0xC);
	stream_read_UINT32(irp->input, hCard);

	DEBUG_SCARD("hcard: 0x%08x, attrib: 0x%08x (%d bytes)",
		(unsigned) hCard, (unsigned) dwAttrId, (int) dwAttrLen);

#ifdef SCARD_AUTOALLOCATE
	if (dwAttrLen == 0)
	{
		attrLen = 0;
	}
	else
	{
		attrLen = SCARD_AUTOALLOCATE;
	}
#endif

	status = SCardGetAttrib(hCard, dwAttrId, attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

	if (status != SCARD_S_SUCCESS)
	{
#ifdef SCARD_AUTOALLOCATE
		if (dwAttrLen == 0)
			attrLen = 0;
		else
			attrLen = SCARD_AUTOALLOCATE;
#endif
	}

	if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
			attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

		if (status != SCARD_S_SUCCESS)
		{
#ifdef SCARD_AUTOALLOCATE
			if (dwAttrLen == 0)
				attrLen = 0;
			else
				attrLen = SCARD_AUTOALLOCATE;
#endif
		}
	}
	if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W && status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
			attrLen == 0 ? NULL : (BYTE*) &pbAttr, &attrLen);

		if (status != SCARD_S_SUCCESS)
		{
#ifdef SCARD_AUTOALLOCATE
			if (dwAttrLen == 0)
				attrLen = 0;
			else
				attrLen = SCARD_AUTOALLOCATE;
#endif
		}
	}
	if (attrLen > dwAttrLen && pbAttr != NULL)
	{
		status = SCARD_E_INSUFFICIENT_BUFFER;
	}
	dwAttrLen = attrLen;

	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)", pcsc_stringify_error(status), (unsigned int) status);
		free(pbAttr);
		return smartcard_output_return(irp, status);
	}
	else
	{
		DEBUG_SCARD("Success (%d bytes)", (int) dwAttrLen);

		stream_write_UINT32(irp->output, dwAttrLen);
		stream_write_UINT32(irp->output, 0x00000200);
		stream_write_UINT32(irp->output, dwAttrLen);

		if (!pbAttr)
		{
			stream_write_zero(irp->output, dwAttrLen);
		}
		else
		{
			stream_write(irp->output, pbAttr, dwAttrLen);
		}
		smartcard_output_repos(irp, dwAttrLen);
		/* align to multiple of 4 */
		stream_write_UINT32(irp->output, 0);
	}
	smartcard_output_alignment(irp, 8);

	free(pbAttr);

	return status;
}

static UINT32 handle_AccessStartedEvent(IRP* irp)
{
	smartcard_output_alignment(irp, 8);
	return SCARD_S_SUCCESS;
}

void scard_error(SMARTCARD_DEVICE* scard, IRP* irp, UINT32 ntstatus)
{
	/* [MS-RDPESC] 3.1.4.4 */
	fprintf(stderr, "scard processing error %x\n", ntstatus);

	stream_set_pos(irp->output, 0);	/* CHECKME */
	irp->IoStatus = ntstatus;
	irp->Complete(irp);
}

/* http://msdn.microsoft.com/en-gb/library/ms938473.aspx */
typedef struct _SERVER_SCARD_ATRMASK
{
	UINT32 cbAtr;
	BYTE rgbAtr[36];
	BYTE rgbMask[36];
}
SERVER_SCARD_ATRMASK;

static UINT32 handle_LocateCardsByATR(IRP* irp, BOOL wide)
{
	LONG status;
	int i, j, k;
	SCARDCONTEXT hContext;
	UINT32 atrMaskCount = 0;
	UINT32 readerCount = 0;
	SCARD_READERSTATE* cur = NULL;
	SCARD_READERSTATE* rsCur = NULL;
	SCARD_READERSTATE* readerStates = NULL;
	SERVER_SCARD_ATRMASK* curAtr = NULL;
	SERVER_SCARD_ATRMASK* pAtrMasks = NULL;

	stream_seek(irp->input, 0x2C);
	stream_read_UINT32(irp->input, hContext);
	stream_read_UINT32(irp->input, atrMaskCount);

	pAtrMasks = malloc(atrMaskCount * sizeof(SERVER_SCARD_ATRMASK));

	if (!pAtrMasks)
		return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

	for (i = 0; i < atrMaskCount; i++)
	{
		stream_read_UINT32(irp->input, pAtrMasks[i].cbAtr);
		stream_read(irp->input, pAtrMasks[i].rgbAtr, 36);
		stream_read(irp->input, pAtrMasks[i].rgbMask, 36);
	}

	stream_read_UINT32(irp->input, readerCount);

	readerStates = malloc(readerCount * sizeof(SCARD_READERSTATE));
	ZeroMemory(readerStates, readerCount * sizeof(SCARD_READERSTATE));

	if (!readerStates)
		return smartcard_output_return(irp, SCARD_E_NO_MEMORY);

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];

		stream_seek(irp->input, 4);

		/*
		 * TODO: on-wire is little endian; need to either
		 * convert to host endian or fix the headers to
		 * request the order we want
		 */
		stream_read_UINT32(irp->input, cur->dwCurrentState);
		stream_read_UINT32(irp->input, cur->dwEventState);
		stream_read_UINT32(irp->input, cur->cbAtr);
		stream_read(irp->input, cur->rgbAtr, 32);

		stream_seek(irp->input, 4);

		/* reset high bytes? */
		cur->dwCurrentState &= 0x0000FFFF;
		cur->dwEventState &= 0x0000FFFF;
		cur->dwEventState = 0;
	}

	for (i = 0; i < readerCount; i++)
	{
		cur = &readerStates[i];
		UINT32 dataLength;

		stream_seek(irp->input, 8);
		stream_read_UINT32(irp->input, dataLength);
		smartcard_input_repos(irp, smartcard_input_string(irp, (char **) &cur->szReader, dataLength, wide));

		DEBUG_SCARD("   \"%s\"", cur->szReader ? cur->szReader : "NULL");
		DEBUG_SCARD("       user: 0x%08x, state: 0x%08x, event: 0x%08x",
				(unsigned) cur->pvUserData, (unsigned) cur->dwCurrentState,
				(unsigned) cur->dwEventState);

		if (strcmp(cur->szReader, "\\\\?PnP?\\Notification") == 0)
			cur->dwCurrentState |= SCARD_STATE_IGNORE;
	}

	status = SCardGetStatusChange(hContext, 0x00000001, readerStates, readerCount);
	if (status != SCARD_S_SUCCESS)
	{
		DEBUG_SCARD("Failure: %s (0x%08x)",
			pcsc_stringify_error(status), (unsigned) status);

		return smartcard_output_return(irp, status);
	}

	DEBUG_SCARD("Success");
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

	stream_write_UINT32(irp->output, readerCount);
	stream_write_UINT32(irp->output, 0x00084dd8);
	stream_write_UINT32(irp->output, readerCount);

	for (i = 0, rsCur = readerStates; i < readerCount; i++, rsCur++)
	{
		stream_write_UINT32(irp->output, cur->dwCurrentState);
		stream_write_UINT32(irp->output, cur->dwEventState);
		stream_write_UINT32(irp->output, cur->cbAtr);
		stream_write(irp->output, cur->rgbAtr, 32);

		stream_write_zero(irp->output, 4);

		free((void*) cur->szReader);
	}

	smartcard_output_alignment(irp, 8);

	free(readerStates);

	return status;
}

BOOL smartcard_async_op(IRP* irp)
{
	UINT32 ioctl_code;

	/* peek ahead */
	stream_seek(irp->input, 8);
	stream_read_UINT32(irp->input, ioctl_code);
	stream_rewind(irp->input, 12);

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

void smartcard_device_control(SMARTCARD_DEVICE* scard, IRP* irp)
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

	stream_read_UINT32(irp->input, output_len);
	stream_read_UINT32(irp->input, input_len);
	stream_read_UINT32(irp->input, ioctl_code);

	stream_seek(irp->input, 20);	/* padding */

	// stream_seek(irp->input, 4);	/* TODO: parse len, le, v1 */
	// stream_seek(irp->input, 4);	/* 0xcccccccc */
	// stream_seek(irp->input, 4);	/* rpce len */

	/* [MS-RDPESC] 3.2.5.1 Sending Outgoing Messages */
	stream_extend(irp->output, 2048);

	irp_result_pos = stream_get_pos(irp->output);

	stream_write_UINT32(irp->output, 0x00000000); 	/* MS-RDPEFS 
							 * OutputBufferLength
							 * will be updated 
							 * later in this 
							 * function.
							 */
	/* [MS-RPCE] 2.2.6.1 */
	stream_write_UINT32(irp->output, 0x00081001); /* len 8, LE, v1 */
	stream_write_UINT32(irp->output, 0xcccccccc); /* filler */

	output_len_pos = stream_get_pos(irp->output);
	stream_seek(irp->output, 4);		/* size */

	stream_write_UINT32(irp->output, 0x0);	/* filler */

	result_pos = stream_get_pos(irp->output);
	stream_seek(irp->output, 4);		/* result */

	/* body */
	switch (ioctl_code)
	{
		case SCARD_IOCTL_ESTABLISH_CONTEXT:
			result = handle_EstablishContext(irp);
			break;

		case SCARD_IOCTL_IS_VALID_CONTEXT:
			result = handle_IsValidContext(irp);
			break;

		case SCARD_IOCTL_RELEASE_CONTEXT:
			result = handle_ReleaseContext(irp);
			break;

		case SCARD_IOCTL_LIST_READERS:
			result = handle_ListReaders(irp, 0);
			break;
		case SCARD_IOCTL_LIST_READERS + 4:
			result = handle_ListReaders(irp, 1);
			break;

		case SCARD_IOCTL_LIST_READER_GROUPS:
		case SCARD_IOCTL_LIST_READER_GROUPS + 4:
			/* typically not used unless list_readers fail */
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GET_STATUS_CHANGE:
			result = handle_GetStatusChange(irp, 0);
			break;
		case SCARD_IOCTL_GET_STATUS_CHANGE + 4:
			result = handle_GetStatusChange(irp, 1);
			break;

		case SCARD_IOCTL_CANCEL:
			result = handle_Cancel(irp);
			break;

		case SCARD_IOCTL_CONNECT:
			result = handle_Connect(irp, 0);
			break;
		case SCARD_IOCTL_CONNECT + 4:
			result = handle_Connect(irp, 1);
			break;

		case SCARD_IOCTL_RECONNECT:
			result = handle_Reconnect(irp);
			break;

		case SCARD_IOCTL_DISCONNECT:
			result = handle_Disconnect(irp);
			break;

		case SCARD_IOCTL_BEGIN_TRANSACTION:
			result = handle_BeginTransaction(irp);
			break;

		case SCARD_IOCTL_END_TRANSACTION:
			result = handle_EndTransaction(irp);
			break;

		case SCARD_IOCTL_STATE:
			result = handle_State(irp);
			break;

		case SCARD_IOCTL_STATUS:
			result = handle_Status(irp, 0);
			break;
		case SCARD_IOCTL_STATUS + 4:
			result = handle_Status(irp, 1);
			break;

		case SCARD_IOCTL_TRANSMIT:
			result = handle_Transmit(irp);
			break;

		case SCARD_IOCTL_CONTROL:
			result = handle_Control(irp);
			break;

		case SCARD_IOCTL_GETATTRIB:
			result = handle_GetAttrib(irp);
			break;

		case SCARD_IOCTL_ACCESS_STARTED_EVENT:
			result = handle_AccessStartedEvent(irp);
			break;

		case SCARD_IOCTL_LOCATE_CARDS_BY_ATR:
			result = handle_LocateCardsByATR(irp, 0);
			break;
		case SCARD_IOCTL_LOCATE_CARDS_BY_ATR + 4:
			result = handle_LocateCardsByATR(irp, 1);
			break;

		default:
			result = 0xc0000001;
			fprintf(stderr, "scard unknown ioctl 0x%x\n", ioctl_code);
			break;
	}

	/* look for NTSTATUS errors */
	if ((result & 0xc0000000) == 0xc0000000)
		return scard_error(scard, irp, result);

	/* per Ludovic Rousseau, map different usage of this particular
  	 * error code between pcsc-lite & windows */
	if (result == 0x8010001F)
		result = 0x80100022;

	/* handle response packet */
	pos = stream_get_pos(irp->output);
	stream_len = pos - irp_result_pos - 4;	/* Value of OutputBufferLength */
	stream_set_pos(irp->output, irp_result_pos);
	stream_write_UINT32(irp->output, stream_len);

	stream_set_pos(irp->output, output_len_pos);
	/* Remove the effect of the MS-RPCE Common Type Header and Private
	 * Header (Sections 2.2.6.1 and 2.2.6.2).
	 */
	stream_write_UINT32(irp->output, stream_len - header_lengths);

	stream_set_pos(irp->output, result_pos);
	stream_write_UINT32(irp->output, result);

	stream_set_pos(irp->output, pos);

#ifdef WITH_DEBUG_SCARD
	winpr_HexDump(stream_get_data(irp->output), stream_get_length(irp->output));
#endif
	irp->IoStatus = 0;

	irp->Complete(irp);

}
