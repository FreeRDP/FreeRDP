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
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"

#include "libusb_udevice.h"

#include <libusb.h>

#if !defined(LIBUSB_HOTPLUG_NO_FLAGS)
#define LIBUSB_HOTPLUG_NO_FLAGS 0
#endif

#define BASIC_STATE_FUNC_DEFINED(_arg, _type)                   \
	static _type udevman_get_##_arg(IUDEVMAN* idevman)          \
	{                                                           \
		UDEVMAN* udevman = (UDEVMAN*)idevman;                   \
		return udevman->_arg;                                   \
	}                                                           \
	static void udevman_set_##_arg(IUDEVMAN* idevman, _type _t) \
	{                                                           \
		UDEVMAN* udevman = (UDEVMAN*)idevman;                   \
		udevman->_arg = _t;                                     \
	}

#define BASIC_STATE_FUNC_REGISTER(_arg, _man)    \
	_man->iface.get_##_arg = udevman_get_##_arg; \
	_man->iface.set_##_arg = udevman_set_##_arg

typedef struct _UDEVMAN UDEVMAN;

struct _UDEVMAN
{
	IUDEVMAN iface;

	IUDEVICE* idev; /* iterator device */
	IUDEVICE* head; /* head device in linked list */
	IUDEVICE* tail; /* tail device in linked list */

	UINT16 flags;
	UINT32 device_num;
	UINT32 next_device_id;
	UINT32 channel_id;

	HANDLE devman_loading;
	libusb_context* context;
	HANDLE thread;
	BOOL running;
};
typedef UDEVMAN* PUDEVMAN;

static BOOL poll_libusb_events(UDEVMAN* udevman);

static void udevman_rewind(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	udevman->idev = udevman->head;
}

static BOOL udevman_has_next(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;

	if (!udevman || !udevman->idev)
		return FALSE;
	else
		return TRUE;
}

static IUDEVICE* udevman_get_next(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	IUDEVICE* pdev;
	pdev = udevman->idev;
	udevman->idev = (IUDEVICE*)((UDEVICE*)udevman->idev)->next;
	return pdev;
}

static IUDEVICE* udevman_get_udevice_by_addr(IUDEVMAN* idevman, BYTE bus_number, BYTE dev_number)
{
	IUDEVICE* dev = NULL;

	if (!idevman)
		return NULL;

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		IUDEVICE* pdev = idevman->get_next(idevman);

		if ((pdev->get_bus_number(pdev) == bus_number) &&
		    (pdev->get_dev_number(pdev) == dev_number))
		{
			dev = pdev;
			break;
		}
	}

	idevman->loading_unlock(idevman);
	return dev;
}

static size_t udevman_register_udevice(IUDEVMAN* idevman, BYTE bus_number, BYTE dev_number,
                                       UINT16 idVendor, UINT16 idProduct, UINT32 flag)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	IUDEVICE* pdev = NULL;
	IUDEVICE** devArray;
	URBDRC_PLUGIN* urbdrc;
	size_t i, num, addnum = 0;

	if (!idevman || !idevman->plugin)
		return 0;

	urbdrc = (URBDRC_PLUGIN*)idevman->plugin;
	pdev = (IUDEVICE*)udevman_get_udevice_by_addr(idevman, bus_number, dev_number);

	if (pdev != NULL)
		return 0;

	if (flag & UDEVMAN_FLAG_ADD_BY_ADDR)
	{
		UINT32 id;
		IUDEVICE* tdev = udev_new_by_addr(urbdrc, udevman->context, bus_number, dev_number);

		if (tdev == NULL)
			return 0;

		id = idevman->get_next_device_id(idevman);
		tdev->set_UsbDevice(tdev, id);
		idevman->loading_lock(idevman);

		if (udevman->head == NULL)
		{
			/* linked list is empty */
			udevman->head = tdev;
			udevman->tail = tdev;
		}
		else
		{
			/* append device to the end of the linked list */
			udevman->tail->set_p_next(udevman->tail, tdev);
			tdev->set_p_prev(tdev, udevman->tail);
			udevman->tail = tdev;
		}

		udevman->device_num += 1;
		idevman->loading_unlock(idevman);
	}
	else if (flag & UDEVMAN_FLAG_ADD_BY_VID_PID)
	{
		addnum = 0;
		/* register all device that match pid vid */
		num = udev_new_by_id(urbdrc, udevman->context, idVendor, idProduct, &devArray);

		for (i = 0; i < num; i++)
		{
			UINT32 id;
			IUDEVICE* tdev = devArray[i];

			if (udevman_get_udevice_by_addr(idevman, tdev->get_bus_number(tdev),
			                                tdev->get_dev_number(tdev)) != NULL)
			{
				tdev->free(tdev);
				devArray[i] = NULL;
				continue;
			}

			id = idevman->get_next_device_id(idevman);
			tdev->set_UsbDevice(tdev, id);
			idevman->loading_lock(idevman);

			if (udevman->head == NULL)
			{
				/* linked list is empty */
				udevman->head = tdev;
				udevman->tail = tdev;
			}
			else
			{
				/* append device to the end of the linked list */
				udevman->tail->set_p_next(udevman->tail, tdev);
				tdev->set_p_prev(tdev, udevman->tail);
				udevman->tail = tdev;
			}

			udevman->device_num += 1;
			idevman->loading_unlock(idevman);
			addnum++;
		}

		free(devArray);
		return addnum;
	}
	else
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "udevman_register_udevice: Invalid flag=%08 " PRIx32,
		           flag);
		return 0;
	}

	return 1;
}

