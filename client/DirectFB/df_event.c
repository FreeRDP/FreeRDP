/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Event Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "df_event.h"

boolean df_event_process(freerdp* instance, DFBEvent* event)
{
	GDI* gdi;
	dfInfo* dfi;
	int keycode;
	int pointer_x;
	int pointer_y;
	int flags;
	DFBInputEvent* input_event;

	dfi = GET_DFI(instance);
	gdi = GET_GDI(instance->update);

	printf("process event\n");

	dfi->layer->GetCursorPosition(dfi->layer, &pointer_x, &pointer_y);

	if (event->clazz == DFEC_INPUT)
	{
		flags = 0;
		input_event = (DFBInputEvent *) event;

		switch (input_event->type)
		{
			case DIET_AXISMOTION:

				if (pointer_x > (gdi->width - 1))
					pointer_x = gdi->width - 1;

				if (pointer_y > (gdi->height - 1))
					pointer_y = gdi->height - 1;

				break;

			case DIET_BUTTONPRESS:
				flags = PTR_FLAGS_DOWN;
				/* fall */

			case DIET_BUTTONRELEASE:

				if (input_event->button == DIBI_LEFT)
					flags |= PTR_FLAGS_BUTTON1;
				else if (input_event->button == DIBI_RIGHT)
					flags |= PTR_FLAGS_BUTTON2;
				else if (input_event->button == DIBI_MIDDLE)
					flags |= PTR_FLAGS_BUTTON3;

				if (flags != 0)
					instance->input->MouseEvent(instance->input, flags, pointer_x, pointer_y);

				break;

			case DIET_KEYPRESS:
				keycode = input_event->key_id - DIKI_UNKNOWN;

				break;

			case DIET_KEYRELEASE:
				keycode = input_event->key_id - DIKI_UNKNOWN;

				break;

			case DIET_UNKNOWN:
				break;
		}
	}

	return True;
}
