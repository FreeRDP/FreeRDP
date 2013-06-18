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

#include "xf_event.h"

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

const char* xf_input_get_class_string(int class)
{
	if (class == XIKeyClass)
		return "XIKeyClass";
	else if (class == XIButtonClass)
		return "XIButtonClass";
	else if (class == XIValuatorClass)
		return "XIValuatorClass";
	else if (class == XIScrollClass)
		return "XIScrollClass";
	else if (class == XITouchClass)
		return "XITouchClass";

	return "XIUnknownClass";
}

int xf_input_init(xfContext* xfc, Window window)
{
	int i, j;
	int nmasks;
	int ndevices;
	int major = 2;
	int minor = 2;
	Status xstatus;
	XIDeviceInfo* info;
	XIEventMask evmasks[64];
	int opcode, event, error;
	BYTE masks[8][XIMaskLen(XI_LASTEVENT)];

	nmasks = 0;
	ndevices = 0;
	active_contacts = 0;
	ZeroMemory(contacts, sizeof(touchContact) * MAX_CONTACTS);

	if (!XQueryExtension(xfc->display, "XInputExtension", &opcode, &event, &error))
	{
		printf("XInput extension not available.\n");
		return -1;
	}

	xfc->XInputOpcode = opcode;

	XIQueryVersion(xfc->display, &major, &minor);

	if (major * 1000 + minor < 2002)
	{
		printf("Server does not support XI 2.2\n");
		return -1;
	}

	if (xfc->settings->MultiTouchInput)
		xfc->use_xinput = TRUE;

	info = XIQueryDevice(xfc->display, XIAllDevices, &ndevices);

	for (i = 0; i < ndevices; i++)
	{
		BOOL touch = FALSE;
		XIDeviceInfo* dev = &info[i];

		for (j = 0; j < dev->num_classes; j++)
		{
			XIAnyClassInfo* class = dev->classes[j];
			XITouchClassInfo* t = (XITouchClassInfo*) class;

			if ((class->type == XITouchClass) && (t->mode == XIDirectTouch) &&
				(strcmp(dev->name, "Virtual core pointer") != 0))
			{
				touch = TRUE;
			}
		}

		for (j = 0; j < dev->num_classes; j++)
		{
			XIAnyClassInfo* class = dev->classes[j];
			XITouchClassInfo* t = (XITouchClassInfo*) class;

			if (xfc->settings->MultiTouchInput)
			{
				printf("%s (%d) \"%s\" id: %d\n",
						xf_input_get_class_string(class->type),
						class->type, dev->name, dev->deviceid);
			}

			evmasks[nmasks].mask = masks[nmasks];
			evmasks[nmasks].mask_len = sizeof(masks[0]);
			ZeroMemory(masks[nmasks], sizeof(masks[0]));
			evmasks[nmasks].deviceid = dev->deviceid;

			if ((class->type == XITouchClass) && (t->mode == XIDirectTouch) &&
					(strcmp(dev->name, "Virtual core pointer") != 0))
			{
				if (xfc->settings->MultiTouchInput)
				{
					printf("%s %s touch device (id: %d, mode: %d), supporting %d touches.\n",
						dev->name, (t->mode == XIDirectTouch) ? "direct" : "dependent",
						dev->deviceid, t->mode, t->num_touches);
				}

				XISetMask(masks[nmasks], XI_TouchBegin);
				XISetMask(masks[nmasks], XI_TouchUpdate);
				XISetMask(masks[nmasks], XI_TouchEnd);
				nmasks++;
			}

			if (xfc->use_xinput)
			{
				if (!touch && (class->type == XIButtonClass))
				{
					XISetMask(masks[nmasks], XI_ButtonPress);
					XISetMask(masks[nmasks], XI_ButtonRelease);
					XISetMask(masks[nmasks], XI_Motion);
					nmasks++;
				}
			}
		}
	}

	if (nmasks > 0)
		xstatus = XISelectEvents(xfc->display, window, evmasks, nmasks);

	return 0;
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

void xf_input_detect_pinch(xfContext* xfc)
{
	double dist;
	double zoom;
	double delta;
	ResizeWindowEventArgs e;

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
			xfc->scale -= 0.05;

			if (xfc->scale < 0.5)
				xfc->scale = 0.5;

			XResizeWindow(xfc->display, xfc->window->handle, xfc->originalWidth * xfc->scale, xfc->originalHeight * xfc->scale);

			EventArgsInit(&e, "xfreerdp");
			e.width = (int) xfc->originalWidth * xfc->scale;
			e.height = (int) xfc->originalHeight * xfc->scale;
			PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);

			z_vector = 0;
		}

		if (z_vector < -10)
		{
			xfc->scale += 0.05;

			if (xfc->scale > 1.5)
				xfc->scale = 1.5;

			XResizeWindow(xfc->display, xfc->window->handle, xfc->originalWidth * xfc->scale, xfc->originalHeight * xfc->scale);

			EventArgsInit(&e, "xfreerdp");
			e.width = (int) xfc->originalWidth * xfc->scale;
			e.height = (int) xfc->originalHeight * xfc->scale;
			PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);

			z_vector = 0;
		}
	}
}

