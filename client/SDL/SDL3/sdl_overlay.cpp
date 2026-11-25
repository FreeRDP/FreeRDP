/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: the stats and info overlay
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

#include <freerdp/client.h>
#include <freerdp/codecs.h>
#include <freerdp/log.h>

#include "sdl_overlay.hpp"
#include "sdl_context.hpp"
#include "sdl_utils.hpp"
#include "dialogs/res/sdl3_resource_manager.hpp"

#define TAG CLIENT_TAG("SDL.overlay")

/* The first reading comes soon after the overlay shows, then one a second. */
static constexpr uint64_t firstIntervalNS = 100ull * 1000ull * 1000ull;
static constexpr uint64_t intervalNS = 1000ull * 1000ull * 1000ull;

/* What the overlay reads of the libraries, all of it: the client's runtime statistics. Keep
 * it the only place that knows them. */
static std::vector<std::string> statsNames()
{
	std::vector<std::string> names;
	const size_t count = freerdp_client_stats_max_index();
	names.reserve(count);
	for (size_t x = 0; x < count; x++)
	{
		const char* name = freerdp_client_stats_name_for_index(x);
		names.emplace_back(name ? name : "");
	}
	return names;
}

/* On the session thread, which owns what is read */
static void statsRead(rdpContext* context, sdl::overlay::Reading& reading)
{
	const size_t count = freerdp_client_stats_max_index();
	reading.values.resize(count);
	for (size_t x = 0; x < count; x++)
		reading.values.at(x) = freerdp_client_stats_value_for_index(context, x);
}

static std::string decodersSupported(rdpContext* context)
{
	struct Codec
	{
		FreeRDP_CodecFlags flag;
		const char* label;
	};
	/* the flags of codecs a build leaves out do not exist */
	const std::vector<Codec> codecs = {
		{ FREERDP_CODEC_AVC420, "H.264 4:2:0" },    { FREERDP_CODEC_AVC444, "H.264 4:4:4" },
#if defined(WITH_GFX_AV1)
		{ FREERDP_CODEC_AV1_I420, "AV1 4:2:0" },    { FREERDP_CODEC_AV1_I444, "AV1 4:4:4" },
#endif
#if defined(WITH_GFX_AV2)
		{ FREERDP_CODEC_AV2_I420, "AV2 4:2:0" },    { FREERDP_CODEC_AV2_I444, "AV2 4:4:4" },
#endif
#if defined(WITH_GFX_AZURE)
		{ FREERDP_CODEC_HEVC_I420, "HEVC" },
#endif
		{ FREERDP_CODEC_REMOTEFX, "RemoteFX" },     { FREERDP_CODEC_PROGRESSIVE, "Progressive" },
		{ FREERDP_CODEC_CLEARCODEC, "ClearCodec" }, { FREERDP_CODEC_PLANAR, "Planar" },
	};
	std::string list;
	for (const auto& codec : codecs)
	{
		if (!freerdp_codec_supported(context, codec.flag, FALSE))
			continue;
		list += list.empty() ? codec.label : std::string(", ") + codec.label;
	}
	return list;
}

static std::string version(int v)
{
	return std::to_string(SDL_VERSIONNUM_MAJOR(v)) + "." + std::to_string(SDL_VERSIONNUM_MINOR(v)) +
	       "." + std::to_string(SDL_VERSIONNUM_MICRO(v));
}

/* a scale the font and the panels were made for: the same unless it really changed */
static bool sameScale(float a, float b)
{
	return std::fabs(a - b) < 0.001F;
}

SdlOverlay::SdlOverlay(SdlContext* sdl) : _sdl(sdl)
{
	WINPR_ASSERT(_sdl);
}

bool SdlOverlay::visible() const
{
	return _visible;
}

bool SdlOverlay::show(bool on)
{
	_visible = on;
	if (!on || !_sdl->isConnected())
		return true;
	return arm();
}

bool SdlOverlay::connected()
{
	{
		std::scoped_lock lock(_mutex);
		_since = std::chrono::steady_clock::now();
		_pending.reset();
		_restart = true;
	}
	if (!_visible)
		return true;
	return arm();
}

bool SdlOverlay::arm()
{
	/* one timer at most: the one that sees _armed false adds it */
	bool expected = false;
	if (!_armed.compare_exchange_strong(expected, true))
		return true;

	if (freerdp_timer_add(_sdl->context(), firstIntervalNS, onTimer, this, true) == 0)
	{
		_armed = false;
		WLog_Print(_sdl->getWLog(), WLOG_ERROR, "the overlay's timer could not start");
		return false;
	}
	return true;
}

