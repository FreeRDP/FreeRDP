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

#include "webview.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <sstream>
#include "../webview_impl.hpp"

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

static void fkt(const std::string& url, void* arg)
{
	auto args = urlsplit(url);
	auto val = args.find("code");
	if (val == args.end())
		return;

	assert(arg);
	auto rcode = static_cast<std::string*>(arg);
	*rcode = val->second;
}

bool webview_impl_run(const std::string& title, const std::string& url, std::string& code)
{
	webview::webview w(false, nullptr);

	w.set_title(title);
	w.set_size(640, 480, WEBVIEW_HINT_NONE);

	std::string scheme;
	w.add_scheme_handler("ms-appx-web", fkt, &scheme);
	w.add_navigate_listener(fkt, &code);
	w.navigate(url);
	w.run();
	return !code.empty();
}
