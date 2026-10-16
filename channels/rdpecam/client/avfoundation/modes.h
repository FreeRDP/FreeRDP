/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation native mode cache.
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

#ifndef FREERDP_RDPECAM_AVFOUNDATION_MODES_H
#define FREERDP_RDPECAM_AVFOUNDATION_MODES_H

#import <AVFoundation/AVFoundation.h>
#include "../camera.h"

/* An immutable snapshot: descriptor order and native selections stay stable until
 * the camera is enumerated again, including across idle capture restarts. */
FREERDP_LOCAL
@interface FreeRDPCameraAVFoundationModes : NSObject
- (instancetype)initWithFormats:(NSArray<AVCaptureDeviceFormat *> *)formats;
- (size_t)copyMediaTypes:(CAM_MEDIA_TYPE_DESCRIPTION *)mediaTypes capacity:(size_t)capacity;
- (AVCaptureDeviceFormat *)formatForMediaType:(const CAM_MEDIA_TYPE_DESCRIPTION *)mediaType
                                frameDuration:(CMTime *)frameDuration;
@end

#endif
