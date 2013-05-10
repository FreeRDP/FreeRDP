/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Input
 *
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include <math.h>

#include "xf_input.h"

#ifdef WITH_XI

#define MAX_CONTACTS 2

typedef struct touch_contact
{
	int id;
	int count;
	double pos_x;
	double pos_y;
	double last_x;
	double last_y;

} touchContact;

touchContact contacts[MAX_CONTACTS];

int active_contacts;
XIDeviceEvent lastEvent;
double firstDist = -1.0;
double lastDist;
double z_vector;
int xinput_opcode;
int scale_cnt;

int xf_input_init(xfInfo* xfi, Window window)
{
	int i, j;
	int ndevices;
	int major = 2;
	int minor = 2;
	Status xstatus;
	XIEventMask evmask;
	XIDeviceInfo* info;
	int opcode, event, error;
	unsigned char mask[XIMaskLen(XI_LASTEVENT)];

	active_contacts = 0;
	ZeroMemory(contacts, sizeof(touchContact) * MAX_CONTACTS);

	if (!XQueryExtension(xfi->display, "XInputExtension", &opcode, &event, &error))
	{
		printf("XInput extension not available.\n");
		return -1;
	}

	xfi->XInputOpcode = opcode;

	XIQueryVersion(xfi->display, &major, &minor);

	if (major * 1000 + minor < 2002)
	{
		printf("Server does not support XI 2.2\n");
		return -1;
	}

	info = XIQueryDevice(xfi->display, XIAllDevices, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
		XIDeviceInfo* dev = &info[i];

		for (j = 0; j < dev->num_classes; j++)
		{
			XIAnyClassInfo* class = dev->classes[j];
			//XITouchClassInfo* t = (XITouchClassInfo*) class;

			if (class->type != XITouchClass)
				continue;

			//printf("%s %s touch device, supporting %d touches.\n",
			//	dev->name, (t->mode == XIDirectTouch) ? "direct" : "dependent", t->num_touches);
		}
	}

	evmask.mask = mask;
	evmask.mask_len = sizeof(mask);
	ZeroMemory(mask, sizeof(mask));
	evmask.deviceid = XIAllDevices;

	XISetMask(mask, XI_TouchBegin);
	XISetMask(mask, XI_TouchUpdate);
	XISetMask(mask, XI_TouchEnd);

	xstatus = XISelectEvents(xfi->display, window, &evmask, 1);

	return -1;
}

BOOL xf_input_is_duplicate(XIDeviceEvent* event)
{
	if ( (lastEvent.time == event->time) &&
			(lastEvent.detail == event->detail) &&
			(lastEvent.event_x == event->event_x) &&
			(lastEvent.event_y == event->event_y) )
	{
		return TRUE;
	}

	return FALSE;
}

void xf_input_save_last_event(XIDeviceEvent* event)
{
	lastEvent.time = event->time;
	lastEvent.detail = event->detail;
	lastEvent.event_x = event->event_x;
	lastEvent.event_y = event->event_y;
}

void xf_input_detect_pinch(xfInfo* xfi)
{
	double dist;
	double zoom;
	double delta;

	if (active_contacts != 2)
	{
		firstDist = -1.0;
		return;
	}

	/* first calculate the distance */
	dist = sqrt(pow(contacts[1].pos_x - contacts[0].last_x, 2.0) +
			pow(contacts[1].pos_y - contacts[0].last_y, 2.0));

	/* if this is the first 2pt touch */
	if (firstDist <= 0)
	{
		firstDist = dist;
		lastDist = firstDist;
		scale_cnt = 0;
		z_vector = 0;
	}
	else
	{
		delta = lastDist - dist;

		/* compare the current distance to the first one */
		zoom = (dist / firstDist);

		z_vector += delta;
		//printf("d: %.2f\n", delta);

		lastDist = dist;

		if (z_vector > 10)
		{
			xfi->scale -= 0.05;

			if (xfi->scale < 0.5)
				xfi->scale = 0.5;

			XResizeWindow(xfi->display, xfi->window->handle, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);
			IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);

			z_vector = 0;
		}

		if (z_vector < -10)
		{
			xfi->scale += 0.05;

			if (xfi->scale > 1.5)
				xfi->scale = 1.5;

			XResizeWindow(xfi->display, xfi->window->handle, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);
			IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);

			z_vector = 0;
		}
	}
}

void xf_input_touch_begin(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == 0)
		{
			contacts[i].id = event->detail;
			contacts[i].count = 1;
			contacts[i].pos_x = event->event_x;
			contacts[i].pos_y = event->event_y;

			active_contacts++;
			break;
		}
	}
}

void xf_input_touch_update(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == event->detail)
		{
			contacts[i].count++;
			contacts[i].last_x = contacts[i].pos_x;
			contacts[i].last_y = contacts[i].pos_y;
			contacts[i].pos_x = event->event_x;
			contacts[i].pos_y = event->event_y;

			xf_input_detect_pinch(xfi);

			break;
		}
	}
}

void xf_input_touch_end(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;

	for (i = 0; i < MAX_CONTACTS; i++)
	{
		if (contacts[i].id == event->detail)
		{
			contacts[i].id = 0;
			contacts[i].count = 0;
			//contacts[i].pos_x = (int)event->event_x;
			//contacts[i].pos_y = (int)event->event_y;

			active_contacts--;
			break;
		}
	}
}

int xf_input_handle_event_local(xfInfo* xfi, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;

	XGetEventData(xfi->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfi->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_begin(xfi, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			case XI_TouchUpdate:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_update(xfi, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			case XI_TouchEnd:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_end(xfi, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			default:
				printf("unhandled xi type= %d\n", cookie->evtype);
				break;
		}
	}

	XFreeEventData(xfi->display,cookie);

	return 0;
}

int xf_input_touch_begin_remote(xfInfo* xfi, XIDeviceEvent* event)
{
	return 0;
}

int xf_input_touch_update_remote(xfInfo* xfi, XIDeviceEvent* event)
{
	return 0;
}

int xf_input_touch_end_remote(xfInfo* xfi, XIDeviceEvent* event)
{
	return 0;
}

int xf_input_handle_event_remote(xfInfo* xfi, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;

	XGetEventData(xfi->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfi->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				xf_input_touch_begin_remote(xfi, cookie->data);
				break;

			case XI_TouchUpdate:
				xf_input_touch_update_remote(xfi, cookie->data);
				break;

			case XI_TouchEnd:
				xf_input_touch_end_remote(xfi, cookie->data);
				break;

			default:
				break;
		}
	}

	XFreeEventData(xfi->display,cookie);

	return 0;
}

#else

int xf_input_init(xfInfo* xfi, Window window)
{
	return 0;
}

#endif

void xf_process_rdpei_event(xfInfo* xfi, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case RdpeiChannel_ServerReady:
			break;

		case RdpeiChannel_SuspendTouch:
			break;

		case RdpeiChannel_ResumeTouch:
			break;
	}
}

int xf_input_handle_event(xfInfo* xfi, XEvent* event)
{
#ifdef WITH_XI
	if (xfi->settings->MultiTouchInput)
	{
		return xf_input_handle_event_remote(xfi, event);
	}

	if (xfi->enableScaling)
		return xf_input_handle_event_local(xfi, event);
#endif

	return 0;
}
