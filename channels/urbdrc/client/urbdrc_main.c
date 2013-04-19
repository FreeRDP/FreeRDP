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
#include <unistd.h>
#include <time.h>
#include <libudev.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/cmdline.h>

#include <freerdp/dvc.h>
#include <freerdp/addin.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"
#include "data_transfer.h"
#include "searchman.h"

int urbdrc_debug = 0;

static int func_hardware_id_format(IUDEVICE* pdev, char (*HardwareIds)[DEVICE_HARDWARE_ID_SIZE])
{
	char str[DEVICE_HARDWARE_ID_SIZE];
	int idVendor, idProduct, bcdDevice;

	memset(str, 0, DEVICE_HARDWARE_ID_SIZE);

	idVendor = pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = pdev->query_device_descriptor(pdev, ID_PRODUCT);
	bcdDevice = pdev->query_device_descriptor(pdev, BCD_DEVICE);

	sprintf(str, "USB\\VID_%04X&PID_%04X", idVendor, idProduct);
	strcpy(HardwareIds[1], str);
	sprintf(str, "%s&REV_%04X", str, bcdDevice);
	strcpy(HardwareIds[0], str);

	return 0;
}

static int func_compat_id_format(IUDEVICE* pdev, char (*CompatibilityIds)[DEVICE_COMPATIBILITY_ID_SIZE])
{
	char str[DEVICE_COMPATIBILITY_ID_SIZE];
	int bDeviceClass, bDeviceSubClass, bDeviceProtocol;

	bDeviceClass = pdev->query_device_descriptor(pdev, B_DEVICE_CLASS);
	bDeviceSubClass = pdev->query_device_descriptor(pdev, B_DEVICE_SUBCLASS);
	bDeviceProtocol = pdev->query_device_descriptor(pdev, B_DEVICE_PROTOCOL);

	if(!(pdev->isCompositeDevice(pdev)))
	{
		sprintf(str, "USB\\Class_%02X", bDeviceClass);
		strcpy(CompatibilityIds[2], str);
		sprintf(str, "%s&SubClass_%02X", str, bDeviceSubClass);
		strcpy(CompatibilityIds[1], str);
		sprintf(str, "%s&Prot_%02X", str, bDeviceProtocol);
		strcpy(CompatibilityIds[0], str);
	}
	else
	{
		sprintf(str, "USB\\DevClass_00");
		strcpy(CompatibilityIds[2], str);
		sprintf(str, "%s&SubClass_00", str);
		strcpy(CompatibilityIds[1], str);
		sprintf(str, "%s&Prot_00", str);
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
	char *p, *path;
	char containerId[17];
	int idVendor, idProduct;

	idVendor = pdev->query_device_descriptor(pdev, ID_VENDOR);
	idProduct = pdev->query_device_descriptor(pdev, ID_PRODUCT);

	path = pdev->getPath(pdev);

	if (strlen(path) > 8)
		p = (path + strlen(path)) - 8;
	else
		p = path;

	sprintf(containerId, "%04X%04X%s", idVendor, idProduct, p);

	/* format */
	sprintf(strContainerId,
		"{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		containerId[0], containerId[1],containerId[2], containerId[3],
		containerId[4], containerId[5], containerId[6], containerId[7],
		containerId[8], containerId[9], containerId[10], containerId[11],
		containerId[12], containerId[13], containerId[14], containerId[15]);

	return 0;
}

static int func_instance_id_generate(IUDEVICE* pdev, char* strInstanceId)
{
	char instanceId[17];

	memset(instanceId, 0, 17);
	sprintf(instanceId, "\\%s", pdev->getPath(pdev));

	/* format */
	sprintf(strInstanceId,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		instanceId[0], instanceId[1],instanceId[2], instanceId[3],
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
				noAck = (RequestField & 0x80000000)>>31;

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

static int urbdrc_process_capability_request(URBDRC_CHANNEL_CALLBACK* callback, char* data, UINT32 data_sizem, UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 Version;
	UINT32 out_size;
	char * out_data;

	LLOGLN(10, ("urbdrc_process_capability_request"));
	data_read_UINT32(data + 0, Version);

	InterfaceId = ((STREAM_ID_NONE<<30) | CAPABILITIES_NEGOTIATOR);

	out_size = 16;
	out_data = (char *) malloc(out_size);
	memset(out_data, 0, out_size);
	data_write_UINT32(out_data + 0, InterfaceId); /* interface id */
	data_write_UINT32(out_data + 4, MessageId); /* message id */
	data_write_UINT32(out_data + 8, Version); /* usb protocol version */
	data_write_UINT32(out_data + 12, 0x00000000); /* HRESULT */
	callback->channel->Write(callback->channel, out_size, (BYTE*) out_data, NULL);
	zfree(out_data);

	return 0;
}

static int urbdrc_process_channel_create(URBDRC_CHANNEL_CALLBACK* callback, char* data, UINT32 data_sizem, UINT32 MessageId)
{
	UINT32 InterfaceId;
	UINT32 out_size;
	UINT32 MajorVersion;
	UINT32 MinorVersion;
	UINT32 Capabilities;
	char* out_data;

	LLOGLN(10, ("urbdrc_process_channel_create"));
	data_read_UINT32(data + 0, MajorVersion);
	data_read_UINT32(data + 4, MinorVersion);
	data_read_UINT32(data + 8, Capabilities);

	InterfaceId = ((STREAM_ID_PROXY<<30) | CLIENT_CHANNEL_NOTIFICATION);

	out_size = 24;
	out_data = (char*) malloc(out_size);
	memset(out_data, 0, out_size);

	data_write_UINT32(out_data + 0, InterfaceId); /* interface id */
	data_write_UINT32(out_data + 4, MessageId); /* message id */
	data_write_UINT32(out_data + 8, CHANNEL_CREATED); /* function id */
	data_write_UINT32(out_data + 12, MajorVersion);
	data_write_UINT32(out_data + 16, MinorVersion);
	data_write_UINT32(out_data + 20, Capabilities); /* capabilities version */
	callback->channel->Write(callback->channel, out_size, (BYTE *)out_data, NULL);
	zfree(out_data);

	return 0;
}

static int urdbrc_send_virtual_channel_add(IWTSVirtualChannel* channel, UINT32 MessageId)
{
	UINT32 out_size;
	UINT32 InterfaceId;
	char* out_data;

	LLOGLN(10, ("urdbrc_send_virtual_channel_add"));

	InterfaceId = ((STREAM_ID_PROXY<<30) | CLIENT_DEVICE_SINK);

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

static int urdbrc_send_usb_device_add(URBDRC_CHANNEL_CALLBACK* callback, IUDEVICE* pdev)
{
	char* out_data;
	UINT32 InterfaceId;
	char HardwareIds[2][DEVICE_HARDWARE_ID_SIZE];
	char CompatibilityIds[3][DEVICE_COMPATIBILITY_ID_SIZE];
	char strContainerId[DEVICE_CONTAINER_STR_SIZE];
	char strInstanceId[DEVICE_INSTANCE_STR_SIZE];
	char* composite_str = "USB\\COMPOSITE";
	int size, out_offset, cchCompatIds, bcdUSB;

	LLOGLN(10, ("urdbrc_send_usb_device_add"));
	InterfaceId = ((STREAM_ID_PROXY<<30) | CLIENT_DEVICE_SINK);

	/* USB kernel driver detach!! */
	pdev->detach_kernel_driver(pdev);

#if ISOCH_FIFO
	/* create/initial isoch queue */
	pdev->set_isoch_queue(pdev, (void *)isoch_queue_new());
#endif

	func_hardware_id_format(pdev, HardwareIds);
	func_compat_id_format(pdev, CompatibilityIds);
	func_instance_id_generate(pdev, strInstanceId);
	func_container_id_generate(pdev, strContainerId);

	cchCompatIds = strlen(CompatibilityIds[0]) + 1 +
			strlen(CompatibilityIds[1]) + 1 +
			strlen(CompatibilityIds[2]) + 2;

	if (pdev->isCompositeDevice(pdev))
		cchCompatIds += strlen(composite_str)+1;

	out_offset = 24;
	size = 24;

	size += (strlen(strInstanceId)+1) * 2 +
			(strlen(HardwareIds[0]) + 1) * 2 + 4 +
			(strlen(HardwareIds[1]) + 1) * 2 + 2 +
			4 + (cchCompatIds) * 2 +
			(strlen(strContainerId) + 1) * 2 + 4 + 28;

	out_data = (char*) malloc(size);
	memset(out_data, 0, size);

	data_write_UINT32(out_data + 0, InterfaceId); /* interface */
	data_write_UINT32(out_data + 4, 0); /* message id */
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
	data_write_UINT16(out_data + out_offset, 0x0000); /* add "\0" */
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

	data_write_UINT16(out_data + out_offset, 0x0000); /* add "\0" */
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
	data_write_UINT32(out_data + out_offset + 16, 0x00000000); /* HcdCapabilities, MUST always be zero */

	if (bcdUSB < 0x200)
		data_write_UINT32(out_data + out_offset + 20, 0x00000000); /* DeviceIsHighSpeed */
	else
		data_write_UINT32(out_data + out_offset + 20, 0x00000001); /* DeviceIsHighSpeed */

	data_write_UINT32(out_data + out_offset + 24, 0x50); /* NoAckIsochWriteJitterBufferSizeInMs, >=10 or <=512 */
	out_offset += 28;

	callback->channel->Write(callback->channel, out_offset, (BYTE *)out_data, NULL);
	zfree(out_data);

	return 0;
}

static int urbdrc_exchange_capabilities(URBDRC_CHANNEL_CALLBACK* callback, char* pBuffer, UINT32 cbSize)
{
	UINT32 MessageId;
	UINT32 FunctionId;

	int error = 0;

	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);

	switch (FunctionId)
	{
		case RIM_EXCHANGE_CAPABILITY_REQUEST:
			error = urbdrc_process_capability_request(callback, pBuffer + 8, cbSize - 8, MessageId);
			break;

		default:
			LLOGLN(10, ("urbdrc_exchange_capabilities: unknown FunctionId 0x%X", FunctionId));
			error = 1;
			break;
	}

	return error;
}

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
	int busnum, devnum;
	int success = 0, error, on_close = 0, found = 0;

	LLOGLN(10, ("urbdrc_search_usb_device: "));

	channel_mgr = urbdrc->listener_callback->channel_mgr;

	/* init usb monitor */
	struct udev* udev;
	struct udev_device* dev;
	struct udev_monitor* mon;

	udev = udev_new();

	if (!udev)
	{
		fprintf(stderr, "Can't create udev\n");
		return 0;
	}

	/* Set up a monitor to monitor usb devices */
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
	udev_monitor_enable_receiving(mon);

	/* Get the file descriptor (fd) for the monitor.
	   This fd will get passed to select() */
	mon_fd = CreateFileDescriptorEvent(NULL, TRUE, FALSE, udev_monitor_get_fd(mon));

	while (1)
	{
		LLOGLN(10, ("=======  SEARCH  ======= "));
		busnum = 0;
		devnum = 0;
		sdev = NULL;
		pdev = NULL;
		dvc_channel = NULL;   
		on_close = 0;
		listobj[0] = searchman->term_event;
		listobj[1] = mon_fd;
		numobj = 2;

		WaitForMultipleObjects(numobj, listobj, FALSE, INFINITE);

		if (WaitForSingleObject(searchman->term_event, 0) == WAIT_OBJECT_0)
		{
			sem_post(&searchman->sem_term);
			return 0;
		}

		if (WaitForSingleObject(mon_fd, 0) == WAIT_OBJECT_0)
		{
			dev = udev_monitor_receive_device(mon);

			if (dev)
			{
				const char* action = udev_device_get_action(dev);

				if (strcmp(action, "add") == 0)
				{
					int idVendor, idProduct;
					success = 0;
					found = 0;

					idVendor = strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16);
					idProduct = strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16);

					if (idVendor < 0 || idProduct < 0)
					{
						udev_device_unref(dev);
						continue;
					}

					busnum = atoi(udev_device_get_property_value(dev,"BUSNUM"));
					devnum = atoi(udev_device_get_property_value(dev,"DEVNUM"));

					dvc_channel = channel_mgr->FindChannelById(channel_mgr, 
						urbdrc->first_channel_id);

					searchman->rewind(searchman);

					while(dvc_channel && searchman->has_next(searchman))
					{
						sdev = searchman->get_next(searchman);

						if (sdev->idVendor == idVendor &&
							sdev->idProduct == idProduct)
						{
							LLOGLN(10, ("Searchman Find Device: %04x:%04x ", 
								sdev->idVendor, sdev->idProduct));
							found = 1;
							break;
						}
					}

					if (!found && udevman->isAutoAdd(udevman))
					{
						LLOGLN(10, ("Auto Find Device: %04x:%04x ", 
							idVendor, idProduct));
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

						WaitForMultipleObjects(numobj, listobj, FALSE, timeout);

						if (WaitForSingleObject(searchman->term_event, 0) == WAIT_OBJECT_0)
						{
							CloseHandle(mon_fd);
							sem_post(&searchman->sem_term);
							return 0;
						}

						error = urdbrc_send_virtual_channel_add(dvc_channel, 0);

						if (found == 1)
							searchman->remove(searchman, sdev->idVendor, sdev->idProduct);
					}
				}
				else if (strcmp(action, "remove") == 0)
				{
					busnum = atoi(udev_device_get_property_value(dev,"BUSNUM"));
					devnum = atoi(udev_device_get_property_value(dev,"DEVNUM"));

					usleep(500000);
					udevman->loading_lock(udevman);
					udevman->rewind(udevman);

					while(udevman->has_next(udevman))
					{
						pdev = udevman->get_next(udevman);

						if (pdev->get_bus_number(pdev) == busnum && pdev->get_dev_number(pdev) == devnum)
						{
							dvc_channel = channel_mgr->FindChannelById(channel_mgr, pdev->get_channel_id(pdev));

							if (dvc_channel == NULL)
							{
								LLOGLN(0, ("SEARCH: dvc_channel %d is NULL!!", pdev->get_channel_id(pdev)));
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

					WaitForMultipleObjects(numobj, listobj, FALSE, timeout);

					if (WaitForSingleObject(searchman->term_event, 0) == WAIT_OBJECT_0)
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
				fprintf(stderr, "No Device from receive_device(). An error occured.\n");
			}
		}
	}

	CloseHandle(mon_fd);
	sem_post(&searchman->sem_term);

	return 0;
}

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

	channel_mgr = urbdrc->listener_callback->channel_mgr;
	ChannelId =  channel_mgr->GetChannelId(callback->channel);

	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);

	int error = 0;

	switch (urbdrc->vchannel_status)
	{
		case INIT_CHANNEL_IN:
			urbdrc->first_channel_id = ChannelId;
			searchman->start(searchman, urbdrc_search_usb_device);
			
			for (i = 0; i < udevman->get_device_num(udevman); i++)
				error = urdbrc_send_virtual_channel_add(callback->channel, MessageId);

			urbdrc->vchannel_status = INIT_CHANNEL_OUT;
			break;

		case INIT_CHANNEL_OUT:
			udevman->loading_lock(udevman);
			udevman->rewind(udevman);

			while(udevman->has_next(udevman))
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
			LLOGLN(0, ("urbdrc_new_device_create: vchannel_status unknown value %d",
											urbdrc->vchannel_status));
			break;
	}

	return 0;
}

static int urbdrc_process_channel_notification(URBDRC_CHANNEL_CALLBACK* callback, char* pBuffer, UINT32 cbSize)
{
	int i, error = 0;
	UINT32 MessageId;
	UINT32 FunctionId;
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) callback->plugin;

	data_read_UINT32(pBuffer + 0, MessageId);
	data_read_UINT32(pBuffer + 4, FunctionId);

	switch (FunctionId)
	{
		case CHANNEL_CREATED:
			error = urbdrc_process_channel_create(callback, pBuffer + 8, cbSize - 8, MessageId);
			break;

		case RIMCALL_RELEASE:
			LLOGLN(10, ("urbdrc_process_channel_notification: recv RIMCALL_RELEASE"));
			pthread_t   thread;

			TRANSFER_DATA*  transfer_data;

			transfer_data = (TRANSFER_DATA*)malloc(sizeof(TRANSFER_DATA));
			transfer_data->callback = callback;
			transfer_data->urbdrc = urbdrc;
			transfer_data->udevman = urbdrc->udevman;
			transfer_data->urbdrc = urbdrc;
			transfer_data->cbSize = cbSize;
			transfer_data->pBuffer = (BYTE*) malloc((cbSize));

			for (i = 0; i < (cbSize); i++)
			{
				transfer_data->pBuffer[i] = pBuffer[i];
			}

			pthread_create(&thread, 0, urbdrc_new_device_create, transfer_data);
			pthread_detach(thread);
			break;

		default:
			LLOGLN(10, ("urbdrc_process_channel_notification: unknown FunctionId 0x%X", FunctionId));
			error = 1;
			break;
	}
	return error;
}

static int urbdrc_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* Buffer)
{
	URBDRC_CHANNEL_CALLBACK* callback = (URBDRC_CHANNEL_CALLBACK*) pChannelCallback;
	URBDRC_PLUGIN* urbdrc;
	IUDEVMAN* udevman;
	UINT32 InterfaceTemp;
	UINT32 InterfaceId;
	UINT32 Mask;
	int error = 0;
	char* pBuffer = (char*) Buffer;

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
	Mask = ((InterfaceTemp & 0xf0000000)>>30);
	LLOGLN(10, ("urbdrc_on_data_received: Size=%d InterfaceId=0x%X Mask=0x%X", cbSize, InterfaceId, Mask));

	switch (InterfaceId)
	{
		case CAPABILITIES_NEGOTIATOR:
			error = urbdrc_exchange_capabilities(callback, pBuffer + 4, cbSize - 4);
			break;

		case SERVER_CHANNEL_NOTIFICATION:
			error = urbdrc_process_channel_notification(callback, pBuffer + 4, cbSize - 4);
			break;

		default:
			LLOGLN(10, ("urbdrc_on_data_received: InterfaceId 0x%X Start matching devices list", InterfaceId));
			pthread_t thread;
			TRANSFER_DATA* transfer_data;

			transfer_data = (TRANSFER_DATA*) malloc(sizeof(TRANSFER_DATA));

			if (transfer_data == NULL)
				fprintf(stderr, "transfer_data is NULL!!");

			transfer_data->callback = callback;
			transfer_data->urbdrc = urbdrc;
			transfer_data->udevman = udevman;
			transfer_data->cbSize = cbSize - 4;
			transfer_data->UsbDevice = InterfaceId;
			transfer_data->pBuffer = (BYTE *)malloc((cbSize - 4));

			memcpy(transfer_data->pBuffer, pBuffer + 4, (cbSize - 4));

			/* To ensure that not too many urb requests at the same time */
			udevman->wait_urb(udevman);

#if ISOCH_FIFO
			/* lock isoch mutex */
			func_lock_isoch_mutex(transfer_data);
#endif

			error = pthread_create(&thread, 0, urbdrc_process_udev_data_transfer, transfer_data);

			if (error < 0)
				LLOGLN(0, ("Create Data Transfer Thread got error = %d", error));
			else
				pthread_detach(thread);

			break;
	}

	return 0;
}

