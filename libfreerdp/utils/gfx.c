/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * GFX Utils - Helper functions converting something to string
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#include <freerdp/utils/gfx.h>
#include <freerdp/channels/rdpgfx.h>

static const char* RDPGFX_CMDID_STRINGS[] = { "RDPGFX_CMDID_UNUSED_0000",
	                                          "RDPGFX_CMDID_WIRETOSURFACE_1",
	                                          "RDPGFX_CMDID_WIRETOSURFACE_2",
	                                          "RDPGFX_CMDID_DELETEENCODINGCONTEXT",
	                                          "RDPGFX_CMDID_SOLIDFILL",
	                                          "RDPGFX_CMDID_SURFACETOSURFACE",
	                                          "RDPGFX_CMDID_SURFACETOCACHE",
	                                          "RDPGFX_CMDID_CACHETOSURFACE",
	                                          "RDPGFX_CMDID_EVICTCACHEENTRY",
	                                          "RDPGFX_CMDID_CREATESURFACE",
	                                          "RDPGFX_CMDID_DELETESURFACE",
	                                          "RDPGFX_CMDID_STARTFRAME",
	                                          "RDPGFX_CMDID_ENDFRAME",
	                                          "RDPGFX_CMDID_FRAMEACKNOWLEDGE",
	                                          "RDPGFX_CMDID_RESETGRAPHICS",
	                                          "RDPGFX_CMDID_MAPSURFACETOOUTPUT",
	                                          "RDPGFX_CMDID_CACHEIMPORTOFFER",
	                                          "RDPGFX_CMDID_CACHEIMPORTREPLY",
	                                          "RDPGFX_CMDID_CAPSADVERTISE",
	                                          "RDPGFX_CMDID_CAPSCONFIRM",
	                                          "RDPGFX_CMDID_UNUSED_0014",
	                                          "RDPGFX_CMDID_MAPSURFACETOWINDOW",
	                                          "RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE",
	                                          "RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT",
	                                          "RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW" };

const char* rdpgfx_get_cmd_id_string(UINT16 cmdId)
{
	if (cmdId <= RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW)
		return RDPGFX_CMDID_STRINGS[cmdId];
	else
		return "RDPGFX_CMDID_UNKNOWN";
}

const char* rdpgfx_get_codec_id_string(UINT16 codecId)
{
	switch (codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			return "RDPGFX_CODECID_UNCOMPRESSED";

		case RDPGFX_CODECID_CAVIDEO:
			return "RDPGFX_CODECID_CAVIDEO";

		case RDPGFX_CODECID_CLEARCODEC:
			return "RDPGFX_CODECID_CLEARCODEC";

		case RDPGFX_CODECID_PLANAR:
			return "RDPGFX_CODECID_PLANAR";

		case RDPGFX_CODECID_AVC420:
			return "RDPGFX_CODECID_AVC420";

		case RDPGFX_CODECID_AVC444:
			return "RDPGFX_CODECID_AVC444";

		case RDPGFX_CODECID_AVC444v2:
			return "RDPGFX_CODECID_AVC444v2";

		case RDPGFX_CODECID_ALPHA:
			return "RDPGFX_CODECID_ALPHA";

		case RDPGFX_CODECID_CAPROGRESSIVE:
			return "RDPGFX_CODECID_CAPROGRESSIVE";

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			return "RDPGFX_CODECID_CAPROGRESSIVE_V2";
	}

	return "RDPGFX_CODECID_UNKNOWN";
}
