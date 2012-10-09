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



#ifndef __LIBUSB_UDEVICE_H
#define __LIBUSB_UDEVICE_H

#include <libusb-1.0/libusb.h>

#include "urbdrc_types.h"
#include "request_queue.h"
#include "urbdrc_main.h"



typedef struct libusb_device				LIBUSB_DEVICE;
typedef struct libusb_device_handle			LIBUSB_DEVICE_HANDLE;
typedef struct libusb_device_descriptor		LIBUSB_DEVICE_DESCRIPTOR;
typedef struct libusb_config_descriptor		LIBUSB_CONFIG_DESCRIPTOR;
typedef struct libusb_interface				LIBUSB_INTERFACE;
typedef struct libusb_interface_descriptor	LIBUSB_INTERFACE_DESCRIPTOR;
typedef struct libusb_endpoint_descriptor	LIBUSB_ENDPOINT_DESCEIPTOR;


typedef struct _UDEVICE UDEVICE;
struct _UDEVICE
{
	IUDEVICE iface;

	void * udev;
	void * prev;
	void * next;

	uint32	UsbDevice; /* An unique interface ID */
	uint32	ReqCompletion; /* An unique interface ID */
	uint32	channel_id;
	uint16	status;
	uint16	bus_number;
	uint16	dev_number;
	char	path[17];
	int	port_number;
	int	isCompositeDevice;

	LIBUSB_DEVICE_HANDLE * libusb_handle;
	LIBUSB_DEVICE_HANDLE * hub_handle;
	LIBUSB_DEVICE * libusb_dev;
	LIBUSB_DEVICE_DESCRIPTOR * devDescriptor;
	MSUSB_CONFIG_DESCRIPTOR * MsConfig;
	LIBUSB_CONFIG_DESCRIPTOR * LibusbConfig;

	REQUEST_QUEUE * request_queue;
	/* Used in isochronous transfer */
	void * isoch_queue;

	pthread_mutex_t mutex_isoch;
	sem_t   sem_id;
};
typedef UDEVICE * PUDEVICE;


int
udev_new_by_id(uint16_t idVendor, uint16_t idProduct, IUDEVICE ***devArray);
IUDEVICE*
udev_new_by_addr(int bus_number, int dev_number);

extern int libusb_debug;

#endif /* __LIBUSB_UDEVICE_H */
