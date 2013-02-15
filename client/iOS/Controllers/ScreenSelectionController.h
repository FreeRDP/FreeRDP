/*
 controller for screen settings selection
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EditorBaseController.h"

@class ConnectionParams;
@class OrderedDictionary;

@interface ScreenSelectionController : EditorBaseController
{
@private
    NSString* _keyPath;
    ConnectionParams* _params;    
    
    // avaiable options
    OrderedDictionary* _color_options;
    NSArray* _resolution_modes;

    // current selections
    int _selection_color;
    int _selection_resolution;
}

- (id)initWithConnectionParams:(ConnectionParams*)params;
- (id)initWithConnectionParams:(ConnectionParams*)params keyPath:(NSString*)keyPath;

@end
