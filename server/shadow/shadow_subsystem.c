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

#include <freerdp/config.h>

#include <winpr/cmdline.h>

#include <freerdp/log.h>

#include "shadow.h"

#include "shadow_subsystem.h"

#define TAG SERVER_TAG("shadow")

/* The subsystems built in, in the order they are tried */
#define SHADOW_SUBSYSTEM_MAX 8
#define SHADOW_SUBSYSTEM_MONITORS 16

typedef struct
{
	char name[32];
	pfnShadowSubsystemEntry entry;
} SHADOW_SUBSYSTEM_CANDIDATE;

static SHADOW_SUBSYSTEM_CANDIDATE g_Candidates[SHADOW_SUBSYSTEM_MAX] = WINPR_C_ARRAY_INIT;
static size_t g_CandidateCount = 0;

/* /subsystem: indexes into g_Candidates; empty means all of them, in their order */
static size_t g_Order[SHADOW_SUBSYSTEM_MAX] = WINPR_C_ARRAY_INIT;
static size_t g_OrderCount = 0;

/* The subsystem in use: set by shadow_subsystem_set_entry or picked from the candidates */
static pfnShadowSubsystemEntry pSubsystemEntry = nullptr;

void shadow_subsystem_set_entry(pfnShadowSubsystemEntry pEntry)
{
	pSubsystemEntry = pEntry;
}

BOOL shadow_subsystem_add_entry(const char* name, pfnShadowSubsystemEntry pEntry)
{
	if (!name || !pEntry || (g_CandidateCount >= SHADOW_SUBSYSTEM_MAX))
		return FALSE;

	SHADOW_SUBSYSTEM_CANDIDATE* candidate = &g_Candidates[g_CandidateCount];
	const int rc = _snprintf(candidate->name, sizeof(candidate->name), "%s", name);
	if ((rc < 0) || ((size_t)rc >= sizeof(candidate->name)))
		return FALSE;

	candidate->entry = pEntry;
	g_CandidateCount++;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static size_t shadow_subsystem_order_count(void)
{
	return (g_OrderCount > 0) ? g_OrderCount : g_CandidateCount;
}

WINPR_ATTR_NODISCARD
static const SHADOW_SUBSYSTEM_CANDIDATE* shadow_subsystem_order_at(size_t index)
{
	WINPR_ASSERT(index < shadow_subsystem_order_count());
	const size_t candidate = (g_OrderCount > 0) ? g_Order[index] : index;
	WINPR_ASSERT(candidate < g_CandidateCount);
	return &g_Candidates[candidate];
}

/* The number of monitors a subsystem finds: 0 when it cannot run here */
WINPR_ATTR_NODISCARD
static UINT32 shadow_subsystem_probe(const SHADOW_SUBSYSTEM_CANDIDATE* candidate)
{
	WINPR_ASSERT(candidate);
	WINPR_ASSERT(candidate->entry);

	RDP_SHADOW_ENTRY_POINTS ep = WINPR_C_ARRAY_INIT;
	if ((candidate->entry(&ep) < 0) || !ep.EnumMonitors)
		return 0;

	MONITOR_DEF monitors[SHADOW_SUBSYSTEM_MONITORS] = WINPR_C_ARRAY_INIT;
	return ep.EnumMonitors(monitors, ARRAYSIZE(monitors));
}

/* Pick the first subsystem that finds a monitor. With a single one there is
 * nothing to choose, so it is used without asking (and reports its own errors). */
WINPR_ATTR_NODISCARD
static BOOL shadow_subsystem_select(void)
{
	if (pSubsystemEntry)
		return TRUE;

	const size_t count = shadow_subsystem_order_count();
	if (count == 0)
		return FALSE;

	const SHADOW_SUBSYSTEM_CANDIDATE* first = shadow_subsystem_order_at(0);
	if (count == 1)
	{
		if (g_CandidateCount > 1)
			WLog_INFO(TAG, "subsystem: %s", first->name);
		pSubsystemEntry = first->entry;
		return TRUE;
	}

	for (size_t index = 0; index < count; index++)
	{
		const SHADOW_SUBSYSTEM_CANDIDATE* candidate = shadow_subsystem_order_at(index);
		if (shadow_subsystem_probe(candidate) > 0)
		{
			WLog_INFO(TAG, "subsystem: %s", candidate->name);
			pSubsystemEntry = candidate->entry;
			return TRUE;
		}

		WLog_INFO(TAG, "subsystem %s found no monitor%s", candidate->name,
		          (index + 1 < count) ? ", trying the next one" : "");
	}

	WLog_WARN(TAG, "no subsystem found a monitor, using %s", first->name);
	pSubsystemEntry = first->entry;
	return TRUE;
}

BOOL shadow_subsystem_set_order(const char* names)
{
	if (!names)
		return FALSE;

	if (g_CandidateCount == 0)
	{
		WLog_ERR(TAG, "this build has a single fixed subsystem");
		return FALSE;
	}

	size_t count = 0;
	char** list = CommandLineParseCommaSeparatedValues(names, &count);
	if (!list || (count == 0))
	{
		free((void*)list);
		return FALSE;
	}

	BOOL rc = TRUE;
	size_t orderCount = 0;
	size_t order[SHADOW_SUBSYSTEM_MAX] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; rc && (x < count); x++)
	{
		size_t found = g_CandidateCount;
		for (size_t y = 0; y < g_CandidateCount; y++)
		{
			if (_stricmp(list[x], g_Candidates[y].name) == 0)
				found = y;
		}

		if (found == g_CandidateCount)
		{
			WLog_ERR(TAG, "unknown subsystem '%s' (/subsystem lists them)", list[x]);
			rc = FALSE;
			break;
		}

		BOOL duplicate = FALSE;
		for (size_t y = 0; y < orderCount; y++)
		{
			if (order[y] == found)
				duplicate = TRUE;
		}

		if (!duplicate)
			order[orderCount++] = found;
	}
	free((void*)list);

	if (!rc)
		return FALSE;

	memcpy(g_Order, order, sizeof(g_Order));
	g_OrderCount = orderCount;
	pSubsystemEntry = nullptr;
	return TRUE;
}

