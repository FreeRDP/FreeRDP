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

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include "urbdrc_types.h"
#include "urbdrc_main.h"

#include "libusb_udevice.h"

int libusb_debug;

#define BASIC_STATE_FUNC_DEFINED(_arg, _type) \
static _type udevman_get_##_arg (IUDEVMAN* idevman) \
{ \
	UDEVMAN * udevman = (UDEVMAN *) idevman; \
	return udevman->_arg; \
} \
static void udevman_set_##_arg (IUDEVMAN* idevman, _type _t) \
{ \
	UDEVMAN * udevman = (UDEVMAN *) idevman; \
	udevman->_arg = _t; \
}

#define BASIC_STATE_FUNC_REGISTER(_arg, _man) \
	_man->iface.get_##_arg = udevman_get_##_arg; \
	_man->iface.set_##_arg = udevman_set_##_arg

typedef struct _UDEVMAN UDEVMAN;

struct _UDEVMAN
{
	IUDEVMAN iface;

	IUDEVICE* idev; /* iterator device */
	IUDEVICE* head; /* head device in linked list */
	IUDEVICE* tail; /* tail device in linked list */

	UINT32 defUsbDevice;
	UINT16 flags;
	int device_num;
	int sem_timeout;

	pthread_mutex_t devman_loading;
	sem_t sem_urb_lock;
};
typedef UDEVMAN* PUDEVMAN;

static void udevman_rewind(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	udevman->idev = udevman->head;
}

static int udevman_has_next(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;

	if (udevman->idev == NULL)
		return 0;
	else
		return 1;
}

static IUDEVICE* udevman_get_next(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	IUDEVICE* pdev;

	pdev = udevman->idev;
	udevman->idev = (IUDEVICE*) ((UDEVICE*) udevman->idev)->next;

	return pdev;
}

static IUDEVICE* udevman_get_udevice_by_addr(IUDEVMAN* idevman, int bus_number, int dev_number)
{
	IUDEVICE* pdev;

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		pdev = idevman->get_next(idevman);

		if ((pdev->get_bus_number(pdev) == bus_number) && (pdev->get_dev_number(pdev) == dev_number))
		{
			idevman->loading_unlock(idevman);
			return pdev;
		}
	}

	idevman->loading_unlock(idevman);
	WLog_WARN(TAG, "bus:%d dev:%d not exist in udevman",
			  bus_number, dev_number);
	return NULL;
}

static int udevman_register_udevice(IUDEVMAN* idevman, int bus_number, int dev_number,
	int UsbDevice, UINT16 idVendor, UINT16 idProduct, int flag)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	IUDEVICE* pdev = NULL;
	IUDEVICE** devArray;
	int i, num, addnum = 0;
	
	pdev = (IUDEVICE*) udevman_get_udevice_by_addr(idevman, bus_number, dev_number);

	if (pdev != NULL)
		return 0;
	
	if (flag == UDEVMAN_FLAG_ADD_BY_ADDR)
	{
		pdev = udev_new_by_addr(bus_number, dev_number);

		if (pdev == NULL)
			return 0;

		pdev->set_UsbDevice(pdev, UsbDevice);
		idevman->loading_lock(idevman);

		if (udevman->head == NULL)
		{
			/* linked list is empty */
			udevman->head = pdev;
			udevman->tail = pdev;
		}
		else
		{
			/* append device to the end of the linked list */
			udevman->tail->set_p_next(udevman->tail, pdev);
			pdev->set_p_prev(pdev, udevman->tail);
			udevman->tail = pdev;
		}

		udevman->device_num += 1;
		idevman->loading_unlock(idevman);
	}
	else if (flag == UDEVMAN_FLAG_ADD_BY_VID_PID)
	{
		addnum = 0;
		/* register all device that match pid vid */
		num = udev_new_by_id(idVendor, idProduct, &devArray);

		for (i = 0; i < num; i++)
		{
			pdev = devArray[i];

			if (udevman_get_udevice_by_addr(idevman, 
					pdev->get_bus_number(pdev), pdev->get_dev_number(pdev)) != NULL)
			{
				zfree(pdev);
				continue;
			}

			pdev->set_UsbDevice(pdev, UsbDevice);
			idevman->loading_lock(idevman);

			if (udevman->head == NULL)
			{
				/* linked list is empty */
				udevman->head = pdev;
				udevman->tail = pdev;
			}
			else
			{
				/* append device to the end of the linked list */
				udevman->tail->set_p_next(udevman->tail, pdev);
				pdev->set_p_prev(pdev, udevman->tail);
				udevman->tail = pdev;
			}

			udevman->device_num += 1;
			idevman->loading_unlock(idevman);
			addnum++;
		}

		zfree(devArray);
		return addnum;
	}
	else
	{
		WLog_ERR(TAG,  "udevman_register_udevice: function error!!");
		return 0;
	}

	return 1;
}

