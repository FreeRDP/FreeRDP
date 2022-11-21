/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX USB Redirection
 *
 * Copyright 2012 Atrust corp.
 * Copyright 2012 Alfred Liu <alfred.liu@atruscorp.com>
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
#include <stdlib.h>
#include <string.h>

#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <errno.h>

#include "libusb_udevice.h"
#include "../common/urbdrc_types.h"

#define BASIC_STATE_FUNC_DEFINED(_arg, _type)             \
	static _type udev_get_##_arg(IUDEVICE* idev)          \
	{                                                     \
		UDEVICE* pdev = (UDEVICE*)idev;                   \
		return pdev->_arg;                                \
	}                                                     \
	static void udev_set_##_arg(IUDEVICE* idev, _type _t) \
	{                                                     \
		UDEVICE* pdev = (UDEVICE*)idev;                   \
		pdev->_arg = _t;                                  \
	}

#define BASIC_POINT_FUNC_DEFINED(_arg, _type)               \
	static _type udev_get_p_##_arg(IUDEVICE* idev)          \
	{                                                       \
		UDEVICE* pdev = (UDEVICE*)idev;                     \
		return pdev->_arg;                                  \
	}                                                       \
	static void udev_set_p_##_arg(IUDEVICE* idev, _type _t) \
	{                                                       \
		UDEVICE* pdev = (UDEVICE*)idev;                     \
		pdev->_arg = _t;                                    \
	}

#define BASIC_STATE_FUNC_REGISTER(_arg, _dev) \
	_dev->iface.get_##_arg = udev_get_##_arg; \
	_dev->iface.set_##_arg = udev_set_##_arg

#if LIBUSB_API_VERSION >= 0x01000103
#define HAVE_STREAM_ID_API 1
#endif

typedef struct
{
	wStream* data;
	BOOL noack;
	UINT32 MessageId;
	UINT32 StartFrame;
	UINT32 ErrorCount;
	IUDEVICE* idev;
	UINT32 OutputBufferSize;
	GENERIC_CHANNEL_CALLBACK* callback;
	t_isoch_transfer_cb cb;
	wArrayList* queue;
#if !defined(HAVE_STREAM_ID_API)
	UINT32 streamID;
#endif
} ASYNC_TRANSFER_USER_DATA;

static void request_free(void* value);

static struct libusb_transfer* list_contains(wArrayList* list, UINT32 streamID)
{
	size_t x, count;
	if (!list)
		return NULL;
	count = ArrayList_Count(list);
	for (x = 0; x < count; x++)
	{
		struct libusb_transfer* transfer = ArrayList_GetItem(list, x);

#if defined(HAVE_STREAM_ID_API)
		const UINT32 currentID = libusb_transfer_get_stream_id(transfer);
#else
		const ASYNC_TRANSFER_USER_DATA* user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
		const UINT32 currentID = user_data->streamID;
#endif
		if (currentID == streamID)
			return transfer;
	}
	return NULL;
}

static UINT32 stream_id_from_buffer(struct libusb_transfer* transfer)
{
	if (!transfer)
		return 0;
#if defined(HAVE_STREAM_ID_API)
	return libusb_transfer_get_stream_id(transfer);
#else
	ASYNC_TRANSFER_USER_DATA* user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
	if (!user_data)
		return 0;
	return user_data->streamID;
#endif
}

static void set_stream_id_for_buffer(struct libusb_transfer* transfer, UINT32 streamID)
{
#if defined(HAVE_STREAM_ID_API)
	libusb_transfer_set_stream_id(transfer, streamID);
#else
	ASYNC_TRANSFER_USER_DATA* user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
	if (!user_data)
		return;
	user_data->streamID = streamID;
#endif
}
static BOOL log_libusb_result_(wLog* log, DWORD lvl, const char* fmt, const char* fkt,
                               const char* file, size_t line, int error, ...)
{
	WINPR_UNUSED(file);

	if (error < 0)
	{
		char buffer[8192] = { 0 };
		va_list ap;
		va_start(ap, error);
		vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);

		WLog_Print(log, lvl, "[%s:%" PRIuz "]: %s: error %s[%d]", fkt, line, buffer,
		           libusb_error_name(error), error);
		return TRUE;
	}
	return FALSE;
}