uint64_t SdlOverlay::onTimer(rdpContext* context, void* userdata,
                             WINPR_ATTR_UNUSED FreeRDP_TimerID timerID, uint64_t timestamp,
                             WINPR_ATTR_UNUSED uint64_t interval)
{
	auto overlay = static_cast<SdlOverlay*>(userdata);
	WINPR_ASSERT(overlay);

	if (!overlay->_visible)
	{
		/* Stop, unless it was shown again meanwhile: then this timer goes on, or the one a
		 * show added does (the compare exchange lets only one go on). */
		overlay->_armed = false;
		if (!overlay->_visible)
			return 0;
		bool expected = false;
		if (!overlay->_armed.compare_exchange_strong(expected, true))
			return 0;
	}

	if (!overlay->read(context, timestamp))
		WLog_Print(overlay->_sdl->getWLog(), WLOG_WARN, "the overlay's reading was not passed on");
	return intervalNS;
}

bool SdlOverlay::read(rdpContext* context, uint64_t timestamp)
{
	WINPR_ASSERT(context);

	sdl::overlay::Reading reading;
	reading.ns = timestamp;
	reading.desktopWidth = freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopWidth);
	reading.desktopHeight = freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopHeight);
	statsRead(context, reading);

	{
		std::scoped_lock lock(_mutex);
		_pending = std::move(reading);
	}
	return sdl_push_user_event(SDL_EVENT_USER_OVERLAY);
}

void SdlOverlay::clientRows(const char* rendererName)
{
	_client.clear();
	std::string freerdp = freerdp_get_version_string();
	const char* revision = freerdp_get_build_revision();
	/* a build from outside a git tree says "n/a" */
	if (revision && (strlen(revision) > 0) && (strcmp(revision, "n/a") != 0))
		freerdp += std::string(" (") + revision + ")";
	_client.emplace_back(sdl::overlay::Row{ "FreeRDP", freerdp });
	_client.emplace_back(
	    sdl::overlay::Row{ "Build", sdl::overlay::buildSummary(freerdp_get_build_config()) });
	_client.emplace_back(sdl::overlay::Row{ "SDL", version(SDL_GetVersion()) + ", SDL_ttf " +
	                                                   version(TTF_Version()) + ", renderer " +
	                                                   (rendererName ? rendererName : "unknown") });
	_client.emplace_back(sdl::overlay::Row{ "Decoders", decodersSupported(_sdl->context()) });
}

bool SdlOverlay::update(int windowWidth, int windowHeight, const char* rendererName)
{
	std::optional<sdl::overlay::Reading> reading;
	std::chrono::steady_clock::time_point since;
	bool restart = false;
	{
		std::scoped_lock lock(_mutex);
		reading.swap(_pending);
		since = _since;
		restart = _restart;
		_restart = false;
	}
	if (restart)
		_prev.reset();
	if (!reading || !_visible)
		return false;

	const auto& names = _names ? *_names : _names.emplace(statsNames());
	if (_client.empty())
		clientRows(rendererName);

	sdl::overlay::View view;
	view.windowWidth = windowWidth;
	view.windowHeight = windowHeight;
	view.sessionSeconds = static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - since)
	        .count());

	_left = _client;
	_right = sdl::overlay::realtimeRows(names, *reading, _prev ? &*_prev : nullptr, view);
	_prev = std::move(reading);
	_generation++;
	return true;
}

const std::vector<sdl::overlay::Row>& SdlOverlay::left() const
{
	return _left;
}

const std::vector<sdl::overlay::Row>& SdlOverlay::right() const
{
	return _right;
}

uint64_t SdlOverlay::generation() const
{
	return _generation;
}

SdlOverlayPainter::SdlOverlayPainter(SDL_Renderer* renderer)
    : _renderer(renderer), _engine(TTF_CreateRendererTextEngine(renderer))
{
	if (!_engine)
		WLog_WARN(TAG, "TTF_CreateRendererTextEngine: %s", SDL_GetError());
}

SdlOverlayPainter::~SdlOverlayPainter()
{
	clear();
	if (_font)
		TTF_CloseFont(_font);
	if (_engine)
		TTF_DestroyRendererTextEngine(_engine);
}

void SdlOverlayPainter::clear()
{
	for (auto* list : { &_left, &_right })
	{
		for (auto& cell : *list)
		{
			if (cell.label)
				TTF_DestroyText(cell.label);
			if (cell.value)
				TTF_DestroyText(cell.value);
		}
		list->clear();
	}
}

