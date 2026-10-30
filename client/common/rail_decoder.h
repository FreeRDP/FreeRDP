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

typedef struct
{
	char* record;
	size_t maxRecord;
	size_t recordBytes;
	BOOL atLineStart;
	BOOL recordInvalid;
	const char* recordError;
} RailDecoder;

typedef enum
{
	RAIL_DECODER_INCOMPLETE,
	RAIL_DECODER_RECORD,
	RAIL_DECODER_REJECTED
} RailDecoderResult;

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rail_decoder_init(RailDecoder* dec, size_t maxRecord);
FREERDP_LOCAL void rail_decoder_uninit(RailDecoder* dec);
FREERDP_LOCAL void rail_decoder_reset(RailDecoder* dec);

/* Stops at the first completed record; consumed includes its terminator.
 * The order's strings are borrowed until the next feed or reset.
 * Rejection reasons are static strings; incomplete input has no order or reason. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL RailDecoderResult rail_decoder_feed(RailDecoder* dec, const BYTE* data, size_t length,
                                                  size_t* consumed, RAIL_EXEC_ORDER* order,
                                                  const char** reason);