#define log_libusb_result(log, lvl, fmt, error, ...) \
	log_libusb_result_((log), (lvl), (fmt), __FUNCTION__, __FILE__, __LINE__, error, ##__VA_ARGS__)

const char* usb_interface_class_to_string(uint8_t class)
{
	switch (class)
	{
		case LIBUSB_CLASS_PER_INTERFACE:
			return "LIBUSB_CLASS_PER_INTERFACE";
		case LIBUSB_CLASS_AUDIO:
			return "LIBUSB_CLASS_AUDIO";
		case LIBUSB_CLASS_COMM:
			return "LIBUSB_CLASS_COMM";
		case LIBUSB_CLASS_HID:
			return "LIBUSB_CLASS_HID";
		case LIBUSB_CLASS_PHYSICAL:
			return "LIBUSB_CLASS_PHYSICAL";
		case LIBUSB_CLASS_PRINTER:
			return "LIBUSB_CLASS_PRINTER";
		case LIBUSB_CLASS_IMAGE:
			return "LIBUSB_CLASS_IMAGE";
		case LIBUSB_CLASS_MASS_STORAGE:
			return "LIBUSB_CLASS_MASS_STORAGE";
		case LIBUSB_CLASS_HUB:
			return "LIBUSB_CLASS_HUB";
		case LIBUSB_CLASS_DATA:
			return "LIBUSB_CLASS_DATA";
		case LIBUSB_CLASS_SMART_CARD:
			return "LIBUSB_CLASS_SMART_CARD";
		case LIBUSB_CLASS_CONTENT_SECURITY:
			return "LIBUSB_CLASS_CONTENT_SECURITY";
		case LIBUSB_CLASS_VIDEO:
			return "LIBUSB_CLASS_VIDEO";
		case LIBUSB_CLASS_PERSONAL_HEALTHCARE:
			return "LIBUSB_CLASS_PERSONAL_HEALTHCARE";
		case LIBUSB_CLASS_DIAGNOSTIC_DEVICE:
			return "LIBUSB_CLASS_DIAGNOSTIC_DEVICE";
		case LIBUSB_CLASS_WIRELESS:
			return "LIBUSB_CLASS_WIRELESS";
		case LIBUSB_CLASS_APPLICATION:
			return "LIBUSB_CLASS_APPLICATION";
		case LIBUSB_CLASS_VENDOR_SPEC:
			return "LIBUSB_CLASS_VENDOR_SPEC";
		default:
			return "UNKNOWN_DEVICE_CLASS";
	}
}

static ASYNC_TRANSFER_USER_DATA* async_transfer_user_data_new(IUDEVICE* idev, UINT32 MessageId,
                                                              size_t offset, size_t BufferSize,
                                                              const BYTE* data, size_t packetSize,
                                                              BOOL NoAck, t_isoch_transfer_cb cb,
                                                              GENERIC_CHANNEL_CALLBACK* callback)
{
	ASYNC_TRANSFER_USER_DATA* user_data;
	UDEVICE* pdev = (UDEVICE*)idev;

	if (BufferSize > UINT32_MAX)
		return NULL;

	user_data = calloc(1, sizeof(ASYNC_TRANSFER_USER_DATA));
	if (!user_data)
		return NULL;

	user_data->data = Stream_New(NULL, offset + BufferSize + packetSize);

	if (!user_data->data)
	{
		free(user_data);
		return NULL;
	}

	Stream_Seek(user_data->data, offset); /* Skip header offset */
	if (data)
		memcpy(Stream_Pointer(user_data->data), data, BufferSize);
	else
		user_data->OutputBufferSize = (UINT32)BufferSize;

	user_data->noack = NoAck;
	user_data->cb = cb;
	user_data->callback = callback;
	user_data->idev = idev;
	user_data->MessageId = MessageId;

	user_data->queue = pdev->request_queue;

	return user_data;
}

static void async_transfer_user_data_free(ASYNC_TRANSFER_USER_DATA* user_data)
{
	if (user_data)
	{
		Stream_Free(user_data->data, TRUE);
		free(user_data);
	}
}

static void LIBUSB_CALL func_iso_callback(struct libusb_transfer* transfer)
{
	ASYNC_TRANSFER_USER_DATA* user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
	const UINT32 streamID = stream_id_from_buffer(transfer);
	wArrayList* list = user_data->queue;

	ArrayList_Lock(list);
	switch (transfer->status)
	{
		case LIBUSB_TRANSFER_COMPLETED:
		{
			int i;
			UINT32 index = 0;
			BYTE* dataStart = Stream_Pointer(user_data->data);
			Stream_SetPosition(user_data->data,
			                   40); /* TS_URB_ISOCH_TRANSFER_RESULT IsoPacket offset */

			for (i = 0; i < transfer->num_iso_packets; i++)
			{
				const UINT32 act_len = transfer->iso_packet_desc[i].actual_length;
				Stream_Write_UINT32(user_data->data, index);
				Stream_Write_UINT32(user_data->data, act_len);
				Stream_Write_UINT32(user_data->data, transfer->iso_packet_desc[i].status);

				if (transfer->iso_packet_desc[i].status != USBD_STATUS_SUCCESS)
					user_data->ErrorCount++;
				else
				{
					const unsigned char* packetBuffer =
					    libusb_get_iso_packet_buffer_simple(transfer, i);
					BYTE* data = dataStart + index;

					if (data != packetBuffer)
						memmove(data, packetBuffer, act_len);

					index += act_len;
				}
			}
		}
			/* fallthrough */

		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_TIMED_OUT:
		case LIBUSB_TRANSFER_ERROR:
		{
			const UINT32 InterfaceId =
			    ((STREAM_ID_PROXY << 30) | user_data->idev->get_ReqCompletion(user_data->idev));

			if (list_contains(list, streamID))
			{
				if (!user_data->noack)
				{
					const UINT32 RequestID = streamID & INTERFACE_ID_MASK;
					user_data->cb(user_data->idev, user_data->callback, user_data->data,
					              InterfaceId, user_data->noack, user_data->MessageId, RequestID,
					              transfer->num_iso_packets, transfer->status,
					              user_data->StartFrame, user_data->ErrorCount,
					              user_data->OutputBufferSize);
					user_data->data = NULL;
				}
				ArrayList_Remove(list, transfer);
			}
		}
		break;
		default:
			break;
	}
	ArrayList_Unlock(list);
}

static const LIBUSB_ENDPOINT_DESCEIPTOR* func_get_ep_desc(LIBUSB_CONFIG_DESCRIPTOR* LibusbConfig,
                                                          MSUSB_CONFIG_DESCRIPTOR* MsConfig,
                                                          UINT32 EndpointAddress)
{
	BYTE alt;
	UINT32 inum, pnum;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	const LIBUSB_INTERFACE* interface;
	const LIBUSB_ENDPOINT_DESCEIPTOR* endpoint;
	MsInterfaces = MsConfig->MsInterfaces;
	interface = LibusbConfig->interface;

	for (inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		alt = MsInterfaces[inum]->AlternateSetting;
		endpoint = interface[inum].altsetting[alt].endpoint;

		for (pnum = 0; pnum < MsInterfaces[inum]->NumberOfPipes; pnum++)
		{
			if (endpoint[pnum].bEndpointAddress == EndpointAddress)
			{
				return &endpoint[pnum];
			}
		}
	}

	return NULL;
}

static void LIBUSB_CALL func_bulk_transfer_cb(struct libusb_transfer* transfer)
{
	ASYNC_TRANSFER_USER_DATA* user_data;
	uint32_t streamID;
	wArrayList* list;

	user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
	if (!user_data)
	{
		WLog_ERR(TAG, "[%s]: Invalid transfer->user_data!");
		return;
	}
	list = user_data->queue;
	ArrayList_Lock(list);
	streamID = stream_id_from_buffer(transfer);

	if (list_contains(list, streamID))
	{
		const UINT32 InterfaceId =
		    ((STREAM_ID_PROXY << 30) | user_data->idev->get_ReqCompletion(user_data->idev));
		const UINT32 RequestID = streamID & INTERFACE_ID_MASK;

		user_data->cb(user_data->idev, user_data->callback, user_data->data, InterfaceId,
		              user_data->noack, user_data->MessageId, RequestID, transfer->num_iso_packets,
		              transfer->status, user_data->StartFrame, user_data->ErrorCount,
		              transfer->actual_length);
		user_data->data = NULL;
		ArrayList_Remove(list, transfer);
	}
	ArrayList_Unlock(list);
}

static BOOL func_set_usbd_status(URBDRC_PLUGIN* urbdrc, UDEVICE* pdev, UINT32* status,
                                 int err_result)
{
	if (!urbdrc || !status)
		return FALSE;

	switch (err_result)
	{
		case LIBUSB_SUCCESS:
			*status = USBD_STATUS_SUCCESS;
			break;

		case LIBUSB_ERROR_IO:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_INVALID_PARAM:
			*status = USBD_STATUS_INVALID_PARAMETER;
			break;

		case LIBUSB_ERROR_ACCESS:
			*status = USBD_STATUS_NOT_ACCESSED;
			break;

		case LIBUSB_ERROR_NO_DEVICE:
			*status = USBD_STATUS_DEVICE_GONE;

			if (pdev)
			{
				if (!(pdev->status & URBDRC_DEVICE_NOT_FOUND))
					pdev->status |= URBDRC_DEVICE_NOT_FOUND;
			}

			break;

		case LIBUSB_ERROR_NOT_FOUND:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_BUSY:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_TIMEOUT:
			*status = USBD_STATUS_TIMEOUT;
			break;

		case LIBUSB_ERROR_OVERFLOW:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_PIPE:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_INTERRUPTED:
			*status = USBD_STATUS_STALL_PID;
			break;

		case LIBUSB_ERROR_NO_MEM:
			*status = USBD_STATUS_NO_MEMORY;
			break;

		case LIBUSB_ERROR_NOT_SUPPORTED:
			*status = USBD_STATUS_NOT_SUPPORTED;
			break;

		case LIBUSB_ERROR_OTHER:
			*status = USBD_STATUS_STALL_PID;
			break;

		default:
			*status = USBD_STATUS_SUCCESS;
			break;
	}

	return TRUE;
}

static int func_config_release_all_interface(URBDRC_PLUGIN* urbdrc,
                                             LIBUSB_DEVICE_HANDLE* libusb_handle,
                                             UINT32 NumInterfaces)
{
	UINT32 i;

	for (i = 0; i < NumInterfaces; i++)
	{
		int ret = libusb_release_interface(libusb_handle, i);

		if (log_libusb_result(urbdrc->log, WLOG_WARN, "libusb_release_interface", ret))
			return -1;
	}

	return 0;
}

static int func_claim_all_interface(URBDRC_PLUGIN* urbdrc, LIBUSB_DEVICE_HANDLE* libusb_handle,
                                    int NumInterfaces)
{
	int i, ret;

	for (i = 0; i < NumInterfaces; i++)
	{
		ret = libusb_claim_interface(libusb_handle, i);

		if (log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_claim_interface", ret))
			return -1;
	}

	return 0;
}

static LIBUSB_DEVICE* udev_get_libusb_dev(libusb_context* context, uint8_t bus_number,
                                          uint8_t dev_number)
{
	ssize_t i;
	LIBUSB_DEVICE** libusb_list = NULL;
	LIBUSB_DEVICE* device = NULL;
	const ssize_t total_device = libusb_get_device_list(context, &libusb_list);

	for (i = 0; i < total_device; i++)
	{
		LIBUSB_DEVICE* dev = libusb_list[i];
		if ((bus_number == libusb_get_bus_number(dev)) &&
		    (dev_number == libusb_get_device_address(dev)))
			device = dev;
		else
			libusb_unref_device(dev);
	}

	libusb_free_device_list(libusb_list, 0);
	return device;
}

static LIBUSB_DEVICE_DESCRIPTOR* udev_new_descript(URBDRC_PLUGIN* urbdrc, LIBUSB_DEVICE* libusb_dev)
{
	int ret;
	LIBUSB_DEVICE_DESCRIPTOR* descriptor =
	    (LIBUSB_DEVICE_DESCRIPTOR*)calloc(1, sizeof(LIBUSB_DEVICE_DESCRIPTOR));
	if (!descriptor)
		return NULL;
	ret = libusb_get_device_descriptor(libusb_dev, descriptor);

	if (log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_get_device_descriptor", ret))
	{
		free(descriptor);
		return NULL;
	}

	return descriptor;
}

static int libusb_udev_select_interface(IUDEVICE* idev, BYTE InterfaceNumber, BYTE AlternateSetting)
{
	int error = 0, diff = 0;
	UDEVICE* pdev = (UDEVICE*)idev;
	URBDRC_PLUGIN* urbdrc;
	MSUSB_CONFIG_DESCRIPTOR* MsConfig;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;

	if (!pdev || !pdev->urbdrc)
		return -1;

	urbdrc = pdev->urbdrc;
	MsConfig = pdev->MsConfig;

	if (MsConfig)
	{
		MsInterfaces = MsConfig->MsInterfaces;
		if (MsInterfaces)
		{
			WLog_Print(urbdrc->log, WLOG_INFO,
			           "select Interface(%" PRIu8 ") curr AlternateSetting(%" PRIu8
			           ") new AlternateSetting(" PRIu8 ")",
			           InterfaceNumber, MsInterfaces[InterfaceNumber]->AlternateSetting,
			           AlternateSetting);

			if (MsInterfaces[InterfaceNumber]->AlternateSetting != AlternateSetting)
			{
				diff = 1;
			}
		}

		if (diff)
		{
			error = libusb_set_interface_alt_setting(pdev->libusb_handle, InterfaceNumber,
			                                         AlternateSetting);

			log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_set_interface_alt_setting", error);
		}
	}

	return error;
}

static MSUSB_CONFIG_DESCRIPTOR*
libusb_udev_complete_msconfig_setup(IUDEVICE* idev, MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface;
	MSUSB_PIPE_DESCRIPTOR** MsPipes;
	MSUSB_PIPE_DESCRIPTOR* MsPipe;
	MSUSB_PIPE_DESCRIPTOR** t_MsPipes;
	MSUSB_PIPE_DESCRIPTOR* t_MsPipe;
	LIBUSB_CONFIG_DESCRIPTOR* LibusbConfig;
	const LIBUSB_INTERFACE* LibusbInterface;
	const LIBUSB_INTERFACE_DESCRIPTOR* LibusbAltsetting;
	const LIBUSB_ENDPOINT_DESCEIPTOR* LibusbEndpoint;
	BYTE LibusbNumEndpoint;
	URBDRC_PLUGIN* urbdrc;
	UINT32 inum = 0, pnum = 0, MsOutSize = 0;

	if (!pdev || !pdev->LibusbConfig || !pdev->urbdrc || !MsConfig)
		return NULL;

	urbdrc = pdev->urbdrc;
	LibusbConfig = pdev->LibusbConfig;

	if (LibusbConfig->bNumInterfaces != MsConfig->NumInterfaces)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR,
		           "Select Configuration: Libusb NumberInterfaces(%" PRIu8 ") is different "
		           "with MsConfig NumberInterfaces(%" PRIu32 ")",
		           LibusbConfig->bNumInterfaces, MsConfig->NumInterfaces);
	}

	/* replace MsPipes for libusb */
	MsInterfaces = MsConfig->MsInterfaces;

	for (inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		MsInterface = MsInterfaces[inum];
		/* get libusb's number of endpoints */
		LibusbInterface = &LibusbConfig->interface[MsInterface->InterfaceNumber];
		LibusbAltsetting = &LibusbInterface->altsetting[MsInterface->AlternateSetting];
		LibusbNumEndpoint = LibusbAltsetting->bNumEndpoints;
		t_MsPipes =
		    (MSUSB_PIPE_DESCRIPTOR**)calloc(LibusbNumEndpoint, sizeof(MSUSB_PIPE_DESCRIPTOR*));

		for (pnum = 0; pnum < LibusbNumEndpoint; pnum++)
		{
			t_MsPipe = (MSUSB_PIPE_DESCRIPTOR*)calloc(1, sizeof(MSUSB_PIPE_DESCRIPTOR));

			if (pnum < MsInterface->NumberOfPipes && MsInterface->MsPipes)
			{
				MsPipe = MsInterface->MsPipes[pnum];
				t_MsPipe->MaximumPacketSize = MsPipe->MaximumPacketSize;
				t_MsPipe->MaximumTransferSize = MsPipe->MaximumTransferSize;
				t_MsPipe->PipeFlags = MsPipe->PipeFlags;
			}
			else
			{
				t_MsPipe->MaximumPacketSize = 0;
				t_MsPipe->MaximumTransferSize = 0xffffffff;
				t_MsPipe->PipeFlags = 0;
			}

			t_MsPipe->PipeHandle = 0;
			t_MsPipe->bEndpointAddress = 0;
			t_MsPipe->bInterval = 0;
			t_MsPipe->PipeType = 0;
			t_MsPipe->InitCompleted = 0;
			t_MsPipes[pnum] = t_MsPipe;
		}

		msusb_mspipes_replace(MsInterface, t_MsPipes, LibusbNumEndpoint);
	}

	/* setup configuration */
	MsOutSize = 8;
	/* ConfigurationHandle:  4 bytes
	 * ---------------------------------------------------------------
	 * ||<<< 1 byte >>>|<<< 1 byte >>>|<<<<<<<<<< 2 byte >>>>>>>>>>>||
	 * ||  bus_number  |  dev_number  |      bConfigurationValue    ||
	 * ---------------------------------------------------------------
	 * ***********************/
	MsConfig->ConfigurationHandle =
	    MsConfig->bConfigurationValue | (pdev->bus_number << 24) | (pdev->dev_number << 16);
	MsInterfaces = MsConfig->MsInterfaces;

	for (inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		MsOutSize += 16;
		MsInterface = MsInterfaces[inum];
		/* get libusb's interface */
		LibusbInterface = &LibusbConfig->interface[MsInterface->InterfaceNumber];
		LibusbAltsetting = &LibusbInterface->altsetting[MsInterface->AlternateSetting];
		/* InterfaceHandle:  4 bytes
		 * ---------------------------------------------------------------
		 * ||<<< 1 byte >>>|<<< 1 byte >>>|<<< 1 byte >>>|<<< 1 byte >>>||
		 * ||  bus_number  |  dev_number  |  altsetting  | interfaceNum ||
		 * ---------------------------------------------------------------
		 * ***********************/
		MsInterface->InterfaceHandle = LibusbAltsetting->bInterfaceNumber |
		                               (LibusbAltsetting->bAlternateSetting << 8) |
		                               (pdev->dev_number << 16) | (pdev->bus_number << 24);
		MsInterface->Length = 16 + (MsInterface->NumberOfPipes * 20);
		MsInterface->bInterfaceClass = LibusbAltsetting->bInterfaceClass;
		MsInterface->bInterfaceSubClass = LibusbAltsetting->bInterfaceSubClass;
		MsInterface->bInterfaceProtocol = LibusbAltsetting->bInterfaceProtocol;
		MsInterface->InitCompleted = 1;
		MsPipes = MsInterface->MsPipes;
		LibusbNumEndpoint = LibusbAltsetting->bNumEndpoints;

		for (pnum = 0; pnum < LibusbNumEndpoint; pnum++)
		{
			MsOutSize += 20;
			MsPipe = MsPipes[pnum];
			/* get libusb's endpoint */
			LibusbEndpoint = &LibusbAltsetting->endpoint[pnum];
			/* PipeHandle:  4 bytes
			 * ---------------------------------------------------------------
			 * ||<<< 1 byte >>>|<<< 1 byte >>>|<<<<<<<<<< 2 byte >>>>>>>>>>>||
			 * ||  bus_number  |  dev_number  |      bEndpointAddress       ||
			 * ---------------------------------------------------------------
			 * ***********************/
			MsPipe->PipeHandle = LibusbEndpoint->bEndpointAddress | (pdev->dev_number << 16) |
			                     (pdev->bus_number << 24);
			/* count endpoint max packet size */
			int max = LibusbEndpoint->wMaxPacketSize & 0x07ff;
			BYTE attr = LibusbEndpoint->bmAttributes;

			if ((attr & 0x3) == 1 || (attr & 0x3) == 3)
			{
				max *= (1 + ((LibusbEndpoint->wMaxPacketSize >> 11) & 3));
			}

			MsPipe->MaximumPacketSize = max;
			MsPipe->bEndpointAddress = LibusbEndpoint->bEndpointAddress;
			MsPipe->bInterval = LibusbEndpoint->bInterval;
			MsPipe->PipeType = attr & 0x3;
			MsPipe->InitCompleted = 1;
		}
	}

	MsConfig->MsOutSize = MsOutSize;
	MsConfig->InitCompleted = 1;

	/* replace device's MsConfig */
	if (MsConfig != pdev->MsConfig)
	{
		msusb_msconfig_free(pdev->MsConfig);
		pdev->MsConfig = MsConfig;
	}

	return MsConfig;
}

static int libusb_udev_select_configuration(IUDEVICE* idev, UINT32 bConfigurationValue)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	MSUSB_CONFIG_DESCRIPTOR* MsConfig;
	LIBUSB_DEVICE_HANDLE* libusb_handle;
	LIBUSB_DEVICE* libusb_dev;
	URBDRC_PLUGIN* urbdrc;
	LIBUSB_CONFIG_DESCRIPTOR** LibusbConfig;
	int ret = 0;

	if (!pdev || !pdev->MsConfig || !pdev->LibusbConfig || !pdev->urbdrc)
		return -1;

	urbdrc = pdev->urbdrc;
	MsConfig = pdev->MsConfig;
	libusb_handle = pdev->libusb_handle;
	libusb_dev = pdev->libusb_dev;
	LibusbConfig = &pdev->LibusbConfig;

	if (MsConfig->InitCompleted)
	{
		func_config_release_all_interface(pdev->urbdrc, libusb_handle,
		                                  (*LibusbConfig)->bNumInterfaces);
	}

	/* The configuration value -1 is mean to put the device in unconfigured state. */
	if (bConfigurationValue == 0)
		ret = libusb_set_configuration(libusb_handle, -1);
	else
		ret = libusb_set_configuration(libusb_handle, bConfigurationValue);

	if (log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_set_configuration", ret))
	{
		func_claim_all_interface(urbdrc, libusb_handle, (*LibusbConfig)->bNumInterfaces);
		return -1;
	}
	else
	{
		ret = libusb_get_active_config_descriptor(libusb_dev, LibusbConfig);

		if (log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_set_configuration", ret))
		{
			func_claim_all_interface(urbdrc, libusb_handle, (*LibusbConfig)->bNumInterfaces);
			return -1;
		}
	}

	func_claim_all_interface(urbdrc, libusb_handle, (*LibusbConfig)->bNumInterfaces);
	return 0;
}

static int libusb_udev_control_pipe_request(IUDEVICE* idev, UINT32 RequestId,
                                            UINT32 EndpointAddress, UINT32* UsbdStatus, int command)
{
	int error = 0;
	UDEVICE* pdev = (UDEVICE*)idev;

	/*
	pdev->request_queue->register_request(pdev->request_queue, RequestId, NULL, 0);
	*/
	switch (command)
	{
		case PIPE_CANCEL:
			/** cancel bulk or int transfer */
			idev->cancel_all_transfer_request(idev);
			// dummy_wait_s_obj(1);
			/** set feature to ep (set halt)*/
			error = libusb_control_transfer(
			    pdev->libusb_handle, LIBUSB_ENDPOINT_OUT | LIBUSB_RECIPIENT_ENDPOINT,
			    LIBUSB_REQUEST_SET_FEATURE, ENDPOINT_HALT, EndpointAddress, NULL, 0, 1000);
			break;

		case PIPE_RESET:
			idev->cancel_all_transfer_request(idev);
			error = libusb_clear_halt(pdev->libusb_handle, EndpointAddress);
			// func_set_usbd_status(pdev, UsbdStatus, error);
			break;

		default:
			error = -0xff;
			break;
	}

	*UsbdStatus = 0;
	return error;
}

static UINT32 libusb_udev_control_query_device_text(IUDEVICE* idev, UINT32 TextType,
                                                    UINT16 LocaleId, UINT8* BufferSize,
                                                    BYTE* Buffer)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	LIBUSB_DEVICE_DESCRIPTOR* devDescriptor;
	const char strDesc[] = "Generic Usb String";
	char deviceLocation[25] = { 0 };
	BYTE bus_number;
	BYTE device_address;
	int ret = 0;
	size_t i, len;
	URBDRC_PLUGIN* urbdrc;
	WCHAR* text = (WCHAR*)Buffer;
	BYTE slen, locale;
	const UINT8 inSize = *BufferSize;

	*BufferSize = 0;
	if (!pdev || !pdev->devDescriptor || !pdev->urbdrc)
		return ERROR_INVALID_DATA;

	urbdrc = pdev->urbdrc;
	devDescriptor = pdev->devDescriptor;

	switch (TextType)
	{
		case DeviceTextDescription:
		{
			BYTE data[0x100] = { 0 };
			ret = libusb_get_string_descriptor(pdev->libusb_handle, devDescriptor->iProduct,
			                                   LocaleId, data, 0xFF);
			/* The returned data in the buffer is:
			 * 1 byte  length of following data
			 * 1 byte  descriptor type, must be 0x03 for strings
			 * n WCHAR unicode string (of length / 2 characters) including '\0'
			 */
			slen = data[0];
			locale = data[1];

			if ((ret <= 0) || (ret <= 4) || (slen <= 4) || (locale != LIBUSB_DT_STRING) ||
			    (ret > UINT8_MAX))
			{
				const char* msg = "SHORT_DESCRIPTOR";
				if (ret < 0)
					msg = libusb_error_name(ret);
				WLog_Print(urbdrc->log, WLOG_DEBUG,
				           "libusb_get_string_descriptor: "
				           "%s [%d], iProduct: %" PRIu8 "!",
				           msg, ret, devDescriptor->iProduct);

				len = MIN(sizeof(strDesc), inSize);
				for (i = 0; i < len; i++)
					text[i] = (WCHAR)strDesc[i];

				*BufferSize = (BYTE)(len * 2);
			}
			else
			{
				/* ret and slen should be equals, but you never know creativity
				 * of device manufacturers...
				 * So also check the string length returned as server side does
				 * not honor strings with multi '\0' characters well.
				 */
				const size_t rchar = _wcsnlen((WCHAR*)&data[2], sizeof(data) / 2);
				len = MIN((BYTE)ret, slen);
				len = MIN(len, inSize);
				len = MIN(len, rchar * 2 + sizeof(WCHAR));
				memcpy(Buffer, &data[2], len);

				/* Just as above, the returned WCHAR string should be '\0'
				 * terminated, but never trust hardware to conform to specs... */
				Buffer[len - 2] = '\0';
				Buffer[len - 1] = '\0';
				*BufferSize = (BYTE)len;
			}
		}
		break;

		case DeviceTextLocationInformation:
			bus_number = libusb_get_bus_number(pdev->libusb_dev);
			device_address = libusb_get_device_address(pdev->libusb_dev);
			sprintf_s(deviceLocation, sizeof(deviceLocation),
			          "Port_#%04" PRIu8 ".Hub_#%04" PRIu8 "", device_address, bus_number);

			len = strnlen(deviceLocation,
			              MIN(sizeof(deviceLocation), (inSize > 0) ? inSize - 1U : 0));
			for (i = 0; i < len; i++)
				text[i] = (WCHAR)deviceLocation[i];
			text[len++] = '\0';
			*BufferSize = (UINT8)(len * sizeof(WCHAR));
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG, "Query Text: unknown TextType %" PRIu32 "",
			           TextType);
			return ERROR_INVALID_DATA;
	}

	return S_OK;
}

