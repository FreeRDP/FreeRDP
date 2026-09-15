/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Out-of-process AAD auth helper integration
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

#pragma once

#include <cstdarg>
#include <memory>

#include <freerdp/freerdp.h>
#include <freerdp/client/aad_helper.h>

class SdlAadAuthHelper
{
  public:
	explicit SdlAadAuthHelper(AadAuthHelper* helper);
	SdlAadAuthHelper(const SdlAadAuthHelper&) = delete;
	SdlAadAuthHelper(SdlAadAuthHelper&&) = delete;
	SdlAadAuthHelper& operator=(const SdlAadAuthHelper&) = delete;
	SdlAadAuthHelper& operator=(SdlAadAuthHelper&&) = delete;
	~SdlAadAuthHelper();

	void stop();

	[[nodiscard]] AadAuthHelper* get() const;

  private:
	AadAuthHelper* _helper;
};

using SdlAadAuthHelperPtr = std::shared_ptr<SdlAadAuthHelper>;

/** @brief va_list-taking implementation, so a caller that's already inside its own variadic
 *  function (see the SDL2/SDL3 GetAccessToken trampolines) can forward its va_list here directly
 *  - the standard vprintf-style pattern - instead of needing to re-expose the raw "..." across a
 *  second function boundary, which C/C++ doesn't allow.
 *
 *  @param helper the caller's own per-connection storage slot (e.g. a member of its SdlContext);
 *  lazily filled in on first use and reused after that. This function never stores anything
 *  itself. */
[[nodiscard]] BOOL sdl_aad_helper_get_access_token_v(freerdp* instance, SdlAadAuthHelperPtr& helper,
                                                     AccessTokenType tokenType, char** token,
                                                     size_t count, va_list args);

/** @brief convenience variadic wrapper around sdl_aad_helper_get_access_token_v(), for callers
 *  that aren't themselves forwarding an existing va_list. */
[[nodiscard]] BOOL sdl_aad_helper_get_access_token(freerdp* instance, SdlAadAuthHelperPtr& helper,
                                                   AccessTokenType tokenType, char** token,
                                                   size_t count, ...);
