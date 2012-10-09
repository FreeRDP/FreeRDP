//
//  AppDelegate.h
//  MacFreeRDP
//
//  Created by Thomas Goddard on 5/8/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "MRDPView.h"

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet MRDPView *mrdpView;
@property (assign) IBOutlet NSWindow *window;


int rdp_connect();
@end
