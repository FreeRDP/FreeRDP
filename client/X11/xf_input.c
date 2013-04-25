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

void xf_input_init(rdpContext* context)
{
	xfInfo*			xfi;
	xfi = ((xfContext*) context)->xfi;

	int opcode, event, error;
	int major = 2, minor = 2;

	XIDeviceInfo *info;
	int ndevices, i, j;

	XIEventMask eventmask;
	unsigned char mask[(XI_LASTEVENT + 7)/8];

	ZeroMemory(mask, sizeof(mask));


	if (XQueryExtension(xfi->display, "XInputExtension", &opcode, &event, &error))
	{
		printf("X Input extension available.\n");

		xfi->XInputOpcode = opcode;
		printf("Input Opcode = %d\n", opcode);

		XIQueryVersion(xfi->display, &major, &minor);
		if (!(major * 1000 + minor < 2002))
		{
			printf("XI 2.2 supported\n");



			info = XIQueryDevice(xfi->display, XIAllDevices, &ndevices);

			for (i = 0; i < ndevices; i++)
			{
				XIDeviceInfo *dev = &info[i];
				printf("Device name [id] %s [%d]\n", dev->name, dev->deviceid);
				for (j = 0; j < dev->num_classes; j++)
				{
					XIAnyClassInfo *class = dev->classes[j];
					XITouchClassInfo *t = (XITouchClassInfo*)class;

					if (class->type != XITouchClass)
						continue;

					printf("%s touch device, supporting %d touches.\n",
							(t->mode == XIDirectTouch) ?  "direct" : "dependent",
									t->num_touches);
				}
			}

			////////////////////

			eventmask.deviceid = XIAllDevices; //13;
			eventmask.mask_len = sizeof(mask); /* always in bytes */
			eventmask.mask = mask;
			/* now set the mask */
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

			default:
				printf("XISelectEvents() returned %d\n", error);
				break;

			}

			printf("Should now be able to get touch events\n");

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
