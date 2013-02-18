/*
 Generic controller to select a single item from a list of options
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EditorBaseController.h"

@class ConnectionParams;

@interface EditorSelectionController : EditorBaseController
{
    ConnectionParams* _params;

    // array with entries in connection parameters that are altered
    NSArray* _entries;

    // array with dictionaries containing label/value pairs that represent the available values for each entry
    NSArray* _selections;    
       
    // current selections
    NSMutableArray* _cur_selections;
}

- (id)initWithConnectionParams:(ConnectionParams*)params entries:(NSArray*)entries selections:(NSArray*)selections;

@end