static int libusb_udev_os_feature_descriptor_request(IUDEVICE* idev, UINT32 RequestId,
                                                     BYTE Recipient, BYTE InterfaceNumber,
                                                     BYTE Ms_PageIndex, UINT16 Ms_featureDescIndex,
                                                     UINT32* UsbdStatus, UINT32* BufferSize,
                                                     BYTE* Buffer, UINT32 Timeout)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	BYTE ms_string_desc[0x13] = { 0 };
	int error = 0;

	WINPR_ASSERT(idev);
	WINPR_ASSERT(UsbdStatus);
	WINPR_ASSERT(BufferSize);
	WINPR_ASSERT(*BufferSize <= UINT16_MAX);

	/*
	pdev->request_queue->register_request(pdev->request_queue, RequestId, NULL, 0);
	*/
	error = libusb_control_transfer(pdev->libusb_handle, LIBUSB_ENDPOINT_IN | Recipient,
	                                LIBUSB_REQUEST_GET_DESCRIPTOR, 0x03ee, 0, ms_string_desc, 0x12,
	                                Timeout);

	log_libusb_result(pdev->urbdrc->log, WLOG_DEBUG, "libusb_control_transfer", error);

	if (error > 0)
	{
		const BYTE bMS_Vendorcode = ms_string_desc[16];
		/** get os descriptor */
		error = libusb_control_transfer(
		    pdev->libusb_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | Recipient,
		    bMS_Vendorcode, (UINT16)((InterfaceNumber << 8) | Ms_PageIndex), Ms_featureDescIndex,
		    Buffer, (UINT16)*BufferSize, Timeout);
		log_libusb_result(pdev->urbdrc->log, WLOG_DEBUG, "libusb_control_transfer", error);

		if (error >= 0)
			*BufferSize = (UINT32)error;
	}

	if (error < 0)
		*UsbdStatus = USBD_STATUS_STALL_PID;
	else
		*UsbdStatus = USBD_STATUS_SUCCESS;

	return ERROR_SUCCESS;
}

