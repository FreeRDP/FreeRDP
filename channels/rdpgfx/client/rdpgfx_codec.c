/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "rdpgfx_codec.h"

int rdpgfx_decode_uncompressed(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_remotefx(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_clear(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_planar(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_h264(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_alpha(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_progressive(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status;

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			status = rdpgfx_decode_uncompressed(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAVIDEO:
			status = rdpgfx_decode_remotefx(gfx, cmd);
			break;

		case RDPGFX_CODECID_CLEARCODEC:
			status = rdpgfx_decode_clear(gfx, cmd);
			break;

		case RDPGFX_CODECID_PLANAR:
			status = rdpgfx_decode_planar(gfx, cmd);
			break;

		case RDPGFX_CODECID_H264:
			status = rdpgfx_decode_h264(gfx, cmd);
			break;

		case RDPGFX_CODECID_ALPHA:
			status = rdpgfx_decode_alpha(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			status = rdpgfx_decode_progressive(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			break;
	}

	return 1;
}