static BOOL udevman_unregister_udevice(IUDEVMAN* idevman, BYTE bus_number, BYTE dev_number)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	UDEVICE* pdev;
	UDEVICE* dev = (UDEVICE*)udevman_get_udevice_by_addr(idevman, bus_number, dev_number);

	if (!dev || !idevman)
		return FALSE;

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		pdev = (UDEVICE*)idevman->get_next(idevman);

		if (pdev == dev) /* device exists */
		{
			/* set previous device to point to next device */
			if (dev->prev != NULL)
			{
				/* unregistered device is not the head */
				pdev = dev->prev;
				pdev->next = dev->next;
			}
			else
			{
				/* unregistered device is the head, update head */
				udevman->head = (IUDEVICE*)dev->next;
			}

			/* set next device to point to previous device */

			if (dev->next != NULL)
			{
				/* unregistered device is not the tail */
				pdev = (UDEVICE*)dev->next;
				pdev->prev = dev->prev;
			}
			else
			{
				/* unregistered device is the tail, update tail */
				udevman->tail = (IUDEVICE*)dev->prev;
			}

			udevman->device_num--;
			break;
		}
	}

	idevman->loading_unlock(idevman);

	if (dev)
	{
		dev->iface.free(&dev->iface);
		return TRUE; /* unregistration successful */
	}

	/* if we reach this point, the device wasn't found */
	return FALSE;
}

static BOOL udevman_unregister_all_udevices(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;

	if (!idevman)
		return FALSE;

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		UDEVICE* dev = (UDEVICE*)idevman->get_next(idevman);

		if (!dev)
			continue;

		/* set previous device to point to next device */
		if (dev->prev != NULL)
		{
			/* unregistered device is not the head */
			UDEVICE* pdev = dev->prev;
			pdev->next = dev->next;
		}
		else
		{
			/* unregistered device is the head, update head */
			udevman->head = (IUDEVICE*)dev->next;
		}

		/* set next device to point to previous device */

		if (dev->next != NULL)
		{
			/* unregistered device is not the tail */
			UDEVICE* pdev = (UDEVICE*)dev->next;
			pdev->prev = dev->prev;
		}
		else
		{
			/* unregistered device is the tail, update tail */
			udevman->tail = (IUDEVICE*)dev->prev;
		}

		dev->iface.free(&dev->iface);
		udevman->device_num--;
	}

	idevman->loading_unlock(idevman);

	return TRUE;
}

