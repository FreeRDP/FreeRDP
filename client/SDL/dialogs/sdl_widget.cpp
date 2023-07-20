/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "sdl_widget.hpp"
#include "../sdl_utils.hpp"

#include "font/opensans_variable_font.hpp"

#include <freerdp/log.h>

#define TAG CLIENT_TAG("SDL.widget")

static const SDL_Color backgroundcolor = { 0x38, 0x36, 0x35, 0xff };

static const Uint32 vpadding = 5;
static const Uint32 hpadding = 10;

SdlWidget::SdlWidget(SDL_Renderer* renderer, const SDL_Rect& rect, bool input)
    : _font(nullptr), _rect(rect), _input(input)
{
	assert(renderer);

	auto ops = SDL_RWFromConstMem(font_buffer.data(), static_cast<int>(font_buffer.size()));
	if (ops)
		_font = TTF_OpenFontRW(ops, 0, 64);
}

SdlWidget::SdlWidget(SdlWidget&& other) noexcept
    : _font(std::move(other._font)), _rect(std::move(other._rect))
{
	other._font = nullptr;
}

SdlWidget::~SdlWidget()
{
	TTF_CloseFont(_font);
}

bool SdlWidget::error_ex(Uint32 res, const char* what, const char* file, size_t line,
                         const char* fkt)
{
	static wLog* log = nullptr;
	if (!log)
		log = WLog_Get(TAG);
	return sdl_log_error_ex(res, log, what, file, line, fkt);
}

static bool draw_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color)
{
	const int drc = SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const int rc = SDL_RenderFillRect(renderer, rect);
	return !widget_log_error(rc, "SDL_RenderFillRect");
}

bool SdlWidget::fill(SDL_Renderer* renderer, SDL_Color color)
{
	std::vector<SDL_Color> colors = { color };
	return fill(renderer, colors);
}

bool SdlWidget::fill(SDL_Renderer* renderer, const std::vector<SDL_Color>& colors)
{
	assert(renderer);
	SDL_BlendMode mode;
	SDL_GetRenderDrawBlendMode(renderer, &mode);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	for (auto color : colors)
	{
		draw_rect(renderer, &_rect, color);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
	}
	SDL_SetRenderDrawBlendMode(renderer, mode);
	return true;
}

bool SdlWidget::update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor,
                            SDL_Color bgcolor)
{
	assert(renderer);

	if (!fill(renderer, bgcolor))
		return false;
	return update_text(renderer, text, fgcolor);
}

const SDL_Rect& SdlWidget::rect() const
{
	return _rect;
}

bool SdlWidget::update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor)
{

	if (text.empty())
		return true;

	auto surface = TTF_RenderUTF8_Blended(_font, text.c_str(), fgcolor);
	if (!surface)
		return !widget_log_error(-1, "TTF_RenderText_Blended");

	auto texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	if (!texture)
		return !widget_log_error(-1, "SDL_CreateTextureFromSurface");

	SDL_Rect src{};
	TTF_SizeUTF8(_font, text.c_str(), &src.w, &src.h);

	/* Do some magic:
	 * - Add padding before and after text
	 * - if text is too long only show the last elements
	 * - if text is too short only update used space
	 */
	auto dst = _rect;
	dst.x += hpadding;
	dst.w -= 2 * hpadding;
	const float scale = static_cast<float>(dst.h) / static_cast<float>(src.h);
	const float sws = static_cast<float>(src.w) * scale;
	const float dws = static_cast<float>(dst.w) / scale;
	if (static_cast<float>(dst.w) > sws)
		dst.w = static_cast<int>(sws);
	if (static_cast<float>(src.w) > dws)
	{
		src.x = src.w - static_cast<int>(dws);
		src.w = static_cast<int>(dws);
	}
	const int rc = SDL_RenderCopy(renderer, texture, &src, &dst);
	SDL_DestroyTexture(texture);
	if (rc < 0)
		return !widget_log_error(rc, "SDL_RenderCopy");
	return true;
}

bool clear_window(SDL_Renderer* renderer)
{
	assert(renderer);

	const int drc = SDL_SetRenderDrawColor(renderer, backgroundcolor.r, backgroundcolor.g,
	                                       backgroundcolor.b, backgroundcolor.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const int rcls = SDL_RenderClear(renderer);
	return !widget_log_error(rcls, "SDL_RenderClear");
}
