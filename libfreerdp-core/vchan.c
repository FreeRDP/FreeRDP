/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Virtual Channels
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

#include "vchan.h"

void vchan_process(rdpVchan* vchan, STREAM* s, uint16 channel_id)
{
	printf("vchan_process\n");
}

rdpVchan* vchan_new(freerdp* instance)
{
	rdpVchan* vchan;

	vchan = xnew(rdpVchan);
	vchan->instance = instance;

	return vchan;
}

void vchan_free(rdpVchan* vchan)
{
	xfree(vchan);
}