std::vector<SdlOverlayPainter::Cell>
SdlOverlayPainter::cells(const std::vector<sdl::overlay::Row>& rows)
{
	std::vector<Cell> list;
	for (const auto& row : rows)
	{
		Cell cell;
		if (!row.label.empty())
			cell.label = TTF_CreateText(_engine, _font, row.label.c_str(), row.label.size());
		if (!row.value.empty())
			cell.value = TTF_CreateText(_engine, _font, row.value.c_str(), row.value.size());
		for (auto* text : { cell.label, cell.value })
		{
			if (text)
				std::ignore = TTF_SetTextColor(text, 0xFF, 0xFF, 0xFF, 0xFF);
		}
		/* the labels a little dimmer than the values */
		if (cell.label)
			std::ignore = TTF_SetTextColor(cell.label, 0xB8, 0xC4, 0xD0, 0xFF);
		list.push_back(cell);
	}
	return list;
}

bool SdlOverlayPainter::rebuild(const SdlOverlay& overlay, float scale)
{
	if (!_engine)
		return false;

	constexpr float points = 13.0F;
	if (!_font)
	{
		auto ops = SDL3ResourceManager::get(SDLResourceManager::typeFonts(),
		                                    "OpenSans-VariableFont_wdth,wght.ttf");
		if (!ops)
			return false;
		_font = TTF_OpenFontIO(ops, true, points * scale);
		if (!_font)
		{
			WLog_WARN(TAG, "TTF_OpenFontIO: %s", SDL_GetError());
			return false;
		}
	}
	else if (!sameScale(scale, _scale))
		std::ignore = TTF_SetFontSize(_font, points * scale);

	clear();
	_left = cells(overlay.left());
	_right = cells(overlay.right());
	_scale = scale;
	_generation = overlay.generation();
	return true;
}

SDL_FPoint SdlOverlayPainter::panelSize(const std::vector<Cell>& list, float& labelWidth) const
{
	labelWidth = 0;
	float valueWidth = 0;
	for (const auto& cell : list)
	{
		int w = 0;
		int h = 0;
		if (cell.label && TTF_GetTextSize(cell.label, &w, &h))
			labelWidth = std::max(labelWidth, static_cast<float>(w));
		if (cell.value && TTF_GetTextSize(cell.value, &w, &h))
			valueWidth = std::max(valueWidth, static_cast<float>(w));
	}
	const float gap = 12.0F * _scale;
	const float pad = 8.0F * _scale;
	const auto lines = static_cast<float>(list.size());
	return { pad + labelWidth + gap + valueWidth + pad,
		     pad + (lines * static_cast<float>(TTF_GetFontLineSkip(_font))) + pad };
}

void SdlOverlayPainter::drawPanel(const std::vector<Cell>& list, const sdl::overlay::Box& where)
{
	if (list.empty())
		return;

	float labelWidth = 0;
	std::ignore = panelSize(list, labelWidth);
	const float x = where.x;
	const float y = where.y;

	const SDL_FRect box = { where.x, where.y, where.w, where.h };
	std::ignore = SDL_SetRenderDrawColor(_renderer, 0x10, 0x14, 0x18, 0xA8);
	std::ignore = SDL_RenderFillRect(_renderer, &box);

	const float gap = 12.0F * _scale;
	const float pad = 8.0F * _scale;
	const auto skip = static_cast<float>(TTF_GetFontLineSkip(_font));
	float line = y + pad;
	for (const auto& cell : list)
	{
		if (cell.label)
			std::ignore = TTF_DrawRendererText(cell.label, x + pad, line);
		if (cell.value)
			std::ignore = TTF_DrawRendererText(cell.value, x + pad + labelWidth + gap, line);
		line += skip;
	}
}

void SdlOverlayPainter::draw(const SdlOverlay& overlay, int width, int height, float scale)
{
	if ((scale <= 0.0F) || (width <= 0) || (height <= 0))
		return;
	if ((overlay.generation() != _generation) || !sameScale(scale, _scale))
	{
		if (!rebuild(overlay, scale))
			return;
	}

	SDL_BlendMode mode = SDL_BLENDMODE_NONE;
	Uint8 r = 0;
	Uint8 g = 0;
	Uint8 b = 0;
	Uint8 a = 0;
	std::ignore = SDL_GetRenderDrawBlendMode(_renderer, &mode);
	std::ignore = SDL_GetRenderDrawColor(_renderer, &r, &g, &b, &a);
	std::ignore = SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

	float labelWidth = 0;
	const auto left = _left.empty() ? SDL_FPoint{ 0, 0 } : panelSize(_left, labelWidth);
	const auto right = _right.empty() ? SDL_FPoint{ 0, 0 } : panelSize(_right, labelWidth);
	const auto where = sdl::overlay::layout(left.x, left.y, right.x, right.y,
	                                        static_cast<float>(width), 12.0F * _scale);
	drawPanel(_left, where.left);
	drawPanel(_right, where.right);

	std::ignore = SDL_SetRenderDrawBlendMode(_renderer, mode);
	std::ignore = SDL_SetRenderDrawColor(_renderer, r, g, b, a);
}
