/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPEDISP Virtual Channel Extension
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_DISP_SERVER_DISP_H
#define FREERDP_CHANNEL_DISP_SERVER_DISP_H

#include <freerdp/channels/disp.h>

#include <freerdp/freerdp.h>
#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct _disp_server_private DispServerPrivate;
typedef struct _disp_server_context DispServerContext;

typedef UINT (*psDispMonitorLayout)(DispServerContext* context,
                                    const DISPLAY_CONTROL_MONITOR_LAYOUT_PDU* pdu);
typedef UINT (*psDispCaps)(DispServerContext* context);
typedef UINT (*psDispOpen)(DispServerContext* context);
typedef UINT (*psDispClose)(DispServerContext* context);

struct _disp_server_context
{
	void* custom;
	HANDLE vcm;

	/* Server capabilities */
	UINT32 MaxNumMonitors;
	UINT32 MaxMonitorAreaFactorA;
	UINT32 MaxMonitorAreaFactorB;

	psDispOpen Open;
	psDispClose Close;

	psDispMonitorLayout DispMonitorLayout;
	psDispCaps DisplayControlCaps;

	DispServerPrivate* priv;
	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API DispServerContext* disp_server_context_new(HANDLE vcm);
	FREERDP_API void disp_server_context_free(DispServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_DISP_SERVER_DISP_H */
