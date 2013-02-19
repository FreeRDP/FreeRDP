/*
 controller for performance settings selection
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EditorBaseController.h"

@class ConnectionParams;

@interface PerformanceEditorController : EditorBaseController
{
@private
    ConnectionParams* _params;
    NSString* _keyPath;
}

- (id)initWithConnectionParams:(ConnectionParams*)params;
- (id)initWithConnectionParams:(ConnectionParams*)params keyPath:(NSString*)keyPath;

@end
