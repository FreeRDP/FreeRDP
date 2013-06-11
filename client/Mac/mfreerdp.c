/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#include <freerdp/utils/event.h>
#include <freerdp/utils/signal.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/channels.h>

#include <freerdp/rail.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/file.h>

#include "mf_interface.h"
#include "mfreerdp.h"

static long xv_port = 0;
static const size_t password_size = 512;



mfInfo* mf_mfi_new()
{
    mfInfo* mfi;

    mfi = (mfInfo*) malloc(sizeof(mfInfo));
    ZeroMemory(mfi, sizeof(mfInfo));

    return mfi;
}

void mf_mfi_free(mfInfo* mfi)
{
    free(mfi);
}

void mf_context_new(freerdp* instance, rdpContext* context)
{
    mfInfo* mfi;

    context->channels = freerdp_channels_new();

    mfi = mf_mfi_new();

    ((mfContext*) context)->mfi = mfi;
    mfi->instance = instance;

    // Register callbacks
    instance->context->client->OnParamChange = mf_on_param_change;
}

void mf_context_free(freerdp* instance, rdpContext* context)
{
    if (context->cache)
        cache_free(context->cache);

    freerdp_channels_free(context->channels);

    mf_mfi_free(((mfContext*) context)->mfi);
    ((mfContext*) context)->mfi = NULL;
}

void mf_on_param_change(freerdp* instance, int id)
{
}
