/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Timer implementation
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <stdint.h>
#include <stdbool.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** Type definition for timer IDs
	 *  @since version 3.16.0
	 */
	typedef uint64_t FreeRDP_TimerID;

	/** @brief Callback function pointer type definition.
	 *  An expired timer will be called, depending on \ref mainloop argument of \ref
	 * freerdp_timer_add, background thread or mainloop. This also greatly influence jitter and
	 * precision of the call. If called by \b mainloop, which might be blocked, delays for up to
	 * 100ms are to be expected. If called from a background thread no locking is performed, so be
	 * sure to lock your resources where necessary.
	 *
	 *
	 *  @param context The RDP context this timer belongs to
	 *  @param userdata Custom userdata provided by \ref freerdp_timer_add
	 *  @param timerID The timer ID that expired
	 *  @param timestamp The current timestamp for the call. The base is not specified, but the
	 * resolution is in nanoseconds.
	 *  @param interval The last interval value
	 *
	 *  @return A new interval (might differ from the last one set) or \b 0 to disable the timer
	 *
	 *  @since version 3.16.0
	 */
	typedef uint64_t (*FreeRDP_TimerCallback)(rdpContext* context, void* userdata,
	                                          FreeRDP_TimerID timerID, uint64_t timestamp,
	                                          uint64_t interval);

	/** @brief Add a new timer to the list of running timers
	 *
	 * @note While the API allows nano second precision the execution time might vary depending on
	 * various circumstances.
	 * \b mainloop executed callbacks will have a huge jitter and execution times are expected to be
	 * delayed up to multiple 10s of milliseconds. Current implementation also does not guarantee
	 * more than 10ms granularity even for background thread callbacks, but that might improve with
	 * newer versions.
	 *
	 * @note Current implementation limits all timers to be executed by a single background thread.
	 * So ensure your callbacks are not blocking for a long time as both, \b mainloop and background
	 * thread executed callbacks will delay execution of other tasks.
	 *
	 *  @param context The RDP context the timer belongs to
	 *  @param intervalNS The (first) timer expiration interval in nanoseconds
	 *  @param callback The function to be called when the timer expires. Must not be \b NULL
	 *  @param userdata Custom userdata passed to the callback. The pointer is only passed, it is up
	 * to the user to ensure the data exists when the timer expires.
	 *  @param mainloop \b true run the callback in mainloop context or \b false from background
	 * thread
	 *  @return A new timer ID or \b 0 in case of failure
	 *  @since version 3.16.0
	 */
	FREERDP_API FreeRDP_TimerID freerdp_timer_add(rdpContext* context, uint64_t intervalNS,
	                                              FreeRDP_TimerCallback callback, void* userdata,
	                                              bool mainloop);

	/** @brief Remove a timer from the list of running timers
	 *
	 *  @param context The RDP context the timer belongs to
	 *  @param id The timer ID to remove
	 *
	 *  @return \b true if the timer was removed, \b false otherwise
	 *  @since version 3.16.0
	 */
	FREERDP_API bool freerdp_timer_remove(rdpContext* context, FreeRDP_TimerID id);

#ifdef __cplusplus
}
#endif