static int libusb_udev_query_device_descriptor(IUDEVICE* idev, int offset)
{
	UDEVICE* pdev = (UDEVICE*)idev;

	switch (offset)
	{
		case B_LENGTH:
			return pdev->devDescriptor->bLength;

		case B_DESCRIPTOR_TYPE:
			return pdev->devDescriptor->bDescriptorType;

		case BCD_USB:
			return pdev->devDescriptor->bcdUSB;

		case B_DEVICE_CLASS:
			return pdev->devDescriptor->bDeviceClass;

		case B_DEVICE_SUBCLASS:
			return pdev->devDescriptor->bDeviceSubClass;

		case B_DEVICE_PROTOCOL:
			return pdev->devDescriptor->bDeviceProtocol;

		case B_MAX_PACKET_SIZE0:
			return pdev->devDescriptor->bMaxPacketSize0;

		case ID_VENDOR:
			return pdev->devDescriptor->idVendor;

		case ID_PRODUCT:
			return pdev->devDescriptor->idProduct;

		case BCD_DEVICE:
			return pdev->devDescriptor->bcdDevice;

		case I_MANUFACTURER:
			return pdev->devDescriptor->iManufacturer;

		case I_PRODUCT:
			return pdev->devDescriptor->iProduct;

		case I_SERIAL_NUMBER:
			return pdev->devDescriptor->iSerialNumber;

		case B_NUM_CONFIGURATIONS:
			return pdev->devDescriptor->bNumConfigurations;

		default:
			return 0;
	}
}