static BOOL udevman_parse_device_addr(const char* str, size_t maxLen, UINT8* id1, UINT8* id2,
                                      char sign)
{
	unsigned long rc;
	char s1[8] = { 0 };
	char* s2;
	size_t len = strnlen(str, maxLen);
	size_t cpLen;
	s2 = (strchr(str, sign)) + 1;

	if (!s2)
		return FALSE;

	cpLen = len - (strnlen(s2, len) + 1);

	if (cpLen >= sizeof(s1))
		cpLen = sizeof(s1) - 1;

	strncpy(s1, str, cpLen);
	rc = strtoul(s1, NULL, 16);

	if ((rc > UINT8_MAX) || (errno != 0))
		return FALSE;

	*id1 = (UINT8)rc;
	rc = strtoul(s2, NULL, 16);

	if ((rc > UINT8_MAX) || (errno != 0))
		return FALSE;

	*id2 = (UINT8)rc;
	return TRUE;
}

static BOOL udevman_parse_device_pid_vid(const char* str, size_t maxLen, UINT16* id1, UINT16* id2,
                                         char sign)
{
	unsigned long rc;
	char s1[8] = { 0 };
	char* s2;
	size_t len = strnlen(str, maxLen);
	size_t cpLen;
	s2 = (strchr(str, sign)) + 1;

	if (!s2)
		return FALSE;

	cpLen = len - (strnlen(s2, len) + 1);

	if (cpLen >= sizeof(s1))
		cpLen = sizeof(s1) - 1;

	strncpy(s1, str, cpLen);
	errno = 0;
	rc = strtoul(s1, NULL, 16);

	if ((rc > UINT16_MAX) || (errno != 0))
		return FALSE;

	*id1 = (UINT16)rc;
	rc = strtoul(s2, NULL, 16);

	if ((rc > UINT16_MAX) || (errno != 0))
		return FALSE;

	*id2 = (UINT16)rc;
	return TRUE;
}

static int udevman_is_auto_add(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	return (udevman->flags & UDEVMAN_FLAG_ADD_BY_AUTO) ? 1 : 0;
}

static IUDEVICE* udevman_get_udevice_by_UsbDevice(IUDEVMAN* idevman, UINT32 UsbDevice)
{
	UDEVICE* pdev;
	URBDRC_PLUGIN* urbdrc;

	if (!idevman || !idevman->plugin)
		return NULL;

	/* Mask highest 2 bits, must be ignored */
	UsbDevice = UsbDevice & INTERFACE_ID_MASK;
	urbdrc = (URBDRC_PLUGIN*)idevman->plugin;
	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		pdev = (UDEVICE*)idevman->get_next(idevman);

		if (pdev->UsbDevice == UsbDevice)
		{
			idevman->loading_unlock(idevman);
			return (IUDEVICE*)pdev;
		}
	}

	idevman->loading_unlock(idevman);
	WLog_Print(urbdrc->log, WLOG_WARN, "Failed to find a USB device mapped to deviceId=%08" PRIx32,
	           UsbDevice);
	return NULL;
}

static void udevman_loading_lock(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	WaitForSingleObject(udevman->devman_loading, INFINITE);
}

static void udevman_loading_unlock(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	ReleaseMutex(udevman->devman_loading);
}

BASIC_STATE_FUNC_DEFINED(device_num, UINT32)

static UINT32 udevman_get_next_device_id(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	return udevman->next_device_id++;
}

static void udevman_set_next_device_id(IUDEVMAN* idevman, UINT32 _t)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;
	udevman->next_device_id = _t;
}

static void udevman_free(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;

	if (!udevman)
		return;

	udevman->running = FALSE;
	WaitForSingleObject(udevman->thread, INFINITE);

	udevman_unregister_all_udevices(idevman);
	CloseHandle(udevman->devman_loading);
	CloseHandle(udevman->thread);
	libusb_exit(udevman->context);

	free(udevman);
}

static BOOL filter_by_class(uint8_t bDeviceClass, uint8_t bDeviceSubClass)
{
	switch (bDeviceClass)
	{
		case LIBUSB_CLASS_AUDIO:
		case LIBUSB_CLASS_HID:
		case LIBUSB_CLASS_MASS_STORAGE:
		case LIBUSB_CLASS_HUB:
		case LIBUSB_CLASS_SMART_CARD:
			return TRUE;
		default:
			break;
	}

	switch (bDeviceSubClass)
	{
		default:
			break;
	}

	return FALSE;
}

