/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPXXXX Remote App Graphics Redirection Virtual Channel Extension
 *
 * Copyright 2020 Microsoft
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

#ifndef FREERDP_CHANNEL_GFXREDIR_SERVER_GFXREDIR_H
#define FREERDP_CHANNEL_GFXREDIR_SERVER_GFXREDIR_H

#include <freerdp/channels/gfxredir.h>

#include <freerdp/freerdp.h>
#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct s_gfxredir_server_private GfxRedirServerPrivate;
typedef struct s_gfxredir_server_context GfxRedirServerContext;

typedef UINT (*psGfxRedirOpen)(GfxRedirServerContext* context);
typedef UINT (*psGfxRedirClose)(GfxRedirServerContext* context);

typedef UINT (*psGfxRedirError)(GfxRedirServerContext* context, const GFXREDIR_ERROR_PDU* error);

typedef UINT (*psGfxRedirGraphicsRedirectionLegacyCaps)(
    GfxRedirServerContext* context, const GFXREDIR_LEGACY_CAPS_PDU* graphicsCaps);

typedef UINT (*psGfxRedirGraphicsRedirectionCapsAdvertise)(
    GfxRedirServerContext* context, const GFXREDIR_CAPS_ADVERTISE_PDU* graphicsCapsAdvertise);
typedef UINT (*psGfxRedirGraphicsRedirectionCapsConfirm)(
    GfxRedirServerContext* context, const GFXREDIR_CAPS_CONFIRM_PDU* graphicsCapsConfirm);

typedef UINT (*psGfxRedirOpenPool)(GfxRedirServerContext* context,
                                   const GFXREDIR_OPEN_POOL_PDU* openPool);
typedef UINT (*psGfxRedirClosePool)(GfxRedirServerContext* context,
                                    const GFXREDIR_CLOSE_POOL_PDU* closePool);

typedef UINT (*psGfxRedirCreateBuffer)(GfxRedirServerContext* context,
                                       const GFXREDIR_CREATE_BUFFER_PDU* createBuffer);
typedef UINT (*psGfxRedirDestroyBuffer)(GfxRedirServerContext* context,
                                        const GFXREDIR_DESTROY_BUFFER_PDU* destroyBuffer);

typedef UINT (*psGfxRedirPresentBuffer)(GfxRedirServerContext* context,
                                        const GFXREDIR_PRESENT_BUFFER_PDU* presentBuffer);
typedef UINT (*psGfxRedirPresentBufferAck)(GfxRedirServerContext* context,
                                           const GFXREDIR_PRESENT_BUFFER_ACK_PDU* presentBufferAck);

struct s_gfxredir_server_context
{
	void* custom;
	HANDLE vcm;

	psGfxRedirOpen Open;
	psGfxRedirClose Close;

	psGfxRedirError Error;

	psGfxRedirGraphicsRedirectionLegacyCaps GraphicsRedirectionLegacyCaps;

	psGfxRedirGraphicsRedirectionCapsAdvertise GraphicsRedirectionCapsAdvertise;
	psGfxRedirGraphicsRedirectionCapsConfirm GraphicsRedirectionCapsConfirm;

	psGfxRedirOpenPool OpenPool;
	psGfxRedirClosePool ClosePool;

	psGfxRedirCreateBuffer CreateBuffer;
	psGfxRedirDestroyBuffer DestroyBuffer;

	psGfxRedirPresentBuffer PresentBuffer;
	psGfxRedirPresentBufferAck PresentBufferAck;

	GfxRedirServerPrivate* priv;
	rdpContext* rdpcontext;

	UINT32 confirmedCapsVersion;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API GfxRedirServerContext* gfxredir_server_context_new(HANDLE vcm);
	FREERDP_API void gfxredir_server_context_free(GfxRedirServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_GFXREDIR_SERVER_GFXREDIR_H */
