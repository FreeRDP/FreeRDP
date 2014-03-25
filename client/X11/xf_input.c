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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include <math.h>

#include "xf_event.h"
#include "xf_input.h"

#ifdef WITH_XI

#define MAX_CONTACTS 2

#define PAN_THRESHOLD 50
#define ZOOM_THRESHOLD 10

#define MIN_FINGER_DIST 5

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
int lastEvType;
XIDeviceEvent lastEvent;
double firstDist = -1.0;
double lastDist;

double z_vector;
double px_vector;
double py_vector;

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
	
	z_vector = 0;
	px_vector = 0;
	py_vector = 0;
	
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
				if (!touch && (class->type == XIButtonClass) && strcmp(dev->name, "Virtual core pointer"))
				{
					printf("%s button device (id: %d, mode: %d)\n",
					       dev->name,
					       dev->deviceid, t->mode);
					XISetMask(masks[nmasks], XI_ButtonPress);
					XISetMask(masks[nmasks], XI_ButtonRelease);
					XISetMask(masks[nmasks], XI_Motion);
					nmasks++;
				}
			}
		}
	}
	XIFreeDeviceInfo(info);

	if (nmasks > 0)
		xstatus = XISelectEvents(xfc->display, window, evmasks, nmasks);
	
	return 0;
}

BOOL xf_input_is_duplicate(XGenericEventCookie* cookie)
{
	XIDeviceEvent* event;
	
	event = cookie->data;
	
	
	if ( (lastEvent.time == event->time) &&
	    (lastEvType == cookie->evtype) &&
	    (lastEvent.detail == event->detail) &&
	    (lastEvent.event_x == event->event_x) &&
	    (lastEvent.event_y == event->event_y) )
	{
		return TRUE;
	}
	
	return FALSE;
}

void xf_input_save_last_event(XGenericEventCookie* cookie)
{
	XIDeviceEvent* event;
	
	event = cookie->data;
	
	lastEvType = cookie->evtype;
	
	lastEvent.time = event->time;
	lastEvent.detail = event->detail;
	lastEvent.event_x = event->event_x;
	lastEvent.event_y = event->event_y;
}