static BOOL libusb_udev_detach_kernel_driver(IUDEVICE* idev)
{
	int i, err = 0;
	UDEVICE* pdev = (UDEVICE*)idev;
	URBDRC_PLUGIN* urbdrc;

	if (!pdev || !pdev->LibusbConfig || !pdev->libusb_handle || !pdev->urbdrc)
		return FALSE;

#ifdef _WIN32
	return TRUE;
#else
	urbdrc = pdev->urbdrc;

	if ((pdev->status & URBDRC_DEVICE_DETACH_KERNEL) == 0)
	{
		for (i = 0; i < pdev->LibusbConfig->bNumInterfaces; i++)
		{
			err = libusb_kernel_driver_active(pdev->libusb_handle, i);
			log_libusb_result(urbdrc->log, WLOG_DEBUG, "libusb_kernel_driver_active", err);

			if (err)
			{
				err = libusb_detach_kernel_driver(pdev->libusb_handle, i);
				log_libusb_result(urbdrc->log, WLOG_DEBUG, "libusb_detach_kernel_driver", err);
			}
		}

		pdev->status |= URBDRC_DEVICE_DETACH_KERNEL;
	}

	return TRUE;
#endif
}

static BOOL libusb_udev_attach_kernel_driver(IUDEVICE* idev)
{
	int i, err = 0;
	UDEVICE* pdev = (UDEVICE*)idev;

	if (!pdev || !pdev->LibusbConfig || !pdev->libusb_handle || !pdev->urbdrc)
		return FALSE;

	for (i = 0; i < pdev->LibusbConfig->bNumInterfaces && err != LIBUSB_ERROR_NO_DEVICE; i++)
	{
		err = libusb_release_interface(pdev->libusb_handle, i);

		log_libusb_result(pdev->urbdrc->log, WLOG_DEBUG, "libusb_release_interface", err);

#ifndef _WIN32
		if (err != LIBUSB_ERROR_NO_DEVICE)
		{
			err = libusb_attach_kernel_driver(pdev->libusb_handle, i);
			log_libusb_result(pdev->urbdrc->log, WLOG_DEBUG, "libusb_attach_kernel_driver if=%d",
			                  err, i);
		}
#endif
	}

	return TRUE;
}

static int libusb_udev_is_composite_device(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	return pdev->isCompositeDevice;
}

static int libusb_udev_is_exist(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	return (pdev->status & URBDRC_DEVICE_NOT_FOUND) ? 0 : 1;
}

static int libusb_udev_is_channel_closed(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	IUDEVMAN* udevman;
	if (!pdev || !pdev->urbdrc)
		return 1;

	udevman = pdev->urbdrc->udevman;
	if (udevman)
	{
		if (udevman->status & URBDRC_DEVICE_CHANNEL_CLOSED)
			return 1;
	}

	if (pdev->status & URBDRC_DEVICE_CHANNEL_CLOSED)
		return 1;

	return 0;
}

static int libusb_udev_is_already_send(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	return (pdev->status & URBDRC_DEVICE_ALREADY_SEND) ? 1 : 0;
}

/* This is called from channel cleanup code.
 * Avoid double free, just remove the device and mark the channel closed. */
static void libusb_udev_mark_channel_closed(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	if (pdev && ((pdev->status & URBDRC_DEVICE_CHANNEL_CLOSED) == 0))
	{
		URBDRC_PLUGIN* urbdrc = pdev->urbdrc;
		const uint8_t busNr = idev->get_bus_number(idev);
		const uint8_t devNr = idev->get_dev_number(idev);

		pdev->status |= URBDRC_DEVICE_CHANNEL_CLOSED;
		urbdrc->udevman->unregister_udevice(urbdrc->udevman, busNr, devNr);
	}
}

/* This is called by local events where the device is removed or in an error
 * state. Remove the device from redirection and close the channel. */
static void libusb_udev_channel_closed(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	if (pdev && ((pdev->status & URBDRC_DEVICE_CHANNEL_CLOSED) == 0))
	{
		URBDRC_PLUGIN* urbdrc = pdev->urbdrc;
		const uint8_t busNr = idev->get_bus_number(idev);
		const uint8_t devNr = idev->get_dev_number(idev);
		IWTSVirtualChannel* channel = NULL;

		if (pdev->channelManager)
			channel = IFCALLRESULT(NULL, pdev->channelManager->FindChannelById,
			                       pdev->channelManager, pdev->channelID);

		pdev->status |= URBDRC_DEVICE_CHANNEL_CLOSED;

		if (channel)
			channel->Write(channel, 0, NULL, NULL);

		urbdrc->udevman->unregister_udevice(urbdrc->udevman, busNr, devNr);
	}
}

static void libusb_udev_set_already_send(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	pdev->status |= URBDRC_DEVICE_ALREADY_SEND;
}

static char* libusb_udev_get_path(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	return pdev->path;
}

static int libusb_udev_query_device_port_status(IUDEVICE* idev, UINT32* UsbdStatus,
                                                UINT32* BufferSize, BYTE* Buffer)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	int success = 0, ret;
	URBDRC_PLUGIN* urbdrc;

	if (!pdev || !pdev->urbdrc)
		return -1;

	urbdrc = pdev->urbdrc;

	if (pdev->hub_handle != NULL)
	{
		ret = idev->control_transfer(
		    idev, 0xffff, 0, 0,
		    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER,
		    LIBUSB_REQUEST_GET_STATUS, 0, pdev->port_number, UsbdStatus, BufferSize, Buffer, 1000);

		if (log_libusb_result(urbdrc->log, WLOG_DEBUG, "libusb_control_transfer", ret))
			*BufferSize = 0;
		else
		{
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "PORT STATUS:0x%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "", Buffer[3],
			           Buffer[2], Buffer[1], Buffer[0]);
			success = 1;
		}
	}

	return success;
}

