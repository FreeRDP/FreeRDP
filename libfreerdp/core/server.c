/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include <freerdp/constants.h>
#include <freerdp/server/channels.h>

#include "rdp.h"

#include "server.h"

#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(fmt, ...) DEBUG_CLASS(DVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_DVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

static rdpPeerChannel* wts_get_dvc_channel_by_id(WTSVirtualChannelManager* vcm, UINT32 ChannelId)
{
	int index;
	int count;
	BOOL found = FALSE;
	rdpPeerChannel* channel = NULL;

	ArrayList_Lock(vcm->dynamicVirtualChannels);

	count = ArrayList_Count(vcm->dynamicVirtualChannels);

	for (index = 0; index < count; index++)
	{
		channel = (rdpPeerChannel*) ArrayList_GetItem(vcm->dynamicVirtualChannels, index);

		if (channel->channelId == ChannelId)
		{
			found = TRUE;
			break;
		}
	}

	ArrayList_Unlock(vcm->dynamicVirtualChannels);

	return found ? channel : NULL;
}

static void wts_queue_receive_data(rdpPeerChannel* channel, const BYTE* Buffer, UINT32 Length)
{
	BYTE* buffer;
	UINT32 length;
	UINT16 channelId;

	length = Length;
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, Buffer, length);
	channelId = channel->channelId;

	MessageQueue_Post(channel->queue, (void*) (UINT_PTR) channelId, 0, (void*) buffer, (void*) (UINT_PTR) length);
}

static void wts_queue_send_item(rdpPeerChannel* channel, BYTE* Buffer, UINT32 Length)
{
	BYTE* buffer;
	UINT32 length;
	UINT16 channelId;

	buffer = Buffer;
	length = Length;
	channelId = channel->channelId;

	MessageQueue_Post(channel->vcm->queue, (void*) (UINT_PTR) channelId, 0, (void*) buffer, (void*) (UINT_PTR) length);
}

static int wts_read_variable_uint(wStream* s, int cbLen, UINT32* val)
{
	switch (cbLen)
	{
		case 0:
			if (Stream_GetRemainingLength(s) < 1)
				return 0;
			Stream_Read_UINT8(s, *val);
			return 1;

		case 1:
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, *val);
			return 2;

		default:
			if (Stream_GetRemainingLength(s) < 4)
				return 0;
			Stream_Read_UINT32(s, *val);
			return 4;
	}
}

static void wts_read_drdynvc_capabilities_response(rdpPeerChannel* channel, UINT32 length)
{
	UINT16 Version;

	if (length < 3)
		return;

	Stream_Seek_UINT8(channel->receiveData); /* Pad (1 byte) */
	Stream_Read_UINT16(channel->receiveData, Version);

	DEBUG_DVC("Version: %d", Version);

	channel->vcm->drdynvc_state = DRDYNVC_STATE_READY;
}

static void wts_read_drdynvc_create_response(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	UINT32 CreationStatus;

	if (length < 4)
		return;

	Stream_Read_UINT32(s, CreationStatus);

	if ((INT32) CreationStatus < 0)
	{
		DEBUG_DVC("ChannelId %d creation failed (%d)", channel->channelId, (INT32) CreationStatus);
		channel->dvc_open_state = DVC_OPEN_STATE_FAILED;
	}
	else
	{
		DEBUG_DVC("ChannelId %d creation succeeded", channel->channelId);
		channel->dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	}
}

static void wts_read_drdynvc_data_first(rdpPeerChannel* channel, wStream* s, int cbLen, UINT32 length)
{
	int value;

	value = wts_read_variable_uint(s, cbLen, &channel->dvc_total_length);

	if (value == 0)
		return;

	length -= value;

	if (length > channel->dvc_total_length)
		return;

	Stream_SetPosition(channel->receiveData, 0);
	Stream_EnsureRemainingCapacity(channel->receiveData, (int) channel->dvc_total_length);
	Stream_Write(channel->receiveData, Stream_Pointer(s), length);
}

