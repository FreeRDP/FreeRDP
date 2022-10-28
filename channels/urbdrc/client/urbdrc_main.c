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

#include <winpr/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <winpr/pool.h>
#include <winpr/print.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/cmdline.h>

#include <freerdp/dvc.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/urbdrc.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"
#include "data_transfer.h"

#include <urbdrc_helpers.h>

static IWTSVirtualChannel* get_channel(IUDEVMAN* idevman)
{
	IWTSVirtualChannelManager* channel_mgr;
	URBDRC_PLUGIN* urbdrc;

	if (!idevman)
		return NULL;

	urbdrc = (URBDRC_PLUGIN*)idevman->plugin;

	if (!urbdrc || !urbdrc->listener_callback)
		return NULL;

	channel_mgr = urbdrc->listener_callback->channel_mgr;

	if (!channel_mgr)
		return NULL;

	return channel_mgr->FindChannelById(channel_mgr, idevman->controlChannelId);
}

static int func_container_id_generate(IUDEVICE* pdev, char* strContainerId)
{
	char *p, *path;
	UINT8 containerId[17] = { 0 };
	UINT16 idVendor, idProduct;
	idVendor = (UINT16)pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = (UINT16)pdev->query_device_descriptor(pdev, ID_PRODUCT);
	path = pdev->getPath(pdev);

	if (strlen(path) > 8)
		p = (path + strlen(path)) - 8;
	else
		p = path;

	sprintf_s((char*)containerId, sizeof(containerId), "%04" PRIX16 "%04" PRIX16 "%s", idVendor,
	          idProduct, p);
	/* format */
	sprintf_s(strContainerId, DEVICE_CONTAINER_STR_SIZE,
	          "{%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8
	          "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
	          "%02" PRIx8 "%02" PRIx8 "}",
	          containerId[0], containerId[1], containerId[2], containerId[3], containerId[4],
	          containerId[5], containerId[6], containerId[7], containerId[8], containerId[9],
	          containerId[10], containerId[11], containerId[12], containerId[13], containerId[14],
	          containerId[15]);
	return 0;
}

static int func_instance_id_generate(IUDEVICE* pdev, char* strInstanceId, size_t len)
{
	char instanceId[17] = { 0 };
	sprintf_s(instanceId, sizeof(instanceId), "\\%s", pdev->getPath(pdev));
	/* format */
	sprintf_s(strInstanceId, len,
	          "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8
	          "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
	          "%02" PRIx8 "%02" PRIx8 "",
	          instanceId[0], instanceId[1], instanceId[2], instanceId[3], instanceId[4],
	          instanceId[5], instanceId[6], instanceId[7], instanceId[8], instanceId[9],
	          instanceId[10], instanceId[11], instanceId[12], instanceId[13], instanceId[14],
	          instanceId[15]);
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_capability_request(GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                              UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 Version;
	UINT32 out_size;
	wStream* out;

	if (!callback || !s)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Version);

	if (Version > RIM_CAPABILITY_VERSION_01)
		Version = RIM_CAPABILITY_VERSION_01;

	InterfaceId = ((STREAM_ID_NONE << 30) | CAPABILITIES_NEGOTIATOR);
	out_size = 16;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, InterfaceId); /* interface id */
	Stream_Write_UINT32(out, MessageId);   /* message id */
	Stream_Write_UINT32(out, Version);     /* usb protocol version */
	Stream_Write_UINT32(out, 0x00000000);  /* HRESULT */
	return stream_write_and_free(callback->plugin, callback->channel, out);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_channel_create(GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                          UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 out_size;
	UINT32 MajorVersion;
	UINT32 MinorVersion;
	UINT32 Capabilities;
	wStream* out;
	URBDRC_PLUGIN* urbdrc;

	if (!callback || !s || !callback->plugin)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, MajorVersion);
	Stream_Read_UINT32(s, MinorVersion);
	Stream_Read_UINT32(s, Capabilities);

	/* Version check, we only support version 1.0 */
	if ((MajorVersion != 1) || (MinorVersion != 0))
	{
		WLog_Print(urbdrc->log, WLOG_WARN,
		           "server supports USB channel version %" PRIu32 ".%" PRIu32);
		WLog_Print(urbdrc->log, WLOG_WARN, "we only support channel version 1.0");
		MajorVersion = 1;
		MinorVersion = 0;
	}

	InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_CHANNEL_NOTIFICATION);
	out_size = 24;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, InterfaceId);     /* interface id */
	Stream_Write_UINT32(out, MessageId);       /* message id */
	Stream_Write_UINT32(out, CHANNEL_CREATED); /* function id */
	Stream_Write_UINT32(out, MajorVersion);
	Stream_Write_UINT32(out, MinorVersion);
	Stream_Write_UINT32(out, Capabilities); /* capabilities version */
	return stream_write_and_free(callback->plugin, callback->channel, out);
}

