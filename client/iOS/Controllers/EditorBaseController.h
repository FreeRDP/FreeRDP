/*
 Basic interface for settings editors

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import "EditTextTableViewCell.h"
#import "EditFlagTableViewCell.h"
#import "EditSelectionTableViewCell.h"
#import "EditSubEditTableViewCell.h"
#import "EditSecretTextTableViewCell.h"

extern NSString *TableCellIdentifierText;
extern NSString *TableCellIdentifierSecretText;
extern NSString *TableCellIdentifierYesNo;
extern NSString *TableCellIdentifierSelection;
extern NSString *TableCellIdentifierSubEditor;
extern NSString *TableCellIdentifierMultiChoice;

@interface EditorBaseController : UITableViewController <UITextFieldDelegate>
// returns one of the requested table view cells
- (UITableViewCell *)tableViewCellFromIdentifier:(NSString *)identifier;

@end
