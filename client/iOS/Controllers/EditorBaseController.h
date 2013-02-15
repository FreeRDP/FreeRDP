/*
 Basic interface for settings editors
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#import <UIKit/UIKit.h>
#import "EditTextTableViewCell.h"
#import "EditFlagTableViewCell.h"
#import "EditSelectionTableViewCell.h"
#import "EditSubEditTableViewCell.h"
#import "EditSecretTextTableViewCell.h"
#import "EditButtonTableViewCell.h"

extern NSString* TableCellIdentifierText;
extern NSString* TableCellIdentifierSecretText;
extern NSString* TableCellIdentifierYesNo;
extern NSString* TableCellIdentifierSelection;
extern NSString* TableCellIdentifierSubEditor;
extern NSString* TableCellIdentifierMultiChoice;
extern NSString* TableCellIdentifierButton;

@interface EditorBaseController : UITableViewController <UITextFieldDelegate>
{   
@private    
    IBOutlet EditTextTableViewCell* _textTableViewCell;
    IBOutlet EditSecretTextTableViewCell* _secretTextTableViewCell;
    IBOutlet EditFlagTableViewCell* _flagTableViewCell;
    IBOutlet EditSelectionTableViewCell* _selectionTableViewCell;
    IBOutlet EditSubEditTableViewCell* _subEditTableViewCell;
    IBOutlet EditButtonTableViewCell* _buttonTableViewCell;
}

// returns one of the requested table view cells
- (UITableViewCell*)tableViewCellFromIdentifier:(NSString*)identifier;

// Adjust text input cells label/textfield widht according to the label's text size
- (void)adjustEditTextTableViewCell:(EditTextTableViewCell*)cell;

@end
