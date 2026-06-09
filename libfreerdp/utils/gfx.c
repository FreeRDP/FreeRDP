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

const char* rdpgfx_get_cmd_id_string(UINT16 cmdId)
{
#define EVCASE(x) \
	case x:       \
		return #x
	switch (cmdId)
	{
		EVCASE(RDPGFX_CMDID_UNUSED_0000);
		EVCASE(RDPGFX_CMDID_WIRETOSURFACE_1);
		EVCASE(RDPGFX_CMDID_WIRETOSURFACE_2);
		EVCASE(RDPGFX_CMDID_DELETEENCODINGCONTEXT);
		EVCASE(RDPGFX_CMDID_SOLIDFILL);
		EVCASE(RDPGFX_CMDID_SURFACETOSURFACE);
		EVCASE(RDPGFX_CMDID_SURFACETOCACHE);
		EVCASE(RDPGFX_CMDID_CACHETOSURFACE);
		EVCASE(RDPGFX_CMDID_EVICTCACHEENTRY);
		EVCASE(RDPGFX_CMDID_CREATESURFACE);
		EVCASE(RDPGFX_CMDID_DELETESURFACE);
		EVCASE(RDPGFX_CMDID_STARTFRAME);
		EVCASE(RDPGFX_CMDID_ENDFRAME);
		EVCASE(RDPGFX_CMDID_FRAMEACKNOWLEDGE);
		EVCASE(RDPGFX_CMDID_RESETGRAPHICS);
		EVCASE(RDPGFX_CMDID_MAPSURFACETOOUTPUT);
		EVCASE(RDPGFX_CMDID_CACHEIMPORTOFFER);
		EVCASE(RDPGFX_CMDID_CACHEIMPORTREPLY);
		EVCASE(RDPGFX_CMDID_CAPSADVERTISE);
		EVCASE(RDPGFX_CMDID_CAPSCONFIRM);
		EVCASE(RDPGFX_CMDID_UNUSED_0014);
		EVCASE(RDPGFX_CMDID_MAPSURFACETOWINDOW);
		EVCASE(RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE);
		EVCASE(RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT);
		EVCASE(RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW);
		default:
			return "RDPGFX_CMDID_UNKNOWN";
	}
#undef EVCASE
}

const char* rdpgfx_get_codec_id_string(UINT16 codecId)
{
	switch (codecId)
	{
#if defined(WITH_GFX_AV1)
		case RDPGFX_CODECID_AV1:
			return "RDPGFX_CODECID_AV1";
#endif

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
		default:
			break;
	}

	return "RDPGFX_CODECID_UNKNOWN";
}