static int libusb_udev_isoch_transfer(IUDEVICE* idev, GENERIC_CHANNEL_CALLBACK* callback,
                                      UINT32 MessageId, UINT32 RequestId, UINT32 EndpointAddress,
                                      UINT32 TransferFlags, UINT32 StartFrame, UINT32 ErrorCount,
                                      BOOL NoAck, const BYTE* packetDescriptorData,
                                      UINT32 NumberOfPackets, UINT32 BufferSize, const BYTE* Buffer,
                                      t_isoch_transfer_cb cb, UINT32 Timeout)
{
	int rc;
	UINT32 iso_packet_size = 0;
	UDEVICE* pdev = (UDEVICE*)idev;
	ASYNC_TRANSFER_USER_DATA* user_data;
	struct libusb_transfer* iso_transfer = NULL;
	URBDRC_PLUGIN* urbdrc;
	size_t outSize = (NumberOfPackets * 12);
	uint32_t streamID = 0x40000000 | RequestId;

	if (!pdev || !pdev->urbdrc)
		return -1;

	urbdrc = pdev->urbdrc;
	user_data = async_transfer_user_data_new(idev, MessageId, 48, BufferSize, Buffer,
	                                         outSize + 1024, NoAck, cb, callback);

	if (!user_data)
		return -1;

	user_data->ErrorCount = ErrorCount;
	user_data->StartFrame = StartFrame;

	if (!Buffer)
		Stream_Seek(user_data->data, (NumberOfPackets * 12));

	if (NumberOfPackets > 0)
	{
		iso_packet_size = BufferSize / NumberOfPackets;
		iso_transfer = libusb_alloc_transfer((int)NumberOfPackets);
	}

	if (iso_transfer == NULL)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR,
		           "Error: libusb_alloc_transfer [NumberOfPackets=%" PRIu32 ", BufferSize=%" PRIu32
		           " ]",
		           NumberOfPackets, BufferSize);
		async_transfer_user_data_free(user_data);
		return -1;
	}

	/**  process URB_FUNCTION_IOSCH_TRANSFER */
	libusb_fill_iso_transfer(iso_transfer, pdev->libusb_handle, EndpointAddress,
	                         Stream_Pointer(user_data->data), BufferSize, NumberOfPackets,
	                         func_iso_callback, user_data, Timeout);
	set_stream_id_for_buffer(iso_transfer, streamID);
	libusb_set_iso_packet_lengths(iso_transfer, iso_packet_size);

	if (!ArrayList_Append(pdev->request_queue, iso_transfer))
	{
		WLog_Print(urbdrc->log, WLOG_WARN,
		           "Failed to queue iso transfer, streamID %08" PRIx32 " already in use!",
		           streamID);
		request_free(iso_transfer);
		return -1;
	}
	rc = libusb_submit_transfer(iso_transfer);
	if (log_libusb_result(urbdrc->log, WLOG_ERROR, "", rc))
		return -1;
	return rc;
}

static BOOL libusb_udev_control_transfer(IUDEVICE* idev, UINT32 RequestId, UINT32 EndpointAddress,
                                         UINT32 TransferFlags, BYTE bmRequestType, BYTE Request,
                                         UINT16 Value, UINT16 Index, UINT32* UrbdStatus,
                                         UINT32* BufferSize, BYTE* Buffer, UINT32 Timeout)
{
	int status = 0;
	UDEVICE* pdev = (UDEVICE*)idev;

	WINPR_ASSERT(BufferSize);
	WINPR_ASSERT(*BufferSize <= UINT16_MAX);

	if (!pdev || !pdev->urbdrc)
		return FALSE;

	status = libusb_control_transfer(pdev->libusb_handle, bmRequestType, Request, Value, Index,
	                                 Buffer, (UINT16)*BufferSize, Timeout);

	if (status >= 0)
		*BufferSize = (UINT32)status;
	else
		log_libusb_result(pdev->urbdrc->log, WLOG_ERROR, "libusb_control_transfer", status);

	if (!func_set_usbd_status(pdev->urbdrc, pdev, UrbdStatus, status))
		return FALSE;

	return TRUE;
}

static int libusb_udev_bulk_or_interrupt_transfer(IUDEVICE* idev,
                                                  GENERIC_CHANNEL_CALLBACK* callback,
                                                  UINT32 MessageId, UINT32 RequestId,
                                                  UINT32 EndpointAddress, UINT32 TransferFlags,
                                                  BOOL NoAck, UINT32 BufferSize, const BYTE* data,
                                                  t_isoch_transfer_cb cb, UINT32 Timeout)
{
	int rc;
	UINT32 transfer_type;
	UDEVICE* pdev = (UDEVICE*)idev;
	const LIBUSB_ENDPOINT_DESCEIPTOR* ep_desc;
	struct libusb_transfer* transfer = NULL;
	URBDRC_PLUGIN* urbdrc;
	ASYNC_TRANSFER_USER_DATA* user_data;
	uint32_t streamID = 0x80000000 | RequestId;

	if (!pdev || !pdev->LibusbConfig || !pdev->urbdrc)
		return -1;

	urbdrc = pdev->urbdrc;
	user_data =
	    async_transfer_user_data_new(idev, MessageId, 36, BufferSize, data, 0, NoAck, cb, callback);

	if (!user_data)
		return -1;

	/* alloc memory for urb transfer */
	transfer = libusb_alloc_transfer(0);
	if (!transfer)
	{
		async_transfer_user_data_free(user_data);
		return -1;
	}

	ep_desc = func_get_ep_desc(pdev->LibusbConfig, pdev->MsConfig, EndpointAddress);

	if (!ep_desc)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "func_get_ep_desc: endpoint 0x%" PRIx32 " not found",
		           EndpointAddress);
		request_free(transfer);
		return -1;
	}

	transfer_type = (ep_desc->bmAttributes) & 0x3;
	WLog_Print(urbdrc->log, WLOG_DEBUG,
	           "urb_bulk_or_interrupt_transfer: ep:0x%" PRIx32 " "
	           "transfer_type %" PRIu32 " flag:%" PRIu32 " OutputBufferSize:0x%" PRIx32 "",
	           EndpointAddress, transfer_type, TransferFlags, BufferSize);

	switch (transfer_type)
	{
		case BULK_TRANSFER:
			/** Bulk Transfer */
			libusb_fill_bulk_transfer(transfer, pdev->libusb_handle, EndpointAddress,
			                          Stream_Pointer(user_data->data), BufferSize,
			                          func_bulk_transfer_cb, user_data, Timeout);
			break;

		case INTERRUPT_TRANSFER:
			/**  Interrupt Transfer */
			libusb_fill_interrupt_transfer(transfer, pdev->libusb_handle, EndpointAddress,
			                               Stream_Pointer(user_data->data), BufferSize,
			                               func_bulk_transfer_cb, user_data, Timeout);
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "urb_bulk_or_interrupt_transfer:"
			           " other transfer type 0x%" PRIX32 "",
			           transfer_type);
			request_free(transfer);
			return -1;
	}

	set_stream_id_for_buffer(transfer, streamID);

	if (!ArrayList_Append(pdev->request_queue, transfer))
	{
		WLog_Print(urbdrc->log, WLOG_WARN,
		           "Failed to queue transfer, streamID %08" PRIx32 " already in use!", streamID);
		request_free(transfer);
		return -1;
	}
	rc = libusb_submit_transfer(transfer);
	if (log_libusb_result(urbdrc->log, WLOG_ERROR, "", rc))
		return -1;
	return rc;
}

static int func_cancel_xact_request(URBDRC_PLUGIN* urbdrc, struct libusb_transfer* transfer)
{
	int status;

	if (!urbdrc || !transfer)
		return -1;

	status = libusb_cancel_transfer(transfer);

	if (log_libusb_result(urbdrc->log, WLOG_WARN, "libusb_cancel_transfer", status))
	{
		if (status == LIBUSB_ERROR_NOT_FOUND)
			return -1;
	}
	else
		return 1;

	return 0;
}

