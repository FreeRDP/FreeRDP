/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL ScopeGuard
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#include <functional>

class ScopeGuard
{
  public:
	template <class Callable> explicit ScopeGuard(Callable&& cleanupFunction)
	try : f(std::forward<Callable>(cleanupFunction))
	{
	}
	catch (...)
	{
		cleanupFunction();
		throw;
	}

	~ScopeGuard()
	{
		if (f)
			f();
	}

	void dismiss() noexcept
	{
		f = nullptr;
	}

	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard(ScopeGuard&& other) noexcept = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&&) = delete;

  private:
	std::function<void()> f;
};
