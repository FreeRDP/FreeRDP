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

int xinput_opcode; //TODO: use this instead of xfi

int scale_cnt;

BOOL xf_input_is_duplicate(XIDeviceEvent* event)
{
	if( (lastEvent.time == event->time) &&
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
	//only save what we care about

	lastEvent.time = event->time;
	lastEvent.detail = event->detail;
	lastEvent.event_x = event->event_x;
	lastEvent.event_y = event->event_y;

	return;
}

void xf_input_detect_pinch(xfInfo* xfi)
{
	double dist;
	double zoom;
    double delta;

	if(active_contacts != 2)
	{
		firstDist = -1.0;
		return;
	}

	//first calculate the distance
	dist = sqrt(pow(contacts[1].pos_x - contacts[0].last_x, 2.0) +
			pow(contacts[1].pos_y - contacts[0].last_y, 2.0));


	//if this is the first 2pt touch
	if(firstDist <= 0)
	{
		firstDist = dist;
        lastDist = firstDist;
        scale_cnt = 0;
        z_vector = 0;
	}
	else
	{
        delta = lastDist - dist;

		//compare the current distance to the first one
        zoom = (dist / firstDist);

        z_vector += delta;
        printf("d: %.2f\n", delta);

        lastDist = dist;

        if(z_vector > 10)
        {
            //zoom in a level
            xfi->scale -= 0.05;

            if(xfi->scale < 0.5)
                xfi->scale = 0.5;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);

            z_vector = 0;
        }

        if(z_vector < -10)
        {
            //zoom in a level
            xfi->scale += 0.05;

            if(xfi->scale > 1.5)
                xfi->scale = 1.5;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);

            z_vector = 0;
        }

        /*printf("zoom: %.2f, cnt:%d\n", zoom, scale_cnt);
        if(zoom < 0.75 && scale_cnt == 0)
        {
            //zoom out one level
            xfi->scale -= 0.05;
            scale_cnt++;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            printf("-5 ");
        }
        else if(zoom < 0.5 && scale_cnt == 1)
        {
            //zoom out an other level
            xfi->scale -= 0.05;
            scale_cnt++;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            printf("-5 ");
        }


        if(zoom > 1.25 && scale_cnt == 0)
        {
            //zoom out one level
            xfi->scale += 0.05;
            scale_cnt++;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            printf("+5 ");
        }
        else if(zoom < 1.5 && scale_cnt == 1)
        {
            //zoom out an other level
            xfi->scale += 0.05;
            scale_cnt++;

            XResizeWindow(xfi->display, xfi->window->handle, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            IFCALL(xfi->client->OnResizeWindow, xfi->instance, xfi->orig_width * xfi->scale, xfi->orig_height * xfi->scale);
            printf("+5 ");
        }
        */
	}

}

#endif

void xf_input_init(xfInfo* xfi, Window win)
{
#ifdef WITH_XI
	int opcode, event, error;
	int major = 2, minor = 2;


	XIEventMask eventmask;
	unsigned char mask[(XI_LASTEVENT + 7)/8];

	active_contacts = 0;
	ZeroMemory(contacts, sizeof(touchContact) * MAX_CONTACTS);

	ZeroMemory(mask, sizeof(mask));


	if (XQueryExtension(xfi->display, "XInputExtension", &opcode, &event, &error))
	{

		xfi->XInputOpcode = opcode;

		XIQueryVersion(xfi->display, &major, &minor);
		if (!(major * 1000 + minor < 2002))
		{

			eventmask.deviceid = XIAllDevices;
			eventmask.mask_len = sizeof(mask);
			eventmask.mask = mask;

			XISetMask(mask, XI_TouchBegin);
			XISetMask(mask, XI_TouchUpdate);
			XISetMask(mask, XI_TouchEnd);


            error = XISelectEvents(xfi->display, win, &eventmask, 1);

			switch (error)
			{
			case BadValue:
				printf("\tBadValue\n");
				break;
			case BadWindow:
				printf("\tBadWindow\n");
				break;

			case 0: //no error
				break;

			default:
				printf("XISelectEvents() returned %d\n", error);
				break;

			}

		}
		else
		{
			printf("Server does not support XI 2.2\n");
		}
	}
	else
	{
		printf("X Input extension not available!!!\n");
	}
#endif
}

