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

#ifndef __SEACH_MAN_H
#define __SEACH_MAN_H

#include "urbdrc_types.h"

typedef struct _USB_SEARCHDEV USB_SEARCHDEV;

struct _USB_SEARCHDEV
{
	void* inode;
	void* prev;
	void* next;
	UINT16 idVendor;
	UINT16 idProduct;
};

typedef struct _USB_SEARCHMAN USB_SEARCHMAN;

struct _USB_SEARCHMAN
{
	int usb_numbers;
	UINT32 UsbDevice;
	USB_SEARCHDEV*	idev; /* iterator device */
	USB_SEARCHDEV*	head; /* head device in linked list */
	USB_SEARCHDEV*	tail; /* tail device in linked list */

	pthread_mutex_t mutex;
	HANDLE term_event;
	sem_t sem_term;
	int started;

	/* for urbdrc channel call back */
	void* urbdrc;

	/* load service */
	void (*rewind) (USB_SEARCHMAN* seachman);
	/* show all device in the list */
	void (*show) (USB_SEARCHMAN* self);
	/* start searchman */
	BOOL (*start) (USB_SEARCHMAN* self, void* (*func)(void*));
	/* close searchman */
	void (*close) (USB_SEARCHMAN* self);
	/* add a new usb device for search */
	BOOL (*add) (USB_SEARCHMAN* seachman, UINT16 idVendor, UINT16 idProduct);
	/* remove a usb device from list */
	int (*remove) (USB_SEARCHMAN* searchman, UINT16 idVendor, UINT16 idProduct);
	/* check list has next device*/
	int (*has_next) (USB_SEARCHMAN* seachman);
	/* get the device from list*/
	USB_SEARCHDEV* (*get_next) (USB_SEARCHMAN* seachman);
	/* free! */
	void (*free) (USB_SEARCHMAN* searchman);
};

USB_SEARCHMAN* searchman_new(void* urbdrc, UINT32 UsbDevice);

#endif

