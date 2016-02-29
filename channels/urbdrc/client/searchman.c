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
 *	 http://www.apache.org/licenses/LICENSE-2.0
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
#include <winpr/synch.h>

#include "searchman.h"

static void searchman_rewind(USB_SEARCHMAN* searchman)
{
	searchman->idev = searchman->head;
}

static int searchman_has_next(USB_SEARCHMAN* searchman)
{
	if (searchman->idev == NULL)
		return 0;
	else
		return 1;
}

static USB_SEARCHDEV* searchman_get_next(USB_SEARCHMAN* searchman)
{
	USB_SEARCHDEV* search;

	search = searchman->idev;
	searchman->idev = (USB_SEARCHDEV*) searchman->idev->next;

	return search;
}

static BOOL searchman_list_add(USB_SEARCHMAN* searchman, UINT16 idVendor, UINT16 idProduct)
{
	USB_SEARCHDEV*	search;
	
	search = (USB_SEARCHDEV*) calloc(1, sizeof(USB_SEARCHDEV));
	if (!search)
		return FALSE;
	
	search->idVendor = idVendor;
	search->idProduct = idProduct;

	if (searchman->head == NULL)
	{
		/* linked list is empty */
		searchman->head = search;
		searchman->tail = search;
	}
	else
	{
		/* append device to the end of the linked list */
		searchman->tail->next = (void*)search;
		search->prev = (void*)searchman->tail;
		searchman->tail = search;
	}
	searchman->usb_numbers += 1;
	
	return TRUE;
}

static int searchman_list_remove(USB_SEARCHMAN* searchman, UINT16 idVendor, UINT16 idProduct)
{
	USB_SEARCHDEV* search;
	USB_SEARCHDEV* point;

	searchman_rewind(searchman);

	while (searchman_has_next(searchman) != 0)
	{
		point = searchman_get_next(searchman);

		if (point->idVendor == idVendor &&
				point->idProduct == idProduct)
		{
			/* set previous device to point to next device */

			search = point;
			if (search->prev != NULL)
			{
				/* unregistered device is not the head */
				point = (USB_SEARCHDEV*)search->prev;
				point->next = search->next;
			}
			else
			{
				/* unregistered device is the head, update head */
				searchman->head = (USB_SEARCHDEV*)search->next;
			}

			/* set next device to point to previous device */

			if (search->next != NULL)
			{
				/* unregistered device is not the tail */
				point = (USB_SEARCHDEV*)search->next;
				point->prev = search->prev;
			}
			else
			{
				/* unregistered device is the tail, update tail */
				searchman->tail = (USB_SEARCHDEV*)search->prev;
			}
			searchman->usb_numbers--;
			
			free(search);
			
			return 1; /* unregistration successful */
		}
	}

	/* if we reach this point, the device wasn't found */
	return 0;
}

static BOOL searchman_start(USB_SEARCHMAN* self, void* func)
{
	HANDLE thread;
	
	/* create search thread */
	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, self, 0, NULL);
	if (!thread)
		return FALSE;

	if (!CloseHandle(thread))
		return FALSE;

	self->started = 1;
	return TRUE;
}

/* close thread */
static void searchman_close(USB_SEARCHMAN* self)
{
	SetEvent(self->term_event);
}

static void searchman_list_show(USB_SEARCHMAN* self)
{
	int num = 0;
	USB_SEARCHDEV* usb;
	WLog_DBG(TAG,  "=========== Usb Search List =========");
	self->rewind(self);
	while (self->has_next(self))
	{
		usb = self->get_next(self);
		WLog_DBG(TAG,  "  USB %d: ", num++);
		WLog_DBG(TAG,  "	idVendor: 0x%04X", usb->idVendor);
		WLog_DBG(TAG,  "	idProduct: 0x%04X", usb->idProduct);
	}

	WLog_DBG(TAG,  "================= END ===============");
}

static void searchman_free(USB_SEARCHMAN* self)
{
	USB_SEARCHDEV * dev;

	while (self->head != NULL)
	{
		dev = (USB_SEARCHDEV *)self->head;
		self->remove (self, dev->idVendor, dev->idProduct);
	}

	/* free searchman */
	CloseHandle(self->sem_term);
	CloseHandle(self->term_event);
	free(self);
}

USB_SEARCHMAN* searchman_new(void * urbdrc, UINT32 UsbDevice)
{
	USB_SEARCHMAN* searchman;

	searchman = (USB_SEARCHMAN*) calloc(1, sizeof(USB_SEARCHMAN));
	if (!searchman)
		return NULL;

	searchman->urbdrc = urbdrc;
	searchman->UsbDevice = UsbDevice;

	searchman->mutex = CreateMutex(NULL, FALSE, NULL);
	if (searchman->mutex == NULL)
	{
		WLog_ERR(TAG, "searchman mutex initialization: searchman->mutex failed");
		goto out_error_mutex;
	}

	/* load service */
	searchman->add = searchman_list_add;
	searchman->remove = searchman_list_remove;
	searchman->rewind = searchman_rewind;
	searchman->get_next = searchman_get_next;
	searchman->has_next = searchman_has_next;
	searchman->show = searchman_list_show;
	searchman->start = searchman_start;
	searchman->close = searchman_close;
	searchman->free = searchman_free;

	searchman->started = 0;
	searchman->term_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!searchman->term_event)
		goto out_error_event;

	searchman->sem_term = CreateSemaphoreA(NULL, 0, 0xFFFFFFFFL, NULL);
	if (!searchman->sem_term)
		goto out_error_sem;

	return searchman;

out_error_sem:
	CloseHandle(searchman->term_event);
out_error_event:
	CloseHandle(&searchman->mutex);
out_error_mutex:
	free(searchman);
	return NULL;
}
