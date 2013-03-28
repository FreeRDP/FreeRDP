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
#include <libudev.h>

#include "libusb_udevice.h"

#define BASIC_STATE_FUNC_DEFINED(_arg, _type) \
static _type udev_get_##_arg (IUDEVICE* idev) \
{ \
	UDEVICE* pdev = (UDEVICE*) idev; \
	return pdev->_arg; \
} \
static void udev_set_##_arg (IUDEVICE* idev, _type _t) \
{ \
	UDEVICE* pdev = (UDEVICE*) idev; \
	pdev->_arg = _t; \
}

#define BASIC_POINT_FUNC_DEFINED(_arg, _type) \
static _type udev_get_p_##_arg (IUDEVICE* idev) \
{ \
	UDEVICE* pdev = (UDEVICE*) idev; \
	return pdev->_arg; \
} \
static void udev_set_p_##_arg (IUDEVICE* idev, _type _t) \
{ \
	UDEVICE* pdev = (UDEVICE*) idev; \
	pdev->_arg = _t; \
}

#define BASIC_STATE_FUNC_REGISTER(_arg, _dev) \
	_dev->iface.get_##_arg = udev_get_##_arg; \
	_dev->iface.set_##_arg = udev_set_##_arg


typedef struct _ISO_USER_DATA ISO_USER_DATA;

struct _ISO_USER_DATA
{
	BYTE* IsoPacket;
	BYTE* output_data;
	int iso_status;
	int completed;
	UINT32 error_count;
	int noack;
	UINT32 start_frame;
};

static int get_next_timeout(libusb_context *ctx, struct timeval *tv, struct timeval *out)
{
	int r;
	struct timeval timeout;

	r = libusb_get_next_timeout(ctx, &timeout);

	if (r)
	{
		/* timeout already expired? */
		if (!timerisset(&timeout))
			return 1;

		/* choose the smallest of next URB timeout or user specified timeout */
		if (timercmp(&timeout, tv, <))
			*out = timeout;
		else
			*out = *tv;
	}
	else
	{
		*out = *tv;
	}
	return 0;
}

/* 
 * a simple wrapper to implement libusb_handle_events_timeout_completed
 * function in libusb library git tree (1.0.9 later) */
static int handle_events_completed(libusb_context *ctx, int *completed)
{
	struct timeval tv;

	tv.tv_sec = 60;
	tv.tv_usec = 0;

#ifdef HAVE_NEW_LIBUSB
	return libusb_handle_events_timeout_completed(ctx, &tv, completed);
#else
	int r;
	struct timeval poll_timeout;

	r = get_next_timeout(ctx, &tv, &poll_timeout);

retry:
	if (libusb_try_lock_events(ctx) == 0)
	{
		if (completed == NULL || !*completed)
		{
			/* we obtained the event lock: do our own event handling */
			LLOGLN(10, ("doing our own event handling"));
			r = libusb_handle_events_locked(ctx, &tv);
		}

		libusb_unlock_events(ctx);

		return r;
	}

	/* another thread is doing event handling. wait for thread events that
	 * notify event completion. */
	libusb_lock_event_waiters(ctx);

	if (completed && *completed)
		goto already_done;

	if (!libusb_event_handler_active(ctx))
	{
		/* we hit a race: whoever was event handling earlier finished in the
		 * time it took us to reach this point. try the cycle again. */
		libusb_unlock_event_waiters(ctx);
		LLOGLN(10, ("event handler was active but went away, retrying"));
		goto retry;
	}

	LLOGLN(10, ("another thread is doing event handling"));
	r = libusb_wait_for_event(ctx, &poll_timeout);

already_done:
	libusb_unlock_event_waiters(ctx);

	if (r < 0)
	{
		return r;
	}
	else if (r == 1)
	{
		return libusb_handle_events_timeout(ctx, &tv);
	}
	else
	{
		return 0;
	}
#endif /* HAVE_NEW_LIBUSE */
}

static void func_iso_callback(struct libusb_transfer *transfer)
{
	ISO_USER_DATA* iso_user_data = (ISO_USER_DATA*) transfer->user_data;
	BYTE* data = iso_user_data->IsoPacket;
	int* completed = &iso_user_data->completed;
	UINT32 offset = 0;
	UINT32 index = 0;
	UINT32 i, act_len;
	BYTE* b;

	*completed = 1;
	/* Fixme: currently fill the dummy frame number, tt needs to be 
	 * filled a real frame number */
	// urbdrc_get_mstime(iso_user_data->start_frame);
	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) && (!iso_user_data->noack))
	{
		for (i = 0; i < transfer->num_iso_packets; i++) 
		{
			act_len = transfer->iso_packet_desc[i].actual_length;
			data_write_UINT32(data + offset, index);
			data_write_UINT32(data + offset + 4, act_len);
			data_write_UINT32(data + offset + 8, 
				transfer->iso_packet_desc[i].status);
			offset += 12;

			if (transfer->iso_packet_desc[i].status == USBD_STATUS_SUCCESS) 
			{
				b = libusb_get_iso_packet_buffer_simple(transfer, i);

				if (act_len > 0) 
				{
					if (iso_user_data->output_data + index != b)
						memcpy(iso_user_data->output_data + index, b, act_len);
					index += act_len;
				}
				else
				{
					//fprintf(stderr, "actual length %d \n", act_len);
					//exit(EXIT_FAILURE);
				}
			} 
			else 
			{
				iso_user_data->error_count++;
				//print_transfer_status(transfer->iso_packet_desc[i].status);
			}
		}
		transfer->actual_length = index;
		iso_user_data->iso_status = 1;
	}
	else if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) && (iso_user_data->noack))
	{
		/* This situation occurs when we do not need to 
		 * return any packet */
		iso_user_data->iso_status = 1;
	}
	else
	{
		//print_status(transfer->status);
		iso_user_data->iso_status = -1;
	}
}