static void libusb_udev_cancel_all_transfer_request(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;
	size_t count, x;

	if (!pdev || !pdev->request_queue || !pdev->urbdrc)
		return;

	ArrayList_Lock(pdev->request_queue);
	count = ArrayList_Count(pdev->request_queue);

	for (x = 0; x < count; x++)
	{
		struct libusb_transfer* transfer = ArrayList_GetItem(pdev->request_queue, x);
		func_cancel_xact_request(pdev->urbdrc, transfer);
	}

	ArrayList_Unlock(pdev->request_queue);
}

static int libusb_udev_cancel_transfer_request(IUDEVICE* idev, UINT32 RequestId)
{
	int rc = -1;
	UDEVICE* pdev = (UDEVICE*)idev;
	struct libusb_transfer* transfer;
	uint32_t cancelID1 = 0x40000000 | RequestId;
	uint32_t cancelID2 = 0x80000000 | RequestId;

	if (!idev || !pdev->urbdrc || !pdev->request_queue)
		return -1;

	ArrayList_Lock(pdev->request_queue);
	transfer = list_contains(pdev->request_queue, cancelID1);
	if (!transfer)
		transfer = list_contains(pdev->request_queue, cancelID2);

	if (transfer)
	{
		URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)pdev->urbdrc;

		rc = func_cancel_xact_request(urbdrc, transfer);
	}
	ArrayList_Unlock(pdev->request_queue);
	return rc;
}

BASIC_STATE_FUNC_DEFINED(channelManager, IWTSVirtualChannelManager*)
BASIC_STATE_FUNC_DEFINED(channelID, UINT32)
BASIC_STATE_FUNC_DEFINED(ReqCompletion, UINT32)
BASIC_STATE_FUNC_DEFINED(bus_number, BYTE)
BASIC_STATE_FUNC_DEFINED(dev_number, BYTE)
BASIC_STATE_FUNC_DEFINED(port_number, int)
BASIC_STATE_FUNC_DEFINED(MsConfig, MSUSB_CONFIG_DESCRIPTOR*)

BASIC_POINT_FUNC_DEFINED(udev, void*)
BASIC_POINT_FUNC_DEFINED(prev, void*)
BASIC_POINT_FUNC_DEFINED(next, void*)

static UINT32 udev_get_UsbDevice(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*)idev;

	if (!pdev)
		return 0;

	return pdev->UsbDevice;
}

static void udev_set_UsbDevice(IUDEVICE* idev, UINT32 val)
{
	UDEVICE* pdev = (UDEVICE*)idev;

	if (!pdev)
		return;

	pdev->UsbDevice = val;
}

static void udev_free(IUDEVICE* idev)
{
	int rc;
	UDEVICE* udev = (UDEVICE*)idev;
	URBDRC_PLUGIN* urbdrc;

	if (!idev || !udev->urbdrc)
		return;

	urbdrc = udev->urbdrc;

	libusb_udev_cancel_all_transfer_request(&udev->iface);
	if (udev->libusb_handle)
	{
		rc = libusb_reset_device(udev->libusb_handle);

		log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_reset_device", rc);
	}

	/* HACK: We need to wait until the cancel transfer has been processed by
	 * poll_libusb_events
	 */
	Sleep(100);

	/* release all interface and  attach kernel driver */
	udev->iface.attach_kernel_driver(idev);
	ArrayList_Free(udev->request_queue);
	/* free the config descriptor that send from windows */
	msusb_msconfig_free(udev->MsConfig);
	libusb_unref_device(udev->libusb_dev);
	libusb_close(udev->libusb_handle);
	libusb_close(udev->hub_handle);
	free(udev->devDescriptor);
	free(idev);
}

static void udev_load_interface(UDEVICE* pdev)
{
	WINPR_ASSERT(pdev);

	/* load interface */
	/* Basic */
	BASIC_STATE_FUNC_REGISTER(channelManager, pdev);
	BASIC_STATE_FUNC_REGISTER(channelID, pdev);
	BASIC_STATE_FUNC_REGISTER(UsbDevice, pdev);
	BASIC_STATE_FUNC_REGISTER(ReqCompletion, pdev);
	BASIC_STATE_FUNC_REGISTER(bus_number, pdev);
	BASIC_STATE_FUNC_REGISTER(dev_number, pdev);
	BASIC_STATE_FUNC_REGISTER(port_number, pdev);
	BASIC_STATE_FUNC_REGISTER(MsConfig, pdev);
	BASIC_STATE_FUNC_REGISTER(p_udev, pdev);
	BASIC_STATE_FUNC_REGISTER(p_prev, pdev);
	BASIC_STATE_FUNC_REGISTER(p_next, pdev);
	pdev->iface.isCompositeDevice = libusb_udev_is_composite_device;
	pdev->iface.isExist = libusb_udev_is_exist;
	pdev->iface.isAlreadySend = libusb_udev_is_already_send;
	pdev->iface.isChannelClosed = libusb_udev_is_channel_closed;
	pdev->iface.setAlreadySend = libusb_udev_set_already_send;
	pdev->iface.setChannelClosed = libusb_udev_channel_closed;
	pdev->iface.markChannelClosed = libusb_udev_mark_channel_closed;
	pdev->iface.getPath = libusb_udev_get_path;
	/* Transfer */
	pdev->iface.isoch_transfer = libusb_udev_isoch_transfer;
	pdev->iface.control_transfer = libusb_udev_control_transfer;
	pdev->iface.bulk_or_interrupt_transfer = libusb_udev_bulk_or_interrupt_transfer;
	pdev->iface.select_interface = libusb_udev_select_interface;
	pdev->iface.select_configuration = libusb_udev_select_configuration;
	pdev->iface.complete_msconfig_setup = libusb_udev_complete_msconfig_setup;
	pdev->iface.control_pipe_request = libusb_udev_control_pipe_request;
	pdev->iface.control_query_device_text = libusb_udev_control_query_device_text;
	pdev->iface.os_feature_descriptor_request = libusb_udev_os_feature_descriptor_request;
	pdev->iface.cancel_all_transfer_request = libusb_udev_cancel_all_transfer_request;
	pdev->iface.cancel_transfer_request = libusb_udev_cancel_transfer_request;
	pdev->iface.query_device_descriptor = libusb_udev_query_device_descriptor;
	pdev->iface.detach_kernel_driver = libusb_udev_detach_kernel_driver;
	pdev->iface.attach_kernel_driver = libusb_udev_attach_kernel_driver;
	pdev->iface.query_device_port_status = libusb_udev_query_device_port_status;
	pdev->iface.free = udev_free;
}