static int udevman_unregister_udevice(IUDEVMAN* idevman, int bus_number, int dev_number)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	UDEVICE * pdev, * dev;
	int ret = 0, err = 0;

	dev = (UDEVICE*) udevman_get_udevice_by_addr(idevman, bus_number, dev_number);

	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman) != 0)
	{
		pdev = (UDEVICE*) idevman->get_next(idevman);

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
				pdev = (UDEVICE *)dev->next;
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
		/* reset device */
		if (err != LIBUSB_ERROR_NO_DEVICE)
		{
			ret = libusb_reset_device(dev->libusb_handle);
			if (ret<0)
			{
				WLog_ERR(TAG, "libusb_reset_device: ERROR!!ret:%d", ret);
			}
		}
		
		/* release all interface and  attach kernel driver */
		dev->iface.attach_kernel_driver((IUDEVICE*)dev);   
		
		if(dev->request_queue) zfree(dev->request_queue);
		/* free the config descriptor that send from windows */
		msusb_msconfig_free(dev->MsConfig);

		libusb_close (dev->libusb_handle);
		libusb_close (dev->hub_handle);
		
		sem_destroy(&dev->sem_id);
		/* free device info */
		if (dev->devDescriptor)
			zfree(dev->devDescriptor);
		if (dev)
			zfree(dev); 
		return 1; /* unregistration successful */
	}

	/* if we reach this point, the device wasn't found */
	return 0;
}

static void udevman_parse_device_addr(char* str, int* id1, int* id2, char sign)
{
	char s1[8];
	char* s2;

	ZeroMemory(s1, sizeof(s1));

	s2 = (strchr(str, sign)) + 1; 
	strncpy(s1, str, strlen(str) - (strlen(s2) + 1));

	*id1 = atoi(s1);
	*id2 = atoi(s2);
}

static void udevman_parse_device_pid_vid(char* str, int* id1, int* id2, char sign)
{
	char s1[8];
	char* s2;

	ZeroMemory(s1, sizeof(s1));

	s2 = (strchr(str, sign)) + 1; 
	strncpy(s1, str, strlen(str) - (strlen(s2) + 1));

	*id1 = (int) strtol(s1, NULL, 16);
	*id2 = (int) strtol(s2, NULL, 16);
}

static int udevman_check_device_exist_by_id(IUDEVMAN* idevman, UINT16 idVendor, UINT16 idProduct)
{
	if (libusb_open_device_with_vid_pid(NULL, idVendor, idProduct))
		return 1;

	return 0;
}

static int udevman_is_auto_add(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	return (udevman->flags & UDEVMAN_FLAG_ADD_BY_AUTO) ? 1 : 0;
}

static IUDEVICE* udevman_get_udevice_by_UsbDevice_try_again(IUDEVMAN* idevman, UINT32 UsbDevice)
{
	UDEVICE* pdev;
	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		pdev = (UDEVICE*) idevman->get_next(idevman);

		if (pdev->UsbDevice == UsbDevice)
		{
			idevman->loading_unlock(idevman);
			return (IUDEVICE*) pdev;
		}
	}

	idevman->loading_unlock(idevman);

	return NULL;
}

static IUDEVICE* udevman_get_udevice_by_UsbDevice(IUDEVMAN* idevman, UINT32 UsbDevice)
{
	UDEVICE* pdev;
	idevman->loading_lock(idevman);
	idevman->rewind(idevman);

	while (idevman->has_next(idevman))
	{
		pdev = (UDEVICE*) idevman->get_next(idevman);

		if (pdev->UsbDevice == UsbDevice)
		{
			idevman->loading_unlock(idevman);
			return (IUDEVICE*) pdev;
		}
	}

	idevman->loading_unlock(idevman);

	/* try again */

	pdev = (UDEVICE*) idevman->get_udevice_by_UsbDevice_try_again(idevman, UsbDevice);

	if (pdev)
	{
		return (IUDEVICE*) pdev;
	}

	WLog_ERR(TAG, "0x%x ERROR!!", UsbDevice);
	return NULL;
}

static void udevman_loading_lock(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	pthread_mutex_lock(&udevman->devman_loading);
}

static void udevman_loading_unlock(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	pthread_mutex_unlock(&udevman->devman_loading);
}

static void udevman_wait_urb(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	sem_wait(&udevman->sem_urb_lock);
}

static void udevman_push_urb(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;
	sem_post(&udevman->sem_urb_lock);
}

BASIC_STATE_FUNC_DEFINED(defUsbDevice, UINT32)
BASIC_STATE_FUNC_DEFINED(device_num, int)
BASIC_STATE_FUNC_DEFINED(sem_timeout, int)

static void udevman_free(IUDEVMAN* idevman)
{
	UDEVMAN* udevman = (UDEVMAN*) idevman;

	pthread_mutex_destroy(&udevman->devman_loading);
	sem_destroy(&udevman->sem_urb_lock);

	libusb_exit(NULL);

	/* free udevman */

	if (udevman)
		zfree(udevman);
}