static const LIBUSB_ENDPOINT_DESCEIPTOR* func_get_ep_desc(LIBUSB_CONFIG_DESCRIPTOR* LibusbConfig,
	MSUSB_CONFIG_DESCRIPTOR* MsConfig, UINT32 EndpointAddress)
{
	BYTE alt;
	int inum, pnum;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	const LIBUSB_INTERFACE*	interface;
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

static void func_bulk_transfer_cb(struct libusb_transfer *transfer)
{
	int *completed = transfer->user_data;
	*completed = 1;
	/* caller interprets results and frees transfer */
}

static int func_set_usbd_status(UDEVICE* pdev, UINT32* status, int err_result)
{
	switch (err_result)
	{
		case LIBUSB_SUCCESS:
			*status = USBD_STATUS_SUCCESS;
			break;

		case LIBUSB_ERROR_IO:
			*status = USBD_STATUS_STALL_PID;
			LLOGLN(10, ("urb_status: LIBUSB_ERROR_IO!!\n"));
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
				{
					pdev->status |= URBDRC_DEVICE_NOT_FOUND;
					LLOGLN(libusb_debug, ("urb_status: LIBUSB_ERROR_NO_DEVICE!!\n"));
				}
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

	return 0;
}

static void func_iso_data_init(ISO_USER_DATA* iso_user_data, UINT32 numPacket, UINT32 buffsize,
	UINT32 noAck, BYTE* isoPacket, BYTE * buffer)
{
	/* init struct iso_user_data */
	iso_user_data->IsoPacket = isoPacket;
	iso_user_data->output_data = buffer;
	iso_user_data->error_count = 0;
	iso_user_data->completed = 0;
	iso_user_data->noack = noAck;
	urbdrc_get_mstime(iso_user_data->start_frame);
}

static int func_config_release_all_interface(LIBUSB_DEVICE_HANDLE* libusb_handle, UINT32 NumInterfaces)
{
	int i, ret;

	for (i = 0; i < NumInterfaces; i++)
	{
		ret = libusb_release_interface (libusb_handle, i);

		if (ret < 0)
		{
			fprintf(stderr, "config_release_all_interface: error num %d\n", ret);
			return -1;
		}
	}

	return 0;
}

static int func_claim_all_interface(LIBUSB_DEVICE_HANDLE* libusb_handle, int NumInterfaces)
{
	int i, ret;

	for (i = 0; i < NumInterfaces; i++)
	{
		ret = libusb_claim_interface(libusb_handle, i);

		if (ret < 0)
		{
			fprintf(stderr, "claim_all_interface: error num %d\n", ret);
			return -1;
		}
	}

	return 0;
}

/*
static void* print_transfer_status(enum libusb_transfer_status status)
{
	switch (status)
	{
		case LIBUSB_TRANSFER_COMPLETED:
			//fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_COMPLETED\n");
			break;
		case LIBUSB_TRANSFER_ERROR:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_ERROR\n");
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_TIMED_OUT\n");
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_CANCELLED\n");
			break;
		case LIBUSB_TRANSFER_STALL:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_STALL\n");
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_NO_DEVICE\n");
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			fprintf(stderr, "Transfer Status: LIBUSB_TRANSFER_OVERFLOW\n");
			break;
		default:
			fprintf(stderr, "Transfer Status: Get unknow error num %d (0x%x)\n",
					status, status);
	}
	return 0;
}

static void print_status(enum libusb_transfer_status status)
{
	switch (status)
	{
		case LIBUSB_TRANSFER_COMPLETED:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_COMPLETED\n");
			break;
		case LIBUSB_TRANSFER_ERROR:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_ERROR\n");
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_TIMED_OUT\n");
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_CANCELLED\n");
			break;
		case LIBUSB_TRANSFER_STALL:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_STALL\n");
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_NO_DEVICE\n");
			break;
		case LIBUSB_TRANSFER_OVERFLOW: 
			fprintf(stderr, "Transfer status: LIBUSB_TRANSFER_OVERFLOW\n");
			break;
		default:
			fprintf(stderr, "Transfer status: unknow status %d(0x%x)\n", status, status);
			break;
	}
}
*/

static LIBUSB_DEVICE* udev_get_libusb_dev(int bus_number, int dev_number)
{
	int i;
	ssize_t total_device;
	LIBUSB_DEVICE** libusb_list;

	total_device = libusb_get_device_list(NULL, &libusb_list); 

	for (i = 0; i < total_device; i++)
	{
		if ((bus_number == libusb_get_bus_number(libusb_list[i])) &&
				(dev_number == libusb_get_device_address(libusb_list[i])))
			return libusb_list[i];
	}

	libusb_free_device_list(libusb_list, 1);

	return NULL;
}

static LIBUSB_DEVICE_DESCRIPTOR* udev_new_descript(LIBUSB_DEVICE* libusb_dev)
{
	int ret;
	LIBUSB_DEVICE_DESCRIPTOR* descriptor;

	descriptor = (LIBUSB_DEVICE_DESCRIPTOR*) malloc(sizeof(LIBUSB_DEVICE_DESCRIPTOR));

	ret = libusb_get_device_descriptor(libusb_dev, descriptor);

	if (ret < 0)
	{
		fprintf(stderr, "libusb_get_device_descriptor: ERROR!!\n");
		return NULL;
	}
		
	return descriptor;
}

 /* Get HUB handle */
static int udev_get_hub_handle(UDEVICE* pdev, UINT16 bus_number, UINT16 dev_number)
{
	struct udev* udev;
	struct udev_enumerate* enumerate;
	struct udev_list_entry* devices;
	struct udev_list_entry* dev_list_entry;
	struct udev_device* dev;
	LIBUSB_DEVICE* libusb_dev;  
	int hub_found = 0;
	int hub_bus = 0;
	int hub_dev = 0;  
	int error = 0;

	udev = udev_new();

	if (!udev)
	{
		LLOGLN(0, ("%s: Can't create udev", __func__));
		return -1;
	}

	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "usb_device");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) 
	{
		const char * path;
		
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		if (!dev)
			continue;

		int tmp_b = atoi(udev_device_get_property_value(dev,"BUSNUM"));
		int tmp_d = atoi(udev_device_get_property_value(dev,"DEVNUM"));

		if (bus_number == tmp_b && dev_number == tmp_d)
		{
			/* get port number */
			char *p1, *p2;
			const char* sysfs_path =
				udev_device_get_property_value(dev,"DEVPATH");
				
			p1 = (char*) sysfs_path;

			do
			{
				p2 = p1 + 1;
				p1 = strchr(p2, '.');
			}
			while (p1 != NULL);
			
			if ((p2 - sysfs_path) < (strlen(sysfs_path) - 2))
			{
				p1 = (char*) sysfs_path;

				do
				{

					p2 = p1 + 1;
					p1 = strchr(p2, '-');
				}
				while (p1 != NULL);
			}

			pdev->port_number = atoi(p2);
			LLOGLN(libusb_debug, ("  Port: %d", pdev->port_number));

			/* get device path	*/
			p1 = (char*) sysfs_path;

			do
			{
				p2 = p1 + 1;
				p1 = strchr(p2, '/');
			}
			while (p1 != NULL);

			memset(pdev->path, 0, 17);
			strcpy(pdev->path, p2);
			LLOGLN(libusb_debug, ("  DevPath: %s", pdev->path));
			
			/* query parent hub info */
			dev = udev_device_get_parent(dev);

			if (dev != NULL)
			{
				hub_found = 1;
				hub_bus = atoi(udev_device_get_property_value(dev,"BUSNUM"));
				hub_dev = atoi(udev_device_get_property_value(dev,"DEVNUM"));
				LLOGLN(libusb_debug, ("  Hub BUS/DEV: %d %d", hub_bus, hub_dev));
			}

			udev_device_unref(dev);
			break;
		}
		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	if (!hub_found)
	{
		LLOGLN(0, ("%s: hub was not found!", __func__));
		return -1;
	}

	/* Get libusb hub handle */
	libusb_dev = udev_get_libusb_dev(hub_bus, hub_dev);

	if (libusb_dev == NULL)
	{
		LLOGLN(0, ("%s: get hub libusb_dev fail!", __func__));
		return -1;
	}

	error = libusb_open (libusb_dev, &pdev->hub_handle);

	if (error < 0)
	{
		LLOGLN(0, ("%s: libusb_open error!", __func__));
		return -1;
	}

	LLOGLN(libusb_debug, ("%s: libusb_open success!", __func__));

	/* Success! */
	return 0;
}

static int libusb_udev_select_interface(IUDEVICE* idev, BYTE InterfaceNumber, BYTE AlternateSetting)
{
	int error = 0, diff = 1;
	UDEVICE* pdev = (UDEVICE*) idev;
	MSUSB_CONFIG_DESCRIPTOR* MsConfig;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;

	MsConfig = pdev->MsConfig;

	if (MsConfig)
	{
		MsInterfaces = MsConfig->MsInterfaces;

		if ((MsInterfaces) && (MsInterfaces[InterfaceNumber]->AlternateSetting == AlternateSetting))
		{
			diff = 0;
		}
	}
					   
	if (diff)
	{
		error = libusb_set_interface_alt_setting(pdev->libusb_handle,
			InterfaceNumber, AlternateSetting);

		if (error < 0)
		{
			fprintf(stderr, "%s: Set interface altsetting get error num %d\n",
				__func__, error);
		} 
	}

	return error;
}

static MSUSB_CONFIG_DESCRIPTOR* libusb_udev_complete_msconfig_setup(IUDEVICE* idev, MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	UDEVICE* pdev = (UDEVICE*) idev;
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
	int inum = 0, pnum = 0, MsOutSize = 0;

	LibusbConfig = pdev->LibusbConfig;
	if (LibusbConfig->bNumInterfaces != MsConfig->NumInterfaces)
	{
		fprintf(stderr, "Select Configuration: Libusb NumberInterfaces(%d) is different "
			"with MsConfig NumberInterfaces(%d)\n", 
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
		t_MsPipes = (MSUSB_PIPE_DESCRIPTOR**) malloc(LibusbNumEndpoint * sizeof(MSUSB_PIPE_DESCRIPTOR*));

		for (pnum = 0; pnum < LibusbNumEndpoint; pnum++)
		{
			t_MsPipe = (MSUSB_PIPE_DESCRIPTOR *) malloc (sizeof(MSUSB_PIPE_DESCRIPTOR));  
			memset(t_MsPipe, 0, sizeof(MSUSB_PIPE_DESCRIPTOR));

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
	MsConfig->ConfigurationHandle = MsConfig->bConfigurationValue | 
										(pdev->bus_number << 24) | 
										(pdev->dev_number << 16);

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
		MsInterface->InterfaceHandle = LibusbAltsetting->bInterfaceNumber 
			| (LibusbAltsetting->bAlternateSetting << 8)
			| (pdev->dev_number << 16) 
			| (pdev->bus_number << 24);

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
			MsPipe->PipeHandle = LibusbEndpoint->bEndpointAddress 
				| (pdev->dev_number << 16) 
				| (pdev->bus_number << 24);
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
	if (!(MsConfig == pdev->MsConfig))
	{
		msusb_msconfig_free(pdev->MsConfig);
		pdev->MsConfig = MsConfig;
	}

	return MsConfig;
}

static int libusb_udev_select_configuration(IUDEVICE* idev, UINT32 bConfigurationValue)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	MSUSB_CONFIG_DESCRIPTOR * MsConfig = pdev->MsConfig;
	LIBUSB_DEVICE_HANDLE * libusb_handle = pdev->libusb_handle;
	LIBUSB_DEVICE * libusb_dev = pdev->libusb_dev;
	LIBUSB_CONFIG_DESCRIPTOR ** LibusbConfig = &pdev->LibusbConfig;
	int ret = 0;

	if (MsConfig->InitCompleted){
		func_config_release_all_interface(libusb_handle, (*LibusbConfig)->bNumInterfaces);
	}
	/* The configuration value -1 is mean to put the device in unconfigured state. */
	if (bConfigurationValue == 0)
		ret = libusb_set_configuration(libusb_handle, -1); 
	else
		ret = libusb_set_configuration(libusb_handle, bConfigurationValue); 

	if (ret < 0){
		fprintf(stderr, "libusb_set_configuration: ERROR number %d!!\n", ret);
		func_claim_all_interface(libusb_handle, (*LibusbConfig)->bNumInterfaces);
		return -1;
	}
	else
	{
		ret = libusb_get_active_config_descriptor (libusb_dev, LibusbConfig);
		if (ret < 0){
			fprintf(stderr, "libusb_get_config_descriptor_by_value: ERROR number %d!!\n", ret);
			func_claim_all_interface(libusb_handle, (*LibusbConfig)->bNumInterfaces);
			return -1;
		}
	}

	func_claim_all_interface(libusb_handle, (*LibusbConfig)->bNumInterfaces);

	return 0;
}

static int libusb_udev_control_pipe_request(IUDEVICE* idev, UINT32 RequestId,
	UINT32 EndpointAddress, UINT32* UsbdStatus, int command)
{
	int error = 0;
	UDEVICE* pdev = (UDEVICE*) idev;
	/*
	pdev->request_queue->register_request(pdev->request_queue, RequestId, NULL, 0);
	*/
	switch (command){
		case PIPE_CANCEL:
			/** cancel bulk or int transfer */
			idev->cancel_all_transfer_request(idev);

			//dummy_wait_s_obj(1);
			/** set feature to ep (set halt)*/
			error = libusb_control_transfer(pdev->libusb_handle, 
				LIBUSB_ENDPOINT_OUT | LIBUSB_RECIPIENT_ENDPOINT,
				LIBUSB_REQUEST_SET_FEATURE, 
				ENDPOINT_HALT, 
				EndpointAddress, 
				NULL,
				0, 
				1000);
			break;
		case PIPE_RESET:
			idev->cancel_all_transfer_request(idev);
			
			error = libusb_clear_halt(pdev->libusb_handle, EndpointAddress);
			
			//func_set_usbd_status(pdev, UsbdStatus, error);
			break;
		default:
			error = -0xff;
			break;
	}

	*UsbdStatus = 0;
	/*
	if(pdev->request_queue->unregister_request(pdev->request_queue, RequestId))
		fprintf(stderr, "request_queue_unregister_request: not fount request 0x%x\n", RequestId);
	*/
	return error;
}

static int libusb_udev_control_query_device_text(IUDEVICE* idev, UINT32 TextType,
	UINT32 LocaleId, 
	UINT32 * BufferSize, 
	BYTE * Buffer)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	LIBUSB_DEVICE_DESCRIPTOR * devDescriptor = pdev->devDescriptor;
	char * strDesc = "Generic Usb String";
	char deviceLocation[25];
	BYTE bus_number;
	BYTE device_address;
	int ret = 0, i = 0;

	switch (TextType){
		case DeviceTextDescription:
			ret = libusb_get_string_descriptor (pdev->libusb_handle, 
				devDescriptor->iProduct, 
				LocaleId, 
				Buffer, 
				*BufferSize);

			for(i = 0; i < ret; i++)
			{
				Buffer[i] = Buffer[i+2];
			}
			ret -= 2;
			
			if (ret <= 0 || ret < 4){
				LLOGLN(libusb_debug, ("libusb_get_string_descriptor: "
					"ERROR num %d, iProduct: %d!", ret, devDescriptor->iProduct));
				memcpy(Buffer, strDesc, strlen(strDesc));
				Buffer[strlen(strDesc)] = '\0';
				*BufferSize = (strlen((char *)Buffer)) * 2;
				for (i = strlen((char *)Buffer); i > 0; i--)
				{
					Buffer[i*2] = Buffer[i];
					Buffer[(i*2)-1] = 0;
				}
			}
			else
			{
				*BufferSize = ret;
			}

			break;
		case DeviceTextLocationInformation:
			bus_number = libusb_get_bus_number(pdev->libusb_dev);
			device_address = libusb_get_device_address(pdev->libusb_dev);
			sprintf(deviceLocation, "Port_#%04d.Hub_#%04d", device_address, bus_number);

			for(i=0;i<strlen(deviceLocation);i++){
				Buffer[i*2] = (BYTE)deviceLocation[i];
				Buffer[(i*2)+1] = 0;
			}
			*BufferSize = (i*2);
			break;
		default:
			LLOGLN(0, ("Query Text: unknown TextType %d", TextType));
		break;
	}

	return 0;
}


static int libusb_udev_os_feature_descriptor_request(IUDEVICE* idev, UINT32 RequestId,
	BYTE Recipient, 
	BYTE InterfaceNumber, 
	BYTE Ms_PageIndex, 
	UINT16 Ms_featureDescIndex, 
	UINT32 * UsbdStatus,
	UINT32 * BufferSize, 
	BYTE* Buffer, 
	int Timeout)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	BYTE ms_string_desc[0x13];
	int error = 0;
	/*
	pdev->request_queue->register_request(pdev->request_queue, RequestId, NULL, 0);
	*/
	memset(ms_string_desc, 0, 0x13);
	error = libusb_control_transfer(pdev->libusb_handle, 
		LIBUSB_ENDPOINT_IN | Recipient, 
		LIBUSB_REQUEST_GET_DESCRIPTOR, 
		0x03ee, 
		0, 
		ms_string_desc, 
		0x12, 
		Timeout);
	//fprintf(stderr, "Get ms string: result number %d", error);
	if (error > 0)
	{
		BYTE bMS_Vendorcode;
		data_read_BYTE(ms_string_desc + 16, bMS_Vendorcode);
		//fprintf(stderr, "bMS_Vendorcode:0x%x", bMS_Vendorcode);
		/** get os descriptor */
		error = libusb_control_transfer(pdev->libusb_handle, 
			LIBUSB_ENDPOINT_IN |LIBUSB_REQUEST_TYPE_VENDOR | Recipient, 
			bMS_Vendorcode, 
			(InterfaceNumber << 8) | Ms_PageIndex, 
			Ms_featureDescIndex, 
			Buffer, 
			*BufferSize, 
			Timeout);
		*BufferSize = error;
	}

	if (error < 0)
		*UsbdStatus = USBD_STATUS_STALL_PID;
	else
		*UsbdStatus = USBD_STATUS_SUCCESS;
	/*
	if(pdev->request_queue->unregister_request(pdev->request_queue, RequestId))
		fprintf(stderr, "request_queue_unregister_request: not fount request 0x%x\n", RequestId);
	*/
	return error;
}

static int libusb_udev_query_device_descriptor(IUDEVICE* idev, int offset)
{
	UDEVICE* pdev = (UDEVICE*) idev;

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
	return 0;
}

static void libusb_udev_detach_kernel_driver(IUDEVICE* idev)
{
	int i, err = 0;
	UDEVICE* pdev = (UDEVICE*) idev;

	if ((pdev->status & URBDRC_DEVICE_DETACH_KERNEL) == 0)
	{		
		for (i = 0; i < pdev->LibusbConfig->bNumInterfaces; i++)
		{
			err = libusb_kernel_driver_active(pdev->libusb_handle , i);
			LLOGLN(libusb_debug, ("libusb_kernel_driver_active = %d\n", err));  
			if (err){
				err = libusb_detach_kernel_driver(pdev->libusb_handle , i);
				LLOGLN(libusb_debug, ("libusb_detach_kernel_driver = %d\n", err)); 
			}
		}
		
		pdev->status |= URBDRC_DEVICE_DETACH_KERNEL;
	}
}

static void libusb_udev_attach_kernel_driver(IUDEVICE* idev)
{
	int i, err = 0;
	UDEVICE* pdev = (UDEVICE*) idev;

	for (i = 0; i < pdev->LibusbConfig->bNumInterfaces && err != LIBUSB_ERROR_NO_DEVICE; i++)
	{
		err = libusb_release_interface (pdev->libusb_handle, i);
		if (err < 0){
			LLOGLN(libusb_debug, ("libusb_release_interface: error num %d = %d", i, err));
		}
		if (err != LIBUSB_ERROR_NO_DEVICE)
		{
			err = libusb_attach_kernel_driver (pdev->libusb_handle , i);
			LLOGLN(libusb_debug, ("libusb_attach_kernel_driver if%d = %d", i, err)); 
		}  
	}
}

static int libusb_udev_is_composite_device(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return pdev->isCompositeDevice;
}

static int libusb_udev_is_signal_end(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return (pdev->status & URBDRC_DEVICE_SIGNAL_END) ? 1 : 0;
}

static int libusb_udev_is_exist(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return (pdev->status & URBDRC_DEVICE_NOT_FOUND) ? 0 : 1;
}

static int libusb_udev_is_channel_closed(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return (pdev->status & URBDRC_DEVICE_CHANNEL_CLOSED) ? 1 : 0;
}

static int libusb_udev_is_already_send(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return (pdev->status & URBDRC_DEVICE_ALREADY_SEND) ? 1 : 0;
}

static void libusb_udev_signal_end(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	pdev->status |= URBDRC_DEVICE_SIGNAL_END;
}

static void libusb_udev_channel_closed(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	pdev->status |= URBDRC_DEVICE_CHANNEL_CLOSED;
}

static void libusb_udev_set_already_send(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	pdev->status |= URBDRC_DEVICE_ALREADY_SEND;
}

static char* libusb_udev_get_path(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	return pdev->path;
}

static int libusb_udev_wait_action_completion(IUDEVICE* idev)
{
	int error, sval;
	UDEVICE* pdev = (UDEVICE*) idev;

	while(1)
	{
		usleep(500000);

		error = sem_getvalue(&pdev->sem_id, &sval);

		if (sval == 0)
			break;
	}

	return error;
}

static void libusb_udev_push_action(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	sem_post(&pdev->sem_id);
}

static void libusb_udev_complete_action(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	sem_trywait(&pdev->sem_id); 
}

static int libusb_udev_wait_for_detach(IUDEVICE* idev)
{
	int error = 0;
	int times = 0;
	UDEVICE* pdev = (UDEVICE*) idev;

	while (times < 25)
	{
		if (pdev->status & URBDRC_DEVICE_SIGNAL_END)
		{
			error = -1;
			break;
		}

		usleep(200000);
		times++;
	}

	return error;
}

static void libusb_udev_lock_fifo_isoch(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	pthread_mutex_lock(&pdev->mutex_isoch);  
}

static void  libusb_udev_unlock_fifo_isoch(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	pthread_mutex_unlock(&pdev->mutex_isoch);
}

static int libusb_udev_query_device_port_status(IUDEVICE* idev, UINT32* UsbdStatus, UINT32* BufferSize, BYTE* Buffer)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	int success = 0, ret;

	if (pdev->hub_handle != NULL)
	{
		ret = idev->control_transfer(idev, 0xffff, 0, 0, 
			LIBUSB_ENDPOINT_IN 
			| LIBUSB_REQUEST_TYPE_CLASS 
			| LIBUSB_RECIPIENT_OTHER, 
			LIBUSB_REQUEST_GET_STATUS, 
			0, pdev->port_number, UsbdStatus, BufferSize, Buffer, 1000);

		if (ret < 0)
		{
			LLOGLN(libusb_debug, ("libusb_control_transfer: error num %d", ret));
			*BufferSize = 0;
		}
		else
		{
			LLOGLN(libusb_debug, ("PORT STATUS:0x%02x%02x%02x%02x", 
				Buffer[3], Buffer[2], Buffer[1], Buffer[0]));
			success = 1;
		}
	}

	return success;
}

