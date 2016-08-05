/**
 * WinPR: Windows Portable Runtime
 * Path Functions
 *
 * Copyright 2016 Armin Novak <armin.novak@thincast.om>
 * Copyright 2016 Thincast Technologies GmbH
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

#import <Foundation/Foundation.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shell_ios.h"

char* ios_get_home(void)
{
	NSString* home_path = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
	                       NSUserDomainMask, YES) lastObject];
	return strdup([home_path UTF8String]);
}

char* ios_get_temp(void)
{
	NSString* tmp_path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"freerdp"];
	return strdup([tmp_path UTF8String]);
}
