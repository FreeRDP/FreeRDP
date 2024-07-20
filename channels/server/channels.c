/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2011-2012 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/constants.h>
#include <freerdp/server/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include "channels.h"

/**
 * this is a workaround to force importing symbols
 * will need to fix that later on cleanly
 */

#if defined(CHANNEL_AUDIN_SERVER)
#include <freerdp/server/audin.h>
#endif
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/cliprdr.h>
#include <freerdp/server/echo.h>
#include <freerdp/server/rdpdr.h>
#if defined(CHANNEL_RAIL_SERVER)
#include <freerdp/server/rdpei.h>
#endif
#include <freerdp/server/drdynvc.h>
#include <freerdp/server/remdesk.h>
#include <freerdp/server/encomsp.h>
#if defined(CHANNEL_RAIL_SERVER)
#include <freerdp/server/rail.h>
#endif
#include <freerdp/server/telemetry.h>
#if defined(CHANNEL_RDPGFX_SERVER)
#include <freerdp/server/rdpgfx.h>
#endif
#if defined(CHANNEL_DISP_SERVER)
#include <freerdp/server/disp.h>
#endif

#if defined(CHANNEL_RDPEMSC_SERVER)
#include <freerdp/server/rdpemsc.h>
#endif /* CHANNEL_RDPEMSC_SERVER */

#if defined(CHANNEL_RDPECAM_SERVER)
#include <freerdp/server/rdpecam-enumerator.h>
#include <freerdp/server/rdpecam.h>
#endif

#if defined(CHANNEL_LOCATION_SERVER)
#include <freerdp/server/location.h>
#endif /* CHANNEL_LOCATION_SERVER */

#ifdef WITH_CHANNEL_GFXREDIR
#include <freerdp/server/gfxredir.h>
#endif /* WITH_CHANNEL_GFXREDIR */

#if defined(CHANNEL_AINPUT_SERVER)
#include <freerdp/server/ainput.h>
#endif

extern void freerdp_channels_dummy(void);

void freerdp_channels_dummy(void)
{
#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context* audin = NULL;
#endif
	RdpsndServerContext* rdpsnd = NULL;
	CliprdrServerContext* cliprdr = NULL;
	echo_server_context* echo = NULL;
	RdpdrServerContext* rdpdr = NULL;
	DrdynvcServerContext* drdynvc = NULL;
#if defined(CHANNEL_RDPEI_SERVER)
	RdpeiServerContext* rdpei = NULL;
#endif
	RemdeskServerContext* remdesk = NULL;
	EncomspServerContext* encomsp = NULL;
#if defined(CHANNEL_RAIL_SERVER)
	RailServerContext* rail = NULL;
#endif
	TelemetryServerContext* telemetry = NULL;
#if defined(CHANNEL_RDPGFX_SERVER)
	RdpgfxServerContext* rdpgfx = NULL;
#endif
#if defined(CHANNEL_DISP_SERVER)
	DispServerContext* disp = NULL;
#endif
#if defined(CHANNEL_RDPEMSC_SERVER)
	MouseCursorServerContext* mouse_cursor = NULL;
#endif /* CHANNEL_RDPEMSC_SERVER */
#if defined(CHANNEL_RDPECAM_SERVER)
	CamDevEnumServerContext* camera_enumerator = NULL;
	CameraDeviceServerContext* camera_device = NULL;
#endif
#if defined(CHANNEL_LOCATION_SERVER)
	LocationServerContext* location = NULL;
#endif /* CHANNEL_LOCATION_SERVER */
#ifdef WITH_CHANNEL_GFXREDIR
	GfxRedirServerContext* gfxredir;
#endif // WITH_CHANNEL_GFXREDIR
#if defined(CHANNEL_AUDIN_SERVER)
	audin = audin_server_context_new(NULL);
#endif
#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context_free(audin);
#endif
	rdpsnd = rdpsnd_server_context_new(NULL);
	rdpsnd_server_context_free(rdpsnd);
	cliprdr = cliprdr_server_context_new(NULL);
	cliprdr_server_context_free(cliprdr);
	echo = echo_server_context_new(NULL);
	echo_server_context_free(echo);
	rdpdr = rdpdr_server_context_new(NULL);
	rdpdr_server_context_free(rdpdr);
	drdynvc = drdynvc_server_context_new(NULL);
	drdynvc_server_context_free(drdynvc);
#if defined(CHANNEL_RDPEI_SERVER)
	rdpei = rdpei_server_context_new(NULL);
	rdpei_server_context_free(rdpei);
#endif
	remdesk = remdesk_server_context_new(NULL);
	remdesk_server_context_free(remdesk);
	encomsp = encomsp_server_context_new(NULL);
	encomsp_server_context_free(encomsp);
#if defined(CHANNEL_RAIL_SERVER)
	rail = rail_server_context_new(NULL);
	rail_server_context_free(rail);
#endif
	telemetry = telemetry_server_context_new(NULL);
	telemetry_server_context_free(telemetry);
#if defined(CHANNEL_RDPGFX_SERVER)
	rdpgfx = rdpgfx_server_context_new(NULL);
	rdpgfx_server_context_free(rdpgfx);
#endif
#if defined(CHANNEL_DISP_SERVER)
	disp = disp_server_context_new(NULL);
	disp_server_context_free(disp);
#endif
#if defined(CHANNEL_RDPEMSC_SERVER)
	mouse_cursor = mouse_cursor_server_context_new(NULL);
	mouse_cursor_server_context_free(mouse_cursor);
#endif /* CHANNEL_RDPEMSC_SERVER */

#if defined(CHANNEL_RDPECAM_SERVER)
	camera_enumerator = cam_dev_enum_server_context_new(NULL);
	cam_dev_enum_server_context_free(camera_enumerator);
	camera_device = camera_device_server_context_new(NULL);
	camera_device_server_context_free(camera_device);
#endif

#if defined(CHANNEL_LOCATION_SERVER)
	location = location_server_context_new(NULL);
	location_server_context_free(location);
#endif /* CHANNEL_LOCATION_SERVER */

#ifdef WITH_CHANNEL_GFXREDIR
	gfxredir = gfxredir_server_context_new(NULL);
	gfxredir_server_context_free(gfxredir);
#endif // WITH_CHANNEL_GFXREDIR
#if defined(CHANNEL_AINPUT_SERVER)
	{
		ainput_server_context* ainput = ainput_server_context_new(NULL);
		ainput_server_context_free(ainput);
	}
#endif
}

/**
 * end of ugly symbols import workaround
 */