static int libusb_udev_request_queue_is_none(IUDEVICE* idev)
{
	UDEVICE* pdev = (UDEVICE*) idev;

	if (pdev->request_queue->request_num == 0)
		return 1;

	return 0;
}

static int libusb_udev_isoch_transfer(IUDEVICE* idev, UINT32 RequestId, UINT32 EndpointAddress,
	UINT32 TransferFlags, int NoAck, UINT32* ErrorCount,
	UINT32* UrbdStatus, UINT32* StartFrame, UINT32 NumberOfPackets,
	BYTE* IsoPacket, UINT32* BufferSize, BYTE* Buffer, int Timeout)
{
	UINT32 iso_packet_size;
	UDEVICE* pdev = (UDEVICE*) idev;
	ISO_USER_DATA iso_user_data;
	struct libusb_transfer* iso_transfer = NULL;
	int status = 0, ret = 0, submit = 0;

	iso_packet_size = *BufferSize / NumberOfPackets;

	iso_transfer = libusb_alloc_transfer(NumberOfPackets);

	if (iso_transfer == NULL)
	{
		fprintf(stderr, "Error: libusb_alloc_transfer.\n");
		status = -1;
	}

	/**  process URB_FUNCTION_IOSCH_TRANSFER */
	func_iso_data_init(&iso_user_data, NumberOfPackets, *BufferSize, NoAck, IsoPacket, Buffer);

	/** fill setting */
	libusb_fill_iso_transfer(iso_transfer, 
		pdev->libusb_handle, EndpointAddress, Buffer, *BufferSize,
		NumberOfPackets, func_iso_callback, &iso_user_data, 2000);

	libusb_set_iso_packet_lengths(iso_transfer, iso_packet_size);

	if (pdev->status & (URBDRC_DEVICE_SIGNAL_END | URBDRC_DEVICE_NOT_FOUND))
		status = -1;

	iso_user_data.iso_status = 0;

	if (!(status < 0))
	{
		submit = libusb_submit_transfer(iso_transfer);

		if (submit < 0)
		{
			LLOGLN(libusb_debug, ("Error: Failed to submit transfer (ret = %d).", submit));
			status = -1;
			func_set_usbd_status(pdev, UrbdStatus, ret);  
		}
	}

#if ISOCH_FIFO
	if (!NoAck)
	{
		idev->unlock_fifo_isoch(idev);
	}
#endif

	while(pdev && iso_user_data.iso_status == 0 && status >= 0 && submit >= 0)
	{
		if (pdev->status & URBDRC_DEVICE_NOT_FOUND){
			status = -1;
			break;
		}
		ret = handle_events_completed(NULL, &iso_user_data.completed);
		if (ret < 0) {
			LLOGLN(libusb_debug, ("Error: libusb_handle_events (ret = %d).", ret));
			status = -1;
			break;
		}
#if WAIT_COMPLETE_SLEEP
		if (iso_user_data.iso_status == 0)
		{
			usleep(WAIT_COMPLETE_SLEEP);
		}
#endif
	} 

	if (iso_user_data.iso_status < 0)
		status = -1;

	*ErrorCount = iso_user_data.error_count;
	*StartFrame = iso_user_data.start_frame;
	*BufferSize = iso_transfer->actual_length;
	libusb_free_transfer(iso_transfer);

	return status;
}

