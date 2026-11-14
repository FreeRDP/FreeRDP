/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: mouse wheel rotation in RDP units
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

#include <cmath>
#include <cstdint>

#include <winpr/wtypes.h>

/* One wheel axis in RDP rotation units, 120 to a notch ([MS-RDPBCGR] 2.2.8.1.1.3.1.1.3).
 * SDL reports fractions of a notch: a touchpad, a high resolution wheel, a browser's pixel
 * deltas (SDL's Emscripten backend makes 100 pixels one notch, so 4 px is 4.8 units).
 * Converting each event alone drops its fraction, and an event of less than one unit is
 * lost entirely, so the rest is carried to the next event. It is kept in thousandths of a
 * unit: whole numbers add up exactly where 50 x 0.04f x 120 in floats is 239.99. */
class SdlWheelAxis
{
  public:
	[[nodiscard]] INT32 add(float notches)
	{
		_milli += std::llround(static_cast<double>(notches) * 120.0 * 1000.0);
		const int64_t units = _milli / 1000; /* toward zero; the rest keeps its sign */
		_milli -= units * 1000;
		return static_cast<INT32>(units);
	}

  private:
	int64_t _milli = 0;
};