static void wts_read_drdynvc_data(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	if (channel->dvc_total_length > 0)
	{
		if (Stream_GetPosition(channel->receiveData) + length > channel->dvc_total_length)
		{
			channel->dvc_total_length = 0;
			fprintf(stderr, "wts_read_drdynvc_data: incorrect fragment data, discarded.\n");
			return;
		}

		Stream_Write(channel->receiveData, Stream_Pointer(s), length);

		if (Stream_GetPosition(channel->receiveData) >= (int) channel->dvc_total_length)
		{
			wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData), channel->dvc_total_length);
			channel->dvc_total_length = 0;
		}
	}
	else
	{
		wts_queue_receive_data(channel, Stream_Pointer(s), length);
	}
}

static void wts_read_drdynvc_close_response(rdpPeerChannel* channel)
{
	DEBUG_DVC("ChannelId %d close response", channel->channelId);
	channel->dvc_open_state = DVC_OPEN_STATE_CLOSED;
}

static void wts_read_drdynvc_pdu(rdpPeerChannel* channel)
{
	UINT32 length;
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	UINT32 ChannelId;
	rdpPeerChannel* dvc;

	length = Stream_GetPosition(channel->receiveData);

	if (length < 1)
		return;

	Stream_SetPosition(channel->receiveData, 0);
	Stream_Read_UINT8(channel->receiveData, value);

	length--;
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;

	if (Cmd == CAPABILITY_REQUEST_PDU)
	{
		wts_read_drdynvc_capabilities_response(channel, length);
	}
	else if (channel->vcm->drdynvc_state == DRDYNVC_STATE_READY)
	{
		value = wts_read_variable_uint(channel->receiveData, cbChId, &ChannelId);

		if (value == 0)
			return;

		length -= value;

		DEBUG_DVC("Cmd %d ChannelId %d length %d", Cmd, ChannelId, length);
		dvc = wts_get_dvc_channel_by_id(channel->vcm, ChannelId);

		if (dvc)
		{
			switch (Cmd)
			{
				case CREATE_REQUEST_PDU:
					wts_read_drdynvc_create_response(dvc, channel->receiveData, length);
					break;

				case DATA_FIRST_PDU:
					wts_read_drdynvc_data_first(dvc, channel->receiveData, Sp, length);
					break;

				case DATA_PDU:
					wts_read_drdynvc_data(dvc, channel->receiveData, length);
					break;

				case CLOSE_REQUEST_PDU:
					wts_read_drdynvc_close_response(dvc);
					break;

				default:
					fprintf(stderr, "wts_read_drdynvc_pdu: Cmd %d not recognized.\n", Cmd);
					break;
			}
		}
		else
		{
			DEBUG_DVC("ChannelId %d not exists.", ChannelId);
		}
	}
	else
	{
		fprintf(stderr, "wts_read_drdynvc_pdu: received Cmd %d but channel is not ready.\n", Cmd);
	}
}

static int wts_write_variable_uint(wStream* stream, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(stream, val);
	}
	else
	{
		cb = 3;
		Stream_Write_UINT32(stream, val);
	}

	return cb;
}

static void wts_write_drdynvc_header(wStream *s, BYTE Cmd, UINT32 ChannelId)
{
	BYTE* bm;
	int cbChId;

	Stream_GetPointer(s, bm);
	Stream_Seek_UINT8(s);
	cbChId = wts_write_variable_uint(s, ChannelId);
	*bm = ((Cmd & 0x0F) << 4) | cbChId;
}

static void wts_write_drdynvc_create_request(wStream *s, UINT32 ChannelId, const char *ChannelName)
{
	UINT32 len;

	wts_write_drdynvc_header(s, CREATE_REQUEST_PDU, ChannelId);
	len = strlen(ChannelName) + 1;
	Stream_EnsureRemainingCapacity(s, (int) len);
	Stream_Write(s, ChannelName, len);
}

static void WTSProcessChannelData(rdpPeerChannel* channel, UINT16 channelId, BYTE* data, int size, int flags, int totalSize)
{
	if (flags & CHANNEL_FLAG_FIRST)
	{
		Stream_SetPosition(channel->receiveData, 0);
	}

	Stream_EnsureRemainingCapacity(channel->receiveData, size);
	Stream_Write(channel->receiveData, data, size);

	if (flags & CHANNEL_FLAG_LAST)
	{
		if (Stream_GetPosition(channel->receiveData) != totalSize)
		{
			fprintf(stderr, "WTSProcessChannelData: read error\n");
		}
		if (channel == channel->vcm->drdynvc_channel)
		{
			wts_read_drdynvc_pdu(channel);
		}
		else
		{
			wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData), Stream_GetPosition(channel->receiveData));
		}
		Stream_SetPosition(channel->receiveData, 0);
	}
}

