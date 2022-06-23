/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display update notifications
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "display.h"

static BOOL display_write_monitor_layout_pdu(wStream* s, UINT32 monitorCount,
                                             const MONITOR_DEF* monitorDefArray)
{
	UINT32 index;

	if (!Stream_EnsureRemainingCapacity(s, 4 + (monitorCount * 20)))
		return FALSE;

	Stream_Write_UINT32(s, monitorCount); /* monitorCount (4 bytes) */

	for (index = 0; index < monitorCount; index++)
	{
		const MONITOR_DEF* monitor = &monitorDefArray[index];

		Stream_Write_INT32(s, monitor->left);   /* left (4 bytes) */
		Stream_Write_INT32(s, monitor->top);    /* top (4 bytes) */
		Stream_Write_INT32(s, monitor->right);  /* right (4 bytes) */
		Stream_Write_INT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Write_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	return TRUE;
}

BOOL display_convert_rdp_monitor_to_monitor_def(UINT32 monitorCount,
                                                const rdpMonitor* monitorDefArray,
                                                MONITOR_DEF** result)
{
	UINT32 index;
	MONITOR_DEF* mdef = NULL;

	if (!monitorDefArray || !result || (*result))
		return FALSE;

	mdef = (MONITOR_DEF*)calloc(monitorCount, sizeof(MONITOR_DEF));

	if (!mdef)
		return FALSE;

	for (index = 0; index < monitorCount; index++)
	{
		const rdpMonitor* monitor = &monitorDefArray[index];
		MONITOR_DEF* current = &mdef[index];

		current->left = monitor->x;                                   /* left (4 bytes) */
		current->top = monitor->y;                                    /* top (4 bytes) */
		current->right = monitor->x + monitor->width - 1;             /* right (4 bytes) */
		current->bottom = monitor->y + monitor->height - 1;           /* bottom (4 bytes) */
		current->flags = monitor->is_primary ? MONITOR_PRIMARY : 0x0; /* flags (4 bytes) */
	}

	*result = mdef;
	return TRUE;
}

BOOL freerdp_display_send_monitor_layout(rdpContext* context, UINT32 monitorCount,
                                         const MONITOR_DEF* monitorDefArray)
{
	rdpRdp* rdp = context->rdp;
	wStream* st = rdp_data_pdu_init(rdp);

	if (!st)
		return FALSE;

	if (!display_write_monitor_layout_pdu(st, monitorCount, monitorDefArray))
	{
		Stream_Release(st);
		return FALSE;
	}

	return rdp_send_data_pdu(rdp, st, DATA_PDU_TYPE_MONITOR_LAYOUT, 0);
}
