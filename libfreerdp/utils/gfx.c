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
		EVCASE(RDPGFX_CMDID_PROTECT_SURFACE);
		EVCASE(RDPGFX_CMDID_WATERMARK);
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

const char* rdpgfx_caps_version_str(UINT32 capsVersion)
{
#define EVCASE(x) \
	case x:       \
		return #x

	switch (capsVersion)
	{
#if defined(WITH_GFX_AV1)
		EVCASE(RDPGFX_CAPVERSION_FRDP_1);
#endif
		EVCASE(RDPGFX_CAPVERSION_8);
		EVCASE(RDPGFX_CAPVERSION_81);
		EVCASE(RDPGFX_CAPVERSION_10);
		EVCASE(RDPGFX_CAPVERSION_101);
		EVCASE(RDPGFX_CAPVERSION_102);
		EVCASE(RDPGFX_CAPVERSION_103);
		EVCASE(RDPGFX_CAPVERSION_104);
		EVCASE(RDPGFX_CAPVERSION_105);
		EVCASE(RDPGFX_CAPVERSION_106);
		EVCASE(RDPGFX_CAPVERSION_106_ERR);
		EVCASE(RDPGFX_CAPVERSION_107);
#if defined(WITH_GFX_AZURE)
		EVCASE(RDPGFX_CAPVERSION_111);
		EVCASE(RDPGFX_CAPVERSION_112);
		EVCASE(RDPGFX_CAPVERSION_113);
#endif
		default:
			return "RDPGFX_CAPVERSION_UNKNOWN";
	}

#undef EVCASE
}

static const char* rdpgfx_caps_flag_str_int(UINT32 flag)
{
#define EVCASE(x) \
	case x:       \
		return #x

	switch (flag)
	{
		EVCASE(RDPGFX_CAPS_FLAG_THINCLIENT);
		EVCASE(RDPGFX_CAPS_FLAG_SMALL_CACHE);
		EVCASE(RDPGFX_CAPS_FLAG_AVC420_ENABLED);
		EVCASE(RDPGFX_CAPS_FLAG_AVC_DISABLED);
		EVCASE(RDPGFX_CAPS_FLAG_AVC_THINCLIENT);
		EVCASE(RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE);
#if defined(WITH_GFX_AZURE)
		EVCASE(RDPGFX_CAPS_FLAG_SCP_DISABLE);
#endif
#if defined(WITH_GFX_AV1)
		EVCASE(RDPGFX_CAPS_FLAG_AV1_I444_SUPPORTED);
		EVCASE(RDPGFX_CAPS_FLAG_AV1_I444_DISABLED);
#endif
		default:
			return "RDPGFX_CAPS_FLAG_UNKNOWN";
	}

#undef EVCASE
}

const char* rdpgfx_caps_flag_str(UINT32 flag)
{
	const char* val = rdpgfx_caps_flag_str_int(flag);
	if (!val)
		return nullptr;

	const char prefix[] = "RDPGFX_CAPS_FLAG_";
	if (strncmp(val, prefix, sizeof(prefix) - 1) != 0)
		return val;
	return &val[sizeof(prefix)];
}

const char* rdpgfx_caps_flags_str(UINT32 flags, char* buffer, size_t length)
{
	if (length < 1)
		return buffer;

	WINPR_ASSERT(buffer);

	buffer[0] = '{';
	for (uint32_t x = 0; x < 32; x++)
	{
		uint32_t val = 1 << x;
		if ((flags & val) != 0)
		{
			const char* flag = rdpgfx_caps_flag_str(val);
			winpr_str_append(flag, &buffer[1], length - 1, "|");
		}
	}

	char number[16] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(number, sizeof(number), "[0x%08" PRIx32 "]", flags);
	winpr_str_append(number, buffer, length, "}");

	return buffer;
}
