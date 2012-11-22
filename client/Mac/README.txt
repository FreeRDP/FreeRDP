          
-------------------------------------------------------------------------
                      Building FreeRDP on Mac OS X
-------------------------------------------------------------------------

Platform: Lion with Xcode 4.3.2

------------------
 installing cmake
------------------

first install macports by googling for it, the run the following command
sudo port install cmake

----------------
 installing gcc 
----------------
Click on Xcode->Preferences->Downloads
Click on Components
Click on Install Command line tools

You will be prompted for your Apple Developer userid and password

----------------------------------------
 download FreeRDP source code using git
----------------------------------------

mkdir ~/projects/A8
cd ~/projects/A8
git clone git://github.com/FreeRDP/FreeRDP.git

------------------
 building FreeRDP
------------------

cd ~projects/A8/FreeRDP
cmake -DWITH_MACAUDIO=ON -DCMAKE_INSTALL_PREFIX="</path/to/your/staging/dir>"
make
make install

------------------------
 creating Xcode project
------------------------

Start xcode
Select 'Create a new xcode project'
In 'Choose a template for your new project', click on Mac OS X -> application
Click on 'Cocoa Application'
Click on next
I used the following:
Product Name: Mac
Company Identifier: com.freerdp
Check 'Automatic Reference Counting'
Create the project in your directory of choice

-------------------------------
 Adding files to your projects
-------------------------------

Add the following files to your project:

cd ~/projects/A8/FreeRDP/client/Mac/MRDPCursor.h
cd ~/projects/A8/FreeRDP/client/Mac/MRDPCursor.m
cd ~/projects/A8/FreeRDP/client/Mac/MRDPView.h
cd ~/projects/A8/FreeRDP/client/Mac/MRDPView.m

This is what your AppDelegate.h file should like like

#import <Cocoa/Cocoa.h>
#import "MRDPView.h"

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet MRDPView *mrdpView;

int rdp_connect();

@end

This is what your AppDelegate.m file should like like

#import "AppDelegate.h"

@implementation AppDelegate

@synthesize window = _window;
@synthesize mrdpView;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    rdp_connect();
}

- (void) applicationWillTerminate:(NSNotification *)notification
{
    [mrdpView releaseResources];
}

@end

----------------------------------
 Modifying your MainMenu.xib file
----------------------------------

In your project select MainMenu.xib and drag a NSView  object into the main window
Name the class MRDPView
In Interface Builder, select the Application Delegate and tie the mrdpview outlet to the NSView 
Set the default size of the main window to 1024x768. This is FreeRDP's default resolution

----------------------------
 Configuring build settings
----------------------------

In Project Navigator, click on Mac
Click on Targets -> Mac
Click on Build Phases
Click on 'Link Binary With Libraries' and click on the + button, then click on the 'Add Other' button to add the following dynamic libraries
~/projects/A8/FreeRDP/libfreerdp-core/libfreerdp-core.dylib
~/projects/A8/FreeRDP/libfreerdp-channels/libfreerdp-channels.dylilb
~/projects/A8/FreeRDP/libfreerdp-utils/libfreerdp-utils.dylib
~/projects/A8/FreeRDP/libfreerdp-codec/libfreerdp-codec.dylib
~/projects/A8/FreeRDP/libfreerdp-cache/libfreerdp-cache.dylib
~/projects/A8/FreeRDP/libfreerdp-gdi/libfreerdp-gdi.dylib

Click on 'Build Settings'
In 'Search Paths -> Library Search Paths' set the following 
    Header Search Path Debug:    ~/projects/A8/FreeRDP/include
    Header Search Path Release:  ~/projects/A8/FreeRDP/include

TODO: in build settings, set strip build product to yes when done debugging

---------------------------
 To deploy the application
---------------------------

in xcode, click on Product->Archive
Click on Distribute button
Select Export As -> application