void xf_input_handle_event(xfInfo* xfi, XEvent* event)
{
#ifdef WITH_XI
	//handle touch events
	XGenericEventCookie* cookie = &event->xcookie;

	XGetEventData(xfi->display, cookie);

	if (	(cookie->type == GenericEvent) &&
			(cookie->extension == xfi->XInputOpcode) )
		{


			switch(cookie->evtype)
			{
				case XI_TouchBegin:
					if(xf_input_is_duplicate(cookie->data) == FALSE)
						xf_input_touch_begin(xfi, cookie->data);
					xf_input_save_last_event(cookie->data);
					break;

				case XI_TouchUpdate:
					if(xf_input_is_duplicate(cookie->data) == FALSE)
						xf_input_touch_update(xfi, cookie->data);
					xf_input_save_last_event(cookie->data);
					break;

				case XI_TouchEnd:
					if(xf_input_is_duplicate(cookie->data) == FALSE)
						xf_input_touch_end(xfi, cookie->data);
					xf_input_save_last_event(cookie->data);
					break;

				default:
					printf("unhandled xi type= %d\n", cookie->evtype);
					break;
			}


		}


	XFreeEventData(xfi->display,cookie);
#endif
}

#ifdef WITH_XI
void xf_input_touch_begin(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;


	//find an empty slot and save it
	for(i=0; i<MAX_CONTACTS; i++)
	{
		if(contacts[i].id == 0)
		{
			contacts[i].id = event->detail;
			contacts[i].count = 1;
			contacts[i].pos_x = event->event_x;
			contacts[i].pos_y = event->event_y;

			active_contacts++;
			break;
		}
	}


	////
	/*
	printf("\tTouchBegin (%d)  [%.2f,%.2f]\n",
								event->detail,
								event->event_x,
								event->event_y);

	printf("state: a=%d time=%lu\n", active_contacts, event->time);
	printf("c0=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[0].id, contacts[0].count,contacts[0].pos_x, contacts[0].pos_y, contacts[0].last_x, contacts[0].last_y );
	printf("c1=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[1].id, contacts[1].count,contacts[1].pos_x, contacts[1].pos_y, contacts[1].last_x, contacts[1].last_y );

	*/

}
void xf_input_touch_update(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;


	for(i=0; i<MAX_CONTACTS; i++)
		{
			if(contacts[i].id == event->detail)
			{
				contacts[i].count++;
				contacts[i].last_x = contacts[i].pos_x;
				contacts[i].last_y = contacts[i].pos_y;
				contacts[i].pos_x = event->event_x;
				contacts[i].pos_y = event->event_y;

				//detect pinch-zoom
                xf_input_detect_pinch(xfi);

				break;
			}
		}

	/*
	printf("\tTouchUpdate (%d)  [%.2f,%.2f]\n",
								event->detail,
								event->event_x,
								event->event_y);

	printf("state: a=%d time=%lu\n", active_contacts, event->time);
	printf("c0=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[0].id, contacts[0].count,contacts[0].pos_x, contacts[0].pos_y, contacts[0].last_x, contacts[0].last_y );
	printf("c1=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[1].id, contacts[1].count,contacts[1].pos_x, contacts[1].pos_y, contacts[1].last_x, contacts[1].last_y );
	*/

}
void xf_input_touch_end(xfInfo* xfi, XIDeviceEvent* event)
{
	int i;

	//find our and delete the point
	for(i=0; i<MAX_CONTACTS; i++)
		{
			if(contacts[i].id == event->detail)
			{
				contacts[i].id = 0;
				contacts[i].count = 0;
				//contacts[i].pos_x = (int)event->event_x;
				//contacts[i].pos_y = (int)event->event_y;

				active_contacts--;
				break;
			}
		}



	/*
	printf("\tTouchEnd (%d)  [%.2f,%.2f]\n",
								event->detail,
								event->event_x,
								event->event_y);

	printf("state: a=%d time=%lu\n", active_contacts, event->time);
	printf("c0=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[0].id, contacts[0].count,contacts[0].pos_x, contacts[0].pos_y, contacts[0].last_x, contacts[0].last_y );
	printf("c1=[%d, %d, %.2f, %.2f, %.2f, %.2f]\n", contacts[1].id, contacts[1].count,contacts[1].pos_x, contacts[1].pos_y, contacts[1].last_x, contacts[1].last_y );
	*/
}

#endif
