/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * wts_services.h interfaces across their C and C++ forms
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_TEST_WTS_SERVICES_CXX_H
#define FREERDP_TEST_WTS_SERVICES_CXX_H

#include <freerdp/wts_services.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* the image format the C++ renderer accepts, passed by value */
	WINPR_COM_DEFINE_GUID(TEST_WTS_CXX_FORMAT, 0x11223344, 0x5566, 0x7788, 0x99, 0xaa, 0xbb, 0xcc,
	                      0xdd, 0xee, 0xff, 0x00);

	/** @brief an IWTSBitmapRenderer implemented in C++ form, which takes a reference on
	 *  `callback` until RemoveMapping(). Each Render() calls OnTargetSizeChanged() with the frame
	 *  size, offset by the value of its first byte. */
	IWTSBitmapRenderer* test_wts_cxx_renderer_new(IWTSBitmapRendererCallback* callback);

	/** @brief how many renderers test_wts_cxx_renderer_new() gave that are not freed yet */
	int test_wts_cxx_renderers_alive(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_TEST_WTS_SERVICES_CXX_H */
