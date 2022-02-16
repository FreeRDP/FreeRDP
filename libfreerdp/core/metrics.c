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

#include <freerdp/config.h>

#include "rdp.h"

double metrics_write_bytes(rdpMetrics* metrics, UINT32 UncompressedBytes, UINT32 CompressedBytes)
{
	double CompressionRatio = 0.0;

	metrics->TotalUncompressedBytes += UncompressedBytes;
	metrics->TotalCompressedBytes += CompressedBytes;

	if (UncompressedBytes != 0)
		CompressionRatio = ((double)CompressedBytes) / ((double)UncompressedBytes);
	if (metrics->TotalUncompressedBytes != 0)
		metrics->TotalCompressionRatio =
		    ((double)metrics->TotalCompressedBytes) / ((double)metrics->TotalUncompressedBytes);

	return CompressionRatio;
}

rdpMetrics* metrics_new(rdpContext* context)
{
	rdpMetrics* metrics;

	metrics = (rdpMetrics*)calloc(1, sizeof(rdpMetrics));

	if (metrics)
	{
		metrics->context = context;
	}

	return metrics;
}

void metrics_free(rdpMetrics* metrics)
{
	free(metrics);
}
