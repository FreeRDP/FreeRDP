/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OAuth2 redirect URI matching for the AAD auth helper
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include "redirect_watcher.hpp"

#include <map>
#include <regex>
#include <vector>

#include <winpr/string.h>

namespace
{
	std::string from_url_encoded_str(const std::string& str)
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

	std::vector<std::string> split(const std::string& input, const std::string& regex)
	{
		// passing -1 as the submatch index parameter performs splitting
		std::regex re(regex);
		std::sregex_token_iterator first{ input.begin(), input.end(), re, -1 };
		std::sregex_token_iterator last;
		return { first, last };
	}

	std::map<std::string, std::string> urlsplit(const std::string& url)
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
} // namespace

RedirectWatcher::RedirectWatcher(std::string redirectUri) : _redirect_uri(std::move(redirectUri))
{
}

bool RedirectWatcher::matches(const std::string& uri) const
{
	if (_redirect_uri.empty())
		return false;

	std::string duri = from_url_encoded_str(uri);
	if (duri.length() < _redirect_uri.length())
		return false;

	return _strnicmp(duri.c_str(), _redirect_uri.c_str(), _redirect_uri.length()) == 0;
}

bool RedirectWatcher::hasError(const std::string& uri, std::string& error) const
{
	auto args = urlsplit(uri);
	auto err = args.find("error");
	if (err == args.end())
		return false;

	error = err->second;
	auto suberr = args.find("error_subcode");
	if (suberr != args.end())
		error += ": " + suberr->second;

	return true;
}
