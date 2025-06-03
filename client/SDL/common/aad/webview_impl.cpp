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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <freerdp/log.h>
#include <winpr/string.h>

#define TAG FREERDP_TAG("client.SDL.common.aad")

class fkt_arg
{
  public:
	fkt_arg(const std::string& url)
	{
		auto args = urlsplit(url);
		auto redir = args.find("redirect_uri");
		if (redir == args.end())
		{
			WLog_ERR(TAG,
			         "[Webview] url %s does not contain a redirect_uri parameter, "
			         "aborting.",
			         url.c_str());
		}
		else
		{
			_redirect_uri = from_url_encoded_str(redir->second);
		}
	}

	bool valid() const
	{
		return !_redirect_uri.empty();
	}

	bool getCode(std::string& c) const
	{
		c = _code;
		return !c.empty();
	}

	bool handle(const std::string& uri) const
	{
		std::string duri = from_url_encoded_str(uri);
		if (duri.length() < _redirect_uri.length())
			return false;
		auto rc = _strnicmp(duri.c_str(), _redirect_uri.c_str(), _redirect_uri.length());
		return rc == 0;
	}

	bool parse(const std::string& uri)
	{
		_args = urlsplit(uri);
		auto err = _args.find("error");
		if (err != _args.end())
		{
			auto suberr = _args.find("error_subcode");
			WLog_ERR(TAG, "[Webview] %s: %s, %s: %s", err->first.c_str(), err->second.c_str(),
			         suberr->first.c_str(), suberr->second.c_str());
			return false;
		}
		auto val = _args.find("code");
		if (val == _args.end())
		{
			WLog_ERR(TAG, "[Webview] no code parameter detected in redirect URI %s", uri.c_str());
			return false;
		}

		_code = val->second;
		return true;
	}

  protected:
	static std::string from_url_encoded_str(const std::string& str)
	{
		std::string cxxstr;
		auto cstr = winpr_str_url_decode(str.c_str(), str.length());
		if (cstr)
		{
			cxxstr = std::string(cstr);
			free(cstr);
		}
		return cxxstr;
	}

	static std::vector<std::string> split(const std::string& input, const std::string& regex)
	{
		// passing -1 as the submatch index parameter performs splitting
		std::regex re(regex);
		std::sregex_token_iterator first{ input.begin(), input.end(), re, -1 };
		std::sregex_token_iterator last;
		return { first, last };
	}

	static std::map<std::string, std::string> urlsplit(const std::string& url)
	{
		auto pos = url.find('?');
		if (pos == std::string::npos)
			return {};

		pos++; // skip '?'
		auto surl = url.substr(pos);
		auto args = split(surl, "&");

		std::map<std::string, std::string> argmap;
		for (const auto& arg : args)
		{
			auto kv = split(arg, "=");
			if (kv.size() == 2)
				argmap.insert({ kv[0], kv[1] });
		}

		return argmap;
	}

  private:
	std::string _redirect_uri;
	std::string _code;
	std::map<std::string, std::string> _args;
};

static void fkt(webview_t webview, const char* uri, webview_navigation_event_t type, void* arg)
{
	assert(arg);
	auto rcode = static_cast<fkt_arg*>(arg);

	if (type != WEBVIEW_LOAD_FINISHED)
		return;

	if (!rcode->handle(uri))
		return;

	(void)rcode->parse(uri);
	webview_terminate(webview);
}

bool webview_impl_run(const std::string& title, const std::string& url, std::string& code)
{
	webview::webview w(false, nullptr);

	w.set_title(title);
	w.set_size(800, 600, WEBVIEW_HINT_NONE);

	fkt_arg arg(url);
	if (!arg.valid())
	{
		return false;
	}
	w.add_navigation_listener(fkt, &arg);
	w.navigate(url);
	w.run();
	return arg.getCode(code);
}