static int libusb_udev_control_transfer(IUDEVICE* idev, UINT32 RequestId, UINT32 EndpointAddress,
	UINT32 TransferFlags, BYTE bmRequestType, BYTE Request, UINT16 Value, UINT16 Index,
	UINT32* UrbdStatus, UINT32* BufferSize, BYTE* Buffer, UINT32 Timeout)
{
	int status = 0;
	UDEVICE* pdev = (UDEVICE*) idev;

	status = libusb_control_transfer(pdev->libusb_handle, 
		bmRequestType, Request, Value, Index, Buffer, *BufferSize, Timeout);

	if (!(status < 0))
		*BufferSize = status;

	func_set_usbd_status(pdev, UrbdStatus, status);   

	return status;
}

static int libusb_udev_bulk_or_interrupt_transfer(IUDEVICE* idev, UINT32 RequestId,
	UINT32 EndpointAddress, UINT32 TransferFlags, UINT32* UsbdStatus, UINT32* BufferSize, BYTE* Buffer, UINT32 Timeout)
{
	UINT32 transfer_type;
	UDEVICE* pdev = (UDEVICE*) idev;
	const LIBUSB_ENDPOINT_DESCEIPTOR* ep_desc;
	struct libusb_transfer* transfer = NULL;
	TRANSFER_REQUEST* request = NULL;
	int completed = 0, status = 0, submit = 0;
	int transferDir = EndpointAddress & 0x80;

	/* alloc memory for urb transfer */
	transfer = libusb_alloc_transfer(0);   
		
	ep_desc = func_get_ep_desc(pdev->LibusbConfig, pdev->MsConfig, EndpointAddress);

	if (!ep_desc)
	{
		fprintf(stderr, "func_get_ep_desc: endpoint 0x%x is not found!!\n", EndpointAddress);
		return -1;
	}
	transfer_type = (ep_desc->bmAttributes) & 0x3;

	LLOGLN(libusb_debug, ("urb_bulk_or_interrupt_transfer: ep:0x%x "
		"transfer_type %d flag:%d OutputBufferSize:0x%x", 
		EndpointAddress, transfer_type, TransferFlags, *BufferSize));

	switch (transfer_type)
	{
		case BULK_TRANSFER:
			/** Bulk Transfer */
			//Timeout = 10000;
			break;

		case INTERRUPT_TRANSFER:
			/**  Interrupt Transfer */
			/** Sometime, we may have receive a oversized transfer request, 
			 * it make submit urb return error, so we set the length of 
			 * request to wMaxPacketSize */ 
			if (*BufferSize != (ep_desc->wMaxPacketSize))
			{
				LLOGLN(libusb_debug, ("Interrupt Transfer(%s): "
					"BufferSize is different than maxPacketsize(0x%x)", 
					((transferDir)?"IN":"OUT"), ep_desc->wMaxPacketSize));
				if((*BufferSize) > (ep_desc->wMaxPacketSize) &&
						transferDir == USBD_TRANSFER_DIRECTION_IN)
					(*BufferSize) = ep_desc->wMaxPacketSize;
			}
			Timeout = 0;
			break;

		default:
			LLOGLN(0, ("urb_bulk_or_interrupt_transfer:"
							" other transfer type 0x%X", transfer_type));
			return -1;
			break;
	}

	libusb_fill_bulk_transfer(transfer, pdev->libusb_handle, EndpointAddress,
		Buffer, *BufferSize, func_bulk_transfer_cb, &completed, Timeout);

	transfer->type = (unsigned char) transfer_type;

	/** Bug fixed in libusb-1.0-8 later: issue of memory crash */
	submit = libusb_submit_transfer(transfer);

	if (submit < 0)
	{
		LLOGLN(libusb_debug, ("libusb_bulk_transfer: error num %d", status));
		func_set_usbd_status(pdev, UsbdStatus, status);
		*BufferSize = 0;
	}
	else
	{
		request = pdev->request_queue->register_request(
			pdev->request_queue, RequestId, transfer, EndpointAddress);
		request->submit = 1; 
	}

	if ((pdev && *UsbdStatus == 0) && (submit >= 0) &&
			(pdev->iface.isSigToEnd((IUDEVICE*) pdev) == 0))
	{
		while (!completed)
		{
			status = handle_events_completed(NULL, &completed);

			if (status < 0)
			{
				if (status == LIBUSB_ERROR_INTERRUPTED)
					continue;

				libusb_cancel_transfer(transfer);

				while (!completed)
				{
					if (handle_events_completed(NULL, &completed) < 0)
						break;
#if WAIT_COMPLETE_SLEEP
					if (!completed)
						usleep(WAIT_COMPLETE_SLEEP);
#endif
				}
				break;
			}
#if WAIT_COMPLETE_SLEEP
			if (!completed)
				usleep(WAIT_COMPLETE_SLEEP);
#endif
		}

		switch (transfer->status)
		{
			case LIBUSB_TRANSFER_COMPLETED:
				func_set_usbd_status(pdev, UsbdStatus, 0);
				break;

			case LIBUSB_TRANSFER_TIMED_OUT:
				func_set_usbd_status(pdev, UsbdStatus, LIBUSB_ERROR_TIMEOUT);
				break;

			case LIBUSB_TRANSFER_STALL:
				func_set_usbd_status(pdev, UsbdStatus, LIBUSB_ERROR_PIPE);
				break;

			case LIBUSB_TRANSFER_OVERFLOW:
				func_set_usbd_status(pdev, UsbdStatus, LIBUSB_ERROR_OVERFLOW);
				break;

			case LIBUSB_TRANSFER_NO_DEVICE:
				func_set_usbd_status(pdev, UsbdStatus, LIBUSB_ERROR_NO_DEVICE);
				break;

			default:
				func_set_usbd_status(pdev, UsbdStatus, LIBUSB_ERROR_OTHER);
				break;
		}
		
		*BufferSize = transfer->actual_length;
	}
	LLOGLN(libusb_debug, ("bulk or interrupt Transfer data size : 0x%x", *BufferSize));

	if (request)
	{
		if(pdev->request_queue->unregister_request(pdev->request_queue, RequestId))
			fprintf(stderr, "request_queue_unregister_request: not fount request 0x%x\n", RequestId);
	}

	libusb_free_transfer(transfer);

	return 0;
}

