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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <err.h>
#endif
#if defined(__linux__)
#include <libudev.h>
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/cmdline.h>

#include <freerdp/dvc.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"
#include "data_transfer.h"
#include "searchman.h"

static int func_hardware_id_format(IUDEVICE* pdev, char(*HardwareIds)[DEVICE_HARDWARE_ID_SIZE])
{
	char str[DEVICE_HARDWARE_ID_SIZE];
	UINT16 idVendor, idProduct, bcdDevice;
	idVendor = (UINT16)pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = (UINT16)pdev->query_device_descriptor(pdev, ID_PRODUCT);
	bcdDevice = (UINT16)pdev->query_device_descriptor(pdev, BCD_DEVICE);
	sprintf_s(str, sizeof(str), "USB\\VID_%04"PRIX16"&PID_%04"PRIX16"", idVendor, idProduct);
	strcpy(HardwareIds[1], str);
	sprintf_s(str, sizeof(str), "%s&REV_%04"PRIX16"", HardwareIds[1], bcdDevice);
	strcpy(HardwareIds[0], str);
	return 0;
}

static int func_compat_id_format(IUDEVICE* pdev,
                                 char (*CompatibilityIds)[DEVICE_COMPATIBILITY_ID_SIZE])
{
	char str[DEVICE_COMPATIBILITY_ID_SIZE];
	UINT8 bDeviceClass, bDeviceSubClass, bDeviceProtocol;
	bDeviceClass = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_CLASS);
	bDeviceSubClass = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_SUBCLASS);
	bDeviceProtocol = (UINT8)pdev->query_device_descriptor(pdev, B_DEVICE_PROTOCOL);

	if (!(pdev->isCompositeDevice(pdev)))
	{
		sprintf_s(str, sizeof(str), "USB\\Class_%02"PRIX8"", bDeviceClass);
		strcpy(CompatibilityIds[2], str);
		sprintf_s(str, sizeof(str), "%s&SubClass_%02"PRIX8"", CompatibilityIds[2], bDeviceSubClass);
		strcpy(CompatibilityIds[1], str);
		sprintf_s(str, sizeof(str), "%s&Prot_%02"PRIX8"", CompatibilityIds[1], bDeviceProtocol);
		strcpy(CompatibilityIds[0], str);
	}
	else
	{
		sprintf_s(str, sizeof(str), "USB\\DevClass_00");
		strcpy(CompatibilityIds[2], str);
		sprintf_s(str, sizeof(str), "%s&SubClass_00", CompatibilityIds[2]);
		strcpy(CompatibilityIds[1], str);
		sprintf_s(str, sizeof(str), "%s&Prot_00", CompatibilityIds[1]);
		strcpy(CompatibilityIds[0], str);
	}

	return 0;
}

static void func_close_udevice(USB_SEARCHMAN* searchman, IUDEVICE* pdev)
{
	int idVendor = 0;
	int idProduct = 0;
	URBDRC_PLUGIN* urbdrc = searchman->urbdrc;
	pdev->SigToEnd(pdev);
	idVendor = pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = pdev->query_device_descriptor(pdev, ID_PRODUCT);
	searchman->add(searchman, (UINT16) idVendor, (UINT16) idProduct);
	pdev->cancel_all_transfer_request(pdev);
	pdev->wait_action_completion(pdev);
#if ISOCH_FIFO
	{
		/* free isoch queue */
		ISOCH_CALLBACK_QUEUE* isoch_queue = pdev->get_isoch_queue(pdev);

		if (isoch_queue)
			isoch_queue->free(isoch_queue);
	}
#endif
	urbdrc->udevman->unregister_udevice(urbdrc->udevman,
	                                    pdev->get_bus_number(pdev),
	                                    pdev->get_dev_number(pdev));
}

static int fun_device_string_send_set(char* out_data, int out_offset, char* str)
{
	int i = 0;
	int offset = 0;

	while (str[i])
	{
		data_write_UINT16(out_data + out_offset + offset, str[i]);   /* str */
		i++;
		offset += 2;
	}

	data_write_UINT16(out_data + out_offset + offset, 0x0000);   /* add "\0" */
	offset += 2;
	return offset + out_offset;
}

static int func_container_id_generate(IUDEVICE* pdev, char* strContainerId)
{
	char* p, *path;
	UINT8 containerId[17];
	UINT16 idVendor, idProduct;
	idVendor = (UINT16)pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = (UINT16)pdev->query_device_descriptor(pdev, ID_PRODUCT);
	path = pdev->getPath(pdev);

	if (strlen(path) > 8)
		p = (path + strlen(path)) - 8;
	else
		p = path;

	ZeroMemory(containerId, sizeof(containerId));
	sprintf_s((char*)containerId, sizeof(containerId), "%04"PRIX16"%04"PRIX16"%s", idVendor, idProduct,
	          p);
	/* format */
	sprintf_s(strContainerId, DEVICE_CONTAINER_STR_SIZE,
	          "{%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"}",
	          containerId[0], containerId[1], containerId[2], containerId[3],
	          containerId[4], containerId[5], containerId[6], containerId[7],
	          containerId[8], containerId[9], containerId[10], containerId[11],
	          containerId[12], containerId[13], containerId[14], containerId[15]);
	return 0;
}

static int func_instance_id_generate(IUDEVICE* pdev, char* strInstanceId)
{
	UINT8 instanceId[17];
	ZeroMemory(instanceId, sizeof(instanceId));
	sprintf_s((char*)instanceId, sizeof(instanceId), "\\%s", pdev->getPath(pdev));
	/* format */
	sprintf_s(strInstanceId, DEVICE_INSTANCE_STR_SIZE,
	          "%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"-%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"%02"PRIx8"",
	          instanceId[0], instanceId[1], instanceId[2], instanceId[3],
	          instanceId[4], instanceId[5], instanceId[6], instanceId[7],
	          instanceId[8], instanceId[9], instanceId[10], instanceId[11],
	          instanceId[12], instanceId[13], instanceId[14], instanceId[15]);
	return 0;
}

#if ISOCH_FIFO