void xf_input_touch_begin(xfContext* xfc, XIDeviceEvent* event)
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

void xf_input_touch_update(xfContext* xfc, XIDeviceEvent* event)
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

			xf_input_detect_pinch(xfc);

			break;
		}
	}
}

void xf_input_touch_end(xfContext* xfc, XIDeviceEvent* event)
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
			break;printf("TouchBegin\n");
		}
	}
}

int xf_input_handle_event_local(xfContext* xfc, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;

	XGetEventData(xfc->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfc->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_begin(xfc, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			case XI_TouchUpdate:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_update(xfc, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			case XI_TouchEnd:
				if (xf_input_is_duplicate(cookie->data) == FALSE)
					xf_input_touch_end(xfc, cookie->data);
				xf_input_save_last_event(cookie->data);
				break;

			default:
				printf("unhandled xi type= %d\n", cookie->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display,cookie);

	return 0;
}

char* xf_input_touch_state_string(DWORD flags)
{
	if (flags & CONTACT_FLAG_DOWN)
		return "TouchBegin";
	else if (flags & CONTACT_FLAG_UPDATE)
		return "TouchUpdate";
	else if (flags & CONTACT_FLAG_UP)
		return "TouchEnd";
	else
		return "TouchUnknown";
}

int xf_input_touch_remote(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	int x, y;
	int touchId;
	int contactId;
	RdpeiClientContext* rdpei = xfc->rdpei;

	if (!rdpei)
		return 0;

	touchId = event->detail;
	x = (int) event->event_x;
	y = (int) event->event_y;

	if (evtype == XI_TouchBegin)
	{
		//printf("TouchBegin: %d\n", touchId);
		contactId = rdpei->TouchBegin(rdpei, touchId, x, y);
	}
	else if (evtype == XI_TouchUpdate)
	{
		//printf("TouchUpdate: %d\n", touchId);
		contactId = rdpei->TouchUpdate(rdpei, touchId, x, y);
	}
	else if (evtype == XI_TouchEnd)
	{
		//printf("TouchEnd: %d\n", touchId);
		contactId = rdpei->TouchEnd(rdpei, touchId, x, y);
	}

	return 0;
}

int xf_input_event(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	return TRUE;

	switch (evtype)
	{
		case XI_ButtonPress:
			printf("ButtonPress\n");
			xf_generic_ButtonPress(xfc, (int) event->event_x, (int) event->event_y,
					event->detail, event->event, xfc->remote_app);
			break;

		case XI_ButtonRelease:
			printf("ButtonRelease\n");
			xf_generic_ButtonRelease(xfc, (int) event->event_x, (int) event->event_y,
					event->detail, event->event, xfc->remote_app);
			break;

		case XI_Motion:
			printf("Motion\n");
			xf_generic_MotionNotify(xfc, (int) event->event_x, (int) event->event_y,
					event->detail, event->event, xfc->remote_app);
			break;
	}

	return 0;
}

int xf_input_handle_event_remote(xfContext* xfc, XEvent* event)
{
	XGenericEventCookie* cookie = &event->xcookie;

	XGetEventData(xfc->display, cookie);

	if ((cookie->type == GenericEvent) && (cookie->extension == xfc->XInputOpcode))
	{
		switch (cookie->evtype)
		{
			case XI_TouchBegin:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchBegin);
				break;

			case XI_TouchUpdate:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchUpdate);
				break;

			case XI_TouchEnd:
				xf_input_touch_remote(xfc, cookie->data, XI_TouchEnd);
				break;

			default:
				xf_input_event(xfc, cookie->data, cookie->evtype);
				break;
		}
	}

	XFreeEventData(xfc->display,cookie);

	return 0;
}

#else

int xf_input_init(xfContext* xfc, Window window)
{
	return 0;
}

#endif

void xf_process_rdpei_event(xfContext* xfc, wMessage* event)
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

int xf_input_handle_event(xfContext* xfc, XEvent* event)
{
#ifdef WITH_XI
	if (xfc->settings->MultiTouchInput)
	{
		return xf_input_handle_event_remote(xfc, event);
	}

	if (xfc->enableScaling)
		return xf_input_handle_event_local(xfc, event);
#endif

	return 0;
}
