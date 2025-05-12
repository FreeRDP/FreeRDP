#include <iostream>
#include <fstream>
#include <memory>

#include <winpr/config.h>
#include <winpr/winpr.h>

#include <sdl_webview.hpp>

int TestSDLWebview(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
#if 0
	RDP_CLIENT_ENTRY_POINTS entry = {};
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpContext);

	std::shared_ptr<rdpContext> context(freerdp_client_context_new(&entry),
	                                    [](rdpContext* ptr) { freerdp_client_context_free(ptr); });

	char* token = nullptr;
	if (!sdl_webview_get_access_token(context->instance, ACCESS_TOKEN_TYPE_AAD, &token, 2, "scope",
	                                  "foobar"))
	{
		std::cerr << "test failed!" << std::endl;
		return -1;
	}
#endif
	return 0;
}