static void libusb_udev_cancel_all_transfer_request(IUDEVICE* idev)
{
	int status;
	UDEVICE* pdev = (UDEVICE*) idev;
	REQUEST_QUEUE* request_queue = pdev->request_queue;
	TRANSFER_REQUEST* request = NULL;

	pthread_mutex_lock(&request_queue->request_loading);
			
	request_queue->rewind(request_queue);

	while (request_queue->has_next(request_queue))
	{
		request = request_queue->get_next(request_queue);
		
		if ((!request) || (!request->transfer) ||
			(request->endpoint != request->transfer->endpoint) ||
			(request->transfer->endpoint == 0) || (request->submit != 1))
		{
			continue;
		}

		status = libusb_cancel_transfer(request->transfer);

		if (status < 0)
		{
			LLOGLN(libusb_debug, ("libusb_cancel_transfer: error num %d!!\n", status));
		}
		else
		{
			request->submit = -1;
		}

	}

	pthread_mutex_unlock(&request_queue->request_loading);
}

static int func_cancel_xact_request(TRANSFER_REQUEST *request)
{
	int status;

	if ((!request->transfer) || (request->endpoint != request->transfer->endpoint) ||
		(request->transfer->endpoint == 0) || (request->submit != 1))
	{
		return 0;
	}

	status = libusb_cancel_transfer(request->transfer);

	if (status < 0)
	{
		LLOGLN(0, ("libusb_cancel_transfer: error num %d!!", status));

		if (status == LIBUSB_ERROR_NOT_FOUND)
			return -1;   
	}
	else
	{
		LLOGLN(libusb_debug, ("libusb_cancel_transfer: Success num:0x%x!!", request->RequestId));
		request->submit = -1;
		return 1;
	}

	return 0;
}

