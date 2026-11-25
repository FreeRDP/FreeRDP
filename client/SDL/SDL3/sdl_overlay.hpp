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

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <freerdp/freerdp.h>
#include <freerdp/timer.h>

#include "sdl_overlay_model.hpp"

class SdlContext;

/** The stats and info overlay: off by default, a hotkey shows and hides it.
 *
 * The client's runtime statistics are read once a second on the session thread, by a FreeRDP
 * timer that runs only while the overlay shows, and drawn on SDL's thread over the picture,
 * never into it: the session's size and picture do not change. Off, nothing runs. */
class SdlOverlay
{
  public:
	explicit SdlOverlay(SdlContext* sdl);
	SdlOverlay(const SdlOverlay& other) = delete;
	SdlOverlay(SdlOverlay&& other) = delete;
	~SdlOverlay() = default;
	SdlOverlay& operator=(const SdlOverlay& other) = delete;
	SdlOverlay& operator=(SdlOverlay&& other) = delete;

	[[nodiscard]] bool visible() const;

	/** SDL's thread: show or hide. Shown before the session connects, it starts with it. */
	[[nodiscard]] bool show(bool on);

	/** The session thread, when a connection (or a reconnect) is up: rows start again. */
	[[nodiscard]] bool connected();

	/** SDL's thread, on SDL_EVENT_USER_OVERLAY: take the newest reading, build the rows.
	 * @return true when there is something new to draw */
	[[nodiscard]] bool update(int windowWidth, int windowHeight, const char* rendererName);

	/** The rows to draw: client info on the left, the real-time part on the right. A new
	 * generation means the rows changed. */
	[[nodiscard]] const std::vector<sdl::overlay::Row>& left() const;
	[[nodiscard]] const std::vector<sdl::overlay::Row>& right() const;
	[[nodiscard]] uint64_t generation() const;

  private:
	[[nodiscard]] bool arm();
	[[nodiscard]] static uint64_t onTimer(rdpContext* context, void* userdata,
	                                      FreeRDP_TimerID timerID, uint64_t timestamp,
	                                      uint64_t interval);
	[[nodiscard]] bool read(rdpContext* context, uint64_t timestamp);
	void clientRows(const char* rendererName);

	SdlContext* _sdl;
	std::atomic<bool> _visible = false;
	std::atomic<bool> _armed = false;

	/* written on the session thread, taken on SDL's */
	std::mutex _mutex;
	std::optional<sdl::overlay::Reading> _pending;
	std::chrono::steady_clock::time_point _since;
	bool _restart = false;

	/* SDL's thread only */
	std::optional<sdl::overlay::Names> _names;
	std::optional<sdl::overlay::Reading> _prev;
	std::vector<sdl::overlay::Row> _client;
	std::vector<sdl::overlay::Row> _left;
	std::vector<sdl::overlay::Row> _right;
	uint64_t _generation = 0;
};

/** Draws the overlay's rows with one window's renderer; the window owns it and frees it before
 * its renderer. */
class SdlOverlayPainter
{
  public:
	explicit SdlOverlayPainter(SDL_Renderer* renderer);
	SdlOverlayPainter(const SdlOverlayPainter& other) = delete;
	SdlOverlayPainter(SdlOverlayPainter&& other) = delete;
	~SdlOverlayPainter();
	SdlOverlayPainter& operator=(const SdlOverlayPainter& other) = delete;
	SdlOverlayPainter& operator=(SdlOverlayPainter&& other) = delete;

	/** Draw onto the renderer's current target, whose size is width x height pixels. */
	void draw(const SdlOverlay& overlay, int width, int height, float scale);

  private:
	struct Cell
	{
		TTF_Text* label = nullptr;
		TTF_Text* value = nullptr;
	};

	void clear();
	[[nodiscard]] bool rebuild(const SdlOverlay& overlay, float scale);
	[[nodiscard]] std::vector<Cell> cells(const std::vector<sdl::overlay::Row>& rows);
	void drawPanel(const std::vector<Cell>& list, const sdl::overlay::Box& where);
	[[nodiscard]] SDL_FPoint panelSize(const std::vector<Cell>& list, float& labelWidth) const;

	SDL_Renderer* _renderer;
	TTF_TextEngine* _engine = nullptr;
	TTF_Font* _font = nullptr;
	float _scale = 0.0F;
	uint64_t _generation = UINT64_MAX;
	std::vector<Cell> _left;
	std::vector<Cell> _right;
};
