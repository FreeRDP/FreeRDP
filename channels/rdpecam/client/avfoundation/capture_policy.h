/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation capture policy for macOS and iOS.
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

#ifndef FREERDP_RDPECAM_AVFOUNDATION_CAPTURE_POLICY_H
#define FREERDP_RDPECAM_AVFOUNDATION_CAPTURE_POLICY_H

/* Adjust capture limits here. These are backend policy, not protocol limits.
 * modes.m uses these bounds to advertise native camera resolutions; nv12.c uses
 * the same ceilings when packing samples. No additional resolutions are synthesized.
 *
 * The limits bound capture cost and uncompressed NV12 traffic. They also apply
 * when the shared codec selects H.264 output. Revisit them when adding VideoToolbox
 * encoding, but retain suitable limits for NV12 passthrough; encoder availability
 * alone does not mean that compression was negotiated for a stream.
 *
 * NV12's even dimensions and plane layout, AVFoundation's available output formats,
 * and exact lookup of previously advertised modes remain required runtime checks.
 * The shared descriptor count limit is ECAM_MAX_MEDIA_TYPE_DESCRIPTORS in ../camera.h.
 */
#define CAM_AVFOUNDATION_MIN_WIDTH 160
#define CAM_AVFOUNDATION_MIN_HEIGHT 120
#define CAM_AVFOUNDATION_MAX_WIDTH 1920
#define CAM_AVFOUNDATION_MAX_HEIGHT 1080

/* Nominal FPS candidates in preference order, not a continuous FPS range.
 * Mode enumeration retains the matching native frame duration and advertises its
 * exact rational rate. Fixed rates outside this list are omitted even if the
 * camera supports them.
 */
#define CAM_AVFOUNDATION_FRAME_RATES { 30, 15 }

/* Native endpoint rounding tolerance: 10 ppm. This is a timing precision rule,
 * not a performance limit; keep distinct rates such as 29.97 and 30 separate.
 */
#define CAM_AVFOUNDATION_FRAME_DURATION_TOLERANCE 0.00001

#endif