static int libusb_udev_cancel_transfer_request(IUDEVICE* idev, UINT32 RequestId)
{
	UDEVICE* pdev = (UDEVICE*) idev;
	REQUEST_QUEUE* request_queue = pdev->request_queue;
	TRANSFER_REQUEST* request = NULL;
	int status = 0, retry_times = 0;

cancel_retry:
	pthread_mutex_lock(&request_queue->request_loading);

	request_queue->rewind(request_queue);

	while (request_queue->has_next(request_queue))
	{
		request = request_queue->get_next(request_queue);

		LLOGLN(libusb_debug, ("%s: CancelId:0x%x RequestId:0x%x endpoint 0x%x!!", 
			__func__, RequestId, request->RequestId, request->endpoint)); 

		if ((request && request->RequestId) == (RequestId && retry_times <= 10))
		{
			status = func_cancel_xact_request(request);
			break;
		}
		else if ((request->transfer) && (retry_times > 10))
		{
			status = -1;
			break;
		}
	}

	pthread_mutex_unlock(&request_queue->request_loading);

	if ((status == 0) && (retry_times < 10))
	{
		retry_times++;
		usleep(100000);
		LLOGLN(10, ("urbdrc_process_cancel_request: go retry!!")); 
		goto cancel_retry;
	}
	else if ((status < 0) || (retry_times >= 10))
	{
		/** END */
		LLOGLN(libusb_debug, ("urbdrc_process_cancel_request: error go exit!!")); 
		return -1;
	}
	LLOGLN(libusb_debug, ("urbdrc_process_cancel_request: success!!")); 

	return 0;
}