static void func_lock_isoch_mutex(TRANSFER_DATA*  transfer_data)
{
	int noAck = 0;
	IUDEVICE* pdev;
	UINT32 FunctionId;
	UINT32 RequestField;
	UINT16 URB_Function;
	IUDEVMAN* udevman = transfer_data->udevman;

	if (transfer_data->cbSize >= 8)
	{
		data_read_UINT32(transfer_data->pBuffer + 4, FunctionId);

		if ((FunctionId == TRANSFER_IN_REQUEST ||
		     FunctionId == TRANSFER_OUT_REQUEST) &&
		    transfer_data->cbSize >= 16)
		{
			data_read_UINT16(transfer_data->pBuffer + 14, URB_Function);

			if (URB_Function == URB_FUNCTION_ISOCH_TRANSFER &&
			    transfer_data->cbSize >= 20)
			{
				data_read_UINT32(transfer_data->pBuffer + 16, RequestField);
				noAck = (RequestField & 0x80000000) >> 31;

				if (!noAck)
				{
					pdev = udevman->get_udevice_by_UsbDevice(udevman, transfer_data->UsbDevice);
					pdev->lock_fifo_isoch(pdev);
				}
			}
		}
	}
}

#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_capability_request(URBDRC_CHANNEL_CALLBACK* callback, char* data,
        UINT32 data_sizem, UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 Version;
	UINT32 out_size;
	char* out_data;
	UINT ret;
	WLog_VRB(TAG, "");
	data_read_UINT32(data + 0, Version);
	InterfaceId = ((STREAM_ID_NONE << 30) | CAPABILITIES_NEGOTIATOR);
	out_size = 16;
	out_data = (char*) calloc(1, out_size);

	if (!out_data)
		return ERROR_OUTOFMEMORY;

	data_write_UINT32(out_data + 0, InterfaceId); /* interface id */
	data_write_UINT32(out_data + 4, MessageId); /* message id */
	data_write_UINT32(out_data + 8, Version); /* usb protocol version */
	data_write_UINT32(out_data + 12, 0x00000000); /* HRESULT */
	ret = callback->channel->Write(callback->channel, out_size, (BYTE*) out_data, NULL);
	zfree(out_data);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_channel_create(URBDRC_CHANNEL_CALLBACK* callback, char* data,
        UINT32 data_sizem, UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 out_size;
	UINT32 MajorVersion;
	UINT32 MinorVersion;
	UINT32 Capabilities;
	char* out_data;
	UINT ret;
	WLog_VRB(TAG, "");
	data_read_UINT32(data + 0, MajorVersion);
	data_read_UINT32(data + 4, MinorVersion);
	data_read_UINT32(data + 8, Capabilities);
	InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_CHANNEL_NOTIFICATION);
	out_size = 24;
	out_data = (char*) calloc(1, out_size);

	if (!out_data)
		return ERROR_OUTOFMEMORY;

	data_write_UINT32(out_data + 0, InterfaceId); /* interface id */
	data_write_UINT32(out_data + 4, MessageId); /* message id */
	data_write_UINT32(out_data + 8, CHANNEL_CREATED); /* function id */
	data_write_UINT32(out_data + 12, MajorVersion);
	data_write_UINT32(out_data + 16, MinorVersion);
	data_write_UINT32(out_data + 20, Capabilities); /* capabilities version */
	ret = callback->channel->Write(callback->channel, out_size, (BYTE*)out_data, NULL);
	zfree(out_data);
	return ret;
}

