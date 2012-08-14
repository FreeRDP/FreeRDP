/**
 * WinPR: Windows Portable Runtime
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_H
#define WINPR_H

#if defined _WIN32 || defined __CYGWIN__
	#ifdef WINPR_EXPORTS
		#ifdef __GNUC__
			#define WINPR_API __attribute__((dllexport))
		#else
			#define WINPR_API __declspec(dllexport)
		#endif
	#else
		#ifdef __GNUC__
			#define WINPR_API __attribute__((dllimport))
		#else
			#define WINPR_API __declspec(dllimport)
		#endif
	#endif
#else
	#if __GNUC__ >= 4
		#define WINPR_API   __attribute__ ((visibility("default")))
	#else
		#define WINPR_API
	#endif
#endif

#ifdef _WIN32
#define INLINE	__inline
#else
#define INLINE	inline
#endif

#endif /* WINPR_H */
