/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "shadow.h"

#include "shadow_subsystem.h"

static pfnShadowSubsystemEntry pSubsystemEntry = NULL;

void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry)
{
	pSubsystemEntry = pEntry;
}

static int shadow_subsystem_load_entry_points(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_SHADOW_ENTRY_POINTS));

	if (!pSubsystemEntry)
		return -1;

	if (pSubsystemEntry(pEntryPoints) < 0)
		return -1;

	return 1;
}

rdpShadowSubsystem* shadow_subsystem_new()
{
	RDP_SHADOW_ENTRY_POINTS ep;
	rdpShadowSubsystem* subsystem = NULL;

	shadow_subsystem_load_entry_points(&ep);

	if (!ep.New)
		return NULL;

	subsystem = ep.New();

	if (!subsystem)
		return NULL;

	CopyMemory(&(subsystem->ep), &ep, sizeof(RDP_SHADOW_ENTRY_POINTS));

	return subsystem;
}

void shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	if (subsystem && subsystem->ep.Free)
		subsystem->ep.Free(subsystem);
}

int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server)
{
	int status = -1;

	if (!subsystem || !subsystem->ep.Init)
		return -1;

	subsystem->server = server;
	subsystem->selectedMonitor = server->selectedMonitor;

	if (!(subsystem->MsgPipe = MessagePipe_New()))
		goto fail;

	if (!(subsystem->updateEvent = shadow_multiclient_new()))
		goto fail;

	region16_init(&(subsystem->invalidRegion));

	if ((status = subsystem->ep.Init(subsystem)) >= 0)
		return status;

fail:
	if (subsystem->MsgPipe)
	{
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	return status;
}

static void shadow_subsystem_free_queued_message(void *obj)
{
	wMessage *message = (wMessage*)obj;
	if (message->Free)
	{
		message->Free(message);
		message->Free = NULL;
	}
}

void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	if (subsystem->ep.Uninit)
		subsystem->ep.Uninit(subsystem);

	if (subsystem->MsgPipe)
	{
		/* Release resource in messages before free */
		subsystem->MsgPipe->In->object.fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->In);
		subsystem->MsgPipe->Out->object.fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->Out);
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = NULL;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = NULL;
	}

	if (subsystem->invalidRegion.data)
		region16_uninit(&(subsystem->invalidRegion));
}

int shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem || !subsystem->ep.Start)
		return -1;

	status = subsystem->ep.Start(subsystem);

	return status;
}

int shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	int status;

	if (!subsystem || !subsystem->ep.Stop)
		return -1;

	status = subsystem->ep.Stop(subsystem);

	return status;
}

int shadow_enum_monitors(MONITOR_DEF* monitors, int maxMonitors)
{
	int numMonitors = 0;
	RDP_SHADOW_ENTRY_POINTS ep;

	if (shadow_subsystem_load_entry_points(&ep) < 0)
		return -1;

	numMonitors = ep.EnumMonitors(monitors, maxMonitors);

	return numMonitors;
}

/**
 * Common function for subsystem implementation.
 * This function convert 32bit ARGB format pixels to xormask data
 * and andmask data and fill into SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE
 * Caller should free the andMaskData and xorMaskData later.
 */
int shadow_subsystem_pointer_convert_alpha_pointer_data(BYTE* pixels, BOOL premultiplied,
		UINT32 width, UINT32 height, SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* pointerColor)
{
	UINT32 x, y;
	BYTE* pSrc8;
	BYTE* pDst8;
	int xorStep;
	int andStep;
	UINT32 andBit;
	BYTE* andBits;
	UINT32 andPixel;
	BYTE A, R, G, B;

	xorStep = (width * 3);
	xorStep += (xorStep % 2);

	andStep = ((width + 7) / 8);
	andStep += (andStep % 2);

	pointerColor->lengthXorMask = height * xorStep;
	pointerColor->xorMaskData = (BYTE*) calloc(1, pointerColor->lengthXorMask);

	if (!pointerColor->xorMaskData)
		return -1;

	pointerColor->lengthAndMask = height * andStep;
	pointerColor->andMaskData = (BYTE*) calloc(1, pointerColor->lengthAndMask);

	if (!pointerColor->andMaskData)
	{
		free(pointerColor->xorMaskData);
		pointerColor->xorMaskData = NULL;
		return -1;
	}

	for (y = 0; y < height; y++)
	{
		pSrc8 = &pixels[(width * 4) * (height - 1 - y)];
		pDst8 = &(pointerColor->xorMaskData[y * xorStep]);

		andBit = 0x80;
		andBits = &(pointerColor->andMaskData[andStep * y]);

		for (x = 0; x < width; x++)
		{
			B = *pSrc8++;
			G = *pSrc8++;
			R = *pSrc8++;
			A = *pSrc8++;

			andPixel = 0;

			if (A < 64)
				A = 0; /* pixel cannot be partially transparent */

			if (!A)
			{
				/* transparent pixel: XOR = black, AND = 1 */
				andPixel = 1;
				B = G = R = 0;
			}
			else
			{
				if (premultiplied)
				{
					B = (B * 0xFF ) / A;
					G = (G * 0xFF ) / A;
					R = (R * 0xFF ) / A;
				}
			}

			*pDst8++ = B;
			*pDst8++ = G;
			*pDst8++ = R;

			if (andPixel) *andBits |= andBit;
			if (!(andBit >>= 1)) { andBits++; andBit = 0x80; }
		}
	}

	return 1;
}

void shadow_subsystem_frame_update(rdpShadowSubsystem* subsystem)
{
	shadow_multiclient_publish_and_wait(subsystem->updateEvent);
}
