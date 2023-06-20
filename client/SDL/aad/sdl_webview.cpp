/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Popup browser for AAD authentication
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <sstream>
#include <stdlib.h>
#include <winpr/string.h>

#include "sdl_webview.h"
#include "webview_impl.h"

BOOL sdl_webview_get_aad_auth_code(freerdp* instance, const char* hostname, char** code,
                                   const char** client_id, const char** redirect_uri)
{
	int argc = 1;
	std::string name = "FreeRDP WebView";

	WINPR_ASSERT(instance);
	WINPR_ASSERT(hostname);
	WINPR_ASSERT(code);
	WINPR_ASSERT(client_id);
	WINPR_ASSERT(redirect_uri);

	WINPR_UNUSED(instance);

	*code = nullptr;
	*client_id = "5177bc73-fd99-4c77-a90c-76844c9b6999";
	*redirect_uri =
	    "ms-appx-web%3a%2f%2fMicrosoft.AAD.BrokerPlugin%2f5177bc73-fd99-4c77-a90c-76844c9b6999";

	std::stringstream url;
	url << "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id="
	    << std::string(*client_id)
	    << "&response_type=code&scope=ms-device-service%3A%2F%2Ftermsrv.wvd.microsoft.com%"
	       "2Fname%2F"
	    << std::string(hostname)
	    << "%2Fuser_impersonation&redirect_uri=" << std::string(*redirect_uri);

	auto urlstr = url.str();
	const std::string title = "FreeRDP WebView";
	std::string cxxcode;
	if (!webview_impl_run(title, urlstr, cxxcode))
		return FALSE;
	*code = _strdup(cxxcode.c_str());
	return TRUE;
}