void shadow_subsystem_list(void)
{
	const size_t count = shadow_subsystem_order_count();
	BOOL picked = FALSE;

	for (size_t index = 0; index < count; index++)
	{
		const SHADOW_SUBSYSTEM_CANDIDATE* candidate = shadow_subsystem_order_at(index);
		const UINT32 monitors = shadow_subsystem_probe(candidate);
		const BOOL pick = !picked && (monitors > 0);
		picked |= pick;
		WLog_INFO(TAG, "      %s %s: %" PRIu32 " monitor(s)", pick ? "*" : " ", candidate->name,
		          monitors);
	}

	if (count == 0)
		WLog_INFO(TAG, "this build has a single fixed subsystem");
}

WINPR_ATTR_NODISCARD
static int shadow_subsystem_load_entry_points(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);
	ZeroMemory(pEntryPoints, sizeof(RDP_SHADOW_ENTRY_POINTS));

	if (!shadow_subsystem_select())
		return -1;

	if (pSubsystemEntry(pEntryPoints) < 0)
		return -1;

	return 1;
}

rdpShadowSubsystem* shadow_subsystem_new(void)
{
	RDP_SHADOW_ENTRY_POINTS ep = WINPR_C_ARRAY_INIT;

	if (shadow_subsystem_load_entry_points(&ep) < 0)
		return nullptr;

	if (!ep.New)
		return nullptr;

	rdpShadowSubsystem* subsystem = ep.New();

	if (!subsystem)
		return nullptr;

	subsystem->ep = ep;

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

	status = subsystem->ep.Init(subsystem);
	if (status >= 0)
		return status;

fail:
	if (subsystem->MsgPipe)
	{
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = nullptr;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = nullptr;
	}

	return status;
}

static void shadow_subsystem_free_queued_message(void* obj)
{
	wMessage* message = (wMessage*)obj;
	if (message->Free)
	{
		message->Free(message);
		message->Free = nullptr;
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
		wObject* obj1 = nullptr;
		wObject* obj2 = nullptr;
		/* Release resource in messages before free */
		obj1 = MessageQueue_Object(subsystem->MsgPipe->In);

		obj1->fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->In);

		obj2 = MessageQueue_Object(subsystem->MsgPipe->Out);
		obj2->fnObjectFree = shadow_subsystem_free_queued_message;
		MessageQueue_Clear(subsystem->MsgPipe->Out);
		MessagePipe_Free(subsystem->MsgPipe);
		subsystem->MsgPipe = nullptr;
	}

	if (subsystem->updateEvent)
	{
		shadow_multiclient_free(subsystem->updateEvent);
		subsystem->updateEvent = nullptr;
	}
}

