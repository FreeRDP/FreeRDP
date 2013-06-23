//
//  AppDelegate.h
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import <Cocoa/Cocoa.h>
#import <MacFreeRDP-library/MRDPView.h>
#import <MacFreeRDP-library/mfreerdp.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
@public
	NSWindow* window;
	rdpContext* context;
	MRDPView* mrdpView;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) rdpContext *context;
@property (assign) IBOutlet MRDPView *mrdpView;

@end