static UINT urdbrc_send_virtual_channel_add(IWTSPlugin* plugin, IWTSVirtualChannel* channel,
                                            UINT32 MessageId)
{
	const UINT32 InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_DEVICE_SINK);
	wStream* out = Stream_New(NULL, 12);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, InterfaceId);         /* interface */
	Stream_Write_UINT32(out, MessageId);           /* message id */
	Stream_Write_UINT32(out, ADD_VIRTUAL_CHANNEL); /* function id */
	return stream_write_and_free(plugin, channel, out);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urdbrc_send_usb_device_add(GENERIC_CHANNEL_CALLBACK* callback, IUDEVICE* pdev)
{
	wStream* out;
	UINT32 InterfaceId;
	char HardwareIds[2][DEVICE_HARDWARE_ID_SIZE] = { { 0 } };
	char CompatibilityIds[3][DEVICE_COMPATIBILITY_ID_SIZE] = { { 0 } };
	char strContainerId[DEVICE_CONTAINER_STR_SIZE] = { 0 };
	char strInstanceId[DEVICE_INSTANCE_STR_SIZE] = { 0 };
	const char* composite_str = "USB\\COMPOSITE";
	const size_t composite_len = 13;
	size_t size;
	size_t CompatibilityIdLen[3];
	size_t HardwareIdsLen[2];
	size_t ContainerIdLen, InstanceIdLen;
	size_t cchCompatIds;
	UINT32 bcdUSB;
	InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_DEVICE_SINK);
	/* USB kernel driver detach!! */
	pdev->detach_kernel_driver(pdev);
	{
		const UINT16 idVendor = (UINT16)pdev->query_device_descriptor(pdev, ID_VENDOR);
		const UINT16 idProduct = (UINT16)pdev->query_device_descriptor(pdev, ID_PRODUCT);
		const UINT16 bcdDevice = (UINT16)pdev->query_device_descriptor(pdev, BCD_DEVICE);
		sprintf_s(HardwareIds[1], DEVICE_HARDWARE_ID_SIZE,
		          "USB\\VID_%04" PRIX16 "&PID_%04" PRIX16 "", idVendor, idProduct);
		sprintf_s(HardwareIds[0], DEVICE_HARDWARE_ID_SIZE,
		          "USB\\VID_%04" PRIX16 "&PID_%04" PRIX16 "&REV_%04" PRIX16 "", idVendor, idProduct,
		          bcdDevice);
	}
	{
		const UINT8 bDeviceClass = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_CLASS);
		const UINT8 bDeviceSubClass = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_SUBCLASS);
		const UINT8 bDeviceProtocol = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_PROTOCOL);

		if (!(pdev->isCompositeDevice(pdev)))
		{
			sprintf_s(CompatibilityIds[2], DEVICE_COMPATIBILITY_ID_SIZE, "USB\\Class_%02" PRIX8 "",
			          bDeviceClass);
			sprintf_s(CompatibilityIds[1], DEVICE_COMPATIBILITY_ID_SIZE,
			          "USB\\Class_%02" PRIX8 "&SubClass_%02" PRIX8 "", bDeviceClass,
			          bDeviceSubClass);
			sprintf_s(CompatibilityIds[0], DEVICE_COMPATIBILITY_ID_SIZE,
			          "USB\\Class_%02" PRIX8 "&SubClass_%02" PRIX8 "&Prot_%02" PRIX8 "",
			          bDeviceClass, bDeviceSubClass, bDeviceProtocol);
		}
		else
		{
			sprintf_s(CompatibilityIds[2], DEVICE_COMPATIBILITY_ID_SIZE, "USB\\DevClass_00");
			sprintf_s(CompatibilityIds[1], DEVICE_COMPATIBILITY_ID_SIZE,
			          "USB\\DevClass_00&SubClass_00");
			sprintf_s(CompatibilityIds[0], DEVICE_COMPATIBILITY_ID_SIZE,
			          "USB\\DevClass_00&SubClass_00&Prot_00");
		}
	}
	func_instance_id_generate(pdev, strInstanceId, DEVICE_INSTANCE_STR_SIZE);
	func_container_id_generate(pdev, strContainerId);
	CompatibilityIdLen[0] = strnlen(CompatibilityIds[0], sizeof(CompatibilityIds[0]));
	CompatibilityIdLen[1] = strnlen(CompatibilityIds[1], sizeof(CompatibilityIds[1]));
	CompatibilityIdLen[2] = strnlen(CompatibilityIds[2], sizeof(CompatibilityIds[2]));
	HardwareIdsLen[0] = strnlen(HardwareIds[0], sizeof(HardwareIds[0]));
	HardwareIdsLen[1] = strnlen(HardwareIds[1], sizeof(HardwareIds[1]));
	cchCompatIds =
	    CompatibilityIdLen[0] + 1 + CompatibilityIdLen[1] + 1 + CompatibilityIdLen[2] + 2;
	InstanceIdLen = strnlen(strInstanceId, sizeof(strInstanceId));
	ContainerIdLen = strnlen(strContainerId, sizeof(strContainerId));

	if (pdev->isCompositeDevice(pdev))
		cchCompatIds += composite_len + 1;

	size = 24;
	size += (InstanceIdLen + 1) * 2 + (HardwareIdsLen[0] + 1) * 2 + 4 +
	        (HardwareIdsLen[1] + 1) * 2 + 2 + 4 + (cchCompatIds)*2 + (ContainerIdLen + 1) * 2 + 4 +
	        28;
	out = Stream_New(NULL, size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, InterfaceId); /* interface */
	Stream_Write_UINT32(out, 0);
	Stream_Write_UINT32(out, ADD_DEVICE);                /* function id */
	Stream_Write_UINT32(out, 0x00000001);                /* NumUsbDevice */
	Stream_Write_UINT32(out, pdev->get_UsbDevice(pdev)); /* UsbDevice */
	Stream_Write_UINT32(out, (UINT32)InstanceIdLen + 1); /* cchDeviceInstanceId */
	if (Stream_Write_UTF16_String_From_UTF8(out, InstanceIdLen, strInstanceId, InstanceIdLen,
	                                        TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	Stream_Write_UINT32(out, HardwareIdsLen[0] + HardwareIdsLen[1] + 3); /* cchHwIds */
	                                                                     /* HardwareIds 1 */
	if (Stream_Write_UTF16_String_From_UTF8(out, HardwareIdsLen[0], HardwareIds[0],
	                                        HardwareIdsLen[0], TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	if (Stream_Write_UTF16_String_From_UTF8(out, HardwareIdsLen[1], HardwareIds[1],
	                                        HardwareIdsLen[1], TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	Stream_Write_UINT16(out, 0);                    /* add "\0" */
	Stream_Write_UINT32(out, (UINT32)cchCompatIds); /* cchCompatIds */
	/* CompatibilityIds */
	if (Stream_Write_UTF16_String_From_UTF8(out, CompatibilityIdLen[0], CompatibilityIds[0],
	                                        CompatibilityIdLen[0], TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	if (Stream_Write_UTF16_String_From_UTF8(out, CompatibilityIdLen[1], CompatibilityIds[1],
	                                        CompatibilityIdLen[1], TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	if (Stream_Write_UTF16_String_From_UTF8(out, CompatibilityIdLen[2], CompatibilityIds[2],
	                                        CompatibilityIdLen[2], TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);

	if (pdev->isCompositeDevice(pdev))
	{
		if (Stream_Write_UTF16_String_From_UTF8(out, composite_len, composite_str, composite_len,
		                                        TRUE) < 0)
			goto fail;
		Stream_Write_UINT16(out, 0);
	}

	Stream_Write_UINT16(out, 0x0000);                     /* add "\0" */
	Stream_Write_UINT32(out, (UINT32)ContainerIdLen + 1); /* cchContainerId */
	/* ContainerId */
	if (Stream_Write_UTF16_String_From_UTF8(out, ContainerIdLen, strContainerId, ContainerIdLen,
	                                        TRUE) < 0)
		goto fail;
	Stream_Write_UINT16(out, 0);
	/* USB_DEVICE_CAPABILITIES 28 bytes */
	Stream_Write_UINT32(out, 0x0000001c);                                /* CbSize */
	Stream_Write_UINT32(out, 2); /* UsbBusInterfaceVersion, 0 ,1 or 2 */ // TODO: Get from libusb
	Stream_Write_UINT32(out, 0x600); /* USBDI_Version, 0x500 or 0x600 */ // TODO: Get from libusb
	/* Supported_USB_Version, 0x110,0x110 or 0x200(usb2.0) */
	bcdUSB = pdev->query_device_descriptor(pdev, BCD_USB);
	Stream_Write_UINT32(out, bcdUSB);
	Stream_Write_UINT32(out, 0x00000000); /* HcdCapabilities, MUST always be zero */

	if (bcdUSB < 0x200)
		Stream_Write_UINT32(out, 0x00000000); /* DeviceIsHighSpeed */
	else
		Stream_Write_UINT32(out, 0x00000001); /* DeviceIsHighSpeed */

	Stream_Write_UINT32(out, 0x50); /* NoAckIsochWriteJitterBufferSizeInMs, >=10 or <=512 */
	return stream_write_and_free(callback->plugin, callback->channel, out);

fail:
	Stream_Free(out, TRUE);
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_exchange_capabilities(GENERIC_CHANNEL_CALLBACK* callback, wStream* data)
{
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT32 InterfaceId;
	UINT error = CHANNEL_RC_OK;

	if (!data)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, data, 8))
		return ERROR_INVALID_DATA;

	Stream_Rewind_UINT32(data);
	Stream_Read_UINT32(data, InterfaceId);
	Stream_Read_UINT32(data, MessageId);
	Stream_Read_UINT32(data, FunctionId);

	switch (FunctionId)
	{
		case RIM_EXCHANGE_CAPABILITY_REQUEST:
			error = urbdrc_process_capability_request(callback, data, MessageId);
			break;

		case RIMCALL_RELEASE:
			break;

		default:
			error = ERROR_NOT_FOUND;
			break;
	}

	return error;
}

static BOOL urbdrc_announce_devices(IUDEVMAN* udevman)
{
	UINT error = ERROR_SUCCESS;

	udevman->loading_lock(udevman);
	udevman->rewind(udevman);

	while (udevman->has_next(udevman))
	{
		IUDEVICE* pdev = udevman->get_next(udevman);

		if (!pdev->isAlreadySend(pdev))
		{
			const UINT32 deviceId = pdev->get_UsbDevice(pdev);
			UINT cerror =
			    urdbrc_send_virtual_channel_add(udevman->plugin, get_channel(udevman), deviceId);

			if (cerror != ERROR_SUCCESS)
				break;
		}
	}

	udevman->loading_unlock(udevman);

	return error == ERROR_SUCCESS;
}

static UINT urbdrc_device_control_channel(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;
	IUDEVMAN* udevman = urbdrc->udevman;
	IWTSVirtualChannel* channel = callback->channel;
	IUDEVICE* pdev = NULL;
	BOOL found = FALSE;
	UINT error = ERROR_INTERNAL_ERROR;
	UINT32 channelId = callback->channel_mgr->GetChannelId(channel);

	switch (urbdrc->vchannel_status)
	{
		case INIT_CHANNEL_IN:
			/* Control channel was established */
			error = ERROR_SUCCESS;
			udevman->initialize(udevman, channelId);

			if (!urbdrc_announce_devices(udevman))
				goto fail;

			urbdrc->vchannel_status = INIT_CHANNEL_OUT;
			break;

		case INIT_CHANNEL_OUT:
			/* A new device channel was created, add the channel
			 * to the device */
			udevman->loading_lock(udevman);
			udevman->rewind(udevman);

			while (udevman->has_next(udevman))
			{
				pdev = udevman->get_next(udevman);

				if (!pdev->isAlreadySend(pdev))
				{
					const UINT32 channelID = callback->channel_mgr->GetChannelId(channel);
					found = TRUE;
					pdev->setAlreadySend(pdev);
					pdev->set_channelManager(pdev, callback->channel_mgr);
					pdev->set_channelID(pdev, channelID);
					break;
				}
			}

			udevman->loading_unlock(udevman);
			error = ERROR_SUCCESS;

			if (found && pdev->isAlreadySend(pdev))
				error = urdbrc_send_usb_device_add(callback, pdev);

			break;

		default:
			WLog_Print(urbdrc->log, WLOG_ERROR, "vchannel_status unknown value %" PRIu32 "",
			           urbdrc->vchannel_status);
			break;
	}

fail:
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_channel_notification(GENERIC_CHANNEL_CALLBACK* callback, wStream* data)
{
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT32 InterfaceId;
	UINT error = CHANNEL_RC_OK;
	URBDRC_PLUGIN* urbdrc;

	if (!callback || !data)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, data, 8))
		return ERROR_INVALID_DATA;

	Stream_Rewind(data, 4);
	Stream_Read_UINT32(data, InterfaceId);
	Stream_Read_UINT32(data, MessageId);
	Stream_Read_UINT32(data, FunctionId);
	WLog_Print(urbdrc->log, WLOG_TRACE, "%s [%" PRIu32 "]",
	           call_to_string(FALSE, InterfaceId, FunctionId), FunctionId);

	switch (FunctionId)
	{
		case CHANNEL_CREATED:
			error = urbdrc_process_channel_create(callback, data, MessageId);
			break;

		case RIMCALL_RELEASE:
			error = urbdrc_device_control_channel(callback, data);
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_TRACE, "%s: unknown FunctionId 0x%" PRIX32 "",
			           __FUNCTION__, FunctionId);
			error = 1;
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	URBDRC_PLUGIN* urbdrc;
	IUDEVMAN* udevman;
	UINT32 InterfaceId;
	UINT error = ERROR_INTERNAL_ERROR;

	if (callback == NULL)
		return ERROR_INVALID_PARAMETER;

	if (callback->plugin == NULL)
		return error;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (urbdrc->udevman == NULL)
		return error;

	udevman = (IUDEVMAN*)urbdrc->udevman;

	if (!Stream_CheckAndLogRequiredLength(TAG, data, 12))
		return ERROR_INVALID_DATA;

	urbdrc_dump_message(urbdrc->log, FALSE, FALSE, data);
	Stream_Read_UINT32(data, InterfaceId);

	/* Need to check InterfaceId and mask values */
	switch (InterfaceId)
	{
		case CAPABILITIES_NEGOTIATOR | (STREAM_ID_NONE << 30):
			error = urbdrc_exchange_capabilities(callback, data);
			break;

		case SERVER_CHANNEL_NOTIFICATION | (STREAM_ID_PROXY << 30):
			error = urbdrc_process_channel_notification(callback, data);
			break;

		default:
			error = urbdrc_process_udev_data_transfer(callback, urbdrc, udevman, data);
			WLog_DBG(TAG, "urbdrc_process_udev_data_transfer returned 0x%08" PRIx32, error);
			error = ERROR_SUCCESS; /* Ignore errors, the device may have been unplugged. */
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	if (callback)
	{
		URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;
		if (urbdrc)
		{
			IUDEVMAN* udevman = urbdrc->udevman;
			if (udevman && callback->channel_mgr)
			{
				UINT32 control = callback->channel_mgr->GetChannelId(callback->channel);
				if (udevman->controlChannelId == control)
					udevman->status |= URBDRC_DEVICE_CHANNEL_CLOSED;
				else
				{ /* Need to notify the local backend the device is gone */
					IUDEVICE* pdev = udevman->get_udevice_by_ChannelID(udevman, control);
					if (pdev)
						pdev->markChannelClosed(pdev);
				}
			}
		}
	}
	free(callback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                             IWTSVirtualChannel* pChannel, BYTE* pData,
                                             BOOL* pbAccept,
                                             IWTSVirtualChannelCallback** ppCallback)
{
	GENERIC_LISTENER_CALLBACK* listener_callback = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;
	GENERIC_CHANNEL_CALLBACK* callback;

	if (!ppCallback)
		return ERROR_INVALID_PARAMETER;

	callback = (GENERIC_CHANNEL_CALLBACK*)calloc(1, sizeof(GENERIC_CHANNEL_CALLBACK));

	if (!callback)
		return ERROR_OUTOFMEMORY;

	callback->iface.OnDataReceived = urbdrc_on_data_received;
	callback->iface.OnClose = urbdrc_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*)callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT status;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)pPlugin;
	IUDEVMAN* udevman;
	char channelName[sizeof(URBDRC_CHANNEL_NAME)] = { URBDRC_CHANNEL_NAME };

	if (!urbdrc || !urbdrc->udevman)
		return ERROR_INVALID_PARAMETER;

	if (urbdrc->initialized)
	{
		WLog_ERR(TAG, "[%s] channel initialized twice, aborting", URBDRC_CHANNEL_NAME);
		return ERROR_INVALID_DATA;
	}
	udevman = urbdrc->udevman;
	urbdrc->listener_callback =
	    (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));

	if (!urbdrc->listener_callback)
		return CHANNEL_RC_NO_MEMORY;

	urbdrc->listener_callback->iface.OnNewChannelConnection = urbdrc_on_new_channel_connection;
	urbdrc->listener_callback->plugin = pPlugin;
	urbdrc->listener_callback->channel_mgr = pChannelMgr;

	/* [MS-RDPEUSB] 2.1 Transport defines the channel name in uppercase letters */
	CharUpperA(channelName);
	status = pChannelMgr->CreateListener(pChannelMgr, channelName, 0,
	                                     &urbdrc->listener_callback->iface, &urbdrc->listener);
	if (status != CHANNEL_RC_OK)
		return status;

	status = CHANNEL_RC_OK;
	if (udevman->listener_created_callback)
		status = udevman->listener_created_callback(udevman);

	urbdrc->initialized = status == CHANNEL_RC_OK;
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_plugin_terminated(IWTSPlugin* pPlugin)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)pPlugin;
	IUDEVMAN* udevman;

	if (!urbdrc)
		return ERROR_INVALID_DATA;
	if (urbdrc->listener_callback)
	{
		IWTSVirtualChannelManager* mgr = urbdrc->listener_callback->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, urbdrc->listener);
	}
	udevman = urbdrc->udevman;

	if (udevman)
	{
		udevman->free(udevman);
		udevman = NULL;
	}

	free(urbdrc->subsystem);
	free(urbdrc->listener_callback);
	free(urbdrc);
	return CHANNEL_RC_OK;
}

static BOOL urbdrc_register_udevman_addin(IWTSPlugin* pPlugin, IUDEVMAN* udevman)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)pPlugin;

	if (urbdrc->udevman)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "existing device, abort.");
		return FALSE;
	}

	DEBUG_DVC("device registered.");
	urbdrc->udevman = udevman;
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_load_udevman_addin(IWTSPlugin* pPlugin, LPCSTR name, const ADDIN_ARGV* args)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)pPlugin;
	PFREERDP_URBDRC_DEVICE_ENTRY entry;
	FREERDP_URBDRC_SERVICE_ENTRY_POINTS entryPoints;
	entry = (PFREERDP_URBDRC_DEVICE_ENTRY)freerdp_load_channel_addin_entry(URBDRC_CHANNEL_NAME,
	                                                                       name, NULL, 0);

	if (!entry)
		return ERROR_INVALID_OPERATION;

	entryPoints.plugin = pPlugin;
	entryPoints.pRegisterUDEVMAN = urbdrc_register_udevman_addin;
	entryPoints.args = args;

	if (entry(&entryPoints) != 0)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "%s entry returns error.", name);
		return ERROR_INVALID_OPERATION;
	}

	return CHANNEL_RC_OK;
}

