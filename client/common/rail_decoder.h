/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL launch record decoder
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
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

#include <freerdp/rail.h>

#define RAIL_DECODER_CAPACITY 4096U

typedef struct
{
	char buffer[RAIL_DECODER_CAPACITY + 1U];
	size_t used;
	BOOL discarding;
} RailDecoder;

typedef enum
{
	RAIL_DECODER_INCOMPLETE,
	RAIL_DECODER_RECORD,
	RAIL_DECODER_REJECTED
} RailDecoderResult;

FREERDP_LOCAL void rail_decoder_consume(RailDecoder* dec, size_t consumed);

/* Append input to buffer + used, up to CAPACITY, then call next until incomplete.
 * Consume each returned prefix after using its borrowed order strings.
 * Rejected spans with no reason are skipped blank lines or oversized-record tails. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL RailDecoderResult rail_decoder_next(RailDecoder* dec, size_t* consumed,
                                                  RAIL_EXEC_ORDER* order, const char** reason);