static int WTSReceiveChannelData(freerdp_peer* client, UINT16 channelId, BYTE* data, int size, int flags, int totalSize)
{
	int i;
	BOOL status = FALSE;
	rdpPeerChannel* channel;
	rdpMcs* mcs = client->context->rdp->mcs;

	for (i = 0; i < mcs->channelCount; i++)
	{
		if (mcs->channels[i].ChannelId == channelId)
			break;
	}

	if (i < mcs->channelCount)
	{
		channel = (rdpPeerChannel*) mcs->channels[i].handle;

		if (channel)
		{
			WTSProcessChannelData(channel, channelId, data, size, flags, totalSize);
			status = TRUE;
		}
	}

	return status;
}

WTSVirtualChannelManager* WTSCreateVirtualChannelManager(freerdp_peer* client)
{
	WTSVirtualChannelManager* vcm;

	vcm = (WTSVirtualChannelManager*) malloc(sizeof(WTSVirtualChannelManager));

	if (vcm)
	{
		ZeroMemory(vcm, sizeof(WTSVirtualChannelManager));

		vcm->client = client;
		vcm->rdp = client->context->rdp;

		vcm->queue = MessageQueue_New(NULL);

		vcm->dvc_channel_id_seq = 1;
		vcm->dynamicVirtualChannels = ArrayList_New(TRUE);

		client->ReceiveChannelData = WTSReceiveChannelData;
	}

	return vcm;
}

void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm)
{
	int index;
	int count;
	rdpPeerChannel* channel;

	if (vcm)
	{
		ArrayList_Lock(vcm->dynamicVirtualChannels);

		count = ArrayList_Count(vcm->dynamicVirtualChannels);

		for (index = 0; index < count; index++)
		{
			channel = (rdpPeerChannel*) ArrayList_GetItem(vcm->dynamicVirtualChannels, index);
			WTSVirtualChannelClose(channel);
		}

		ArrayList_Unlock(vcm->dynamicVirtualChannels);

		ArrayList_Free(vcm->dynamicVirtualChannels);

		if (vcm->drdynvc_channel)
		{
			WTSVirtualChannelClose(vcm->drdynvc_channel);
			vcm->drdynvc_channel = NULL;
		}

		MessageQueue_Free(vcm->queue);

		free(vcm);
	}
}

void WTSVirtualChannelManagerGetFileDescriptor(WTSVirtualChannelManager* vcm, void** fds, int* fds_count)
{
	void* fd;

	fd = GetEventWaitObject(MessageQueue_Event(vcm->queue));

	if (fd)
	{
		fds[*fds_count] = fd;
		(*fds_count)++;
	}

#if 0
	if (vcm->drdynvc_channel)
	{
		fd = GetEventWaitObject(vcm->drdynvc_channel->receiveEvent);

		if (fd)
		{
			fds[*fds_count] = fd;
			(*fds_count)++;
		}
	}
#endif
}

BOOL WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm)
{
	wMessage message;
	BOOL status = TRUE;
	rdpPeerChannel* channel;
	UINT32 dynvc_caps;

	if ((vcm->drdynvc_state == DRDYNVC_STATE_NONE) && vcm->client->activated)
	{
		/* Initialize drdynvc channel once and only once. */
		vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;

		channel = WTSVirtualChannelManagerOpenEx(vcm, "drdynvc", 0);

		if (channel)
		{
			vcm->drdynvc_channel = channel;
			dynvc_caps = 0x00010050; /* DYNVC_CAPS_VERSION1 (4 bytes) */
			WTSVirtualChannelWrite(channel, (PCHAR) &dynvc_caps, sizeof(dynvc_caps), NULL);
		}
	}

	while (MessageQueue_Peek(vcm->queue, &message, TRUE))
	{
		BYTE* buffer;
		UINT32 length;
		UINT16 channelId;

		channelId = (UINT16) (UINT_PTR) message.context;
		buffer = (BYTE*) message.wParam;
		length = (UINT32) (UINT_PTR) message.lParam;

		if (vcm->client->SendChannelData(vcm->client, channelId, buffer, length) == FALSE)
		{
			status = FALSE;
		}

		free(buffer);

		if (!status)
			break;
	}

	return status;
}