static int urdbrc_send_virtual_channel_add(IWTSVirtualChannel* channel, UINT32 MessageId)
{
	UINT32 out_size;
	UINT32 InterfaceId;
	char* out_data;
	WLog_VRB(TAG, "");
	assert(NULL != channel);
	assert(NULL != channel->Write);
	InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_DEVICE_SINK);
	out_size = 12;
	out_data = (char*) malloc(out_size);
	memset(out_data, 0, out_size);
	data_write_UINT32(out_data + 0, InterfaceId); /* interface */
	data_write_UINT32(out_data + 4, MessageId); /* message id */
	data_write_UINT32(out_data + 8, ADD_VIRTUAL_CHANNEL); /* function id */
	channel->Write(channel, out_size, (BYTE*) out_data, NULL);
	zfree(out_data);
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urdbrc_send_usb_device_add(URBDRC_CHANNEL_CALLBACK* callback, IUDEVICE* pdev)
{
	char* out_data;
	UINT32 InterfaceId;
	char HardwareIds[2][DEVICE_HARDWARE_ID_SIZE];
	char CompatibilityIds[3][DEVICE_COMPATIBILITY_ID_SIZE];
	char strContainerId[DEVICE_CONTAINER_STR_SIZE];
	char strInstanceId[DEVICE_INSTANCE_STR_SIZE];
	char* composite_str = "USB\\COMPOSITE";
	int size, out_offset, cchCompatIds, bcdUSB;
	ISOCH_CALLBACK_QUEUE* cb_queue;
	UINT ret;
	WLog_VRB(TAG, "");
	InterfaceId = ((STREAM_ID_PROXY << 30) | CLIENT_DEVICE_SINK);
	/* USB kernel driver detach!! */
	pdev->detach_kernel_driver(pdev);
#if ISOCH_FIFO
	/* create/initial isoch queue */
	cb_queue = isoch_queue_new();

	if (!cb_queue)
		return ERROR_OUTOFMEMORY;

	pdev->set_isoch_queue(pdev, (void*)cb_queue);
#endif
	func_hardware_id_format(pdev, HardwareIds);
	func_compat_id_format(pdev, CompatibilityIds);
	func_instance_id_generate(pdev, strInstanceId);
	func_container_id_generate(pdev, strContainerId);
	cchCompatIds = strlen(CompatibilityIds[0]) + 1 +
	               strlen(CompatibilityIds[1]) + 1 +
	               strlen(CompatibilityIds[2]) + 2;

	if (pdev->isCompositeDevice(pdev))
		cchCompatIds += strlen(composite_str) + 1;

	out_offset = 24;
	size = 24;
	size += (strlen(strInstanceId) + 1) * 2 +
	        (strlen(HardwareIds[0]) + 1) * 2 + 4 +
	        (strlen(HardwareIds[1]) + 1) * 2 + 2 +
	        4 + (cchCompatIds) * 2 +
	        (strlen(strContainerId) + 1) * 2 + 4 + 28;
	out_data = (char*)calloc(1, size);

	if (!out_data)
		return ERROR_OUTOFMEMORY;

	data_write_UINT32(out_data + 0, InterfaceId); /* interface */
	/* data_write_UINT32(out_data + 4, 0);*/ /* message id */
	data_write_UINT32(out_data + 8, ADD_DEVICE); /* function id */
	data_write_UINT32(out_data + 12, 0x00000001); /* NumUsbDevice */
	data_write_UINT32(out_data + 16, pdev->get_UsbDevice(pdev)); /* UsbDevice */
	data_write_UINT32(out_data + 20, 0x00000025); /* cchDeviceInstanceId */
	out_offset = fun_device_string_send_set(out_data, out_offset, strInstanceId);
	data_write_UINT32(out_data + out_offset, 0x00000036); /* cchHwIds */
	out_offset += 4;
	/* HardwareIds 1 */
	out_offset = fun_device_string_send_set(out_data, out_offset, HardwareIds[0]);
	/* HardwareIds 2 */
	out_offset = fun_device_string_send_set(out_data, out_offset, HardwareIds[1]);
	/*data_write_UINT16(out_data + out_offset, 0x0000);*/ /* add "\0" */
	out_offset += 2;
	data_write_UINT32(out_data + out_offset, cchCompatIds); /* cchCompatIds */
	out_offset += 4;
	/* CompatibilityIds 1 */
	out_offset = fun_device_string_send_set(out_data, out_offset, CompatibilityIds[0]);
	/* CompatibilityIds 2 */
	out_offset = fun_device_string_send_set(out_data, out_offset, CompatibilityIds[1]);
	/* CompatibilityIds 3 */
	out_offset = fun_device_string_send_set(out_data, out_offset, CompatibilityIds[2]);

	if (pdev->isCompositeDevice(pdev))
		out_offset = fun_device_string_send_set(out_data, out_offset, composite_str);

	/*data_write_UINT16(out_data + out_offset, 0x0000);*/ /* add "\0" */
	out_offset += 2;
	data_write_UINT32(out_data + out_offset, 0x00000027); /* cchContainerId */
	out_offset += 4;
	/* ContainerId */
	out_offset = fun_device_string_send_set(out_data, out_offset, strContainerId);
	/* USB_DEVICE_CAPABILITIES 28 bytes */
	data_write_UINT32(out_data + out_offset, 0x0000001c); /* CbSize */
	data_write_UINT32(out_data + out_offset + 4, 2); /* UsbBusInterfaceVersion, 0 ,1 or 2 */
	data_write_UINT32(out_data + out_offset + 8, 0x600); /* USBDI_Version, 0x500 or 0x600 */
	/* Supported_USB_Version, 0x110,0x110 or 0x200(usb2.0) */
	bcdUSB = pdev->query_device_descriptor(pdev, BCD_USB);
	data_write_UINT32(out_data + out_offset + 12, bcdUSB);
	data_write_UINT32(out_data + out_offset + 16,
	                  0x00000000); /* HcdCapabilities, MUST always be zero */

	if (bcdUSB < 0x200)
		data_write_UINT32(out_data + out_offset + 20, 0x00000000); /* DeviceIsHighSpeed */
	else
		data_write_UINT32(out_data + out_offset + 20, 0x00000001); /* DeviceIsHighSpeed */

	data_write_UINT32(out_data + out_offset + 24,
	                  0x50); /* NoAckIsochWriteJitterBufferSizeInMs, >=10 or <=512 */
	out_offset += 28;
	ret = callback->channel->Write(callback->channel, out_offset, (BYTE*)out_data, NULL);
	zfree(out_data);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_exchange_capabilities(URBDRC_CHANNEL_CALLBACK* callback, char* pBuffer,
        UINT32 cbSize)
{
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT error = CHANNEL_RC_OK;
	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);

	switch (FunctionId)
	{
		case RIM_EXCHANGE_CAPABILITY_REQUEST:
			error = urbdrc_process_capability_request(callback, pBuffer + 8, cbSize - 8, MessageId);
			break;

		default:
			WLog_ERR(TAG, "%s: unknown FunctionId 0x%"PRIX32"", __FUNCTION__, FunctionId);
			error = ERROR_NOT_FOUND;
			break;
	}

	return error;
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
static char* devd_get_val(char* buf, size_t buf_size, const char* val_name, size_t val_name_size,
                          size_t* val_size)
{
	char* ret, *buf_end, *ptr;
	buf_end = (buf + buf_size);

	for (ret = buf; ret != NULL && ret < buf_end;)
	{
		ret = memmem(ret, (buf_end - ret), val_name, val_name_size);

		if (ret == NULL)
			return NULL;

		/* Found. */
		/* Check: space before or buf+1. */
		if ((buf + 1) < ret && ret[-1] != ' ')
		{
			ret += val_name_size;
			continue;
		}

		/* Check: = after name and size for value. */
		ret += val_name_size;

		if ((ret + 1) >= buf_end)
			return NULL;

		if (ret[0] != '=')
			continue;

		ret ++;
		break;
	}

	if (ret == NULL || val_size == NULL)
		return ret;

	/* Calc value data size. */
	ptr = memchr(ret, ' ', (buf_end - ret));

	if (ptr == NULL) /* End of string/last value. */
		ptr = buf_end;

	(*val_size) = (ptr - ret);
	return ret;
}

static void* urbdrc_search_usb_device(void* arg)
{
	USB_SEARCHMAN* searchman = (USB_SEARCHMAN*)arg;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)searchman->urbdrc;
	IUDEVMAN* udevman = urbdrc->udevman;
	IWTSVirtualChannelManager* channel_mgr = urbdrc->listener_callback->channel_mgr;
	IWTSVirtualChannel* dvc_channel;
	USB_SEARCHDEV* sdev;
	IUDEVICE* pdev;
	HANDLE listobj[2];
	HANDLE mon_fd;
	int devd_skt;
	char buf[4096], *val, *ptr, *end_val;
	ssize_t data_size;
	size_t val_size, tm;
	long idVendor, idProduct;
	long busnum, devnum;
	int action, success, error, found, on_close;
	struct sockaddr_un sun;
	DWORD status;
	UINT32 error;
	WLog_DBG(TAG, "urbdrc_search_usb_device - devd: start");
	devd_skt = socket(PF_LOCAL, SOCK_SEQPACKET, 0);

	if (devd_skt == -1)
	{
		WLog_ERR(TAG, "Can't create devd socket: error = %i", errno);
		goto err_out;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = PF_LOCAL;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, "/var/run/devd.seqpacket.pipe", sizeof(sun.sun_path));

	if (-1 == connect(devd_skt, (struct sockaddr*)&sun, sizeof(sun)))
	{
		WLog_ERR(TAG, "Can't connect devd socket: error = %i - %s", errno, strerror(errno));
		goto err_out;
	}

	/* Get the file descriptor (fd) for the monitor.
	   This fd will get passed to select() */
	mon_fd = CreateFileDescriptorEvent(NULL, TRUE, FALSE, devd_skt);
	listobj[0] = searchman->term_event;
	listobj[1] = mon_fd;

	while (WaitForMultipleObjects(2, listobj, FALSE, INFINITE) != WAIT_OBJECT_0)
	{
		status = WaitForMultipleObjects(2, listobj, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return 0;
		}

		if (status == WAIT_OBJECT_0)
			break;

		errno = 0;
		WLog_DBG(TAG, "=======  SEARCH  ======= ");
		/* !system=USB subsystem=DEVICE type=ATTACH ugen=ugen3.3 cdev=ugen3.3 vendor=0x046d product=0x082d devclass=0xef devsubclass=0x02 sernum="6E7D726F" release=0x0011 mode=host port=4 parent=ugen3.1 */
		/* !system=USB subsystem=DEVICE type=DETACH ugen=ugen3.3 cdev=ugen3.3 vendor=0x046d product=0x082d devclass=0xef devsubclass=0x02 sernum="6E7D726F" release=0x0011 mode=host port=4 parent=ugen3.1 */
		data_size = read(devd_skt, buf, (sizeof(buf) - 1));

		if (data_size == -1)
		{
			WLog_ERR(TAG, "devd socket read: error = %i", errno);
			break;
		}

		buf[data_size] = 0;
		WLog_DBG(TAG, "devd event: %s", buf);

		if (buf[0] != '!') /* Skeep non notify events. */
			continue;

		/* Check: system=USB */
		val = devd_get_val(buf, data_size, "system", 6, &val_size);

		if (val == NULL || val_size != 3 || memcmp(val, "USB", 3) != 0)
			continue;

		/* Check: subsystem=DEVICE */
		val = devd_get_val(buf, data_size, "subsystem", 9, &val_size);

		if (val == NULL || val_size != 6 || memcmp(val, "DEVICE", 6) != 0)
			continue;

		/* Get event type. */
		val = devd_get_val(buf, data_size, "type", 4, &val_size);

		if (val == NULL || val_size != 6)
			continue;

		action = -1;

		if (memcmp(val, "ATTACH", 6) == 0)
			action = 0;

		if (memcmp(val, "DETACH", 6) == 0)
			action = 1;

		if (action == -1)
			continue; /* Skeep other actions. */

		/* Get bus and dev num. */
		/* ugen=ugen3.3 */
		val = devd_get_val(buf, data_size, "ugen", 4, &val_size);

		if (val == NULL || val_size < 7 || memcmp(val, "ugen", 4) != 0)
			continue;

		val += 4;
		val_size -= 4;
		ptr = memchr(val, '.', val_size);

		if (ptr == NULL)
			continue;

		/* Prepare strings. */
		ptr[0] = 0;
		ptr ++;
		val[val_size] = 0;
		/* Extract numbers. */
		busnum = strtol(val, NULL, 0);

		if (errno != 0)
			continue;

		devnum = strtol(ptr, NULL, 0);

		if (errno != 0)
			continue;

		/* Restore spaces. */
		ptr[-1] = ' ';
		val[val_size] = ' ';
		/* Handle event. */
		dvc_channel = NULL;

		switch (action)
		{
			case 0: /* ATTACH */
				sdev = NULL;
				success = 0;
				found = 0;
				/* vendor=0x046d */
				val = devd_get_val(buf, data_size, "vendor", 6, &val_size);

				if (val == NULL || val_size < 1)
					continue;

				val[val_size] = 0;
				idVendor = strtol(val, NULL, 16);

				if (errno != 0)
					continue;

				val[val_size] = ' ';
				/* product=0x082d */
				val = devd_get_val(buf, data_size, "product", 7, &val_size);

				if (val == NULL || val_size < 1)
					continue;

				val[val_size] = 0;
				idProduct = strtol(val, NULL, 16);

				if (errno != 0)
					continue;

				val[val_size] = ' ';
				WLog_DBG(TAG, "ATTACH: bus: %i, dev: %i, ven: %i, prod: %i", busnum, devnum, idVendor, idProduct);
				dvc_channel = channel_mgr->FindChannelById(channel_mgr, urbdrc->first_channel_id);
				searchman->rewind(searchman);

				while (dvc_channel && searchman->has_next(searchman))
				{
					sdev = searchman->get_next(searchman);

					if (sdev->idVendor == idVendor &&
					    sdev->idProduct == idProduct)
					{
						WLog_VRB(TAG, "Searchman Found Device: %04"PRIx16":%04"PRIx16"",
						         sdev->idVendor, sdev->idProduct);
						found = 1;
						break;
					}
				}

				if (!found && udevman->isAutoAdd(udevman))
				{
					WLog_VRB(TAG, "Auto Find Device: %04x:%04x ",
					         idVendor, idProduct);
					found = 2;
				}

				if (found)
				{
					success = udevman->register_udevice(udevman, busnum, devnum,
					                                    searchman->UsbDevice, 0, 0, UDEVMAN_FLAG_ADD_BY_ADDR);
				}

				if (success)
				{
					searchman->UsbDevice ++;
					usleep(400000);
					error = urdbrc_send_virtual_channel_add(dvc_channel, 0);

					if (found == 1)
						searchman->remove(searchman, sdev->idVendor, sdev->idProduct);
				}

				break;

			case 1: /* DETACH */
				pdev = NULL;
				on_close = 0;
				WLog_DBG(TAG, "DETACH: bus: %i, dev: %i", busnum, devnum);
				usleep(500000);
				udevman->loading_lock(udevman);
				udevman->rewind(udevman);

				while (udevman->has_next(udevman))
				{
					pdev = udevman->get_next(udevman);

					if (pdev->get_bus_number(pdev) == busnum &&
					    pdev->get_dev_number(pdev) == devnum)
					{
						dvc_channel = channel_mgr->FindChannelById(channel_mgr, pdev->get_channel_id(pdev));

						if (dvc_channel == NULL)
						{
							WLog_ERR(TAG, "SEARCH: dvc_channel %d is NULL!!", pdev->get_channel_id(pdev));
							func_close_udevice(searchman, pdev);
							break;
						}

						if (!pdev->isSigToEnd(pdev))
						{
							dvc_channel->Write(dvc_channel, 0, NULL, NULL);
							pdev->SigToEnd(pdev);
						}

						on_close = 1;
						break;
					}
				}

				udevman->loading_unlock(udevman);
				usleep(300000);

				if (pdev && on_close && dvc_channel &&
				    pdev->isSigToEnd(pdev) &&
				    !(pdev->isChannelClosed(pdev)))
				{
					dvc_channel->Close(dvc_channel);
				}

				break;
		}
	}

	CloseHandle(mon_fd);
err_out:
	close(devd_skt);
	sem_post(&searchman->sem_term);
	WLog_DBG(TAG, "urbdrc_search_usb_device - devd: end");
	return 0;
}
#endif
#if defined (__linux__)
static void* urbdrc_search_usb_device(void* arg)
{
	USB_SEARCHMAN* searchman = (USB_SEARCHMAN*) arg;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) searchman->urbdrc;
	IUDEVMAN* udevman = urbdrc->udevman;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* dvc_channel;
	USB_SEARCHDEV* sdev;
	IUDEVICE* pdev = NULL;
	HANDLE listobj[2];
	HANDLE mon_fd;
	int numobj, timeout;
	long busnum, devnum;
	int success = 0, on_close = 0, found = 0;
	WLog_VRB(TAG, "");
	channel_mgr = urbdrc->listener_callback->channel_mgr;
	DWORD status;
	DWORD dwError;
	/* init usb monitor */
	struct udev* udev;
	struct udev_device* dev;
	struct udev_monitor* mon;
	udev = udev_new();

	if (!udev)
	{
		WLog_ERR(TAG,  "Can't create udev");
		return 0;
	}

	/* Set up a monitor to monitor usb devices */
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
	udev_monitor_enable_receiving(mon);
	/* Get the file descriptor (fd) for the monitor.
	   This fd will get passed to select() */
	mon_fd = CreateFileDescriptorEvent(NULL, TRUE, FALSE,
	                                   udev_monitor_get_fd(mon), WINPR_FD_READ);

	if (!mon_fd)
		goto fail_create_monfd_event;

	while (1)
	{
		WLog_VRB(TAG, "=======  SEARCH  ======= ");
		busnum = 0;
		devnum = 0;
		sdev = NULL;
		pdev = NULL;
		dvc_channel = NULL;
		on_close = 0;
		listobj[0] = searchman->term_event;
		listobj[1] = mon_fd;
		numobj = 2;
		status = WaitForMultipleObjects(numobj, listobj, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			dwError = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", dwError);
			goto out;
		}

		status = WaitForSingleObject(searchman->term_event, 0);

		if (status == WAIT_FAILED)
		{
			dwError = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", dwError);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			sem_post(&searchman->sem_term);
			goto out;
		}

		status = WaitForSingleObject(mon_fd, 0);

		if (status == WAIT_FAILED)
		{
			dwError = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", dwError);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			dev = udev_monitor_receive_device(mon);

			if (dev)
			{
				const char* action = udev_device_get_action(dev);

				if (strcmp(action, "add") == 0)
				{
					long idVendor, idProduct;
					success = 0;
					found = 0;
					idVendor = strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16);

					if (errno != 0)
						continue;

					idProduct = strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16);

					if (errno != 0)
						continue;

					if (idVendor < 0 || idProduct < 0)
					{
						udev_device_unref(dev);
						continue;
					}

					busnum = strtol(udev_device_get_property_value(dev, "BUSNUM"), NULL, 0);

					if (errno != 0)
						continue;

					devnum = strtol(udev_device_get_property_value(dev, "DEVNUM"), NULL, 0);

					if (errno != 0)
						continue;

					dvc_channel = channel_mgr->FindChannelById(channel_mgr,
					              urbdrc->first_channel_id);
					searchman->rewind(searchman);

					while (dvc_channel && searchman->has_next(searchman))
					{
						sdev = searchman->get_next(searchman);

						if (sdev->idVendor == idVendor &&
						    sdev->idProduct == idProduct)
						{
							WLog_VRB(TAG, "Searchman Find Device: %04"PRIx16":%04"PRIx16"",
							         sdev->idVendor, sdev->idProduct);
							found = 1;
							break;
						}
					}

					if (!found && udevman->isAutoAdd(udevman))
					{
						WLog_VRB(TAG, "Auto Find Device: %04x:%04x ",
						         idVendor, idProduct);
						found = 2;
					}

					if (found)
					{
						success = udevman->register_udevice(udevman, busnum, devnum,
						                                    searchman->UsbDevice, 0, 0, UDEVMAN_FLAG_ADD_BY_ADDR);
					}

					if (success)
					{
						searchman->UsbDevice++;
						/* when we send the usb device add request,
						 * we will detach the device driver at same
						 * time. But, if the time of detach the
						 * driver and attach driver is too close,
						 * the system will crash. workaround: we
						 * wait it for some time to avoid system
						 * crash. */
						listobj[0] = searchman->term_event;
						numobj = 1;
						timeout = 4000; /* milliseconds */
						status = WaitForMultipleObjects(numobj, listobj, FALSE, timeout);

						if (status == WAIT_FAILED)
						{
							dwError = GetLastError();
							WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", dwError);
							goto out;
						}

						status = WaitForSingleObject(searchman->term_event, 0);

						if (status == WAIT_FAILED)
						{
							dwError = GetLastError();
							WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", dwError);
							goto out;
						}

						if (status == WAIT_OBJECT_0)
						{
							CloseHandle(mon_fd);
							sem_post(&searchman->sem_term);
							return 0;
						}

						urdbrc_send_virtual_channel_add(dvc_channel, 0);

						if (found == 1)
							searchman->remove(searchman, sdev->idVendor, sdev->idProduct);
					}
				}
				else if (strcmp(action, "remove") == 0)
				{
					busnum = strtol(udev_device_get_property_value(dev, "BUSNUM"), NULL, 0);

					if (errno != 0)
						goto out;

					devnum = strtol(udev_device_get_property_value(dev, "DEVNUM"), NULL, 0);

					if (errno != 0)
						goto out;

					usleep(500000);
					udevman->loading_lock(udevman);
					udevman->rewind(udevman);

					while (udevman->has_next(udevman))
					{
						pdev = udevman->get_next(udevman);

						if (pdev->get_bus_number(pdev) == busnum && pdev->get_dev_number(pdev) == devnum)
						{
							dvc_channel = channel_mgr->FindChannelById(channel_mgr, pdev->get_channel_id(pdev));

							if (dvc_channel == NULL)
							{
								WLog_ERR(TAG, "SEARCH: dvc_channel %d is NULL!!", pdev->get_channel_id(pdev));
								func_close_udevice(searchman, pdev);
								break;
							}

							if (!pdev->isSigToEnd(pdev))
							{
								dvc_channel->Write(dvc_channel, 0, NULL, NULL);
								pdev->SigToEnd(pdev);
							}

							on_close = 1;
							break;
						}
					}

					udevman->loading_unlock(udevman);
					listobj[0] = searchman->term_event;
					numobj = 1;
					timeout = 3000; /* milliseconds */
					status = WaitForMultipleObjects(numobj, listobj, FALSE, timeout);

					if (status == WAIT_FAILED)
					{
						dwError = GetLastError();
						WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", dwError);
						goto out;
					}

					status = WaitForSingleObject(searchman->term_event, 0);

					if (status == WAIT_FAILED)
					{
						dwError = GetLastError();
						WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", dwError);
						goto out;
					}

					if (status == WAIT_OBJECT_0)
					{
						CloseHandle(mon_fd);
						sem_post(&searchman->sem_term);
						return 0;
					}

					if (pdev && on_close && dvc_channel && pdev->isSigToEnd(pdev) && !(pdev->isChannelClosed(pdev)))
					{
						on_close = 0;
						dvc_channel->Close(dvc_channel);
					}
				}

				udev_device_unref(dev);
			}
			else
			{
				WLog_ERR(TAG,  "No Device from receive_device(). An error occurred.");
			}
		}
	}

