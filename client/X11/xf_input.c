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

#define PAN_THRESHOLD 50
#define ZOOM_THRESHOLD 10

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

	z_vector = 0;
	px_vector = 0;
	py_vector = 0;

	return -1;
}

//BOOL xf_input_is_duplicate(XIDeviceEvent* event)
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

//void xf_input_save_last_event(XIDeviceEvent* event)
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

void xf_input_detect_pan(xfInfo* xfi)
{
  double dx[2];
  double dy[2];

  double px;
  double py;

  double dist_x;
  double dist_y;

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
  

  //only pan in x if dist_y is greater than something
  if(dist_y > 30)
    {
      
      if(px_vector > PAN_THRESHOLD)
	{
	  xfi->offset_x += 5;
	  
	  if(xfi->offset_x > 0)
	    xfi->offset_x = 0;
	  
	  xf_transform_window(xfi);
	  
	  xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);
	  
	  px_vector = 0;
	  
	  px_vector = 0;
	  py_vector = 0;
	  z_vector = 0;
	}
      else if(px_vector < -PAN_THRESHOLD)
	{
	  xfi->offset_x -= 5;
	  
	  xf_transform_window(xfi);
	  
	  xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);
	  
	  px_vector = 0;
	  
	  px_vector = 0;
	  py_vector = 0;
	  z_vector = 0;
	}
      
    }
  
  if(dist_x > 30)
    {
      
      if(py_vector > PAN_THRESHOLD)
	{
	  xfi->offset_y += 5;
	  
	  if(xfi->offset_y > 0)
	    xfi->offset_y = 0;
	  
	  xf_transform_window(xfi);
	  
	  xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);
	  
	  py_vector = 0;
	  
	  px_vector = 0;
	  py_vector = 0;
	  z_vector = 0;
	}
      else if(py_vector < -PAN_THRESHOLD)
	{
	  xfi->offset_y -= 5;
	  
	  xf_transform_window(xfi);
	  
	  xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);
	  
	  py_vector = 0;
	  
	  px_vector = 0;
	  py_vector = 0;
	  z_vector = 0;
	}
    }
  
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

		//	printf("%.2f = %.2f - %.2f\t(%.2f)\n", delta, lastDist, dist, z_vector);

		lastDist = dist;

		if (z_vector > ZOOM_THRESHOLD)
		{
			xfi->scale -= 0.05;

			if (xfi->scale < 0.8)
				xfi->scale = 0.8;

			xfi->currentWidth = xfi->originalWidth * xfi->scale;
			xfi->currentHeight = xfi->originalHeight * xfi->scale;

			xf_transform_window(xfi);
			IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);
			xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);

			z_vector = 0;

			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}

		if (z_vector < -ZOOM_THRESHOLD)
		{
			xfi->scale += 0.05;

			if (xfi->scale > 1.2)
				xfi->scale = 1.2;

			xfi->currentWidth = xfi->originalWidth * xfi->scale;
			xfi->currentHeight = xfi->originalHeight * xfi->scale;

			xf_transform_window(xfi);
			IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->originalWidth * xfi->scale, xfi->originalHeight * xfi->scale);
			xf_draw_screen_scaled(xfi, 0, 0, 0, 0, FALSE);

			z_vector = 0;

			px_vector = 0;
			py_vector = 0;
			z_vector = 0;
		}
	}
}

void xf_input_touch_begin(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;
	/*
	printf("id: %d active: %d x:%.2f y:%.2f BEGIN\n",
	       event->detail,
	       active_contacts,
	       event->event_x,
	       event->event_y
	       );
	*/
	if(active_contacts == MAX_CONTACTS)
	  printf("Houston, we have a problem!\n\n");

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
	/*
	printf("id: %d active: %d x:%.2f y:%.2f UPDATE\n",
	       event->detail,
	       active_contacts,
	       event->event_x,
	       event->event_y
	       );
	*/
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
			xf_input_detect_pan(xfi);

			break;
		}
	}
}

void xf_input_touch_end(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;

	/*	printf("id: %d active: %d x:%.2f y:%.2f END\n",
	       event->detail,
	       active_contacts,
	       event->event_x,
	       event->event_y
	       );
	*/
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
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_begin(xfi, cookie->data);
				xf_input_save_last_event(cookie);
				break;

			case XI_TouchUpdate:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_update(xfi, cookie->data);
				xf_input_save_last_event(cookie);
				break;

			case XI_TouchEnd:
				if (xf_input_is_duplicate(cookie) == FALSE)
					xf_input_touch_end(xfi, cookie->data);
				xf_input_save_last_event(cookie);
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

	if (1)
		return xf_input_handle_event_local(xfi, event);
#endif

	return 0;
}