HANDLE WTSVirtualChannelManagerGetEventHandle(WTSVirtualChannelManager* vcm)
{
	return MessageQueue_Event(vcm->queue);
}

BOOL WTSVirtualChannelManagerIsChannelJoined(WTSVirtualChannelManager* vcm, const char* name)
{
	UINT32 index;
	BOOL joined = FALSE;
	rdpMcs* mcs = vcm->rdp->mcs;

	for (index = 0; index < mcs->channelCount; index++)
	{
		if (mcs->channels[index].joined)
		{
			if (strncmp(mcs->channels[index].Name, name, strlen(name)) == 0)
			{
				joined = TRUE;
			}
		}
	}

	return joined;
}

HANDLE WTSVirtualChannelManagerOpenEx(WTSVirtualChannelManager* vcm, LPSTR pVirtualName, DWORD flags)
{
	int i;
	int len;
	wStream* s;
	rdpPeerChannel* channel;
	freerdp_peer* client = vcm->client;
	rdpMcs* mcs = client->context->rdp->mcs;

	if ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0)
	{
		for (i = 0; i < mcs->channelCount; i++)
		{
			if (mcs->channels[i].joined && (strncmp(mcs->channels[i].Name, "drdynvc", 7) == 0))
			{
				break;
			}
		}

		if (i >= mcs->channelCount)
		{
			DEBUG_DVC("Dynamic virtual channel not registered.");
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		if (!vcm->drdynvc_channel || (vcm->drdynvc_state != DRDYNVC_STATE_READY))
		{
			DEBUG_DVC("Dynamic virtual channel not ready.");
			SetLastError(ERROR_NOT_READY);
			return NULL;
		}

		channel = (rdpPeerChannel*) malloc(sizeof(rdpPeerChannel));
		ZeroMemory(channel, sizeof(rdpPeerChannel));

		channel->vcm = vcm;
		channel->client = client;
		channel->channelType = RDP_PEER_CHANNEL_TYPE_DVC;
		channel->receiveData = Stream_New(NULL, client->settings->VirtualChannelChunkSize);
		channel->queue = MessageQueue_New(NULL);

		channel->channelId = vcm->dvc_channel_id_seq++;
		ArrayList_Add(vcm->dynamicVirtualChannels, channel);

		s = Stream_New(NULL, 64);
		wts_write_drdynvc_create_request(s, channel->channelId, pVirtualName);
		WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
		Stream_Free(s, TRUE);
	}
	else
	{
		len = strlen(pVirtualName);

		if (len > 8)
		{
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		for (i = 0; i < mcs->channelCount; i++)
		{
			if (mcs->channels[i].joined && (strncmp(mcs->channels[i].Name, pVirtualName, len) == 0))
			{
				break;
			}
		}

		if (i >= mcs->channelCount)
		{
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		channel = (rdpPeerChannel*) mcs->channels[i].handle;

		if (!channel)
		{
			channel = (rdpPeerChannel*) malloc(sizeof(rdpPeerChannel));
			ZeroMemory(channel, sizeof(rdpPeerChannel));

			channel->vcm = vcm;
			channel->client = client;
			channel->channelId = mcs->channels[i].ChannelId;
			channel->index = i;
			channel->channelType = RDP_PEER_CHANNEL_TYPE_SVC;
			channel->receiveData = Stream_New(NULL, client->settings->VirtualChannelChunkSize);
			channel->queue = MessageQueue_New(NULL);

			mcs->channels[i].handle = channel;
		}
	}

	return channel;
}

BOOL FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	void* pfd;
	BOOL bval;
	void* fds[10];
	HANDLE hEvent;
	int fds_count = 0;
	BOOL status = FALSE;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;
	ZeroMemory(fds, sizeof(fds));

	hEvent = MessageQueue_Event(channel->queue);

	switch (WtsVirtualClass)
	{
		case WTSVirtualFileHandle:

			pfd = GetEventWaitObject(hEvent);

			if (pfd)
			{
				fds[fds_count] = pfd;
				(fds_count)++;
			}

			*ppBuffer = malloc(sizeof(void*));
			CopyMemory(*ppBuffer, &fds[0], sizeof(void*));
			*pBytesReturned = sizeof(void*);
			status = TRUE;
			break;

		case WTSVirtualEventHandle:
			*ppBuffer = malloc(sizeof(HANDLE));
			CopyMemory(*ppBuffer, &(hEvent), sizeof(HANDLE));
			*pBytesReturned = sizeof(void*);
			status = TRUE;
			break;

		case WTSVirtualChannelReady:
			if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
			{
				bval = TRUE;
				status = TRUE;
			}
			else
			{
				switch (channel->dvc_open_state)
				{
					case DVC_OPEN_STATE_NONE:
						bval = FALSE;
						status = TRUE;
						break;

					case DVC_OPEN_STATE_SUCCEEDED:
						bval = TRUE;
						status = TRUE;
						break;

					default:
						bval = FALSE;
						status = FALSE;
						break;
				}
			}

			*ppBuffer = malloc(sizeof(BOOL));
			CopyMemory(*ppBuffer, &bval, sizeof(BOOL));
			*pBytesReturned = sizeof(BOOL);
			break;

		default:
			break;
	}
	return status;
}

VOID FreeRDP_WTSFreeMemory(PVOID pMemory)
{
	free(pMemory);
}

HANDLE FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	return INVALID_HANDLE_VALUE;
}