static BOOL append(char* dst, size_t length, const char* src)
{
	size_t slen = strlen(src);
	size_t dlen = strnlen(dst, length);
	if (dlen + slen >= length)
		return FALSE;
	strcat(dst, src);
	return TRUE;
}

static BOOL device_is_filtered(struct libusb_device* dev,
                               const struct libusb_device_descriptor* desc,
                               libusb_hotplug_event event)
{
	char buffer[8192] = { 0 };
	char* what;
	BOOL filtered = FALSE;
	append(buffer, sizeof(buffer), usb_interface_class_to_string(desc->bDeviceClass));
	if (filter_by_class(desc->bDeviceClass, desc->bDeviceSubClass))
		filtered = TRUE;

	switch (desc->bDeviceClass)
	{
		case LIBUSB_CLASS_PER_INTERFACE:
		{
			struct libusb_config_descriptor* config = NULL;
			int rc = libusb_get_active_config_descriptor(dev, &config);
			if (rc == LIBUSB_SUCCESS)
			{
				uint8_t x;

				for (x = 0; x < config->bNumInterfaces; x++)
				{
					uint8_t y;
					const struct libusb_interface* ifc = &config->interface[x];
					for (y = 0; y < ifc->num_altsetting; y++)
					{
						const struct libusb_interface_descriptor* const alt = &ifc->altsetting[y];
						if (filter_by_class(alt->bInterfaceClass, alt->bInterfaceSubClass))
							filtered = TRUE;

						append(buffer, sizeof(buffer), "|");
						append(buffer, sizeof(buffer),
						       usb_interface_class_to_string(alt->bInterfaceClass));
					}
				}
			}
			libusb_free_config_descriptor(config);
		}
		break;
		default:
			break;
	}

	if (filtered)
		what = "Filtered";
	else
	{
		switch (event)
		{
			case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
				what = "Hotplug remove";
				break;
			case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
				what = "Hotplug add";
				break;
			default:
				what = "Hotplug unknown";
				break;
		}
	}

	WLog_DBG(TAG, "%s device VID=0x%04X,PID=0x%04X class %s", what, desc->idVendor, desc->idProduct,
	         buffer);
	return filtered;
}

static int hotplug_callback(struct libusb_context* ctx, struct libusb_device* dev,
                            libusb_hotplug_event event, void* user_data)
{
	struct libusb_device_descriptor desc;
	IUDEVMAN* idevman = (IUDEVMAN*)user_data;
	const uint8_t bus = libusb_get_bus_number(dev);
	const uint8_t addr = libusb_get_device_address(dev);
	int rc = libusb_get_device_descriptor(dev, &desc);

	WINPR_UNUSED(ctx);

	if (rc != LIBUSB_SUCCESS)
		return rc;

	if (device_is_filtered(dev, &desc, event))
		return 0;

	switch (event)
	{
		case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
			if (idevman->isAutoAdd(idevman))
				add_device(idevman, DEVICE_ADD_FLAG_ALL, bus, addr, desc.idVendor, desc.idProduct);
			break;

		case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
			del_device(idevman, DEVICE_ADD_FLAG_ALL, bus, addr, desc.idVendor, desc.idProduct);
			break;

		default:
			break;
	}

	return 0;
}

static BOOL udevman_initialize(IUDEVMAN* idevman, UINT32 channelId)
{
	UDEVMAN* udevman = (UDEVMAN*)idevman;

	if (!udevman)
		return FALSE;

	idevman->status &= ~URBDRC_DEVICE_CHANNEL_CLOSED;
	idevman->controlChannelId = channelId;
	return TRUE;
}

static void udevman_load_interface(UDEVMAN* udevman)
{
	/* standard */
	udevman->iface.free = udevman_free;
	/* manage devices */
	udevman->iface.rewind = udevman_rewind;
	udevman->iface.get_next = udevman_get_next;
	udevman->iface.has_next = udevman_has_next;
	udevman->iface.register_udevice = udevman_register_udevice;
	udevman->iface.unregister_udevice = udevman_unregister_udevice;
	udevman->iface.get_udevice_by_UsbDevice = udevman_get_udevice_by_UsbDevice;
	/* Extension */
	udevman->iface.isAutoAdd = udevman_is_auto_add;
	/* Basic state */
	BASIC_STATE_FUNC_REGISTER(device_num, udevman);
	BASIC_STATE_FUNC_REGISTER(next_device_id, udevman);

	/* control semaphore or mutex lock */
	udevman->iface.loading_lock = udevman_loading_lock;
	udevman->iface.loading_unlock = udevman_loading_unlock;
	udevman->iface.initialize = udevman_initialize;
}