int shadow_subsystem_start(rdpShadowSubsystem* subsystem)
{
	int status = 0;

	if (!subsystem || !subsystem->ep.Start)
		return -1;

	status = subsystem->ep.Start(subsystem);

	return status;
}

int shadow_subsystem_stop(rdpShadowSubsystem* subsystem)
{
	int status = 0;

	if (!subsystem || !subsystem->ep.Stop)
		return -1;

	status = subsystem->ep.Stop(subsystem);

	return status;
}

UINT32 shadow_enum_monitors(MONITOR_DEF* monitors, UINT32 maxMonitors)
{
	UINT32 numMonitors = 0;
	RDP_SHADOW_ENTRY_POINTS ep;

	if (shadow_subsystem_load_entry_points(&ep) < 0)
		return 0;

	numMonitors = ep.EnumMonitors(monitors, maxMonitors);

	return numMonitors;
}

/**
 * Common function for subsystem implementation.
 * This function convert 32bit ARGB format pixels to xormask data
 * and andmask data and fill into SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE
 * Caller should free the andMaskData and xorMaskData later.
 */
#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)
int shadow_subsystem_pointer_convert_alpha_pointer_data(
    const BYTE* WINPR_RESTRICT pixels, BOOL premultiplied, UINT32 width, UINT32 height,
    SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* WINPR_RESTRICT pointerColor)
{
	return shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(
	    pixels, PIXEL_FORMAT_BGRX32, premultiplied, width, height, pointerColor);
}
#endif

int shadow_subsystem_pointer_convert_alpha_pointer_data_to_format(
    const BYTE* WINPR_RESTRICT pixels, UINT32 format, BOOL premultiplied, UINT32 width,
    UINT32 height, SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* WINPR_RESTRICT pointerColor)
{
	UINT32 xorStep = 0;
	UINT32 andStep = 0;
	UINT32 andBit = 0;
	BYTE* andBits = nullptr;
	UINT32 andPixel = 0;
	const size_t bpp = FreeRDPGetBytesPerPixel(format);

	xorStep = (width * 3);
	xorStep += (xorStep % 2);

	andStep = ((width + 7) / 8);
	andStep += (andStep % 2);

	WINPR_ASSERT(pointerColor);

	pointerColor->lengthXorMask = height * xorStep;
	pointerColor->xorMaskData = (BYTE*)calloc(1, pointerColor->lengthXorMask);

	if (!pointerColor->xorMaskData)
		return -1;

	pointerColor->lengthAndMask = height * andStep;
	pointerColor->andMaskData = (BYTE*)calloc(1, pointerColor->lengthAndMask);

	if (!pointerColor->andMaskData)
	{
		free(pointerColor->xorMaskData);
		pointerColor->xorMaskData = nullptr;
		return -1;
	}

	for (size_t y = 0; y < height; y++)
	{
		WINPR_ASSERT(pixels);
		const BYTE* pSrc8 = &pixels[(width * bpp) * (height - 1 - y)];
		BYTE* pDst8 = &(pointerColor->xorMaskData[y * xorStep]);

		andBit = 0x80;
		andBits = &(pointerColor->andMaskData[andStep * y]);

		for (size_t x = 0; x < width; x++)
		{
			BYTE B = 0;
			BYTE G = 0;
			BYTE R = 0;
			BYTE A = 0;

			const UINT32 color = FreeRDPReadColor(&pSrc8[x * bpp], format);
			FreeRDPSplitColor(color, format, &R, &G, &B, &A, nullptr);

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
					B = (B * 0xFF) / A;
					G = (G * 0xFF) / A;
					R = (R * 0xFF) / A;
				}
			}

			*pDst8++ = B;
			*pDst8++ = G;
			*pDst8++ = R;

			if (andPixel)
				*andBits |= andBit;
			if (!(andBit >>= 1))
			{
				andBits++;
				andBit = 0x80;
			}
		}
	}

	return 1;
}

void shadow_subsystem_frame_update(rdpShadowSubsystem* subsystem)
{
	shadow_multiclient_publish_and_wait(subsystem->updateEvent);
}