out:
	CloseHandle(mon_fd);
fail_create_monfd_event:
	sem_post(&searchman->sem_term);
	return 0;
}
#endif

void* urbdrc_new_device_create(void* arg)
{
	TRANSFER_DATA* transfer_data = (TRANSFER_DATA*) arg;
	URBDRC_CHANNEL_CALLBACK* callback = transfer_data->callback;
	IWTSVirtualChannelManager* channel_mgr;
	URBDRC_PLUGIN* urbdrc = transfer_data->urbdrc;
	USB_SEARCHMAN* searchman = urbdrc->searchman;
	BYTE* pBuffer = transfer_data->pBuffer;
	IUDEVMAN* udevman = transfer_data->udevman;
	IUDEVICE* pdev = NULL;
	UINT32 ChannelId = 0;
	UINT32 MessageId;
	UINT32 FunctionId;
	int i = 0, found = 0;
	WLog_DBG(TAG, "...");
	channel_mgr = urbdrc->listener_callback->channel_mgr;
	ChannelId = channel_mgr->GetChannelId(callback->channel);
	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);
	int error = 0;

	switch (urbdrc->vchannel_status)
	{
		case INIT_CHANNEL_IN:
			urbdrc->first_channel_id = ChannelId;

			if (!searchman->start(searchman, urbdrc_search_usb_device))
			{
				WLog_ERR(TAG, "unable to start searchman thread");
				return 0;
			}

			for (i = 0; i < udevman->get_device_num(udevman); i++)
				error = urdbrc_send_virtual_channel_add(callback->channel, MessageId);

			urbdrc->vchannel_status = INIT_CHANNEL_OUT;
			break;

		case INIT_CHANNEL_OUT:
			udevman->loading_lock(udevman);
			udevman->rewind(udevman);

			while (udevman->has_next(udevman))
			{
				pdev = udevman->get_next(udevman);

				if (!pdev->isAlreadySend(pdev))
				{
					found = 1;
					pdev->setAlreadySend(pdev);
					pdev->set_channel_id(pdev, ChannelId);
					break;
				}
			}

			udevman->loading_unlock(udevman);

			if (found && pdev->isAlreadySend(pdev))
			{
				/* when we send the usb device add request, we will detach
				 * the device driver at same time. But, if the time of detach the
				 * driver and attach driver is too close, the system will crash.
				 * workaround: we wait it for some time to avoid system crash. */
				error = pdev->wait_for_detach(pdev);

				if (error >= 0)
					urdbrc_send_usb_device_add(callback, pdev);
			}

			break;

		default:
			WLog_ERR(TAG, "vchannel_status unknown value %"PRIu32"",
			         urbdrc->vchannel_status);
			break;
	}

	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_channel_notification(URBDRC_CHANNEL_CALLBACK* callback, char* pBuffer,
        UINT32 cbSize)
{
	int i;
	UINT32 MessageId;
	UINT32 FunctionId;
	UINT error = CHANNEL_RC_OK;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) callback->plugin;
	WLog_DBG(TAG, "...");
	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);

	switch (FunctionId)
	{
		case CHANNEL_CREATED:
			error = urbdrc_process_channel_create(callback, pBuffer + 8, cbSize - 8, MessageId);
			break;

		case RIMCALL_RELEASE:
			WLog_VRB(TAG, "recv RIMCALL_RELEASE");
			pthread_t thread;
			TRANSFER_DATA*  transfer_data;
			transfer_data = (TRANSFER_DATA*)malloc(sizeof(TRANSFER_DATA));

			if (!transfer_data)
				return ERROR_OUTOFMEMORY;

			transfer_data->callback = callback;
			transfer_data->urbdrc = urbdrc;
			transfer_data->udevman = urbdrc->udevman;
			transfer_data->urbdrc = urbdrc;
			transfer_data->cbSize = cbSize;
			transfer_data->pBuffer = (BYTE*) malloc((cbSize));

			if (!transfer_data->pBuffer)
			{
				free(transfer_data);
				return ERROR_OUTOFMEMORY;
			}

			for (i = 0; i < (cbSize); i++)
			{
				transfer_data->pBuffer[i] = pBuffer[i];
			}

			if (pthread_create(&thread, 0, urbdrc_new_device_create, transfer_data) != 0)
			{
				free(transfer_data->pBuffer);
				free(transfer_data);
				return ERROR_INVALID_OPERATION;
			}

			pthread_detach(thread);
			break;

		default:
			WLog_VRB(TAG, "%s: unknown FunctionId 0x%"PRIX32"", __FUNCTION__, FunctionId);
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
	URBDRC_CHANNEL_CALLBACK* callback = (URBDRC_CHANNEL_CALLBACK*) pChannelCallback;
	URBDRC_PLUGIN* urbdrc;
	IUDEVMAN* udevman;
	UINT32 InterfaceTemp;
	UINT32 InterfaceId;
	UINT32 Mask;
	UINT error = CHANNEL_RC_OK;
	char* pBuffer = (char*)Stream_Pointer(data);
	UINT32 cbSize = Stream_GetRemainingLength(data);

	if (callback == NULL)
		return 0;

	if (callback->plugin == NULL)
		return 0;

	urbdrc = (URBDRC_PLUGIN*) callback->plugin;

	if (urbdrc->udevman == NULL)
		return 0;

	udevman = (IUDEVMAN*) urbdrc->udevman;
	data_read_UINT32(pBuffer + 0, InterfaceTemp);
	InterfaceId = (InterfaceTemp & 0x0fffffff);
	Mask = ((InterfaceTemp & 0xf0000000) >> 30);
	WLog_VRB(TAG, "Size=%"PRIu32" InterfaceId=0x%"PRIX32" Mask=0x%"PRIX32"", cbSize, InterfaceId, Mask);

	switch (InterfaceId)
	{
		case CAPABILITIES_NEGOTIATOR:
			error = urbdrc_exchange_capabilities(callback, pBuffer + 4, cbSize - 4);
			break;

		case SERVER_CHANNEL_NOTIFICATION:
			error = urbdrc_process_channel_notification(callback, pBuffer + 4, cbSize - 4);
			break;

		default:
			WLog_VRB(TAG, "InterfaceId 0x%"PRIX32" Start matching devices list", InterfaceId);
			pthread_t thread;
			TRANSFER_DATA* transfer_data;
			transfer_data = (TRANSFER_DATA*)malloc(sizeof(TRANSFER_DATA));

			if (!transfer_data)
			{
				WLog_ERR(TAG, "transfer_data is NULL!!");
				return ERROR_OUTOFMEMORY;
			}

			transfer_data->callback = callback;
			transfer_data->urbdrc = urbdrc;
			transfer_data->udevman = udevman;
			transfer_data->cbSize = cbSize - 4;
			transfer_data->UsbDevice = InterfaceId;
			transfer_data->pBuffer = (BYTE*)malloc((cbSize - 4));

			if (!transfer_data->pBuffer)
			{
				free(transfer_data);
				return ERROR_OUTOFMEMORY;
			}

			memcpy(transfer_data->pBuffer, pBuffer + 4, (cbSize - 4));
			/* To ensure that not too many urb requests at the same time */
			udevman->wait_urb(udevman);
#if ISOCH_FIFO
			/* lock isoch mutex */
			func_lock_isoch_mutex(transfer_data);
#endif
			error = pthread_create(&thread, 0, urbdrc_process_udev_data_transfer, transfer_data);

			if (error != 0)
			{
				WLog_ERR(TAG, "Create Data Transfer Thread got error = %"PRIu32"", error);
				free(transfer_data->pBuffer);
				free(transfer_data);
				return ERROR_INVALID_OPERATION;
			}

			pthread_detach(thread);
			break;
	}

	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	URBDRC_CHANNEL_CALLBACK* callback = (URBDRC_CHANNEL_CALLBACK*) pChannelCallback;
	URBDRC_PLUGIN* urbdrc  = (URBDRC_PLUGIN*) callback->plugin;
	IUDEVMAN* udevman = (IUDEVMAN*) urbdrc->udevman;
	USB_SEARCHMAN* searchman = (USB_SEARCHMAN*) urbdrc->searchman;
	IUDEVICE* pdev = NULL;
	UINT32 ChannelId = 0;
	int found = 0;
	ChannelId = callback->channel_mgr->GetChannelId(callback->channel);
	WLog_INFO(TAG, "urbdrc_on_close: channel id %"PRIu32"", ChannelId);
	udevman->loading_lock(udevman);
	udevman->rewind(udevman);

	while (udevman->has_next(udevman))
	{
		pdev = udevman->get_next(udevman);

		if (pdev->get_channel_id(pdev) == ChannelId)
		{
			found = 1;
			break;
		}
	}

	udevman->loading_unlock(udevman);

	if (found && pdev && !(pdev->isChannelClosed(pdev)))
	{
		pdev->setChannelClosed(pdev);
		func_close_udevice(searchman, pdev);
	}

	zfree(callback);
	WLog_DBG(TAG, "success");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
        IWTSVirtualChannel* pChannel, BYTE* pData, BOOL* pbAccept, IWTSVirtualChannelCallback** ppCallback)
{
	URBDRC_LISTENER_CALLBACK* listener_callback = (URBDRC_LISTENER_CALLBACK*) pListenerCallback;
	URBDRC_CHANNEL_CALLBACK* callback;
	WLog_VRB(TAG, "");
	callback = (URBDRC_CHANNEL_CALLBACK*) calloc(1, sizeof(URBDRC_CHANNEL_CALLBACK));

	if (!callback)
		return ERROR_OUTOFMEMORY;

	callback->iface.OnDataReceived = urbdrc_on_data_received;
	callback->iface.OnClose = urbdrc_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*) callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) pPlugin;
	IUDEVMAN* udevman = NULL;
	USB_SEARCHMAN* searchman = NULL;
	WLog_VRB(TAG, "");
	urbdrc->listener_callback = (URBDRC_LISTENER_CALLBACK*) calloc(1, sizeof(URBDRC_LISTENER_CALLBACK));

	if (!urbdrc->listener_callback)
		return CHANNEL_RC_NO_MEMORY;

	urbdrc->listener_callback->iface.OnNewChannelConnection = urbdrc_on_new_channel_connection;
	urbdrc->listener_callback->plugin = pPlugin;
	urbdrc->listener_callback->channel_mgr = pChannelMgr;
	/* Init searchman */
	udevman = urbdrc->udevman;
	searchman = searchman_new((void*) urbdrc, udevman->get_defUsbDevice(udevman));

	if (!searchman)
	{
		free(urbdrc->listener_callback);
		urbdrc->listener_callback = NULL;
		return CHANNEL_RC_NO_MEMORY;
	}

	urbdrc->searchman = searchman;
	return pChannelMgr->CreateListener(pChannelMgr, "URBDRC", 0,
	                                   (IWTSListenerCallback*) urbdrc->listener_callback, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_plugin_terminated(IWTSPlugin* pPlugin)
{
	URBDRC_PLUGIN*	urbdrc = (URBDRC_PLUGIN*) pPlugin;
	IUDEVMAN* udevman = urbdrc->udevman;
	USB_SEARCHMAN* searchman = urbdrc->searchman;
	WLog_VRB(TAG, "");

	if (searchman)
	{
		/* close searchman */
		searchman->close(searchman);

		/* free searchman */
		if (searchman->started)
		{
			struct timespec ts;
			ts.tv_sec = time(NULL) + 10;
			ts.tv_nsec = 0;
			sem_timedwait(&searchman->sem_term, &ts);
		}

		searchman->free(searchman);
		searchman = NULL;
	}

	if (udevman)
	{
		udevman->free(udevman);
		udevman = NULL;
	}

	if (urbdrc->listener_callback)
		zfree(urbdrc->listener_callback);

	if (urbdrc)
		zfree(urbdrc);

	return CHANNEL_RC_OK;
}

static void urbdrc_register_udevman_addin(IWTSPlugin* pPlugin, IUDEVMAN* udevman)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) pPlugin;

	if (urbdrc->udevman)
	{
		WLog_ERR(TAG, "existing device, abort.");
		return;
	}

	DEBUG_DVC("device registered.");
	urbdrc->udevman = udevman;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_load_udevman_addin(IWTSPlugin* pPlugin, const char* name, ADDIN_ARGV* args)
{
	PFREERDP_URBDRC_DEVICE_ENTRY entry;
	FREERDP_URBDRC_SERVICE_ENTRY_POINTS entryPoints;
	entry = (PFREERDP_URBDRC_DEVICE_ENTRY) freerdp_load_channel_addin_entry("urbdrc", (LPSTR) name,
	        NULL, 0);

	if (!entry)
		return ERROR_INVALID_OPERATION;

	entryPoints.plugin = pPlugin;
	entryPoints.pRegisterUDEVMAN = urbdrc_register_udevman_addin;
	entryPoints.args = args;

	if (entry(&entryPoints) != 0)
	{
		WLog_ERR(TAG, "%s entry returns error.", name);
		return ERROR_INVALID_OPERATION;
	}

	return CHANNEL_RC_OK;
}