static BOOL urbdrc_udevman_register_devices(UDEVMAN* udevman, const char* devices)
{
	BOOL rc = FALSE;
	char* token;
	int success = 0;
	char* tmp;
	char hardware_id[16];
	const char* default_devices = "id";

	if (!devices)
		tmp = _strdup(default_devices);
	else
		tmp = _strdup(devices);

	/* register all usb devices */
	token = strtok(tmp, "#");

	while (token)
	{
		strcpy(hardware_id, token);
		token = strtok(NULL, "#");

		if (udevman->flags & UDEVMAN_FLAG_ADD_BY_VID_PID)
		{
			UINT16 idVendor, idProduct;

			if (!udevman_parse_device_pid_vid(hardware_id, sizeof(hardware_id), &idVendor,
			                                  &idProduct, ':'))
				goto fail;

			success = add_device(&udevman->iface, DEVICE_ADD_FLAG_VENDOR | DEVICE_ADD_FLAG_PRODUCT,
			                     0, 0, idVendor, idProduct);
		}
		else if (udevman->flags & UDEVMAN_FLAG_ADD_BY_ADDR)
		{
			UINT8 bus_number, dev_number;

			if (!udevman_parse_device_addr(hardware_id, sizeof(hardware_id), &bus_number,
			                               &dev_number, ':'))
				goto fail;

			success = add_device(&udevman->iface, DEVICE_ADD_FLAG_BUS | DEVICE_ADD_FLAG_DEV,
			                     bus_number, dev_number, 0, 0);
		}
	}

	rc = TRUE;
fail:
	free(tmp);
	return rc;
}