static int udev_get_device_handle(URBDRC_PLUGIN* urbdrc, libusb_context* ctx, UDEVICE* pdev,
                                  UINT16 bus_number, UINT16 dev_number)
{
	int error = -1;
	ssize_t i;
	uint8_t port_numbers[16] = { 0 };
	LIBUSB_DEVICE** libusb_list = NULL;
	const ssize_t total_device = libusb_get_device_list(ctx, &libusb_list);

	WINPR_ASSERT(urbdrc);

	/* Look for device. */
	for (i = 0; i < total_device; i++)
	{
		LIBUSB_DEVICE* dev = libusb_list[i];

		if ((bus_number != libusb_get_bus_number(dev)) ||
		    (dev_number != libusb_get_device_address(dev)))
			libusb_unref_device(dev);
		else
		{
			error = libusb_open(dev, &pdev->libusb_handle);

			if (log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_open", error))
			{
				libusb_unref_device(dev);
				continue;
			}

			/* get port number */
			error = libusb_get_port_numbers(dev, port_numbers, sizeof(port_numbers));
			if (error < 1)
			{
				/* Prevent open hub, treat as error. */
				log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_get_port_numbers", error);
				libusb_unref_device(dev);
				continue;
			}

			pdev->port_number = port_numbers[(error - 1)];
			error = 0;
			WLog_Print(urbdrc->log, WLOG_DEBUG, "  Port: %d", pdev->port_number);
			/* gen device path */
			sprintf(pdev->path, "%" PRIu16 "-%d", bus_number, pdev->port_number);

			WLog_Print(urbdrc->log, WLOG_DEBUG, "  DevPath: %s", pdev->path);
		}
	}
	libusb_free_device_list(libusb_list, 0);

	if (error < 0)
		return -1;
	return 0;
}

static int udev_get_hub_handle(URBDRC_PLUGIN* urbdrc, libusb_context* ctx, UDEVICE* pdev,
                               UINT16 bus_number, UINT16 dev_number)
{
	int error = -1;
	ssize_t i;
	LIBUSB_DEVICE** libusb_list = NULL;
	LIBUSB_DEVICE_HANDLE* handle = NULL;
	const ssize_t total_device = libusb_get_device_list(ctx, &libusb_list);

	WINPR_ASSERT(urbdrc);

	/* Look for device hub. */
	for (i = 0; i < total_device; i++)
	{
		LIBUSB_DEVICE* dev = libusb_list[i];

		if ((bus_number != libusb_get_bus_number(dev)) ||
		    (1 != libusb_get_device_address(dev))) /* Root hub allways first on bus. */
			libusb_unref_device(dev);
		else
		{
			WLog_Print(urbdrc->log, WLOG_DEBUG, "  Open hub: %" PRIu16 "", bus_number);
			error = libusb_open(dev, &handle);

			if (!log_libusb_result(urbdrc->log, WLOG_ERROR, "libusb_open", error))
				pdev->hub_handle = handle;
			else
				libusb_unref_device(dev);
		}
	}

	libusb_free_device_list(libusb_list, 0);

	if (error < 0)
		return -1;

	return 0;
}

static void request_free(void* value)
{
	ASYNC_TRANSFER_USER_DATA* user_data;
	struct libusb_transfer* transfer = (struct libusb_transfer*)value;
	if (!transfer)
		return;

	user_data = (ASYNC_TRANSFER_USER_DATA*)transfer->user_data;
	async_transfer_user_data_free(user_data);
	transfer->user_data = NULL;
	libusb_free_transfer(transfer);
}

static IUDEVICE* udev_init(URBDRC_PLUGIN* urbdrc, libusb_context* context, LIBUSB_DEVICE* device,
                           BYTE bus_number, BYTE dev_number)
{
	UDEVICE* pdev;
	int status = LIBUSB_ERROR_OTHER;
	LIBUSB_DEVICE_DESCRIPTOR* devDescriptor;
	LIBUSB_CONFIG_DESCRIPTOR* config_temp;
	LIBUSB_INTERFACE_DESCRIPTOR interface_temp;

	WINPR_ASSERT(urbdrc);

	pdev = (PUDEVICE)calloc(1, sizeof(UDEVICE));

	if (!pdev)
		return NULL;

	pdev->urbdrc = urbdrc;
	udev_load_interface(pdev);

	if (device)
		pdev->libusb_dev = device;
	else
		pdev->libusb_dev = udev_get_libusb_dev(context, bus_number, dev_number);

	if (pdev->libusb_dev == NULL)
		goto fail;

	if (urbdrc->listener_callback)
		udev_set_channelManager(&pdev->iface, urbdrc->listener_callback->channel_mgr);

	/* Get DEVICE handle */
	status = udev_get_device_handle(urbdrc, context, pdev, bus_number, dev_number);
	if (status != LIBUSB_SUCCESS)
	{
		struct libusb_device_descriptor desc;
		const uint8_t port = libusb_get_port_number(pdev->libusb_dev);
		libusb_get_device_descriptor(pdev->libusb_dev, &desc);

		log_libusb_result(urbdrc->log, WLOG_ERROR,
		                  "libusb_open [b=0x%02X,p=0x%02X,a=0x%02X,VID=0x%04X,PID=0x%04X]", status,
		                  bus_number, port, dev_number, desc.idVendor, desc.idProduct);
		goto fail;
	}

	/* Get HUB handle */
	status = udev_get_hub_handle(urbdrc, context, pdev, bus_number, dev_number);

	if (status < 0)
		pdev->hub_handle = NULL;

	pdev->devDescriptor = udev_new_descript(urbdrc, pdev->libusb_dev);

	if (!pdev->devDescriptor)
		goto fail;

	status = libusb_get_active_config_descriptor(pdev->libusb_dev, &pdev->LibusbConfig);

	if (status == LIBUSB_ERROR_NOT_FOUND)
		status = libusb_get_config_descriptor(pdev->libusb_dev, 0, &pdev->LibusbConfig);

	if (status < 0)
		goto fail;

	config_temp = pdev->LibusbConfig;
	/* get the first interface and first altsetting */
	interface_temp = config_temp->interface[0].altsetting[0];
	WLog_Print(urbdrc->log, WLOG_DEBUG,
	           "Registered Device: Vid: 0x%04" PRIX16 " Pid: 0x%04" PRIX16 ""
	           " InterfaceClass = %s",
	           pdev->devDescriptor->idVendor, pdev->devDescriptor->idProduct,
	           usb_interface_class_to_string(interface_temp.bInterfaceClass));
	/* Check composite device */
	devDescriptor = pdev->devDescriptor;

	if ((devDescriptor->bNumConfigurations == 1) && (config_temp->bNumInterfaces > 1) &&
	    (devDescriptor->bDeviceClass == LIBUSB_CLASS_PER_INTERFACE))
	{
		pdev->isCompositeDevice = 1;
	}
	else if ((devDescriptor->bDeviceClass == 0xef) &&
	         (devDescriptor->bDeviceSubClass == LIBUSB_CLASS_COMM) &&
	         (devDescriptor->bDeviceProtocol == 0x01))
	{
		pdev->isCompositeDevice = 1;
	}
	else
		pdev->isCompositeDevice = 0;

	/* set device class to first interface class */
	devDescriptor->bDeviceClass = interface_temp.bInterfaceClass;
	devDescriptor->bDeviceSubClass = interface_temp.bInterfaceSubClass;
	devDescriptor->bDeviceProtocol = interface_temp.bInterfaceProtocol;
	/* initialize pdev */
	pdev->bus_number = bus_number;
	pdev->dev_number = dev_number;
	pdev->request_queue = ArrayList_New(TRUE);

	if (!pdev->request_queue)
		goto fail;

	ArrayList_Object(pdev->request_queue)->fnObjectFree = request_free;

	/* set config of windows */
	pdev->MsConfig = msusb_msconfig_new();

	if (!pdev->MsConfig)
		goto fail;

	// deb_config_msg(pdev->libusb_dev, config_temp, devDescriptor->bNumConfigurations);
	return &pdev->iface;
fail:
	pdev->iface.free(&pdev->iface);
	return NULL;
}

size_t udev_new_by_id(URBDRC_PLUGIN* urbdrc, libusb_context* ctx, UINT16 idVendor, UINT16 idProduct,
                      IUDEVICE*** devArray)
{
	LIBUSB_DEVICE** libusb_list;
	UDEVICE** array;
	ssize_t i, total_device;
	size_t num = 0;

	if (!urbdrc || !devArray)
		return 0;

	WLog_Print(urbdrc->log, WLOG_INFO, "VID: 0x%04" PRIX16 ", PID: 0x%04" PRIX16 "", idVendor,
	           idProduct);
	total_device = libusb_get_device_list(ctx, &libusb_list);
	if (total_device < 0)
		return 0;

	array = (UDEVICE**)calloc((size_t)total_device, sizeof(UDEVICE*));

	if (!array)
		goto fail;

	for (i = 0; i < total_device; i++)
	{
		LIBUSB_DEVICE* dev = libusb_list[i];
		LIBUSB_DEVICE_DESCRIPTOR* descriptor = udev_new_descript(urbdrc, dev);

		if ((descriptor->idVendor == idVendor) && (descriptor->idProduct == idProduct))
		{
			array[num] = (PUDEVICE)udev_init(urbdrc, ctx, dev, libusb_get_bus_number(dev),
			                                 libusb_get_device_address(dev));

			if (array[num] != NULL)
				num++;
		}
		else
			libusb_unref_device(dev);

		free(descriptor);
	}

fail:
	libusb_free_device_list(libusb_list, 0);
	*devArray = (IUDEVICE**)array;
	return num;
}

IUDEVICE* udev_new_by_addr(URBDRC_PLUGIN* urbdrc, libusb_context* context, BYTE bus_number,
                           BYTE dev_number)
{
	WLog_Print(urbdrc->log, WLOG_DEBUG, "bus:%d dev:%d", bus_number, dev_number);
	return udev_init(urbdrc, context, NULL, bus_number, dev_number);
}
