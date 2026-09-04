/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OAuth2 redirect URI matching for the AAD auth helper
 *
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

#pragma once

#include <string>

/** Watches browser navigation events for a specific OAuth2 redirect_uri. The helper never
 *  interprets the "code" query parameter itself (that stays FreeRDP's job) - it only needs to
 *  recognize when the browser reached the redirect target and whether the IdP reported an
 *  error, then hand the whole URI back verbatim. */
class RedirectWatcher
{
  public:
	explicit RedirectWatcher(std::string redirectUri);

	[[nodiscard]] bool matches(const std::string& uri) const;
	[[nodiscard]] bool hasError(const std::string& uri, std::string& error) const;

  private:
	std::string _redirect_uri;
};