static BOOL urbdrc_set_subsystem(URBDRC_PLUGIN* urbdrc, const char* subsystem)
{
	free(urbdrc->subsystem);
	urbdrc->subsystem = _strdup(subsystem);
	return (urbdrc->subsystem != NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_addin_args(URBDRC_PLUGIN* urbdrc, const ADDIN_ARGV* args)
{
	int status;
	COMMAND_LINE_ARGUMENT_A urbdrc_args[] = {
		{ "dbg", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "debug" },
		{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device list>", NULL, NULL, -1, NULL, "devices" },
		{ "encode", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "encode" },
		{ "quality", COMMAND_LINE_VALUE_REQUIRED, "<[0-2] -> [high-medium-low]>", NULL, NULL, -1,
		  NULL, "quality" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	const DWORD flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	const COMMAND_LINE_ARGUMENT_A* arg;
	status =
	    CommandLineParseArgumentsA(args->argc, args->argv, urbdrc_args, flags, urbdrc, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_DATA;

	arg = urbdrc_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dbg")
		{
			WLog_SetLogLevel(urbdrc->log, WLOG_TRACE);
		}
		CommandLineSwitchCase(arg, "sys")
		{
			if (!urbdrc_set_subsystem(urbdrc, arg->Value))
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

BOOL add_device(IUDEVMAN* idevman, UINT32 flags, BYTE busnum, BYTE devnum, UINT16 idVendor,
                UINT16 idProduct)
{
	size_t success = 0;
	URBDRC_PLUGIN* urbdrc;
	UINT32 mask, regflags = 0;

	if (!idevman)
		return FALSE;

	urbdrc = (URBDRC_PLUGIN*)idevman->plugin;

	if (!urbdrc || !urbdrc->listener_callback)
		return FALSE;

	mask = (DEVICE_ADD_FLAG_VENDOR | DEVICE_ADD_FLAG_PRODUCT);
	if ((flags & mask) == mask)
		regflags |= UDEVMAN_FLAG_ADD_BY_VID_PID;
	mask = (DEVICE_ADD_FLAG_BUS | DEVICE_ADD_FLAG_DEV);
	if ((flags & mask) == mask)
		regflags |= UDEVMAN_FLAG_ADD_BY_ADDR;

	success = idevman->register_udevice(idevman, busnum, devnum, idVendor, idProduct, regflags);

	if ((success > 0) && (flags & DEVICE_ADD_FLAG_REGISTER))
	{
		if (!urbdrc_announce_devices(idevman))
			return FALSE;
	}

	return TRUE;
}

BOOL del_device(IUDEVMAN* idevman, UINT32 flags, BYTE busnum, BYTE devnum, UINT16 idVendor,
                UINT16 idProduct)
{
	IUDEVICE* pdev = NULL;
	URBDRC_PLUGIN* urbdrc;

	if (!idevman)
		return FALSE;

	urbdrc = (URBDRC_PLUGIN*)idevman->plugin;

	if (!urbdrc || !urbdrc->listener_callback)
		return FALSE;

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		BOOL match = TRUE;
		IUDEVICE* dev = idevman->get_next(idevman);

		if ((flags & (DEVICE_ADD_FLAG_BUS | DEVICE_ADD_FLAG_DEV | DEVICE_ADD_FLAG_VENDOR |
		              DEVICE_ADD_FLAG_PRODUCT)) == 0)
			match = FALSE;
		if (flags & DEVICE_ADD_FLAG_BUS)
		{
			if (dev->get_bus_number(dev) != busnum)
				match = FALSE;
		}
		if (flags & DEVICE_ADD_FLAG_DEV)
		{
			if (dev->get_dev_number(dev) != devnum)
				match = FALSE;
		}
		if (flags & DEVICE_ADD_FLAG_VENDOR)
		{
			int vid = dev->query_device_descriptor(dev, ID_VENDOR);
			if (vid != idVendor)
				match = FALSE;
		}
		if (flags & DEVICE_ADD_FLAG_PRODUCT)
		{
			int pid = dev->query_device_descriptor(dev, ID_PRODUCT);
			if (pid != idProduct)
				match = FALSE;
		}

		if (match)
		{
			pdev = dev;
			break;
		}
	}

	if (pdev)
		pdev->setChannelClosed(pdev);

	idevman->loading_unlock(idevman);
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT urbdrc_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT status = 0;
	const ADDIN_ARGV* args;
	URBDRC_PLUGIN* urbdrc;
	urbdrc = (URBDRC_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, URBDRC_CHANNEL_NAME);
	args = pEntryPoints->GetPluginData(pEntryPoints);

	if (urbdrc == NULL)
	{
		urbdrc = (URBDRC_PLUGIN*)calloc(1, sizeof(URBDRC_PLUGIN));

		if (!urbdrc)
			return CHANNEL_RC_NO_MEMORY;

		urbdrc->iface.Initialize = urbdrc_plugin_initialize;
		urbdrc->iface.Terminated = urbdrc_plugin_terminated;
		urbdrc->vchannel_status = INIT_CHANNEL_IN;
		status = pEntryPoints->RegisterPlugin(pEntryPoints, URBDRC_CHANNEL_NAME, &urbdrc->iface);

		/* After we register the plugin free will be taken care of by dynamic channel */
		if (status != CHANNEL_RC_OK)
		{
			free(urbdrc);
			goto fail;
		}

		urbdrc->log = WLog_Get(TAG);

		if (!urbdrc->log)
			goto fail;
	}

	status = urbdrc_process_addin_args(urbdrc, args);

	if (status != CHANNEL_RC_OK)
		goto fail;

	if (!urbdrc->subsystem && !urbdrc_set_subsystem(urbdrc, "libusb"))
		goto fail;

	return urbdrc_load_udevman_addin(&urbdrc->iface, urbdrc->subsystem, args);
fail:
	return status;
}

UINT stream_write_and_free(IWTSPlugin* plugin, IWTSVirtualChannel* channel, wStream* out)
{
	UINT rc;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)plugin;

	if (!out)
		return ERROR_INVALID_PARAMETER;

	if (!channel || !out || !urbdrc)
	{
		Stream_Free(out, TRUE);
		return ERROR_INVALID_PARAMETER;
	}

	if (!channel->Write)
	{
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	urbdrc_dump_message(urbdrc->log, TRUE, TRUE, out);
	rc = channel->Write(channel, Stream_GetPosition(out), Stream_Buffer(out), NULL);
	Stream_Free(out, TRUE);
	return rc;
}