static void udevman_load_interface(UDEVMAN * udevman)
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
	udevman->iface.get_udevice_by_UsbDevice_try_again = 
		udevman_get_udevice_by_UsbDevice_try_again;

	/* Extension */
	udevman->iface.check_device_exist_by_id = udevman_check_device_exist_by_id;
	udevman->iface.isAutoAdd = udevman_is_auto_add;

	/* Basic state */
	BASIC_STATE_FUNC_REGISTER(defUsbDevice, udevman);
	BASIC_STATE_FUNC_REGISTER(device_num, udevman);
	BASIC_STATE_FUNC_REGISTER(sem_timeout, udevman);

	/* control semaphore or mutex lock */
	udevman->iface.loading_lock = udevman_loading_lock;
	udevman->iface.loading_unlock = udevman_loading_unlock;
	udevman->iface.push_urb = udevman_push_urb;
	udevman->iface.wait_urb = udevman_wait_urb;
}

COMMAND_LINE_ARGUMENT_A urbdrc_udevman_args[] =
{
	{ "dbg", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "debug" },
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<devices>", NULL, NULL, -1, NULL, "device list" },
	{ "id", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "FLAG_ADD_BY_VID_PID" },
	{ "addr", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "FLAG_ADD_BY_ADDR" },
	{ "auto", COMMAND_LINE_VALUE_FLAG, "", NULL, BoolValueFalse, -1, NULL, "FLAG_ADD_BY_AUTO" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void urbdrc_udevman_register_devices(UDEVMAN* udevman, char* devices)
{
	char* token;
	int idVendor;
	int idProduct;
	int bus_number;
	int dev_number;
	int success = 0;
	char hardware_id[16];
	char* default_devices = "id";
	UINT32 UsbDevice = BASE_USBDEVICE_NUM;

	if (!devices)
		devices = default_devices;

	/* register all usb devices */
	token = strtok(devices, "#");

	while (token)
	{
		bus_number = 0;
		dev_number = 0;
		idVendor = 0;
		idProduct = 0;

		strcpy(hardware_id, token);
		token = strtok(NULL, "#");

		if (udevman->flags & UDEVMAN_FLAG_ADD_BY_VID_PID)
		{
			udevman_parse_device_pid_vid(hardware_id, &idVendor, &idProduct, ':');
			success = udevman->iface.register_udevice((IUDEVMAN*) udevman,
				0, 0, UsbDevice, (UINT16) idVendor, (UINT16) idProduct, UDEVMAN_FLAG_ADD_BY_VID_PID);
		}
		else if (udevman->flags & UDEVMAN_FLAG_ADD_BY_ADDR)
		{
			udevman_parse_device_addr(hardware_id, &bus_number, &dev_number, ':');

			success = udevman->iface.register_udevice((IUDEVMAN*) udevman,
				bus_number, dev_number, UsbDevice, 0, 0, UDEVMAN_FLAG_ADD_BY_ADDR);
		}

		if (success)
			UsbDevice++;
	}

	udevman->defUsbDevice = UsbDevice;
}

static void urbdrc_udevman_parse_addin_args(UDEVMAN* udevman, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			urbdrc_udevman_args, flags, udevman, NULL, NULL);

	arg = urbdrc_udevman_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dbg")
		{
			WLog_SetLogLevel(WLog_Get(TAG), WLOG_TRACE);
		}
		CommandLineSwitchCase(arg, "dev")
		{
			urbdrc_udevman_register_devices(udevman, arg->Value);
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
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef BUILTIN_CHANNELS
#define freerdp_urbdrc_client_subsystem_entry	libusb_freerdp_urbdrc_client_subsystem_entry
#else
#define freerdp_urbdrc_client_subsystem_entry	FREERDP_API freerdp_urbdrc_client_subsystem_entry
#endif

int freerdp_urbdrc_client_subsystem_entry(PFREERDP_URBDRC_SERVICE_ENTRY_POINTS pEntryPoints)
{
	UDEVMAN* udevman;
	ADDIN_ARGV* args = pEntryPoints->args;

	libusb_init(NULL);

	udevman = (PUDEVMAN) malloc(sizeof(UDEVMAN));
	if (!udevman)
		return -1;
	udevman->device_num = 0;
	udevman->idev = NULL;
	udevman->head = NULL;
	udevman->tail = NULL;   
	udevman->sem_timeout = 0;
	udevman->flags = UDEVMAN_FLAG_ADD_BY_VID_PID;

	pthread_mutex_init(&udevman->devman_loading, NULL);
	sem_init(&udevman->sem_urb_lock, 0, MAX_URB_REQUSET_NUM);

	/* load usb device service management */
	udevman_load_interface(udevman);

	/* set debug flag, to enable Debug message for usb data transfer */

	libusb_debug = 10;

	urbdrc_udevman_parse_addin_args(udevman, args);

	pEntryPoints->pRegisterUDEVMAN(pEntryPoints->plugin, (IUDEVMAN*) udevman);

	WLog_DBG(TAG, "UDEVMAN device registered.");

	return 0;
}