BOOL urbdrc_set_subsystem(URBDRC_PLUGIN* urbdrc, char* subsystem)
{
	free(urbdrc->subsystem);
	urbdrc->subsystem = _strdup(subsystem);
	return (urbdrc->subsystem != NULL);
}

COMMAND_LINE_ARGUMENT_A urbdrc_args[] =
{
	{ "dbg", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "debug" },
	{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT urbdrc_process_addin_args(URBDRC_PLUGIN* urbdrc, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;
	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
	                                    urbdrc_args, flags, urbdrc, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_DATA;

	arg = urbdrc_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "dbg")
		{
			WLog_SetLogLevel(WLog_Get(TAG), WLOG_TRACE);
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
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry urbdrc_DVCPluginEntry
#else
#define DVCPluginEntry FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT status = 0;
	ADDIN_ARGV* args;
	URBDRC_PLUGIN* urbdrc;
	urbdrc = (URBDRC_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "urbdrc");
	args = pEntryPoints->GetPluginData(pEntryPoints);

	if (urbdrc == NULL)
	{
		urbdrc = (URBDRC_PLUGIN*) calloc(1, sizeof(URBDRC_PLUGIN));

		if (!urbdrc)
			return CHANNEL_RC_NO_MEMORY;

		urbdrc->iface.Initialize = urbdrc_plugin_initialize;
		urbdrc->iface.Connected = NULL;
		urbdrc->iface.Disconnected = NULL;
		urbdrc->iface.Terminated = urbdrc_plugin_terminated;
		urbdrc->searchman = NULL;
		urbdrc->vchannel_status = INIT_CHANNEL_IN;
		status = pEntryPoints->RegisterPlugin(pEntryPoints, "urbdrc", (IWTSPlugin*) urbdrc);

		if (status != CHANNEL_RC_OK)
			goto error_register;
	}

	status = urbdrc_process_addin_args(urbdrc, args);

	if (status != CHANNEL_RC_OK)
	{
		/* TODO: we should unregister the plugin ? */
		WLog_ERR(TAG, "error processing arguments");
		//return status;
	}

	if (!urbdrc->subsystem && !urbdrc_set_subsystem(urbdrc, "libusb"))
	{
		/* TODO: we should unregister the plugin ? */
		WLog_ERR(TAG, "error setting subsystem");
		return ERROR_OUTOFMEMORY;
	}

	return urbdrc_load_udevman_addin((IWTSPlugin*) urbdrc, urbdrc->subsystem, args);
error_register:
	free(urbdrc);
	return status;
}