static UINT urbdrc_udevman_parse_addin_args(UDEVMAN* udevman, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	LPSTR devices = NULL;
	const UINT16 mask = UDEVMAN_FLAG_ADD_BY_VID_PID | UDEVMAN_FLAG_ADD_BY_ADDR;
	COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A urbdrc_udevman_args[] = {
		{ "dbg", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "debug" },
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<devices>", NULL, NULL, -1, NULL, "device list" },
		{ "id", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL,
		  "FLAG_ADD_BY_VID_PID" },
		{ "addr", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "FLAG_ADD_BY_ADDR" },
		{ "auto", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "FLAG_ADD_BY_AUTO" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;
	status = CommandLineParseArgumentsA(args->argc, args->argv, urbdrc_udevman_args, flags, udevman,
	                                    NULL, NULL);

	if (status != CHANNEL_RC_OK)
		return status;

	arg = urbdrc_udevman_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dbg")
		{
			WLog_SetLogLevel(WLog_Get(TAG), WLOG_TRACE);
		}
		CommandLineSwitchCase(arg, "dev")
		{
			devices = arg->Value;
		}
		CommandLineSwitchCase(arg, "id")
		{
			udevman->flags = UDEVMAN_FLAG_ADD_BY_VID_PID;
		}
		CommandLineSwitchCase(arg, "addr")
		{
			udevman->flags = UDEVMAN_FLAG_ADD_BY_ADDR;
		}
		CommandLineSwitchCase(arg, "auto")
		{
			udevman->flags |= UDEVMAN_FLAG_ADD_BY_AUTO;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	/* Can not add devices by address and VID/PID */
	if ((udevman->flags & mask) == mask)
		return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;

	/* Add listed devices after we know the format of addressing */
	if (devices)
	{
		if (!urbdrc_udevman_register_devices(udevman, devices))
			return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
	}

	return CHANNEL_RC_OK;
}

static BOOL poll_libusb_events(UDEVMAN* udevman)
{
	int rc = LIBUSB_SUCCESS;
	struct timeval tv = { 0, 500 };
	if (libusb_try_lock_events(udevman->context))
	{
		if (libusb_event_handling_ok(udevman->context))
		{
			rc = libusb_handle_events_locked(udevman->context, &tv);
			if (rc != LIBUSB_SUCCESS)
				WLog_WARN(TAG, "libusb_handle_events_locked %d", rc);
		}
		libusb_unlock_events(udevman->context);
	}
	else
	{
		libusb_lock_event_waiters(udevman->context);
		if (libusb_event_handler_active(udevman->context))
		{
			rc = libusb_wait_for_event(udevman->context, &tv);
			if (rc < LIBUSB_SUCCESS)
				WLog_WARN(TAG, "libusb_wait_for_event %d", rc);
		}
		libusb_unlock_event_waiters(udevman->context);
	}

	return rc > 0;
}

static DWORD poll_thread(LPVOID lpThreadParameter)
{
	libusb_hotplug_callback_handle handle;
	UDEVMAN* udevman = (UDEVMAN*)lpThreadParameter;
	BOOL hasHotplug = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);

	if (hasHotplug)
	{
		int rc = libusb_hotplug_register_callback(
		    udevman->context,
		    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
		    LIBUSB_HOTPLUG_NO_FLAGS, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
		    LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, udevman, &handle);

		if (rc != LIBUSB_SUCCESS)
			udevman->running = FALSE;
	}
	else
		WLog_WARN(TAG, "Platform does not support libusb hotplug. USB devices plugged in later "
		               "will not be detected.");

	while (udevman->running)
	{
		poll_libusb_events(udevman);
	}

	if (hasHotplug)
		libusb_hotplug_deregister_callback(udevman->context, handle);

	/* Process remaining usb events */
	while (poll_libusb_events(udevman))
		;

	ExitThread(0);
	return 0;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_urbdrc_client_subsystem_entry libusb_freerdp_urbdrc_client_subsystem_entry
#else
#define freerdp_urbdrc_client_subsystem_entry FREERDP_API freerdp_urbdrc_client_subsystem_entry
#endif
int freerdp_urbdrc_client_subsystem_entry(PFREERDP_URBDRC_SERVICE_ENTRY_POINTS pEntryPoints)
{
	int rc;
	UINT status;
	UDEVMAN* udevman;
	ADDIN_ARGV* args = pEntryPoints->args;
	udevman = (PUDEVMAN)calloc(1, sizeof(UDEVMAN));

	if (!udevman)
		goto fail;

	udevman->next_device_id = BASE_USBDEVICE_NUM;
	udevman->iface.plugin = pEntryPoints->plugin;
	rc = libusb_init(&udevman->context);

	if (rc != LIBUSB_SUCCESS)
		goto fail;

#if LIBUSB_API_VERSION >= 0x01000106
	/* Prefer usbDK backend on windows. Not uspported on other platforms. */
	rc = libusb_set_option(udevman->context, LIBUSB_OPTION_USE_USBDK);
	switch (rc)
	{
		case LIBUSB_SUCCESS:
			break;
		case LIBUSB_ERROR_NOT_FOUND:
		case LIBUSB_ERROR_NOT_SUPPORTED:
			WLog_WARN(TAG, "LIBUSB_OPTION_USE_USBDK %s [%d]", libusb_strerror(rc), rc);
			break;
		default:
			WLog_ERR(TAG, "LIBUSB_OPTION_USE_USBDK %s [%d]", libusb_strerror(rc), rc);
			goto fail;
	}
#endif

	udevman->flags = UDEVMAN_FLAG_ADD_BY_VID_PID;
	udevman->devman_loading = CreateMutexA(NULL, FALSE, "devman_loading");

	if (!udevman->devman_loading)
		goto fail;

	/* load usb device service management */
	udevman_load_interface(udevman);
	status = urbdrc_udevman_parse_addin_args(udevman, args);

	if (status != CHANNEL_RC_OK)
		goto fail;

	udevman->running = TRUE;
	udevman->thread = CreateThread(NULL, 0, poll_thread, udevman, 0, NULL);

	if (!udevman->thread)
		goto fail;

	if (!pEntryPoints->pRegisterUDEVMAN(pEntryPoints->plugin, (IUDEVMAN*)udevman))
		goto fail;

	WLog_DBG(TAG, "UDEVMAN device registered.");
	return 0;
fail:
	udevman_free(&udevman->iface);
	return -1;
}
