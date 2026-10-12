/*
 Shared look of the session overlays (key bar, extended keyboard, toolbar)

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

// return liquid glass (ios 26 or higher) or blur effect view
UIVisualEffectView *RDPCreateGlassCapsuleView(void);
void RDPUpdateGlassCapsuleCorners(UIVisualEffectView *view);

// draw 4 square like windows logo
UIImage *RDPWindowsLogoImage(CGFloat size);

// create button
UIButton *RDPCreateKeyButton(NSString *title, UIImage *image);

// create a page tab button (AdvancedKeyboard tab button)
UIButton *RDPCreateTabButton(NSString *title);

// check system support liquid glass (ios 26+)
BOOL RDPSupportLiquidGlass(void);
