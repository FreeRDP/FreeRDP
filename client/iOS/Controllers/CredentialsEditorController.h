/*
 Controller to edit bookmark credentials
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EditorBaseController.h"

@class ComputerBookmark;
@class ConnectionParams;

@interface CredentialsEditorController : EditorBaseController
{
@private
    ComputerBookmark* _bookmark;
    ConnectionParams* _params;
}

// init for the given bookmark
- (id)initWithBookmark:(ComputerBookmark*)bookmark;

@end
