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

#include <webview.h>

#include "webview_impl.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>

#include <winpr/crt.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("client.SDL.common.aad")

/** @brief Release a string that held credential material
 *
 *  Scrubs the authorization code the parser handed out, like the token request bodies the SDL
 *  AAD helper releases.
 *
 *  @param str The string to scrub and release, may be \b nullptr
 */
static void free_secret(char* str)
{
	if (str)
		SecureZeroMemory(str, strlen(str));
	free(str);
}

/** Collects the authorization code of the OAuth transaction the navigation listener watches. */
class fkt_arg
{
  public:
	explicit fkt_arg(rdpClientContext* cctx) : _cctx(cctx)
	{
	}

	[[nodiscard]] bool getCode(std::string& c) const
	{
		c = _code;
		return !c.empty();
	}

	/** @return true when the transaction has finished and the web view can be closed. */
	[[nodiscard]] bool handle(const char* uri)
	{
		char* code = nullptr;

		switch (freerdp_client_aad_parse_callback(_cctx, uri, &code))
		{
			case FREERDP_CLIENT_AAD_CALLBACK_UNRELATED:
				return false;
			case FREERDP_CLIENT_AAD_CALLBACK_CODE:
				_code = code;
				free_secret(code);
				return true;
			case FREERDP_CLIENT_AAD_CALLBACK_ERROR:
				WLog_ERR(TAG, "[Webview] the authorization server declined the request");
				return true;
			default:
				/* A navigation that reaches the redirect URI without a usable response is not
				 * the end of the sign in: the redirect URI is a page of the authorization
				 * server like any other, and anything that drives a top level navigation in
				 * the view could otherwise abort the transaction. Keep the window open and
				 * keep navigating, the user can still close it. The paste flow, which has one
				 * attempt, does treat this as a failure. */
				WLog_WARN(TAG, "[Webview] the authorization response was rejected, ignoring");
				return false;
		}
	}

  private:
	rdpClientContext* _cctx = nullptr;
	std::string _code;
};

static void fkt(webview_t webview, const char* uri, webview_navigation_event_t type, void* arg)
{
	assert(arg);
	auto rcode = static_cast<fkt_arg*>(arg);

	if (type != WEBVIEW_LOAD_FINISHED)
		return;

	if (!rcode->handle(uri))
		return;

	webview_terminate(webview);
}

bool webview_impl_run(const std::string& title, const std::string& url, rdpClientContext* cctx,
                      std::string& code)
{
	if (!cctx)
		return false;

	webview::webview w(false, nullptr);

	w.set_title(title);
	w.set_size(800, 600, WEBVIEW_HINT_NONE);

	fkt_arg arg(cctx);
	w.add_navigation_listener(fkt, &arg);
	w.navigate(url);
	w.run();
	return arg.getCode(code);
}
