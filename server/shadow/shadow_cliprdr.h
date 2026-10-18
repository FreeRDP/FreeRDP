/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Shadow server clipboard (cliprdr) bridge
 *
 * Bridges the RDP clipboard virtual channel to the X11 CLIPBOARD selection so
 * that copy & paste works between the shadow server and the connected client.
 */

#ifndef FREERDP_SHADOW_SERVER_CLIPRDR_H
#define FREERDP_SHADOW_SERVER_CLIPRDR_H

#include "shadow.h"

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_ATTR_NODISCARD int shadow_client_cliprdr_init(rdpShadowClient* client);
	void shadow_client_cliprdr_uninit(rdpShadowClient* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_CLIPRDR_H */
