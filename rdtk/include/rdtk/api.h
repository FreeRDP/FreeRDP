/**
 * RdTk: Remote Desktop Toolkit
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

#ifndef RDTK_API_H
#define RDTK_API_H

#include <winpr/spec.h>

/* Don't do any export */
#if 0
#if defined _WIN32 || defined __CYGWIN__
	#ifdef RDTK_EXPORTS
		#ifdef __GNUC__
			#define RDTK_EXPORT __attribute__((dllexport))
		#else
			#define RDTK_EXPORT __declspec(dllexport)
		#endif
	#else
		#ifdef __GNUC__
			#define RDTK_EXPORT __attribute__((dllimport))
		#else
			#define RDTK_EXPORT __declspec(dllimport)
		#endif
	#endif
#else
	#if __GNUC__ >= 4
		#define RDTK_EXPORT   __attribute__ ((visibility("default")))
	#else
		#define RDTK_EXPORT
	#endif
#endif
#endif
#define RDTK_EXPORT

#endif /* RDTK_API_H */
