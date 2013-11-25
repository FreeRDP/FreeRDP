/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP6 Planar Codec
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "planar.h"

int freerdp_split_color_planes(BYTE* data, int width, int height, int scanline, BYTE* planes[4])
{
	BYTE* srcp;
	int i, j, k;

	k = 0;
	srcp = data;

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			planes[0][k] = data[(j * 4) + 0];
			planes[1][k] = data[(j * 4) + 1];
			planes[2][k] = data[(j * 4) + 2];
			planes[3][k] = data[(j * 4) + 3];
			k++;
		}

		srcp += scanline;
	}

	return 0;
}
