/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Protocol Metrics
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_METRICS_H
#define FREERDP_METRICS_H

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	struct rdp_metrics
	{
		rdpContext* context;

		UINT64 TotalCompressedBytes;
		UINT64 TotalUncompressedBytes;
		double TotalCompressionRatio;
	};
	typedef struct rdp_metrics rdpMetrics;

	FREERDP_API double metrics_write_bytes(rdpMetrics* metrics, UINT32 UncompressedBytes,
	                                       UINT32 CompressedBytes);

	FREERDP_API void metrics_free(rdpMetrics* metrics);

	WINPR_ATTR_MALLOC(metrics_free, 1)
	FREERDP_API rdpMetrics* metrics_new(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_METRICS_H */
