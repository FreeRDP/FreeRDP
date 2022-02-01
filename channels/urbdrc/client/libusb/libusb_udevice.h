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

#ifndef FREERDP_CHANNEL_URBDRC_CLIENT_LIBUSB_UDEVICE_H
#define FREERDP_CHANNEL_URBDRC_CLIENT_LIBUSB_UDEVICE_H

#include <winpr/windows.h>
#include <libusb.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"

typedef struct libusb_device LIBUSB_DEVICE;
typedef struct libusb_device_handle LIBUSB_DEVICE_HANDLE;
typedef struct libusb_device_descriptor LIBUSB_DEVICE_DESCRIPTOR;
typedef struct libusb_config_descriptor LIBUSB_CONFIG_DESCRIPTOR;
typedef struct libusb_interface LIBUSB_INTERFACE;
typedef struct libusb_interface_descriptor LIBUSB_INTERFACE_DESCRIPTOR;
typedef struct libusb_endpoint_descriptor LIBUSB_ENDPOINT_DESCEIPTOR;

typedef struct
{
	IUDEVICE iface;

	void* udev;
	void* prev;
	void* next;

	UINT32 UsbDevice;     /* An unique interface ID */
	UINT32 ReqCompletion; /* An unique interface ID */
	IWTSVirtualChannelManager* channelManager;
	UINT32 channelID;
	UINT16 status;
	BYTE bus_number;
	BYTE dev_number;
	char path[17];
	int port_number;
	int isCompositeDevice;

	LIBUSB_DEVICE_HANDLE* libusb_handle;
	LIBUSB_DEVICE_HANDLE* hub_handle;
	LIBUSB_DEVICE* libusb_dev;
	LIBUSB_DEVICE_DESCRIPTOR* devDescriptor;
	MSUSB_CONFIG_DESCRIPTOR* MsConfig;
	LIBUSB_CONFIG_DESCRIPTOR* LibusbConfig;

	wArrayList* request_queue;

	URBDRC_PLUGIN* urbdrc;
} UDEVICE;
typedef UDEVICE* PUDEVICE;

size_t udev_new_by_id(URBDRC_PLUGIN* urbdrc, libusb_context* ctx, UINT16 idVendor, UINT16 idProduct,
                      IUDEVICE*** devArray);
IUDEVICE* udev_new_by_addr(URBDRC_PLUGIN* urbdrc, libusb_context* ctx, BYTE bus_number,
                           BYTE dev_number);
const char* usb_interface_class_to_string(uint8_t class);

#endif /* FREERDP_CHANNEL_URBDRC_CLIENT_LIBUSB_UDEVICE_H */
