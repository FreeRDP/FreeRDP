//
//  RDPSessionToolbar.h
//  iFreeRDP
//
//  Created by byungho on 6/21/26.
//

#ifndef RDPSessionToolbar_h
#define RDPSessionToolbar_h

#import <UIKit/UIKit.h>

// Session toolbar that only reacts to direct touches. Mouse/trackpad pointer input is
// redirected to passthroughView (the remote session) instead of hitting the toolbar.
@interface RDPSessionToolbar : UIToolbar

@property(nonatomic, assign) UIView *passthroughView;

@end

#endif /* RDPSessionToolbar_h */
