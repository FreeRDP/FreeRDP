/*
 Bookmark editor controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#import <UIKit/UIKit.h>
#import "EditorBaseController.h"

@class ComputerBookmark;
@class ConnectionParams;


@protocol BookmarkEditorDelegate <NSObject>
// bookmark editing finsihed
- (void)commitBookmark:(ComputerBookmark*)bookmark;
@end


@interface BookmarkEditorController : EditorBaseController
{
@private
    ComputerBookmark* _bookmark;
    ConnectionParams* _params;
        
    BOOL _display_server_settings;
    
    id<BookmarkEditorDelegate> delegate;
}

@property (nonatomic, assign) id<BookmarkEditorDelegate> delegate;

// init for the given bookmark
- (id)initWithBookmark:(ComputerBookmark*)bookmark;

@end
