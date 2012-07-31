
#import <Foundation/Foundation.h>
#import "MRDPRailView.h"
#import "MRDPRailWindow.h"

@interface MRDPWindow : NSObject
{
}

@property (assign) int windowID;
@property (retain) MRDPRailWindow * window;
@property (retain) MRDPRailView * view;

@end

