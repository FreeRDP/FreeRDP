/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client browser clipboard: which offers of the page are announced
 *
 * Copyright 2026 Gilad Chen <gilad.chen@island.io>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <winpr/wtypes.h>

/* The page offers the browser clipboard when it can't tell whether it changed, e.g. on focus.
 * Once the server announced its clipboard, an offer that is not a new local copy would take the
 * clipboard back from the server. */
class SdlWebClipOffers
{
  public:
	/* returns false when the offer must not be announced */
	[[nodiscard]] bool offer(const std::string& text, const BYTE* image, size_t imageSize)
	{
		const size_t textHash = hash(text);
		const size_t offerHash = textHash ^ (hash(image, imageSize) << 1);
		const bool isLastOffer = (offerHash == _offered);
		const bool isServerText = !imageSize && !text.empty() && (textHash == _received);
		if (_serverOwns && (isLastOffer || isServerText))
			return false;
		_serverOwns = false;
		_offered = offerHash;
		return true;
	}

	void serverAnnounced()
	{
		_serverOwns = true;
	}

	/* the page replaces what we offered */
	void serverSentText(const std::string& text)
	{
		_offered.reset();
		_received = hash(text);
	}

	void serverSentImage()
	{
		_offered.reset();
	}

  private:
	static size_t hash(const std::string& text)
	{
		return std::hash<std::string>{}(text);
	}

	static size_t hash(const BYTE* data, size_t size)
	{
		return std::hash<std::string_view>{}(
		    std::string_view(reinterpret_cast<const char*>(data), data ? size : 0));
	}

	bool _serverOwns = false;
	std::optional<size_t> _offered;
	std::optional<size_t> _received;
};