BASIC_STATE_FUNC_DEFINED(channel_id, UINT32)
BASIC_STATE_FUNC_DEFINED(UsbDevice, UINT32)
BASIC_STATE_FUNC_DEFINED(ReqCompletion, UINT32)
BASIC_STATE_FUNC_DEFINED(bus_number, UINT16)
BASIC_STATE_FUNC_DEFINED(dev_number, UINT16)
BASIC_STATE_FUNC_DEFINED(port_number, int)
BASIC_STATE_FUNC_DEFINED(isoch_queue, void *)
BASIC_STATE_FUNC_DEFINED(MsConfig, MSUSB_CONFIG_DESCRIPTOR *)

BASIC_POINT_FUNC_DEFINED(udev, void *)
BASIC_POINT_FUNC_DEFINED(prev, void *)
BASIC_POINT_FUNC_DEFINED(next, void *)

static void udev_load_interface(UDEVICE* pdev)
{
	/* load interface */

	/* Basic */
	BASIC_STATE_FUNC_REGISTER(channel_id, pdev);
	BASIC_STATE_FUNC_REGISTER(UsbDevice, pdev);
	BASIC_STATE_FUNC_REGISTER(ReqCompletion, pdev);
	BASIC_STATE_FUNC_REGISTER(bus_number, pdev);
	BASIC_STATE_FUNC_REGISTER(dev_number, pdev);
	BASIC_STATE_FUNC_REGISTER(port_number, pdev);
	BASIC_STATE_FUNC_REGISTER(isoch_queue, pdev);
	BASIC_STATE_FUNC_REGISTER(MsConfig, pdev);

	BASIC_STATE_FUNC_REGISTER(p_udev, pdev);
	BASIC_STATE_FUNC_REGISTER(p_prev, pdev);
	BASIC_STATE_FUNC_REGISTER(p_next, pdev);

	pdev->iface.isCompositeDevice = libusb_udev_is_composite_device;
	pdev->iface.isSigToEnd = libusb_udev_is_signal_end;
	pdev->iface.isExist = libusb_udev_is_exist;
	pdev->iface.isAlreadySend = libusb_udev_is_already_send;
	pdev->iface.isChannelClosed = libusb_udev_is_channel_closed;
	pdev->iface.SigToEnd = libusb_udev_signal_end;
	pdev->iface.setAlreadySend = libusb_udev_set_already_send;
	pdev->iface.setChannelClosed = libusb_udev_channel_closed;
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
	pdev->iface.wait_action_completion = libusb_udev_wait_action_completion;
	pdev->iface.push_action = libusb_udev_push_action;
	pdev->iface.complete_action = libusb_udev_complete_action;
	pdev->iface.lock_fifo_isoch = libusb_udev_lock_fifo_isoch;
	pdev->iface.unlock_fifo_isoch = libusb_udev_unlock_fifo_isoch;
	pdev->iface.query_device_port_status = libusb_udev_query_device_port_status;
	pdev->iface.request_queue_is_none = libusb_udev_request_queue_is_none;
	pdev->iface.wait_for_detach = libusb_udev_wait_for_detach;
}