HANDLE FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	return INVALID_HANDLE_VALUE;
}

BOOL FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	wStream* s;
	rdpMcs* mcs;
	WTSVirtualChannelManager* vcm;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (channel)
	{
		vcm = channel->vcm;
		mcs = vcm->client->context->rdp->mcs;

		if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
		{
			if (channel->index < mcs->channelCount)
				mcs->channels[channel->index].handle = NULL;
		}
		else
		{
			ArrayList_Remove(vcm->dynamicVirtualChannels, channel);

			if (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED)
			{
				s = Stream_New(NULL, 8);
				wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, channel->channelId);
				WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
				Stream_Free(s, TRUE);
			}
		}

		if (channel->receiveData)
			Stream_Free(channel->receiveData, TRUE);

		if (channel->queue)
		{
			MessageQueue_Free(channel->queue);
			channel->queue = NULL;
		}

		free(channel);
	}

	return TRUE;
}


BOOL FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	BYTE* buffer;
	UINT32 length;
	UINT16 channelId;
	wMessage message;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (!MessageQueue_Peek(channel->queue, &message, TRUE))
	{
		*pBytesRead = 0;
		return TRUE;
	}

	channelId = (UINT16) (UINT_PTR) message.context;
	buffer = (BYTE*) message.wParam;
	length = (UINT32) (UINT_PTR) message.lParam;

	*pBytesRead = length;

	if (length > BufferSize)
		return FALSE;

	CopyMemory(Buffer, buffer, length);
	free(buffer);

	return TRUE;
}

BOOL FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	wStream* s;
	int cbLen;
	int cbChId;
	int first;
	BYTE* buffer;
	UINT32 length;
	UINT32 written;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (!channel)
		return FALSE;

	if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		length = Length;
		buffer = (BYTE*) malloc(length);
		CopyMemory(buffer, Buffer, length);

		wts_queue_send_item(channel, buffer, length);
	}
	else if (!channel->vcm->drdynvc_channel || (channel->vcm->drdynvc_state != DRDYNVC_STATE_READY))
	{
		DEBUG_DVC("drdynvc not ready");
		return FALSE;
	}
	else
	{
		first = TRUE;

		while (Length > 0)
		{
			s = Stream_New(NULL, channel->client->settings->VirtualChannelChunkSize);
			buffer = Stream_Buffer(s);

			Stream_Seek_UINT8(s);
			cbChId = wts_write_variable_uint(s, channel->channelId);

			if (first && (Length > (UINT32) Stream_GetRemainingLength(s)))
			{
				cbLen = wts_write_variable_uint(s, Length);
				buffer[0] = (DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId;
			}
			else
			{
				buffer[0] = (DATA_PDU << 4) | cbChId;
			}

			first = FALSE;
			written = Stream_GetRemainingLength(s);

			if (written > Length)
				written = Length;

			Stream_Write(s, Buffer, written);
			length = Stream_GetPosition(s);
			Stream_Free(s, FALSE);

			Length -= written;
			Buffer += written;

			wts_queue_send_item(channel->vcm->drdynvc_channel, buffer, length);
		}
	}

	if (pBytesWritten)
		*pBytesWritten = Length;

	return TRUE;
}