void xf_input_detect_pan(xfContext* xfc)
{
	double dx[2];
	double dy[2];
	
	double px;
	double py;
	
	double dist_x;
	double dist_y;
	
	if (active_contacts != 2)
	{
		return;
	}
	
	dx[0] = contacts[0].pos_x - contacts[0].last_x;
	dx[1] = contacts[1].pos_x - contacts[1].last_x;
	
	dy[0] = contacts[0].pos_y - contacts[0].last_y;
	dy[1] = contacts[1].pos_y - contacts[1].last_y;
	
	px = fabs(dx[0]) < fabs(dx[1]) ? dx[0] : dx[1];
	py = fabs(dy[0]) < fabs(dy[1]) ? dy[0] : dy[1];
	
	px_vector += px;
	py_vector += py;
	
	dist_x = fabs(contacts[0].pos_x - contacts[1].pos_x);
	dist_y = fabs(contacts[0].pos_y - contacts[1].pos_y);

	if (dist_y > MIN_FINGER_DIST)
	{
		if (px_vector > PAN_THRESHOLD)
		{
			
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 5;
				e.YPan = 0;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			px_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
		else if (px_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = -5;
				e.YPan = 0;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			px_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
		
	}
	
	if (dist_x > MIN_FINGER_DIST)
	{
		
		if (py_vector > PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 0;
				e.YPan = 5;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			py_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
		else if (py_vector < -PAN_THRESHOLD)
		{
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 0;
				e.YPan = -5;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			py_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
	}
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
		
		px_vector = 0;
		py_vector = 0;
		z_vector = 0;
	}
	else
	{
		delta = lastDist - dist;
		
		if(delta > 1.0)
			delta = 1.0;
		if(delta < -1.0)
			delta = -1.0;
		
		/* compare the current distance to the first one */
		zoom = (dist / firstDist);
		
		z_vector += delta;
		
		lastDist = dist;
		
		if (z_vector > ZOOM_THRESHOLD)
		{
			xfc->settings->ScalingFactor -= 0.05;
			
			if (xfc->settings->ScalingFactor < 0.8)
				xfc->settings->ScalingFactor = 0.8;
			
			EventArgsInit(&e, "xfreerdp");
			e.width = (int) xfc->originalWidth * xfc->settings->ScalingFactor;
			e.height = (int) xfc->originalHeight * xfc->settings->ScalingFactor;
			
			xf_transform_window(xfc);
			PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);
			xf_draw_screen_scaled(xfc, 0, 0, 0, 0, FALSE);
			
			z_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
		
		if (z_vector < -ZOOM_THRESHOLD)
		{
			xfc->settings->ScalingFactor += 0.05;
			
			if (xfc->settings->ScalingFactor > 1.2)
				xfc->settings->ScalingFactor = 1.2;
			
			EventArgsInit(&e, "xfreerdp");
			e.width = (int) xfc->originalWidth * xfc->settings->ScalingFactor;
			e.height = (int) xfc->originalHeight * xfc->settings->ScalingFactor;
			
			xf_transform_window(xfc);
			PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);
			xf_draw_screen_scaled(xfc, 0, 0, 0, 0, FALSE);
			
			z_vector = 0;
			
			px_vector = 0;
			py_vector = 0;
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
			xf_input_detect_pan(xfc);
			
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
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_begin(xfc, cookie->data);
				xf_input_save_last_event(cookie);
				break;
				
			case XI_TouchUpdate:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_update(xfc, cookie->data);
				xf_input_save_last_event(cookie);
				break;
				
			case XI_TouchEnd:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_end(xfc, cookie->data);
				xf_input_save_last_event(cookie);
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

void xf_input_hide_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR
	if (!xfc->cursorHidden)
	{
		XcursorImage ci;
		XcursorPixel xp = 0;
		static Cursor nullcursor = None;

		xf_lock_x11(xfc, FALSE);

		ZeroMemory(&ci, sizeof(ci));
		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = ci.height = 1;
		ci.xhot = ci.yhot = 0;
		ci.pixels = &xp;
		nullcursor = XcursorImageLoadCursor(xfc->display, &ci);

		if ((xfc->window) && (nullcursor != None))
			XDefineCursor(xfc->display, xfc->window->handle, nullcursor);

		xfc->cursorHidden = TRUE;

		xf_unlock_x11(xfc, FALSE);
	}
#endif
}

void xf_input_show_cursor(xfContext* xfc)
{
#ifdef WITH_XCURSOR
	xf_lock_x11(xfc, FALSE);

	if (xfc->cursorHidden)
	{
		if (xfc->window)
		{
			if (!xfc->pointer)
				XUndefineCursor(xfc->display, xfc->window->handle);
			else
				XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);
		}

		xfc->cursorHidden = FALSE;
	}

	xf_unlock_x11(xfc, FALSE);
#endif
}

int xf_input_touch_remote(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	int x, y;
	int touchId;
	int contactId;
	RdpeiClientContext* rdpei = xfc->rdpei;
	
	if (!rdpei)
		return 0;
	
	xf_input_hide_cursor(xfc);

	touchId = event->detail;
	x = (int) event->event_x;
	y = (int) event->event_y;
	
	if (evtype == XI_TouchBegin)
	{
		printf("TouchBegin: %d\n", touchId);
		contactId = rdpei->TouchBegin(rdpei, touchId, x, y);
	}
	else if (evtype == XI_TouchUpdate)
	{
	        printf("TouchUpdate: %d\n", touchId);
		contactId = rdpei->TouchUpdate(rdpei, touchId, x, y);
	}
	else if (evtype == XI_TouchEnd)
	{
		printf("TouchEnd: %d\n", touchId);
		contactId = rdpei->TouchEnd(rdpei, touchId, x, y);
	}
	
	return 0;
}

int xf_input_event(xfContext* xfc, XIDeviceEvent* event, int evtype)
{
	xf_input_show_cursor(xfc);

	switch (evtype)
	{
		case XI_ButtonPress:
			
			xf_generic_ButtonPress(xfc, (int) event->event_x, (int) event->event_y,
					       event->detail, event->event, xfc->remote_app);
			break;
			
		case XI_ButtonRelease:
			
			xf_generic_ButtonRelease(xfc, (int) event->event_x, (int) event->event_y,
						 event->detail, event->event, xfc->remote_app);
			break;
			
		case XI_Motion:
			
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
	
	if (xfc->settings->MultiTouchGestures)
	{
		return xf_input_handle_event_local(xfc, event);
	}
#endif
	
	return 0;
}