static IUDEVICE* udev_init(UDEVICE* pdev, UINT16 bus_number, UINT16 dev_number)
{
	int status, num;
	LIBUSB_DEVICE_DESCRIPTOR* devDescriptor;
	LIBUSB_CONFIG_DESCRIPTOR* config_temp;
	LIBUSB_INTERFACE_DESCRIPTOR interface_temp;

	/* Get HUB handle */
	status = udev_get_hub_handle(pdev, bus_number, dev_number);

	if (status < 0)
	{
		fprintf(stderr, "USB init: Error to get HUB handle!!\n");
		pdev->hub_handle = NULL;
	}

	pdev->devDescriptor = udev_new_descript(pdev->libusb_dev);

	if (!pdev->devDescriptor)
	{
		fprintf(stderr, "USB init: Error to get device descriptor!!\n");
		zfree(pdev);
		return NULL;
	}

	num = pdev->devDescriptor->bNumConfigurations;

	status = libusb_get_active_config_descriptor (pdev->libusb_dev, &pdev->LibusbConfig);

	if (status < 0)
	{
		fprintf(stderr, "libusb_get_descriptor: ERROR!!ret:%d\n", status);
		zfree(pdev);
		return NULL;
	}

	config_temp = pdev->LibusbConfig;
	/* get the first interface and first altsetting */
	interface_temp = config_temp->interface[0].altsetting[0];

	LLOGLN(0, ("Regist Device: Vid: 0x%04X Pid: 0x%04X"
		" InterfaceClass = 0x%X", 
		pdev->devDescriptor->idVendor, 
		pdev->devDescriptor->idProduct,
		interface_temp.bInterfaceClass)); 

	/* Denied list */
	switch(interface_temp.bInterfaceClass)
	{
		case CLASS_RESERVE:
		//case CLASS_COMMUNICATION_IF:
		//case CLASS_HID:
		//case CLASS_PHYSICAL:
		case CLASS_MASS_STORAGE:
		case CLASS_HUB:
		//case CLASS_COMMUNICATION_DATA_IF:
		case CLASS_SMART_CARD:
		case CLASS_CONTENT_SECURITY:
		//case CLASS_WIRELESS_CONTROLLER:
		//case CLASS_ELSE_DEVICE:
			fprintf(stderr, "    Device is not supported!!\n");
			zfree(pdev);
			return NULL;
		default:
			break;
	}

	/* Check composite device */
	devDescriptor = pdev->devDescriptor;

	if ((devDescriptor->bNumConfigurations == 1) &&
		(config_temp->bNumInterfaces > 1) &&
		(devDescriptor->bDeviceClass == 0x0))
	{
		pdev->isCompositeDevice = 1;
	}
	else if ((devDescriptor->bDeviceClass == 0xef) &&
			(devDescriptor->bDeviceSubClass == 0x02) &&
			(devDescriptor->bDeviceProtocol == 0x01))
	{
		pdev->isCompositeDevice = 1;
	}
	else
	{
		pdev->isCompositeDevice = 0;
	}

	/* set device class to first interface class */ 
	devDescriptor->bDeviceClass = interface_temp.bInterfaceClass;
	devDescriptor->bDeviceSubClass = interface_temp.bInterfaceSubClass;
	devDescriptor->bDeviceProtocol = interface_temp.bInterfaceProtocol;

	/* initialize pdev */
	pdev->prev = NULL;
	pdev->next = NULL;
	pdev->bus_number = bus_number;
	pdev->dev_number = dev_number;
	pdev->status = 0;
	pdev->ReqCompletion = 0;
	pdev->channel_id = 0xffff;
	pdev->request_queue = request_queue_new();
	pdev->isoch_queue = NULL;
	sem_init(&pdev->sem_id, 0, 0);

	/* set config of windows */
	pdev->MsConfig = msusb_msconfig_new();

	pthread_mutex_init(&pdev->mutex_isoch, NULL);

	//deb_config_msg(pdev->libusb_dev, config_temp, devDescriptor->bNumConfigurations);  

	udev_load_interface(pdev);

	return (IUDEVICE*) pdev;
}

int udev_new_by_id(UINT16 idVendor, UINT16 idProduct, IUDEVICE*** devArray)
{
	LIBUSB_DEVICE_DESCRIPTOR* descriptor;
	LIBUSB_DEVICE** libusb_list;
	UDEVICE** array;
	UINT16 bus_number;
	UINT16 dev_number;
	ssize_t total_device;
	int i, status, num = 0;

	fprintf(stderr, "VID: 0x%04X PID: 0x%04X\n", idVendor, idProduct);

	array = (UDEVICE**) malloc(16 * sizeof(UDEVICE*));

	total_device = libusb_get_device_list(NULL, &libusb_list); 

	for (i = 0; i < total_device; i++)
	{
		descriptor = udev_new_descript(libusb_list[i]);

		if ((descriptor->idVendor == idVendor) && (descriptor->idProduct == idProduct))
		{
			bus_number = 0;
			dev_number = 0;
			array[num] = (PUDEVICE) malloc(sizeof(UDEVICE));
			array[num]->libusb_dev = libusb_list[i];

			status = libusb_open(libusb_list[i], &array[num]->libusb_handle);

			if (status < 0)
			{
				fprintf(stderr, "libusb_open: (by id) error: 0x%08X (%d)\n", status, status);
				zfree(descriptor);
				zfree(array[num]);
				continue;
			}

			bus_number = libusb_get_bus_number(libusb_list[i]);
			dev_number = libusb_get_device_address(libusb_list[i]);

			array[num] = (PUDEVICE) udev_init(array[num], bus_number, dev_number);

			if (array[num] != NULL)
				num++;
		}
		zfree(descriptor);
	}

	libusb_free_device_list(libusb_list, 1);

	*devArray = (IUDEVICE**) array;

	return num;
}

IUDEVICE* udev_new_by_addr(int bus_number, int dev_number)
{
	int status;
	UDEVICE* pDev;

	LLOGLN(10, ("bus:%d dev:%d\n", bus_number, dev_number));

	pDev = (PUDEVICE) malloc(sizeof(UDEVICE));

	pDev->libusb_dev = udev_get_libusb_dev(bus_number, dev_number);

	if (pDev->libusb_dev == NULL)
	{
		fprintf(stderr, "libusb_device_new: ERROR!!\n");
		zfree(pDev);
		return NULL;
	}

	status = libusb_open(pDev->libusb_dev, &pDev->libusb_handle);

	if (status < 0)
	{
		fprintf(stderr, "libusb_open: (by addr) ERROR!!\n");
		zfree(pDev);
		return NULL;
	}

	return udev_init(pDev, bus_number, dev_number);
}
