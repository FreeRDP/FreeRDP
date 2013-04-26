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

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include "xf_input.h"

int xinput_opcode; //TODO: use this instead of xfi

void xf_input_init(xfInfo* xfi)
{
	int opcode, event, error;
	int major = 2, minor = 2;


	XIEventMask eventmask;
	unsigned char mask[(XI_LASTEVENT + 7)/8];

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

			error = XISelectEvents(xfi->display, xfi->window->handle, &eventmask, 1);

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
}

void xf_input_handle_event(xfInfo* xfi, XEvent* event)
{
	//handle touch events
	XGenericEventCookie* cookie = &event->xcookie;
	XIDeviceEvent* devEvent;

	XGetEventData(xfi->display, cookie);

	if (	(cookie->type == GenericEvent) &&
			(cookie->extension == xfi->XInputOpcode) )
		{
			switch(cookie->evtype)
			{
				case XI_TouchBegin:
				case XI_TouchUpdate:
				case XI_TouchEnd:
					devEvent = cookie->data;
					printf("\tTouch (%d - dev:%d - src:%d)  [%f,%f]\n",
							cookie->evtype,
							devEvent->deviceid,
							devEvent->sourceid,
							devEvent->event_x,
							devEvent->event_y);
					//do_something(ev.xcookie.data);
					break;

				default:
					printf("unhandled xi type= %d\n", cookie->evtype);
					break;
			}
		}


	XFreeEventData(xfi->display,cookie);
}