static int urbdrc_on_close(IWTSVirtualChannelCallback * pChannelCallback)
{
	URBDRC_CHANNEL_CALLBACK* callback = (URBDRC_CHANNEL_CALLBACK*) pChannelCallback;
	URBDRC_PLUGIN* urbdrc  = (URBDRC_PLUGIN*) callback->plugin;
	IUDEVMAN* udevman = (IUDEVMAN*) urbdrc->udevman;
	USB_SEARCHMAN* searchman = (USB_SEARCHMAN*) urbdrc->searchman;
	IUDEVICE* pdev = NULL;
	UINT32 ChannelId = 0;
	int found = 0;

	ChannelId = callback->channel_mgr->GetChannelId(callback->channel);

	LLOGLN(0, ("urbdrc_on_close: channel id %d", ChannelId));

	udevman->loading_lock(udevman);
	udevman->rewind(udevman);

	while(udevman->has_next(udevman))
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

	LLOGLN(urbdrc_debug, ("urbdrc_on_close: success"));

	return 0;
}

static int urbdrc_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel * pChannel, BYTE* pData, int* pbAccept, IWTSVirtualChannelCallback** ppCallback)
{
	URBDRC_LISTENER_CALLBACK* listener_callback = (URBDRC_LISTENER_CALLBACK*) pListenerCallback;
	URBDRC_CHANNEL_CALLBACK* callback;

	LLOGLN(10, ("urbdrc_on_new_channel_connection:"));
	callback = (URBDRC_CHANNEL_CALLBACK*) malloc(sizeof(URBDRC_CHANNEL_CALLBACK));
	callback->iface.OnDataReceived = urbdrc_on_data_received;
	callback->iface.OnClose = urbdrc_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int urbdrc_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) pPlugin;
	IUDEVMAN* udevman = NULL;
	USB_SEARCHMAN* searchman = NULL;

	LLOGLN(10, ("urbdrc_plugin_initialize:"));
	urbdrc->listener_callback = (URBDRC_LISTENER_CALLBACK*) malloc(sizeof(URBDRC_LISTENER_CALLBACK));
	memset(urbdrc->listener_callback, 0, sizeof(URBDRC_LISTENER_CALLBACK));

	urbdrc->listener_callback->iface.OnNewChannelConnection = urbdrc_on_new_channel_connection;
	urbdrc->listener_callback->plugin = pPlugin;
	urbdrc->listener_callback->channel_mgr = pChannelMgr;

	/* Init searchman */
	udevman = urbdrc->udevman;
	searchman = searchman_new((void*) urbdrc, udevman->get_defUsbDevice(udevman));
	urbdrc->searchman = searchman;

	return pChannelMgr->CreateListener(pChannelMgr, "URBDRC", 0,
		(IWTSListenerCallback*) urbdrc->listener_callback, NULL);
}

