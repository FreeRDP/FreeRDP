/**
 * WinPR: Windows Portable Runtime
 * lsa4posix: per-user LSA daemon
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

#ifndef WINPR_TOOLS_LSA4POSIX_H
#define WINPR_TOOLS_LSA4POSIX_H

#include <winpr/wtypes.h>

#define LSA4POSIX_TAG "com.winpr.lsa4posix"

/* Serve one accepted client connection until it closes. Owns and closes fd. */
void lsa4posix_serve_connection(int fd);

#endif /* WINPR_TOOLS_LSA4POSIX_H */