static int urbdrc_plugin_terminated(IWTSPlugin* pPlugin)
{
	URBDRC_PLUGIN*	urbdrc = (URBDRC_PLUGIN*) pPlugin;
	IUDEVMAN* udevman = urbdrc->udevman;
	USB_SEARCHMAN* searchman = urbdrc->searchman;

	LLOGLN(10, ("urbdrc_plugin_terminated:"));

	if (searchman)
	{
		/* close searchman */
		searchman->close(searchman);

		/* free searchman */
		if (searchman->strated)
		{
			struct timespec ts;
			ts.tv_sec = time(NULL)+10;
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

	if(urbdrc)
		zfree(urbdrc);

	return 0;
}

static void urbdrc_register_udevman_addin(IWTSPlugin* pPlugin, IUDEVMAN* udevman)
{
	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*) pPlugin;

	if (urbdrc->udevman)
	{
		DEBUG_WARN("existing device, abort.");
		return;
	}

	DEBUG_DVC("device registered.");

	urbdrc->udevman = udevman;
}

static int urbdrc_load_udevman_addin(IWTSPlugin* pPlugin, const char* name, ADDIN_ARGV* args)
{
	PFREERDP_URBDRC_DEVICE_ENTRY entry;
	FREERDP_URBDRC_SERVICE_ENTRY_POINTS entryPoints;

	entry = (PFREERDP_URBDRC_DEVICE_ENTRY) freerdp_load_channel_addin_entry("urbdrc", (LPSTR) name, NULL, 0);

	if (entry == NULL)
		return FALSE;

	entryPoints.plugin = pPlugin;
	entryPoints.pRegisterUDEVMAN = urbdrc_register_udevman_addin;
	entryPoints.args = args;

	if (entry(&entryPoints) != 0)
	{
		DEBUG_WARN("%s entry returns error.", name);
		return FALSE;
	}

	return TRUE;
}

void urbdrc_set_subsystem(URBDRC_PLUGIN* urbdrc, char* subsystem)
{
	if (urbdrc->subsystem)
		free(urbdrc->subsystem);

	urbdrc->subsystem = _strdup(subsystem);
}

COMMAND_LINE_ARGUMENT_A urbdrc_args[] =
{
	{ "dbg", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "debug" },
	{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void urbdrc_process_addin_args(URBDRC_PLUGIN* urbdrc, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			urbdrc_args, flags, urbdrc, NULL, NULL);

	arg = urbdrc_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dbg")
		{
			urbdrc_debug = 0;
		}
		CommandLineSwitchCase(arg, "sys")
		{
			urbdrc_set_subsystem(urbdrc, arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry urbdrc_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int status = 0;
	ADDIN_ARGV* args;
	URBDRC_PLUGIN* urbdrc;

	urbdrc = (URBDRC_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "urbdrc");
	args = pEntryPoints->GetPluginData(pEntryPoints);

	if (urbdrc == NULL)
	{
		urbdrc = (URBDRC_PLUGIN*) malloc(sizeof(URBDRC_PLUGIN));
		ZeroMemory(urbdrc, sizeof(URBDRC_PLUGIN));

		urbdrc->iface.Initialize = urbdrc_plugin_initialize;
		urbdrc->iface.Connected = NULL;
		urbdrc->iface.Disconnected = NULL;
		urbdrc->iface.Terminated = urbdrc_plugin_terminated;
		urbdrc->searchman = NULL;
		urbdrc->vchannel_status = INIT_CHANNEL_IN;

		urbdrc_debug = 10;

		status = pEntryPoints->RegisterPlugin(pEntryPoints, "urbdrc", (IWTSPlugin*) urbdrc);
	}

	if (status == 0)
		urbdrc_process_addin_args(urbdrc, args);

	if (!urbdrc->subsystem)
		urbdrc_set_subsystem(urbdrc, "libusb");

	urbdrc_load_udevman_addin((IWTSPlugin*) urbdrc, urbdrc->subsystem, args);

	return status;
}
